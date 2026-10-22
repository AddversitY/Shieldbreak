"""Builds Shieldbreak's soldier armor around the Lyra mannequin and exports one FBX per bone.

    python build_armor.py <bones.json> [SKM_Manny.obj | --proxy] [--render] [--preview-dir <dir>]

bones.json comes from Tools/ExportArtReference.bat, the skin from Tools/DumpMannequin.bat. Each piece is a faceted shell: the convex hull of the body
vertices that bone drives, pushed outward, simplified to big flat plates, trimmed, and painted by facet direction
(team plates, dark trim, visor, team-colored glow strips). Pieces are exported in the mannequin's mesh space, and the
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


# ---- The armor set -------------------------------------------------------------------------------------------

def build(verts, j, jt):
    """Returns {bone: [objects]} in Unreal mesh space."""
    pieces = {}
    side = {"l": 1.0, "r": -1.0}

    # Helmet: hull of the head, roomy and slightly flattened on the sides, with a wide faceted visor.
    head = gather(verts, ["head", "neck_02"], 0.5)
    head_c = j["head"] + Vector((0, 2.0, 8.0))
    visor_z = (head_c.z - 2.0, head_c.z + 4.5)

    def helmet_rule(c, n):
        if n.y > 0.45 and visor_z[0] < c.z < visor_z[1] and abs(c.x - head_c.x) < 8.5:
            return "Visor"
        if c.z < head_c.z - 6.0:
            return "Gunmetal"
        if abs(c.x - head_c.x) > 9.0 and abs(n.x) > 0.7 and abs(c.z - head_c.z) < 3:
            return "TeamGlow"
        return "Team"

    pieces["head"] = [plate("helmet", head, 3.2, helmet_rule, cuts=[(j["neck_01"] + Vector((0, 0, 3)), (0, 0, -1))],
                            scale=(1.02, 1.08, 1.0), pivot=head_c, simplify=14)]

    # Chest and back: upper torso (spine_04/05 + clavicles), broad and squared off.
    chest = gather(verts, ["spine_05", "spine_04", "clavicle_l", "clavicle_r"], 0.55,
                   clip=lambda c: c.z > j["spine_04"].z - 2)
    neck_z = j["neck_01"].z

    def chest_rule(c, n):
        if c.z > neck_z - 4:
            return "Gunmetal"
        if n.y > 0.5 and abs(c.x) < 3.0:
            return "Gunmetal"
        if n.y < -0.6 and abs(c.x) < 7 and c.z < j["spine_05"].z + 6:
            return "TeamGlow" if abs(c.x) < 2.5 else "Gunmetal"
        return "Team"

    pieces["spine_05"] = [plate("chest", chest, 3.4, chest_rule, cuts=[(j["neck_01"] + Vector((0, 0, 4)), (0, 0, 1))],
                                scale=(1.08, 1.06, 1.0), pivot=j["spine_05"], simplify=13)]

    # Abdomen: a narrower dark segmented belt band (spine_02/03).
    belly = gather(verts, ["spine_02", "spine_03"], 0.55)
    pieces["spine_03"] = [plate("abdomen", belly, 1.8, lambda c, n: "Gunmetal" if abs(n.z) < 0.6 else "Undersuit",
                                scale=(0.97, 0.97, 0.85), pivot=j["spine_03"], simplify=16)]

    # Pelvis: belt + front and side plates.
    hips = gather(verts, ["pelvis"], 0.6, clip=lambda c: c.z > j["thigh_l"].z - 9)

    def pelvis_rule(c, n):
        if c.z > j["pelvis"].z + 4:
            return "Gunmetal"
        return "Team" if abs(n.x) > 0.5 or n.y > 0.5 else "Undersuit"

    pieces["pelvis"] = [plate("pelvis", hips, 2.4, pelvis_rule, scale=(1.05, 1.05, 1.0), pivot=j["pelvis"], simplify=14)]

    for s, sx in side.items():
        # Shoulders: big pauldrons over the top of the upper arm.
        shoulder = gather(verts, ["upperarm_" + s, "upperarm_twist_01_" + s, "clavicle_" + s], 0.45,
                          clip=lambda c, s=s: (c - j["upperarm_" + s]).length < 15)
        sh = j["upperarm_" + s]
        arm_dir = (j["lowerarm_" + s] - sh).normalized()

        def shoulder_rule(c, n, sh=sh, arm_dir=arm_dir):
            along = (c - sh).dot(arm_dir)
            if along > 9:
                return "Gunmetal"
            return "Team"

        pieces["upperarm_" + s] = [plate("shoulder_" + s, shoulder, 3.6, shoulder_rule,
                                         cuts=[(sh + arm_dir * 12, arm_dir), (sh - Vector((sx, 0, 0)) * 6, Vector((-sx, 0, 0)))],
                                         scale=(1.1, 1.1, 1.1), pivot=sh, simplify=14)]

        # Forearm guards.
        fore = gather(verts, ["lowerarm_" + s, "lowerarm_twist_01_" + s, "lowerarm_twist_02_" + s], 0.5)
        el, wr = j["lowerarm_" + s], j["hand_" + s]
        fdir = (wr - el).normalized()
        fore = [p for p in fore if 6 < (p - el).dot(fdir) < (wr - el).length - 2]
        pieces["lowerarm_" + s] = [plate("forearm_" + s, fore, 1.8, lambda c, n, el=el, fdir=fdir: "Gunmetal" if (c - el).dot(fdir) > 20 else "Team",
                                         simplify=15)]

        # Thigh plates.
        thigh = gather(verts, ["thigh_" + s, "thigh_twist_01_" + s, "thigh_twist_02_" + s], 0.5)
        hip, knee = j["thigh_" + s], j["calf_" + s]
        tdir = (knee - hip).normalized()
        thigh = [p for p in thigh if 8 < (p - hip).dot(tdir) < (knee - hip).length - 6]
        pieces["thigh_" + s] = [plate("thigh_" + s, thigh, 1.8, lambda c, n: "Team" if n.y > -0.2 or abs(n.x) > 0.6 else "Undersuit",
                                      simplify=15)]

        # Knee + shin guards.
        shin = gather(verts, ["calf_" + s, "calf_twist_01_" + s, "calf_twist_02_" + s], 0.5)
        ankle = j["foot_" + s]
        cdir = (ankle - knee).normalized()
        shin = [p for p in shin if -4 < (p - knee).dot(cdir) < (ankle - knee).length - 7]

        def shin_rule(c, n, knee=knee, cdir=cdir):
            along = (c - knee).dot(cdir)
            if along < 4 and n.y > 0.3:
                return "Gunmetal"
            if n.y > 0.55 and 16 < along < 22:
                return "TeamGlow"
            return "Team" if n.y > -0.3 else "Undersuit"

        pieces["calf_" + s] = [plate("shin_" + s, shin, 2.3, shin_rule, simplify=15)]

        # Boots.
        foot = gather(verts, ["foot_" + s, "ball_" + s], 0.5)
        pieces["foot_" + s] = [plate("boot_" + s, foot, 1.6, lambda c, n, ankle=ankle: "Polymer" if c.z < 3 else "Gunmetal",
                                     simplify=15)]
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
