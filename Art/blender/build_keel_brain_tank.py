"""Keel brain tank: an alien brain suspended in a wall specimen tank, mid-surgery (clamps, probes, cables).

Replaces Keel's teal circuit-glass wall panels (M_SB_KeelCircuit) and nothing else; see docs/KEEL_SHELL_KIT.md.

Units: 1 Blender unit = 1 cm, Unreal mesh space (X right, Y forward, Z up), like sb_kit. The pivot is the centre of the
tank's back face, which sits on the wall panel; local +X points out of the wall into the room, Y runs along the wall.
Outer size 270 (Y) x 210 (Z) x 82 (X).

Two meshes, because Nanite draws opaque materials only:
  SM_SB_Kit_KeelBrainTank       housing, back plate, light strips, the brain, clamps, probes and cables (Nanite)
  SM_SB_Kit_KeelBrainTankGlass  the bulged front glass and three fluid-haze cards (translucent, classic mesh)

The brain is sculpted procedurally: two hemispheres, a cerebellum and a brain stem, with gyri and sulci from the zero
crossings of a domain-warped noise field, a lateral fissure, and cerebellar folia. Tissue colour (pale gyri, dark wet
sulci, a fine net of veins) is baked into vertex colours; the engine material (Tools/create_keel_brain_tank.py) makes
it wet (clear coat). Also writes the haze texture Art/Export/Textures/T_SB_TankHaze.png.

Run headless:  blender --background --factory-startup --python Art/blender/build_keel_brain_tank.py
Exports FBX to Art/Export/MapKit (+ keel_brain_tank_report.txt). Import: Tools/ImportKeelBrainTank.bat.
"""
import json
import math
import os
import random
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Matrix, Vector, noise  # noqa: E402

import sb_kit  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Art/Export/MapKit"
PREFIX = "SM_SB_Kit_KeelBrainTank"

# Palette keys this piece adds (the importer makes MI_SB_<key> from them). Brain, glass and fluid are engine materials
# made by Tools/create_keel_brain_tank.py, so they are in LOCAL only and never written to palette.json.
TANK_PALETTE = {
    "TankGlow": ((0.55, 0.95, 0.90), 0.0, 0.30, 1.5),          # soft teal-white light strips inside the tank
    "TankChrome": ((0.80, 0.80, 0.83), 1.0, 0.12, 0.0),        # surgical steel: clamps, needles, probe bodies
    "TankCable": ((0.025, 0.025, 0.03), 0.0, 0.42, 0.0),       # black rubber cables
    "TankCableRed": ((0.30, 0.025, 0.03), 0.0, 0.38, 0.0),     # red insulated leads and feed tubes
}
LOCAL = {
    "TankBrain": ((0.70, 0.52, 0.55), 0.0, 0.25, 0.0),
    "TankGlass": ((0.05, 0.10, 0.10), 0.0, 0.03, 0.0),
    "TankFluid": ((0.20, 0.60, 0.55), 0.0, 0.50, 0.0),
}
sb_kit.PALETTE.update(TANK_PALETTE)
sb_kit.PALETTE.update(LOCAL)
# The shell kit's colours (housing steel, dark back, cyan probe collars), as build_keel_shell_kit.py wrote them.
for _key, _value in json.loads((ROOT / "Art/Export/palette.json").read_text()).items():
    if _key.startswith("Shell"):
        sb_kit.PALETTE[_key] = (tuple(_value[0]), _value[1], _value[2], _value[3])

# Tank geometry
HW, HH, R = 135.0, 105.0, 34.0     # outer half width (Y), half height (Z), corner radius
DEPTH = 82.0                       # bezel front
WALL = 20.0                        # housing wall thickness: the opening is the outer rect inset by this
GLASS_X = 68.0
BRAIN_CENTER = Vector((30.0, 0.0, 6.0))


# ---- helpers --------------------------------------------------------------------------------------------------------
def rounded_rect(hw, hh, r, per_corner=10):
    """Closed rounded rectangle in YZ, counter-clockwise seen from +X: list of ((y, z), outward normal (ny, nz))."""
    pts = []
    for cy, cz, a0 in ((hw - r, hh - r, 0.0), (-(hw - r), hh - r, 90.0), (-(hw - r), -(hh - r), 180.0), (hw - r, -(hh - r), 270.0)):
        for i in range(per_corner + 1):
            a = math.radians(a0 + 90.0 * i / per_corner)
            n = (math.cos(a), math.sin(a))
            pts.append(((cy + r * n[0], cz + r * n[1]), n))
    return pts


def sweep(name, path, section, mat, smooth=True):
    """Sweeps a closed (inset d, x) section around a closed path of ((y, z), outward normal)."""
    bm = bmesh.new()
    rings = []
    for (y, z), (ny, nz) in path:
        rings.append([bm.verts.new((x, y - ny * d, z - nz * d)) for d, x in section])
    n, m = len(rings), len(section)
    for i in range(n):
        a, b = rings[i], rings[(i + 1) % n]
        for j in range(m):
            k = (j + 1) % m
            bm.faces.new((a[j], a[k], b[k], b[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb_kit._link(name, me)
    for p in me.polygons:
        p.use_smooth = smooth
    obj.data.materials.append(sb_kit.material(mat))
    return obj


def fill_rr(name, hw, hh, r, x, mat):
    bm = bmesh.new()
    verts = [bm.verts.new((x, y, z)) for (y, z), _ in rounded_rect(hw, hh, r)]
    bm.faces.new(verts)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb_kit._link(name, me)
    obj.data.materials.append(sb_kit.material(mat))
    return obj


def seg(name, a, b, radius, mat, verts=10, radius2=None):
    """Cylinder from point a to point b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=radius, radius2=radius if radius2 is None else radius2, depth=d.length)
    rot = Vector((0, 0, 1)).rotation_difference(d.normalized()).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation((a + b) * 0.5) @ rot, verts=bm.verts)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb_kit._link(name, me)
    for p in me.polygons:
        p.use_smooth = True
    obj.data.materials.append(sb_kit.material(mat))
    return obj


def ball(name, c, radius, mat, subdiv=2):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=radius)
    bmesh.ops.translate(bm, vec=Vector(c), verts=bm.verts)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb_kit._link(name, me)
    for p in me.polygons:
        p.use_smooth = True
    obj.data.materials.append(sb_kit.material(mat))
    return obj


def cable(name, points, radius, mat):
    """A smooth cable through the given points (Bezier, auto handles), as a mesh tube."""
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.resolution_u = 10
    cu.bevel_depth = radius
    cu.bevel_resolution = 2
    cu.use_fill_caps = True
    sp = cu.splines.new("BEZIER")
    sp.bezier_points.add(len(points) - 1)
    for bp, p in zip(sp.bezier_points, points):
        bp.co = p
        bp.handle_left_type = bp.handle_right_type = "AUTO"
    obj = bpy.data.objects.new(name, cu)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(sb_kit.material(mat))
    return obj


# ---- the brain ------------------------------------------------------------------------------------------------------
def smoothstep(e0, e1, x):
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3 - 2 * t)


def lerp3(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


GYRUS = (0.64, 0.44, 0.48)      # pale pink-grey with a violet cast (alien)
SULCUS = (0.26, 0.05, 0.09)     # dark, wet, blood-red folds
VEIN = (0.36, 0.05, 0.22)       # purple-red surface vessels
STEM = (0.70, 0.54, 0.52)


def shaped_sphere(name, subdiv, shape, detail, colour):
    """Icosphere pushed into shape(unit dir) -> position, then displaced along its normals by detail(p) -> (d, groove),
    coloured by colour(p, groove) into the 'Col' point colours."""
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.0)
    for v in bm.verts:
        v.co = shape(v.co.normalized())
    bm.normal_update()
    grooves = []
    for v in bm.verts:
        d, g = detail(v.co)
        grooves.append(g)
        v.co = v.co + v.normal * d
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    attr = me.color_attributes.new(name="Col", type="BYTE_COLOR", domain="POINT")
    for i, v in enumerate(me.vertices):
        attr.data[i].color = (*colour(v.co, grooves[i]), 1.0)
    obj = sb_kit._link(name, me)
    for p in me.polygons:
        p.use_smooth = True
    obj.data.materials.append(sb_kit.material("TankBrain"))
    return obj


def warp(p, scale, amount):
    w = noise.noise_vector(p * scale, noise_basis="PERLIN_ORIGINAL")
    return p + w * amount


def cortex_detail(p):
    """Gyri and sulci: grooves along the zero crossings of a warped noise field, a lateral fissure, gentle domes."""
    q = warp(p, 0.025, 9.0)
    v = noise.noise(q * 0.11, noise_basis="PERLIN_ORIGINAL")
    g = math.exp(-(v / 0.11) ** 2)
    d = -3.4 * g + 0.9 * smoothstep(0.1, 0.5, abs(v))
    # Lateral (Sylvian) fissure on the outer face: a deep groove rising from the temporal pole toward the back.
    if abs(p.x) > 14.0:
        line_z = -6.0 + 0.32 * (p.y - 22.0)
        if -40.0 < p.y < 30.0:
            dist = abs(p.z - line_z)
            f = math.exp(-(dist / 2.2) ** 2) * smoothstep(-40.0, -25.0, p.y)
            d -= 5.0 * f
            g = max(g, f)
    return d, g


def cortex_colour(p, g):
    c = lerp3(GYRUS, SULCUS, smoothstep(0.15, 0.85, g))
    # A fine net of vessels on the gyri: thin zero crossings of a second field, only where a mask is high.
    w = noise.noise(warp(p, 0.04, 5.0) * 0.07 + Vector((31.7, 7.1, 3.3)), noise_basis="PERLIN_ORIGINAL")
    mask = smoothstep(0.05, 0.35, noise.noise(p * 0.03 + Vector((5.0, 9.0, 1.0)), noise_basis="PERLIN_ORIGINAL"))
    vein = math.exp(-(w / 0.025) ** 2) * mask * (1.0 - g)
    c = lerp3(c, VEIN, min(vein, 1.0) * 0.85)
    # Slow mottling so it is not one flat tone.
    m = noise.noise(p * 0.05 + Vector((2.0, 3.0, 4.0)), noise_basis="PERLIN_ORIGINAL") * 0.10
    return tuple(min(max(x + m, 0.0), 1.0) for x in c)


def hemisphere(side):
    cx = 11.0 * side

    def shape(u):
        x, y, z = u.x * 15.0, u.y * 58.0, u.z * 40.0
        if u.x * side < 0:
            x *= 0.35                                    # flat medial face along the midline
        if u.z < 0:
            z *= 0.72                                    # flatter base
            # temporal lobe: a rounded bulge down and out at the front-middle
            t = math.exp(-((y - 12.0) / 24.0) ** 2) * (-u.z)
            z -= 9.0 * t
            if u.x * side > 0:
                x += 3.0 * t
        z *= 1.0 + 0.10 * u.y                            # frontal lobe a little taller than the occipital
        return Vector((cx + x, y, z))

    return shaped_sphere("BrainHemi%+d" % side, 7, shape, cortex_detail, cortex_colour)


def cerebellum():
    def shape(u):
        x, y, z = u.x * 27.0, u.y * 17.0, u.z * 13.0
        x *= 1.0 - 0.12 * math.exp(-(u.x / 0.18) ** 2)   # a shallow notch on the midline
        return Vector((x, -41.0 + y, -22.0 + z))

    def detail(p):
        s = math.sin((p.z + 1.6 * noise.noise(p * 0.08)) * 1.05 + 0.4 * p.y * 0.1)
        g = smoothstep(0.75, 1.0, s)
        return -1.5 * g, g * 0.8

    def colour(p, g):
        return lerp3((0.70, 0.52, 0.54), SULCUS, smoothstep(0.1, 0.9, g))

    return shaped_sphere("Cerebellum", 6, shape, detail, colour)


def brain_stem():
    def shape(u):
        # a tapering column from under the brain down and slightly back
        # a rounded column (not a squeezed sphere): radial profile from the horizontal direction, rounded ends
        t = (u.z + 1.0) * 0.5                           # 0 bottom .. 1 top
        rho = math.hypot(u.x, u.y)
        r = (6.0 + 4.0 * t) * min(1.0, rho * 2.2)
        dx, dy = (u.x / rho, u.y / rho) if rho > 1e-6 else (0.0, 0.0)
        return Vector((dx * r, -24.0 + dy * r * 0.85 - (1.0 - t) * 5.0, -58.0 + t * 34.0))

    def detail(p):
        return 0.4 * noise.noise(p * 0.2), 0.0

    def colour(p, g):
        return STEM

    return shaped_sphere("BrainStem", 5, shape, detail, colour)


def build_brain():
    parts = [hemisphere(-1), hemisphere(1), cerebellum(), brain_stem()]
    # Tilt the front up a touch and put it in the tank.
    m = Matrix.Translation(BRAIN_CENTER) @ Matrix.Rotation(math.radians(-6.0), 4, "X")
    for obj in parts:
        obj.data.transform(m)
    return parts


# ---- tank -----------------------------------------------------------------------------------------------------------
def build_housing():
    path = rounded_rect(HW, HH, R, per_corner=12)
    # (inset from the outer edge, x): back flange, outer wall, rounded bezel, inner lip, glass seat, inner wall
    section = [(0.0, 0.0), (0.0, 74.0), (2.0, 79.0), (6.0, DEPTH), (16.0, DEPTH), (21.0, 78.0), (22.0, 71.0),
               (20.0, 66.0), (20.0, 4.0), (16.0, 0.0)]
    parts = [sweep("Housing", path, section, "ShellSteel")]
    # Back plate, dark, with horizontal ribs and a ring of soft light around its edge (rims the brain from behind).
    parts.append(fill_rr("Back", HW - WALL, HH - WALL, R - WALL, 3.0, "ShellGrate"))
    for z in (-60.0, -30.0, 0.0, 30.0, 60.0):
        parts.append(sb_kit.box("Rib%d" % z, (5.0, 0.0, z), (4.0, 2 * (HW - WALL) - 30.0, 5.0), "ShellSteel", bevel=1.2, segments=2))
    glow_section = [(20.0, 4.0), (20.0, 9.0), (23.5, 9.0), (23.5, 4.0)]
    parts.append(sweep("BackGlow", rounded_rect(HW - 1.5, HH - 1.5, R - 1.5, per_corner=12), glow_section, "TankGlow", smooth=False))
    # A light bar along the inside top, behind the bezel: lights the brain from above.
    parts.append(sb_kit.box("TopLight", (58.0, 0.0, HH - WALL - 1.5), (10.0, 2 * (HW - WALL) - 70.0, 3.0), "TankGlow", bevel=0.8))
    return parts


def build_surgery():
    parts = []
    rnd = random.Random(7)
    top = HH - WALL
    # Retractor clamps: a rod from the housing wall, a knuckle, and two splayed jaws biting into the cortex.
    clamps = [
        ((40.0, -50.0, top), (50.0, -30.0, 38.0)),
        ((40.0, 45.0, top), (52.0, 34.0, 36.0)),
        ((30.0, HW - WALL, 0.0), (50.0, 54.0, 4.0)),
        ((30.0, -(HW - WALL), -10.0), (48.0, -50.0, -6.0)),
    ]
    for i, (wall, tip) in enumerate(clamps):
        wall, tip = Vector(wall), Vector(tip)
        d = (tip - wall).normalized()
        parts.append(sb_kit.box("ClampFoot%d" % i, tuple(wall), (8.0, 8.0, 8.0), "ShellSteel", bevel=1.0))
        knuckle = tip - d * 6.0
        parts.append(seg("ClampRod%d" % i, wall, knuckle, 1.1, "TankChrome"))
        parts.append(ball("ClampKnuckle%d" % i, knuckle, 2.4, "TankChrome"))
        side = d.cross(Vector((1.0, 0.0, 0.0)))
        if side.length < 0.1:
            side = d.cross(Vector((0.0, 0.0, 1.0)))
        side.normalize()
        for s in (-1.0, 1.0):
            jaw = tip + side * 4.5 * s
            parts.append(seg("ClampJaw%d%+d" % (i, s), knuckle, jaw, 0.6, "TankChrome", verts=8))
            parts.append(seg("ClampTooth%d%+d" % (i, s), jaw, jaw + d * 2.5 - side * 1.2 * s, 0.45, "TankChrome", verts=8, radius2=0.15))
        # A lead from each clamp back to the top of the tank
        parts.append(cable("ClampLead%d" % i, [tuple(knuckle), tuple(knuckle + Vector((-8.0, 0.0, 10.0))), (8.0, wall.y * 0.6, top - 4.0)],
                           0.45, "TankCableRed" if i % 2 else "TankCable"))
    # Probes: steel bodies hanging from the top, needles into the cortex, a glowing collar, a cable each.
    probes = [(-22.0, 44.0, 26.0), (4.0, 50.0, 44.0), (28.0, 48.0, 30.0), (-38.0, 42.0, 10.0), (16.0, 56.0, 12.0)]
    for i, (y, x, ztip) in enumerate(probes):
        tip = Vector((x, y, ztip))
        body_top = Vector((x - 6.0 + rnd.uniform(-3, 3), y + rnd.uniform(-6, 6), ztip + 30.0))
        d = (tip - body_top).normalized()
        body_bottom = body_top + d * 14.0
        parts.append(seg("ProbeBody%d" % i, body_top, body_bottom, 1.7, "TankChrome", verts=12))
        parts.append(seg("ProbeCollar%d" % i, body_bottom, body_bottom + d * 1.5, 1.9, "ShellGlowCyan", verts=12))
        parts.append(seg("ProbeNeedle%d" % i, body_bottom + d * 1.5, tip + d * 3.0, 0.35, "TankChrome", verts=8, radius2=0.12))
        parts.append(cable("ProbeCable%d" % i, [tuple(body_top), tuple(body_top + Vector((-4.0, rnd.uniform(-10, 10), 12.0))),
                                                 (6.0, y * 0.8 + rnd.uniform(-15, 15), top - 3.0)], 0.5, "TankCable"))
    # Feed tubes from the brain stem down to the floor of the tank, sagging slightly.
    for i, (y, x) in enumerate(((-30.0, 28.0), (-18.0, 40.0))):
        start = Vector((x, -27.0 + i * 4.0, -50.0))
        parts.append(cable("Feed%d" % i, [tuple(start), (x - 4.0, y - 6.0, -70.0), (10.0, y - 20.0, -(HH - WALL) + 2.0)],
                           1.3, "TankCableRed" if i == 0 else "TankCable"))
    # Sockets on the tank floor for the feed tubes
    for i, y in enumerate((-56.0, -38.0)):
        parts.append(seg("FeedSocket%d" % i, (10.0, y, -(HH - WALL)), (10.0, y, -(HH - WALL) + 5.0), 2.6, "ShellSteel"))
    return parts


def build_glass():
    parts = []
    # Front pane, bulged a little so it catches curved reflections; tucked into the bezel's seat at the edge.
    gw, gh, nx, nz = HW - WALL + 2.0, HH - WALL + 2.0, 28, 22
    bm = bmesh.new()
    grid = []
    for j in range(nz + 1):
        row = []
        for i in range(nx + 1):
            y = -gw + 2 * gw * i / nx
            z = -gh + 2 * gh * j / nz
            bulge = 5.0 * (1 - (y / gw) ** 2) * (1 - (z / gh) ** 2)
            row.append(bm.verts.new((GLASS_X + bulge, y, z)))
        grid.append(row)
    for j in range(nz):
        for i in range(nx):
            bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
    for f in bm.faces:
        if f.normal.x < 0:
            f.normal_flip()
    me = bpy.data.meshes.new("Glass")
    bm.to_mesh(me)
    bm.free()
    glass = sb_kit._link("Glass", me)
    for p in me.polygons:
        p.use_smooth = True
    glass.data.materials.append(sb_kit.material("TankGlass"))
    parts.append(glass)
    # Haze cards through the fluid (the material fades them where they cut the brain).
    for k, x in enumerate((14.0, 36.0, 58.0)):
        bm = bmesh.new()
        hw, hh = HW - WALL - 1.0, HH - WALL - 1.0
        vs = [bm.verts.new((x, y, z)) for y, z in ((-hw, -hh), (hw, -hh), (hw, hh), (-hw, hh))]
        f = bm.faces.new(vs)
        if f.normal.x < 0:
            f.normal_flip()
        me = bpy.data.meshes.new("Haze%d" % k)
        bm.to_mesh(me)
        bm.free()
        obj = sb_kit._link("Haze%d" % k, me)
        obj.data.materials.append(sb_kit.material("TankFluid"))
        uv = me.uv_layers.new(name="UVMap")
        for poly in me.polygons:
            for li in poly.loop_indices:
                co = me.vertices[me.loops[li].vertex_index].co
                # each card a different window of the haze texture
                uv.data[li].uv = (co.y / 260.0 + k * 0.37, co.z / 260.0 + k * 0.21)
        parts.append(obj)
    return parts


def join_with_colours(name, objects):
    """Applies modifiers (curves become meshes), gives every part the 'Col' point colours (white where it had none) so
    the join keeps the brain's, and joins them into one object named name."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    meshes = []
    for obj in objects:
        me = bpy.data.meshes.new_from_object(obj.evaluated_get(depsgraph), depsgraph=depsgraph)
        me.transform(obj.matrix_world)
        if "Col" not in me.color_attributes:
            a = me.color_attributes.new(name="Col", type="BYTE_COLOR", domain="POINT")
            for d in a.data:
                d.color = (1.0, 1.0, 1.0, 1.0)
        meshes.append((obj.name, me))
    for obj in objects:
        bpy.data.objects.remove(obj, do_unlink=True)
    new_objs = []
    for n, me in meshes:
        o = bpy.data.objects.new(n, me)
        bpy.context.scene.collection.objects.link(o)
        new_objs.append(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in new_objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = new_objs[0]
    bpy.ops.object.join()
    result = bpy.context.view_layer.objects.active
    result.name = name
    result.data.name = name
    return result


def box_uvs(obj, scale=1.0 / 200.0):
    me = obj.data
    uv = me.uv_layers[0] if me.uv_layers else me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        nx, ny, nz = (abs(c) for c in poly.normal)
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            if nz >= nx and nz >= ny:
                uv.data[li].uv = (co.x * scale, co.y * scale)
            elif nx >= ny:
                uv.data[li].uv = (co.y * scale, co.z * scale)
            else:
                uv.data[li].uv = (co.x * scale, co.z * scale)


def haze_texture(path, size=512, seed=11):
    """Tileable soft fluid haze: 1/f^1.6 filtered noise (FFT, so it wraps), R = density, G = a finer speck layer."""
    import numpy as np
    rng = np.random.default_rng(seed)
    fy = np.fft.fftfreq(size)[:, None]
    fx = np.fft.fftfreq(size)[None, :]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1.0

    def field(power, lo):
        spec = np.fft.fft2(rng.standard_normal((size, size))) / (f ** power)
        spec[f < lo] = 0
        out = np.real(np.fft.ifft2(spec))
        out -= out.min()
        return out / out.max()

    density = field(1.6, 1.0 / size) ** 1.4
    specks = np.clip((field(0.6, 8.0 / size) - 0.78) * 6.0, 0.0, 1.0)
    img = bpy.data.images.new("T_SB_TankHaze", size, size, alpha=False, float_buffer=False)
    rgba = np.ones((size, size, 4), dtype=np.float32)
    rgba[..., 0] = density
    rgba[..., 1] = specks
    rgba[..., 2] = 0.0
    img.pixels.foreach_set(rgba.ravel())
    img.filepath_raw = str(path)
    img.file_format = "PNG"
    img.colorspace_settings.name = "Non-Color"
    img.save()


def main():
    sb_kit.reset_scene()
    tank = build_housing() + build_brain() + build_surgery()
    tank_obj = join_with_colours(PREFIX, tank)
    box_uvs(tank_obj)
    glass_obj = join_with_colours(PREFIX + "Glass", build_glass())
    OUT.mkdir(parents=True, exist_ok=True)
    lines = []
    for obj in (tank_obj, glass_obj):
        line = f"{obj.name}: {sb_kit.triangle_count(obj)} tris, {sb_kit.bounds(obj)}, slots " + ", ".join(m.name for m in obj.data.materials)
        sb_kit.export_fbx(str(OUT / (obj.name + ".fbx")), [obj])
        lines.append(line)
        print(line)
    (OUT / "keel_brain_tank_report.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    palette_file = ROOT / "Art/Export/palette.json"
    palette = json.loads(palette_file.read_text())
    for key, (colour, metallic, roughness, emission) in TANK_PALETTE.items():
        palette[key] = [list(colour), metallic, roughness, emission]
    with open(palette_file, "w", newline="") as f:
        f.write(json.dumps(palette, indent=1).replace("\n", "\r\n") + "\r\n")
    haze_texture(ROOT / "Art/Export/Textures/T_SB_TankHaze.png")
    print("RESULT OK")


if __name__ == "__main__":
    main()
