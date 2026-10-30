"""Imports the sci-fi kit pieces (Art/Export/SciFiKit/*.fbx, made by Art/blender/build_scifi_kit.py from Quaternius'
CC0 Modular Sci-Fi MegaKit) into /Game/Shieldbreak/Art/SciFiKit, headless, with the kit's trim-sheet textures.

- Textures come from the unzipped kit's Textures folder (Saved/Quaternius/kit/<kit>/Textures, fetched by
  Tools/GetSciFiKit.ps1; SB_SCIFI_TEXTURES overrides) into /Game/Shieldbreak/Art/SciFiKit/Textures.
- M_SB_Trim (rebuilt each run): the trim sheet's colour times Tint (x2, so 0.5 keeps it), its detail mask painted
  AccentColor and glowing, its normal map and packed AO / roughness / metallic. The game sets Tint and AccentColor per
  map (AShieldbreakArena::BuildCladding). Instances MI_SB_Trim01 / 02 / 03 / 03Dark pick the sheet.
- Slots M_Trim* get those instances; M_KitLight gets MI_SB_GlowAmber and M_KitDark MI_SB_Gunmetal.
- Writes Saved/Logs/SciFiKitImport.txt with each mesh's triangles, bounds and slots; fails if a mesh is missing or
  comes in at the wrong size (checked against Art/Export/SciFiKit/scifikit_report.txt).

Run with Tools/ImportSciFiKit.bat.
"""
import glob
import os
import re

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
EXPORT = os.path.join(PROJECT, "Art", "Export", "SciFiKit")
DEST = "/Game/Shieldbreak/Art/SciFiKit"
TEXTURES = DEST + "/Textures"
MATERIALS = "/Game/Shieldbreak/Art/Materials"
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
lines = []

# Sheet -> textures (BaseColor, Normal, ORM, DetailMask, Emissive); sheet 03 has no mask or emissive.
SHEETS = {
    "01": ("T_Trim_01_BaseColor", "T_Trim_01_Normal", "T_Trim_01_ORM", "T_Trim_01_DetailMask", "T_Trim_01_Emissive"),
    "02": ("T_Trim_02_BaseColor", "T_Trim_02_Normal", "T_Trim_02_ORM", "T_Trim_02_DetailMask", None),
    "03": ("T_Trim_03_BaseColor", "T_Trim_03_Normal", "T_Trim_03_ORM", None, None),
}
# Instance -> (sheet, shade: multiplies the colour, accent amount)
INSTANCES = {
    "Trim01": ("01", 1.0, 1.0),
    "Trim02": ("02", 1.0, 1.0),
    "Trim03": ("03", 1.0, 0.0),
    "Trim03Dark": ("03", 0.35, 0.0),
}


def log(text):
    lines.append(text)
    unreal.log("ImportSciFiKit: " + text)


def texture_folder():
    override = os.environ.get("SB_SCIFI_TEXTURES")
    if override:
        return override
    root = os.path.join(PROJECT, "Saved", "Quaternius", "kit")
    for folder, dirs, files in os.walk(root):
        if os.path.basename(folder) == "Textures" and "T_Trim_01_BaseColor.png" in files:
            return folder
    raise RuntimeError("No kit Textures folder under %s: run Tools/GetSciFiKit.ps1 first" % root)


def import_textures():
    folder = texture_folder()
    wanted = sorted({name for sheet in SHEETS.values() for name in sheet if name})
    textures = {}
    for name in wanted:
        task = unreal.AssetImportTask()
        task.filename = os.path.join(folder, name + ".png")
        task.destination_path = TEXTURES
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        tools.import_asset_tasks([task])
        texture = unreal.load_asset("%s/%s" % (TEXTURES, name))
        if not texture:
            raise RuntimeError("texture import failed: " + name)
        if name.endswith("_Normal"):
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("srgb", False)
            # The kit ships glTF-convention (OpenGL, green up) normal maps; Unreal reads green down.
            texture.set_editor_property("flip_green_channel", True)
        elif name.endswith("_ORM") or name.endswith("_DetailMask"):
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            texture.set_editor_property("srgb", False)
        eal.save_asset("%s/%s" % (TEXTURES, name))
        textures[name] = texture
    log("%d textures from %s" % (len(textures), folder))
    return textures


def trim_material(textures):
    path = MATERIALS + "/M_SB_Trim"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_SB_Trim", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("two_sided", True)

    def node(cls, x, y, **props):
        n = mel.create_material_expression(mat, cls, x, y)
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def link(a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            log("M_SB_Trim: could not connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))

    def sample(name, texture, sampler, x, y):
        return node(unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, texture=texture, sampler_type=sampler)

    color = sample("BaseColorTex", textures["T_Trim_01_BaseColor"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1200, -400)
    normal = sample("NormalTex", textures["T_Trim_01_Normal"], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1200, 400)
    orm = sample("ORMTex", textures["T_Trim_01_ORM"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1200, 100)
    mask = sample("MaskTex", textures["T_Trim_01_DetailMask"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1200, -100)
    lights = sample("EmissiveTex", textures["T_Trim_01_Emissive"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1200, -700)

    # Colour: sheet x Tint x 2 x Shade, then the detail mask painted with the accent.
    tint = node(unreal.MaterialExpressionVectorParameter, -1000, -550, parameter_name="Tint", default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1))
    shade = node(unreal.MaterialExpressionScalarParameter, -1000, -450, parameter_name="Shade", default_value=1.0)
    tint2 = node(unreal.MaterialExpressionMultiply, -850, -500, const_b=2.0)
    link(tint, "", tint2, "A")
    tinted = node(unreal.MaterialExpressionMultiply, -700, -450)
    link(color, "RGB", tinted, "A")
    link(tint2, "", tinted, "B")
    shaded = node(unreal.MaterialExpressionMultiply, -550, -450)
    link(tinted, "", shaded, "A")
    link(shade, "", shaded, "B")
    accent = node(unreal.MaterialExpressionVectorParameter, -1000, -250, parameter_name="AccentColor", default_value=unreal.LinearColor(1.0, 0.5, 0.12, 1))
    amount = node(unreal.MaterialExpressionScalarParameter, -1000, -150, parameter_name="AccentAmount", default_value=1.0)
    painted = node(unreal.MaterialExpressionMultiply, -850, -150)
    link(mask, "R", painted, "A")
    link(amount, "", painted, "B")
    base = node(unreal.MaterialExpressionLinearInterpolate, -400, -400)
    link(shaded, "", base, "A")
    link(accent, "", base, "B")
    link(painted, "", base, "Alpha")

    # Glow: the accent stripes and the sheet's small lights in the accent colour, plus a little self light.
    glow = node(unreal.MaterialExpressionScalarParameter, -700, -250, parameter_name="AccentGlow", default_value=6.0)
    stripe = node(unreal.MaterialExpressionMultiply, -550, -200)
    link(painted, "", stripe, "A")
    link(glow, "", stripe, "B")
    lamp_level = node(unreal.MaterialExpressionScalarParameter, -1000, -800, parameter_name="LightGlow", default_value=8.0)
    lamp = node(unreal.MaterialExpressionMultiply, -850, -750)
    link(lights, "R", lamp, "A")
    link(lamp_level, "", lamp, "B")
    glow_sum = node(unreal.MaterialExpressionAdd, -400, -200)
    link(stripe, "", glow_sum, "A")
    link(lamp, "", glow_sum, "B")
    glow_color = node(unreal.MaterialExpressionMultiply, -250, -200)
    link(glow_sum, "", glow_color, "A")
    link(accent, "", glow_color, "B")
    self_light = node(unreal.MaterialExpressionScalarParameter, -400, -50, parameter_name="SelfLight", default_value=0.08)
    self_glow = node(unreal.MaterialExpressionMultiply, -250, -50)
    link(base, "", self_glow, "A")
    link(self_light, "", self_glow, "B")
    emissive = node(unreal.MaterialExpressionAdd, -100, -150)
    link(glow_color, "", emissive, "A")
    link(self_glow, "", emissive, "B")

    rough_scale = node(unreal.MaterialExpressionScalarParameter, -900, 250, parameter_name="RoughnessScale", default_value=1.0)
    roughness = node(unreal.MaterialExpressionMultiply, -700, 200)
    link(orm, "G", roughness, "A")
    link(rough_scale, "", roughness, "B")

    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("built %s (%d expressions)" % (path, mel.get_num_material_expressions(mat)))
    return mat


def trim_instances(master, textures):
    instances = {}
    for name, (sheet, shade, accent) in INSTANCES.items():
        path = "%s/MI_SB_%s" % (MATERIALS, name)
        mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
            "MI_SB_" + name, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        color, normal, orm, mask, lights = SHEETS[sheet]
        for param, texture in (("BaseColorTex", color), ("NormalTex", normal), ("ORMTex", orm), ("MaskTex", mask), ("EmissiveTex", lights)):
            if texture:
                mel.set_material_instance_texture_parameter_value(mi, param, textures[texture])
        mel.set_material_instance_scalar_parameter_value(mi, "Shade", shade)
        mel.set_material_instance_scalar_parameter_value(mi, "AccentAmount", accent)
        mel.set_material_instance_scalar_parameter_value(mi, "LightGlow", 8.0 if lights else 0.0)
        eal.save_asset(path)
        instances["M_" + name] = mi
    log("%d trim instances" % len(instances))
    return instances


def expected_sizes():
    """name -> (size x, y, z) from the Blender report line '...: kind, tris N, X a..b Y c..d Z e..f, slots ...'."""
    sizes = {}
    with open(os.path.join(EXPORT, "scifikit_report.txt")) as f:
        for line in f:
            match = re.match(r"(\S+): .*X (-?[\d.]+)\.\.(-?[\d.]+) Y (-?[\d.]+)\.\.(-?[\d.]+) Z (-?[\d.]+)\.\.(-?[\d.]+)", line)
            if match:
                v = [float(x) for x in match.groups()[1:]]
                sizes[match.group(1)] = (v[1] - v[0], v[3] - v[2], v[5] - v[4])
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
    # As import_map_art.py: normals from the winding (export_fbx's mirror leaves the exported ones pointing inward).
    data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS
    data.import_uniform_scale = 1.0
    task.options = ui
    tools.import_asset_tasks([task])
    return name, unreal.load_asset("%s/%s" % (DEST, name))


def main():
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    textures = import_textures()
    master = trim_material(textures)
    slots_to_materials = trim_instances(master, textures)
    for slot, palette in (("M_KitLight", "MI_SB_GlowAmber"), ("M_KitDark", "MI_SB_Gunmetal")):
        material = unreal.load_asset("%s/%s" % (MATERIALS, palette))
        if not material:
            raise RuntimeError("Missing %s/%s: run Tools/ImportArt.bat first" % (MATERIALS, palette))
        slots_to_materials[slot] = material
    sizes = expected_sizes()
    problems = []
    fbxs = sorted(glob.glob(os.path.join(EXPORT, "*.fbx")))
    for fbx in fbxs:
        name, mesh = import_fbx(fbx)
        if not isinstance(mesh, unreal.StaticMesh):
            problems.append("import failed: " + name)
            continue
        slots = []
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            slot_name = str(slot.material_slot_name)
            material = slots_to_materials.get(slot_name)
            if material:
                mesh.set_material(index, material)
            slots.append(slot_name + ("" if material else " (unmatched)"))
            if not material:
                problems.append("%s: unmatched slot %s" % (name, slot_name))
        eal.save_asset("%s/%s" % (DEST, name))
        box = mesh.get_bounding_box()
        size = (box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z)
        log("%s: tris %d, bounds (%.1f, %.1f, %.1f) .. (%.1f, %.1f, %.1f), slots %s" % (
            name, mesh.get_num_triangles(0), box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z, ", ".join(slots)))
        want = sizes.get(name)
        if want and any(abs(a - b) > 2.0 for a, b in zip(size, want)):
            problems.append("%s: size %s, expected %s (axes or scale)" % (name, size, want))
    if len(fbxs) != len(sizes):
        problems.append("%d FBX files but %d in scifikit_report.txt" % (len(fbxs), len(sizes)))
    log("RESULT " + ("OK, %d meshes" % len(fbxs) if not problems else "FAILED: " + "; ".join(problems)))
    out = os.path.join(PROJECT, "Saved", "Logs", "SciFiKitImport.txt")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")
    if problems:
        raise RuntimeError("; ".join(problems))


main()
