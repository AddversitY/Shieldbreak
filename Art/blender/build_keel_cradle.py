"""Keel's centrepiece, the Cradle (run headless: python3 Art/blender/build_keel_cradle.py [--render=<folder>]).

Brief: docs/KEEL_MIDSHIP_STUDY.md. The blockout (Design/maps/shieldbreak_maps.py, keel()) has a cargo pod block
(x -700..700, y -400..400, z 400..650) hung over a drydock trench (z 0, x -1300..1300), a crane-arm ramp from each
approach deck (x -+1300, z 350) up to the pod, and a 15 m ceiling. These pieces dress those blocks. They are visual
only (no collision), so every part either hugs a block face (at most 25 cm proud), hangs where nobody can stand
(under the pod, under the ramps, under the ceiling) or is too thin to hide a player (cables).

All are authored in map coordinates (cm, origin at the map centre on the trench floor) and placed at the origin;
the red-side pieces are placed again turned 180 degrees for blue.

  CradlePod     the pod's shell: faceted belly with ribs and work lamps, plated side skirts with a hazard band,
                end caps, edge light strips, four lifting lugs and the cables up to the gantry.
  CradleGantry  the overhead crane the pod hangs from: two rails along the ship, a trolley with four winches.
                Hung piece: top face at z = 0, placed at the ceiling.
  CraneArm      red's crane arm: girders with edge lights down both sides of the ramp, a hinge on the deck edge and
                a stay under it.
  TrenchWall    red's side of the trench: buttress ribs, a hazard chevron band and a lit lip on the deck's face.

Same conventions as build_hero_kit.py. Slots: M_Steel / M_Gunmetal take the map's panel material in game, M_Glow*
takes the map's glow colour, M_Hazard and M_HullAmber keep their palette instance.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit  # noqa: E402  (imports bpy first)
import build_map_kit as kit  # noqa: E402

OUT = kit.OUT

POD_X, POD_Y, POD_LO, POD_HI = 700.0, 400.0, 400.0, 650.0
DECK_X, DECK_Z = 1300.0, 350.0
ARM_HALF = 125.0          # the crane-arm ramp is 250 wide
SKIN = 22.0               # how far the shell stands off the pod block


def cradle_pod():
    parts = []
    # Belly: a faceted hull under the pod, 70 cm deep at the keel line (3.3 m clear over the trench floor).
    belly = [(-POD_X, POD_LO), (-POD_X + 150, POD_LO - 70), (POD_X - 150, POD_LO - 70), (POD_X, POD_LO)]
    parts.append(kit.prism_xz("belly", belly, -POD_Y, POD_Y, "Gunmetal", bevel=6.0))
    for i in range(-4, 5):   # frames across the belly
        parts.append(sb_kit.box("frame", (i * 125.0, 0, POD_LO - 78), (34, 2 * POD_Y - 60, 22), "Steel", bevel=3.0))
    parts.append(sb_kit.box("keel", (0, 0, POD_LO - 92), (1040, 70, 26), "Steel", bevel=5.0))
    for sx in (-1, 1):       # work lamps over the Overshield
        parts.append(sb_kit.box("lamp_house", (sx * 310, 0, POD_LO - 84), (150, 260, 30), "Gunmetal", bevel=5.0))
        parts.append(sb_kit.box("lamp", (sx * 310, 0, POD_LO - 101), (110, 220, 6), "HullAmber", bevel=0.0))

    # Side skirts (the long faces): a dark plate, raised panels, a hazard band and a light strip on the top edge.
    for sy in (-1, 1):
        y = sy * (POD_Y + SKIN / 2)
        parts.append(sb_kit.box("skirt", (0, y, (POD_LO + POD_HI) / 2), (2 * POD_X, SKIN, POD_HI - POD_LO), "Gunmetal", bevel=3.0))
        for i in range(7):
            x = -600 + i * 200
            parts.append(kit.placed(kit.frustum("panel", (0, 0, 0), (240, 200), (200, 160), 10, "Steel", bevel=2.0),
                                    (x, sy * (POD_Y + SKIN), POD_LO + 95), (-sy * 90, 0, 0)))
        parts.append(sb_kit.box("hazard", (0, sy * (POD_Y + SKIN + 3), POD_HI - 42), (2 * POD_X - 40, 6, 44), "Hazard", bevel=0.0))
        parts.append(sb_kit.box("edge_glow", (0, sy * (POD_Y + SKIN - 5), POD_HI + 1), (2 * POD_X - 60, 10, 6), "GlowCyan", bevel=0.0))
        parts.append(sb_kit.box("sill", (0, sy * (POD_Y + SKIN + 6), POD_LO + 6), (2 * POD_X, 34, 24), "Steel", bevel=5.0))

    # End caps either side of where the crane arm lands.
    for sx in (-1, 1):
        for sy in (-1, 1):
            cy = sy * (ARM_HALF + 30 + (POD_Y - ARM_HALF - 30) / 2)
            wide = POD_Y - ARM_HALF - 30
            x = sx * (POD_X + SKIN / 2)
            parts.append(sb_kit.box("cap", (x, cy, (POD_LO + POD_HI) / 2), (SKIN, wide, POD_HI - POD_LO), "Gunmetal", bevel=3.0))
            parts.append(sb_kit.box("cap_panel", (sx * (POD_X + SKIN + 3), cy, POD_LO + 120), (8, wide - 50, 130), "Steel", bevel=2.0))
            parts.append(sb_kit.box("cap_haz", (sx * (POD_X + SKIN + 3), cy, POD_HI - 42), (6, wide - 20, 44), "Hazard", bevel=0.0))
            parts.append(sb_kit.box("cap_glow", (sx * (POD_X + SKIN - 5), cy, POD_HI + 1), (10, wide - 30, 6), "GlowCyan", bevel=0.0))

    # Lifting lugs outside each corner, and the cables up to the gantry's winches (see cradle_gantry).
    for sx in (-1, 1):
        for sy in (-1, 1):
            lx, ly = sx * (POD_X + 34), sy * (POD_Y + 34)
            parts.append(sb_kit.box("lug_foot", (lx, ly, POD_HI - 110), (96, 96, 200), "Steel", bevel=8.0))
            parts.append(kit.frustum("lug", (lx, ly, POD_HI - 10), (84, 84), (44, 44), 70, "Gunmetal", bevel=4.0, rotate=0.0))
            parts.append(sb_kit.cyl("shackle", (lx, ly, POD_HI + 66), 26, 30, "Steel", axis="Y" if sx * sy > 0 else "X", verts=10))
            top = (sx * 420.0, sy * 300.0, 1500.0 - 285.0)
            base = (lx, ly, POD_HI + 70)
            mid = tuple((a + b) / 2 for a, b in zip(base, top))
            dx, dy, dz = (top[0] - base[0], top[1] - base[1], top[2] - base[2])
            length = math.sqrt(dx * dx + dy * dy + dz * dz)
            cable = sb_kit.cyl("cable", (0, 0, 0), 5.0, length, "Gunmetal", axis="Z", verts=6, bevel=0.0)
            pitch = math.degrees(math.atan2(math.hypot(dx, dy), dz))
            yaw = math.degrees(math.atan2(dy, dx))
            parts.append(kit.placed(cable, mid, (0, pitch, yaw)))
    return sb_kit.join("SM_SB_Kit_CradlePod", parts)


def cradle_gantry():
    """Top face at z = 0 (the ceiling); hangs to z = -300. Rails run along Y (across the trench's long axis)."""
    parts = []
    for sx in (-1, 1):
        parts.append(sb_kit.box("rail", (sx * 560, 0, -45), (90, 3600, 90), "Steel", bevel=6.0))
        parts.append(sb_kit.box("rail_web", (sx * 560, 0, -110), (36, 3600, 50), "Gunmetal", bevel=0.0))
        parts.append(sb_kit.box("rail_glow", (sx * 560 - sx * 48, 0, -60), (6, 3400, 16), "GlowCyan", bevel=0.0))
        for y in (-1500, -750, 750, 1500):     # hangers to the ceiling
            parts.append(sb_kit.box("hanger", (sx * 560, y, -20), (150, 60, 40), "Gunmetal", bevel=4.0))
    # Trolley: two cross girders, a machinery deck between them, four winches over the pod's lugs.
    for sy in (-1, 1):
        parts.append(sb_kit.box("girder", (0, sy * 300, -150), (1300, 110, 130), "Steel", bevel=8.0))
        parts.append(sb_kit.box("girder_haz", (0, sy * 358, -150), (1240, 6, 40), "Hazard", bevel=0.0))
        for sx in (-1, 1):
            parts.append(sb_kit.box("bogie", (sx * 560, sy * 300, -100), (190, 220, 70), "Gunmetal", bevel=8.0))
            parts.append(sb_kit.cyl("winch", (sx * 420, sy * 300, -245), 60, 150, "Gunmetal", axis="X", verts=12, bevel=2.0))
            parts.append(sb_kit.box("winch_cheek", (sx * 420, sy * 300, -225), (190, 40, 110), "Steel", bevel=4.0))
    parts.append(sb_kit.box("deck", (0, 0, -175), (760, 500, 60), "Gunmetal", bevel=6.0))
    parts.append(kit.frustum("motor", (0, 0, -300), (300, 240), (420, 340), 95, "Steel", bevel=6.0, rotate=0.0))
    for sx in (-1, 1):   # two small work lamps (one big amber panel bloomed out in game, K2)
        parts.append(sb_kit.box("spot", (sx * 90, 0, -302), (60, 110, 4), "HullAmber", bevel=0.0))
    for sx in (-1, 1):
        parts.append(sb_kit.box("cabinet", (sx * 250, 0, -230), (120, 380, 70), "Steel", bevel=5.0))
    return sb_kit.join("SM_SB_Kit_CradleGantry", parts)


def crane_arm():
    """Red's crane arm: the ramp runs from (-1300, 0, 350) on the deck edge to (-700, 0, 650) on the pod."""
    parts = []
    x0, z0, x1, z1 = -DECK_X, DECK_Z, -POD_X, POD_HI
    run, rise = x1 - x0, z1 - z0
    for sy in (-1, 1):
        y0, y1 = sy * ARM_HALF, sy * (ARM_HALF + 26)
        lo, hi = min(y0, y1), max(y0, y1)
        # Box girder down the side of the ramp: its top is the ramp surface, 46 cm deep.
        side = [(x0, z0 + 2), (x1, z1 + 2), (x1, z1 - 46), (x0, z0 - 46)]
        parts.append(kit.prism_xz("girder", side, lo, hi, "Steel", bevel=3.0))
        strip = [(x0 + 20, z0 + 3), (x1 - 20, z1 + 3), (x1 - 20, z1 + 9), (x0 + 20, z0 + 9)]
        parts.append(kit.prism_xz("edge_glow", strip, sy * (ARM_HALF + 8), sy * (ARM_HALF + 18), "GlowCyan", bevel=0.0))
        # Lightening pockets along the girder (dark insets, proud by 3).
        for i in range(5):
            t = 0.12 + i * 0.19
            cx, cz = x0 + run * t, z0 + rise * t - 22
            pocket = [(cx - 40, cz - 14 - 20 * rise / run), (cx + 40, cz - 14 + 20 * rise / run),
                      (cx + 40, cz + 10 + 20 * rise / run), (cx - 40, cz + 10 - 20 * rise / run)]
            parts.append(kit.prism_xz("pocket", pocket, sy * (ARM_HALF + 24), sy * (ARM_HALF + 30), "Gunmetal", bevel=0.0))
        # Hinge knuckle on the deck's face, below deck level.
        parts.append(sb_kit.box("hinge", (x0 + 22, sy * (ARM_HALF + 40), z0 - 70), (44, 90, 130), "Gunmetal", bevel=6.0))
    parts.append(sb_kit.cyl("pin", (x0 + 24, 0, z0 - 60), 20, 2 * ARM_HALF + 150, "Steel", axis="Y", verts=10))
    # Underside: cross ties and a stay from the trench wall up to the middle of the arm.
    for i in range(4):
        t = 0.15 + i * 0.23
        parts.append(kit.placed(sb_kit.box("tie", (0, 0, 0), (36, 2 * ARM_HALF, 20), "Gunmetal", bevel=2.0),
                                (x0 + run * t, 0, z0 + rise * t - 56), (0, -math.degrees(math.atan2(rise, run)), 0)))
    foot = (x0 + 12, 110.0)
    head = (x0 + run * 0.6, z0 + rise * 0.6 - 50)
    for sy in (-1, 1):
        stay = [(foot[0], foot[1]), (foot[0] + 34, foot[1] - 6), (head[0] + 14, head[1]), (head[0] - 20, head[1])]
        parts.append(kit.prism_xz("stay", stay, sy * 70 - 12, sy * 70 + 12, "Steel", bevel=2.0))
    parts.append(sb_kit.box("stay_foot", (x0 + 16, 0, 100), (32, 220, 70), "Gunmetal", bevel=5.0))
    return sb_kit.join("SM_SB_Kit_CraneArm", parts)


def trench_wall():
    """Red's trench wall: the approach deck's face at x = -1300 (z 0..350), between the two trench ramps
    (y -1050..1050), leaving the crane arm's hinge clear in the middle."""
    parts = []
    x = -DECK_X
    for sy in (-1, 1):
        y_in, y_out = 230.0, 1040.0
        mid, wide = sy * (y_in + y_out) / 2, y_out - y_in
        parts.append(sb_kit.box("lip", (x + 9, mid, DECK_Z - 12), (18, wide, 24), "Steel", bevel=4.0))
        parts.append(sb_kit.box("lip_glow", (x + 20, mid, DECK_Z - 14), (5, wide - 30, 8), "GlowCyan", bevel=0.0))
        parts.append(sb_kit.box("kick", (x + 10, mid, 14), (20, wide, 28), "Gunmetal", bevel=4.0))
        # Hazard chevrons under the lip, pointing down into the trench.
        count = 9
        for i in range(count):
            cy = sy * (y_in + 45 + i * (wide - 90) / (count - 1))
            chevron = [(-34, 0), (0, -30), (34, 0), (34, 22), (0, -8), (-34, 22)]
            piece = kit.prism_xz("chevron", chevron, -3, 3, "Hazard", bevel=0.0)
            parts.append(kit.placed(piece, (x + 6, cy, DECK_Z - 62), (0, 0, 90)))
        # Buttress ribs: tapered blades 22 proud at the foot, 8 at the lip.
        for i in range(4):
            cy = sy * (y_in + 60 + i * (wide - 120) / 3)
            rib = [(x, 30), (x + 22, 30), (x + 22, 120), (x + 8, DECK_Z - 100), (x, DECK_Z - 100)]
            parts.append(kit.prism_xz("rib", rib, cy - 20, cy + 20, "Steel", bevel=2.0))
        # Recessed-looking service panels between the ribs.
        for i in range(3):
            cy = sy * (y_in + 60 + (i + 0.5) * (wide - 120) / 3)
            parts.append(sb_kit.box("panel", (x + 4, cy, 150), (8, (wide - 120) / 3 - 70, 150), "Gunmetal", bevel=2.0))
            parts.append(sb_kit.box("panel_light", (x + 9, cy, 232), (4, 90, 8), "HullAmber", bevel=0.0))
    return sb_kit.join("SM_SB_Kit_TrenchWall", parts)


PIECES = (cradle_pod, cradle_gantry, crane_arm, trench_wall)


def build(render=None):
    os.makedirs(OUT, exist_ok=True)
    report = []
    for make in PIECES:
        sb_kit.reset_scene()
        obj = make()
        slots = ", ".join(m.name for m in obj.data.materials)
        line = "%s: %d tris, %s, slots %s" % (obj.name, sb_kit.triangle_count(obj), sb_kit.bounds(obj), slots)
        print(line)
        report.append(line)
        if render:
            import sb_render
            sb_render.render_views([obj], os.path.join(render, "cradle_%s.png" % make.__name__),
                                   [((1.0, -0.9, 0.45), False), ((0.6, -1.0, -0.25), False)], floor=False)
        sb_kit.export_fbx(os.path.join(OUT, obj.name + ".fbx"), [obj])
    with open(os.path.join(OUT, "cradle_report.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


if __name__ == "__main__":
    previews = next((a.split("=", 1)[1] for a in sys.argv if a.startswith("--render=")), None)
    build(render=previews)
