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
    "Ground": dict(side="MossyRockyGround", top="MossyGrass", side_tile=400.0, top_tile=350.0, threshold=0.35, coverage=0.62, desat=0.0, tint=(1, 1, 1), patch_noise=1.0,
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


SET_FOLDERS = ["CanyonSandstoneRidge", "MassiveCanyonSandstoneMesa", "HugeSandstoneCliff", "MassiveSandstoneCliff",
               "SandstoneBoulder", "SandstoneRockyGround", "CanyonErodedRockyGround", "DryRiverBed", "MassiveTundraRockFormation",
               "TundraMossyBoulder", "TundraRockyGround", "NordicBoulder", "NordicBeachRock", "NordicBeachRocks",
               "NordicBeachRockyGround", "MassiveNordicCoastalCliff", "SmoothRockShelf"]


def describe_textures():
    """Lists the textures of the Creekbed / Bastion scan sets (their surfaces are built from these)."""
    for root in ["/Game/Megascans/3D_Assets/" + f for f in SET_FOLDERS] + ["/Game/Megascans/Decals/AridDebris"]:
        folder = root.rsplit("/", 1)[1]
        if not eal.does_directory_exist(root):
            log("SET %s missing" % folder)
            continue
        for data in registry.get_assets_by_path(root, recursive=True):
            cls = str(data.asset_class_path.asset_name)
            if cls.startswith("Texture"):
                tex = unreal.load_asset(str(data.package_name))
                log("TEX %s %dx%d srgb %s vt %s" % (data.package_name, tex.blueprint_get_size_x(), tex.blueprint_get_size_y(),
                                                     tex.get_editor_property("srgb"), tex.get_editor_property("virtual_texture_streaming")))


# Suffixes a scanned 3D asset's textures may use for each map (Megascans exports vary between packs).
ASSET_SUFFIXES = {"BC": ("_BC", "_D", "_Albedo", "_BaseColor"), "N": ("_N", "_Normal"),
                  "AoRDp": ("_AoRDp", "_ORDp", "_ARD", "_ORD", "_AORD", "_ORDS", "_DpR", "_DpRF")}


def is_dpr(surface):
    """Whether a surface's packed map is Electric Dreams' DpR / DpRF (R displacement, G roughness; no AO channel)
    rather than the surfaces' AoRDp (R AO, G roughness, B displacement)."""
    tex = texture(surface, "AoRDp")
    return bool(tex) and "_DpR" in tex.get_name()


def texture(surface, suffix):
    """A surface's map: a Megascans surface by name (Surfaces/<name>/T_<name>_01_<suffix>), or "asset:<Folder>" for the
    textures of a scanned 3D asset in 3D_Assets/<Folder> (the first texture whose name ends in one of the suffixes)."""
    if surface.startswith("asset:"):
        folder = "/Game/Megascans/3D_Assets/" + surface[6:]
        names = sorted(str(d.package_name) for d in registry.get_assets_by_path(folder, recursive=True)
                       if str(d.asset_class_path.asset_name).startswith("Texture"))
        for ending in ASSET_SUFFIXES.get(suffix, ("_" + suffix,)):
            for name in names:
                if name.endswith(ending):
                    return unreal.load_asset(name)
        log("MISSING texture %s %s (has %s)" % (folder, suffix, ", ".join(n.rsplit("/", 1)[1] for n in names)))
        return None
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


def scan_material(name="M_SB_Scan", side_default="BeachCliff", top_default=None):
    """The world-projected scan master. The terrain sets build one each with their own textures as the defaults, so
    their samplers match their textures (a virtual-textured default cannot take a plain texture, or the reverse)."""
    top_default = top_default or TOP_DEFAULT
    side_dpr, top_dpr = is_dpr(side_default), is_dpr(top_default)
    path = MATERIALS + "/" + name
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y, **props):
        n = mel.create_material_expression(mat, cls, x, y)
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def link(a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            log(name + ": could not connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))

    def scalar(name, value, x, y):
        return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)

    def binary(cls, a, b, x, y, a_out="", b_out=""):
        n = node(cls, x, y)
        link(a, a_out, n, "A")
        link(b, b_out, n, "B")
        return n

    shared = getattr(unreal.SamplerSourceMode, "SSM_WRAP_WORLD_GROUP_SETTINGS", None)
    kinds = (("BC", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, side_default, "BC"),
             ("N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, side_default, "N"),
             ("ARD", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, side_default, "AoRDp"))
    position = node(unreal.MaterialExpressionWorldPosition, -2400, 0)
    tiles = {}

    def projected(layer, tile_name, tile_default, projections, y0):
        """Samples a layer's three maps along the given projections (masks of world X, Y, Z); returns {map: [samples]}."""
        tile = scalar(tile_name, tile_default, -2400, y0 + 150)
        tiles[layer] = tile
        uvw = binary(unreal.MaterialExpressionDivide, position, tile, -2200, y0)
        out = {}
        for k, (suffix, sampler, surface, file_suffix) in enumerate(kinds):
            default = texture(surface if layer == "Side" else top_default, file_suffix)
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
    height = node(unreal.MaterialExpressionComponentMask, -1250, 3550, r=top_dpr, g=False, b=not top_dpr, a=False)
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
    top_ard_default = texture(top_default, "AoRDp")
    patch_sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -1700, 3900, parameter_name="TopARD",
                        texture=top_ard_default, sampler_type=sampler_for(top_ard_default, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR))
    if shared is not None:
        patch_sample.set_editor_property("sampler_source", shared)
    link(patch_xy, "", patch_sample, "UVs")
    patch_tex = node(unreal.MaterialExpressionComponentMask, -1550, 3900, r=top_dpr, g=False, b=not top_dpr, a=False)
    link(patch_sample, "RGB", patch_tex, "")
    # Or (PatchNoise 1) broad world-space noise, for a top layer whose height map is too even to break up: the
    # ground's grass, which otherwise flips from all grass to all earth at one coverage value.
    noise = node(unreal.MaterialExpressionNoise, -1700, 4200, scale=0.0005, levels=3, output_min=0.0, output_max=1.0, quality=1)
    link(position, "", noise, "Position")
    patch_h = node(unreal.MaterialExpressionLinearInterpolate, -1400, 3900)
    link(patch_tex, "", patch_h, "A")
    link(noise, "", patch_h, "B")
    link(scalar("PatchNoise", 0.0, -1550, 4350), "", patch_h, "Alpha")
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
    top_tinted = binary(unreal.MaterialExpressionMultiply, top_desat, top_tint, -450, -1800)
    # TopLift pulls the top layer toward TopLiftColor (snow from a gravel scan keeps its relief but not its grey).
    top_col = node(unreal.MaterialExpressionLinearInterpolate, -350, -1950)
    link(top_tinted, "", top_col, "A")
    link(node(unreal.MaterialExpressionVectorParameter, -500, -2100, parameter_name="TopLiftColor",
              default_value=unreal.LinearColor(0.86, 0.89, 0.94, 1)), "", top_col, "B")
    link(scalar("TopLift", 0.0, -500, -2250), "", top_col, "Alpha")
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
    if not (side_dpr or top_dpr):
        mel.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    # The scanned rocks, cliffs and stones are Nanite meshes: without this flag they fall back to the default material.
    nanite = getattr(unreal.MaterialUsage, "MATUSAGE_NANITE", None)
    if nanite is not None:
        mel.set_material_usage(mat, nanite)
    else:
        mat.set_editor_property("used_with_nanite", True)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("built %s (%d expressions, side %s, top %s)" % (path, mel.get_num_material_expressions(mat),
                                                       "DpR" if side_dpr else "AoRDp", "DpR" if top_dpr else "AoRDp"))
    return mat


# The outdoor terrain sets (AShieldbreakArena::BuildTerrainScan): MI_SB_Scan_<Set><Kind>, each on its own master
# (M_SB_Scan_<Set><Kind>) built with its own textures. "asset:<Folder>" takes a scanned 3D asset's textures.
# Snow on Bastion is the tundra ground's own scan, fully desaturated and lifted: packed snow with the ground's relief.
SNOW = dict(top_desat=1.0, top_tint=(1.3, 1.33, 1.4), top_lift=0.72)
TERRAIN = {
    "CanyonGround": dict(side="asset:CanyonErodedRockyGround", top="asset:SandstoneRockyGround", side_tile=450.0, top_tile=420.0,
                         threshold=0.35, coverage=1.6, desat=0.0, tint=(1.0, 0.95, 0.9), patch_noise=1.0),
    "CanyonRock": dict(side="asset:HugeSandstoneCliff", top="asset:SandstoneRockyGround", side_tile=650.0, top_tile=420.0,
                       threshold=0.8, coverage=0.55, desat=0.0, tint=(1.0, 1.0, 1.0)),
    # Full snow cover on the ground: at 0.8 the bare-earth patches showed through as brown streaks all over the snow.
    "TundraGround": dict(side="asset:TundraRockyGround", top="asset:TundraRockyGround", side_tile=420.0, top_tile=520.0,
                         threshold=0.35, coverage=1.6, desat=0.6, tint=(0.9, 0.92, 0.95), patch_noise=1.0, **SNOW),
    "TundraRock": dict(side="asset:MassiveTundraRockFormation", top="asset:TundraRockyGround", side_tile=600.0, top_tile=420.0,
                       threshold=0.62, coverage=0.75, desat=0.3, tint=(0.85, 0.88, 0.92), **SNOW),
    "TundraStone": dict(side="IcelandicQuarryRock", top="asset:TundraRockyGround", side_tile=480.0, top_tile=420.0,
                        threshold=0.75, coverage=1.0, desat=0.85, tint=(0.95, 0.97, 1.0), **SNOW),
    # Creek (Creekbed's 2026-11-02 rework): a green valley. Grass and earth underfoot, tan sandstone with grass on top.
    "CreekGround": dict(side="MossyRockyGround", top="MossyGrass", side_tile=420.0, top_tile=360.0, threshold=0.35,
                        coverage=0.6, desat=0.0, tint=(1.0, 0.97, 0.92), patch_noise=1.0, top_desat=0.25,
                        top_tint=(0.74, 0.84, 0.56)),
    "CreekRock": dict(side="asset:HugeSandstoneCliff", top="MossyGrass", side_tile=650.0, top_tile=360.0,
                      threshold=0.55, coverage=0.85, desat=0.15, tint=(1.08, 1.02, 0.94), top_desat=0.1,
                      top_tint=(0.82, 0.95, 0.62)),
}


def terrain_sets():
    for kind, look in TERRAIN.items():
        if not texture(look["side"], "BC") or not texture(look["top"], "BC"):
            log("TERRAIN %s skipped: textures missing" % kind)
            continue
        master = scan_material("M_SB_Scan_" + kind, look["side"], look["top"])
        scan_instances(master, {kind: look}, {"Side": look["side"], "Top": look["top"]})


def scan_instances(master, surfaces=None, defaults=None):
    defaults = defaults or {"Side": "BeachCliff", "Top": TOP_DEFAULT}
    for kind, look in (surfaces or SURFACES).items():
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
        mel.set_material_instance_scalar_parameter_value(mi, "PatchNoise", look.get("patch_noise", 0.0))
        mel.set_material_instance_scalar_parameter_value(mi, "TopLift", look.get("top_lift", 0.0))
        eal.save_asset(path)
        log("%s: %s; %s" % (path, ", ".join(used), ", ".join("%s %s" % kv for kv in look.items() if kv[0] not in ("side", "top"))))


def describe_foliage():
    """Lists the tree leaf materials' parameters and values (the leaves render very dark under Lumen)."""
    for path in ("/Game/Megascans/3D_Plants/EuropeanHornbeam/MI_EuropeanHornbeam_01",
                 "/Game/Megascans/3D_Plants/EuropeanHornbeam/MI_EuropeanHornbeam_TwoSided",
                 "/Game/Megascans/3D_Plants/KikuyuGrass/MI_KikuyuGrass_01"):
        if not eal.does_asset_exist(path):
            found = [a for a in eal.list_assets(path.rsplit("/", 1)[0], recursive=True) if a.split(".")[0].endswith(path.rsplit("/", 1)[1])]
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


def grass_instance():
    """MI_SB_Grass: the Kikuyu grass a little lighter and with more light through the blades (it reads dark brown
    on the ground). Only parameters the parent has are set; the game puts it on the grass tufts."""
    found = [a for a in eal.list_assets("/Game/Megascans/3D_Plants/KikuyuGrass", recursive=True)
             if a.split(".")[0].endswith("/MI_KikuyuGrass_01")]
    if not found:
        log("MI_SB_Grass: no MI_KikuyuGrass_01")
        return
    parent = unreal.load_asset(found[0].split(".")[0])
    scalars = set(str(n) for n in mel.get_scalar_parameter_names(parent))
    vectors = set(str(n) for n in mel.get_vector_parameter_names(parent))
    path = "%s/MI_SB_Grass" % MATERIALS
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = tools.create_asset("MI_SB_Grass", MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    applied = []
    for name, value in (("LightInfluence Subsurface Strength", 1.0), ("LightInfluence BaseColor Strength", 1.3),
                        ("Specular", 0.3), ("Specular Leaves", 0.3)):
        if name in scalars:
            mel.set_material_instance_scalar_parameter_value(mi, name, value)
            applied.append(name)
    for name, (r, g, b) in (("Albedo Controls", (2.2, 1.0, 1.3)), ("Albedo Tint", (1.1, 1.4, 0.8)),
                            ("Translucency Controls", (1.5, 1.3, 1.0))):
        if name in vectors:
            mel.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(r, g, b, 1.0))
            applied.append(name)
    eal.save_asset(path)
    log("%s: parent %s, set %s" % (path, parent.get_path_name(), ", ".join(applied) or "nothing"))


def grass_material():
    """M_SB_Grass: our own masked, two-sided grass material on the Kikuyu grass's albedo. The Megascans foliage
    master renders near black in this project (like the scanned rocks' own), whatever its instance parameters."""
    found = [a.split(".")[0] for a in eal.list_assets("/Game/Megascans/3D_Plants/KikuyuGrass", recursive=True)]
    albedo_path = next((a for a in found if a.endswith("/T_KikuyuGrass_01_BC")), None)
    art_path = next((a for a in found if a.endswith("/T_KikuyuGrass_01_AoRT")), None)
    if not albedo_path:
        log("M_SB_Grass: no T_KikuyuGrass_01_BC")
        return
    albedo = unreal.load_asset(albedo_path)
    art = unreal.load_asset(art_path) if art_path else None
    albedo_alpha = not albedo.get_editor_property("compression_no_alpha")
    path = MATERIALS + "/M_SB_Grass"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_SB_Grass", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("opacity_mask_clip_value", 0.4)

    def node(cls, x, y, **props):
        n = mel.create_material_expression(mat, cls, x, y)
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    bc = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, 0, parameter_name="Albedo", texture=albedo,
              sampler_type=sampler_for(albedo, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR))
    tint = node(unreal.MaterialExpressionVectorParameter, -900, 300, parameter_name="Tint", default_value=unreal.LinearColor(0.8, 0.95, 0.6, 1.0))
    bright = node(unreal.MaterialExpressionScalarParameter, -900, 450, parameter_name="Brightness", default_value=0.8)
    tinted = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, 100)
    mel.connect_material_expressions(bc, "RGB", tinted, "A")
    mel.connect_material_expressions(tint, "", tinted, "B")
    base = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 150)
    mel.connect_material_expressions(tinted, "", base, "A")
    mel.connect_material_expressions(bright, "", base, "B")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if albedo_alpha or not art:
        mel.connect_material_property(bc, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
        source = "albedo alpha"
    else:
        art_node = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, 650, parameter_name="AoRT", texture=art,
                        sampler_type=sampler_for(art, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR))
        mel.connect_material_property(art_node, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
        source = "AoRT alpha"
    rough = node(unreal.MaterialExpressionConstant, -400, 400, r=0.85)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    nanite = getattr(unreal.MaterialUsage, "MATUSAGE_NANITE", None)
    if nanite is not None:
        mel.set_material_usage(mat, nanite)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("%s: albedo %s, opacity from %s" % (path, albedo.get_name(), source))


def water_material():
    """M_SB_Water: opaque, two-sided running water for the lush terrain sets. World-projected (flat: X/Y; Vertical 1:
    along the sheet and Z, for a waterfall), two scrolling copies of a scanned ground's normal map as ripples, foam
    from its albedo. Instances: MI_SB_Water (the creek, slow, glassy) and MI_SB_Waterfall (fast, rough, white)."""
    surface = "MossyRockyGround"
    normal_tex, albedo_tex = texture(surface, "N"), texture(surface, "BC")
    if not normal_tex or not albedo_tex:
        log("M_SB_Water: textures missing")
        return
    path = MATERIALS + "/M_SB_Water"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_SB_Water", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("two_sided", True)

    def node(cls, x, y, **props):
        n = mel.create_material_expression(mat, cls, x, y)
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def link(a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            log("M_SB_Water: could not connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))

    def scalar(name, value, x, y):
        return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)

    def binary(cls, a, b, x, y, a_out="", b_out=""):
        n = node(cls, x, y)
        link(a, a_out, n, "A")
        link(b, b_out, n, "B")
        return n

    def mask(src, x, y, r=False, g=False, b=False):
        n = node(unreal.MaterialExpressionComponentMask, x, y, r=r, g=g, b=b, a=False)
        link(src, "", n, "")
        return n

    position = node(unreal.MaterialExpressionWorldPosition, -2600, 0)
    flat = mask(position, -2400, -100, r=True, g=True)
    along = binary(unreal.MaterialExpressionAdd, mask(position, -2400, 50, r=True), mask(position, -2400, 150, g=True), -2200, 100)
    upright = binary(unreal.MaterialExpressionAppendVector, along, mask(position, -2400, 250, b=True), -2000, 150)
    lerp = node(unreal.MaterialExpressionLinearInterpolate, -1800, 0)
    link(flat, "", lerp, "A")
    link(upright, "", lerp, "B")
    link(scalar("Vertical", 0.0, -2000, 300), "", lerp, "Alpha")
    uv = binary(unreal.MaterialExpressionDivide, lerp, scalar("Tile", 600.0, -1800, 200), -1600, 0)
    flow = binary(unreal.MaterialExpressionAppendVector, scalar("FlowX", 0.0, -1800, 350), scalar("FlowY", -0.08, -1800, 450), -1600, 400)
    offset = binary(unreal.MaterialExpressionMultiply, node(unreal.MaterialExpressionTime, -1600, 300), flow, -1400, 350)
    uv1 = binary(unreal.MaterialExpressionAdd, uv, offset, -1200, 0)
    slow = binary(unreal.MaterialExpressionMultiply, offset, node(unreal.MaterialExpressionConstant, -1400, 500, r=0.71), -1200, 450)
    scaled = binary(unreal.MaterialExpressionMultiply, uv, node(unreal.MaterialExpressionConstant, -1400, 200, r=0.61), -1200, 200)
    uv2 = binary(unreal.MaterialExpressionAdd, binary(unreal.MaterialExpressionAdd, scaled, slow, -1000, 300),
                 node(unreal.MaterialExpressionConstant, -1200, 600, r=0.37), -800, 300)
    normal_sampler = sampler_for(normal_tex, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    n1 = node(unreal.MaterialExpressionTextureSampleParameter2D, -800, -200, parameter_name="RippleN", texture=normal_tex, sampler_type=normal_sampler)
    n2 = node(unreal.MaterialExpressionTextureSampleParameter2D, -600, 300, parameter_name="RippleN2", texture=normal_tex, sampler_type=normal_sampler)
    link(uv1, "", n1, "UVs")
    link(uv2, "", n2, "UVs")
    summed = binary(unreal.MaterialExpressionAdd, n1, n2, -400, 0, "RGB", "RGB")
    strength = node(unreal.MaterialExpressionVectorParameter, -600, 600, parameter_name="RippleScale", default_value=unreal.LinearColor(0.3, 0.3, 1.0, 1.0))
    shaped = binary(unreal.MaterialExpressionMultiply, summed, strength, -200, 0, "", "RGB")
    normal = node(unreal.MaterialExpressionNormalize, 0, 0)
    link(shaped, "", normal, "")
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    foam_tex = node(unreal.MaterialExpressionTextureSampleParameter2D, -800, -600, parameter_name="FoamBC", texture=albedo_tex,
                    sampler_type=sampler_for(albedo_tex, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR))
    link(uv1, "", foam_tex, "UVs")
    foam = binary(unreal.MaterialExpressionMultiply, foam_tex, scalar("FoamAmount", 0.0, -800, -400), -500, -500, "R", "")
    foam_clamped = node(unreal.MaterialExpressionSaturate, -300, -500)
    link(foam, "", foam_clamped, "")
    color = node(unreal.MaterialExpressionLinearInterpolate, -100, -400)
    link(node(unreal.MaterialExpressionVectorParameter, -300, -700, parameter_name="Color", default_value=unreal.LinearColor(0.03, 0.075, 0.07, 1.0)), "RGB", color, "A")
    link(node(unreal.MaterialExpressionVectorParameter, -300, -850, parameter_name="FoamColor", default_value=unreal.LinearColor(0.75, 0.8, 0.8, 1.0)), "RGB", color, "B")
    link(foam_clamped, "", color, "Alpha")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = binary(unreal.MaterialExpressionMultiply, color, scalar("Emissive", 0.0, -100, -200), 100, -300)
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(scalar("Roughness", 0.06, 0, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar("Specular", 0.6, 0, 300), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("%s: ripples and foam from %s" % (path, surface))

    for name, scalars, vectors in (
            ("MI_SB_Water", {"Vertical": 0.0, "Tile": 650.0, "FlowX": 0.0, "FlowY": -0.07, "FoamAmount": 0.0, "Roughness": 0.02, "Emissive": 0.0, "Specular": 0.8},
             {"Color": (0.018, 0.035, 0.03), "RippleScale": (0.35, 0.35, 1.0)}),
            ("MI_SB_Waterfall", {"Vertical": 1.0, "Tile": 260.0, "FlowX": 0.0, "FlowY": -1.3, "FoamAmount": 1.6, "Roughness": 0.3, "Emissive": 0.35},
             {"Color": (0.32, 0.38, 0.38), "FoamColor": (0.85, 0.9, 0.9), "RippleScale": (0.5, 0.5, 1.0)})):
        mi_path = MATERIALS + "/" + name
        if eal.does_asset_exist(mi_path):
            mi = unreal.load_asset(mi_path)
        else:
            mi = tools.create_asset(name, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, mat)
        mel.clear_all_material_instance_parameters(mi)
        for key, value in scalars.items():
            mel.set_material_instance_scalar_parameter_value(mi, key, value)
        for key, (r, g, b) in vectors.items():
            mel.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(r, g, b, 1.0))
        eal.save_asset(mi_path)
        log("%s: built" % mi_path)


def leaf_material():
    """M_SB_Leaves: our own masked, two-sided leaf material on the hornbeam's summer albedo. MI_SB_Leaves is an
    instance of the Megascans foliage master, which renders near black in this project (Creekbed R17)."""
    found = [a.split(".")[0] for a in eal.list_assets("/Game/Megascans/3D_Plants/EuropeanHornbeam", recursive=True)]
    albedo_path = next((a for a in found if a.endswith("/T_EuropeanHornbeam_TwoSided_Summer_Albedo")), None)
    mask_path = next((a for a in found if a.endswith("/T_EuropeanHornbeam_TwoSided_Summer_Mask")), None)
    if not albedo_path:
        log("M_SB_Leaves: no T_EuropeanHornbeam_TwoSided_Summer_Albedo")
        return
    albedo = unreal.load_asset(albedo_path)
    mask = unreal.load_asset(mask_path) if mask_path else None
    albedo_alpha = not albedo.get_editor_property("compression_no_alpha")
    path = MATERIALS + "/M_SB_Leaves"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_SB_Leaves", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("opacity_mask_clip_value", 0.4)
    bc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0)
    bc.set_editor_property("parameter_name", "Albedo")
    bc.set_editor_property("texture", albedo)
    bc.set_editor_property("sampler_type", sampler_for(albedo, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR))
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 300)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.1, 1.2, 0.95, 1.0))
    tinted = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, 100)
    mel.connect_material_expressions(bc, "RGB", tinted, "A")
    mel.connect_material_expressions(tint, "", tinted, "B")
    mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if albedo_alpha or not mask:
        mel.connect_material_property(bc, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
        source = "albedo alpha"
    else:
        m = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 600)
        m.set_editor_property("parameter_name", "Mask")
        m.set_editor_property("texture", mask)
        m.set_editor_property("sampler_type", sampler_for(mask, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR))
        mel.connect_material_property(m, "R", unreal.MaterialProperty.MP_OPACITY_MASK)
        source = "mask R (compression %s)" % mask.get_editor_property("compression_settings")
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 400)
    rough.set_editor_property("r", 0.7)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    nanite = getattr(unreal.MaterialUsage, "MATUSAGE_NANITE", None)
    if nanite is not None:
        mel.set_material_usage(mat, nanite)
    mel.recompile_material(mat)
    eal.save_asset(path)
    log("%s: albedo %s, opacity from %s" % (path, albedo.get_name(), source))


def main():
    try:
        assets = check_dependencies()
        describe_meshes(assets)
        describe_textures()
        describe_foliage()
        leaf_instance()
        grass_instance()
        grass_material()
        scan_instances(scan_material())
        terrain_sets()
        water_material()
        leaf_material()
        log("RESULT ok")
    except Exception as error:  # noqa: BLE001 - report anything into the log the cloud reads
        log("RESULT failed: %s" % error)
        raise
    finally:
        out = os.path.join(PROJECT, "Saved", "Logs", "ScanArt.txt")
        with open(out, "w") as f:
            f.write("\n".join(lines) + "\n")


main()
