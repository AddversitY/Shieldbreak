"""Imports Shieldbreak's scripted art (Art/Export/**.fbx, made by Art/blender/*.py) into /Game/Shieldbreak/Art (headless).

- One master material, M_SB_Surface (BaseColor, Metallic, Roughness, EmissiveColor, EmissiveStrength), and one
  instance per palette entry (MI_SB_<Name>, from Art/Export/palette.json).
- Every FBX becomes a static mesh in the folder named after its subfolder (Weapons, Armor, ...). Material slots are
  named M_<Palette>, and each gets MI_SB_<Palette>. SOCKET_* empties become sockets.
- Art/Export/Textures/*.png (made by Art/textures/make_textures.py) become textures in /Game/Shieldbreak/Art/Textures, and
  the world material M_SB_World (world-aligned triplanar detail texture tinted by the map palette; AShieldbreakArena
  sets "Tint" per piece) gets one instance per surface kind (MI_SB_World_<Kind>, see WORLD_STYLES).
- Art/Export/Audio/*.wav (made by Art/audio/make_music.py) become sound waves in /Game/Shieldbreak/Art/Audio; MUS_* loop.
- Writes Saved/Logs/ImportArt.txt with each mesh's triangles, bounds and sockets, so the cloud can check the axes.
- Weapons and armor get two reduced LODs (LOD_SETTINGS): soldiers far away cost a quarter of the triangles.
- SB_IMPORT_FOLDERS=Weapons,Armor (environment variable) limits the FBX import to those Art/Export subfolders, so a
  character or weapon pass doesn't reimport (and touch) the map kits.

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
AUDIO = ROOT + "/Audio"
# Surface kinds for the map pieces: (detail texture, tile size in cm, metallic, roughness, detail strength).
# Detail strength fades the texture toward flat grey (0 = flat palette colour, 1 = full pattern).
WORLD_STYLES = {
    "Panel": ("T_SB_Panel", 600.0, 0.35, 0.4, 0.7),
    "Tile": ("T_SB_Tile", 600.0, 0.0, 0.6, 0.55),
    "Stone": ("T_SB_Stone", 700.0, 0.0, 0.85, 0.85),
    "Grass": ("T_SB_Grass", 300.0, 0.0, 0.95, 0.6),
}
# Art/Export subfolder -> [(fraction of LOD0 triangles, screen size where the LOD starts)], LOD0 first. Screen size is
# the bounding sphere's share of the screen: a 30 cm armor piece is about 0.06 at 6 m and 0.025 at 15 m with the
# default field of view; a 90 cm rifle about 0.1 at 10 m. The first-person gun fills the screen and stays at LOD0.
# Palette entries owned by the character art: a narrow (SB_IMPORT_FOLDERS) import still rewrites their values,
# so tuning them in palette.json reaches the game without a full import. Map kits never use them.
CHARACTER_PALETTE = ("ArmorMetal", "WeaponMetal", "Polymer")
LOD_SETTINGS = {
    "Weapons": [(1.0, 1.0), (0.5, 0.1), (0.25, 0.04)],
    "Armor": [(1.0, 1.0), (0.5, 0.06), (0.25, 0.025)],
}
# Character and weapon surfaces (M_SB_Character, MI_SBC_<Palette>): wear from T_SB_Wear projected in the mesh's own
# space, so it rides with the gun and the suit. Per palette key: (colour variation, roughness variation, scratches).
# Keys not listed get DEFAULT_WEAR; glows, lenses and visors stay clean.
CHARACTER_WEAR_TEXTURE = "T_SB_Wear"
CHARACTER_WEAR_TILE = 24.0
DEFAULT_WEAR = (0.25, 0.35, 0.5)
CHARACTER_WEAR = {
    # Task (7): 0.3/0.8 read as camo mottling on the BR at first-person distance; the sidearm's polymer looked right.
    # Task (8): still mottled at roughness variation 0.3 (the reflections blotch), so that comes down too.
    "WeaponMetal": (0.06, 0.12, 0.25),
    "ArmorMetal": (0.08, 0.15, 0.3),
    "Team": (0.25, 0.35, 0.55),
    "Steel": (0.15, 0.3, 0.4),
    "Polymer": (0.35, 0.45, 0.15),
    # White armor panels on the guns: a faint satin variation and light edge scratches, so the pale shells read as
    # painted ceramic under light rather than flat white (task 19).
    "Ceramic": (0.1, 0.22, 0.35),
    "Undersuit": (0.3, 0.4, 0.0),
    "GlowCyan": (0, 0, 0), "GlowAmber": (0, 0, 0), "TeamGlow": (0, 0, 0), "Lens": (0, 0, 0), "Visor": (0, 0, 0.05),
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
        mat = unreal.load_asset(path)
        if not mel.has_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH):
            # Polymer now covers the mannequin and FP arms as well as static props.
            # Without this permutation the runtime silently draws Default Material.
            mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
            mel.recompile_material(mat)
            eal.save_asset(path)
            log("enabled SkeletalMesh usage on " + path)
        return mat
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
    mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("created " + path)
    return mat


def palette_instances(master, reuse_existing=False):
    with open(os.path.join(EXPORT, "palette.json")) as f:
        palette = json.load(f)
    instances = {}
    for name, (rgb, metallic, roughness, emissive) in palette.items():
        path = "%s/MI_SB_%s" % (MATERIALS, name)
        if eal.does_asset_exist(path):
            mi = unreal.load_asset(path)
            if reuse_existing and name not in CHARACTER_PALETTE:
                instances["M_" + name] = mi
                continue
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
        if name in CHARACTER_PALETTE:
            log("%s: BaseColor %s, Metallic %.3f, Roughness %.3f, EmissiveStrength %.3f" %
                (path, tuple(rgb), metallic, roughness, emissive))
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


def import_audio():
    count = 0
    for wav in sorted(glob.glob(os.path.join(EXPORT, "Audio", "*.wav"))):
        name = os.path.splitext(os.path.basename(wav))[0]
        task = unreal.AssetImportTask()
        task.filename = wav
        task.destination_path = AUDIO
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        tools.import_asset_tasks([task])
        path = "%s/%s" % (AUDIO, name)
        sound = unreal.load_asset(path)
        if not sound:
            log("FAILED " + wav)
            continue
        if name.startswith("MUS_"):
            sound.set_editor_property("looping", True)
        eal.save_asset(path)
        log("%s: %.1f s%s" % (path, sound.get_editor_property("duration"), ", looping" if name.startswith("MUS_") else ""))
        count += 1
    log("%d sounds" % count)


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

    # Fade the pattern toward mid-grey by DetailStrength, so the palette colour reads first.
    strength = node(unreal.MaterialExpressionScalarParameter, -350, 250, parameter_name="DetailStrength", default_value=1.0)
    grey = node(unreal.MaterialExpressionConstant, -350, 350, r=0.5)
    faded = node(unreal.MaterialExpressionLinearInterpolate, -250, 150)
    link(grey, "", faded, "A")
    link(blended, "", faded, "B")
    link(strength, "", faded, "Alpha")
    blended = faded

    tint = node(unreal.MaterialExpressionVectorParameter, -500, -500, parameter_name="Tint",
                default_value=unreal.LinearColor(0.25, 0.25, 0.25, 1))
    tint2 = node(unreal.MaterialExpressionMultiply, -350, -450, const_b=2.0)
    link(tint, "", tint2, "A")
    base = node(unreal.MaterialExpressionMultiply, -200, -300)
    link(blended, "", base, "A")
    link(tint2, "", base, "B")
    metallic = node(unreal.MaterialExpressionScalarParameter, -200, 0, parameter_name="Metallic", default_value=0.0)
    roughness = node(unreal.MaterialExpressionScalarParameter, -200, 100, parameter_name="Roughness", default_value=0.7)
    # SelfLight: a little of the surface colour as emissive, for far scenery that Lumen and shadows leave near black
    # (the mountain ring). 0 on map pieces.
    self_light = node(unreal.MaterialExpressionScalarParameter, -200, -150, parameter_name="SelfLight", default_value=0.0)
    emissive = node(unreal.MaterialExpressionMultiply, -100, -200)
    link(base, "", emissive, "A")
    link(self_light, "", emissive, "B")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("built %s (%d expressions)" % (path, mel.get_num_material_expressions(mat)))
    return mat


def character_material(wear_texture):
    """M_SB_Character: the palette surface (same parameters as M_SB_Surface, so team MIDs keep working) plus wear:
    colour and roughness variation and bright, less rough scratches, from a triplanar sample of T_SB_Wear in
    pre-skinned local space (no swimming on the skinned suit). Rebuilt on every run. Three samples of one 512 px
    texture, used only by soldiers and guns."""
    path = MATERIALS + "/M_SB_Character"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_SB_Character", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)

    def node(cls, x, y, **props):
        n = mel.create_material_expression(mat, cls, x, y)
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def link(a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            log("M_SB_Character: could not connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))

    def scalar(name, default, x, y):
        return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=default)

    # Pre-skinned position and normal are vertex-shader only, so both reach the pixel shader through an interpolator.
    pre_skinned = node(unreal.MaterialExpressionPreSkinnedPosition, -2000, 0)
    position = node(unreal.MaterialExpressionVertexInterpolator, -1800, 0)
    link(pre_skinned, "", position, "VS")
    tile = scalar("WearTile", CHARACTER_WEAR_TILE, -1800, 150)
    uvw = node(unreal.MaterialExpressionDivide, -1600, 0)
    link(position, "", uvw, "A")
    link(tile, "", uvw, "B")
    samples = []
    for i, (r, g, b) in enumerate(((False, True, True), (True, False, True), (True, True, False))):
        mask = node(unreal.MaterialExpressionComponentMask, -1400, -300 + 300 * i, r=r, g=g, b=b, a=False)
        link(uvw, "", mask, "")
        sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -1200, -300 + 300 * i,
                      parameter_name="Wear", texture=wear_texture)
        link(mask, "", sample, "UVs")
        samples.append(sample)
    pre_skinned_normal = node(unreal.MaterialExpressionPreSkinnedNormal, -1800, 700)
    normal = node(unreal.MaterialExpressionVertexInterpolator, -1600, 700)
    link(pre_skinned_normal, "", normal, "VS")
    absolute = node(unreal.MaterialExpressionAbs, -1450, 700)
    link(normal, "", absolute, "")
    power = node(unreal.MaterialExpressionPower, -1300, 700, const_exponent=4.0)
    link(absolute, "", power, "Base")
    ones = node(unreal.MaterialExpressionConstant3Vector, -1300, 850, constant=unreal.LinearColor(1, 1, 1, 1))
    total = node(unreal.MaterialExpressionDotProduct, -1150, 800)
    link(power, "", total, "A")
    link(ones, "", total, "B")
    weights = node(unreal.MaterialExpressionDivide, -1000, 700)
    link(power, "", weights, "A")
    link(total, "", weights, "B")
    wear = None
    for i, (sample, channel) in enumerate(zip(samples, ("R", "G", "B"))):
        mask = node(unreal.MaterialExpressionComponentMask, -850, 600 + 120 * i,
                    r=channel == "R", g=channel == "G", b=channel == "B", a=False)
        link(weights, "", mask, "")
        weighted = node(unreal.MaterialExpressionMultiply, -700, -300 + 300 * i)
        link(sample, "RGB", weighted, "A")
        link(mask, "", weighted, "B")
        if wear is None:
            wear = weighted
        else:
            add = node(unreal.MaterialExpressionAdd, -550, -150 + 200 * i)
            link(wear, "", add, "A")
            link(weighted, "", add, "B")
            wear = add

    def channel(name, y):
        m = node(unreal.MaterialExpressionComponentMask, -400, y, r=name == "R", g=name == "G", b=name == "B", a=False)
        link(wear, "", m, "")
        return m

    colour_var, rough_var, scratch = channel("R", -400), channel("G", 0), channel("B", 400)
    half = node(unreal.MaterialExpressionConstant, -400, 200, r=0.5)

    # BaseColor * (1 + (R - 0.5) * 2 * ColourVariation), then lerp toward a bright bare-metal grey by the scratches.
    base = node(unreal.MaterialExpressionVectorParameter, -400, -600, parameter_name="BaseColor",
                default_value=unreal.LinearColor(0.2, 0.2, 0.2, 1))
    colour_amount = scalar("ColourVariation", DEFAULT_WEAR[0], -250, -300)
    centred = node(unreal.MaterialExpressionSubtract, -250, -420)
    link(colour_var, "", centred, "A")
    link(half, "", centred, "B")
    scaled = node(unreal.MaterialExpressionMultiply, -100, -420)
    link(centred, "", scaled, "A")
    link(colour_amount, "", scaled, "B")
    factor = node(unreal.MaterialExpressionMultiply, 50, -420, const_b=2.0)
    link(scaled, "", factor, "A")
    one_plus = node(unreal.MaterialExpressionAdd, 200, -420, const_b=1.0)
    link(factor, "", one_plus, "A")
    varied = node(unreal.MaterialExpressionMultiply, 350, -500)
    link(base, "", varied, "A")
    link(one_plus, "", varied, "B")
    scratch_amount = scalar("Scratches", DEFAULT_WEAR[2], -250, 500)
    scratch_mask = node(unreal.MaterialExpressionMultiply, -100, 420)
    link(scratch, "", scratch_mask, "A")
    link(scratch_amount, "", scratch_mask, "B")
    bare = node(unreal.MaterialExpressionConstant3Vector, 350, -300, constant=unreal.LinearColor(0.42, 0.43, 0.45, 1))
    colour = node(unreal.MaterialExpressionLinearInterpolate, 500, -400)
    link(varied, "", colour, "A")
    link(bare, "", colour, "B")
    link(scratch_mask, "", colour, "Alpha")

    # Roughness + (G - 0.5) * RoughnessVariation, minus a little on scratches (polished edges); Metallic up on them.
    roughness = scalar("Roughness", 0.5, -250, 100)
    rough_amount = scalar("RoughnessVariation", DEFAULT_WEAR[1], -250, 200)
    rough_centred = node(unreal.MaterialExpressionSubtract, -100, 50)
    link(rough_var, "", rough_centred, "A")
    link(half, "", rough_centred, "B")
    rough_scaled = node(unreal.MaterialExpressionMultiply, 50, 50)
    link(rough_centred, "", rough_scaled, "A")
    link(rough_amount, "", rough_scaled, "B")
    rough_sum = node(unreal.MaterialExpressionAdd, 200, 100)
    link(roughness, "", rough_sum, "A")
    link(rough_scaled, "", rough_sum, "B")
    polish = node(unreal.MaterialExpressionMultiply, 200, 300, const_b=0.25)
    link(scratch_mask, "", polish, "A")
    rough_final = node(unreal.MaterialExpressionSubtract, 350, 150)
    link(rough_sum, "", rough_final, "A")
    link(polish, "", rough_final, "B")
    rough_clamped = node(unreal.MaterialExpressionClamp, 500, 150)
    link(rough_final, "", rough_clamped, "")
    metallic = scalar("Metallic", 0.5, -250, 300)
    metal_up = node(unreal.MaterialExpressionMultiply, 200, 400, const_b=0.6)
    link(scratch_mask, "", metal_up, "A")
    metallic_sum = node(unreal.MaterialExpressionAdd, 350, 350)
    link(metallic, "", metallic_sum, "A")
    link(metal_up, "", metallic_sum, "B")
    metallic_clamped = node(unreal.MaterialExpressionClamp, 500, 350)
    link(metallic_sum, "", metallic_clamped, "")

    glow = node(unreal.MaterialExpressionVectorParameter, 200, 600, parameter_name="EmissiveColor",
                default_value=unreal.LinearColor(0, 0, 0, 1))
    strength = scalar("EmissiveStrength", 0.0, 200, 750)
    emissive = node(unreal.MaterialExpressionMultiply, 400, 650)
    link(glow, "", emissive, "A")
    link(strength, "", emissive, "B")

    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metallic_clamped, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(rough_clamped, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("built %s (%d expressions)" % (path, mel.get_num_material_expressions(mat)))
    return mat


def character_instances(master):
    """MI_SBC_<Palette>: every palette entry on M_SB_Character, with its CHARACTER_WEAR. Rewritten on every run."""
    with open(os.path.join(EXPORT, "palette.json")) as f:
        palette = json.load(f)
    instances = {}
    for name, (rgb, metallic, roughness, emissive) in palette.items():
        path = "%s/MI_SBC_%s" % (MATERIALS, name)
        if eal.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = tools.create_asset("MI_SBC_" + name, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        colour_var, rough_var, scratches = CHARACTER_WEAR.get(name, DEFAULT_WEAR)
        mel.set_material_instance_vector_parameter_value(mi, "BaseColor", color(rgb))
        mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
        mel.set_material_instance_vector_parameter_value(mi, "EmissiveColor", color(rgb))
        mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", emissive)
        mel.set_material_instance_scalar_parameter_value(mi, "ColourVariation", colour_var)
        mel.set_material_instance_scalar_parameter_value(mi, "RoughnessVariation", rough_var)
        mel.set_material_instance_scalar_parameter_value(mi, "Scratches", scratches)
        eal.save_asset(path)
        instances["M_" + name] = mi
    log("%d character instances on M_SB_Character" % len(instances))
    return instances


# ---- Textured weapons (Art/blender/bake_weapons.py) -----------------------------------------------------------------
# Each baked gun has T_SB_<Gun>_BC (sRGB colour, emissive mask in alpha), _ORM (AO, roughness, metallic) and _N
# (DirectX tangent normal) in Art/Export/Textures/Weapons. Its FBX has one slot, M_SBW_<Gun>, bound to MI_SBW_<Gun>.
WEAPON_TEXTURES = os.path.join(EXPORT, "Textures", "Weapons")
WEAPON_TEXTURE_FOLDER = TEXTURES + "/Weapons"


def import_weapon_textures():
    """Imports the baked weapon maps with the right colour space and compression. Returns {gun: {suffix: texture}}."""
    sets = {}
    for png in sorted(glob.glob(os.path.join(WEAPON_TEXTURES, "T_SB_*_*.png"))):
        name = os.path.splitext(os.path.basename(png))[0]
        gun, suffix = name[len("T_SB_"):].rsplit("_", 1)
        task = unreal.AssetImportTask()
        task.filename = png
        task.destination_path = WEAPON_TEXTURE_FOLDER
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        tools.import_asset_tasks([task])
        texture = unreal.load_asset("%s/%s" % (WEAPON_TEXTURE_FOLDER, name))
        if not texture:
            log("FAILED " + png)
            continue
        if suffix == "N":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("srgb", False)
        elif suffix == "ORM":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            texture.set_editor_property("srgb", False)
        else:
            texture.set_editor_property("srgb", True)
        group = getattr(unreal.TextureGroup, "TEXTUREGROUP_WEAPON", None)
        if group is not None:
            texture.set_editor_property("lod_group", group)
        eal.save_asset(texture.get_path_name())
        sets.setdefault(gun, {})[suffix] = texture
        log("%s: %dx%d %s" % (name, texture.blueprint_get_size_x(), texture.blueprint_get_size_y(), suffix))
    return {gun: maps for gun, maps in sets.items() if all(s in maps for s in ("BC", "ORM", "N"))}


def weapon_pbr_material(defaults):
    """M_SB_WeaponPBR: baked BC/ORM/N maps. BaseColor = BC * BaseTint; Roughness = ORM.g * RoughnessScale + RoughnessAdd;
    Metallic = ORM.b; AO = lerp(1, ORM.r, AOStrength); Emissive = BC.rgb * BC.a * EmissiveStrength. Rebuilt every run."""
    path = MATERIALS + "/M_SB_WeaponPBR"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_SB_WeaponPBR", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y, **props):
        n = mel.create_material_expression(mat, cls, x, y)
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def link(a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            log("M_SB_WeaponPBR: could not connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))

    def scalar(name, default, x, y):
        return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=default)

    bc = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, -300, parameter_name="BC", texture=defaults["BC"],
              sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    orm = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, 100, parameter_name="ORM", texture=defaults["ORM"],
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    nrm = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, 500, parameter_name="N", texture=defaults["N"],
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    tint = node(unreal.MaterialExpressionVectorParameter, -600, -450, parameter_name="BaseTint",
                default_value=unreal.LinearColor(1, 1, 1, 1))
    colour = node(unreal.MaterialExpressionMultiply, -400, -350)
    link(bc, "RGB", colour, "A")
    link(tint, "", colour, "B")
    rough_scale = scalar("RoughnessScale", 1.0, -600, 150)
    rough_add = scalar("RoughnessAdd", 0.0, -600, 250)
    rough_mul = node(unreal.MaterialExpressionMultiply, -400, 120)
    link(orm, "G", rough_mul, "A")
    link(rough_scale, "", rough_mul, "B")
    rough_sum = node(unreal.MaterialExpressionAdd, -250, 150)
    link(rough_mul, "", rough_sum, "A")
    link(rough_add, "", rough_sum, "B")
    rough = node(unreal.MaterialExpressionClamp, -100, 150)
    link(rough_sum, "", rough, "")
    ao_strength = scalar("AOStrength", 1.0, -600, 350)
    one = node(unreal.MaterialExpressionConstant, -600, 30, r=1.0)
    ao = node(unreal.MaterialExpressionLinearInterpolate, -250, 330)
    link(one, "", ao, "A")
    link(orm, "R", ao, "B")
    link(ao_strength, "", ao, "Alpha")
    em_strength = scalar("EmissiveStrength", 4.0, -600, -150)
    em_mask = node(unreal.MaterialExpressionMultiply, -400, -150)
    link(bc, "A", em_mask, "A")
    link(em_strength, "", em_mask, "B")
    emissive = node(unreal.MaterialExpressionMultiply, -250, -200)
    link(bc, "RGB", emissive, "A")
    link(em_mask, "", emissive, "B")
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("built %s (%d expressions)" % (path, mel.get_num_material_expressions(mat)))
    return mat


def weapon_pbr_instances():
    """MI_SBW_<Gun> for every complete baked texture set, keyed by the FBX slot name M_SBW_<Gun>."""
    sets = import_weapon_textures()
    if not sets:
        return {}
    master = weapon_pbr_material(next(iter(sets.values())))
    instances = {}
    for gun, maps in sets.items():
        path = "%s/MI_SBW_%s" % (MATERIALS, gun)
        if eal.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = tools.create_asset("MI_SBW_" + gun, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        for suffix in ("BC", "ORM", "N"):
            mel.set_material_instance_texture_parameter_value(mi, suffix, maps[suffix])
        eal.save_asset(path)
        instances["M_SBW_" + gun] = mi
        log("%s on M_SB_WeaponPBR" % path)
    return instances


def import_texture(name):
    """One texture from Art/Export/Textures (a narrow import still needs the character wear texture)."""
    png = os.path.join(EXPORT, "Textures", name + ".png")
    if not os.path.exists(png):
        log("MISSING " + png)
        return None
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = TEXTURES
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    return unreal.load_asset("%s/%s" % (TEXTURES, name))


def world_instances(master, textures):
    for kind, (texture_name, tile_size, metallic, roughness, detail) in WORLD_STYLES.items():
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
        mel.set_material_instance_scalar_parameter_value(mi, "DetailStrength", detail)
        eal.save_asset(path)
        log("%s: %s, tile %.0f cm, detail %.2f" % (path, texture_name, tile_size, detail))


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
    if os.path.basename(destination) in LOD_SETTINGS:
        # Character FBX files own their named palette slots. Reimports otherwise
        # retain old slot names, silently binding renamed surfaces to old materials.
        ui.set_editor_property("reset_to_fbx_on_material_conflict", True)
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = ui.static_mesh_import_data
    data.combine_meshes = True
    data.auto_generate_collision = False
    data.generate_lightmap_u_vs = False
    # Normals from the triangle winding, as the map importers do (import_map_art.py): export_fbx's mirror leaves the
    # exported normals pointing inward, so the guns and armor were lit from behind. Their lit sides read black and
    # only edges caught light, whatever the palette (desktop shots after3-after5, 2026-11-05).
    data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS
    data.import_uniform_scale = 1.0
    task.options = ui
    tools.import_asset_tasks([task])
    path = "%s/%s" % (destination, task.destination_name)
    return unreal.load_asset(path), path


def set_lods(mesh, folder):
    """Generates reduced LODs for meshes in LOD_SETTINGS folders. Returns a short note for the log."""
    settings = LOD_SETTINGS.get(folder)
    if not settings:
        return ""
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("LOD generation needs editor mode: use Tools/ImportCharacterArt.bat")
    options = unreal.StaticMeshReductionOptions()
    options.set_editor_property("auto_compute_lod_screen_size", False)
    reductions = []
    for fraction, screen in settings:
        entry = unreal.StaticMeshReductionSettings()
        entry.set_editor_property("percent_triangles", fraction)
        entry.set_editor_property("screen_size", screen)
        reductions.append(entry)
    options.set_editor_property("reduction_settings", reductions)
    count = subsystem.set_lods(mesh, options)
    if count != len(settings):
        raise RuntimeError("Expected %d LODs for %s, got %d" % (len(settings), mesh.get_name(), count))
    tris = [mesh.get_num_triangles(i) for i in range(max(count, 0))]
    return ", LODs %d (tris %s)" % (count, "/".join(str(t) for t in tris))


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
    only = [f.strip() for f in os.environ.get("SB_IMPORT_FOLDERS", "").split(",") if f.strip()]
    master = master_material()
    instances = palette_instances(master, reuse_existing=bool(only))
    if only:
        # A narrow pass: meshes only. Textures, the world material and audio stay as they are.
        log("FBX folders: " + ", ".join(only))
    else:
        textures = import_textures()
        if textures:
            world = world_material(textures.get("T_SB_Panel") or next(iter(textures.values())))
            world_instances(world, textures)
        import_audio()
    character = {}
    if not only or any(folder in LOD_SETTINGS for folder in only):
        wear = import_texture(CHARACTER_WEAR_TEXTURE)
        if wear:
            character = character_instances(character_material(wear))
        character = dict(character)
        character.update(weapon_pbr_instances())
    for fbx in sorted(glob.glob(os.path.join(EXPORT, "*", "*.fbx"))):
        folder = os.path.basename(os.path.dirname(fbx))
        if only and folder not in only:
            continue
        mesh, path = import_fbx(fbx, "%s/%s" % (ROOT, folder))
        if not mesh:
            log("FAILED " + fbx)
            continue
        materials = mesh.get_editor_property("static_materials")
        if folder in LOD_SETTINGS:
            # UE can preserve the display slot name even when its imported name
            # has changed. Scripted character assets use the FBX-authored names.
            for index, slot in enumerate(materials):
                imported_name = str(slot.get_editor_property("imported_material_slot_name"))
                if imported_name and imported_name != "None":
                    slot.set_editor_property("material_slot_name", unreal.Name(imported_name))
                    materials[index] = slot
            mesh.set_editor_property("static_materials", materials)
            materials = mesh.get_editor_property("static_materials")
        missing = []
        # Soldiers and guns wear the character material (with wear); everything else the plain palette.
        bound = character if folder in LOD_SETTINGS and character else instances
        for index, slot in enumerate(materials):
            name = str(slot.material_slot_name)
            # The FBX importer can suffix slot names (M_Gunmetal_001, M_Gunmetal_Skin); match on the palette prefix.
            key = next((k for k in bound if name == k or name.startswith(k + "_")), None)
            if key:
                mesh.set_material(index, bound[key])
            else:
                missing.append(name)
        lod_note = set_lods(mesh, folder)
        eal.save_asset(path)
        bindings = ", ".join("%s -> %s" % (slot.material_slot_name,
                             slot.material_interface.get_path_name() if slot.material_interface else "None")
                             for slot in mesh.get_editor_property("static_materials"))
        log("%s: %s%s%s, slots [%s]" % (path, describe(mesh), lod_note,
            (", unmatched slots " + ", ".join(missing)) if missing else "", bindings))
    out = os.path.join(PROJECT, "Saved", "Logs", "ImportArt.txt")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")


main()
