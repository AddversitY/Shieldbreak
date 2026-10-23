"""Imports Shieldbreak's scripted art (Art/Export/**.fbx, made by Art/blender/*.py) into /Game/Shieldbreak/Art (headless).

- One master material, M_SB_Surface (BaseColor, Metallic, Roughness, EmissiveColor, EmissiveStrength), and one
  instance per palette entry (MI_SB_<Name>, from Art/Export/palette.json).
- Every FBX becomes a static mesh in the folder named after its subfolder (Weapons, Armor, ...). Material slots are
  named M_<Palette>, and each gets MI_SB_<Palette>. SOCKET_* empties become sockets.
- Writes Saved/Logs/ImportArt.txt with each mesh's triangles, bounds and sockets, so the cloud can check the axes.

Run with Tools/ImportArt.bat.
"""
import glob
import json
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
EXPORT = os.path.join(PROJECT, "Art", "Export")
ROOT = "/Game/Shieldbreak/Art"
MATERIALS = ROOT + "/Materials"
lines = []
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(text):
    lines.append(text)
    unreal.log("ImportArt: " + text)


def color(rgb, a=1.0):
    return unreal.LinearColor(rgb[0], rgb[1], rgb[2], a)


def master_material():
    path = MATERIALS + "/M_SB_Surface"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = tools.create_asset("M_SB_Surface", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())

    def vector(name, default, x, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", default)
        return node

    def scalar(name, default, x, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", default)
        return node

    base = vector("BaseColor", unreal.LinearColor(0.2, 0.2, 0.2, 1), -600, -200)
    metallic = scalar("Metallic", 0.5, -600, 0)
    roughness = scalar("Roughness", 0.5, -600, 100)
    glow = vector("EmissiveColor", unreal.LinearColor(0, 0, 0, 1), -800, 250)
    strength = scalar("EmissiveStrength", 0.0, -800, 450)
    multiply = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 300)
    mel.connect_material_expressions(glow, "", multiply, "A")
    mel.connect_material_expressions(strength, "", multiply, "B")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("created " + path)
    return mat


def palette_instances(master):
    with open(os.path.join(EXPORT, "palette.json")) as f:
        palette = json.load(f)
    instances = {}
    for name, (rgb, metallic, roughness, emissive) in palette.items():
        path = "%s/MI_SB_%s" % (MATERIALS, name)
        if eal.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = tools.create_asset("MI_SB_" + name, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        mel.set_material_instance_vector_parameter_value(mi, "BaseColor", color(rgb))
        mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
        mel.set_material_instance_vector_parameter_value(mi, "EmissiveColor", color(rgb))
        mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", emissive)
        eal.save_asset(path)
        instances["M_" + name] = mi
    log("%d palette instances" % len(instances))
    return instances


def import_fbx(fbx, destination):
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = destination
    task.destination_name = os.path.splitext(os.path.basename(fbx))[0]
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
    path = "%s/%s" % (destination, task.destination_name)
    return unreal.load_asset(path), path


def describe(mesh):
    box = mesh.get_bounding_box()
    text = "tris %d, bounds min (%.1f, %.1f, %.1f) max (%.1f, %.1f, %.1f)" % (
        mesh.get_num_triangles(0), box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)
    for name in ("Muzzle",):
        socket = mesh.find_socket(name)
        if socket:
            loc = socket.relative_location
            text += ", socket %s at (%.1f, %.1f, %.1f)" % (name, loc.x, loc.y, loc.z)
    return text


def main():
    # The legacy FBX importer turns SOCKET_ empties into sockets and honors FbxImportUI.
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    master = master_material()
    instances = palette_instances(master)
    for fbx in sorted(glob.glob(os.path.join(EXPORT, "*", "*.fbx"))):
        folder = os.path.basename(os.path.dirname(fbx))
        mesh, path = import_fbx(fbx, "%s/%s" % (ROOT, folder))
        if not mesh:
            log("FAILED " + fbx)
            continue
        materials = mesh.get_editor_property("static_materials")
        missing = []
        for index, slot in enumerate(materials):
            name = str(slot.material_slot_name)
            # The FBX importer can suffix slot names (M_Gunmetal_001, M_Gunmetal_Skin); match on the palette prefix.
            key = next((k for k in instances if name == k or name.startswith(k + "_")), None)
            if key:
                mesh.set_material(index, instances[key])
            else:
                missing.append(name)
        eal.save_asset(path)
        log("%s: %s%s" % (path, describe(mesh), (", unmatched slots " + ", ".join(missing)) if missing else ""))
    out = os.path.join(PROJECT, "Saved", "Logs", "ImportArt.txt")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")


main()
