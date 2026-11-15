"""Keel's animated light materials (no textures, no ticking actors): the reactor pylons' slow pulse.

M_SB_KeelPulse: emissive colour times a sine of game time, phase-shifted by world X so neighbouring pylons breathe
out of step. MI_SB_KeelPulse is what the game puts on the reactor pylons' light slots and columns.
Run with Tools/MakeKeelFX.bat.
"""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST = '/Game/Shieldbreak/Art/Keel'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def main():
    path = DEST + '/M_SB_KeelPulse'
    material = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        'M_SB_KeelPulse', DEST, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(material)

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

    colour = node(unreal.MaterialExpressionVectorParameter, -900, -300, parameter_name='EmissiveColor',
                  default_value=unreal.LinearColor(0.72, 0.84, 1.0, 1))
    time = node(unreal.MaterialExpressionTime, -1200, 0)
    speed = node(unreal.MaterialExpressionMultiply, -1000, 0); link(time, speed, 'A'); link(scalar('PulseSpeed', 1.6, -1200, 120), speed, 'B')
    position = node(unreal.MaterialExpressionWorldPosition, -1200, 250)
    x = node(unreal.MaterialExpressionComponentMask, -1000, 250, r=True, g=False, b=False, a=False); link(position, x, '')
    phase = node(unreal.MaterialExpressionMultiply, -800, 250, const_b=0.004); link(x, phase, 'A')
    angle = node(unreal.MaterialExpressionAdd, -800, 0); link(speed, angle, 'A'); link(phase, angle, 'B')
    wave = node(unreal.MaterialExpressionSine, -600, 0, period=6.283185); link(angle, wave, '')
    # 1 - Depth * (0.5 + 0.5 * sin): the light dips by Depth at the bottom of each breath, never goes out.
    half = node(unreal.MaterialExpressionMultiply, -400, 0, const_b=0.5); link(wave, half, 'A')
    lifted = node(unreal.MaterialExpressionAdd, -250, 0, const_b=0.5); link(half, lifted, 'A')
    dip = node(unreal.MaterialExpressionMultiply, -100, 0); link(lifted, dip, 'A'); link(scalar('Depth', 0.55, -250, 120), dip, 'B')
    pulse = node(unreal.MaterialExpressionOneMinus, 50, 0); link(dip, pulse, '')
    strength = node(unreal.MaterialExpressionMultiply, 200, -150); link(colour, strength, 'A'); link(scalar('EmissiveStrength', 3.0, 0, -150), strength, 'B')
    glow = node(unreal.MaterialExpressionMultiply, 400, -100); link(strength, glow, 'A'); link(pulse, glow, 'B')
    MEL.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    base = node(unreal.MaterialExpressionMultiply, 200, -350, const_b=0.2); link(colour, base, 'A')
    MEL.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(scalar('Roughness', 0.4, 200, 200), '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)

    instance_path = DEST + '/MI_SB_KeelPulse'
    instance = unreal.load_asset(instance_path) if EAL.does_asset_exist(instance_path) else TOOLS.create_asset(
        'MI_SB_KeelPulse', DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(instance, material)
    EAL.save_loaded_asset(instance)
    report = 'RESULT OK: M_SB_KeelPulse and MI_SB_KeelPulse'
    (ROOT / 'Saved/Logs/KeelFX.txt').write_text(report + '\n', encoding='utf-8')
    unreal.log(report)


main()
