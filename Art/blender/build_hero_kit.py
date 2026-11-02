"""Hero landmarks for the redesigned maps, one per map (run headless: python3 Art/blender/build_hero_kit.py [--render=<folder>]).

Each piece is original, visual only (AShieldbreakArena places it as an instanced mesh with no collision) and stands
where it cannot hide a player or block a shot: outside the walls on the outdoor maps, up under the ceiling on the
enclosed ones. Same conventions as build_map_kit.py: 1 unit = 1 cm, pivot on the piece's base (or, for the hanging
pieces, on their top face, so they hang down from the ceiling), material slots named after the shared palette;
M_Glow* slots keep their emissive instance and the game recolours them per map (FShieldbreakMapLook::KitGlow).

  ReactorCrown  Keel: the warship's reactor crown, an octagonal ring of armoured segments with an amber core,
                hung from the ceiling over the reactor (top at z = 0, hangs to z = -260).
  Condenser     Spire: an inverted, stepped hard-light condenser over the energy column, hung from the ceiling
                (top at z = 0, hangs to z = -420).
  Watchtower    Bastion: a square stone keep-tower with corner buttresses, battlements and a beacon fire, 5400 cm tall.
  SandstoneArch Creekbed: a natural sandstone arch with a flat, heavy span and uneven legs, weathered by noise.

Exported to Art/Export/MapKit (Tools/ImportMapArt.bat imports the whole folder).
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit  # noqa: E402  (imports bpy first)
import build_map_kit as kit  # noqa: E402

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Vector, noise  # noqa: E402

OUT = kit.OUT


def ring_of(name, count, radius, make):
    """`count` copies of make(i) turned round the Z axis at `radius` (each built facing +X at the origin)."""
    parts = []
    for i in range(count):
        ang = 360.0 * i / count
        obj = make(i)
        a = math.radians(ang)
        kit.placed(obj, (math.cos(a) * radius, math.sin(a) * radius, obj.location.z), (0, 0, ang))
        parts.append(obj)
    return parts


def reactor_crown():
    """Octagonal ring (radius 900) of armoured segments with a glowing inner band, eight hanger struts up to the
    ceiling and a faceted amber core in the middle. Top face at z = 0."""
    parts = []
    segs = 8
    seg_len = 2 * 900 * math.tan(math.pi / segs) * 0.94
    for i in range(segs):
        a = 360.0 * i / segs + 360.0 / segs / 2
        r = math.radians(a)
        body = sb_kit.box("seg", (0, 0, 0), (220, seg_len, 160), "Gunmetal", bevel=10.0)
        kit.placed(body, (math.cos(r) * 900, math.sin(r) * 900, -170), (0, 0, a))
        plate = sb_kit.box("plate", (0, 0, 0), (60, seg_len * 0.8, 120), "Steel", bevel=6.0)
        kit.placed(plate, (math.cos(r) * 1020, math.sin(r) * 1020, -170), (0, 0, a))
        band = sb_kit.box("band", (0, 0, 0), (14, seg_len * 0.9, 40), "GlowAmber", bevel=0.0)
        kit.placed(band, (math.cos(r) * 786, math.sin(r) * 786, -170), (0, 0, a))
        strut = sb_kit.box("strut", (0, 0, 0), (60, 60, 160), "Steel", bevel=4.0)
        kit.placed(strut, (math.cos(r) * 900, math.sin(r) * 900, -80), (0, 0, a))
        parts += [body, plate, band, strut]
    # Spokes to the core and the core itself: a stretched octahedron of amber hard light in a steel collar.
    for i in range(4):
        a = 45.0 * i * 2
        spoke = sb_kit.box("spoke", (0, 0, 0), (1500, 50, 50), "Gunmetal", bevel=3.0)
        kit.placed(spoke, (0, 0, -150), (0, 0, a))
        parts.append(spoke)
    parts.append(kit.frustum("collar", (0, 0, -200), (420, 420), (360, 360), 120, "Steel", bevel=8.0, sides=8, rotate=22.5))
    parts.append(kit.frustum("core_lo", (0, 0, -260), (40, 40), (300, 300), 120, "GlowAmber", bevel=0.0, sides=8, rotate=22.5))
    parts.append(kit.frustum("core_hi", (0, 0, -140), (300, 300), (60, 60), 140, "GlowAmber", bevel=0.0, sides=8, rotate=22.5))
    return sb_kit.join("SM_SB_Kit_ReactorCrown", parts)


def condenser():
    """Three stepped octagonal plates narrowing downward, glowing rings between them, and a hanging prism.
    Top face at z = 0; the prism's tip is at z = -420."""
    parts = []
    z = 0.0
    for i, (r_top, r_bot, h) in enumerate(((760, 640, 70), (560, 440, 70), (360, 240, 70))):
        z -= h
        parts.append(kit.frustum("plate%d" % i, (0, 0, z), (r_bot * 2, r_bot * 2), (r_top * 2, r_top * 2), h,
                                 "Ivory", bevel=6.0, sides=8, rotate=22.5))
        z -= 30
        parts.append(kit.frustum("ring%d" % i, (0, 0, z), (r_bot * 1.9, r_bot * 1.9), (r_bot * 1.9, r_bot * 1.9), 30,
                                 "GlowCyan", bevel=0.0, sides=8, rotate=22.5))
    # Four fins down the sides, and the prism.
    for a in range(4):
        ang = 45 + 90 * a
        fin = kit.prism_xz("fin", [(0, 0), (120, 0), (40, -260), (-10, -300)], -12, 12, "Ivory", bevel=2.0)
        kit.placed(fin, (math.cos(math.radians(ang)) * 620, math.sin(math.radians(ang)) * 620, 0), (0, 0, ang))
        parts.append(fin)
    parts.append(kit.frustum("prism", (0, 0, -420), (10, 10), (180, 180), 120, "GlowCyan", bevel=0.0, sides=4, rotate=0.0))
    return sb_kit.join("SM_SB_Kit_Condenser", parts)


def watchtower():
    """A 5000 cm square stone keep-tower: a battered base, a tall shaft with angled buttresses at the corners, a
    machicolated gallery, a crenellated top and a warm beacon fire burning on it. Snow-pale stone is the Ivory slot,
    dark stone the Sand slot."""
    parts = [kit.frustum("base", (0, 0, 0), (2000, 2000), (1500, 1500), 1000, "Sand", bevel=18.0),
             kit.frustum("shaft", (0, 0, 1000), (1200, 1200), (1000, 1000), 2800, "Ivory", bevel=14.0),
             kit.frustum("corbel", (0, 0, 3800), (1000, 1000), (1400, 1400), 300, "Sand", bevel=10.0),
             kit.frustum("gallery", (0, 0, 4100), (1400, 1400), (1400, 1400), 500, "Ivory", bevel=10.0)]
    for a in range(4):
        ang = 45 + 90 * a
        b = kit.prism_xz("buttress", [(0, 0), (1000, 0), (380, 1300), (80, 3100), (0, 3100)], -90, 90, "Ivory", bevel=8.0)
        kit.placed(b, (math.cos(math.radians(ang)) * 560, math.sin(math.radians(ang)) * 560, 700), (0, 0, ang))
        parts.append(b)
    # Merlons round the top (4 per side), the beacon fire in a brazier, and arrow slits down the shaft.
    for side in range(4):
        r = math.radians(90 * side)
        for k in (-1.5, -0.5, 0.5, 1.5):
            along = k * 300
            x, y = 640, along
            parts.append(sb_kit.box("merlon", (x * math.cos(r) - y * math.sin(r), x * math.sin(r) + y * math.cos(r), 4750),
                                    (150, 180, 300), "Ivory", bevel=6.0))
        parts.append(kit.placed(sb_kit.box("slit", (0, 0, 0), (14, 50, 380), "GlowAmber", bevel=0.0),
                                (math.cos(r) * 545, math.sin(r) * 545, 2600), (0, 0, 90 * side)))
    parts.append(kit.frustum("brazier", (0, 0, 4600), (500, 500), (700, 700), 260, "Sand", bevel=8.0))
    parts.append(kit.frustum("fire", (0, 0, 4860), (560, 560), (60, 60), 520, "GlowAmber", bevel=0.0, sides=6, rotate=0.0))
    return sb_kit.join("SM_SB_Kit_Watchtower", parts)


def sandstone_arch():
    """A natural arch: two leaning legs and a sagging span, built as a swept, noise-weathered tube in the YZ plane
    (it spans Y, faces the arena along +X). 7600 wide, 4600 tall."""
    rng = random.Random(41)
    bm = bmesh.new()
    rings = []
    steps, sides = 36, 10
    offset = Vector((rng.uniform(-40, 40), rng.uniform(-40, 40), rng.uniform(-40, 40)))
    for s in range(steps + 1):
        t = s / steps
        ang = math.pi * t                       # 0 .. pi round the arch
        # A wide, flat-topped span with uneven legs (the -Y leg is the heavier one), not a round hoop.
        y = -3500 * math.cos(ang) + 250 * math.sin(2 * ang)
        z = 3700 * math.sin(ang) ** 0.4 + 200 * math.sin(3 * ang)
        thick = (1700 if t < 0.5 else 1250) - (900 if t < 0.5 else 500) * math.sin(ang) ** 0.8 + 250 * math.sin(ang) ** 6
        depth = 1700 - 400 * math.sin(ang)
        dy = 3500 * math.sin(ang) + 500 * math.cos(2 * ang)
        dz = 3700 * 0.4 * math.cos(ang) * max(math.sin(ang), 1e-3) ** -0.6 + 600 * math.cos(3 * ang)
        tangent = Vector((0.0, dy, dz)).normalized()
        normal = Vector((0.0, -tangent.z, tangent.y))
        ring = []
        for k in range(sides):
            phi = 2 * math.pi * k / sides
            p = Vector((0.0, y, z)) + Vector((math.cos(phi) * depth * 0.5, 0, 0)) + normal * math.sin(phi) * thick * 0.5
            n = noise.noise(p * 0.0011 + offset) * 0.5 + noise.noise(p * 0.0031 + offset) * 0.25
            p += (p - Vector((0.0, y, z))).normalized() * n * 260
            if s in (0, steps):
                p.z = min(p.z, 0.0) - 40       # feet sunk into the ground
            ring.append(bm.verts.new(p))
        rings.append(ring)
    for s in range(steps):
        for k in range(sides):
            j = (k + 1) % sides
            bm.faces.new((rings[s][k], rings[s][j], rings[s + 1][j], rings[s + 1][k]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new("arch")
    bm.to_mesh(mesh)
    bm.free()
    arch = sb_kit._link("arch", mesh)
    for p in arch.data.polygons:
        p.use_smooth = False
    arch.data.materials.append(sb_kit.material("Sand"))
    # Talus boulders at each foot.
    parts = [arch]
    for i, (y, sx) in enumerate(((-3900, 1400), (-2600, 900), (3700, 1200), (2700, 800))):
        b = kit.rock("talus%d" % i, (sx, sx * 0.9, sx * 0.55), 60 + i, detail=2, roughness=0.3)
        b.location = (rng.uniform(-300, 300), y, 0)
        parts.append(b)
    return sb_kit.join("SM_SB_Kit_SandstoneArch", parts)


PIECES = (reactor_crown, condenser, watchtower, sandstone_arch)


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
        sb_kit.export_fbx(os.path.join(OUT, obj.name + ".fbx"), [obj])
        if render:
            import sb_render
            sb_render.render_views([obj], os.path.join(render, "hero_%s.png" % make.__name__),
                                   [((1.0, -0.9, 0.45), False), ((1.0, 0.0, 0.0), True)], floor=make in (watchtower, sandstone_arch))
    with open(os.path.join(OUT, "herokit_report.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


if __name__ == "__main__":
    previews = next((a.split("=", 1)[1] for a in sys.argv if a.startswith("--render=")), None)
    build(render=previews)
