"""Checks the scanned art Tools/GetScanArt.bat copied from Electric Dreams and builds the materials the game puts on
the map blocks with it, headless (Tools/MakeScanArt.bat).

1. Every asset under /Game/Megascans and /Game/MSPresets: its packages it depends on must exist (a missing one means
   GetScanArt.bat has to copy another folder). Each static mesh's bounds and material slots are listed, since the game
   fits rocks to blocks by their bounds.
2. M_SB_Scan (rebuilt each run): two scanned surfaces projected in world space, so stretched cubes keep even texels.
   The side layer covers every face (three projections blended by the normal); the top layer (one projection, from
   above) covers faces that look up, its own height map breaking up the edge, so blocks read as stone with moss or
   earth on top. Tint / TopTint multiply each layer. Instances MI_SB_Scan_<Kind> (see SURFACES) pick the textures.

Writes Saved/Logs/ScanArt.txt.
"""
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
MATERIALS = "/Game/Shieldbreak/Art/Scan"
SURF = "/Game/Megascans/Surfaces"
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
lines = []

# Kind -> side surface, top surface, side / top tile (cm), top threshold (normal Z where the top layer starts),
# top coverage (how much of an upward face the top layer takes: 1.5 all of it, 0.5 broken patches), side
# desaturation (0 keeps the scan's colour, 1 makes it grey) and side tint.
SURFACES = {
    "Ground": dict(side="MossyRockyGround", top="MossyGrass", side_tile=400.0, top_tile=350.0, threshold=0.35, coverage=1.6, desat=0.0, tint=(1, 1, 1),
                   top_desat=0.3, top_tint=(0.64, 0.72, 0.55)),
    "Stone": dict(side="IcelandicQuarryRock", top="NordicMoss", side_tile=520.0, top_tile=320.0, threshold=0.75, coverage=0.45, desat=0.8, tint=(1.0, 0.98, 0.94)),
    "Path": dict(side="IcelandicQuarryRock", top="MossyRockyGround", side_tile=480.0, top_tile=420.0, threshold=0.75, coverage=1.6, desat=0.8, tint=(0.92, 0.9, 0.86),
                 top_desat=0.7, top_tint=(0.85, 0.84, 0.8)),
    "Dark": dict(side="BeachCliff", top="NordicMoss", side_tile=420.0, top_tile=320.0, threshold=0.72, coverage=0.55, desat=0.75, tint=(0.7, 0.7, 0.67),
                 top_desat=0.45, top_tint=(0.55, 0.6, 0.48)),
}
TOP_DEFAULT = "MossyGrass"
ROOTS = ["/Game/Megascans", "/Game/MSPresets", "/Game/Custom", "/Game/PhysicalMaterials", "/Game/Meshes"]

def log(text):
    lines.append(text)
    unreal.log("ScanArt: " + text)


def check_dependencies():
    """Walks every hard reference out of the copied folders, through to everything they reach, and logs each package
    that is not there (GetScanArt.bat must copy its folder)."""
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False, include_hard_package_references=True,
                                                    include_searchable_names=False, include_soft_management_references=False,
                                                    include_hard_management_references=False)
    roots = [r for r in ROOTS if eal.does_directory_exist(r)]
    registry.scan_paths_synchronous(roots, True)
    assets = []
    for root in roots:
        assets += registry.get_assets_by_path(root, recursive=True)
    missing = {}
    seen = set()
    queue = [str(data.package_name) for data in assets]
    while queue:
        package = queue.pop()
        if package in seen:
            continue
        seen.add(package)
        for dep in registry.get_dependencies(package, options) or []:
            dep = str(dep)
            if not dep.startswith("/Game/"):
                continue
            if eal.does_asset_exist(dep):
                queue.append(dep)
            else:
                missing.setdefault(dep, []).append(package)
    log("%d scanned assets, %d packages reached" % (len(assets), len(seen)))
    for dep, users in sorted(missing.items()):
        log("MISSING %s (used by %s%s)" % (dep, users[0], " and %d more" % (len(users) - 1) if len(users) > 1 else ""))
    log("%d missing packages" % len(missing))
    return [a for a in assets if str(a.package_name).startswith("/Game/Megascans")]


def describe_meshes(assets):
    for data in assets:
        if str(data.asset_class_path.asset_name) != "StaticMesh":
            continue
        path = str(data.package_name)
        mesh = unreal.load_asset(path)
        if not mesh:
            log("MESH %s did not load" % path)
            continue
        box = mesh.get_bounding_box()
        lo, hi = box.min, box.max
        nanite = mesh.get_editor_property("nanite_settings").enabled
        slots = []
        for m in mesh.static_materials:
            chain, mi = [], m.material_interface
            while mi is not None and len(chain) < 4:
                chain.append(mi.get_name())
                mi = mi.get_editor_property("parent") if isinstance(mi, unreal.MaterialInstance) else None
            slots.append("%s=%s" % (m.material_slot_name, ">".join(chain)))
        log("MESH %s min (%.0f %.0f %.0f) max (%.0f %.0f %.0f) nanite %s slots %s" % (
            path, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z, nanite, ",".join(slots)))


def texture(surface, suffix):
    path = "%s/%s/T_%s_01_%s" % (SURF, surface, surface, suffix)
    tex = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if not tex:
        log("MISSING texture %s" % path)
    return tex


def sampler_for(tex, fallback):
    """The sampler type a texture's own compression and colour space need (else the material does not compile)."""
    if not tex:
        return fallback
    if tex.get_editor_property("virtual_texture_streaming"):
        compression = tex.get_editor_property("compression_settings")
        if compression == unreal.TextureCompressionSettings.TC_NORMALMAP:
            return unreal.MaterialSamplerType.SAMPLERTYPE_VIRTUAL_NORMAL
        if compression == unreal.TextureCompressionSettings.TC_MASKS:
            return unreal.MaterialSamplerType.SAMPLERTYPE_VIRTUAL_MASKS
        return unreal.MaterialSamplerType.SAMPLERTYPE_VIRTUAL_COLOR if tex.get_editor_property("srgb") else unreal.MaterialSamplerType.SAMPLERTYPE_VIRTUAL_LINEAR_COLOR
    compression = tex.get_editor_property("compression_settings")
    if compression == unreal.TextureCompressionSettings.TC_NORMALMAP:
        return unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    if compression == unreal.TextureCompressionSettings.TC_MASKS:
        return unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    if compression == unreal.TextureCompressionSettings.TC_GRAYSCALE:
        return unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE if tex.get_editor_property("srgb") else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE
    return unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if tex.get_editor_property("srgb") else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR


def scan_material():
    path = MATERIALS + "/M_SB_Scan"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_SB_Scan", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y, **props):
        n = mel.create_material_expression(mat, cls, x, y)
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def link(a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            log("M_SB_Scan: could not connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))

    def scalar(name, value, x, y):
        return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)

    def binary(cls, a, b, x, y, a_out="", b_out=""):
        n = node(cls, x, y)
        link(a, a_out, n, "A")
        link(b, b_out, n, "B")
        return n

    shared = getattr(unreal.SamplerSourceMode, "SSM_WRAP_WORLD_GROUP_SETTINGS", None)
    kinds = (("BC", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "BeachCliff", "BC"),
             ("N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, "BeachCliff", "N"),
             ("ARD", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, "BeachCliff", "AoRDp"))
    position = node(unreal.MaterialExpressionWorldPosition, -2400, 0)
    tiles = {}

    def projected(layer, tile_name, tile_default, projections, y0):
        """Samples a layer's three maps along the given projections (masks of world X, Y, Z); returns {map: [samples]}."""
        tile = scalar(tile_name, tile_default, -2400, y0 + 150)
        tiles[layer] = tile
        uvw = binary(unreal.MaterialExpressionDivide, position, tile, -2200, y0)
        out = {}
        for k, (suffix, sampler, surface, file_suffix) in enumerate(kinds):
            default = texture(surface if layer == "Side" else TOP_DEFAULT, file_suffix)
            sampler = sampler_for(default, sampler)
            log("%s%s: %s, sampler %s" % (layer, suffix, default.get_name() if default else None, sampler))
            out[suffix] = []
            for i, (r, g, b) in enumerate(projections):
                mask = node(unreal.MaterialExpressionComponentMask, -2000, y0 + 120 * i, r=r, g=g, b=b, a=False)
                link(uvw, "", mask, "")
                sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -1700, y0 + 300 * (3 * k + i),
                              parameter_name=layer + suffix, texture=default, sampler_type=sampler)
                if shared is not None:
                    sample.set_editor_property("sampler_source", shared)
                link(mask, "", sample, "UVs")
                out[suffix].append(sample)
        return out

    side = projected("Side", "SideTile", 350.0, ((False, True, True), (True, False, True), (True, True, False)), -1500)
    top = projected("Top", "TopTile", 300.0, ((True, True, False),), 1800)
    top_tile_node = tiles["Top"]

    # Side blend weights: |normal|^4, normalized.
    normal = node(unreal.MaterialExpressionVertexNormalWS, -1700, 3000)
    absolute = node(unreal.MaterialExpressionAbs, -1550, 3000)
    link(normal, "", absolute, "")
    power = node(unreal.MaterialExpressionPower, -1400, 3000, const_exponent=4.0)
    link(absolute, "", power, "Base")
    ones = node(unreal.MaterialExpressionConstant3Vector, -1400, 3150, constant=unreal.LinearColor(1, 1, 1, 1))
    total = binary(unreal.MaterialExpressionDotProduct, power, ones, -1250, 3100)
    weights = binary(unreal.MaterialExpressionDivide, power, total, -1100, 3000)
    channel = []
    for i, c in enumerate("RGB"):
        m = node(unreal.MaterialExpressionComponentMask, -950, 2900 + 120 * i, r=c == "R", g=c == "G", b=c == "B", a=False)
        link(weights, "", m, "")
        channel.append(m)

    def blend3(samples, out_pin, x, y):
        acc = None
        for i, sample in enumerate(samples):
            w = binary(unreal.MaterialExpressionMultiply, sample, channel[i], x, y + 150 * i, a_out=out_pin)
            acc = w if acc is None else binary(unreal.MaterialExpressionAdd, acc, w, x + 150, y + 150 * i)
        return acc

    side_bc = blend3(side["BC"], "RGB", -800, -1500)
    side_n = blend3(side["N"], "RGB", -800, -600)
    side_ard = blend3(side["ARD"], "RGB", -800, 300)

    # Top mask: faces whose normal Z passes TopThreshold, the edge broken up by the top layer's height map.
    nz = node(unreal.MaterialExpressionComponentMask, -1550, 3400, r=False, g=False, b=True, a=False)
    link(normal, "", nz, "")
    threshold = scalar("TopThreshold", 0.8, -1550, 3550)
    above = binary(unreal.MaterialExpressionSubtract, nz, threshold, -1400, 3400)
    sharp = node(unreal.MaterialExpressionMultiply, -1250, 3400, const_b=12.0)
    link(above, "", sharp, "A")
    height = node(unreal.MaterialExpressionComponentMask, -1250, 3550, r=False, g=False, b=True, a=False)
    link(top["ARD"][0], "RGB", height, "")
    breakup = scalar("TopBreakup", 1.5, -1250, 3700)
    centered = node(unreal.MaterialExpressionSubtract, -1100, 3550, const_b=0.5)
    link(height, "", centered, "A")
    jitter = binary(unreal.MaterialExpressionMultiply, centered, breakup, -950, 3550)
    raw = binary(unreal.MaterialExpressionAdd, sharp, jitter, -800, 3450)
    facing = node(unreal.MaterialExpressionSaturate, -650, 3450)
    link(raw, "", facing, "")
    # Patches: the top layer's height map again at four times its tile, so moss sits in broad drifts on wall tops
    # instead of carpeting them. TopCoverage 1.5+ covers everything.
    patch_tile = node(unreal.MaterialExpressionMultiply, -2400, 3900, const_b=4.0)
    link(top_tile_node, "", patch_tile, "A")
    patch_uvw = binary(unreal.MaterialExpressionDivide, position, patch_tile, -2200, 3900)
    patch_xy = node(unreal.MaterialExpressionComponentMask, -2000, 3900, r=True, g=True, b=False, a=False)
    link(patch_uvw, "", patch_xy, "")
    top_ard_default = texture(TOP_DEFAULT, "AoRDp")
    patch_sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -1700, 3900, parameter_name="TopARD",
                        texture=top_ard_default, sampler_type=sampler_for(top_ard_default, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR))
    if shared is not None:
        patch_sample.set_editor_property("sampler_source", shared)
    link(patch_xy, "", patch_sample, "UVs")
    patch_h = node(unreal.MaterialExpressionComponentMask, -1400, 3900, r=False, g=False, b=True, a=False)
    link(patch_sample, "RGB", patch_h, "")
    coverage = scalar("TopCoverage", 1.6, -1400, 4050)
    patch_raw = binary(unreal.MaterialExpressionAdd, patch_h, coverage, -1250, 3900)
    patch_off = node(unreal.MaterialExpressionSubtract, -1100, 3900, const_b=1.0)
    link(patch_raw, "", patch_off, "A")
    contrast = scalar("TopContrast", 6.0, -1100, 4050)
    patch_c = binary(unreal.MaterialExpressionMultiply, patch_off, contrast, -950, 3900)
    patch_mid = node(unreal.MaterialExpressionAdd, -800, 3900, const_b=0.5)
    link(patch_c, "", patch_mid, "A")
    patch = node(unreal.MaterialExpressionSaturate, -650, 3900)
    link(patch_mid, "", patch, "")
    mask = binary(unreal.MaterialExpressionMultiply, facing, patch, -500, 3600)

    def lerp(a, b, x, y, b_out="RGB"):
        n = node(unreal.MaterialExpressionLinearInterpolate, x, y)
        link(a, "", n, "A")
        link(b, b_out, n, "B")
        link(mask, "", n, "Alpha")
        return n

    tint = node(unreal.MaterialExpressionVectorParameter, -800, -1800, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
    top_tint = node(unreal.MaterialExpressionVectorParameter, -800, -1950, parameter_name="TopTint", default_value=unreal.LinearColor(1, 1, 1, 1))
    desat = node(unreal.MaterialExpressionDesaturation, -650, -1500)
    link(side_bc, "", desat, "")
    link(scalar("SideDesaturation", 0.0, -800, -1350), "", desat, "Fraction")
    side_col = binary(unreal.MaterialExpressionMultiply, desat, tint, -450, -1600)
    top_desat = node(unreal.MaterialExpressionDesaturation, -650, -1950)
    link(top["BC"][0], "RGB", top_desat, "")
    link(scalar("TopDesaturation", 0.0, -800, -2100), "", top_desat, "Fraction")
    top_col = binary(unreal.MaterialExpressionMultiply, top_desat, top_tint, -450, -1800)
    base = lerp(side_col, top_col, -250, -1600, b_out="")
    normal_out = lerp(side_n, top["N"][0], -250, -600)
    ard = lerp(side_ard, top["ARD"][0], -250, 300)
    ao = node(unreal.MaterialExpressionComponentMask, -100, 300, r=True, g=False, b=False, a=False)
    link(ard, "", ao, "")
    rough = node(unreal.MaterialExpressionComponentMask, -100, 450, r=False, g=True, b=False, a=False)
    link(ard, "", rough, "")
    rough_scale = scalar("RoughnessScale", 1.0, -100, 600)
    roughness = binary(unreal.MaterialExpressionMultiply, rough, rough_scale, 50, 450)

    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal_out, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("built %s (%d expressions)" % (path, mel.get_num_material_expressions(mat)))
    return mat


def scan_instances(master):
    defaults = {"Side": "BeachCliff", "Top": TOP_DEFAULT}
    for kind, look in SURFACES.items():
        path = "%s/MI_SB_Scan_%s" % (MATERIALS, kind)
        if eal.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = tools.create_asset("MI_SB_Scan_" + kind, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        mel.clear_all_material_instance_parameters(mi)
        used = []
        for layer, surface in (("Side", look["side"]), ("Top", look["top"])):
            for suffix, file_suffix in (("BC", "BC"), ("N", "N"), ("ARD", "AoRDp")):
                tex = texture(surface, file_suffix)
                default = texture(defaults[layer], file_suffix)
                if tex and default and tex.get_editor_property("virtual_texture_streaming") != default.get_editor_property("virtual_texture_streaming"):
                    log("%s: %s%s %s is %s virtual-textured than the default; keeping %s" % (
                        kind, layer, suffix, tex.get_name(), "more" if tex.get_editor_property("virtual_texture_streaming") else "less", default.get_name()))
                    tex = None
                if tex:
                    mel.set_material_instance_texture_parameter_value(mi, layer + suffix, tex)
                used.append(tex.get_name() if tex else "(default %s)" % defaults[layer])
        for name, value in (("SideTile", look["side_tile"]), ("TopTile", look["top_tile"]), ("TopThreshold", look["threshold"]),
                            ("TopCoverage", look["coverage"]), ("SideDesaturation", look["desat"])):
            mel.set_material_instance_scalar_parameter_value(mi, name, value)
        r, g, b = look["tint"]
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(r, g, b, 1.0))
        r, g, b = look.get("top_tint", (1, 1, 1))
        mel.set_material_instance_vector_parameter_value(mi, "TopTint", unreal.LinearColor(r, g, b, 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "TopDesaturation", look.get("top_desat", 0.0))
        eal.save_asset(path)
        log("%s: %s; %s" % (path, ", ".join(used), ", ".join("%s %s" % kv for kv in look.items() if kv[0] not in ("side", "top"))))


def describe_foliage():
    """Lists the tree leaf materials' parameters and values (the leaves render very dark under Lumen)."""
    for path in ("/Game/Megascans/3D_Plants/EuropeanHornbeam/MI_EuropeanHornbeam_01",
                 "/Game/Megascans/3D_Plants/EuropeanHornbeam/MI_EuropeanHornbeam_TwoSided"):
        if not eal.does_asset_exist(path):
            found = [a for a in eal.list_assets("/Game/Megascans/3D_Plants/EuropeanHornbeam", recursive=True) if a.split(".")[0].endswith(path.rsplit("/", 1)[1])]
            if not found:
                log("FOLIAGE %s not found" % path)
                continue
            path = found[0].split(".")[0]
        mi = unreal.load_asset(path)
        for getter, kind in ((mel.get_scalar_parameter_names, "scalar"), (mel.get_vector_parameter_names, "vector"),
                             (mel.get_texture_parameter_names, "texture"), (mel.get_static_switch_parameter_names, "switch")):
            try:
                names = getter(mi)
            except Exception as error:  # noqa: BLE001
                log("FOLIAGE %s %s: %s" % (path, kind, error))
                continue
            values = []
            for name in names:
                try:
                    if kind == "scalar":
                        value = "%.3f" % mel.get_material_instance_scalar_parameter_value(mi, name)
                    elif kind == "vector":
                        c = mel.get_material_instance_vector_parameter_value(mi, name)
                        value = "(%.2f %.2f %.2f)" % (c.r, c.g, c.b)
                    elif kind == "texture":
                        t = mel.get_material_instance_texture_parameter_value(mi, name)
                        value = t.get_name() if t else "-"
                    else:
                        value = str(mel.get_material_instance_static_switch_parameter_value(mi, name))
                except Exception:  # noqa: BLE001
                    value = "?"
                values.append("%s=%s" % (name, value))
            log("FOLIAGE %s %s: %s" % (path, kind, ", ".join(values)))


def leaf_instance():
    """MI_SB_Leaves: the hornbeam leaves with more light through them and a lighter albedo (the stock ones read near
    black in shade). The game puts it on the trees' TwoSided slot."""
    parents = [a for a in eal.list_assets("/Game/Megascans/3D_Plants/EuropeanHornbeam", recursive=True)
               if a.split(".")[0].endswith("/SimpleWind/MI_EuropeanHornbeam_TwoSided")]
    if not parents:
        log("MI_SB_Leaves: no MI_EuropeanHornbeam_TwoSided")
        return
    parent = unreal.load_asset(parents[0].split(".")[0])
    path = "%s/MI_SB_Leaves" % MATERIALS
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = tools.create_asset("MI_SB_Leaves", MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    for name, value in (("LightInfluence Subsurface Strength", 1.2), ("LightInfluence BaseColor Strength", 1.4),
                        ("Specular Leaves", 0.5), ("Backside Specular", 0.3)):
        mel.set_material_instance_scalar_parameter_value(mi, name, value)
    for name, (r, g, b) in (("Albedo Controls", (1.5, 1.15, 1.0)), ("Translucency Controls", (1.6, 1.3, 1.0)),
                            ("Albedo Tint Leaves", (1.25, 1.4, 1.05))):
        mel.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(r, g, b, 1.0))
    eal.save_asset(path)
    log("%s: parent %s" % (path, parent.get_path_name()))


def main():
    try:
        assets = check_dependencies()
        describe_meshes(assets)
        describe_foliage()
        leaf_instance()
        scan_instances(scan_material())
        log("RESULT ok")
    except Exception as error:  # noqa: BLE001 - report anything into the log the cloud reads
        log("RESULT failed: %s" % error)
        raise
    finally:
        out = os.path.join(PROJECT, "Saved", "Logs", "ScanArt.txt")
        with open(out, "w") as f:
            f.write("\n".join(lines) + "\n")


main()
