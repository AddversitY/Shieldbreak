"""Turns a generated 3D character (Meshy or any other textured GLB/FBX/OBJ) into Shieldbreak's soldier armor set.

    python import_meshy.py <model.glb|.fbx|.obj> [--bones Art/Reference/manny_bones.json] [--skin Art/Reference/SKM_Manny.obj]
                           [--tris 45000] [--res 4096] [--yaw auto|0|180] [--keep-hands] [--team auto|none]
                           [--preview DIR] [--no-export]

The game wears the suit as rigid per-bone pieces (UShieldbreakBodyAnimator::AttachArmor: SM_SB_Armor_<bone>, modeled in
the Lyra mannequin's reference pose, Unreal mesh space, cm). A generated character is one textured mesh in its own pose,
scale and facing, so this script:

 1. Imports and joins the model, turns it to face +Y (feet point forward) and scales it to the mannequin's height.
 2. Fits the pose: for each arm, forearm, thigh and shin it searches for the rotation about the joint that puts the
    mannequin's own limb inside the model's limb (the least mannequin skin left outside the model's surface), so an
    A-pose with straight arms fits the mannequin's bent-elbow reference pose.
 3. Cuts the model into the armor pieces by the nearest bone of that posed skeleton, and moves each piece rigidly back
    into the mannequin's reference pose. Hands are dropped (the game animates the mannequin's own gloved hands) unless
    --keep-hands.
 4. Pushes any surface that sits inside the mannequin (plus a small margin) out over it, so the body never shows through.
 5. Reduces each piece to its share of --tris, unwraps all pieces into one atlas and bakes the model's own materials
    onto it (base colour, roughness, metallic, tangent normal including its normal map, AO), with a team-colour mask
    (light, neutral armor plates) and a glow mask (bright warm or saturated light, e.g. the visor).
 6. Exports Art/Export/Armor/SM_SB_Armor_<bone>.fbx (slot M_Team_Soldier) and Art/Export/Textures/Armor/T_SB_Soldier_*,
    the same names as the scripted soldier, so Tools/import_art.py and the game need no change.

If a GLB will not import (Draco-compressed meshes need a Blender build with the Draco library; the pip bpy has none),
use the FBX or OBJ download instead.

Licence: only feed it models we are allowed to ship (Meshy paid plan output, or CC0/owned files), and keep the design
original (no copied franchise characters).
"""
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sb_kit as k  # noqa: E402  (imports bpy first)
import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402
from mathutils.kdtree import KDTree  # noqa: E402
import bake_weapons as bw  # noqa: E402
import build_armor as ba  # noqa: E402

REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
EXPORT = os.path.join(REPO, "Art", "Export")
TEX_OUT = os.path.join(EXPORT, "Textures", "Armor")
SLOT = "M_Team_Soldier"
TARGET_HEIGHT = 184.0   # cm, floor to helmet top: the mannequin is 180.5 cm and the helmet sits ~3.5 cm over its head

# Mannequin bone -> armor piece. Bones not listed here (fingers when --keep-hands is off) are dropped.
PIECE = {
    "head": "head", "neck_01": "head", "neck_02": "head",
    "spine_05": "spine_05", "spine_04": "spine_05", "clavicle_l": "spine_05", "clavicle_r": "spine_05",
    "spine_03": "spine_03", "spine_02": "spine_03", "spine_01": "spine_03",
    "pelvis": "pelvis",
}
for _s in ("l", "r"):
    PIECE.update({"upperarm_" + _s: "upperarm_" + _s, "lowerarm_" + _s: "lowerarm_" + _s, "hand_" + _s: "hand_" + _s,
                  "thigh_" + _s: "thigh_" + _s, "calf_" + _s: "calf_" + _s, "foot_" + _s: "foot_" + _s, "ball_" + _s: "foot_" + _s})
ORDER = ["head", "spine_05", "spine_03", "pelvis", "upperarm_l", "upperarm_r", "lowerarm_l", "lowerarm_r", "hand_l", "hand_r",
         "thigh_l", "thigh_r", "calf_l", "calf_r", "foot_l", "foot_r"]
# Share of the triangle budget per piece (renormalised over the pieces present).
BUDGET = {"head": 0.12, "spine_05": 0.26, "spine_03": 0.05, "pelvis": 0.08, "upperarm": 0.06, "lowerarm": 0.035, "hand": 0.015,
          "thigh": 0.045, "calf": 0.05, "foot": 0.03}

# Skeleton used for labelling: (bone, start joint, end joint or offset). End offsets are in cm.
CHAIN = [
    ("pelvis", "pelvis", "spine_01"), ("pelvis", "pelvis", (0, 0, -9)),
    ("spine_01", "spine_01", "spine_02"), ("spine_02", "spine_02", "spine_03"), ("spine_03", "spine_03", "spine_04"),
    ("spine_04", "spine_04", "spine_05"), ("spine_05", "spine_05", "neck_01"), ("neck_01", "neck_01", "head"),
    ("head", "head", (0, 1.5, 20)),
]
for _s in ("l", "r"):
    CHAIN += [("clavicle_" + _s, "clavicle_" + _s, "upperarm_" + _s), ("upperarm_" + _s, "upperarm_" + _s, "lowerarm_" + _s),
              ("lowerarm_" + _s, "lowerarm_" + _s, "hand_" + _s), ("hand_" + _s, "hand_" + _s, "middle_02_" + _s),
              ("thigh_" + _s, "thigh_" + _s, "calf_" + _s), ("calf_" + _s, "calf_" + _s, "foot_" + _s),
              ("foot_" + _s, "foot_" + _s, "ball_" + _s), ("ball_" + _s, "ball_" + _s, (0, 8, 0))]
# Limbs whose rotation is fitted, parent first: (bone whose pivot rotates, child bones carried along).
FIT = []
for _s in ("l", "r"):
    FIT += [("upperarm_" + _s, ["upperarm_" + _s, "lowerarm_" + _s, "hand_" + _s]),
            ("lowerarm_" + _s, ["lowerarm_" + _s, "hand_" + _s]),
            ("thigh_" + _s, ["thigh_" + _s, "calf_" + _s, "foot_" + _s, "ball_" + _s]),
            ("calf_" + _s, ["calf_" + _s, "foot_" + _s, "ball_" + _s])]


SCORE = {"upperarm": ["upperarm"], "lowerarm": ["lowerarm"], "thigh": ["thigh"], "calf": ["calf", "foot"]}


def log(msg):
    print(msg, flush=True)


# =====================================================================================================================
# 1. Import and normalise
# =====================================================================================================================
def import_model(path):
    ext = os.path.splitext(path)[1].lower()
    before = set(bpy.data.objects)
    if ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=path)
    elif ext == ".obj":
        bpy.ops.wm.obj_import(filepath=path)
    else:
        sys.exit("unsupported model type: " + ext)
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    if not meshes:
        sys.exit("no mesh in " + path)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    bm = bmesh.new()
    mats = []
    for obj in meshes:
        ev = obj.evaluated_get(depsgraph)          # applies an armature's rest/current pose and any modifiers
        mesh = bpy.data.meshes.new_from_object(ev, depsgraph=depsgraph)
        mesh.transform(obj.matrix_world)
        remap = []
        for m in mesh.materials:
            if m not in mats:
                mats.append(m)
            remap.append(mats.index(m))
        tmp = bmesh.new()
        tmp.from_mesh(mesh)
        for f in tmp.faces:
            f.material_index = remap[f.material_index] if remap else 0
        tmp.to_mesh(mesh)
        tmp.free()
        bm.from_mesh(mesh)
        bpy.data.meshes.remove(mesh)
    for o in new:
        bpy.data.objects.remove(o, do_unlink=True)
    mesh = bpy.data.meshes.new("HIGH")
    bm.to_mesh(mesh)
    bm.free()
    for m in mats:
        mesh.materials.append(m)
    obj = bpy.data.objects.new("HIGH", mesh)
    bpy.context.scene.collection.objects.link(obj)
    log("imported %s: %d verts, %d tris, %d materials" % (os.path.basename(path), len(mesh.vertices), k.triangle_count(obj), len(mats)))
    return obj


def coords(obj):
    arr = np.empty(len(obj.data.vertices) * 3, dtype=np.float64)
    obj.data.vertices.foreach_get("co", arr)
    return arr.reshape(-1, 3)


def set_coords(obj, arr):
    obj.data.vertices.foreach_set("co", arr.astype(np.float32).ravel())
    obj.data.update()


def normalise(obj, yaw):
    """Z up, feet on the floor, centred on X, the toes toward +Y, TARGET_HEIGHT tall."""
    v = coords(obj)
    ext = v.max(0) - v.min(0)
    if ext[1] > ext[2] * 1.5:          # lying along Y: a Y-up file that arrived unconverted
        v = v[:, [0, 2, 1]] * np.array([1, -1, 1])
        ext = v.max(0) - v.min(0)
    scale = TARGET_HEIGHT / ext[2]
    v = (v - np.array([(v[:, 0].max() + v[:, 0].min()) / 2, 0, v[:, 2].min()])) * scale
    if yaw == "auto":
        # Toes stick out forward from the ankles: the lowest 6 cm of the model sits ahead of the shin above it.
        feet = v[v[:, 2] < 6.0]
        shins = v[(v[:, 2] > 25.0) & (v[:, 2] < 45.0)]
        yaw = 0 if feet[:, 1].mean() >= shins[:, 1].mean() else 180
        log("facing: toes %+.1f cm ahead of the shins -> yaw %d" % (feet[:, 1].mean() - shins[:, 1].mean(), yaw))
    if int(yaw) == 180:
        v = v * np.array([-1, -1, 1])
    # Line the hips up with the mannequin's (Y), using the middle of the model's depth at hip height.
    hips = v[(v[:, 2] > 85) & (v[:, 2] < 100)]
    v[:, 1] += 2.3 - (np.percentile(hips[:, 1], 10) + np.percentile(hips[:, 1], 90)) / 2
    set_coords(obj, v)
    log("normalised: scale x%.3f, %.1f cm tall" % (scale, v[:, 2].max()))
    if obj.data.has_custom_normals:
        pass
    return obj


# =====================================================================================================================
# 2. Pose fit
# =====================================================================================================================
def segments(j):
    out = []
    for bone, a, b in CHAIN:
        if a not in j:
            continue
        pa = j[a]
        pb = j[b] if isinstance(b, str) else pa + Vector(b)
        out.append((bone, pa, pb))
    return out


def rot_about(pivot, rot):
    return Matrix.Translation(pivot) @ rot.to_4x4() @ Matrix.Translation(-pivot)


def fit_pose(model_bvh, skin, j):
    """Returns {bone: 4x4} taking the mannequin's reference pose to the model's pose (identity for unfitted bones), and
    the posed joints. Each fitted limb tries rotations about its joint and keeps the one that leaves the least
    mannequin skin outside the model."""
    pose = {b: Matrix.Identity(4) for b in set(seg[0] for seg in CHAIN)}
    joints = {name: v.copy() for name, v in j.items()}
    skin_by_bone = {}
    for p, bone, _ in skin:
        skin_by_bone.setdefault(bone, []).append(p)

    def outside(points, m):
        total, n = 0.0, 0
        for p in points:
            q = m @ p
            hit, nrm, _, dist = model_bvh.find_nearest(q, 40.0)
            if hit is None:
                total += 40.0
            elif (q - hit).dot(nrm) > 0:
                total += dist
            n += 1
        return total / max(n, 1)

    for pivot_bone, carried in FIT:
        # Score with the limb's own skin only: a child still in the wrong pose would drag the parent's fit off.
        pts = []
        for b in SCORE[pivot_bone[:-2]]:
            pts += skin_by_bone.get(b + pivot_bone[-2:], [])
        if not pts:
            continue
        pts = pts[::max(1, len(pts) // 1200)]
        base = pose[pivot_bone]
        pivot = base @ j[pivot_bone]
        child = {"upperarm": "lowerarm", "lowerarm": "hand", "thigh": "calf", "calf": "foot"}[pivot_bone[:-2]] + pivot_bone[-2:]
        axis = ((base @ j[child]) - pivot).normalized()
        # Two hinge axes perpendicular to the limb: swing it out/in and forward/back.
        ref = Vector((0, 1, 0)) if abs(axis.y) < 0.8 else Vector((1, 0, 0))
        h1 = axis.cross(ref).normalized()
        if pivot_bone.startswith(("lowerarm", "calf")):
            # Elbows and knees are hinges: the main axis is the joint's own (normal of the parent/child bone plane).
            parent = {"lowerarm": "upperarm", "calf": "thigh"}[pivot_bone[:-2]] + pivot_bone[-2:]
            bend = axis.cross(pivot - (base @ j[parent]))
            if bend.length > 1e-3:
                h1 = bend.normalized()
        h2 = axis.cross(h1).normalized()
        span2 = {"upperarm": 40, "lowerarm": 12, "thigh": 16, "calf": 8}[pivot_bone[:-2]]
        best = (outside(pts, base) + 0.0, Matrix.Identity(3))
        span, step = {"upperarm": (40, 8), "lowerarm": (64, 8), "thigh": (16, 4), "calf": (24, 6)}[pivot_bone[:-2]]
        for _ in range(2):
            centre = best[1]
            s2 = min(span, span2)
            for a1 in range(-span, span + 1, step):
                for a2 in range(-s2, s2 + 1, step):
                    r = Matrix.Rotation(math.radians(a1), 3, h1) @ Matrix.Rotation(math.radians(a2), 3, h2) @ centre
                    # A symmetric limb fits many rotations about equally well: prefer the smallest one.
                    score = outside(pts, rot_about(pivot, r) @ base) + 0.002 * math.degrees(r.to_quaternion().angle)
                    if score < best[0] - 1e-4:
                        best = (score, r)
            span, step = step, max(1, step // 4)
        m = rot_about(pivot, best[1])
        for b in carried:
            pose[b] = m @ pose[b]
        angle = math.degrees(best[1].to_quaternion().angle)
        log("  fit %-11s %5.1f deg, skin outside %.2f cm" % (pivot_bone, angle, best[0]))
    for name in joints:
        bone = name if name in pose else None
        if bone:
            joints[name] = pose[bone] @ j[name]
    # Children share the pose of the bone that carries them (finger and ball joints, chain ends).
    for bone, a, b in CHAIN:
        if isinstance(b, str) and b in j and b not in pose:
            joints[b] = pose[bone] @ j[b]
    return pose, joints


# =====================================================================================================================
# 3. Cut into pieces and repose
# =====================================================================================================================
def label_faces(obj, j_posed, keep_hands):
    segs = segments(j_posed)
    a = np.array([s[1] for s in segs])
    b = np.array([s[2] for s in segs])
    ab = b - a
    ab2 = np.maximum((ab * ab).sum(1), 1e-6)
    centres = np.empty(len(obj.data.polygons) * 3)
    obj.data.polygons.foreach_get("center", centres)
    c = centres.reshape(-1, 3)
    best = np.full(len(c), 1e9)
    lab = np.zeros(len(c), dtype=np.int32)
    for i in range(len(segs)):
        t = np.clip(((c - a[i]) * ab[i]).sum(1) / ab2[i], 0, 1)
        d = np.linalg.norm(c - (a[i] + ab[i] * t[:, None]), axis=1)
        closer = d < best
        best[closer] = d[closer]
        lab[closer] = i
    names = []
    for i in lab:
        piece = PIECE.get(segs[i][0])
        if piece and piece.startswith("hand") and not keep_hands:
            piece = None
        names.append(piece)
    return names, [segs[i][0] for i in lab]


def split_pieces(obj, labels, bones, pose):
    """One object per armor piece, moved rigidly from the model's pose back to the mannequin's reference pose."""
    pieces = {}
    for piece in ORDER:
        idx = [i for i, p in enumerate(labels) if p == piece]
        if not idx:
            continue
        keep = set(idx)
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bm.faces.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in keep], context="FACES")
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
        # The piece follows the bone that most of its faces belong to (e.g. lowerarm, not the twist or hand).
        counts = {}
        for i in idx:
            counts[bones[i]] = counts.get(bones[i], 0) + 1
        bone = max(counts, key=counts.get)
        bmesh.ops.transform(bm, matrix=pose[bone].inverted(), verts=bm.verts)
        mesh = bpy.data.meshes.new("P_" + piece)
        bm.to_mesh(mesh)
        bm.free()
        for m in obj.data.materials:
            mesh.materials.append(m)
        part = bpy.data.objects.new("P_" + piece, mesh)
        bpy.context.scene.collection.objects.link(part)
        pieces[piece] = part
    return pieces


# =====================================================================================================================
# 4. Keep the body inside
# =====================================================================================================================
def skin_bvh(path):
    verts, faces = [], []
    with open(path) as f:
        for line in f:
            if line.startswith("v "):
                verts.append(tuple(map(float, line.split()[1:4])))
            elif line.startswith("f "):
                faces.append([int(t.split("/")[0]) - 1 for t in line.split()[1:]])
    return BVHTree.FromPolygons(verts, faces)


def push_out(piece, body, margin=0.6, reach=6.0):
    """Moves vertices that are inside the mannequin (or within margin of it) to margin outside it."""
    # The skin comes out of Unreal with its winding mirrored, so its normals may point inward: check on the crown.
    hit, nrm, _, _ = body.find_nearest(Vector((0, 0, 260)), 200.0)
    flip = -1.0 if nrm.z < 0 else 1.0
    v = coords(piece)
    moved = 0
    for i, p in enumerate(v):
        q = Vector(p)
        hit, nrm, _, dist = body.find_nearest(q, reach)
        if hit is None:
            continue
        nrm = nrm * flip
        s = (q - hit).dot(nrm)
        if s < margin:
            v[i] = hit + nrm * margin
            moved += 1
    set_coords(piece, v)
    return moved


# =====================================================================================================================
# 5. Reduce, bake, masks
# =====================================================================================================================
def decimate(obj, target):
    tris = k.triangle_count(obj)
    if tris <= target:
        return obj
    mod = obj.modifiers.new("Decimate", "DECIMATE")
    mod.ratio = max(0.01, target / tris)
    mod.use_collapse_triangulate = True
    depsgraph = bpy.context.evaluated_depsgraph_get()
    mesh = bpy.data.meshes.new_from_object(obj.evaluated_get(depsgraph), depsgraph=depsgraph)
    obj.modifiers.clear()
    old = obj.data
    obj.data = mesh
    bpy.data.meshes.remove(old)
    return obj


def join_ordered(pieces, name, copy=False):
    """Joins pieces in ORDER; returns (object, {piece: (first face, end face)})."""
    ranges, start, objs = {}, 0, []
    for piece in ORDER:
        if piece not in pieces:
            continue
        src = pieces[piece]
        obj = src
        if copy:
            obj = bpy.data.objects.new(src.name + "_c", src.data.copy())
            bpy.context.scene.collection.objects.link(obj)
        n = len(obj.data.polygons)
        ranges[piece] = (start, start + n)
        start += n
        objs.append(obj)
    return k.join(name, objs), ranges


def principled_sources(mat):
    """{input name: (socket or None, default)} feeding the material's Principled BSDF."""
    nodes = mat.node_tree.nodes if mat and mat.use_nodes else []
    bsdf = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
    out = {}
    for key in ("Base Color", "Roughness", "Metallic", "Emission Color", "Emission Strength", "Alpha"):
        if bsdf is None:
            out[key] = (None, {"Base Color": (0.4, 0.4, 0.4, 1), "Roughness": 0.6, "Metallic": 0.0}.get(key, 0.0))
            continue
        sock = bsdf.inputs.get(key)
        if sock is None:
            out[key] = (None, 0.0)
        elif sock.links:
            out[key] = (sock.links[0].from_socket, None)
        else:
            out[key] = (None, tuple(sock.default_value) if hasattr(sock.default_value, "__len__") else sock.default_value)
    return out


def route_high(high, key):
    """Points every HIGH material's output at an emission of the given principled input (or the glow mask)."""
    for mat in high.data.materials:
        if mat is None or not mat.use_nodes:
            continue
        tree = mat.node_tree
        src = mat.get("_sb_src")
        if src is None:
            mat["_sb_src"] = 1
            mat["_sb_bsdf_inputs"] = 1
        em = tree.nodes.get("SB_Emit") or tree.nodes.new("ShaderNodeEmission")
        em.name = "SB_Emit"
        out = tree.nodes.get("SB_Out") or tree.nodes.new("ShaderNodeOutputMaterial")
        out.name = "SB_Out"
        for n in tree.nodes:
            if n.type == "OUTPUT_MATERIAL":
                n.is_active_output = n == out
        if not out.inputs["Surface"].links:
            tree.links.new(em.outputs[0], out.inputs["Surface"])
        for link in list(em.inputs["Color"].links):
            tree.links.remove(link)
        sources = SOURCES[mat.name]
        sock, default = sources[key]
        if sock is not None:
            tree.links.new(sock, em.inputs["Color"])
        else:
            val = default if isinstance(default, tuple) else (default, default, default, 1.0)
            em.inputs["Color"].default_value = tuple(val[:3]) + (1.0,)
        em.inputs["Strength"].default_value = 1.0


SOURCES = {}


def restore_high(high):
    for mat in high.data.materials:
        if mat is None or not mat.use_nodes:
            continue
        for n in mat.node_tree.nodes:
            if n.type == "OUTPUT_MATERIAL":
                n.is_active_output = n.name != "SB_Out"


def bake_all(low, high, res, samples=16):
    scene = bw.setup_cycles(samples)
    for mat in high.data.materials:
        if mat is not None:
            SOURCES[mat.name] = principled_sources(mat)
    target = bw.new_image("SB_Target", res)
    mat = bw.single_slot(low, SLOT)
    mat.use_nodes = True
    node = mat.node_tree.nodes.new("ShaderNodeTexImage")
    node.image = target
    mat.node_tree.nodes.active = node
    bpy.ops.object.select_all(action="DESELECT")
    high.select_set(True)
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    b = scene.render.bake
    b.use_selected_to_active = True
    b.use_cage = False
    b.cage_extrusion = 0.8
    b.max_ray_distance = 2.5
    out = {}
    t0 = time.time()
    restore_high(high)
    b.normal_space = "TANGENT"
    scene.cycles.samples = 4
    bpy.ops.object.bake(type="NORMAL")
    out["normal"] = bw.pixels(target).copy()
    log("  normal (%.0fs)" % (time.time() - t0))
    scene.cycles.samples = 1
    for key, name in (("Base Color", "basecolor"), ("Roughness", "rough"), ("Metallic", "metal")):
        route_high(high, key)
        bpy.ops.object.bake(type="EMIT")
        out[name] = bw.pixels(target).copy()
        log("  %s (%.0fs)" % (name, time.time() - t0))
    restore_high(high)
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")
    scene.world.light_settings.distance = 6.0
    scene.cycles.samples = max(samples * 2, 32)
    bpy.ops.object.bake(type="AO")
    out["ao"] = bw.pixels(target).copy()
    log("  ao (%.0fs)" % (time.time() - t0))
    b.use_selected_to_active = False
    return out


def rgb_to_hsv(rgb):
    mx, mn = rgb.max(-1), rgb.min(-1)
    sat = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0.0)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    d = np.maximum(mx - mn, 1e-6)
    h = np.where(mx == r, ((g - b) / d) % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) / 6.0
    return h, sat, mx


def smooth(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def masks(bc_lin, team_mode):
    """Team mask: the light, neutral armor plates (the parts a Halo-style team colour would paint); glow: bright warm
    or strongly saturated light areas (visor, lamps)."""
    h, sat, val = rgb_to_hsv(np.clip(bc_lin, 0, 1) ** (1 / 2.2))
    if team_mode == "none":
        team = np.zeros_like(val)
    else:
        team = smooth(val, 0.28, 0.42) * (1 - smooth(sat, 0.12, 0.25))
    warm = ((h < 0.14) | (h > 0.95)).astype(np.float64)
    glow = smooth(val, 0.72, 0.9) * smooth(sat, 0.45, 0.65) * np.maximum(warm, smooth(sat, 0.8, 0.9))
    return team, glow


def write_textures(surf, res):
    os.makedirs(TEX_OUT, exist_ok=True)
    n = surf["normal"][:, :, :3].copy()
    n[:, :, 1] = 1.0 - n[:, :, 1]
    n = bw.fix_normal_misses(n)
    bw.save_png(n, os.path.join(TEX_OUT, "T_SB_Soldier_N.png"), "RGB")
    bc = surf["basecolor"][:, :, :3]
    team, glow = masks(bc, TEAM_MODE)
    # Plates under the team mask bake toward a light neutral so the team colour reads (the material multiplies it in).
    lum = (bc * np.array([0.2126, 0.7152, 0.0722])).sum(-1, keepdims=True)
    neutral = np.repeat(lum, 3, axis=2) * 1.15
    bc_out = bc * (1 - team[..., None] * 0.85) + neutral * (team[..., None] * 0.85)
    bw.save_png(np.concatenate([bw.linear_to_srgb(bc_out), glow[..., None]], axis=2), os.path.join(TEX_OUT, "T_SB_Soldier_BC.png"), "RGBA")
    orm = np.stack([surf["ao"][:, :, 0], np.clip(surf["rough"][:, :, 0], 0.08, 1), np.clip(surf["metal"][:, :, 0], 0, 1)], axis=2)
    bw.save_png(orm, os.path.join(TEX_OUT, "T_SB_Soldier_ORM.png"), "RGB")
    bw.save_png(np.stack([team, glow, np.zeros_like(team)], axis=2), os.path.join(TEX_OUT, "T_SB_Soldier_M.png"), "RGB")


def extract(low, name, a, b):
    mesh = low.data.copy()
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if not (a <= f.index < b)], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


TEAM_MODE = "auto"


def main():
    global TEAM_MODE
    args = sys.argv[1:]
    if not args or args[0].startswith("--"):
        sys.exit(__doc__)

    def opt(name, default):
        return args[args.index(name) + 1] if name in args else default
    model = args[0]
    bones_path = opt("--bones", os.path.join(REPO, "Art", "Reference", "manny_bones.json"))
    skin_path = opt("--skin", os.path.join(REPO, "Art", "Reference", "SKM_Manny.obj"))
    budget = int(opt("--tris", 45000))
    res = int(opt("--res", 4096))
    TEAM_MODE = opt("--team", "auto")
    global TEX_OUT
    if "--no-export" in args:
        # A dry run must not overwrite the suit's textures in the repo; previews read them from here instead.
        TEX_OUT = os.path.join(opt("--preview", "/tmp"), "textures")
        import bake_soldier as bks
        bks.TEX_OUT = TEX_OUT
    preview = opt("--preview", None)
    keep_hands = "--keep-hands" in args
    t0 = time.time()

    k.reset_scene()
    j = ba.load_bones(bones_path)
    skin = ba.load_skin(skin_path, j)
    high = import_model(model)
    normalise(high, opt("--yaw", "auto"))
    # Very dense scans/generations: thin to ~600k triangles first so the fit and bakes stay quick.
    decimate(high, 600000)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    model_bvh = BVHTree.FromObject(high, depsgraph)
    log("pose fit:")
    pose, j_posed = fit_pose(model_bvh, skin, j)
    labels, bones = label_faces(high, j_posed, keep_hands)
    dropped = sum(1 for p in labels if p is None)
    pieces = split_pieces(high, labels, bones, pose)
    bpy.data.objects.remove(high, do_unlink=True)
    log("cut into %d pieces (%d faces dropped: hands/fingers) (%.0fs)" % (len(pieces), dropped, time.time() - t0))
    body = skin_bvh(skin_path)
    for piece, obj in pieces.items():
        moved = push_out(obj, body)
        if moved:
            log("  %s: %d vertices pushed out of the body" % (piece, moved))
    # LOW: each piece reduced to its share of the budget.
    shares = {p: BUDGET[p.rsplit("_", 1)[0] if p.endswith(("_l", "_r")) else p] for p in pieces}
    total = sum(shares.values())
    lows = {}
    for piece, obj in pieces.items():
        low = bpy.data.objects.new("L_" + piece, obj.data.copy())
        bpy.context.scene.collection.objects.link(low)
        decimate(low, int(budget * shares[piece] / total))
        lows[piece] = low
    high, _ = join_ordered(pieces, "HIGH")
    low, ranges = join_ordered(lows, "LOW")
    for p in high.data.polygons:
        p.use_smooth = True
    for p in low.data.polygons:
        p.use_smooth = True
    log("HIGH %d tris, LOW %d tris (%.0fs)" % (k.triangle_count(high), k.triangle_count(low), time.time() - t0))
    bw.unwrap(low)
    surf = bake_all(low, high, res)
    write_textures(surf, res)
    bpy.data.objects.remove(high, do_unlink=True)
    log("textures written (%.0fs)" % (time.time() - t0))
    out = []
    report = ["source: %s (import_meshy.py, %d px atlas, 1 material)" % (os.path.basename(model), res)]
    for piece, (a, b) in ranges.items():
        obj = extract(low, "SM_SB_Armor_" + piece, a, b)
        bw.single_slot(obj, SLOT)
        out.append(obj)
        report.append("%s: %d tris, bounds %s" % (obj.name, k.triangle_count(obj), k.bounds(obj)))
    report.append("total: %d tris" % sum(k.triangle_count(o) for o in out))
    log("\n".join(report))
    if "--no-export" not in args:
        arm = os.path.join(EXPORT, "Armor")
        exported = {o.name for o in out}
        for f in os.listdir(arm):
            # A piece the model doesn't have (e.g. hands) must not keep the previous suit's mesh.
            if f.startswith("SM_SB_Armor_") and f.endswith(".fbx") and f[:-4] not in exported:
                os.remove(os.path.join(arm, f))
                log("removed stale " + f)
        for o in out:
            k.export_fbx(os.path.join(arm, o.name + ".fbx"), [o])
        with open(os.path.join(arm, "armor_report.txt"), "w") as f:
            f.write("\n".join(report) + "\n")
    if preview:
        import bake_soldier as bks
        import build_soldier as bs
        import sb_render
        bks.TEX_OUT = TEX_OUT
        os.makedirs(preview, exist_ok=True)
        bpy.data.objects.remove(low, do_unlink=True)
        bs.load_body(skin_path)
        for label, colour in (("neutral", None), ("red", (0.55, 0.05, 0.04)), ("blue", (0.05, 0.16, 0.55))):
            mat = bks.preview_material(colour)
            for o in out:
                o.data.materials[0] = mat
            for name in list(bpy.data.objects.keys()):
                if name.startswith(("Cam", "Key", "Rim", "Fill", "Plane")):
                    bpy.data.objects.remove(bpy.data.objects[name], do_unlink=True)
            sb_render.render_views(out, os.path.join(preview, "Meshy_%s.png" % label), bs.VIEWS, resolution=(800, 1200), floor=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(preview, "Meshy_soldier.blend"), compress=True)
    log("done (%.0fs)" % (time.time() - t0))


if __name__ == "__main__":
    main()
