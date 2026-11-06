"""Creates the bullet hit materials in /Game/Shieldbreak/Art/FX (UShieldbreakImpactFXSubsystem drives them). All
procedural and original; the only input is Lyra's tiling noise texture for break-up.

- M_SB_FXSpark: unlit additive, emissive = per-instance colour (custom data 0-2) x brightness (3). Drawn on thin
  stretched cubes in one instanced mesh, so every spark in the match is one draw call.
- M_SB_FXGlowInst: unlit additive camera-facing card, soft radial glow, per-instance colour/brightness, depth faded.
- M_SB_FXPuffInst: unlit translucent camera-facing card, a soft noisy dust/smoke puff, per-instance colour (0-2),
  opacity (3) and noise offset (4), depth faded so it never shows a hard line against the wall it came off.
- M_SB_MuzzleFlash: unlit additive card, a hot core plus N spikes (Spikes, Sharpness, Rotation), Color x Intensity.
- M_SB_BulletHole: deferred decal, a dark pocked hole with a scorched, noisy ring faded by Opacity (the subsystem fades old holes).

Writes Saved/Logs/ImpactFXMaterials.txt. Run with Tools/CreateImpactFX.bat."""
import os
import unreal

FOLDER = "/Game/Shieldbreak/Art/FX"
NOISE_TEX = "/Game/Effects/Textures/General/Good64x64TilingNoiseHighFreq"
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
log = []


def material(name):
    path = FOLDER + "/" + name
    asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(
        name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(asset)
    return asset


def builder(asset):
    def node(kind, x, y, **props):
        n = mel.create_material_expression(asset, kind, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(a, b, pin, out=""):
        mel.connect_material_expressions(a, out, b, pin)
    return node, link


def scalar(node, name, value, x, y):
    return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def custom(node, index, x, y, default=0.0):
    n = node(unreal.MaterialExpressionPerInstanceCustomData, x, y, data_index=index)
    try:
        n.set_editor_property("const_default_value", default)
    except Exception:
        pass
    return n


def custom_rgb(node, link, x, y):
    r, g, b = custom(node, 0, x, y, 1.0), custom(node, 1, x, y + 80, 1.0), custom(node, 2, x, y + 160, 1.0)
    rg = node(unreal.MaterialExpressionAppendVector, x + 200, y)
    link(r, rg, "A")
    link(g, rg, "B")
    rgb = node(unreal.MaterialExpressionAppendVector, x + 350, y + 40)
    link(rg, rgb, "A")
    link(b, rgb, "B")
    return rgb


def radius(node, link, x, y):
    """Distance from the card's centre, 0 in the middle and 1 at the edge's midpoint."""
    uv = node(unreal.MaterialExpressionTextureCoordinate, x, y)
    centred = node(unreal.MaterialExpressionSubtract, x + 150, y, const_b=0.5)
    link(uv, centred, "A")
    length = node(unreal.MaterialExpressionLength, x + 300, y)
    link(centred, length, "")
    scaled = node(unreal.MaterialExpressionMultiply, x + 450, y, const_b=2.0)
    link(length, scaled, "A")
    return scaled, centred


def radial_mask(node, link, power, x, y):
    r, _ = radius(node, link, x, y)
    inverted = node(unreal.MaterialExpressionOneMinus, x + 600, y)
    link(r, inverted, "")
    clamped = node(unreal.MaterialExpressionSaturate, x + 750, y)
    link(inverted, clamped, "")
    shaped = node(unreal.MaterialExpressionPower, x + 900, y, const_exponent=power)
    link(clamped, shaped, "Base")
    return shaped


def mul(node, link, a, b, x, y):
    m = node(unreal.MaterialExpressionMultiply, x, y)
    link(a, m, "A")
    link(b, m, "B")
    return m


def depth_fade(node, link, value, distance, x, y):
    fade = node(unreal.MaterialExpressionDepthFade, x, y)
    link(value, fade, "Opacity")
    dist = node(unreal.MaterialExpressionConstant, x - 150, y + 120, r=distance)
    link(dist, fade, "FadeDistance")
    return fade


def unlit(asset, blend, two_sided=True):
    asset.set_editor_property("blend_mode", blend)
    asset.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    asset.set_editor_property("two_sided", two_sided)


def finish(asset, name, note, instanced=False):
    if instanced:
        mel.set_material_usage(asset, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    mel.recompile_material(asset)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log.append("%s/%s: %s" % (FOLDER, name, note))


noise_tex = unreal.load_asset(NOISE_TEX)

# ---- Spark ----
spark = material("M_SB_FXSpark")
unlit(spark, unreal.BlendMode.BLEND_ADDITIVE)
node, link = builder(spark)
rgb = custom_rgb(node, link, -900, -200)
bright = custom(node, 3, -900, 100, 1.0)
emissive = mul(node, link, rgb, bright, -300, -100)
mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(spark, "M_SB_FXSpark", "additive unlit, custom data rgb+brightness", instanced=True)

# ---- Glow card (instanced) ----
glow = material("M_SB_FXGlowInst")
unlit(glow, unreal.BlendMode.BLEND_ADDITIVE)
node, link = builder(glow)
rgb = custom_rgb(node, link, -900, -300)
bright = custom(node, 3, -900, 0, 1.0)
mask = radial_mask(node, link, 2.4, -1500, 200)
e1 = mul(node, link, rgb, bright, -300, -200)
e2 = mul(node, link, e1, mask, -150, -100)
mel.connect_material_property(depth_fade(node, link, e2, 20.0, 50, -100), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(glow, "M_SB_FXGlowInst", "additive unlit radial glow, custom data rgb+brightness", instanced=True)

# ---- Puff card (instanced) ----
puff = material("M_SB_FXPuffInst")
# Lit (volumetric, non-directional): an unlit grey puff glowed near-white under Keel's exposure (FX3 captures).
puff.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
puff.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
puff.set_editor_property("two_sided", True)
try:
    puff.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
except Exception as exc:
    log.append("puff lighting mode not set: %s" % exc)
node, link = builder(puff)
rgb = custom_rgb(node, link, -900, -400)
mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
alpha = custom(node, 3, -900, -50, 1.0)
seed = custom(node, 4, -1500, 450, 0.0)
uv = node(unreal.MaterialExpressionTextureCoordinate, -1500, 300, u_tiling=0.5, v_tiling=0.5)
offset = node(unreal.MaterialExpressionAdd, -1300, 350)
link(uv, offset, "A")
link(seed, offset, "B")
noise = node(unreal.MaterialExpressionTextureSample, -1100, 350)
if noise_tex:
    noise.set_editor_property("texture", noise_tex)
link(offset, noise, "UVs")
# Noise in 0.35..1 so the puff breaks up without holes.
lifted = node(unreal.MaterialExpressionLinearInterpolate, -850, 350, const_a=0.35, const_b=1.0)
link(noise, lifted, "Alpha", "R")
mask = radial_mask(node, link, 1.6, -1500, 100)
o1 = mul(node, link, alpha, mask, -500, 0)
o2 = mul(node, link, o1, lifted, -350, 100)
mel.connect_material_property(depth_fade(node, link, o2, 30.0, -100, 100), "", unreal.MaterialProperty.MP_OPACITY)
finish(puff, "M_SB_FXPuffInst", "translucent lit noisy puff, custom data rgb+opacity+seed, noise %s" % ("found" if noise_tex else "MISSING"), instanced=True)

# ---- Muzzle flash ----
flash = material("M_SB_MuzzleFlash")
unlit(flash, unreal.BlendMode.BLEND_ADDITIVE)
node, link = builder(flash)
r, centred = radius(node, link, -1800, 0)
# Angle around the centre, then |cos(angle * Spikes / 2 + Rotation)|^Sharpness makes N spikes.
mask_x = node(unreal.MaterialExpressionComponentMask, -1500, 250, r=True, g=False, b=False, a=False)
link(centred, mask_x, "")
mask_y = node(unreal.MaterialExpressionComponentMask, -1500, 350, r=False, g=True, b=False, a=False)
link(centred, mask_y, "")
angle = node(unreal.MaterialExpressionArctangent2, -1350, 300)
link(mask_y, angle, "Y")
link(mask_x, angle, "X")
spikes = scalar(node, "Spikes", 6.0, -1350, 450)
half = node(unreal.MaterialExpressionMultiply, -1200, 450, const_b=0.5)
link(spikes, half, "A")
freq = mul(node, link, angle, half, -1050, 350)
rot = scalar(node, "Rotation", 0.0, -1050, 500)
phase = node(unreal.MaterialExpressionAdd, -900, 400)
link(freq, phase, "A")
link(rot, phase, "B")
cos = node(unreal.MaterialExpressionCosine, -750, 400, period=6.283185)
link(phase, cos, "")
abs_cos = node(unreal.MaterialExpressionAbs, -600, 400)
link(cos, abs_cos, "")
sharp = scalar(node, "Sharpness", 8.0, -600, 550)
spike_shape = node(unreal.MaterialExpressionPower, -450, 400)
link(abs_cos, spike_shape, "Base")
link(sharp, spike_shape, "Exp")
falloff = node(unreal.MaterialExpressionOneMinus, -1200, 0)
link(r, falloff, "")
falloff_c = node(unreal.MaterialExpressionSaturate, -1050, 0)
link(falloff, falloff_c, "")
spike_len = node(unreal.MaterialExpressionPower, -900, 0, const_exponent=1.3)
link(falloff_c, spike_len, "Base")
core = node(unreal.MaterialExpressionPower, -900, -150, const_exponent=4.0)
link(falloff_c, core, "Base")
spike_term = mul(node, link, spike_shape, spike_len, -300, 200)
core_term = node(unreal.MaterialExpressionMultiply, -600, -150, const_b=1.6)
link(core, core_term, "A")
shape = node(unreal.MaterialExpressionAdd, -150, 0)
link(core_term, shape, "A")
link(spike_term, shape, "B")
color = node(unreal.MaterialExpressionVectorParameter, -150, -350, parameter_name="Color", default_value=unreal.LinearColor(1.0, 0.7, 0.35, 1))
intensity = scalar(node, "Intensity", 30.0, -150, -200)
c1 = mul(node, link, color, intensity, 50, -250)
c2 = mul(node, link, c1, shape, 200, -100)
mel.connect_material_property(c2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(flash, "M_SB_MuzzleFlash", "additive unlit star/flame card, params Color/Intensity/Spikes/Sharpness/Rotation")

# ---- Bullet hole decal ----
hole = material("M_SB_BulletHole")
hole.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
hole.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
try:
    hole.set_editor_property("decal_blend_mode", unreal.DecalBlendMode.DBM_TRANSLUCENT)
except Exception:
    pass
node, link = builder(hole)
r, _ = radius(node, link, -1800, 0)
# Hole: solid dark core, soft at its edge.
core = node(unreal.MaterialExpressionSmoothStep, -1200, -200, const_min=0.1, const_max=0.2)
link(r, core, "Value")
core_mask = node(unreal.MaterialExpressionOneMinus, -1050, -200)
link(core, core_mask, "")
# Scorch ring: fades out toward the edge and is broken up by noise.
ring_fall = node(unreal.MaterialExpressionSmoothStep, -1200, 50, const_min=0.15, const_max=1.0)
link(r, ring_fall, "Value")
ring = node(unreal.MaterialExpressionOneMinus, -1050, 50)
link(ring_fall, ring, "")
seed = scalar(node, "Seed", 0.0, -1800, 450)
uv = node(unreal.MaterialExpressionTextureCoordinate, -1800, 300, u_tiling=0.35, v_tiling=0.35)
offset = node(unreal.MaterialExpressionAdd, -1600, 350)
link(uv, offset, "A")
link(seed, offset, "B")
noise = node(unreal.MaterialExpressionTextureSample, -1400, 350)
if noise_tex:
    noise.set_editor_property("texture", noise_tex)
link(offset, noise, "UVs")
noisy_ring = mul(node, link, ring, noise, -850, 100)
darkness = scalar(node, "Darkness", 0.8, -850, 250)
ring_term = mul(node, link, noisy_ring, darkness, -700, 100)
# Rim: a thin band of bright, torn metal around the hole (Rim = 1 on metal, 0 on dirt) so holes read on dark panels too.
rim_in = node(unreal.MaterialExpressionSmoothStep, -1200, 600, const_min=0.13, const_max=0.2)
link(r, rim_in, "Value")
rim_out_s = node(unreal.MaterialExpressionSmoothStep, -1200, 750, const_min=0.22, const_max=0.38)
link(r, rim_out_s, "Value")
rim_out = node(unreal.MaterialExpressionOneMinus, -1050, 750)
link(rim_out_s, rim_out, "")
rim_band = mul(node, link, rim_in, rim_out, -900, 650)
rim_noisy = mul(node, link, rim_band, noise, -750, 650)
rim_amt = scalar(node, "Rim", 1.0, -750, 800)
rim_term = mul(node, link, rim_noisy, rim_amt, -600, 700)
opacity0 = node(unreal.MaterialExpressionAdd, -550, 0)
link(core_mask, opacity0, "A")
link(ring_term, opacity0, "B")
opacity = node(unreal.MaterialExpressionAdd, -480, 100)
link(opacity0, opacity, "A")
link(rim_term, opacity, "B")
opacity_c = node(unreal.MaterialExpressionSaturate, -400, 0)
link(opacity, opacity_c, "")
life = scalar(node, "Opacity", 1.0, -400, 150)
final_opacity = mul(node, link, opacity_c, life, -250, 50)
mel.connect_material_property(final_opacity, "", unreal.MaterialProperty.MP_OPACITY)
tint = node(unreal.MaterialExpressionVectorParameter, -700, -400, parameter_name="Tint", default_value=unreal.LinearColor(0.035, 0.03, 0.028, 1))
hole_color = node(unreal.MaterialExpressionConstant3Vector, -700, -550, constant=unreal.LinearColor(0.004, 0.004, 0.004, 1))
base = node(unreal.MaterialExpressionLinearInterpolate, -400, -450)
link(tint, base, "A")
link(hole_color, base, "B")
link(core_mask, base, "Alpha")
rim_color = node(unreal.MaterialExpressionConstant3Vector, -400, -650, constant=unreal.LinearColor(0.42, 0.4, 0.37, 1))
rim_alpha = node(unreal.MaterialExpressionSaturate, -300, -600)
link(rim_term, rim_alpha, "")
base_rim = node(unreal.MaterialExpressionLinearInterpolate, -200, -500)
link(base, base_rim, "A")
link(rim_color, base_rim, "B")
link(rim_alpha, base_rim, "Alpha")
mel.connect_material_property(base_rim, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = node(unreal.MaterialExpressionConstant, -400, -250, r=0.9)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
finish(hole, "M_SB_BulletHole", "deferred decal, dark hole + noisy scorch ring, params Tint/Darkness/Seed/Opacity/Rim")

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
out = os.path.join(root, "Saved", "Logs", "ImpactFXMaterials.txt")
with open(out, "w", encoding="utf-8") as f:
    f.write("\n".join(log) + "\n")
unreal.log("Impact FX materials:\n" + "\n".join(log))
