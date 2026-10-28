"""Create Shieldbreak's original translucent energy-hit overlay, with no textures."""
from pathlib import Path
import unreal

path = '/Game/Shieldbreak/Art/Materials/M_SB_ShieldImpact'
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(
    'M_SB_ShieldImpact', '/Game/Shieldbreak/Art/Materials', unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(asset)
asset.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
asset.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
asset.set_editor_property('two_sided', False)

def node(kind, x, y):
    return mel.create_material_expression(asset, kind, x, y)

intensity = node(unreal.MaterialExpressionScalarParameter, -700, 0)
intensity.set_editor_property('parameter_name', 'Intensity')
intensity.set_editor_property('default_value', 0.0)
color = node(unreal.MaterialExpressionVectorParameter, -700, -200)
color.set_editor_property('parameter_name', 'ShieldColor')
color.set_editor_property('default_value', unreal.LinearColor(0.18, 0.7, 1.0, 1))
fresnel = node(unreal.MaterialExpressionFresnel, -700, 200)
fresnel.set_editor_property('exponent', 2.5)
fresnel.set_editor_property('base_reflect_fraction', 0.2)
opacity = node(unreal.MaterialExpressionMultiply, -400, 100)
mel.connect_material_expressions(fresnel, '', opacity, 'A')
mel.connect_material_expressions(intensity, '', opacity, 'B')
brightness = node(unreal.MaterialExpressionMultiply, -150, -150)
brightness.set_editor_property('const_b', 4.0)
# Additive blending already multiplies emission by opacity. Multiplying by
# intensity here too made ordinary hits almost invisible while breaks showed.
mel.connect_material_expressions(color, '', brightness, 'A')
mel.connect_material_property(brightness, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
mel.set_material_usage(asset, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
mel.recompile_material(asset)
unreal.EditorAssetLibrary.save_loaded_asset(asset)
root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
(root/'Saved/Logs/ShieldMaterial.txt').write_text('Created '+path+'; additive/unlit, skeletal usage, Intensity and ShieldColor parameters', encoding='utf-8')
unreal.log('Created '+path)
