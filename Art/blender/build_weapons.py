"""Builds Shieldbreak's weapon models and exports one FBX per weapon.

    python build_weapons.py [--render [preview_dir]]

FBX files go to Art/Export/Weapons (committed, imported on the PC by Tools/ImportArt.bat); previews to preview_dir.

Mesh space matches Lyra's guns (and so the hand bone the code attaches to): origin at the top of the pistol grip,
barrel along +Y, up +Z, X across the gun. Every gun has a SOCKET_Muzzle (Unreal socket "Muzzle") at the barrel tip.
Material slots are named after the palette in sb_kit.py; Tools/import_art.py builds matching Unreal materials.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit as k  # noqa: E402


def battle_rifle():
    """Warden BR: long bullpup, top scope, olive handguard, three-round burst."""
    p = []
    # Receiver: a long slab from the butt to the handguard, chamfered nose.
    p.append(k.profile("receiver", [(-40, -7), (-40, 5), (-36, 8), (16, 8), (20, 6.5), (20, -3), (8, -3), (4, -4.5),
                                    (-6, -4.5), (-8, -3), (-34, -3), (-38, -7)], 5.2, "Gunmetal", bevel=0.7))
    # Two-tone side panels.
    for x in (2.75, -2.75):
        p.append(k.profile("panel", [(-33, -1.5), (-33, 5.5), (-12, 5.5), (-9, 3), (-9, -1.5)], 0.6, "Olive", x=x, bevel=0.2))
    # Butt pad.
    p.append(k.box("buttpad", (0, -40.6, -0.8), (5.6, 1.4, 12), "Polymer", bevel=0.4))
    # Magazine behind the grip (bullpup) with a slight rake.
    p.append(k.profile("mag", [(-19, -3), (-10, -3), (-11, -15), (-19.5, -14)], 3.8, "Polymer", bevel=0.5))
    # Pistol grip, raked back.
    p.append(k.profile("grip", [(-3.5, -3.5), (3, -3.5), (1.5, -15.5), (-4.5, -15)], 3.4, "Polymer", bevel=0.6))
    # Trigger guard.
    p.append(k.profile("guard", [(3, -4), (9, -4), (9, -9), (7.5, -9), (7.5, -5.2), (3, -5.2)], 1.0, "Gunmetal", bevel=0.2))
    # Handguard with a thumb ledge.
    p.append(k.profile("handguard", [(14, -4), (14, 6), (33, 6), (35, 4), (35, -2), (31, -6), (16, -6)], 6.0, "Olive", bevel=0.8))
    for z in (-1, 2):
        for x in (3.05, -3.05):
            p.append(k.box("vent", (x, 26, z), (0.3, 7, 1.0), "Polymer", bevel=0))
    # Barrel and muzzle brake.
    p.append(k.cyl("barrel", (0, 40, 2.5), 1.5, 12, "Steel", verts=10))
    p.append(k.cyl("barrel_collar", (0, 36, 2.5), 2.0, 2, "Gunmetal", verts=10))
    p.append(k.box("brake", (0, 46, 2.5), (3.4, 4, 3.0), "Gunmetal", bevel=0.4))
    for y in (45, 47):
        for x in (1.75, -1.75):
            p.append(k.box("port", (x, y, 2.5), (0.2, 0.8, 1.6), "Polymer", bevel=0))
    # Top rail + scope.
    p.append(k.box("rail", (0, 2, 9), (2.6, 30, 1.6), "Gunmetal", bevel=0.3))
    for y in (-6, 10):
        p.append(k.box("mount", (0, y, 10.8), (2.4, 2.4, 2.6), "Gunmetal", bevel=0.3))
    p.append(k.cyl("scope", (0, 2, 13.2), 2.1, 22, "Gunmetal", verts=12))
    p.append(k.cyl("scope_bell", (0, 15, 13.2), 2.6, 4, "Gunmetal", verts=12))
    p.append(k.cyl("scope_eye", (0, -10.5, 13.2), 2.4, 3, "Polymer", verts=12))
    p.append(k.cyl("lens", (0, 17.05, 13.2), 2.1, 0.2, "Lens", verts=12, bevel=0))
    # Ammo counter on the back of the receiver (faces the player).
    p.append(k.box("counter", (0, -20, 8.4), (3.0, 5, 0.6), "GlowCyan", bevel=0.1))
    p.append(k.box("strip", (2.9, -4, 6.5), (0.15, 14, 0.4), "GlowCyan", bevel=0))
    return p, (0, 48.2, 2.5)


def sidearm():
    """Kestrel sidearm: long slab-sided slide, compensator cuts, a cyan strip down each side."""
    p = []
    p.append(k.profile("slide", [(-6.5, 0), (-6.5, 4.6), (-5.5, 5.4), (16, 5.4), (17.5, 4), (17.5, 0)], 3.4, "Steel", bevel=0.45))
    p.append(k.profile("frame", [(-5.5, -2.2), (-5.5, 0.2), (17, 0.2), (17, -1.6), (14, -2.6), (4, -2.6)], 3.0, "Gunmetal", bevel=0.4))
    p.append(k.profile("grip", [(-5.5, -2.2), (1.5, -2.2), (0.2, -11), (-1.2, -12), (-7.2, -11.3)], 3.3, "Polymer", bevel=0.6))
    p.append(k.profile("guard", [(1.5, -2.4), (6.5, -2.4), (6.5, -6.0), (5.2, -6.0), (5.2, -3.5), (1.5, -3.5)], 0.9, "Gunmetal", bevel=0.2))
    p.append(k.box("serrations", (0, -4.2, 3), (3.55, 2.5, 3.6), "Gunmetal", bevel=0.2))
    p.append(k.cyl("barrel", (0, 17.7, 2.2), 0.8, 1.2, "Polymer", verts=10, bevel=0.1))
    for y in (11.5, 13.5):
        p.append(k.box("top_vent", (0, y, 5.45), (1.6, 1.0, 0.2), "Polymer", bevel=0))
    p.append(k.box("front_sight", (0, 15.8, 5.9), (0.7, 1.2, 1.0), "Gunmetal", bevel=0.15))
    p.append(k.box("rear_sight", (0, -5.6, 5.9), (2.6, 1.2, 1.0), "Gunmetal", bevel=0.15))
    for x in (1.72, -1.72):
        p.append(k.box("strip", (x, 5.5, 3.4), (0.12, 14, 0.45), "GlowCyan", bevel=0))
    p.append(k.box("mag_base", (0, -3.4, -11.8), (3.6, 7.0, 1.2), "Gunmetal", bevel=0.3))
    return p, (0, 18.4, 2.2)


def sniper():
    """Longshot sniper: long fluted barrel, big scope, skeleton stock."""
    p = []
    p.append(k.profile("receiver", [(-16, -3), (-16, 7), (22, 7), (26, 5), (26, -2), (6, -3.5), (-4, -4)], 5.6, "Gunmetal", bevel=0.7))
    p.append(k.profile("stock", [(-48, -9), (-48, 6), (-44, 7), (-16, 7), (-16, 1), (-30, 1), (-33, -3), (-16, -3), (-16, -5), (-40, -5), (-44, -9)],
                       4.4, "Polymer", bevel=0.6))
    p.append(k.box("cheek", (0, -32, 7.6), (4.0, 14, 1.6), "Olive", bevel=0.4))
    p.append(k.profile("grip", [(-4, -3.5), (2.5, -3.5), (1, -15.5), (-5, -15)], 3.4, "Polymer", bevel=0.6))
    p.append(k.profile("guard", [(2.5, -4), (8.5, -4), (8.5, -9), (7, -9), (7, -5.2), (2.5, -5.2)], 1.0, "Gunmetal", bevel=0.2))
    p.append(k.profile("mag", [(10, -3.5), (19, -3.5), (18.5, -11), (10.5, -11)], 3.6, "Polymer", bevel=0.4))
    p.append(k.profile("handguard", [(24, -2.5), (24, 5.5), (46, 5.5), (48, 4), (48, -0.5), (45, -2.5)], 5.0, "Olive", bevel=0.7))
    p.append(k.cyl("barrel", (0, 60, 2), 1.35, 28, "Steel", verts=12))
    for y in (52, 58, 64):
        p.append(k.ring("flute", (0, y, 2), 1.45, 0.25, "Gunmetal", verts=12, tube_verts=4))
    p.append(k.cyl("brake", (0, 75.5, 2), 2.0, 5, "Gunmetal", verts=8, bevel=0.4))
    p.append(k.box("rail", (0, 4, 8.1), (2.6, 34, 1.4), "Gunmetal", bevel=0.3))
    for y in (-6, 14):
        p.append(k.box("mount", (0, y, 10), (2.6, 3, 3), "Gunmetal", bevel=0.3))
    p.append(k.cyl("scope", (0, 4, 13), 2.4, 26, "Gunmetal", verts=14))
    p.append(k.cyl("scope_bell", (0, 19.5, 13), 3.4, 6, "Gunmetal", verts=14, radius2=3.0))
    p.append(k.cyl("scope_eye", (0, -11, 13), 2.9, 4, "Polymer", verts=14))
    p.append(k.cyl("lens", (0, 22.55, 13), 2.9, 0.2, "Lens", verts=14, bevel=0))
    p.append(k.cyl("turret", (0, 5, 16), 1.2, 2.4, "Steel", axis="Z", verts=10))
    p.append(k.cyl("turret_side", (2.6, 5, 13), 1.2, 2.0, "Steel", axis="X", verts=10))
    p.append(k.box("strip", (2.85, 10, 4), (0.15, 18, 0.45), "GlowCyan", bevel=0))
    return p, (0, 78.2, 2)


def rocket_launcher():
    """Hammerhead launcher: one fat octagonal tube, hazard bands, side optic, two grips."""
    p = []
    tube_z = 6.5
    p.append(k.cyl("tube", (0, 6, tube_z), 6.2, 88, "Olive", verts=8, bevel=0.8))
    p.append(k.cyl("muzzle_ring", (0, 49, tube_z), 7.0, 5, "Gunmetal", verts=8, bevel=0.6))
    p.append(k.cyl("muzzle_hole", (0, 51.6, tube_z), 5.0, 0.4, "Polymer", verts=8, bevel=0))
    p.append(k.cyl("rear_ring", (0, -37, tube_z), 7.0, 5, "Gunmetal", verts=8, bevel=0.6))
    p.append(k.cyl("rear_hole", (0, -39.6, tube_z), 5.0, 0.4, "Polymer", verts=8, bevel=0))
    for y in (40, 43.5):
        p.append(k.cyl("hazard", (0, y, tube_z), 6.35, 2, "Hazard", verts=8, bevel=0.2))
    # Underslung body: grip, trigger guard, magazine housing for 2 rockets.
    p.append(k.profile("body", [(-14, 1), (12, 1), (12, -1), (-14, -1)], 6.6, "Gunmetal", bevel=0.5))
    p.append(k.profile("grip", [(-3.5, -1), (3, -1), (1.5, -14), (-4.5, -13.5)], 3.6, "Polymer", bevel=0.6))
    p.append(k.profile("guard", [(3, -1.5), (9, -1.5), (9, -7), (7.5, -7), (7.5, -2.8), (3, -2.8)], 1.0, "Gunmetal", bevel=0.2))
    p.append(k.profile("foregrip", [(18, 0.5), (24, 0.5), (23, -11), (18.5, -11)], 3.6, "Polymer", bevel=0.6))
    p.append(k.box("breech", (0, -22, tube_z), (14.2, 10, 10), "Gunmetal", bevel=0.8))
    p.append(k.box("shoulder", (0, -30, 0), (5, 8, 4), "Polymer", bevel=0.5))
    # Optic on the left side of the tube.
    p.append(k.box("optic_arm", (-6.6, 4, tube_z + 4), (2, 4, 2), "Gunmetal", bevel=0.3))
    p.append(k.box("optic", (-8.6, 4, tube_z + 4.5), (3.6, 12, 4.6), "Gunmetal", bevel=0.6))
    p.append(k.box("optic_lens", (-8.6, 10.05, tube_z + 4.5), (2.6, 0.2, 3.2), "Lens", bevel=0))
    p.append(k.box("optic_glow", (-8.6, -2.05, tube_z + 4.5), (2.4, 0.2, 2.8), "GlowAmber", bevel=0))
    for x in (7.05, -7.05):
        p.append(k.box("ammo_light", (x, -22, tube_z + 2), (0.15, 3, 1.2), "GlowAmber", bevel=0))
        p.append(k.box("ammo_light2", (x, -22, tube_z - 1), (0.15, 3, 1.2), "GlowAmber", bevel=0))
    return p, (0, 52, tube_z)


def rail_lance():
    """Rail Lance: smooth ivory shell, two forward prongs with a cyan rail glowing between them."""
    p = []
    p.append(k.profile("shell", [(-34, -6), (-34, 4), (-28, 8), (12, 9), (20, 7), (24, 3), (24, -3), (10, -4), (-6, -4), (-10, -3), (-26, -3), (-30, -6)],
                       6.4, "Ivory", bevel=1.2, segments=2))
    p.append(k.profile("core", [(-26, -1.5), (-26, 5), (10, 6), (22, 3), (22, -1.5)], 7.2, "Gunmetal", bevel=0.6))
    p.append(k.profile("grip", [(-3.5, -3.5), (3, -3.5), (1.5, -15), (-4.5, -14.5)], 3.4, "Polymer", bevel=0.6))
    p.append(k.profile("guard", [(3, -4), (9, -4), (9, -8.5), (7.5, -8.5), (7.5, -5.2), (3, -5.2)], 1.0, "Gunmetal", bevel=0.2))
    # Prongs (upper and lower, mirrored about z = 2) reaching forward; the rail glows between them.
    upper = [(20, 3.2), (20, 7.2), (44, 6.2), (52, 4.8), (52, 3.4)]
    for name, pts in (("prong_up", upper), ("prong_down", [(y, 4.0 - z) for y, z in reversed(upper)])):
        p.append(k.profile(name, pts, 3.0, "Ivory", bevel=0.6, segments=2))
    for z in (4.1, -0.1):
        p.append(k.box("prong_tip", (0, 51.2, z), (3.3, 2.0, 1.2), "GlowCyan", bevel=0.2))
    p.append(k.box("rail_glow", (0, 36, 2), (1.0, 30, 1.0), "GlowCyan", bevel=0.15))
    for y in (-18, -12, -6):
        for x in (3.25, -3.25):
            p.append(k.box("vent", (x, y, 3), (0.2, 3.5, 3.2), "GlowCyan", bevel=0))
    p.append(k.box("sight", (0, 0, 9.6), (1.8, 10, 1.6), "Gunmetal", bevel=0.4))
    p.append(k.box("sight_glow", (0, -5.05, 9.6), (1.2, 0.2, 1.0), "GlowCyan", bevel=0))
    return p, (0, 50, 2)


WEAPONS = {
    "SM_SB_BattleRifle": battle_rifle,
    "SM_SB_Sidearm": sidearm,
    "SM_SB_Sniper": sniper,
    "SM_SB_RocketLauncher": rocket_launcher,
    "SM_SB_RailLance": rail_lance,
}


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    export_root = os.path.join(here, "..", "Export")
    out_dir = os.path.join(export_root, "Weapons")
    os.makedirs(out_dir, exist_ok=True)
    k.write_palette(os.path.join(export_root, "palette.json"))
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    preview_dir = os.path.abspath(args[0]) if args else os.path.join(here, "previews")
    os.makedirs(preview_dir, exist_ok=True)
    report = []
    for name, build in WEAPONS.items():
        k.reset_scene()
        parts, muzzle = build()
        obj = k.join(name, parts)
        k.socket("Muzzle", muzzle, obj)
        report.append("%s: %d tris, %d materials, bounds %s" % (name, k.triangle_count(obj), len(obj.data.materials), k.bounds(obj)))
        k.export_fbx(os.path.join(out_dir, name + ".fbx"), [obj])
        if "--render" in sys.argv:
            import sb_render
            sb_render.render_object(obj, os.path.join(preview_dir, name + ".png"))
    print("\n".join(report))
    with open(os.path.join(out_dir, "weapons_report.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


if __name__ == "__main__":
    main()
