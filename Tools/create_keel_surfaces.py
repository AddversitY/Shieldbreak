"""Create Keel's scanned hull materials: one world-projected master and Plate / Deck / Frame instances.

Each instance samples one packed masks texture (Art/textures/make_keel_surfaces.py: R albedo detail, G roughness,
B height) three times (triplanar), tints the detail with its own colour and bends the normal from the height with a
surface gradient (no normal-map samples). Shared by every hull piece, so the whole ship costs one master's shaders.
Run with Tools/MakeKeelSurfaces.bat. The game uses the instances on Keel's hull kit unless -SBLegacyKeelHull is set.
"""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST = '/Game/Shieldbreak/Art/Keel'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

# name: (tint, metallic, roughness min, roughness max, tile size cm, relief depth, detail contrast)
SURFACES = {
    'Plate': ((.15, .16, .18), .50, .38, .80, 300.0, .0014, 0.7),
    'Deck': ((.075, .078, .085), .55, .45, .85, 160.0, .0022, 0.9),
    'Frame': ((.065, .07, .08), .55, .35, .75, 140.0, .0012, 0.8),
    # Worn hazard-orange paint on the bridges' wall plates (the brief's fourth hull material), on the plate scan.
    'Hazard': ((.40, .15, .03), .15, .45, .85, 300.0, .0014, 1.0),
}
MASKS = {'Plate': 'Plate', 'Deck': 'Deck', 'Frame': 'Frame', 'Hazard': 'Plate'}


def import_masks(surface):
    name = 'T_SB_Keel%sMasks' % surface
    task = unreal.AssetImportTask()
    task.filename = str(ROOT / ('Art/Export/Textures/%s.png' % name))
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset('%s/%s' % (DEST, name))
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError('%s was not imported' % name)
    texture.set_editor_property('srgb', False)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_WRAP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_WRAP)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    EAL.save_loaded_asset(texture)
    return texture


def build_master(default_texture):
    path = DEST + '/M_SB_KeelHull'
    material = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        'M_SB_KeelHull', DEST, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(material)
    material.set_editor_property('tangent_space_normal', False)

    def node(kind, x, y, **props):
        result = MEL.create_material_expression(material, kind, x, y)
        for key, value in props.items():
            result.set_editor_property(key, value)
        return result

    def link(a, b, pin, output=''):
        if not MEL.connect_material_expressions(a, output, b, pin):
            raise RuntimeError('Cannot connect %s to %s.%s' % (a.get_name(), b.get_name(), pin))

    def scalar(name, value, x, y):
        return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)

    position = node(unreal.MaterialExpressionWorldPosition, -1600, 0)
    normal = node(unreal.MaterialExpressionVertexNormalWS, -1600, 600)
    uvw = node(unreal.MaterialExpressionDivide, -1400, 0)
    link(position, uvw, 'A'); link(scalar('TileSize', 200.0, -1600, 150), uvw, 'B')
    absolute = node(unreal.MaterialExpressionAbs, -1400, 600); link(normal, absolute, '')
    power = node(unreal.MaterialExpressionPower, -1200, 600, const_exponent=8.0); link(absolute, power, 'Base')
    ones = node(unreal.MaterialExpressionConstant3Vector, -1200, 760, constant=unreal.LinearColor(1, 1, 1, 1))
    total = node(unreal.MaterialExpressionDotProduct, -1000, 650); link(power, total, 'A'); link(ones, total, 'B')
    weights = node(unreal.MaterialExpressionDivide, -800, 600); link(power, weights, 'A'); link(total, weights, 'B')
    packed = None
    for index, channels in enumerate(((False, True, True), (True, False, True), (True, True, False))):
        uv = node(unreal.MaterialExpressionComponentMask, -1200, -450 + index * 280,
                  r=channels[0], g=channels[1], b=channels[2], a=False); link(uvw, uv, '')
        sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -1000, -450 + index * 280,
                      parameter_name='SurfaceMasks', texture=default_texture,
                      sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS); link(uv, sample, 'UVs')
        mask = node(unreal.MaterialExpressionComponentMask, -600, 500 + index * 120,
                    r=index == 0, g=index == 1, b=index == 2, a=False); link(weights, mask, '')
        weighted = node(unreal.MaterialExpressionMultiply, -600, -450 + index * 280)
        link(sample, weighted, 'A', 'RGB'); link(mask, weighted, 'B')
        if packed is None:
            packed = weighted
        else:
            added = node(unreal.MaterialExpressionAdd, -400, -350 + index * 240)
            link(packed, added, 'A'); link(weighted, added, 'B'); packed = added

    # Base colour: the tint times the scan's wear and scratches (R around 0.5), never its paint colour.
    tint = node(unreal.MaterialExpressionVectorParameter, -200, -700,
                parameter_name='BaseColor', default_value=unreal.LinearColor(.16, .17, .19, 1))
    detail = node(unreal.MaterialExpressionComponentMask, -200, -420, r=True, g=False, b=False, a=False)
    link(packed, detail, '')
    centred = node(unreal.MaterialExpressionSubtract, 0, -420, const_b=.5); link(detail, centred, 'A')
    contrast = node(unreal.MaterialExpressionMultiply, 180, -420); link(centred, contrast, 'A')
    link(scalar('DetailContrast', 1.0, 0, -300), contrast, 'B')
    bias = node(unreal.MaterialExpressionAdd, 360, -420, const_b=1.0); link(contrast, bias, 'A')
    base = node(unreal.MaterialExpressionMultiply, 540, -600); link(tint, base, 'A'); link(bias, base, 'B')
    MEL.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)

    rough_mask = node(unreal.MaterialExpressionComponentMask, -200, 50, r=False, g=True, b=False, a=False)
    link(packed, rough_mask, '')
    rough = node(unreal.MaterialExpressionLinearInterpolate, 360, -80)
    link(scalar('RoughnessMin', .32, 0, -120), rough, 'A'); link(scalar('RoughnessMax', .78, 0, -40), rough, 'B')
    link(rough_mask, rough, 'Alpha')
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(scalar('Metallic', .55, 360, -240), '', unreal.MaterialProperty.MP_METALLIC)

    height = node(unreal.MaterialExpressionComponentMask, -200, 260, r=False, g=False, b=True, a=False)
    link(packed, height, '')
    custom_inputs = []
    for name in ('P', 'N', 'H', 'Depth'):
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property('input_name', name)
        custom_inputs.append(custom_input)
    bump = node(unreal.MaterialExpressionCustom, 360, 300,
                output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, inputs=custom_inputs,
                description='Filtered surface-gradient relief (world space)',
                code='float3 dx=ddx(P),dy=ddy(P); float3 a=cross(dy,N),b=cross(N,dx); '
                     'float det=dot(dx,a); float3 grad=sign(det)*(ddx(H)*a+ddy(H)*b); '
                     'return normalize(max(abs(det),1e-8)*N-Depth*grad);')
    link(position, bump, 'P'); link(normal, bump, 'N'); link(height, bump, 'H')
    link(scalar('ReliefDepth', .0016, 0, 400), bump, 'Depth')
    MEL.connect_material_property(bump, '', unreal.MaterialProperty.MP_NORMAL)
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)
    return material


def main():
    textures = {surface: import_masks(surface) for surface in set(MASKS.values())}
    master = build_master(textures['Plate'])
    for surface, (tint, metal, rmin, rmax, tile, depth, contrast) in SURFACES.items():
        path = DEST + '/MI_SB_Keel' + surface
        instance = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
            'MI_SB_Keel' + surface, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(instance, master)
        MEL.set_material_instance_texture_parameter_value(instance, 'SurfaceMasks', textures[MASKS[surface]])
        MEL.set_material_instance_vector_parameter_value(instance, 'BaseColor', unreal.LinearColor(*tint, 1))
        for name, value in (('Metallic', metal), ('RoughnessMin', rmin), ('RoughnessMax', rmax), ('TileSize', tile),
                            ('ReliefDepth', depth), ('DetailContrast', contrast)):
            MEL.set_material_instance_scalar_parameter_value(instance, name, value)
        EAL.save_loaded_asset(instance)
    report = 'RESULT OK: M_SB_KeelHull and MI_SB_Keel%s' % '/'.join(SURFACES)
    (ROOT / 'Saved/Logs/KeelSurfaces.txt').write_text(report + '\n', encoding='utf-8')
    unreal.log(report)


main()
