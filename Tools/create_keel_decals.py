"""Create Keel's landmark decals: a deferred-decal master and the 01 / 02 bridge numbers and trench chevron instances.

Textures come from Art/textures/make_keel_stencils.py (RGB paint colour, alpha = worn paint coverage).
Run with Tools/MakeKeelDecals.bat; the game places them on Keel unless -SBNoKeelDecals is set.
"""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST = '/Game/Shieldbreak/Art/Keel'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

# instance: (texture, paint tint)
DECALS = {
    'Stencil01': ('T_SB_KeelStencil01', (0.85, 0.30, 0.20)),   # red bridge: worn red-orange paint
    'Stencil02': ('T_SB_KeelStencil02', (0.30, 0.50, 0.85)),   # blue bridge
    'Chevrons': ('T_SB_KeelChevrons', (1.0, 1.0, 1.0)),
}


def import_texture(name):
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
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    EAL.save_loaded_asset(texture)
    return texture


def build_master(default_texture):
    path = DEST + '/M_SB_KeelDecal'
    material = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        'M_SB_KeelDecal', DEST, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(material)
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)

    def node(kind, x, y, **props):
        result = MEL.create_material_expression(material, kind, x, y)
        for key, value in props.items():
            result.set_editor_property(key, value)
        return result

    def link(a, b, pin, output=''):
        if not MEL.connect_material_expressions(a, output, b, pin):
            raise RuntimeError('Cannot connect %s to %s.%s' % (a.get_name(), b.get_name(), pin))

    sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -700, 0, parameter_name='Stencil', texture=default_texture)
    tint = node(unreal.MaterialExpressionVectorParameter, -700, -250, parameter_name='Tint',
                default_value=unreal.LinearColor(1, 1, 1, 1))
    colour = node(unreal.MaterialExpressionMultiply, -350, -150); link(sample, colour, 'A', 'RGB'); link(tint, colour, 'B')
    MEL.connect_material_property(colour, '', unreal.MaterialProperty.MP_BASE_COLOR)
    opacity = node(unreal.MaterialExpressionMultiply, -350, 100); link(sample, opacity, 'A', 'A')
    link(node(unreal.MaterialExpressionScalarParameter, -700, 250, parameter_name='Opacity', default_value=0.9), opacity, 'B')
    MEL.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    MEL.connect_material_property(node(unreal.MaterialExpressionScalarParameter, -350, 300, parameter_name='Roughness',
                                       default_value=0.7), '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(node(unreal.MaterialExpressionConstant, -350, 400, r=0.0), '',
                                  unreal.MaterialProperty.MP_METALLIC)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)
    return material


def main():
    textures = {name: import_texture(texture) for name, (texture, _) in DECALS.items()}
    master = build_master(textures['Stencil01'])
    for name, (_, tint) in DECALS.items():
        path = DEST + '/MI_SB_Keel' + name
        instance = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
            'MI_SB_Keel' + name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(instance, master)
        MEL.set_material_instance_texture_parameter_value(instance, 'Stencil', textures[name])
        MEL.set_material_instance_vector_parameter_value(instance, 'Tint', unreal.LinearColor(*tint, 1))
        EAL.save_loaded_asset(instance)
    report = 'RESULT OK: M_SB_KeelDecal and MI_SB_Keel%s' % '/'.join(DECALS)
    (ROOT / 'Saved/Logs/KeelDecals.txt').write_text(report + '\n', encoding='utf-8')
    unreal.log(report)


main()
