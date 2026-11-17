"""Keel's iridescent alien hull material: glossy metal shifting from deep violet (facing) to teal (grazing).

M_SB_KeelAlien + MI_SB_KeelAlien in /Game/Shieldbreak/Art/Keel. The game puts it on the alien kit's M_AlienHull
slots. Run with Tools/ImportKeelAlien.bat (after the mesh import).
"""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST = '/Game/Shieldbreak/Art/Keel'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def main():
    path = DEST + '/M_SB_KeelAlien'
    material = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        'M_SB_KeelAlien', DEST, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(material)

    def node(kind, x, y, **props):
        result = MEL.create_material_expression(material, kind, x, y)
        for key, value in props.items():
            result.set_editor_property(key, value)
        return result

    def link(a, b, pin, output=''):
        if not MEL.connect_material_expressions(a, output, b, pin):
            raise RuntimeError('Cannot connect %s to %s.%s' % (a.get_name(), b.get_name(), pin))

    facing = node(unreal.MaterialExpressionVectorParameter, -800, -300, parameter_name='Facing',
                  default_value=unreal.LinearColor(0.10, 0.03, 0.18, 1))
    grazing = node(unreal.MaterialExpressionVectorParameter, -800, -100, parameter_name='Grazing',
                   default_value=unreal.LinearColor(0.02, 0.30, 0.34, 1))
    fresnel = node(unreal.MaterialExpressionFresnel, -800, 100, exponent=2.5, base_reflect_fraction=0.04)
    blend = node(unreal.MaterialExpressionLinearInterpolate, -400, -200)
    link(facing, blend, 'A'); link(grazing, blend, 'B'); link(fresnel, blend, 'Alpha')
    MEL.connect_material_property(blend, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(node(unreal.MaterialExpressionScalarParameter, -400, 50, parameter_name='Metallic',
                                       default_value=0.85), '', unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(node(unreal.MaterialExpressionScalarParameter, -400, 150, parameter_name='Roughness',
                                       default_value=0.24), '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)
    instance_path = DEST + '/MI_SB_KeelAlien'
    instance = unreal.load_asset(instance_path) if EAL.does_asset_exist(instance_path) else TOOLS.create_asset(
        'MI_SB_KeelAlien', DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(instance, material)
    EAL.save_loaded_asset(instance)
    build_glyphs()
    (ROOT / 'Saved/Logs/KeelAlienMaterial.txt').write_text('RESULT OK\n', encoding='utf-8')



def build_glyphs():
    """M_SB_KeelGlyph: a dark mauve panel whose glyph strokes (T_SB_KeelGlyphs R) glow purple."""
    task = unreal.AssetImportTask()
    task.filename = str(ROOT / 'Art/Export/Textures/T_SB_KeelGlyphs.png')
    task.destination_path = DEST
    task.destination_name = 'T_SB_KeelGlyphs'
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(DEST + '/T_SB_KeelGlyphs')
    texture.set_editor_property('srgb', False)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
    EAL.save_loaded_asset(texture)
    path = DEST + '/M_SB_KeelGlyph'
    material = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        'M_SB_KeelGlyph', DEST, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(material)

    def node(kind, x, y, **props):
        result = MEL.create_material_expression(material, kind, x, y)
        for key, value in props.items():
            result.set_editor_property(key, value)
        return result

    def link(a, b, pin, output=''):
        if not MEL.connect_material_expressions(a, output, b, pin):
            raise RuntimeError('Cannot connect %s to %s.%s' % (a.get_name(), b.get_name(), pin))

    sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -800, 0, parameter_name='Glyphs', texture=texture,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    colour = node(unreal.MaterialExpressionVectorParameter, -800, -250, parameter_name='GlowColor',
                  default_value=unreal.LinearColor(0.62, 0.22, 1.0, 1))
    glow = node(unreal.MaterialExpressionMultiply, -500, -150); link(sample, glow, 'A', 'R'); link(colour, glow, 'B')
    strength = node(unreal.MaterialExpressionMultiply, -300, -150); link(glow, strength, 'A')
    link(node(unreal.MaterialExpressionScalarParameter, -500, 0, parameter_name='Strength', default_value=8.0), strength, 'B')
    MEL.connect_material_property(strength, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    base = node(unreal.MaterialExpressionMultiply, -300, 150); link(sample, base, 'A', 'G')
    link(node(unreal.MaterialExpressionConstant3Vector, -500, 250, constant=unreal.LinearColor(0.06, 0.03, 0.08, 1)), base, 'B')
    MEL.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(node(unreal.MaterialExpressionConstant, -300, 350, r=0.35), '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)


main()
