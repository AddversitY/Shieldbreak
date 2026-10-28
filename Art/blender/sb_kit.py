"""Small hard-surface modeling kit for Shieldbreak's scripted art (Blender 4.x / 5.x, run headless).

Units: 1 Blender unit = 1 cm, and coordinates are Unreal's mesh space (X right, Y forward along the barrel,
Z up) so numbers here match what the game sees. export_fbx() handles the conversion to Unreal on export.
"""
import math

import bpy  # first: bpy-as-a-module registers bmesh and mathutils
import bmesh
from mathutils import Matrix, Vector

# ---- Palette: shared by weapons, armor and (later) the environment kit -------------------------------------
# name: (base color RGB linear, metallic, roughness, emissive strength)
PALETTE = {
    "Gunmetal": ((0.1, 0.105, 0.115), 0.8, 0.42, 0.0),
    "Steel": ((0.32, 0.34, 0.36), 0.9, 0.3, 0.0),
    "Polymer": ((0.03, 0.031, 0.034), 0.0, 0.6, 0.0),
    "Olive": ((0.11, 0.13, 0.075), 0.2, 0.55, 0.0),
    "Sand": ((0.42, 0.36, 0.25), 0.15, 0.6, 0.0),
    "Ivory": ((0.72, 0.72, 0.7), 0.25, 0.32, 0.0),
    "Hazard": ((0.8, 0.5, 0.04), 0.1, 0.5, 0.0),
    "GlowCyan": ((0.15, 0.85, 1.0), 0.0, 0.3, 6.0),
    "GlowAmber": ((1.0, 0.55, 0.12), 0.0, 0.3, 5.0),
    "Lens": ((0.02, 0.08, 0.12), 0.6, 0.08, 0.6),
    # Armor: "Team" is tinted per team in Unreal (TeamColor parameter); the preview shows red.
    "Team": ((0.55, 0.04, 0.03), 0.15, 0.5, 0.0),
    "Undersuit": ((0.03, 0.03, 0.032), 0.1, 0.7, 0.0),
    "Visor": ((0.9, 0.42, 0.06), 0.95, 0.06, 0.25),
    "TeamGlow": ((1.0, 0.18, 0.08), 0.0, 0.3, 4.0),
}


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 0.01  # 1 unit = 1 cm
    return scene


def material(name):
    mat = bpy.data.materials.get("M_" + name)
    if mat:
        return mat
    color, metallic, roughness, emissive = PALETTE[name]
    mat = bpy.data.materials.new("M_" + name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if emissive > 0:
        bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emissive
    mat.diffuse_color = (*color, 1.0)
    return mat


def _finish(obj, mat, bevel, segments=1, angle=35.0):
    obj.data.materials.append(material(mat))
    if bevel > 0:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = segments
        mod.limit_method = "ANGLE"
        mod.angle_limit = math.radians(angle)
        mod.harden_normals = True
        mod.miter_outer = "MITER_ARC"
    return obj


def _link(name, mesh):
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def box(name, center, size, mat, bevel=0.6, segments=1, taper=None):
    """Axis-aligned box. taper=(sx, sz) scales the +Y face, for wedge-like noses."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        if taper and v.co.y > 0:
            v.co.x *= taper[0]
            v.co.z *= taper[1]
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2])) + Vector(center)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    return _finish(_link(name, mesh), mat, bevel, segments)


def profile(name, points_yz, thickness, mat, x=0.0, bevel=0.6, segments=1, inset_back=0.0):
    """Side-profile extrusion: a polygon in the YZ plane (counter-clockwise when seen from +X), extruded along X
    and centered on x. inset_back shrinks the far (+Y-most) half's thickness for a tapered look."""
    bm = bmesh.new()
    half = thickness * 0.5
    front = [bm.verts.new((x + half, y, z)) for y, z in points_yz]
    back = [bm.verts.new((x - half, y, z)) for y, z in points_yz]
    n = len(points_yz)
    bm.faces.new(front)
    bm.faces.new(list(reversed(back)))
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((front[j], front[i], back[i], back[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    return _finish(_link(name, mesh), mat, bevel, segments)


def cyl(name, center, radius, length, mat, axis="Y", verts=12, bevel=0.3, radius2=None):
    """Cylinder (or cone frustum with radius2) along an axis, centered."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=radius, radius2=radius if radius2 is None else radius2, depth=length)
    rot = {"X": Matrix.Rotation(math.radians(90), 4, "Y"), "Y": Matrix.Rotation(math.radians(-90), 4, "X"), "Z": Matrix.Identity(4)}[axis]
    bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(center)) @ rot, verts=bm.verts)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = _link(name, mesh)
    for p in obj.data.polygons:
        p.use_smooth = True
    return _finish(obj, mat, bevel, 1, angle=60.0)


def ring(name, center, radius, tube, mat, axis="Y", verts=16, tube_verts=6):
    bpy.ops.mesh.primitive_torus_add(major_radius=radius, minor_radius=tube, major_segments=verts, minor_segments=tube_verts, location=center,
                                     rotation={"X": (0, math.radians(90), 0), "Y": (math.radians(90), 0, 0), "Z": (0, 0, 0)}[axis])
    obj = bpy.context.active_object
    obj.name = name
    obj.data.materials.append(material(mat))
    for p in obj.data.polygons:
        p.use_smooth = True
    return obj


def mirror_x(obj):
    """Mirrors across X = 0 (applied before the bevel so the seam stays clean)."""
    mod = obj.modifiers.new("MirrorX", "MIRROR")
    mod.use_axis = (True, False, False)
    mod.use_clip = True
    obj.modifiers.move(len(obj.modifiers) - 1, 0)
    return obj


def socket(name, location, parent):
    empty = bpy.data.objects.new("SOCKET_" + name, None)
    empty.empty_display_size = 2.0
    bpy.context.scene.collection.objects.link(empty)
    empty.location = location
    empty.parent = parent
    return empty


def join(name, objects):
    """Applies modifiers and joins everything into one mesh object named name (materials become slots)."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    meshes = []
    for obj in objects:
        evaluated = obj.evaluated_get(depsgraph)
        mesh = bpy.data.meshes.new_from_object(evaluated, depsgraph=depsgraph)
        mesh.transform(obj.matrix_world)
        meshes.append(mesh)
    for obj in objects:
        bpy.data.objects.remove(obj, do_unlink=True)

    bm = bmesh.new()
    slots = []
    for mesh in meshes:
        offset_map = []
        for mat in mesh.materials:
            if mat not in slots:
                slots.append(mat)
            offset_map.append(slots.index(mat))
        temp = bmesh.new()
        temp.from_mesh(mesh)
        for f in temp.faces:
            f.material_index = offset_map[f.material_index] if offset_map else 0
        temp_mesh = bpy.data.meshes.new("tmp")
        temp.to_mesh(temp_mesh)
        temp.free()
        bm.from_mesh(temp_mesh)
        bpy.data.meshes.remove(temp_mesh)
    result = bpy.data.meshes.new(name)
    bm.to_mesh(result)
    bm.free()
    for mat in slots:
        result.materials.append(mat)
    # Custom split normals from harden_normals survive through new_from_object; keep sharp edges otherwise.
    obj = _link(name, result)
    for mesh in meshes:
        bpy.data.meshes.remove(mesh)
    return obj


def triangle_count(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def export_fbx(path, objects):
    """Exports objects (meshes + their SOCKET_ children) so Unreal reads our cm / X-right Y-forward Z-up numbers as-is.

    Unreal converts an FBX scene to right-handed Z-up with -Y as the front, then flips Y into its left-handed space.
    Blender's coordinates go out unchanged with forward -Y / up Z, so we mirror Y on the way out (object scale -1,
    which the importer bakes into the vertices) and the two flips cancel. The importer keeps the winding through that
    mirror, though (in game the armor was inside out: its outer faces were culled and the skin showed through from
    behind), so we reverse the faces too. Tools/CheckWinding.bat compares the imported meshes with the engine cube."""
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.scale.y = -obj.scale.y
        if obj.type == "MESH":
            obj.data.flip_normals()
        obj.select_set(True)
        for child in obj.children:
            child.select_set(True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, global_scale=1.0,
                             apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z",
                             object_types={"MESH", "EMPTY"}, use_mesh_modifiers=True, mesh_smooth_type="FACE",
                             use_tspace=False, add_leaf_bones=False, bake_anim=False, path_mode="STRIP")
    for obj in objects:
        obj.scale.y = -obj.scale.y
        if obj.type == "MESH":
            obj.data.flip_normals()


def bounds(obj):
    xs = [v.co.x for v in obj.data.vertices]
    ys = [v.co.y for v in obj.data.vertices]
    zs = [v.co.z for v in obj.data.vertices]
    return "X %.1f..%.1f Y %.1f..%.1f Z %.1f..%.1f" % (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs))


def write_palette(path):
    """palette.json for Tools/import_art.py: name -> [rgb, metallic, roughness, emissive]."""
    import json
    with open(path, "w") as f:
        json.dump({name: [list(rgb), m, r, e] for name, (rgb, m, r, e) in PALETTE.items()}, f, indent=1)
