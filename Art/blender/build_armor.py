"""Builds Shieldbreak's soldier armor around the Lyra mannequin and exports one FBX per bone.

    python build_armor.py <bones.json> [SKM_Manny.obj | --proxy] [--render] [--preview-dir <dir>]

bones.json comes from Tools/ExportArtReference.bat, the skin from Tools/DumpMannequin.bat. Each bone gets a dark
under-layer (the convex hull of the body vertices that bone drives, pushed outward and simplified) with designed
plates shrink-wrapped over it (see conform()): team-coloured plates, gunmetal trim, glow lights, a smooth helmet with
a proud visor and a backpack. Pieces are exported in the mannequin's mesh space, and the
game attaches each to its bone with the bone's inverse reference pose (UShieldbreakBodyAnimator::AttachArmor), so
they sit where they were modeled and follow the animation and the ragdoll.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit as k  # noqa: E402
import bpy  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402


# ---- Loading the mannequin (Unreal mesh space: cm; X = the mannequin's left, Y = forward, Z = up) -------------
# Inputs come from the laptop: bones.json (Tools/ExportArtReference.bat, reference pose in component space) and the
# skin as SKM_Manny.obj (Tools/DumpMannequin.bat). Without the skin, a proxy body of capsules stands in.

# (bone, child or offset, (start radius x, y), (end radius x, y)): segments that claim skin vertices. Radii are only
# used by the proxy body.
SEGMENTS = [
    ("pelvis", "spine_01", (16, 11), (15, 10.5)),
    ("pelvis", (0, 0, -9), (16, 11), (15, 10)),
    ("spine_01", "spine_02", (15, 10.5), (14.5, 10)),
    ("spine_02", "spine_03", (14.5, 10), (15, 10.5)),
    ("spine_03", "spine_04", (15, 10.5), (16.5, 11.5)),
    ("spine_04", "spine_05", (16.5, 11.5), (17, 12)),
    ("spine_05", "neck_01", (17, 12), (12, 9)),
    ("neck_01", "head", (6, 6.5), (6, 6.5)),
    ("head", (0, 1.5, 18), (8.5, 10.5), (8, 10)),
    ("clavicle_l", "upperarm_l", (7, 7), (6.5, 6.5)),
    ("clavicle_r", "upperarm_r", (7, 7), (6.5, 6.5)),
    ("upperarm_l", "lowerarm_l", (6.2, 6.2), (4.6, 4.6)),
    ("upperarm_r", "lowerarm_r", (6.2, 6.2), (4.6, 4.6)),
    ("lowerarm_l", "hand_l", (4.6, 4.6), (3.4, 3.0)),
    ("lowerarm_r", "hand_r", (4.6, 4.6), (3.4, 3.0)),
    ("thigh_l", "calf_l", (8.5, 8.5), (6.0, 6.0)),
    ("thigh_r", "calf_r", (8.5, 8.5), (6.0, 6.0)),
    ("calf_l", "foot_l", (6.0, 6.0), (4.0, 4.0)),
    ("calf_r", "foot_r", (6.0, 6.0), (4.0, 4.0)),
    ("foot_l", "ball_l", (4.5, 5.0), (4.5, 3.5)),
    ("foot_r", "ball_r", (4.5, 5.0), (4.5, 3.5)),
    ("ball_l", (0, 7, 0), (4.5, 3.0), (4.0, 2.5)),
    ("ball_r", (0, 7, 0), (4.5, 3.0), (4.0, 2.5)),
]


def load_bones(path):
    import json
    with open(path) as f:
        data = json.load(f)
    return {name: Vector(b["loc"]) for name, b in data.items()}


def _segments(j):
    out = []
    for bone, end, r0, r1 in SEGMENTS:
        a = j[bone]
        b = j[end] if isinstance(end, str) else a + Vector(end)
        out.append((bone, a, b, r0, r1))
    return out


def _closest_bone(p, segs):
    best, best_d = None, 1e9
    for bone, a, b, _, _ in segs:
        ab = b - a
        t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-6)))
        d = (p - (a + ab * t)).length
        if d < best_d:
            best, best_d = bone, d
    return best


def load_skin(obj_path, j):
    segs = _segments(j)
    points = []
    with open(obj_path) as f:
        for line in f:
            if line.startswith("v "):
                x, y, z = map(float, line.split()[1:4])
                points.append(Vector((x, y, z)))
    verts = [(p, _closest_bone(p, segs), None) for p in points]
    print("skin: %d vertices from %s" % (len(verts), obj_path))
    return verts


def proxy_skin(j):
    """Capsule body along the bones: elliptical rings every 2 cm (X/Y radii in the segment's frame)."""
    verts = []
    for bone, a, b, r0, r1 in _segments(j):
        axis = (b - a)
        length = max(axis.length, 0.01)
        axis.normalize()
        ref = Vector((0, 1, 0)) if abs(axis.y) < 0.9 else Vector((0, 0, 1))
        u = ref.cross(axis).normalized()   # across (mostly X for vertical segments)
        w = axis.cross(u).normalized()      # depth (mostly Y)
        steps = max(2, int(length / 2.0))
        for i in range(steps + 1):
            t = i / steps
            rx = r0[0] + (r1[0] - r0[0]) * t
            ry = r0[1] + (r1[1] - r0[1]) * t
            c = a + axis * (length * t)
            for k in range(16):
                ang = 2 * math.pi * k / 16
                verts.append((c + u * (math.cos(ang) * rx) + w * (math.sin(ang) * ry), bone, None))
    print("proxy skin: %d points" % len(verts))
    return verts


# ---- Shell construction --------------------------------------------------------------------------------------

def gather(verts, bones, min_weight=0.5, clip=None):
    """Skin vertices claimed by any of bones (nearest bone segment), optionally filtered by clip(co)."""
    keep = set(bones)
    return [co.copy() for co, bone, _ in verts if bone in keep and (clip is None or clip(co))]


def shell(name, points, inflate, simplify_angle=12.0, scale=(1.0, 1.0, 1.0), pivot=None):
    """Convex hull of points, inflated by `inflate` cm along the hull normals, scaled about pivot, and simplified to
    big planar facets (limited dissolve) so it reads as armor plates."""
    bm = bmesh.new()
    for p in points:
        bm.verts.new(p)
    result = bmesh.ops.convex_hull(bm, input=bm.verts[:], use_existing_faces=False)
    for v in result.get("geom_interior", []) + result.get("geom_unused", []):
        if isinstance(v, bmesh.types.BMVert) and v.is_valid:
            bm.verts.remove(v)
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(simplify_angle), verts=bm.verts[:], edges=bm.edges[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(simplify_angle * 0.6), verts=bm.verts[:], edges=bm.edges[:])
    # Tiny edges left by the dissolves make the bevel throw spikes; weld them away.
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=0.4)
    bmesh.ops.dissolve_degenerate(bm, dist=0.05, edges=bm.edges[:])
    bm.normal_update()
    center = sum((v.co for v in bm.verts), Vector()) / max(len(bm.verts), 1)
    pivot = Vector(pivot) if pivot is not None else center
    for v in bm.verts:
        v.co = v.co + v.normal * inflate
        d = v.co - pivot
        v.co = pivot + Vector((d.x * scale[0], d.y * scale[1], d.z * scale[2]))
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def cut(obj, point, normal, fill=True):
    """Keeps the side of the plane that normal points away from (removes geometry on the +normal side)."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    res = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=Vector(point), plane_no=Vector(normal), clear_outer=True)
    if fill:
        edges = [e for e in res["geom_cut"] if isinstance(e, bmesh.types.BMEdge)]
        if edges:
            bmesh.ops.holes_fill(bm, edges=edges, sides=0)
    bm.to_mesh(obj.data)
    bm.free()
    return obj


def paint(obj, rule):
    """rule(face_center, face_normal) -> palette name, per face. Adds the slots it uses."""
    used = []
    names = []
    for poly in obj.data.polygons:
        c = obj.matrix_world @ poly.center
        n = (obj.matrix_world.to_3x3() @ poly.normal).normalized()
        names.append(rule(c, n))
    for n in names:
        if n not in used:
            used.append(n)
    obj.data.materials.clear()
    for n in used:
        obj.data.materials.append(k.material(n))
    for poly, n in zip(obj.data.polygons, names):
        poly.material_index = used.index(n)
    return obj


def finish(obj, bevel=0.5):
    mod = obj.modifiers.new("Bevel", "BEVEL")
    mod.width = bevel
    mod.segments = 1
    mod.limit_method = "ANGLE"
    mod.angle_limit = math.radians(25)
    mod.harden_normals = True
    return obj


def plate(name, points, inflate, rule, cuts=(), bevel=0.5, simplify=12.0, scale=(1, 1, 1), pivot=None):
    obj = shell(name, points, inflate, simplify, scale, pivot)
    for point, normal in cuts:
        cut(obj, point, normal)
    paint(obj, rule)
    return finish(obj, bevel)


# ---- Layered plates (v2, 2026-11-04) ---------------------------------------------------------------------------
# The first set was one faceted hull per bone in flat colours, which Vincent called plain. v2 keeps the hulls as a
# dark under-layer that is fitted to the skin (so nothing clips), and lays designed plates over them: each plate is a
# grid shrink-wrapped onto the hull, thickened and bevelled, so it follows the body and leaves a dark seam at its
# edge. Plates, the helmet and the backpack carry the team colour; the hulls are gunmetal and undersuit.

def bvh_of(obj):
    from mathutils.bvhtree import BVHTree
    depsgraph = bpy.context.evaluated_depsgraph_get()
    return BVHTree.FromObject(obj, depsgraph)


def conform(name, bvh, center, normal, up, half_w, half_h, mat, thickness=1.0, offset=0.6, res=(7, 6), chamfer=0.35,
            bevel=0.35, curve=0.0, taper=0.0):
    """A plate shrink-wrapped onto bvh: a res grid spanning +-half_w along (up x normal) and +-half_h along up, centered
    on center, each point projected along -normal onto the surface and lifted by offset. chamfer cuts the corners (0..1
    of the smaller half size). taper narrows the +up edge (0..1). curve bends the edges back toward the body (cm)."""
    n = Vector(normal).normalized()
    u_axis = Vector(up)
    u_axis = (u_axis - n * u_axis.dot(n)).normalized()
    s_axis = u_axis.cross(n).normalized()
    c = Vector(center)
    nx, ny = res
    bm = bmesh.new()
    front = {}
    for i in range(nx + 1):
        for j in range(ny + 1):
            a = -1.0 + 2.0 * i / nx
            b = -1.0 + 2.0 * j / ny
            # Chamfered rectangle: pull points past the cut corner back onto the chamfer line.
            lim = 2.0 - chamfer
            if abs(a) + abs(b) > lim:
                scale = lim / (abs(a) + abs(b))
                a, b = a * scale, b * scale
            w = half_w * (1.0 - taper * max(0.0, b) * 0.5 * (1 + b) / 2)
            p = c + s_axis * (a * w) + u_axis * (b * half_h)
            origin = p + n * 40.0
            hit, hit_n, _, _ = bvh.ray_cast(origin, -n, 80.0)
            if hit is None:
                hit, hit_n, _, _ = bvh.find_nearest(p)
            edge = max(abs(a), abs(b))
            lift = offset - curve * edge * edge
            front[(i, j)] = bm.verts.new(hit + n * lift)
    back = {key: bm.verts.new(v.co - n * thickness) for key, v in front.items()}
    for i in range(nx):
        for j in range(ny):
            q = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
            bm.faces.new([front[k] for k in q])
            bm.faces.new([back[k] for k in reversed(q)])
    rim = [(i, 0) for i in range(nx)] + [(nx, j) for j in range(ny)] + [(i, ny) for i in range(nx, 0, -1)] + [(0, j) for j in range(ny, 0, -1)]
    for idx, key in enumerate(rim):
        nxt = rim[(idx + 1) % len(rim)]
        bm.faces.new((front[nxt], front[key], back[key], back[nxt]))
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=0.05)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(k.material(mat))
    for poly in obj.data.polygons:
        poly.use_smooth = True
    if bevel > 0:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        mod.limit_method = "ANGLE"
        mod.angle_limit = math.radians(40)
        mod.harden_normals = True
    return obj


def smooth_by_angle(obj, angle=35.0):
    for poly in obj.data.polygons:
        poly.use_smooth = True
    try:
        obj.data.set_sharpness_by_angle(angle=math.radians(angle))
    except AttributeError:
        pass
    return obj


def ellipsoid(name, center, radii, mat, segments=20, rings=12, shape=None):
    """A UV ellipsoid; shape(local Vector) -> Vector reshapes each vertex (local = offset from center, unit radii)."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings, radius=1.0)
    for v in bm.verts:
        local = v.co.copy()
        if shape:
            local = shape(local)
        v.co = Vector(center) + Vector((local.x * radii[0], local.y * radii[1], local.z * radii[2]))
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(k.material(mat))
    return smooth_by_angle(obj, 50)


def keep_faces(obj, test, new_name=None, mat=None, push=0.0):
    """Copies the faces of obj whose (center, normal) pass test into a new object, pushed out by push cm."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.normal_update()
    drop = [f for f in bm.faces if not test(f.calc_center_median(), f.normal)]
    bmesh.ops.delete(bm, geom=drop, context="FACES")
    for v in bm.verts:
        v.co = v.co + v.normal * push
    mesh = bpy.data.meshes.new(new_name or obj.name + "_part")
    bm.to_mesh(mesh)
    bm.free()
    part = bpy.data.objects.new(mesh.name, mesh)
    bpy.context.scene.collection.objects.link(part)
    part.data.materials.clear()
    part.data.materials.append(k.material(mat))
    return smooth_by_angle(part, 50)


def solidify(obj, thickness):
    mod = obj.modifiers.new("Solidify", "SOLIDIFY")
    mod.thickness = thickness
    mod.offset = -1.0
    return obj


def build(verts, j, jt):
    """Returns {bone: [objects]} in Unreal mesh space (X = the mannequin's left, Y forward, Z up)."""
    pieces = {}
    side = {"l": 1.0, "r": -1.0}
    up = Vector((0, 0, 1))

    # ---- Helmet: an angular, faceted shell (in keeping with the plates) with a ridge along the crown, a squared jaw,
    # a proud wrap-around visor under a brow, and flush ear modules with a team light.
    head_c = j["head"] + Vector((0, 2.2, 9.5))

    def helmet_shape(v):
        # A rounded box (superellipsoid): squarer cheeks and crown than a ball, so it reads as armour, not a crash
        # helmet. Then a jaw pushed forward and a rear neck flare.
        def sq(x, p=0.72):
            return math.copysign(abs(x) ** p, x)
        out = Vector((sq(v.x), sq(v.y, 0.8), sq(v.z, 0.85)))
        if v.z < -0.15 and v.y > 0.0:                              # squared jaw pushed forward
            out.y = out.y * 1.1 + 0.04
            out.x = out.x * 0.9
        if v.z < -0.35 and v.y < -0.1:                             # rear neck flare
            out.y = out.y * 1.08
        if v.z < -0.35:                                            # widen the base over the collar
            out.x = out.x * 1.05
        return out

    head_c = head_c - Vector((0, 0, 0.8))
    dome = ellipsoid("helmet", head_c, (11.0, 12.6, 12.6), "Team", segments=16, rings=10, shape=helmet_shape)
    cut(dome, head_c - Vector((0, 0, 12.2)), (0, 0, -1), fill=True)
    paint(dome, lambda c, n: "Gunmetal" if c.z < head_c.z - 6.0 or n.z < -0.6 else "Team")
    smooth_by_angle(dome, 22)

    def visor_test(c, n):
        rel = c - head_c
        return n.y > 0.35 and -4.5 < rel.z < 3.4 and abs(rel.x) < 10.5
    visor = keep_faces(dome, visor_test, "visor", "Visor", push=0.5)
    smooth_by_angle(visor, 22)
    solidify(visor, 0.5)
    hb = bvh_of(dome)
    brow = conform("brow", hb, head_c + Vector((0, 11.5, 4.6)), (0, 1, 0.45), up, 10.0, 1.3, "Gunmetal",
                   thickness=0.8, offset=0.7, res=(8, 2), chamfer=0.5, bevel=0.25)
    helmet_parts = [dome, visor, brow]
    for s, sx in side.items():
        ear = head_c + Vector((sx * 10.9, -1.5, -1.5))
        helmet_parts.append(k.cyl("ear_" + s, ear, 3.0, 2.0, "Gunmetal", axis="X", verts=8, bevel=0.4))
        helmet_parts.append(k.cyl("ear_glow_" + s, ear + Vector((sx * 1.05, 0, 0)), 1.3, 0.3, "TeamGlow", axis="X", verts=8, bevel=0))
        # Jaw vents either side of the chin.
        helmet_parts.append(conform("jaw_" + s, hb, head_c + Vector((sx * 5.5, 11.0, -8.0)), (sx * 0.5, 1, -0.1), up, 2.2, 2.4,
                                    "Gunmetal", thickness=0.7, offset=0.5, res=(2, 3), chamfer=0.3, bevel=0.2))
    # Mouth guard: a squared, vented block under the visor.
    chin = head_c + Vector((0, 11.6, -8.6))
    helmet_parts.append(k.box("mouthguard", chin, (8.0, 3.4, 5.4), "Gunmetal", bevel=1.0, segments=2, taper=(0.72, 0.8)))
    for i in range(3):
        helmet_parts.append(k.box("mouth_vent", chin + Vector((0, 1.75, 1.4 - i * 1.4)), (4.4, 0.3, 0.5), "Polymer", bevel=0))
    pieces["head"] = helmet_parts

    # ---- Torso under-layer: hulls fitted to the skin, dark.
    chest = gather(verts, ["spine_05", "spine_04", "clavicle_l", "clavicle_r"], 0.55,
                   clip=lambda c: c.z > j["spine_04"].z - 2)
    chest_hull = plate("chest_base", chest, 2.2, lambda c, n: "Gunmetal" if n.z > -0.3 else "Undersuit",
                       cuts=[(j["neck_01"] + Vector((0, 0, 4)), (0, 0, 1))], scale=(1.06, 1.05, 1.0), pivot=j["spine_05"], simplify=8)
    cb = bvh_of(chest_hull)
    s5 = j["spine_05"]
    torso = [chest_hull]
    for s, sx in side.items():
        # Pectoral plates, angled out, with a narrow team light at the inner edge.
        torso.append(conform("pec_" + s, cb, s5 + Vector((sx * 8.6, 14.0, 1.0)), (sx * 0.3, 1, 0.08), up, 7.6, 7.4, "Team",
                             thickness=1.4, offset=1.1, chamfer=0.28, curve=0.4, taper=-0.25))
        torso.append(conform("rib_" + s, cb, s5 + Vector((sx * 14.5, 6.0, -8.0)), (sx * 1, 0.45, 0), up, 4.5, 5.0, "Team",
                             thickness=1.0, offset=0.8, res=(4, 5), chamfer=0.5))
        # Shoulder yokes over the trapezius.
        torso.append(conform("yoke_" + s, cb, s5 + Vector((sx * 9.0, -1.0, 10.5)), (sx * 0.2, 0, 1), (0, 1, 0), 5.0, 9.0, "Gunmetal",
                             thickness=1.0, offset=0.7, res=(4, 6), chamfer=0.4))
        # Back plates either side of the pack.
        torso.append(conform("lat_" + s, cb, s5 + Vector((sx * 11.0, -12.0, -2.0)), (sx * 0.4, -1, 0), up, 6.0, 9.0, "Team",
                             thickness=1.0, offset=0.8, chamfer=0.45))
    torso.append(conform("sternum", cb, s5 + Vector((0, 15.5, -5.0)), (0, 1, 0), up, 2.4, 9.0, "Gunmetal", thickness=1.4,
                         offset=1.3, res=(2, 6), chamfer=0.4, taper=0.6))
    torso.append(conform("sternum_light", cb, s5 + Vector((0, 15.5, -9.0)), (0, 1, 0), up, 0.5, 2.6, "TeamGlow", thickness=0.3,
                         offset=2.9, res=(1, 2), chamfer=0.0, bevel=0))
    # Backpack: the power cell, with two glowing vents and two exhaust ports.
    pack_c = s5 + Vector((0, -19.5, -2.0))
    torso.append(k.box("pack", pack_c, (19.0, 8.0, 24.0), "Gunmetal", bevel=1.6, segments=2, taper=(0.86, 0.92)))
    torso.append(k.box("pack_cap", pack_c + Vector((0, -4.1, 2.5)), (13.0, 1.4, 14.0), "Team", bevel=0.5, segments=2))
    for sx in (1, -1):
        torso.append(k.box("pack_vent", pack_c + Vector((sx * 4.4, -4.85, -1.5)), (1.0, 0.4, 9.0), "TeamGlow", bevel=0))
        torso.append(k.cyl("pack_port", pack_c + Vector((sx * 5.5, -1.0, -13.0)), 2.0, 2.4, "Polymer", axis="Z", verts=10, bevel=0.3))
    # Collar ring around the neck.
    torso.append(k.cyl("collar", j["neck_01"] + Vector((0, 1.0, -2.0)), 9.0, 4.0, "Gunmetal", axis="Z", verts=20, bevel=0.6, radius2=8.0))
    pieces["spine_05"] = torso

    # ---- Abdomen: a dark core with three stacked armour bands at the front.
    belly = gather(verts, ["spine_02", "spine_03"], 0.55)
    belly_hull = plate("abdomen", belly, 1.4, lambda c, n: "Undersuit", scale=(0.97, 0.97, 0.88), pivot=j["spine_03"], simplify=10)
    bb = bvh_of(belly_hull)
    abdomen = [belly_hull]
    for i, dz in enumerate((5.0, 0.0, -5.0)):
        abdomen.append(conform("band_%d" % i, bb, j["spine_03"] + Vector((0, 12.0, dz)), (0, 1, 0), up, 9.5 - i * 0.6, 2.1,
                               "Gunmetal", thickness=1.0, offset=0.9, res=(6, 2), chamfer=0.5, curve=0.6))
    pieces["spine_03"] = abdomen

    # ---- Pelvis: belt, a front plate and hip plates.
    hips = gather(verts, ["pelvis"], 0.6, clip=lambda c: c.z > j["thigh_l"].z - 9)
    hip_hull = plate("pelvis", hips, 2.0, lambda c, n: "Gunmetal" if c.z > j["pelvis"].z + 2 else "Undersuit",
                     scale=(1.05, 1.05, 1.0), pivot=j["pelvis"], simplify=10)
    hb = bvh_of(hip_hull)
    pelvis = [hip_hull, conform("codpiece", hb, j["pelvis"] + Vector((0, 13.0, -2.0)), (0, 1, 0.1), up, 6.0, 5.0, "Gunmetal",
                                thickness=1.2, offset=0.8, res=(4, 4), chamfer=0.2, taper=-0.6)]
    for s, sx in side.items():
        pelvis.append(conform("hip_" + s, hb, j["pelvis"] + Vector((sx * 14.0, 2.0, -3.0)), (sx, 0.2, 0), up, 6.0, 6.0, "Team",
                              thickness=1.0, offset=0.9, res=(5, 5), chamfer=0.45))
    pelvis.append(k.box("buckle", j["pelvis"] + Vector((0, 14.2, 4.5)), (6.0, 1.2, 2.6), "Gunmetal", bevel=0.4, segments=2))
    pieces["pelvis"] = pelvis

    for s, sx in side.items():
        out = Vector((sx, 0, 0))
        # ---- Shoulders: a fitted dark cap with a big team pauldron and a trim band on top.
        shoulder = gather(verts, ["upperarm_" + s, "upperarm_twist_01_" + s, "clavicle_" + s], 0.45,
                          clip=lambda c, s=s: (c - j["upperarm_" + s]).length < 15)
        sh = j["upperarm_" + s]
        arm_dir = (j["lowerarm_" + s] - sh).normalized()
        sh_hull = plate("shoulder_base_" + s, shoulder, 2.4, lambda c, n: "Gunmetal",
                        cuts=[(sh + arm_dir * 12, arm_dir), (sh - out * 6, -out)], scale=(1.08, 1.08, 1.08), pivot=sh, simplify=10)
        sb = bvh_of(sh_hull)
        pauldron_n = (out * 0.75 + up * 0.65).normalized()
        arm_parts = [sh_hull,
                     conform("pauldron_" + s, sb, sh + out * 4.0 + up * 4.0, pauldron_n, (0, 1, 0), 9.0, 10.5, "Team",
                             thickness=1.6, offset=1.8, res=(7, 7), chamfer=0.35, curve=1.0),
                     conform("pauldron_trim_" + s, sb, sh + out * 9.5 - up * 1.5 + arm_dir * 3.0, (out * 0.95 + up * 0.2).normalized(),
                             (0, 1, 0), 2.0, 8.0, "Gunmetal", thickness=1.0, offset=2.4, res=(2, 6), chamfer=0.5)]
        # Upper-arm guard below the pauldron, so the bicep isn't bare undersuit.
        upper = gather(verts, ["upperarm_" + s, "upperarm_twist_01_" + s, "upperarm_twist_02_" + s], 0.5)
        ulen = (j["lowerarm_" + s] - sh).length
        upper = [p for p in upper if 11 < (p - sh).dot(arm_dir) < ulen - 4]
        if upper:
            arm_parts.append(plate("bicep_" + s, upper, 1.0, lambda c, n: "Gunmetal", simplify=10))
        pieces["upperarm_" + s] = arm_parts

        # ---- Forearms: a dark gauntlet with a team plate on the outside and a wrist cuff.
        fore = gather(verts, ["lowerarm_" + s, "lowerarm_twist_01_" + s, "lowerarm_twist_02_" + s], 0.5)
        el, wr = j["lowerarm_" + s], j["hand_" + s]
        fdir = (wr - el).normalized()
        flen = (wr - el).length
        fore = [p for p in fore if 6 < (p - el).dot(fdir) < flen - 2]
        fore_hull = plate("forearm_base_" + s, fore, 1.4, lambda c, n: "Gunmetal", simplify=10)
        fb = bvh_of(fore_hull)
        outer = (out - fdir * out.dot(fdir)).normalized()
        topn = (up - fdir * up.dot(fdir)).normalized()
        plate_n = (outer * 0.6 + topn * 0.8).normalized()
        mid = el + fdir * (flen * 0.5)
        pieces["lowerarm_" + s] = [
            fore_hull,
            conform("vambrace_" + s, fb, mid, plate_n, fdir, 4.2, flen * 0.36, "Team", thickness=1.2, offset=1.0,
                    res=(5, 6), chamfer=0.5, curve=0.4, taper=0.3),
            conform("cuff_" + s, fb, el + fdir * (flen - 4.0), topn, fdir, 4.6, 1.3, "Gunmetal", thickness=0.8, offset=1.6,
                    res=(6, 1), chamfer=0.2),
        ]
        pieces["lowerarm_" + s].append(conform("vambrace_light_" + s, bvh_of(pieces["lowerarm_" + s][1]), mid + fdir * (flen * 0.12),
                                               plate_n, fdir, 0.5, 2.0, "TeamGlow", thickness=0.3, offset=0.15, res=(1, 2), chamfer=0, bevel=0))

        # ---- Thighs: dark under-layer, a team plate at the front, a pouch on the outside.
        thigh = gather(verts, ["thigh_" + s, "thigh_twist_01_" + s, "thigh_twist_02_" + s], 0.5)
        hip, knee = j["thigh_" + s], j["calf_" + s]
        tdir = (knee - hip).normalized()
        tlen = (knee - hip).length
        thigh = [p for p in thigh if 8 < (p - hip).dot(tdir) < tlen - 6]
        thigh_hull = plate("thigh_base_" + s, thigh, 1.2, lambda c, n: "Undersuit", simplify=10)
        tb = bvh_of(thigh_hull)
        tmid = hip + tdir * (tlen * 0.45)
        pieces["thigh_" + s] = [
            thigh_hull,
            conform("thigh_plate_" + s, tb, tmid, (sx * 0.25, 1, 0), -tdir, 6.5, tlen * 0.3, "Team", thickness=1.3,
                    offset=1.0, res=(5, 7), chamfer=0.45, curve=0.5, taper=-0.2),
            conform("thigh_side_" + s, tb, tmid + Vector((0, 0, 2.0)), (sx, 0.1, 0), -tdir, 4.0, tlen * 0.22, "Gunmetal",
                    thickness=1.0, offset=0.8, res=(3, 5), chamfer=0.4),
            k.box("pouch_" + s, tmid + out * 10.0 - tdir * 3.0 + Vector((0, -1.5, 0)), (3.2, 7.0, 8.0), "Polymer", bevel=0.8, segments=2),
        ]

        # ---- Shins: dark under-layer, a knee cop and a team greave with a light.
        shin = gather(verts, ["calf_" + s, "calf_twist_01_" + s, "calf_twist_02_" + s], 0.5)
        ankle = j["foot_" + s]
        cdir = (ankle - knee).normalized()
        clen = (ankle - knee).length
        shin = [p for p in shin if -4 < (p - knee).dot(cdir) < clen - 7]
        shin_hull = plate("shin_base_" + s, shin, 1.4, lambda c, n: "Gunmetal" if n.y > -0.3 else "Undersuit", simplify=10)
        cbv = bvh_of(shin_hull)
        pieces["calf_" + s] = [
            shin_hull,
            conform("knee_" + s, cbv, knee + Vector((0, 6.0, -1.0)), (sx * 0.15, 1, 0.15), up, 5.0, 4.6, "Gunmetal",
                    thickness=1.6, offset=1.6, res=(5, 5), chamfer=0.7, curve=0.6),
            conform("greave_" + s, cbv, knee + cdir * (clen * 0.48) + Vector((0, 4.0, 0)), (sx * 0.15, 1, 0), -cdir, 5.0, clen * 0.28,
                    "Team", thickness=1.2, offset=1.0, res=(5, 7), chamfer=0.45, curve=0.4, taper=0.35),
        ]
        pieces["calf_" + s].append(conform("greave_light_" + s, bvh_of(pieces["calf_" + s][2]), knee + cdir * (clen * 0.34) + Vector((0, 4.0, 0)),
                                           (sx * 0.15, 1, 0), -cdir, 0.6, 2.4, "TeamGlow", thickness=0.3, offset=0.15, res=(1, 2), chamfer=0, bevel=0))

        # ---- Boots: a hull with a thick sole and a toe cap.
        foot = gather(verts, ["foot_" + s, "ball_" + s], 0.5)
        boot = plate("boot_" + s, foot, 1.6, lambda c, n, ankle=ankle: "Polymer" if c.z < 3 else "Gunmetal",
                     cuts=[((0, 0, -0.8), (0, 0, -1))], simplify=10)
        ball = j["ball_" + s]
        pieces["foot_" + s] = [
            boot,
            k.box("sole_" + s, Vector((ball.x - sx * 0.6, (ankle.y + ball.y) * 0.5 + 2.0, 0.2)), (10.0, 28.0, 2.2), "Polymer", bevel=0.6, segments=2),
            k.box("toecap_" + s, ball + Vector((0, 5.0, 2.6)), (9.0, 6.0, 4.2), "Gunmetal", bevel=1.2, segments=2, taper=(0.8, 0.7)),
        ]
    return pieces


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(here, "..", "Export", "Armor")
    os.makedirs(out_dir, exist_ok=True)
    preview_dir = args[args.index("--preview-dir") + 1] if "--preview-dir" in args else os.path.join(here, "previews")
    os.makedirs(preview_dir, exist_ok=True)

    k.reset_scene()
    joints = load_bones(args[0])
    skin = next((a for a in args[1:] if a.endswith(".obj")), None)
    verts = load_skin(skin, joints) if skin else proxy_skin(joints)
    pieces = build(verts, joints, None)
    report = []
    exported = []
    for bone, objs in pieces.items():
        name = "SM_SB_Armor_" + bone
        obj = k.join(name, objs)
        report.append("%s: %d tris, %d materials, bounds %s" % (name, k.triangle_count(obj), len(obj.data.materials), k.bounds(obj)))
        exported.append(obj)
    for obj in exported:
        k.export_fbx(os.path.join(out_dir, obj.name + ".fbx"), [obj])
    print("\n".join(report))
    with open(os.path.join(out_dir, "armor_report.txt"), "w") as f:
        f.write("source: %s\n" % (os.path.basename(skin) if skin else "proxy body"))
        f.write("\n".join(report) + "\n")
    if "--render" in args:
        render_preview(skin, verts, exported, preview_dir)


def render_preview(skin, verts, armor, preview_dir):
    """Renders the armored mannequin (red team) from the front, the back and the side, over a dark undersuit body."""
    import sb_render
    if skin:
        bpy.ops.wm.obj_import(filepath=skin, forward_axis="Y", up_axis="Z", global_scale=1.0)
        body = bpy.context.selected_objects[0]
        # The OBJ importer reads Unreal's left-handed numbers as right-handed: same numbers, so keep them.
        body.matrix_world = Matrix.Identity(4)
        bodies = [body]
    else:
        bodies = []
        for bone in sorted({v[1] for v in verts}):
            pts = [v[0] for v in verts if v[1] == bone]
            bodies.append(shell("proxy_" + bone, pts, 0.0, 1.0))
    for body in bodies:
        body.data.materials.clear()
        body.data.materials.append(k.material("Undersuit"))
        for poly in body.data.polygons:
            poly.use_smooth = True
    sb_render.render_views(armor, os.path.join(preview_dir, "SB_Armor.png"),
                           [((0.45, 1.0, 0.15), False), ((-0.5, -1.0, 0.2), False), ((1.0, 0.0, 0.05), True)],
                           resolution=(640, 1000), floor=True)


if __name__ == "__main__":
    main()
