"""Imports only the map set pieces (Art/Export/MapKit/*.fbx, made by Art/blender/build_map_kit.py) into
/Game/Shieldbreak/Art/MapKit, headless. Does not reimport the weapons, armor or other art.

- Material slots get the palette instance named by the slot (M_GlowCyan -> MI_SB_GlowCyan, ...); missing palette
  instances (e.g. MI_SB_Bark on a first run) are created from Art/Export/palette.json under M_SB_Surface.
  The game swaps the solid slots for its tinted world materials at runtime and keeps the M_Glow* ones.
- M_SB_Surface and M_SB_World are flagged for instanced static meshes (the arena draws the kit instanced).
- Writes Saved/Logs/MapArtImport.txt with each mesh's triangles, bounds and slots; fails if a mesh is missing, does
  not stand on its pivot, or comes in at the wrong scale (checked against Art/Export/MapKit/mapkit_report.txt).

Run with Tools/ImportMapArt.bat.
"""
import glob
import json
import os
import re

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
EXPORT = os.path.join(PROJECT, "Art", "Export")
DEST = "/Game/Shieldbreak/Art/MapKit"
MATERIALS = "/Game/Shieldbreak/Art/Materials"
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
lines = []


def log(text):
    lines.append(text)
    unreal.log("ImportMapArt: " + text)


def instanced_usage():
    for name in ("M_SB_Surface", "M_SB_World"):
        material = unreal.load_asset("%s/%s" % (MATERIALS, name))
        if not material:
            raise RuntimeError("Missing %s/%s: run Tools/ImportArt.bat first" % (MATERIALS, name))
        mel.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        mel.recompile_material(material)
        eal.save_loaded_asset(material)


def palette_instance(slot, palette, master):
    key = next((k for k in palette if slot == "M_" + k or slot.startswith("M_" + k + "_")), None)
    if not key:
        return None
    path = "%s/MI_SB_%s" % (MATERIALS, key)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    rgb, metallic, roughness, emissive = palette[key]
    mi = tools.create_asset("MI_SB_" + key, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    mel.set_material_instance_vector_parameter_value(mi, "BaseColor", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
    mel.set_material_instance_vector_parameter_value(mi, "EmissiveColor", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))
    mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", emissive)
    eal.save_asset(path)
    log("created " + path)
    return mi


def expected_sizes():
    """name -> (size x, y, z) from the Blender report line 'X a..b Y c..d Z e..f'."""
    sizes = {}
    with open(os.path.join(EXPORT, "MapKit", "mapkit_report.txt")) as f:
        for line in f:
            match = re.match(r"(\S+): .*X (-?[\d.]+)\.\.(-?[\d.]+) Y (-?[\d.]+)\.\.(-?[\d.]+) Z (-?[\d.]+)\.\.(-?[\d.]+)", line)
            if match:
                v = [float(x) for x in match.groups()[1:]]
                sizes[match.group(1)] = (v[1] - v[0], v[3] - v[2], v[5] - v[4], v[4])
    return sizes


def import_fbx(fbx):
    name = os.path.splitext(os.path.basename(fbx))[0]
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = ui.static_mesh_import_data
    data.combine_meshes = True
    data.auto_generate_collision = False
    data.generate_lightmap_u_vs = False
    data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    data.import_uniform_scale = 1.0
    task.options = ui
    tools.import_asset_tasks([task])
    return name, unreal.load_asset("%s/%s" % (DEST, name))


def main():
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    instanced_usage()
    master = unreal.load_asset(MATERIALS + "/M_SB_Surface")
    with open(os.path.join(EXPORT, "palette.json")) as f:
        palette = json.load(f)
    sizes = expected_sizes()
    problems = []
    fbxs = sorted(glob.glob(os.path.join(EXPORT, "MapKit", "*.fbx")))
    for fbx in fbxs:
        name, mesh = import_fbx(fbx)
        if not isinstance(mesh, unreal.StaticMesh):
            problems.append("import failed: " + name)
            continue
        slots = []
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            slot_name = str(slot.material_slot_name)
            mi = palette_instance(slot_name, palette, master)
            if mi:
                mesh.set_material(index, mi)
            slots.append(slot_name + ("" if mi else " (unmatched)"))
        eal.save_asset("%s/%s" % (DEST, name))
        box = mesh.get_bounding_box()
        size = (box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z)
        log("%s: tris %d, bounds (%.1f, %.1f, %.1f) .. (%.1f, %.1f, %.1f), slots %s" % (
            name, mesh.get_num_triangles(0), box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z, ", ".join(slots)))
        want = sizes.get(name)
        if want and any(abs(a - b) > 2.0 for a, b in zip(size, want[:3])):
            problems.append("%s: size %s, expected %s (axes or scale)" % (name, size, want[:3]))
        if want and abs(box.min.z - want[3]) > 2.0:
            problems.append("%s: base at z %.1f, expected %.1f" % (name, box.min.z, want[3]))
    if len(fbxs) != len(sizes):
        problems.append("%d FBX files but %d in mapkit_report.txt" % (len(fbxs), len(sizes)))
    log("RESULT " + ("OK, %d meshes" % len(fbxs) if not problems else "FAILED: " + "; ".join(problems)))
    out = os.path.join(PROJECT, "Saved", "Logs", "MapArtImport.txt")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")
    if problems:
        raise RuntimeError("; ".join(problems))


main()
