"""Turns a generated 3D character (Meshy or any other textured GLB/FBX/OBJ) into Shieldbreak's soldier armor set.

    python import_meshy.py <model.glb|.fbx|.obj> [--bones Art/Reference/manny_bones.json] [--skin Art/Reference/SKM_Manny.obj]
                           [--tris 45000] [--res 4096] [--yaw auto|0|180] [--keep-hands] [--team auto|none]
                           [--detail dense_untextured_copy.glb]
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


def normalise(obj, yaw, also=()):
    """Z up, feet on the floor, centred on X, the toes toward +Y, TARGET_HEIGHT tall. Objects in `also` (a denser
    copy of the same generation) get exactly the same transform."""
    v = coords(obj)
    ext = v.max(0) - v.min(0)
    swap = ext[1] > ext[2] * 1.5          # lying along Y: a Y-up file that arrived unconverted

    def stage1(a):
        return a[:, [0, 2, 1]] * np.array([1, -1, 1]) if swap else a
    v = stage1(v)
    ext = v.max(0) - v.min(0)
    scale = TARGET_HEIGHT / ext[2]
    origin = np.array([(v[:, 0].max() + v[:, 0].min()) / 2, 0, v[:, 2].min()])
    v = (v - origin) * scale
    if yaw == "auto":
        # Toes stick out forward from the ankles: the lowest 6 cm of the model sits ahead of the shin above it.
        feet = v[v[:, 2] < 6.0]
        shins = v[(v[:, 2] > 25.0) & (v[:, 2] < 45.0)]
        yaw = 0 if feet[:, 1].mean() >= shins[:, 1].mean() else 180
        log("facing: toes %+.1f cm ahead of the shins -> yaw %d" % (feet[:, 1].mean() - shins[:, 1].mean(), yaw))
    flip = np.array([-1, -1, 1]) if int(yaw) == 180 else np.ones(3)
    v = v * flip
    # Line the hips up with the mannequin's (Y), using the middle of the model's depth at hip height.
    hips = v[(v[:, 2] > 85) & (v[:, 2] < 100)]
    shift = 2.3 - (np.percentile(hips[:, 1], 10) + np.percentile(hips[:, 1], 90)) / 2
    v[:, 1] += shift
    set_coords(obj, v)
    for other in also:
        w = (stage1(coords(other)) - origin) * scale * flip
        w[:, 1] += shift
        set_coords(other, w)
    log("normalised: scale x%.3f, %.1f cm tall" % (scale, v[:, 2].max()))
    return obj


def label_like(detail, high, labels, bones):
    """Labels for a denser copy of the model: each face takes the label of the nearest face of the textured mesh."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    tree = BVHTree.FromObject(high, depsgraph)
    centres = np.empty(len(detail.data.polygons) * 3)
    detail.data.polygons.foreach_get("center", centres)
    dl, db = [], []
    for c in centres.reshape(-1, 3):
        hit, _, idx, _ = tree.find_nearest(Vector(c), 5.0)
        if hit is None:
            dl.append(None)
            db.append("pelvis")
        else:
            dl.append(labels[idx])
            db.append(bones[idx])
    return dl, db


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
        # Distance to the bone minus that body part's rough radius, so a vest hem next to a hanging forearm still goes
        # to the torso rather than riding along with the arm.
        d = np.linalg.norm(c - (a[i] + ab[i] * t[:, None]), axis=1) - seg_radius(segs[i][0])
        closer = d < best
        best[closer] = d[closer]
        lab[closer] = i
    lab = clean_islands(obj, lab, [PIECE.get(s[0]) for s in segs])
    names = []
    for i in lab:
        piece = PIECE.get(segs[i][0])
        if piece and piece.startswith("hand") and not keep_hands:
            piece = None
        names.append(piece)
    return names, [segs[i][0] for i in lab]


SEG_RADIUS = {"pelvis": 16, "spine": 19, "neck": 11, "head": 14, "clavicle": 9, "upperarm": 6, "lowerarm": 4.5, "hand": 3.5,
              "middle": 2, "thigh": 8, "calf": 6, "foot": 4.5, "ball": 3.5}


def seg_radius(bone):
    return SEG_RADIUS.get(bone.split("_")[0], 5.0)


def clean_islands(obj, lab, seg_piece, passes=4):
    """Each piece should be one connected patch of surface. Any stray island of faces (a vest flap labelled forearm,
    a boot sliver labelled calf) smaller than the piece's main island joins whichever piece borders it most."""
    me = obj.data
    nf = len(me.polygons)
    nbrs = face_neighbours(me)
    piece = np.array([hash(seg_piece[i]) for i in lab])
    moved_total = 0
    for _ in range(passes):
        comp = np.full(nf, -1, dtype=np.int64)
        sizes, comp_piece = [], []
        for f0 in range(nf):
            if comp[f0] >= 0:
                continue
            cid = len(sizes)
            stack = [f0]
            comp[f0] = cid
            n = 0
            while stack:
                f = stack.pop()
                n += 1
                for g in nbrs[f]:
                    if comp[g] < 0 and piece[g] == piece[f0]:
                        comp[g] = cid
                        stack.append(g)
            sizes.append(n)
            comp_piece.append(piece[f0])
        sizes = np.array(sizes)
        main = {}
        for cid, pc in enumerate(comp_piece):
            if pc not in main or sizes[cid] > sizes[main[pc]]:
                main[pc] = cid
        keep = set(main.values())
        moved = 0
        for cid in np.argsort(sizes):
            if cid in keep or sizes[cid] > 0.35 * sizes[main[comp_piece[cid]]]:
                continue
            faces = np.nonzero(comp == cid)[0]
            votes = {}
            for f in faces:
                for g in nbrs[f]:
                    if comp[g] != cid:
                        votes[lab[g]] = votes.get(lab[g], 0) + 1
            if not votes:
                continue  # a floating part on its own: leave it with the nearest bone
            new = max(votes, key=votes.get)
            lab[faces] = new
            piece[faces] = hash(seg_piece[new])
            comp[faces] = comp[np.nonzero(lab == new)[0][0]]
            moved += len(faces)
        moved_total += moved
        if not moved:
            break
    log("label cleanup: %d faces moved off stray islands" % moved_total)
    return lab


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


def push_out(piece, body, margin=1.2, reach=6.0):
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
        if key == "Team":
            attr = tree.nodes.get("SB_Team") or tree.nodes.new("ShaderNodeAttribute")
            attr.name = "SB_Team"
            attr.attribute_name = "SBTeam"
            tree.links.new(attr.outputs["Color"], em.inputs["Color"])
            em.inputs["Strength"].default_value = 1.0
            continue
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


TEAM_MIN_AREA = 60.0   # cm^2: a light neutral region smaller than this (a buckle, a scuff) stays as painted
TEAM_MIN_VALUE = 0.34   # sRGB value of a light plate (after the generator's baked shading)
TEAM_MAX_SAT = 0.24
TEAM_CLOSE = 2           # face rings of closing


def base_image(mat):
    sock, _ = principled_sources(mat)["Base Color"]
    node = sock.node if sock is not None else None
    seen = 0
    while node is not None and node.type != "TEX_IMAGE" and seen < 8:
        ins = [i for i in node.inputs if i.links]
        node = ins[0].links[0].from_node if ins else None
        seen += 1
    return node.image if node is not None and node.type == "TEX_IMAGE" else None


def face_team(high):
    """Team mask by plate, not by texel. Faces get their painted colour from the source texture, faces of similar
    colour grow into regions over the welded surface, and each large, light, neutral region becomes team colour as a
    whole. A per-texel colour test left speckles of team colour on dark cloth and holes in the plates."""
    me = high.data
    nf = len(me.polygons)
    col = np.full((nf, 3), 0.3)
    uv_layer = me.uv_layers.active
    if uv_layer is None:
        return
    uv = np.empty(len(me.loops) * 2)
    uv_layer.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    ls = np.empty(nf, dtype=np.int64)
    lt = np.empty(nf, dtype=np.int64)
    mi = np.empty(nf, dtype=np.int64)
    me.polygons.foreach_get("loop_start", ls)
    me.polygons.foreach_get("loop_total", lt)
    me.polygons.foreach_get("material_index", mi)
    for m_i, mat in enumerate(me.materials):
        img = base_image(mat) if mat else None
        if img is None:
            continue
        w, h = img.size
        px = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        px = px.reshape(h, w, 4)[:, :, :3]
        faces = np.nonzero(mi == m_i)[0]
        acc = np.zeros((len(faces), 3))
        # three corners pulled 1/3 toward the centre, plus the centre: robust against seams
        cen = np.zeros((len(faces), 2))
        for c in range(3):
            cen += uv[ls[faces] + np.minimum(c, lt[faces] - 1)]
        cen /= 3
        for c in range(4):
            q = cen if c == 3 else uv[ls[faces] + np.minimum(c, lt[faces] - 1)] * 0.66 + cen * 0.34
            xi = np.clip((q[:, 0] % 1.0) * w, 0, w - 1).astype(int)
            yi = np.clip((q[:, 1] % 1.0) * h, 0, h - 1).astype(int)
            acc += px[yi, xi]
        col[faces] = acc / 4
    # (pixels of byte sRGB images come back in sRGB, which is what the HSV test wants)
    hsv = np.stack(rgb_to_hsv(np.clip(col, 0, 1)), -1)
    area = np.empty(nf)
    me.polygons.foreach_get("area", area)
    nbrs = face_neighbours(me)
    # Candidate faces: light and neutral (baked-in shading moves value a lot, chroma little).
    cand = (hsv[:, 2] > TEAM_MIN_VALUE) & (hsv[:, 1] < TEAM_MAX_SAT)
    comps = components(nbrs, cand)
    team = np.zeros(nf, dtype=bool)
    n_team = 0
    for members in comps:
        if area[members].sum() >= TEAM_MIN_AREA:
            team[members] = True
            n_team += 1
    # Morphological closing over the surface: scratches, decals and shading dips inside a plate join the plate.
    for _ in range(TEAM_CLOSE):
        team = team | np.array([any(team[g] for g in nbrs[f]) for f in range(nf)])
    for _ in range(TEAM_CLOSE):
        team = team & np.array([all(team[g] for g in nbrs[f]) for f in range(nf)])
    # Holes: small non-team islands fully enclosed by team faces join it.
    for members in components(nbrs, ~team):
        if area[members].sum() < TEAM_MIN_AREA * 0.6:
            team[members] = True
    team = team.astype(np.float64)
    attr = me.color_attributes.new("SBTeam", "FLOAT_COLOR", "CORNER")
    corner = np.repeat(team, lt)
    attr.data.foreach_set("color", np.stack([corner, corner, corner, np.ones_like(corner)], -1).ravel())
    log("team mask: %d plates, %.0f%% of the surface" % (n_team, 100 * (area * team).sum() / area.sum()))


def components(nbrs, mask):
    seen = np.zeros(len(nbrs), dtype=bool)
    out = []
    for f0 in np.nonzero(mask)[0]:
        if seen[f0]:
            continue
        seen[f0] = True
        stack, members = [f0], []
        while stack:
            f = stack.pop()
            members.append(f)
            for g in nbrs[f]:
                if mask[g] and not seen[g]:
                    seen[g] = True
                    stack.append(g)
        out.append(np.array(members))
    return out


def face_neighbours(me):
    nf = len(me.polygons)
    loop_start = np.empty(nf, dtype=np.int64)
    loop_total = np.empty(nf, dtype=np.int64)
    me.polygons.foreach_get("loop_start", loop_start)
    me.polygons.foreach_get("loop_total", loop_total)
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    _, weld = np.unique(np.round(co.reshape(-1, 3) * 50).astype(np.int64), axis=0, return_inverse=True)
    weld = weld.ravel()
    lv = np.empty(len(me.loops), dtype=np.int64)
    me.loops.foreach_get("vertex_index", lv)
    lv = weld[lv]
    nxt = np.arange(len(lv)) + 1
    nxt[loop_start + loop_total - 1] = loop_start
    lo, hi = np.minimum(lv, lv[nxt]), np.maximum(lv, lv[nxt])
    key = lo * (weld.max() + 1) + hi
    face_of_loop = np.repeat(np.arange(nf), loop_total)
    order = np.argsort(key, kind="stable")
    k_sorted, f_sorted = key[order], face_of_loop[order]
    same = k_sorted[1:] == k_sorted[:-1]
    nbrs = [[] for _ in range(nf)]
    for x, y in zip(f_sorted[:-1][same].tolist(), f_sorted[1:][same].tolist()):
        nbrs[x].append(y)
        nbrs[y].append(x)
    return nbrs


def select_source(src, low):
    bpy.ops.object.select_all(action="DESELECT")
    src.select_set(True)
    low.select_set(True)
    bpy.context.view_layer.objects.active = low


def bake_all(low, high, res, samples=16, dense=None):
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
    if dense:
        select_source(dense, low)
    bpy.ops.object.bake(type="NORMAL")
    select_source(high, low)
    out["normal"] = bw.pixels(target).copy()
    log("  normal (%.0fs)" % (time.time() - t0))
    scene.cycles.samples = 1
    for key, name in (("Base Color", "basecolor"), ("Roughness", "rough"), ("Metallic", "metal")):
        route_high(high, key)
        bpy.ops.object.bake(type="EMIT")
        out[name] = bw.pixels(target).copy()
        log("  %s (%.0fs)" % (name, time.time() - t0))
    if "SBTeam" in high.data.attributes:
        route_high(high, "Team")
        bpy.ops.object.bake(type="EMIT")
        out["team"] = bw.pixels(target).copy()
        log("  team (%.0fs)" % (time.time() - t0))
    restore_high(high)
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")
    scene.world.light_settings.distance = 6.0
    scene.cycles.samples = max(samples * 2, 32)
    if dense:
        select_source(dense, low)
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


def masks(bc_lin, team_mode, baked_team=None):
    """Team mask: the light, neutral armor plates (the parts a Halo-style team colour would paint); glow: bright warm
    or strongly saturated light areas (visor, lamps)."""
    h, sat, val = rgb_to_hsv(np.clip(bc_lin, 0, 1) ** (1 / 2.2))
    if team_mode == "none":
        team = np.zeros_like(val)
    else:
        team = smooth(val, 0.28, 0.42) * (1 - smooth(sat, 0.12, 0.25))
        if baked_team is not None:
            team = baked_team
        # Painted texels are noisy: blur, then re-threshold, so whole plates flip to team colour instead of speckles.
        team = smooth(blur(team, TEAM_BLUR if baked_team is None else 1.0), 0.35, 0.6)
    # Amber through yellow-green only (generated visors drift toward green). Red is left out: S3 lit a red helmet
    # stripe as a gold lamp. Then an opening pass so only coherent lamps/visors glow, not bright specks of paint.
    warm = ((h > 0.05) & (h < 0.25)).astype(np.float64)
    glow = smooth(val, 0.72, 0.9) * smooth(sat, 0.55, 0.75) * warm
    glow = smooth(blur(glow, 2.0), 0.45, 0.7)
    team = team * (1 - glow)
    return team, glow


TEAM_BLUR = 4.0           # px at 2048; scaled with the atlas size
VISOR_AMBER = np.array([1.0, 0.42, 0.06])   # linear; the concept's amber visor


def blur(x, radius):
    """Separable gaussian blur of a 2D float array (numpy only)."""
    r = max(1, int(radius * x.shape[0] / 2048))
    t = np.arange(-3 * r, 3 * r + 1)
    kern = np.exp(-0.5 * (t / r) ** 2)
    kern /= kern.sum()
    pad = np.pad(x, 3 * r, mode="edge")
    out = _blur_cols(_blur_rows(pad, kern, r), kern, r)
    return out[3 * r:3 * r + x.shape[0], 3 * r:3 * r + x.shape[1]]


def _blur_rows(a, kern, r):
    out = np.zeros_like(a)
    for i, w in enumerate(kern):
        out += w * np.roll(a, i - 3 * r, axis=0)
    return out


def _blur_cols(a, kern, r):
    out = np.zeros_like(a)
    for i, w in enumerate(kern):
        out += w * np.roll(a, i - 3 * r, axis=1)
    return out


TARGET_MEDIAN = 0.16   # linear median albedo of the armor (~0.44 sRGB): mid grey, like the concept sheet
BLACK_LIFT = 0.045     # linear albedo of the darkest cloth after the lift
TEAM_FLOOR = 0.32      # linear albedo floor under the team mask so the team tint stays readable


def exposure_fix(bc):
    """Generated textures often carry baked-in shading and come out dark. Scale albedo so the median of the covered
    texels sits at TARGET_MEDIAN (gain clamped to 0.6..3), then keep everything inside PBR-safe 0.02..0.9."""
    lum = (bc * np.array([0.2126, 0.7152, 0.0722])).sum(-1)
    used = lum > 1e-3
    if used.sum() < 100:
        return bc
    gain = float(np.clip(TARGET_MEDIAN / max(np.median(lum[used]), 1e-4), 0.6, 2.0))
    print(f"albedo exposure: median {np.median(lum[used]):.3f} -> gain {gain:.2f}")
    # Lift the blacks: S4's helmet, legs and pack were crushed to black in game. AAA undersuits sit around 0.04-0.06.
    out = BLACK_LIFT + np.clip(bc * gain, 0.0, 0.9) * (1 - BLACK_LIFT / 0.9)
    out[~used] = bc[~used]
    return out


def derived_rough_metal(bc, team, glow):
    """Painted plates semi-gloss, dark cloth and webbing rough, worn light spots (chips) on dark parts read as bare
    metal, grime (local darkening) rougher. Values follow the scripted soldier's material table."""
    h, sat, val = rgb_to_hsv(np.clip(bc, 0, 1) ** (1 / 2.2))
    local = blur(val, 6.0)
    grime = np.clip((local - val) * 4, 0, 1)
    # Only strong, isolated bright marks count as chipped metal; S3's close-up read the generator's noise as
    # speckle when every small contrast became metal.
    chips = smooth(np.clip((val - local) * 3, 0, 1), 0.45, 0.8) * (1 - team)
    rough = 0.8 - 0.3 * smooth(val, 0.2, 0.5) - 0.12 * team + 0.12 * blur(grime, 2.0) - 0.2 * chips
    rough = blur(rough, 1.5) * (1 - glow) + 0.1 * glow
    metal = chips * 0.8
    return rough, metal


DETAIL_STRENGTH = 2.2   # albedo high-pass -> micro normal: panel seams, scratches and stitching the mesh doesn't carry


def add_albedo_detail(n, bc):
    lum = (np.clip(bc, 0, 1) * np.array([0.2126, 0.7152, 0.0722])).sum(-1) ** (1 / 2.2)
    height = lum - blur(lum, 3.0)
    gx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 0.5 * DETAIL_STRENGTH
    gy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 0.5 * DETAIL_STRENGTH
    v = n * 2 - 1
    # whiteout blend of the baked normal with the detail normal (-gx, +gy after the green flip above)
    out = np.stack([v[..., 0] - gx, v[..., 1] + gy, v[..., 2]], -1)
    out /= np.linalg.norm(out, axis=-1, keepdims=True) + 1e-8
    return out * 0.5 + 0.5


def write_textures(surf, res):
    os.makedirs(TEX_OUT, exist_ok=True)
    n = surf["normal"][:, :, :3].copy()
    n[:, :, 1] = 1.0 - n[:, :, 1]
    n = bw.fix_normal_misses(n)
    n = add_albedo_detail(n, surf["basecolor"][:, :, :3])
    bw.save_png(n, os.path.join(TEX_OUT, "T_SB_Soldier_N.png"), "RGB")
    bc = exposure_fix(surf["basecolor"][:, :, :3])
    team, glow = masks(bc, TEAM_MODE, surf["team"][:, :, 0] if "team" in surf else None)
    # Plates under the team mask bake toward a light neutral so the team colour reads (the material multiplies it in).
    lum = (bc * np.array([0.2126, 0.7152, 0.0722])).sum(-1, keepdims=True)
    # Desktop S1: a dark suit read near black in game and red vs blue was indistinguishable, so team plates get a floor.
    # Painted plates: the generator's texel noise (S3 close-up: "blotchy, speckled") is smoothed away and only the
    # broad light/dark variation kept, so the team colour reads as one paint job.
    plate = 0.6 * blur(lum[..., 0], 6.0) + 0.4 * blur(lum[..., 0], 1.5)
    neutral = np.repeat(np.maximum(plate * 1.15, TEAM_FLOOR)[..., None], 3, axis=2)
    bc_out = bc * (1 - team[..., None] * 0.92) + neutral * (team[..., None] * 0.92)
    # Visor and lamps take the concept's amber whatever hue the generator painted them.
    bc_out = bc_out * (1 - glow[..., None]) + np.clip(lum * 0.8 + 0.12, 0, 0.6) * VISOR_AMBER * glow[..., None]
    bw.save_png(np.concatenate([bw.linear_to_srgb(bc_out), glow[..., None]], axis=2), os.path.join(TEX_OUT, "T_SB_Soldier_BC.png"), "RGBA")
    rough, metal = surf["rough"][:, :, 0], surf["metal"][:, :, 0]
    if rough.std() < 0.01:   # the generator gave base colour only: derive painted plate / cloth roughness from it
        rough, metal = derived_rough_metal(bc, team, glow)
    orm = np.stack([surf["ao"][:, :, 0], np.clip(rough, 0.08, 1), np.clip(metal, 0, 1)], axis=2)
    bw.save_png(orm, os.path.join(TEX_OUT, "T_SB_Soldier_ORM.png"), "RGB")
    bw.save_png(np.stack([team, glow, np.zeros_like(team)], axis=2), os.path.join(TEX_OUT, "T_SB_Soldier_M.png"), "RGB")


def unwrap_organic(obj):
    """Generated meshes are organic: a wider angle limit gives fewer, larger islands than the weapons' unwrap, and a
    wider margin keeps the islands from bleeding into each other in the lower mips (S4: dark and white flecks along
    plate borders at mid range)."""
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(70), island_margin=0.006, area_weight=0.0, scale_to_bounds=False)
    bpy.ops.uv.pack_islands(rotate=True, margin=0.006)
    bpy.ops.object.mode_set(mode="OBJECT")


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
    detail = import_model(opt("--detail", None)) if "--detail" in args else None
    normalise(high, opt("--yaw", "auto"), [detail] if detail else [])
    # Very dense scans/generations: thin to ~600k triangles first so the fit and bakes stay quick.
    decimate(high, 600000)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    model_bvh = BVHTree.FromObject(high, depsgraph)
    log("pose fit:")
    pose, j_posed = fit_pose(model_bvh, skin, j)
    labels, bones = label_faces(high, j_posed, keep_hands)
    dropped = sum(1 for p in labels if p is None)
    pieces = split_pieces(high, labels, bones, pose)
    detail_pieces = {}
    if detail:
        # The untextured, denser mesh of the same generation: only used as the source of the normal and AO bakes.
        decimate(detail, 900000)
        dl, db = label_like(detail, high, labels, bones)
        detail_pieces = split_pieces(detail, dl, db, pose)
        bpy.data.objects.remove(detail, do_unlink=True)
    bpy.data.objects.remove(high, do_unlink=True)
    log("cut into %d pieces (%d faces dropped: hands/fingers) (%.0fs)" % (len(pieces), dropped, time.time() - t0))
    body = skin_bvh(skin_path)
    for piece, obj in pieces.items():
        moved = push_out(obj, body)
        if moved:
            log("  %s: %d vertices pushed out of the body" % (piece, moved))
    for obj in detail_pieces.values():
        push_out(obj, body)
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
    dense = None
    if detail_pieces:
        dense, _ = join_ordered(detail_pieces, "DETAIL")
        for p in dense.data.polygons:
            p.use_smooth = True
    for p in high.data.polygons:
        p.use_smooth = True
    for p in low.data.polygons:
        p.use_smooth = True
    log("HIGH %d tris, LOW %d tris (%.0fs)" % (k.triangle_count(high), k.triangle_count(low), time.time() - t0))
    unwrap_organic(low)
    if TEAM_MODE != "none":
        face_team(high)
    surf = bake_all(low, high, res, dense=dense)
    write_textures(surf, res)
    bpy.data.objects.remove(high, do_unlink=True)
    if dense:
        bpy.data.objects.remove(dense, do_unlink=True)
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
            if f.startswith("SM_SB_Armor_") and f.endswith(".fbx") and f[:-4] not in exported:  # (the suit is rewritten below)
                os.remove(os.path.join(arm, f))
                log("removed stale " + f)
        for o in out:
            k.export_fbx(os.path.join(arm, o.name + ".fbx"), [o])
        # The whole suit as one mesh, same space and UVs: Tools/make_soldier_suit.py skins it to the mannequin skeleton
        # in Unreal (weights transferred from SKM_Manny), so it bends with the body instead of tearing at the joints.
        suit, _ = join_ordered({o.name[len("SM_SB_Armor_"):]: o for o in out}, "SM_SB_SoldierSuit", copy=True)
        bw.single_slot(suit, SLOT)
        k.export_fbx(os.path.join(arm, "SM_SB_SoldierSuit.fbx"), [suit])
        bpy.data.objects.remove(suit, do_unlink=True)
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
