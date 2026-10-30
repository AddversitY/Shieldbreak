"""Original environment set pieces for the five launch maps (run headless: python3 Art/blender/build_map_kit.py [--render=<folder>]).

Every piece stands outside the playable walls (or above the wall line) and is visual only: AShieldbreakArena's
BuildMapArt places them as instanced meshes with no collision, so routes, cover and sight lines stay authored.

Mesh space: 1 unit = 1 cm, pivot on the ground at the piece's base, local +X faces the arena. Material slots use
palette names; the game puts its tinted world material (triplanar Panel / Stone / Grass) on the solid slots by name
and keeps the imported emissive instance on M_Glow* slots, so glow seams need no extra draw calls.

Pieces (theme in brackets; see docs/MAP_ART_STYLE.md):
  Rib        tall leaning hull rib that rises over a perimeter wall           [Keel drydock, Bastion buttress]
  Gantry     two-legged crane frame with a glowing beam                        [Keel]
  Pylon      tapered three-stage tower with fins and glow slots, a skyline landmark   [Spire, Keel, Bastion]
  Monument   two swept blades with a hard-light bar between them, the far landmark   [every launch map]
  Monolith   chamfered, leaning obelisk split by a glowing seam                [Sanctum]
  Arch       broken temple arch                                                [Sanctum]
  Pine       low-poly conifer                                                  [Creekbed]
  Crown      pointed cap with an amber band for tall unwalkable blocks (inside the map) [Sanctum]
  Relief     carved facade panel stretched over wall faces, 6 cm proud (inside the map) [Sanctum]
  Fascia     chamfered edge band with an amber line for slab and walkway edges (inside) [Sanctum]
  Inlay      2 cm glowing diamond floor inlay, stretched over open floor (inside)      [Sanctum]
  Sigil      floating hard-light cracked-shield team sigil (team-tinted M_TeamGlow)     [Sanctum temples]
  Beacon     floating amber hard-light prism hung over a map's centre                   [Sanctum]
  RockTall / RockBoulder / RockShelf   faceted rocks and cliff chunks          [all maps]

Exported to Art/Export/MapKit for Tools/ImportMapArt.bat.
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit  # noqa: E402  (imports bpy first)

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Vector, noise  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Export", "MapKit")


def prism_xz(name, points_xz, y0, y1, mat, bevel=4.0):
    """A polygon in the XZ plane extruded along Y from y0 to y1 (the arena-facing silhouettes are drawn in XZ)."""
    bm = bmesh.new()
    front = [bm.verts.new((x, y0, z)) for x, z in points_xz]
    back = [bm.verts.new((x, y1, z)) for x, z in points_xz]
    bm.faces.new(front)
    bm.faces.new(list(reversed(back)))
    n = len(points_xz)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((front[i], front[j], back[j], back[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    return sb_kit._finish(sb_kit._link(name, mesh), mat, bevel)


def frustum(name, center, size_lo, size_hi, height, mat, bevel=3.0, sides=4, rotate=45.0):
    """A tapered column (square by default) standing on center: size_* are (x, y) widths at the bottom and top."""
    bm = bmesh.new()
    rings = []
    for z, (sx, sy) in ((0.0, size_lo), (height, size_hi)):
        ring = []
        for i in range(sides):
            a = math.radians(rotate + 360.0 * i / sides)
            ring.append(bm.verts.new((center[0] + math.cos(a) * sx * 0.7071, center[1] + math.sin(a) * sy * 0.7071, center[2] + z)))
        rings.append(ring)
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[1])
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((rings[0][i], rings[0][j], rings[1][j], rings[1][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    return sb_kit._finish(sb_kit._link(name, mesh), mat, bevel)


def placed(obj, location, rotation_deg):
    """Moves and turns a piece built around the origin (sb_kit.box bakes its centre into the vertices, so build it at
    the origin and place it here; rotating a baked box would swing it around the world origin)."""
    obj.location = location
    obj.rotation_euler = tuple(math.radians(a) for a in rotation_deg)
    return obj


def rock(name, size, seed, mat="Sand", detail=2, roughness=0.32, flat_bottom=True):
    """A faceted rock: a subdivided icosphere pushed around by noise, scaled to size (x, y, z), base on z = 0."""
    rng = random.Random(seed)
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=detail, radius=1.0)
    offset = Vector((rng.uniform(-50, 50), rng.uniform(-50, 50), rng.uniform(-50, 50)))
    for v in bm.verts:
        n = noise.noise(v.co * 1.6 + offset) * 0.6 + noise.noise(v.co * 3.7 + offset) * 0.25
        v.co *= 1.0 + n * roughness * 2.0
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0] * 0.5, v.co.y * size[1] * 0.5, v.co.z * size[2] * 0.5 + size[2] * 0.35))
        if flat_bottom and v.co.z < 0.0:
            v.co.z = -20.0  # sunk a little into the ground
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = sb_kit._link(name, mesh)
    for p in obj.data.polygons:
        p.use_smooth = False
    obj.data.materials.append(sb_kit.material(mat))
    return obj


# ---- Pieces --------------------------------------------------------------------------------------------------

def rib():
    """A hull rib / buttress: a blade 120 cm thick that climbs from 900 cm outside the wall to 1700 cm high and
    leans in to stand over the wall line (x = 0 is the wall's outer face, the wall is 100 cm thick, so x <= 90
    stays inside the wall's footprint). The game scales Z to the map's wall height (authored for a 1000 cm wall)."""
    parts = []
    outline = [(-900, 0), (-420, 0), (-120, 520), (40, 1000), (90, 1450), (60, 1700), (-40, 1700),
               (-200, 1380), (-520, 900), (-820, 380)]
    parts.append(prism_xz("rib", outline, -60, 60, "Steel", bevel=6.0))
    # A recessed spine plate and a cap, so the edge reads as built, not cut.
    parts.append(prism_xz("rib_plate", [(-700, 120), (-470, 120), (-190, 580), (-30, 1040), (10, 1440), (-80, 1440),
                                        (-260, 1080), (-560, 640)], -72, 72, "Gunmetal", bevel=4.0))
    parts.append(sb_kit.box("rib_cap", (10, 0, 1700), (170, 150, 60), "Steel", bevel=8.0))
    # Glow seam down the inner edge, proud of both faces.
    seam = [(-130, 560), (-110, 560), (28, 1010), (74, 1440), (54, 1440), (8, 1010)]
    parts.append(prism_xz("rib_glow", seam, -78, 78, "GlowCyan", bevel=0.0))
    parts.append(sb_kit.box("rib_lamp", (20, 0, 1640), (40, 160, 26), "GlowCyan", bevel=0.0))
    return sb_kit.join("SM_SB_Kit_Rib", parts)


def gantry():
    """A crane frame 3200 wide and 3000 high, its beam facing the arena (+X)."""
    parts = []
    for side in (-1, 1):
        y = side * 1450
        parts.append(sb_kit.box("leg", (0, y, 1400), (260, 220, 2800), "Steel", bevel=10.0, taper=None))
        parts.append(sb_kit.box("foot", (0, y, 120), (520, 420, 240), "Gunmetal", bevel=14.0))
        # Diagonal knee braces between each leg and the beam.
        parts.append(placed(sb_kit.box("brace", (0, 0, 0), (90, 900, 80), "Gunmetal", bevel=4.0),
                            (0, side * 1130, 2480), (side * 40, 0, 0)))
    parts.append(sb_kit.box("beam", (0, 0, 2850), (320, 3400, 300), "Steel", bevel=12.0))
    parts.append(sb_kit.box("beam_top", (0, 0, 3030), (220, 3200, 60), "Gunmetal", bevel=6.0))
    parts.append(sb_kit.box("beam_glow", (165, 0, 2850), (14, 3000, 40), "GlowCyan", bevel=0.0))
    parts.append(sb_kit.box("trolley", (0, 380, 2620), (360, 500, 220), "Gunmetal", bevel=10.0))
    parts.append(sb_kit.box("hook_line", (0, 380, 2100), (24, 24, 900), "Gunmetal", bevel=0.0))
    parts.append(sb_kit.box("hook", (0, 380, 1600), (160, 120, 140), "Steel", bevel=8.0, taper=(0.6, 0.6)))
    return sb_kit.join("SM_SB_Kit_Gantry", parts)


def pylon():
    """A 6000 cm three-stage tower on a stepped plinth, with four fins and glow slots; reads across the skyline."""
    parts = [frustum("plinth", (0, 0, 0), (1400, 1400), (1150, 1150), 300, "Gunmetal", bevel=12.0)]
    z = 300
    for i, (lo, hi, h) in enumerate(((900, 700, 2200), (640, 460, 1900), (400, 160, 1600))):
        parts.append(frustum("stage%d" % i, (0, 0, z), (lo, lo), (hi, hi), h, "Steel", bevel=10.0))
        # A collar between stages and two glow slots per face.
        parts.append(frustum("collar%d" % i, (0, 0, z + h - 60), (hi + 120, hi + 120), (hi + 60, hi + 60), 120, "Gunmetal", bevel=6.0))
        for a in range(4):
            ang = math.radians(90 * a)
            r = (lo + hi) * 0.25 + 6
            slot = sb_kit.box("slot", (math.cos(ang) * r, math.sin(ang) * r, z + h * 0.5), (30, 30, h * 0.55), "GlowCyan", bevel=0.0)
            parts.append(slot)
        z += h
    # Fins: tall blades on the diagonals of the lower two stages.
    for a in range(4):
        ang = math.radians(45 + 90 * a)
        fin = prism_xz("fin", [(-200, 0), (560, 0), (420, 700), (60, 3400), (-200, 3900)], -45, 45, "Steel", bevel=5.0)
        fin.location = (math.cos(ang) * 560, math.sin(ang) * 560, 300)
        fin.rotation_euler = (0, 0, ang)
        parts.append(fin)
    parts.append(sb_kit.box("beacon", (0, 0, z + 60), (110, 110, 140), "GlowCyan", bevel=0.0))
    return sb_kit.join("SM_SB_Kit_Pylon", parts)


def monolith():
    """A 2600 cm obelisk in two leaning halves with a glowing seam between them, on a chamfered base."""
    parts = [frustum("base", (0, 0, 0), (900, 700), (760, 560), 220, "Ivory", bevel=14.0)]
    for side in (-1, 1):
        half = prism_xz("half", [(-260, 0), (260, 0), (200, 1800), (60, 2600), (-160, 2300), (-230, 900)],
                        side * 22, side * 300, "Ivory", bevel=10.0)
        half.location = (0, 0, 220)
        parts.append(half)
    parts.append(prism_xz("seam", [(-240, 60), (240, 60), (180, 1800), (50, 2520), (-150, 2250), (-215, 900)],
                          -24, 24, "GlowAmber", bevel=0.0))
    parts[-1].location = (0, 0, 220)
    parts.append(sb_kit.box("glyph", (262, 0, 1200), (16, 120, 520), "GlowAmber", bevel=0.0))
    return sb_kit.join("SM_SB_Kit_Monolith", parts)


def arch():
    """A broken temple arch: two stepped piers and a lintel that has sheared off one side. 2000 wide, 1900 high."""
    parts = []
    for side, height in ((-1, 1900), (1, 1350)):
        y = side * 820
        parts.append(frustum("pier", (0, y, 0), (520, 440), (420, 360), height, "Ivory", bevel=10.0))
        parts.append(sb_kit.box("plinth", (0, y, 90), (640, 560, 180), "Sand", bevel=12.0))
    # Lintel from the tall pier, broken partway across (a tilted stub), plus a fallen block at its foot.
    parts.append(sb_kit.box("lintel", (0, -380, 1820), (440, 1300, 260), "Ivory", bevel=10.0))
    parts.append(placed(sb_kit.box("lintel_stub", (0, 0, 0), (420, 520, 220), "Ivory", bevel=10.0), (0, 560, 1330), (14, 0, 0)))
    parts.append(placed(sb_kit.box("fallen", (0, 0, 0), (420, 760, 240), "Ivory", bevel=12.0), (420, 160, 110), (0, 6, 28)))
    parts.append(sb_kit.box("arch_glow", (225, -380, 1820), (12, 1100, 30), "GlowAmber", bevel=0.0))
    return sb_kit.join("SM_SB_Kit_Arch", parts)


def monument():
    """A skyline landmark in the Forerunner manner: two swept blades 8800 cm tall leaning toward each other (their
    tips stay apart, so it never reads as a ring), a hard-light bar between them and a stepped plinth. Spans Y."""
    parts = [frustum("plinth", (0, 0, 0), (2400, 7600), (2000, 7000), 300, "Gunmetal", bevel=20.0, rotate=0.0),
             frustum("plinth_top", (0, 0, 300), (1500, 6400), (1300, 6000), 200, "Steel", bevel=16.0, rotate=0.0)]
    # Blade silhouette in the YZ plane (written as x = y here, then turned): wide foot, a long inward sweep, a hooked tip.
    outline = [(-650, 0), (650, 0), (520, 1800), (180, 4200), (-420, 6400), (-1300, 8100), (-1650, 8800),
               (-1500, 8350), (-950, 6900), (-480, 4600), (-470, 2200)]
    for side in (-1, 1):
        blade = prism_xz("blade", outline, -160, 160, "Steel", bevel=14.0)
        trim = prism_xz("blade_trim", [(-120, 900), (40, 900), (-60, 4300), (-640, 6550), (-1350, 8150), (-1450, 8150),
                                         (-780, 6450), (-240, 4250)], -175, 175, "GlowCyan", bevel=0.0)
        for obj in (blade, trim):
            # Lay the XZ outline into YZ, sweeping toward the centre line, and stand it on the plinth.
            placed(obj, (0, side * 2600, 500), (0, 0, 90 * side))
            parts.append(obj)
    parts.append(sb_kit.box("hardlight", (0, 0, 5600), (60, 3600, 60), "GlowCyan", bevel=0.0))
    parts.append(sb_kit.box("hardlight_lo", (0, 0, 3600), (40, 4700, 40), "GlowCyan", bevel=0.0))
    return sb_kit.join("SM_SB_Kit_Monument", parts)


def crown():
    """A pointed cap for tall, unwalkable blocks (temple spires, colossi): a chamfered collar, four leaning fins and a
    glowing band. Authored on a 100 x 100 footprint so the game scales it to the block it caps."""
    parts = [frustum("collar", (0, 0, 0), (112, 112), (104, 104), 18, "Ivory", bevel=1.5),
             frustum("band", (0, 0, 18), (100, 100), (100, 100), 8, "GlowAmber", bevel=0.0),
             frustum("roof", (0, 0, 26), (104, 104), (40, 40), 70, "Ivory", bevel=1.5),
             frustum("tip", (0, 0, 96), (40, 40), (6, 6), 110, "Ivory", bevel=0.8)]
    for a in range(4):
        ang = math.radians(90 * a)
        fin = prism_xz("fin", [(-6, 0), (14, 0), (6, 60), (-10, 150), (-14, 150)], -4, 4, "Ivory", bevel=0.6)
        placed(fin, (math.cos(ang) * 44, math.sin(ang) * 44, 26), (0, 0, math.degrees(ang)))
        parts.append(fin)
    return sb_kit.join("SM_SB_Kit_Crown", parts)


def relief():
    """A carved facade panel for a wall face: a frame, stepped chevron plates and an amber seam, standing 6 cm proud
    of the face (+X). Authored 100 wide (Y) and 100 high so the game stretches it over a face, minus a margin."""
    parts = [sb_kit.box("frame_l", (2, -46, 50), (4, 8, 88), "Ivory", bevel=0.0),
             sb_kit.box("frame_r", (2, 46, 50), (4, 8, 88), "Ivory", bevel=0.0),
             sb_kit.box("frame_b", (2, 0, 3), (4, 100, 6), "Ivory", bevel=0.0),
             sb_kit.box("frame_t", (2, 0, 97), (4, 100, 6), "Ivory", bevel=0.0),
             sb_kit.box("plate", (1, 0, 50), (2, 84, 88), "Sand", bevel=0.0)]
    for i, z in enumerate((22, 46, 70)):
        # Chevron: two plates angled up toward the centre line.
        for side in (-1, 1):
            plate = sb_kit.box("chev", (0, 0, 0), (3, 38, 9), "Ivory", bevel=0.0)
            placed(plate, (4, side * 19, z), (side * -18, 0, 0))
            parts.append(plate)
    parts.append(sb_kit.box("seam", (4.5, 0, 86), (3, 60, 2.5), "GlowAmber", bevel=0.0))
    parts.append(sb_kit.box("seam_v", (4.5, 0, 50), (3, 2.5, 60), "GlowAmber", bevel=0.0))
    return sb_kit.join("SM_SB_Kit_Relief", parts)


def fascia():
    """An edge band for floor slabs and walkways: a chamfered lip with a recessed amber line, 8 cm proud of the
    face (+X). Authored 100 wide and 100 high so it stretches over a slab edge."""
    parts = [prism_xz("lip", [(0, 0), (6, 0), (8, 18), (8, 82), (6, 100), (0, 100)], -50, 50, "Ivory", bevel=0.0),
             sb_kit.box("line", (8.5, 0, 50), (2, 96, 10), "GlowAmber", bevel=0.0)]
    return sb_kit.join("SM_SB_Kit_Fascia", parts)


def inlay():
    """A floor inlay, 2 cm high on a 100 x 100 footprint: a diamond of glowing lines round a solid centre stone.
    Stretched over open floor (Sanctum's courtyard) to draw the eye to the centre."""
    parts = [sb_kit.box("centre", (0, 0, 1), (16, 16, 2), "Ivory", bevel=0.0)]
    for a in range(4):
        # Edge of the diamond: a thin bar from one tip to the next.
        ang = math.radians(45 + 90 * a)
        bar = sb_kit.box("edge", (0, 0, 0), (48, 2.4, 2), "GlowAmber", bevel=0.0)
        parts.append(placed(bar, (math.cos(ang) * 24, math.sin(ang) * 24, 1), (0, 0, math.degrees(ang) + 90)))
        spoke = sb_kit.box("spoke", (0, 0, 0), (16, 1.6, 2), "GlowAmber", bevel=0.0)
        parts.append(placed(spoke, (math.cos(math.radians(90 * a)) * 42, math.sin(math.radians(90 * a)) * 42, 1), (0, 0, 90 * a)))
    return sb_kit.join("SM_SB_Kit_Inlay", parts)


def sigil():
    """A floating hard-light team sigil: a cracked kite-shield outline split by a gap, made only of glowing bars, in
    the team-tinted slot. 400 cm tall, centred on the origin, facing +X (both sides read). Original to Shieldbreak."""
    outline = [(0, 200), (150, 140), (130, -40), (0, -200), (-130, -40), (-150, 140)]
    parts = []
    for i in range(len(outline)):
        (y0, z0), (y1, z1) = outline[i], outline[(i + 1) % len(outline)]
        length = math.hypot(y1 - y0, z1 - z0)
        bar = sb_kit.box("bar", (0, 0, 0), (10, length - 14, 14), "TeamGlow", bevel=0.0)
        parts.append(placed(bar, (0, (y0 + y1) / 2, (z0 + z1) / 2), (math.degrees(math.atan2(z1 - z0, y1 - y0)), 0, 0)))
    # The crack: a jagged gap line down the middle, offset bars.
    for (y0, z0), (y1, z1) in (((-10, 170), (20, 60)), ((20, 60), (-15, -40)), ((-15, -40), (8, -160))):
        length = math.hypot(y1 - y0, z1 - z0)
        bar = sb_kit.box("crack", (0, 0, 0), (8, length, 8), "TeamGlow", bevel=0.0)
        parts.append(placed(bar, (0, (y0 + y1) / 2, (z0 + z1) / 2), (math.degrees(math.atan2(z1 - z0, y1 - y0)), 0, 0)))
    return sb_kit.join("SM_SB_Kit_Sigil", parts)


def beacon():
    """A floating hard-light prism (two pyramids base to base) with a slow-reading amber core: a landmark hung high
    over a map's centre. 300 cm tall, centred on the origin."""
    parts = [frustum("upper", (0, 0, 0), (140, 140), (2, 2), 160, "GlowAmber", bevel=0.0),
             frustum("lower", (0, 0, 0), (140, 140), (2, 2), -140, "GlowAmber", bevel=0.0),
             frustum("collar", (0, 0, -10), (190, 190), (190, 190), 20, "Ivory", bevel=1.0)]
    return sb_kit.join("SM_SB_Kit_Beacon", parts)


def pine():
    """A 2200 cm low-poly conifer: a bark trunk and five stacked, slightly twisted cones."""
    parts = [sb_kit.cyl("trunk", (0, 0, 450), 55, 900, "Bark", axis="Z", verts=7, bevel=0.0, radius2=35)]
    z, radius = 380, 560
    for i in range(5):
        h = 620 - i * 60
        cone = sb_kit.cyl("tier%d" % i, (0, 0, z + h * 0.5), radius, h, "Olive", axis="Z", verts=8, bevel=0.0, radius2=radius * 0.18)
        cone.rotation_euler = (0, 0, math.radians(17 * i))
        for p in cone.data.polygons:
            p.use_smooth = False
        parts.append(cone)
        z += h * 0.62
        radius *= 0.8
    return sb_kit.join("SM_SB_Kit_Pine", parts)


def rocks():
    return [
        sb_kit.join("SM_SB_Kit_RockTall", [rock("rt", (900, 760, 2400), 3, detail=3, roughness=0.28)]),
        sb_kit.join("SM_SB_Kit_RockBoulder", [rock("rb", (1000, 900, 700), 7, detail=3, roughness=0.35)]),
        sb_kit.join("SM_SB_Kit_RockShelf", [rock("rs", (2600, 1500, 900), 12, detail=3, roughness=0.3)]),
    ]


PIECES = (rib, gantry, pylon, monument, monolith, arch, pine, crown, relief, fascia, inlay, sigil, beacon)


def build(render=None):
    """render: a folder for Cycles previews of each piece (slow), or None."""
    sb_kit.PALETTE.setdefault("Bark", ((0.11, 0.075, 0.05), 0.0, 0.9, 0.0))
    os.makedirs(OUT, exist_ok=True)
    report = []
    for make in PIECES + (rocks,):
        sb_kit.reset_scene()
        objects = make()
        objects = objects if isinstance(objects, list) else [objects]
        for obj in objects:
            slots = ", ".join(m.name for m in obj.data.materials)
            line = "%s: %d tris, %s, slots %s" % (obj.name, sb_kit.triangle_count(obj), sb_kit.bounds(obj), slots)
            print(line)
            report.append(line)
            sb_kit.export_fbx(os.path.join(OUT, obj.name + ".fbx"), [obj])
        if render:
            import sb_render
            sb_render.render_views(objects, os.path.join(render, "mapkit_%s.png" % make.__name__),
                                   [((1.0, -0.9, 0.45), False), ((1.0, 0.0, 0.0), True)], floor=True)
    sb_kit.write_palette(os.path.join(OUT, "..", "palette.json"))
    with open(os.path.join(OUT, "mapkit_report.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


if __name__ == "__main__":
    previews = next((a.split("=", 1)[1] for a in sys.argv if a.startswith("--render=")), None)
    build(render=previews)
