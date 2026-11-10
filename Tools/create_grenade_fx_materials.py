"""Creates the explosion's three small materials in /Game/Shieldbreak/Art/FX (AShieldbreakExplosionFX drives them):

- M_SB_FXFire: unlit additive card, a SubUV flipbook (Tex/Columns/Rows/Frame) tinted by Color, faded by Opacity, with
  a soft radial edge and depth fade so it never shows a card edge or a hard line where it meets the floor.
- M_SB_FXSmoke: unlit translucent card, a soft radial puff broken up by panning noise (no flipbook needed), Color and
  Opacity driven per frame, depth faded.
- M_SB_Scorch: deferred decal, a dark, noisy burn mark that fades with the decal's lifetime.

Lyra textures only as inputs (T_Explosion_Areal_E64 and the tiling noise); the materials are ours. Writes
Saved/Logs/GrenadeFXMaterials.txt. Run with Tools/CreateGrenadeFX.bat."""
import os
import unreal

FOLDER = "/Game/Shieldbreak/Art/FX"
FIRE_TEX = "/Game/Effects/Textures/Flipbooks/T_Explosion_Areal_E64"
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


def radial_mask(node, link, power, x, y):
    """(1 - 2|uv - 0.5|)^power, saturated: 1 in the middle, 0 at the card's edge."""
    uv = node(unreal.MaterialExpressionTextureCoordinate, x, y)
    centred = node(unreal.MaterialExpressionSubtract, x + 150, y, const_b=0.5)
    link(uv, centred, "A")
    length = node(unreal.MaterialExpressionLength, x + 300, y)
    link(centred, length, "")
    scaled = node(unreal.MaterialExpressionMultiply, x + 450, y, const_b=2.0)
    link(length, scaled, "A")
    inverted = node(unreal.MaterialExpressionOneMinus, x + 600, y)
    link(scaled, inverted, "")
    clamped = node(unreal.MaterialExpressionSaturate, x + 750, y)
    link(inverted, clamped, "")
    shaped = node(unreal.MaterialExpressionPower, x + 900, y, const_exponent=power)
    link(clamped, shaped, "Base")
    return shaped


def finish(asset, name, note):
    mel.recompile_material(asset)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log.append("%s/%s: %s" % (FOLDER, name, note))


# ---- Fire ----
fire = material("M_SB_FXFire")
fire.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
fire.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
fire.set_editor_property("two_sided", True)
node, link = builder(fire)
frame = scalar(node, "Frame", 0.0, -1600, -300)
cols = scalar(node, "Columns", 8.0, -1600, -150)
rows = scalar(node, "Rows", 8.0, -1600, 0)
index = node(unreal.MaterialExpressionFloor, -1400, -300)
link(frame, index, "")
col = node(unreal.MaterialExpressionFmod, -1250, -300)
link(index, col, "A")
link(cols, col, "B")
row_div = node(unreal.MaterialExpressionDivide, -1250, -150)
link(index, row_div, "A")
link(cols, row_div, "B")
row = node(unreal.MaterialExpressionFloor, -1100, -150)
link(row_div, row, "")
cell = node(unreal.MaterialExpressionAppendVector, -950, -250)
link(col, cell, "A")
link(row, cell, "B")
grid = node(unreal.MaterialExpressionAppendVector, -950, -50)
link(cols, grid, "A")
link(rows, grid, "B")
uv = node(unreal.MaterialExpressionTextureCoordinate, -950, -400)
cell_uv = node(unreal.MaterialExpressionAdd, -800, -300)
link(uv, cell_uv, "A")
link(cell, cell_uv, "B")
sub_uv = node(unreal.MaterialExpressionDivide, -650, -250)
link(cell_uv, sub_uv, "A")
link(grid, sub_uv, "B")
tex = node(unreal.MaterialExpressionTextureSampleParameter2D, -450, -250, parameter_name="Tex")
fire_tex = unreal.load_asset(FIRE_TEX)
if fire_tex:
    tex.set_editor_property("texture", fire_tex)
link(sub_uv, tex, "UVs")
lum = node(unreal.MaterialExpressionDotProduct, -200, -250)
link(tex, lum, "A", "RGB")
lum_w = node(unreal.MaterialExpressionConstant3Vector, -400, -50, constant=unreal.LinearColor(0.4, 0.4, 0.2, 1))
link(lum_w, lum, "B")
color = node(unreal.MaterialExpressionVectorParameter, -200, -450, parameter_name="Color", default_value=unreal.LinearColor(1, 0.5, 0.15, 1))
intensity = scalar(node, "Intensity", 10.0, -200, -50)
opacity = scalar(node, "Opacity", 1.0, -200, 50)
mask = radial_mask(node, link, 0.8, -1400, 300)
glow = node(unreal.MaterialExpressionMultiply, 50, -350)
link(color, glow, "A")
link(lum, glow, "B")
g2 = node(unreal.MaterialExpressionMultiply, 200, -300)
link(glow, g2, "A")
link(intensity, g2, "B")
g3 = node(unreal.MaterialExpressionMultiply, 350, -250)
link(g2, g3, "A")
link(opacity, g3, "B")
g4 = node(unreal.MaterialExpressionMultiply, 500, -200)
link(g3, g4, "A")
link(mask, g4, "B")
fade = node(unreal.MaterialExpressionDepthFade, 650, -150)
link(g4, fade, "Opacity")
fade_dist = node(unreal.MaterialExpressionConstant, 500, 0, r=60.0)
link(fade_dist, fade, "FadeDistance")
mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(fire, "M_SB_FXFire", "additive unlit, Tex %s, params Frame/Columns/Rows/Color/Intensity/Opacity" % ("found" if fire_tex else "MISSING"))

# ---- Smoke ----
smoke = material("M_SB_FXSmoke")
smoke.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
smoke.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
smoke.set_editor_property("two_sided", True)
node, link = builder(smoke)
frame = scalar(node, "Frame", 0.0, -1400, -300)
uv = node(unreal.MaterialExpressionTextureCoordinate, -1400, -450, u_tiling=0.6, v_tiling=0.6)
drift = node(unreal.MaterialExpressionMultiply, -1250, -300, const_b=0.04)
link(frame, drift, "A")
panned = node(unreal.MaterialExpressionAdd, -1100, -400)
link(uv, panned, "A")
link(drift, panned, "B")
noise_tex = unreal.load_asset(NOISE_TEX)
noise = node(unreal.MaterialExpressionTextureSample, -900, -400)
if noise_tex:
    noise.set_editor_property("texture", noise_tex)
link(panned, noise, "UVs")
uv2 = node(unreal.MaterialExpressionTextureCoordinate, -1400, -150, u_tiling=1.3, v_tiling=1.3)
drift2 = node(unreal.MaterialExpressionMultiply, -1250, -150, const_b=-0.025)
link(frame, drift2, "A")
panned2 = node(unreal.MaterialExpressionAdd, -1100, -150)
link(uv2, panned2, "A")
link(drift2, panned2, "B")
noise2 = node(unreal.MaterialExpressionTextureSample, -900, -150)
if noise_tex:
    noise2.set_editor_property("texture", noise_tex)
link(panned2, noise2, "UVs")
billow = node(unreal.MaterialExpressionMultiply, -650, -300)
link(noise, billow, "A", "R")
link(noise2, billow, "B", "R")
billow_lift = node(unreal.MaterialExpressionLinearInterpolate, -450, -300, const_a=0.35, const_b=1.0)
link(billow, billow_lift, "Alpha")
mask = radial_mask(node, link, 1.6, -1400, 150)
shape = node(unreal.MaterialExpressionMultiply, -250, -200)
link(mask, shape, "A")
link(billow_lift, shape, "B")
opacity = scalar(node, "Opacity", 0.7, -250, 0)
alpha = node(unreal.MaterialExpressionMultiply, -50, -150)
link(shape, alpha, "A")
link(opacity, alpha, "B")
fade = node(unreal.MaterialExpressionDepthFade, 150, -100)
link(alpha, fade, "Opacity")
fade_dist = node(unreal.MaterialExpressionConstant, 0, 50, r=80.0)
link(fade_dist, fade, "FadeDistance")
color = node(unreal.MaterialExpressionVectorParameter, -50, -450, parameter_name="Color", default_value=unreal.LinearColor(0.07, 0.065, 0.06, 1))
shade = node(unreal.MaterialExpressionMultiply, 150, -400)
link(color, shade, "A")
link(billow_lift, shade, "B")
mel.connect_material_property(shade, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
finish(smoke, "M_SB_FXSmoke", "translucent unlit, noise %s, params Frame/Color/Opacity" % ("found" if noise_tex else "MISSING"))

# ---- Scorch decal ----
scorch = material("M_SB_Scorch")
scorch.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
scorch.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
node, link = builder(scorch)
mask = radial_mask(node, link, 0.7, -1400, 0)
uv = node(unreal.MaterialExpressionTextureCoordinate, -1400, -250, u_tiling=2.0, v_tiling=2.0)
noise = node(unreal.MaterialExpressionTextureSample, -1200, -250)
if noise_tex:
    noise.set_editor_property("texture", noise_tex)
link(uv, noise, "UVs")
ragged = node(unreal.MaterialExpressionMultiply, -350, -100)
link(mask, ragged, "A")
link(noise, ragged, "B", "R")
boost = node(unreal.MaterialExpressionMultiply, -200, -100, const_b=2.2)
link(ragged, boost, "A")
clamp = node(unreal.MaterialExpressionSaturate, -50, -100)
link(boost, clamp, "")
life = node(unreal.MaterialExpressionDecalLifetimeOpacity, -50, 50)
alpha = node(unreal.MaterialExpressionMultiply, 150, -50)
link(clamp, alpha, "A")
link(life, alpha, "B")
alpha2 = node(unreal.MaterialExpressionMultiply, 300, -50, const_b=0.85)
link(alpha, alpha2, "A")
base = node(unreal.MaterialExpressionConstant3Vector, 150, -300, constant=unreal.LinearColor(0.015, 0.013, 0.012, 1))
rough = node(unreal.MaterialExpressionConstant, 150, -200, r=0.95)
mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(alpha2, "", unreal.MaterialProperty.MP_OPACITY)
finish(scorch, "M_SB_Scorch", "deferred decal, translucent, lifetime fade")

out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "GrenadeFXMaterials.txt")
with open(out, "w") as f:
    f.write("\n".join(log) + "\n")
unreal.log("wrote " + out)
