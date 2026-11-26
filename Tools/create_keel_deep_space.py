"""Keel's deep-space windows (2026-11-26): M_SB_KeelDeepSpace, a window into a real 3D scene, and MI_SB_KeelWindowGlass.

Keel's windows are panels on solid hull blocks, so geometry behind them would be hidden by the wall. Instead each pixel
of the panel casts the camera's ray out through the window into a small analytic scene, in the window's own frame
(origin the panel's centre, +Z straight out). Everything therefore moves with true perspective and parallax as the
player walks past:

  - stars at infinity (two densities, by direction) and a layer of glints 120 m out
  - a violet-teal nebula 3 km out, and thin wisps 600 m out in front of the planet
  - the gas giant: a real sphere 4 km out, lit from the left (soft terminator, limb darkening), cloud bands from
    T_SB_KeelBands slowly turning, an atmosphere rim on the lit limb and a halo just outside it
  - a small cratered moon 1.6 km out
  - drifting hull debris and two ship silhouettes 90 m and 250 m out, rim-lit (T_SB_KeelDebris)

Textures from Art/textures/make_keel_deep_space.py. MI_SB_KeelWindowGlass (a child of M_SB_TankGlass, from
Tools/create_keel_brain_tank.py) is the pane in front: faint reflections and a teal edge tint. Run with
Tools/MakeKeelDeepSpace.bat. Writes Saved/Logs/KeelDeepSpace.txt.
"""
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DEST = "/Game/Shieldbreak/Art/Keel"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

CODE = r"""
struct SBSpace
{
    float2 H22(float2 p)
    {
        p = float2(dot(p, float2(127.1, 311.7)), dot(p, float2(269.5, 183.3)));
        return frac(sin(p) * 43758.5453);
    }
    // Point stars on a jittered grid; a star never shrinks below a pixel (energy kept), so it does not shimmer.
    float Stars(float2 uv, float density, float threshold)
    {
        float2 g = uv * density;
        float2 c = floor(g);
        float2 f = frac(g) - 0.5;
        float2 h = H22(c);
        float2 j = (H22(c + 17.3) - 0.5) * 0.7;
        float px = max(length(fwidth(g)), 1e-4);
        float size = max(0.04, px * 0.9);
        float core = saturate(1.0 - length(f - j) / size);
        float energy = min(pow(0.04 / size, 2.0), 1.0);
        return step(threshold, h.x) * core * core * energy * (0.3 + 1.7 * h.y * h.y);
    }
    float3 Tint(float2 uv, float density)
    {
        return lerp(float3(0.70, 0.80, 1.00), float3(1.00, 0.86, 0.72), H22(floor(uv * density) + 3.1).x);
    }
    float2 Plane(float3 ro, float3 rd, float d)
    {
        return (ro.xy + rd.xy * ((d - ro.z) / rd.z)) / d;
    }
};
SBSpace S;

// Out of the window: whichever side of the panel the camera is not on (the face normal's sign is not relied on).
float3 F = normalize(N);
if (dot(RO, F) > 0.0) F = -F;
float3 R = normalize(cross(float3(0, 0, 1), F));
float3 U = cross(F, R);
float3 w = normalize(V);
float3 rd = float3(dot(w, R), dot(w, U), dot(w, F));
float3 ro = float3(dot(RO, R), dot(RO, U), dot(RO, F));
rd.z = max(rd.z, 0.02);
rd = normalize(rd);
float3 L = normalize(float3(-0.80, 0.45, 0.15));

float3 col = float3(0.002, 0.003, 0.008);
float2 sky = rd.xy / rd.z;
col += S.Stars(sky, 140.0, 0.92) * S.Tint(sky, 140.0) * 0.8;
col += S.Stars(sky + 3.7, 60.0, 0.965) * S.Tint(sky + 3.7, 60.0) * 1.6;
col += Texture2DSample(Neb, NebSampler, S.Plane(ro, rd, 300000.0) * 0.35 + float2(0.21, 0.63)).rgb * 0.55;

// The gas giant
float Dp = 400000.0;
float3 Cp = float3(0.30, 0.22, 1.0) * Dp;
float Rp = 0.22 * Dp;
float3 oc = ro - Cp;
float b = dot(oc, rd);
float3 q = oc - b * rd;   // stable form: the ray's closest approach to the centre
float h = Rp * Rp - dot(q, q);
float tP = 1e12;
if (h > 0.0 && b < 0.0)
{
    tP = -b - sqrt(h);
    float3 n = (ro + rd * tP - Cp) / Rp;
    float3 ax = normalize(float3(0.22, 1.0, 0.12));
    float3 e1 = normalize(cross(ax, float3(0, 0, 1)));
    float3 e2 = cross(ax, e1);
    float2 buv = float2(atan2(dot(n, e2), dot(n, e1)) / 6.2831853 + T * 0.0004, 0.5 - 0.5 * dot(n, ax));
    float3 alb = Texture2DSampleLevel(Bands, BandsSampler, buv, 0).rgb;
    float ndl = dot(n, L);
    float view = saturate(dot(n, -rd));
    float3 day = alb * smoothstep(-0.12, 0.45, ndl) * (0.55 + 0.45 * saturate(ndl));
    float3 atmo = float3(0.35, 0.55, 1.0) * pow(1.0 - view, 3.0) * smoothstep(-0.25, 0.4, ndl) * 1.4;
    col = day * (0.55 + 0.45 * sqrt(view)) * 1.25 + atmo + alb * 0.012;
}
else if (b < 0.0)
{
    float dist = length(q);
    float x = (dist - Rp) / (Rp * 0.025);
    float3 limb = normalize(ro + rd * (-b) - Cp);
    col += float3(0.30, 0.50, 1.0) * exp(-max(x, 0.0) * 2.2) * smoothstep(-0.3, 0.5, dot(limb, L)) * 0.9;
}

// The moon
float Dm = 160000.0;
float3 Cm = float3(-0.62, 0.48, 1.0) * Dm;
float Rm = 0.03 * Dm;
float3 om = ro - Cm;
float bm = dot(om, rd);
float3 qm = om - bm * rd;
float hm = Rm * Rm - dot(qm, qm);
if (hm > 0.0 && bm < 0.0)
{
    float tM = -bm - sqrt(hm);
    if (tM < tP)
    {
        float3 n = (ro + rd * tM - Cm) / Rm;
        float2 muv = float2(atan2(n.x, n.z) / 6.2831853, n.y * 0.25 + 0.25);
        float g = Texture2DSampleLevel(Neb, NebSampler, muv, 0).a;
        float3 alb = lerp(float3(0.30, 0.28, 0.27), float3(0.64, 0.60, 0.56), smoothstep(0.3, 0.7, g));
        float ndl = dot(n, L);
        col = alb * smoothstep(-0.05, 0.35, ndl) * (0.4 + 0.6 * saturate(ndl)) * 1.2 + alb * 0.01;
    }
}

// Nearer layers, each with its own parallax
col += Texture2DSample(Neb, NebSampler, S.Plane(ro, rd, 60000.0) * 0.6 + float2(0.47, 0.11)).rgb * float3(0.6, 0.9, 1.0) * 0.18;
float2 glint = S.Plane(ro, rd, 12000.0) + 7.1;
col += S.Stars(glint, 45.0, 0.985) * 1.2;
float4 far = Texture2DSample(Debris, DebrisSampler, S.Plane(ro, rd, 25000.0) * 0.22 + float2(T * 0.0015, 0.37));
col = lerp(col, float3(0.006, 0.006, 0.010) + far.g * float3(0.9, 0.75, 0.6) * 0.12, far.a);
float4 near = Texture2DSample(Debris, DebrisSampler, S.Plane(ro, rd, 9000.0) * 0.12 + float2(0.53 - T * 0.003, 0.81));
col = lerp(col, float3(0.004, 0.004, 0.007) + near.g * float3(0.9, 0.75, 0.6) * 0.18, near.a);
return col * Brightness;
"""

lines = []


def log(text):
    lines.append(text)
    unreal.log("KeelDeepSpace: " + text)


def import_texture(name, srgb, clamp=False):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(PROJECT, "Art", "Export", "Textures", name + ".png")
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(DEST + "/" + name)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(name + " was not imported")
    texture.set_editor_property("srgb", srgb)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    if clamp:
        texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    EAL.save_loaded_asset(texture)
    return texture


def build_space(neb, bands, debris):
    path = DEST + "/M_SB_KeelDeepSpace"
    m = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        "M_SB_KeelDeepSpace", DEST, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(m)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    def node(kind, x, y, **props):
        result = MEL.create_material_expression(m, kind, x, y)
        for key, value in props.items():
            result.set_editor_property(key, value)
        return result

    def link(a, b, pin, output=""):
        if not MEL.connect_material_expressions(a, output, b, pin):
            raise RuntimeError("Cannot connect %s to %s.%s" % (a.get_name(), b.get_name(), pin))

    custom = node(unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", CODE)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "KeelDeepSpace")
    names = ["V", "RO", "N", "T", "Neb", "Bands", "Debris", "Brightness"]
    inputs = []
    for name in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)

    world = node(unreal.MaterialExpressionWorldPosition, -1100, -200)
    camera = node(unreal.MaterialExpressionCameraPositionWS, -1100, -80)
    obj = node(unreal.MaterialExpressionObjectPositionWS, -1100, 40)
    view = node(unreal.MaterialExpressionSubtract, -850, -150); link(world, view, "A"); link(camera, view, "B")
    origin = node(unreal.MaterialExpressionSubtract, -850, 0); link(camera, origin, "A"); link(obj, origin, "B")
    link(view, custom, "V")
    link(origin, custom, "RO")
    link(node(unreal.MaterialExpressionVertexNormalWS, -850, 120), custom, "N")
    link(node(unreal.MaterialExpressionTime, -850, 200), custom, "T")
    for i, (pin, tex) in enumerate((("Neb", neb), ("Bands", bands), ("Debris", debris))):
        link(node(unreal.MaterialExpressionTextureObjectParameter, -850, 300 + 120 * i, parameter_name=pin, texture=tex), custom, pin)
    link(node(unreal.MaterialExpressionScalarParameter, -850, 680, parameter_name="Brightness", default_value=0.35), custom, "Brightness")
    MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("built " + path)


def build_window_glass():
    parent = unreal.load_asset(DEST + "/M_SB_TankGlass")
    if not parent:
        raise RuntimeError("M_SB_TankGlass missing: run Tools/ImportKeelBrainTank.bat first")
    path = DEST + "/MI_SB_KeelWindowGlass"
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        "MI_SB_KeelWindowGlass", DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for name, value in (("FacingOpacity", 0.035), ("EdgeOpacity", 0.35), ("EdgeTintStrength", 0.3), ("IORFacing", 1.0),
                        ("IOREdge", 1.0), ("Roughness", 0.16), ("Specular", 0.35)):
        MEL.set_material_instance_scalar_parameter_value(mi, name, value)
    MEL.set_material_instance_vector_parameter_value(mi, "GlassColor", unreal.LinearColor(0.01, 0.02, 0.03, 1))
    EAL.save_loaded_asset(mi)
    log("built " + path)


try:
    build_space(import_texture("T_SB_KeelNebula", True), import_texture("T_SB_KeelBands", True, clamp=True),
                import_texture("T_SB_KeelDebris", False))
    build_window_glass()
    log("RESULT OK")
except Exception as error:
    log("RESULT FAILED: %s" % error)
    raise
finally:
    with open(os.path.join(PROJECT, "Saved", "Logs", "KeelDeepSpace.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
