"""Materials for Keel's brain tank (Art/blender/build_keel_brain_tank.py), headless. Run from Tools/ImportKeelBrainTank.bat
after the narrow mesh import.

- M_SB_KeelBrain: wet tissue. Clear coat shading (a glossy wet film over a softer tissue layer), base colour from the
  mesh's baked vertex colours (pale gyri, dark wet sulci, veins), a faint warm self-light for the translucent look.
- M_SB_TankGlass: real glass. Translucent, lit per pixel (forward shading, so it shows specular highlights and
  reflections), refraction by index of refraction, Fresnel opacity, a faint teal edge tint.
- M_SB_TankFluid: drifting haze in the tank fluid. Unlit translucent, the T_SB_TankHaze texture panning slowly upward,
  softened where it cuts the brain and the tank (depth fade).
Assigns them to the meshes' M_TankBrain / M_TankGlass / M_TankFluid slots and turns Nanite on for the opaque mesh only
(Nanite does not draw translucent materials). Writes Saved/Logs/KeelBrainTank.txt.
"""
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DEST = "/Game/Shieldbreak/Art/Keel"
KIT = "/Game/Shieldbreak/Art/MapKit"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
lines = []


def log(text):
    lines.append(text)
    unreal.log("KeelBrainTank: " + text)


def prop(*names):
    """A MaterialProperty by the first of names this engine's Python exposes (the custom data pins are named
    differently between versions)."""
    for name in names:
        if hasattr(unreal.MaterialProperty, name):
            return getattr(unreal.MaterialProperty, name)
    raise RuntimeError("No MaterialProperty among %s; have %s" % (names, [n for n in dir(unreal.MaterialProperty) if n.startswith("MP_")]))


def fresh_material(name):
    path = DEST + "/" + name
    material = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(material)
    return material


class Graph:
    def __init__(self, material):
        self.m = material

    def node(self, kind, x, y, **props):
        result = MEL.create_material_expression(self.m, kind, x, y)
        for key, value in props.items():
            result.set_editor_property(key, value)
        return result

    def link(self, a, b, pin, output=""):
        if not MEL.connect_material_expressions(a, output, b, pin):
            raise RuntimeError("Cannot connect %s.%s to %s.%s" % (a.get_name(), output, b.get_name(), pin))

    def out(self, a, prop, output=""):
        if not MEL.connect_material_property(a, output, prop):
            raise RuntimeError("Cannot connect %s to %s" % (a.get_name(), prop))

    def scalar(self, name, value, x, y):
        return self.node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)

    def colour(self, name, rgb, x, y):
        return self.node(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                         default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def mul(self, a, b, x, y, out_a="", out_b=""):
        n = self.node(unreal.MaterialExpressionMultiply, x, y)
        self.link(a, n, "A", out_a)
        self.link(b, n, "B", out_b)
        return n

    def lerp(self, a, b, alpha, x, y, out_alpha=""):
        n = self.node(unreal.MaterialExpressionLinearInterpolate, x, y)
        self.link(a, n, "A")
        self.link(b, n, "B")
        self.link(alpha, n, "Alpha", out_alpha)
        return n


def finish(material, usages):
    for usage in usages:
        MEL.set_base_material_usage(material, usage, True)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)
    log("built " + material.get_path_name())


def build_brain():
    m = fresh_material("M_SB_KeelBrain")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    # Python does not expose the clear coat pins (CustomData0/1) as material properties, so everything goes through a
    # Make Material Attributes node, whose ClearCoat / ClearCoatRoughness inputs can be linked by name.
    m.set_editor_property("use_material_attributes", True)
    g = Graph(m)
    attrs = g.node(unreal.MaterialExpressionMakeMaterialAttributes, 0, 0)
    vc = g.node(unreal.MaterialExpressionVertexColor, -900, 0)
    tint = g.colour("Tint", (0.72, 0.58, 0.64), -900, -200)
    base = g.mul(vc, tint, -600, -100)
    g.link(base, attrs, "BaseColor")
    # Darker (sulcus) tissue is wetter: roughness from the base colour's brightness.
    lum = g.node(unreal.MaterialExpressionComponentMask, -600, 100, r=True, g=False, b=False, a=False)
    g.link(vc, lum, "")
    rough = g.lerp(g.scalar("SulcusRoughness", 0.2, -600, 200), g.scalar("GyrusRoughness", 0.42, -600, 300), lum, -300, 200)
    g.link(rough, attrs, "Roughness")
    g.link(g.scalar("Specular", 0.55, -300, 350), attrs, "Specular")
    g.link(g.scalar("ClearCoat", 1.0, -300, 450), attrs, "ClearCoat")
    g.link(g.scalar("ClearCoatRoughness", 0.05, -300, 550), attrs, "ClearCoatRoughness")
    # A faint warm self-light so the tissue reads translucent rather than plaster in the dark room.
    glow = g.mul(base, g.scalar("SelfLight", 0.015, -600, 650), -300, 650)
    g.link(glow, attrs, "EmissiveColor")
    g.out(attrs, unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    finish(m, [unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, unreal.MaterialUsage.MATUSAGE_NANITE])
    return m


def build_glass():
    m = fresh_material("M_SB_TankGlass")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    try:
        m.set_editor_property("refraction_method", unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
    except Exception as error:  # older names
        log("refraction_method not set: %s" % error)
    g = Graph(m)
    fresnel = g.node(unreal.MaterialExpressionFresnel, -900, 0, exponent=5.0, base_reflect_fraction=0.04)
    g.out(g.colour("GlassColor", (0.02, 0.06, 0.06), -600, -300), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.scalar("Metallic", 0.0, -600, -200), unreal.MaterialProperty.MP_METALLIC)
    g.out(g.scalar("Specular", 0.6, -600, -120), unreal.MaterialProperty.MP_SPECULAR)
    g.out(g.scalar("Roughness", 0.1, -600, -40), unreal.MaterialProperty.MP_ROUGHNESS)
    opacity = g.lerp(g.scalar("FacingOpacity", 0.08, -600, 60), g.scalar("EdgeOpacity", 0.3, -600, 140), fresnel, -300, 100)
    g.out(opacity, unreal.MaterialProperty.MP_OPACITY)
    ior = g.lerp(g.scalar("IORFacing", 1.18, -600, 240), g.scalar("IOREdge", 1.0, -600, 320), fresnel, -300, 280)
    g.out(ior, unreal.MaterialProperty.MP_REFRACTION)
    edge = g.mul(g.mul(fresnel, g.colour("EdgeTint", (0.05, 0.35, 0.32), -600, 420), -450, 420),
                 g.scalar("EdgeTintStrength", 0.5, -600, 520), -300, 460)
    g.out(edge, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, [unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES])
    return m


def import_haze():
    task = unreal.AssetImportTask()
    task.filename = os.path.join(PROJECT, "Art", "Export", "Textures", "T_SB_TankHaze.png")
    task.destination_path = DEST
    task.destination_name = "T_SB_TankHaze"
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(DEST + "/T_SB_TankHaze")
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    EAL.save_loaded_asset(texture)
    return texture


def build_fluid(texture):
    m = fresh_material("M_SB_TankFluid")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    g = Graph(m)
    panner = g.node(unreal.MaterialExpressionPanner, -1100, 0, speed_x=0.004, speed_y=-0.012)
    sample = g.node(unreal.MaterialExpressionTextureSampleParameter2D, -850, 0, parameter_name="Haze", texture=texture,
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    g.link(panner, sample, "UVs")
    density = g.mul(sample, g.scalar("Density", 0.07, -850, 200), -600, 100, out_a="R")
    fade = g.node(unreal.MaterialExpressionDepthFade, -350, 100)
    fade.set_editor_property("fade_distance_default", 14.0)
    linked = False
    for pin in ("InOpacity", "Opacity"):
        if MEL.connect_material_expressions(density, "", fade, pin):
            linked = True
            break
    g.out(fade if linked else density, unreal.MaterialProperty.MP_OPACITY)
    # Glow: the haze plus its fine suspended specks, in the tank's teal.
    specks = g.mul(sample, g.scalar("Specks", 0.8, -850, 400), -600, 350, out_a="G")
    add = g.node(unreal.MaterialExpressionAdd, -450, 250)
    g.link(sample, add, "A", "R")
    g.link(specks, add, "B")
    glow = g.mul(g.mul(add, g.colour("FluidColor", (0.12, 0.55, 0.5), -850, 550), -300, 300),
                 g.scalar("Strength", 0.2, -600, 600), -150, 350)
    g.out(glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, [unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES])
    return m


def assign(mesh_name, by_slot, nanite):
    mesh = unreal.load_asset("%s/%s" % (KIT, mesh_name))
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Missing %s/%s: run the narrow import first" % (KIT, mesh_name))
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        name = str(slot.material_slot_name)
        if name in by_slot:
            mesh.set_material(index, by_slot[name])
            log("%s: %s -> %s" % (mesh_name, name, by_slot[name].get_name()))
    settings = mesh.get_editor_property("nanite_settings")
    if settings.enabled != nanite:
        settings.enabled = nanite
        mesh.set_editor_property("nanite_settings", settings)
    EAL.save_loaded_asset(mesh)
    log("%s: %d tris, nanite %s" % (mesh_name, mesh.get_num_triangles(0), "on" if nanite else "off"))


def main():
    brain = build_brain()
    glass = build_glass()
    fluid = build_fluid(import_haze())
    assign("SM_SB_Kit_KeelBrainTank", {"M_TankBrain": brain}, True)
    assign("SM_SB_Kit_KeelBrainTankGlass", {"M_TankGlass": glass, "M_TankFluid": fluid}, False)
    # The light strips' palette instance (made by the import from palette.json) at the strength tuned in game.
    glow = unreal.load_asset("/Game/Shieldbreak/Art/Materials/MI_SB_TankGlow")
    if glow:
        MEL.set_material_instance_scalar_parameter_value(glow, "EmissiveStrength", 1.5)
        EAL.save_loaded_asset(glow)
        log("MI_SB_TankGlow: EmissiveStrength 1.5")
    log("RESULT OK")


try:
    main()
except Exception as error:
    log("RESULT FAILED: %s" % error)
    raise
finally:
    out = os.path.join(PROJECT, "Saved", "Logs", "KeelBrainTank.txt")
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")
