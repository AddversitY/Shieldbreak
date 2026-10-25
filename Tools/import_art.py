"""Imports Shieldbreak's scripted art (Art/Export/**.fbx, made by Art/blender/*.py) into /Game/Shieldbreak/Art (headless).

- One master material, M_SB_Surface (BaseColor, Metallic, Roughness, EmissiveColor, EmissiveStrength), and one
  instance per palette entry (MI_SB_<Name>, from Art/Export/palette.json).
- Every FBX becomes a static mesh in the folder named after its subfolder (Weapons, Armor, ...). Material slots are
  named M_<Palette>, and each gets MI_SB_<Palette>. SOCKET_* empties become sockets.
- Art/Export/Textures/*.png (made by Art/textures/make_textures.py) become textures in /Game/Shieldbreak/Art/Textures, and
  the world material M_SB_World (world-aligned triplanar detail texture tinted by the map palette; AShieldbreakArena
  sets "Tint" per piece) gets one instance per surface kind (MI_SB_World_<Kind>, see WORLD_STYLES).
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
TEXTURES = ROOT + "/Textures"
# Surface kinds for the map pieces: (detail texture, tile size in cm, metallic, roughness).
WORLD_STYLES = {
    "Panel": ("T_SB_Panel", 400.0, 0.35, 0.4),
    "Tile": ("T_SB_Tile", 400.0, 0.0, 0.6),
    "Stone": ("T_SB_Stone", 700.0, 0.0, 0.85),
    "Grass": ("T_SB_Grass", 300.0, 0.0, 0.95),
}
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


def import_textures():
    textures = {}
    for png in sorted(glob.glob(os.path.join(EXPORT, "Textures", "*.png"))):
        name = os.path.splitext(os.path.basename(png))[0]
        task = unreal.AssetImportTask()
        task.filename = png
        task.destination_path = TEXTURES
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        tools.import_asset_tasks([task])
        texture = unreal.load_asset("%s/%s" % (TEXTURES, name))
        if texture:
            textures[name] = texture
        else:
            log("FAILED " + png)
    log("%d textures" % len(textures))
    return textures


def world_material(default_texture):
    """M_SB_World: BaseColor = Tint * 2 * detail texture, projected along world X, Y and Z and blended by the normal,
    so stretched cubes keep square, evenly sized texels. Rebuilt on every run."""
    path = MATERIALS + "/M_SB_World"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_SB_World", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y, **props):
        n = mel.create_material_expression(mat, cls, x, y)
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def link(a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            log("M_SB_World: could not connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))

    position = node(unreal.MaterialExpressionWorldPosition, -1600, 0)
    tile = node(unreal.MaterialExpressionScalarParameter, -1600, 150, parameter_name="TileSize", default_value=400.0)
    uvw = node(unreal.MaterialExpressionDivide, -1400, 0)
    link(position, "", uvw, "A")
    link(tile, "", uvw, "B")

    # One sample per projection: X faces use (Y, Z), Y faces (X, Z), Z faces (X, Y).
    samples = []
    for i, (r, g, b) in enumerate(((False, True, True), (True, False, True), (True, True, False))):
        mask = node(unreal.MaterialExpressionComponentMask, -1200, -300 + 300 * i, r=r, g=g, b=b, a=False)
        link(uvw, "", mask, "")
        sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -1000, -300 + 300 * i,
                      parameter_name="Detail", texture=default_texture)
        link(mask, "", sample, "UVs")
        samples.append(sample)

    # Blend weights: |normal|^4, normalized.
    normal = node(unreal.MaterialExpressionVertexNormalWS, -1400, 700)
    absolute = node(unreal.MaterialExpressionAbs, -1250, 700)
    link(normal, "", absolute, "")
    power = node(unreal.MaterialExpressionPower, -1100, 700, const_exponent=4.0)
    link(absolute, "", power, "Base")
    ones = node(unreal.MaterialExpressionConstant3Vector, -1100, 850, constant=unreal.LinearColor(1, 1, 1, 1))
    total = node(unreal.MaterialExpressionDotProduct, -950, 800)
    link(power, "", total, "A")
    link(ones, "", total, "B")
    weights = node(unreal.MaterialExpressionDivide, -800, 700)
    link(power, "", weights, "A")
    link(total, "", weights, "B")

    blended = None
    for i, (sample, channel) in enumerate(zip(samples, ("R", "G", "B"))):
        mask = node(unreal.MaterialExpressionComponentMask, -650, 600 + 120 * i,
                    r=channel == "R", g=channel == "G", b=channel == "B", a=False)
        link(weights, "", mask, "")
        weighted = node(unreal.MaterialExpressionMultiply, -500, -300 + 300 * i)
        link(sample, "RGB", weighted, "A")
        link(mask, "", weighted, "B")
        if blended is None:
            blended = weighted
        else:
            add = node(unreal.MaterialExpressionAdd, -350, -150 + 200 * i)
            link(blended, "", add, "A")
            link(weighted, "", add, "B")
            blended = add

    tint = node(unreal.MaterialExpressionVectorParameter, -500, -500, parameter_name="Tint",
                default_value=unreal.LinearColor(0.25, 0.25, 0.25, 1))
    tint2 = node(unreal.MaterialExpressionMultiply, -350, -450, const_b=2.0)
    link(tint, "", tint2, "A")
    base = node(unreal.MaterialExpressionMultiply, -200, -300)
    link(blended, "", base, "A")
    link(tint2, "", base, "B")
    metallic = node(unreal.MaterialExpressionScalarParameter, -200, 0, parameter_name="Metallic", default_value=0.0)
    roughness = node(unreal.MaterialExpressionScalarParameter, -200, 100, parameter_name="Roughness", default_value=0.7)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("built %s (%d expressions)" % (path, mel.get_num_material_expressions(mat)))
    return mat


def world_instances(master, textures):
    for kind, (texture_name, tile_size, metallic, roughness) in WORLD_STYLES.items():
        texture = textures.get(texture_name)
        if not texture:
            log("MISSING texture %s for MI_SB_World_%s" % (texture_name, kind))
            continue
        path = "%s/MI_SB_World_%s" % (MATERIALS, kind)
        if eal.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = tools.create_asset("MI_SB_World_" + kind, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        mel.set_material_instance_texture_parameter_value(mi, "Detail", texture)
        mel.set_material_instance_scalar_parameter_value(mi, "TileSize", tile_size)
        mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
        eal.save_asset(path)
        log("%s: %s, tile %.0f cm" % (path, texture_name, tile_size))


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
    textures = import_textures()
    if textures:
        world = world_material(textures.get("T_SB_Panel") or next(iter(textures.values())))
        world_instances(world, textures)
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
