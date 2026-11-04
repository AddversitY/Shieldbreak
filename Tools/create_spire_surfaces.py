"""Create Spire-only world-projected satin alloy / ceramic materials.

One original packed texture, three triplanar samples; no changes to shared world or weapon materials.
Surface-gradient bump uses the same packed samples (no extra normal-map samples).
"""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST = '/Game/Shieldbreak/Art/Spire'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def main():
    task = unreal.AssetImportTask()
    task.filename = str(ROOT / 'Art/Export/Textures/T_SB_SpireSurfaceMasks.png')
    task.destination_path = DEST
    task.destination_name = 'T_SB_SpireSurfaceMasks'
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(DEST + '/T_SB_SpireSurfaceMasks')
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError('Packed Spire masks were not imported')
    texture.set_editor_property('srgb', False)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_WRAP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_WRAP)
    EAL.save_loaded_asset(texture)

    path = DEST + '/M_SB_SpireSurface'
    material = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        'M_SB_SpireSurface', DEST, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(material)
    material.set_editor_property('tangent_space_normal', False)

    def node(kind, x, y, **props):
        result = MEL.create_material_expression(material, kind, x, y)
        for key, value in props.items():
            result.set_editor_property(key, value)
        return result

    def link(a, b, pin, output=''):
        if not MEL.connect_material_expressions(a, output, b, pin):
            raise RuntimeError(f'Cannot connect {a.get_name()} to {b.get_name()}.{pin}')

    position = node(unreal.MaterialExpressionWorldPosition, -1600, 0)
    normal = node(unreal.MaterialExpressionVertexNormalWS, -1600, 600)
    tile = node(unreal.MaterialExpressionScalarParameter, -1600, 150,
                parameter_name='TileSize', default_value=100.0)
    uvw = node(unreal.MaterialExpressionDivide, -1400, 0)
    link(position, uvw, 'A'); link(tile, uvw, 'B')
    absolute = node(unreal.MaterialExpressionAbs, -1400, 600); link(normal, absolute, '')
    power = node(unreal.MaterialExpressionPower, -1200, 600, const_exponent=8.0); link(absolute, power, 'Base')
    ones = node(unreal.MaterialExpressionConstant3Vector, -1200, 760, constant=unreal.LinearColor(1, 1, 1, 1))
    total = node(unreal.MaterialExpressionDotProduct, -1000, 650); link(power, total, 'A'); link(ones, total, 'B')
    weights = node(unreal.MaterialExpressionDivide, -800, 600); link(power, weights, 'A'); link(total, weights, 'B')
    packed = None
    for index, channels in enumerate(((False, True, True), (True, False, True), (True, True, False))):
        uv = node(unreal.MaterialExpressionComponentMask, -1200, -450+index*280,
                  r=channels[0], g=channels[1], b=channels[2], a=False); link(uvw, uv, '')
        sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -1000, -450+index*280,
                      parameter_name='SurfaceMasks', texture=texture,
                      sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS); link(uv, sample, 'UVs')
        mask = node(unreal.MaterialExpressionComponentMask, -600, 500+index*120,
                    r=index == 0, g=index == 1, b=index == 2, a=False); link(weights, mask, '')
        weighted = node(unreal.MaterialExpressionMultiply, -600, -450+index*280)
        link(sample, weighted, 'A', 'RGB'); link(mask, weighted, 'B')
        if packed is None:
            packed = weighted
        else:
            added = node(unreal.MaterialExpressionAdd, -400, -350+index*240)
            link(packed, added, 'A'); link(weighted, added, 'B'); packed = added

    tint = node(unreal.MaterialExpressionVectorParameter, -200, -700,
                parameter_name='Tint', default_value=unreal.LinearColor(.40, .46, .50, 1))
    colour = node(unreal.MaterialExpressionComponentMask, -200, -420, r=True, g=False, b=False, a=False)
    link(packed, colour, '')
    variation = node(unreal.MaterialExpressionMultiply, 0, -420, const_b=.08); link(colour, variation, 'A')
    bias = node(unreal.MaterialExpressionAdd, 180, -420, const_b=.96); link(variation, bias, 'A')
    base = node(unreal.MaterialExpressionMultiply, 360, -600); link(tint, base, 'A'); link(bias, base, 'B')
    MEL.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)

    rough_base = node(unreal.MaterialExpressionScalarParameter, -200, -80,
                      parameter_name='Roughness', default_value=.48)
    rough_mask = node(unreal.MaterialExpressionComponentMask, -200, 50, r=False, g=True, b=False, a=False)
    link(packed, rough_mask, '')
    rough_center = node(unreal.MaterialExpressionSubtract, 0, 50, const_b=.5); link(rough_mask, rough_center, 'A')
    rough_variation = node(unreal.MaterialExpressionMultiply, 180, 50, const_b=.1); link(rough_center, rough_variation, 'A')
    rough = node(unreal.MaterialExpressionAdd, 360, -80); link(rough_base, rough, 'A'); link(rough_variation, rough, 'B')
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    metallic = node(unreal.MaterialExpressionScalarParameter, 0, -240,
                    parameter_name='Metallic', default_value=.65)
    MEL.connect_material_property(metallic, '', unreal.MaterialProperty.MP_METALLIC)

    height = node(unreal.MaterialExpressionComponentMask, -200, 260, r=False, g=False, b=True, a=False)
    link(packed, height, '')
    amplitude = node(unreal.MaterialExpressionScalarParameter, -200, 400,
                     parameter_name='ReliefDepth', default_value=.001)
    custom_inputs = []
    for name in ('P', 'N', 'H', 'Depth'):
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property('input_name', name)
        custom_inputs.append(custom_input)
    bump = node(unreal.MaterialExpressionCustom, 360, 300,
                output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                inputs=custom_inputs,
                description='Filtered surface-gradient microstructure (world space)',
                code='float3 dx=ddx(P),dy=ddy(P); float3 a=cross(dy,N),b=cross(N,dx); '
                     'float det=dot(dx,a); float3 grad=sign(det)*(ddx(H)*a+ddy(H)*b); '
                     'return normalize(max(abs(det),1e-8)*N-Depth*grad);')
    link(position, bump, 'P'); link(normal, bump, 'N'); link(height, bump, 'H'); link(amplitude, bump, 'Depth')
    MEL.connect_material_property(bump, '', unreal.MaterialProperty.MP_NORMAL)
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)

    for name, tint_value, metal, roughness in (
        ('Alloy', (.46, .51, .56), .50, .55),
        ('Inset', (.075, .095, .115), .12, .66),
        ('Deck', (.20, .25, .29), .20, .64),
    ):
        instance_path = DEST + '/MI_SB_Spire' + name
        instance = unreal.load_asset(instance_path) if EAL.does_asset_exist(instance_path) else TOOLS.create_asset(
            'MI_SB_Spire'+name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(instance, material)
        MEL.set_material_instance_vector_parameter_value(instance, 'Tint', unreal.LinearColor(*tint_value, 1))
        MEL.set_material_instance_scalar_parameter_value(instance, 'Metallic', metal)
        MEL.set_material_instance_scalar_parameter_value(instance, 'Roughness', roughness)
        EAL.save_loaded_asset(instance)
    report = 'RESULT OK: original packed texture, world-projected Spire master and Alloy/Inset/Deck instances'
    (ROOT/'Saved/Logs/SpireSurfaces.txt').write_text(report+'\n', encoding='utf-8')
    unreal.log(report)


main()
