"""Create Keel's view-out material: the procedural space panorama (Art/textures/make_keel_space.py) as an unlit
emissive panel with parallax (bump offset on a constant height), so the window reads as far away and shifts as you
walk past it. Run with Tools/MakeKeelSpace.bat; the game hangs it in the Breach's hull window.
"""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST = '/Game/Shieldbreak/Art/Keel'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def main():
    task = unreal.AssetImportTask()
    task.filename = str(ROOT / 'Art/Export/Textures/T_SB_KeelSpace.png')
    task.destination_path = DEST
    task.destination_name = 'T_SB_KeelSpace'
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(DEST + '/T_SB_KeelSpace')
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError('T_SB_KeelSpace was not imported')
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    EAL.save_loaded_asset(texture)

    path = DEST + '/M_SB_KeelSpace'
    material = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        'M_SB_KeelSpace', DEST, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(material)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)

    def node(kind, x, y, **props):
        result = MEL.create_material_expression(material, kind, x, y)
        for key, value in props.items():
            result.set_editor_property(key, value)
        return result

    def link(a, b, pin, output=''):
        if not MEL.connect_material_expressions(a, output, b, pin):
            raise RuntimeError('Cannot connect %s to %s.%s' % (a.get_name(), b.get_name(), pin))

    # The window is about 10:1 and the panorama 4:1: show a horizontal band of it (no stretched stars), with the
    # planet's limb in the lower right.
    coords = node(unreal.MaterialExpressionTextureCoordinate, -1300, 0)
    band = node(unreal.MaterialExpressionMultiply, -1100, 0); link(coords, band, 'A')
    link(node(unreal.MaterialExpressionConstant2Vector, -1300, 120, r=1.0, g=0.39), band, 'B')
    shifted = node(unreal.MaterialExpressionAdd, -900, 0); link(band, shifted, 'A')
    link(node(unreal.MaterialExpressionConstant2Vector, -1100, 120, r=0.0, g=0.36), shifted, 'B')
    offset = node(unreal.MaterialExpressionBumpOffset, -650, 0, height_ratio=0.0)
    link(shifted, offset, 'Coordinate')
    link(node(unreal.MaterialExpressionConstant, -900, 150, r=1.0), offset, 'Height')
    link(node(unreal.MaterialExpressionScalarParameter, -900, 250, parameter_name='Depth', default_value=0.08), offset, 'HeightRatioInput')
    sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -400, 0, parameter_name='Space', texture=texture)
    link(offset, sample, 'UVs')
    glow = node(unreal.MaterialExpressionMultiply, -150, 0); link(sample, glow, 'A', 'RGB')
    link(node(unreal.MaterialExpressionScalarParameter, -400, 250, parameter_name='Brightness', default_value=0.35), glow, 'B')
    MEL.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)
    report = 'RESULT OK: T_SB_KeelSpace and M_SB_KeelSpace'
    (ROOT / 'Saved/Logs/KeelSpace.txt').write_text(report + '\n', encoding='utf-8')
    unreal.log(report)


main()
