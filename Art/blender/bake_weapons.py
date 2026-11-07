"""Textured AAA pass for the first-person weapons: high-poly to low-poly bakes and procedural PBR texturing in Blender.

    python bake_weapons.py [GunName ...] [--res 2048] [--preview DIR] [--no-export]

The pipeline every gun goes through (the same steps a weapon artist runs by hand):
 1. LOW: the game mesh from build_weapons.py, exactly as before (sockets, magazine split and hand fit unchanged).
    One material slot (M_SBW_<Gun>) replaces the 4-7 palette slots, so a gun is one draw call per LOD.
 2. HIGH: the same parts rebuilt with 2-4x the bevel segments and 3x the cylinder sides, smooth shaded, with a
    micro-surface layer (polymer stipple, brushed steel, orange peel and scratches in the paint) in its shader.
 3. UVs: smart-projected and packed on LOW into one 0-1 atlas.
 4. Bakes (Cycles): tangent-space normal HIGH -> LOW (DirectX green for Unreal), ambient occlusion on LOW, then
    procedural materials baked to textures: base colour with an emissive mask in alpha, and ORM (AO, roughness,
    metallic). The procedural layer is driven by the baked AO (grime in cavities), a bevel-shader edge mask (chipped
    paint showing bare metal, polished edges), per-part tone variation and noise, plus projected decals (weapon_decals).
 5. Export: Art/Export/Weapons/<Gun>.fbx (+ _NoMag/_Mag) and Art/Export/Textures/Weapons/T_<Gun>_{BC,ORM,N}.png.
    Tools/import_art.py turns those into M_SB_WeaponPBR instances (MI_SBW_<Gun>).

Assets are original: shapes from build_weapons.py, markings from weapon_decals.py.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sb_kit as k  # noqa: E402  (imports bpy first)
import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
import build_weapons as bw  # noqa: E402
import weapon_decals as wd  # noqa: E402

EXPORT = os.path.join(HERE, "..", "Export")
TEX_OUT = os.path.join(EXPORT, "Textures", "Weapons")
DECAL_DIR = os.path.join(HERE, "decals")

# ---- Surface looks (linear colours). Values were chosen against the FP light rig: pale ceramic must not clip to
# flat white (tasks 15, 19), dark metal must not read black (task 10), so the palette sits mid-range and the texture
# supplies the contrast (AO, grime, edges). --------------------------------------------------------------------------
SURFACES = {
    # kind, base colour, roughness, metallic, tone variation
    "Ceramic": ("paint", (0.5, 0.505, 0.5), 0.38, 0.0, 0.03),
    "WeaponMetal": ("paint", (0.19, 0.195, 0.205), 0.42, 0.15, 0.06),
    "Gunmetal": ("paint", (0.06, 0.063, 0.07), 0.42, 0.3, 0.10),
    "ArmorMetal": ("paint", (0.12, 0.125, 0.135), 0.45, 0.15, 0.08),
    "Olive": ("paint", (0.09, 0.105, 0.06), 0.5, 0.0, 0.08),
    "Sand": ("paint", (0.36, 0.3, 0.2), 0.55, 0.0, 0.08),
    "Ivory": ("paint", (0.5, 0.5, 0.48), 0.38, 0.0, 0.05),
    "Hazard": ("paint", (0.7, 0.38, 0.03), 0.5, 0.0, 0.05),
    "Team": ("paint", (0.45, 0.04, 0.03), 0.45, 0.0, 0.05),
    "Polymer": ("polymer", (0.032, 0.033, 0.036), 0.62, 0.0, 0.10),
    "Undersuit": ("polymer", (0.03, 0.03, 0.032), 0.7, 0.0, 0.05),
    "Steel": ("steel", (0.42, 0.43, 0.44), 0.3, 1.0, 0.06),
    "GlowCyan": ("glow", (0.15, 0.85, 1.0), 0.3, 0.0, 1.0),
    "GlowAmber": ("glow", (1.0, 0.55, 0.12), 0.3, 0.0, 1.0),
    "TeamGlow": ("glow", (1.0, 0.18, 0.08), 0.3, 0.0, 1.0),
    "Display": ("display", (0.04, 0.32, 0.48), 0.12, 0.0, 0.35),
    "Lens": ("lens", (0.01, 0.025, 0.05), 0.05, 0.0, 0.06),
}
# Paint wears through to this bare metal at the edges.
BARE_METAL = (0.48, 0.47, 0.45)
DARK_METAL = (0.07, 0.072, 0.078)  # what chipped white ceramic shows: the dark alloy shell under the paint
EDGE_AO = (0.75, 0.45)  # inside-AO range mapped to the convex-edge mask on HIGH
GRIME = (0.05, 0.045, 0.038)
DUST = (0.16, 0.15, 0.13)

# ---- Markings per gun: (image, axis, rect, slab, colour, surface). axis X-/X+ project onto the gun's sides:
# rect = (y0, y1, z0, z1) in cm, slab = (x_lo, x_hi) keeps the decal to the outer skin. Z+ projects from above:
# rect = (y0, y1, x0, x1), slab = (z_lo, z_hi). Text reads correctly from the side it is on (the FP camera sees -X).
DARK_INK = (0.025, 0.026, 0.028)
LIGHT_INK = (0.55, 0.55, 0.53)
ORANGE = (0.62, 0.16, 0.02)
CYAN_INK = (0.05, 0.35, 0.45)


def br_decals():
    d = []
    for side, xs in (("X-", (-3.6, -2.9)), ("X+", (2.9, 3.6))):
        # Receiver panel (y -4..6 between seams): model name and a data plate.
        d += [(wd.text(9.0, 1.25, "VANGUARD", "display", spacing=6), side, (-3.5, 5.5, 5.0, 6.25), xs, DARK_INK, "Ceramic"),
              (wd.lines(7.2, 0.9, ["VGD-6  BR  9.5x40 CSL", "SN 0417-2291-B"]), side, (-3.2, 4.0, 3.1, 4.0), xs, DARK_INK, "Ceramic"),
              (wd.bar(10.5, 0.16), side, (-3.8, 5.8, 4.72, 4.88), xs, ORANGE, "Ceramic"),
              # Rear receiver panel: emblem stamp.
              (wd.emblem(3.2, 3.2), side, (-8.6, -5.4, 1.9, 5.1), xs, (0.16, 0.165, 0.17), "Ceramic"),
              # Shroud panel, behind the vent window: calibre and a heat warning near the muzzle.
              (wd.warning(5.5, 1.1, "HOT"), side, (41.6, 47.1, 1.4, 2.5), (xs[0] - 0.3, xs[1] + 0.3), ORANGE, "Ceramic"),
              (wd.text(10.0, 0.75, "CAUTION  BARREL ASSEMBLY", "bold"), side, (28.0, 38.0, 5.2, 5.95), (xs[0] - 0.3, xs[1] + 0.3), DARK_INK, "Ceramic"),
              (wd.chevrons(3.2, 0.9), side, (23.0, 26.2, -1.9, -1.0), (xs[0] - 0.3, xs[1] + 0.3), ORANGE, "Ceramic"),
              # Lower receiver: fire selector legend and a magazine-release arrow.
              (wd.text(4.6, 0.7, "SAFE   BURST", "bold"), side, (-5.4, -0.8, -3.2, -2.5), (-2.6 if side == "X-" else 2.0, -2.0 if side == "X-" else 2.6), LIGHT_INK, "Polymer"),
              (wd.text(3.0, 0.55, "MAG REL", "bold"), side, (17.4, 20.4, -1.4, -0.85), (-2.6 if side == "X-" else 2.0, -2.0 if side == "X-" else 2.6), LIGHT_INK, "Polymer"),
              # Magazine: round-count ladder and a capacity stamp.
              (wd.ticks(3.6, 0.6), side, (12.4, 16.4, -12.6, -12.0), (-2.2 if side == "X-" else 1.5, -1.5 if side == "X-" else 2.2), LIGHT_INK, "Polymer"),
              (wd.text(3.0, 0.6, "36 RD", "bold"), side, (12.6, 15.6, -5.6, -5.0), (-2.2 if side == "X-" else 1.5, -1.5 if side == "X-" else 2.2), LIGHT_INK, "Polymer"),
              # Stock: barcode and inspection stamp.
              (wd.barcode(4.0, 0.8), side, (-31.0, -27.0, 4.3, 5.1), (xs[0] + 0.8, xs[1] - 0.8) if side == "X-" else (xs[0] - 0.8, xs[1] - 0.8), LIGHT_INK, "Polymer")]
    # Optic hood (seen from above-behind in first person): a thin orange index stripe and the optic model.
    d += [(wd.bar(14.0, 0.18), "Z+", (4.6, 18.6, -1.05, -0.87), (15.8, 17.4), ORANGE, "Ceramic"),
          (wd.text(4.2, 1.0, "MKS-4X", "display", spacing=3), "Z+", (5.0, 9.2, -0.2, 0.8), (15.8, 17.4), DARK_INK, "Ceramic"),
          (wd.ticks(5.0, 0.45, 11), "Z+", (11.0, 16.0, 0.35, 0.8), (15.8, 17.4), DARK_INK, "Ceramic")]
    return d


def sniper_decals():
    d = []
    for side, xs in (("X-", (-3.3, -2.7)), ("X+", (2.7, 3.3))):
        d += [(wd.text(13.5, 1.3, "LONGSHOT", "display", spacing=8), side, (-1.4, 12.1, 3.5, 4.8), xs, DARK_INK, "Ceramic"),
              (wd.lines(10.0, 1.0, ["LS-3  SR  14.5x114 APFSDS", "SN 1108-4471-D"]), side, (-1.0, 9.0, 1.3, 2.3), xs, DARK_INK, "Ceramic"),
              (wd.bar(14.5, 0.16), side, (-1.6, 12.9, 3.15, 3.31), xs, ORANGE, "Ceramic"),
              (wd.emblem(3.0, 3.0), side, (-9.5, -6.5, -0.2, 2.8), xs, (0.16, 0.165, 0.17), "Ceramic"),
              (wd.text(18.0, 0.7, "CAUTION  BARREL ASSEMBLY  DO NOT TOUCH", "bold"), side, (48.0, 66.0, 5.6, 6.3), (xs[0] + 0.3, xs[1] - 0.3) if side == "X-" else (xs[0] - 0.4, xs[1]), DARK_INK, "Ceramic"),
              (wd.warning(5.5, 1.1, "HOT"), side, (77.0, 82.5, 1.8, 2.9), (xs[0] + 0.3, xs[1] - 0.3) if side == "X-" else (xs[0] - 0.4, xs[1]), ORANGE, "Ceramic"),
              (wd.chevrons(3.6, 0.9), side, (69.5, 73.1, 5.0, 5.9), (xs[0] + 0.3, xs[1] - 0.3) if side == "X-" else (xs[0] - 0.4, xs[1]), ORANGE, "Ceramic"),
              (wd.text(3.0, 0.6, "5 RD", "bold"), side, (12.6, 15.6, -6.0, -5.4), (-2.2 if side == "X-" else 1.5, -1.5 if side == "X-" else 2.2), LIGHT_INK, "Polymer"),
              (wd.text(4.6, 0.7, "SAFE   FIRE", "bold"), side, (-5.4, -0.8, -3.2, -2.5), (-2.6 if side == "X-" else 2.0, -2.0 if side == "X-" else 2.6), LIGHT_INK, "Polymer")]
    return d


def sidearm_decals():
    d = []
    for side, xs in (("X-", (-2.1, -1.5)), ("X+", (1.5, 2.1))):
        d += [(wd.text(7.5, 0.95, "KESTREL", "display", spacing=5), side, (7.5, 15.0, 1.0, 1.95), xs, (0.03, 0.03, 0.032), "Steel"),
              (wd.text(5.0, 0.5, "KS-2  10mm CSL", "mono"), side, (8.0, 13.0, 0.35, 0.85), xs, (0.03, 0.03, 0.032), "Steel"),
              (wd.text(5.5, 0.55, "SN 2290-0613-K", "mono"), side, (7.0, 12.5, -1.65, -1.1), (xs[0] + 0.1, xs[1] - 0.1) if side == "X-" else (xs[0] - 0.1, xs[1] - 0.1), LIGHT_INK, "WeaponMetal")]
    return d


DECALS = {"SM_SB_BattleRifle": br_decals, "SM_SB_Sniper": sniper_decals, "SM_SB_Sidearm": sidearm_decals}


# =====================================================================================================================
# Node helpers
# =====================================================================================================================
class Nodes:
    def __init__(self, tree):
        self.tree = tree
        self.n = tree.nodes
        self.l = tree.links
        self.x = 0

    def add(self, typ, **props):
        node = self.n.new(typ)
        for key, value in props.items():
            setattr(node, key, value)
        node.location = (self.x, 0)
        self.x += 30
        return node

    def put(self, sock, value):
        if hasattr(value, "is_output"):
            self.l.new(value, sock)
        else:
            sock.default_value = value

    def math(self, op, a, b=0.0, clamp=False):
        node = self.add("ShaderNodeMath", operation=op, use_clamp=clamp)
        self.put(node.inputs[0], a)
        self.put(node.inputs[1], b)
        return node.outputs[0]

    def mul(self, a, b, clamp=False):
        return self.math("MULTIPLY", a, b, clamp)

    def addf(self, a, b, clamp=False):
        return self.math("ADD", a, b, clamp)

    def remap(self, v, a, b, c=0.0, d=1.0, smooth=True):
        node = self.add("ShaderNodeMapRange", interpolation_type="SMOOTHSTEP" if smooth else "LINEAR", clamp=True)
        self.put(node.inputs["Value"], v)
        node.inputs["From Min"].default_value = a
        node.inputs["From Max"].default_value = b
        node.inputs["To Min"].default_value = c
        node.inputs["To Max"].default_value = d
        return node.outputs["Result"]

    def mix(self, fac, a, b):
        node = self.add("ShaderNodeMix", data_type="RGBA")
        self.put(node.inputs["Factor"], fac)
        sa = next(s for s in node.inputs if s.identifier == "A_Color")
        sb = next(s for s in node.inputs if s.identifier == "B_Color")
        self.put(sa, tuple(a) + (1.0,) if isinstance(a, tuple) and len(a) == 3 else a)
        self.put(sb, tuple(b) + (1.0,) if isinstance(b, tuple) and len(b) == 3 else b)
        return next(s for s in node.outputs if s.identifier == "Result_Color")

    def mixf(self, fac, a, b):
        node = self.add("ShaderNodeMix", data_type="FLOAT")
        self.put(node.inputs["Factor"], fac)
        self.put(next(s for s in node.inputs if s.identifier == "A_Float"), a)
        self.put(next(s for s in node.inputs if s.identifier == "B_Float"), b)
        return next(s for s in node.outputs if s.identifier == "Result_Float")

    def scale_color(self, col, f):
        node = self.add("ShaderNodeMix", data_type="RGBA", blend_type="MULTIPLY")
        self.put(node.inputs["Factor"], 1.0)
        self.put(next(s for s in node.inputs if s.identifier == "A_Color"), col)
        sb = next(s for s in node.inputs if s.identifier == "B_Color")
        if hasattr(f, "is_output"):
            comb = self.add("ShaderNodeCombineColor")
            for i in range(3):
                self.put(comb.inputs[i], f)
            self.l.new(comb.outputs[0], sb)
        else:
            sb.default_value = (f, f, f, 1)
        return next(s for s in node.outputs if s.identifier == "Result_Color")

    def coords(self):
        return self.add("ShaderNodeTexCoord").outputs["Object"]

    def vscale(self, vec, s):
        node = self.add("ShaderNodeVectorMath", operation="MULTIPLY")
        self.put(node.inputs[0], vec)
        node.inputs[1].default_value = s
        return node.outputs[0]

    def noise(self, vec, scale, detail=4.0, rough=0.55, dims="3D"):
        node = self.add("ShaderNodeTexNoise", noise_dimensions=dims)
        self.put(node.inputs["Vector"], vec)
        node.inputs["Scale"].default_value = scale
        node.inputs["Detail"].default_value = detail
        node.inputs["Roughness"].default_value = rough
        return node.outputs["Fac"]

    def voronoi(self, vec, scale, feature="F1"):
        node = self.add("ShaderNodeTexVoronoi", feature=feature)
        self.put(node.inputs["Vector"], vec)
        node.inputs["Scale"].default_value = scale
        return node.outputs["Distance"]

    def sep(self, vec):
        node = self.add("ShaderNodeSeparateXYZ")
        self.put(node.inputs[0], vec)
        return node.outputs

    def combine(self, x, y, z=0.0):
        node = self.add("ShaderNodeCombineXYZ")
        for i, v in enumerate((x, y, z)):
            self.put(node.inputs[i], v)
        return node.outputs[0]


# =====================================================================================================================
# Build LOW and HIGH
# =====================================================================================================================
_orig_finish = k._finish
_orig_cyl = k.cyl


def _high_finish(obj, mat, bevel, segments=1, angle=35.0):
    return _orig_finish(obj, mat, bevel, max(4, segments * 2), angle)


def _high_cyl(name, center, radius, length, mat, axis="Y", verts=12, bevel=0.3, radius2=None):
    verts = verts if verts <= 6 else max(32, verts * 3)
    return _orig_cyl(name, center, radius, length, mat, axis, verts, bevel, radius2)


# Guns that get the full professional high poly (pro_highpoly.py): boolean cuts, recessed screws, real slots.
PRO = {"SM_SB_BattleRifle"}


def build_parts(name, high):
    import pro_highpoly as ph
    collector = ph.Collector(high) if name in PRO else None
    k._finish, k.cyl = (_high_finish, _high_cyl) if high else (_orig_finish, _orig_cyl)
    try:
        if collector:
            with ph.patched(collector):
                parts, muzzle = bw.WEAPONS[name]()
        else:
            parts, muzzle = bw.WEAPONS[name]()
        blade, rest = bw.TRIGGERS[name]
        parts.append(bw.trigger_blade(*blade))
        extras = bw.MAGAZINES[name][2]() if name in bw.MAGAZINES else []
    finally:
        k._finish, k.cyl = _orig_finish, _orig_cyl
    if collector:
        slots = [p for p in parts if ph.is_slot(p)]
        parts = [p for p in parts if p not in slots]
        for p in slots:
            if high:
                ph.slot_to_cutter(collector, p)
            else:
                bpy.data.objects.remove(p, do_unlink=True)
        if high:
            ph.extra_cuts(collector)
            cuts = ph.finish_high(parts + extras, collector)
            print("  high poly: %d cutters, %d boolean cuts" % (len(collector.cutters), cuts), flush=True)
    return parts, extras, muzzle, collector


def build_low(name):
    """LOW = gun parts, then magazine parts, then magazine-only extras, in that face order (for the export split)."""
    parts, extras, muzzle, _ = build_parts(name, high=False)
    ranges = {}
    if name in bw.MAGAZINES:
        box_min, box_max = bw.MAGAZINES[name][1]
        mag = [p for p in parts if bw._inside(p, box_min, box_max)]
        gun = [p for p in parts if p not in mag]
    else:
        mag, gun = [], parts
    groups = [("gun", gun), ("mag", mag), ("extra", extras)]
    objs = []
    start = 0
    for label, group in groups:
        if not group:
            ranges[label] = (start, start)
            continue
        obj = k.join(name + "_" + label, group)
        count = len(obj.data.polygons)
        ranges[label] = (start, start + count)
        start += count
        objs.append(obj)
    low = k.join(name + "_LOW", objs)
    # Shading exactly as Unreal computes it from the exported smoothing (ComputeNormals): no custom split normals.
    if low.data.has_custom_normals:
        bpy.context.view_layer.objects.active = low
        bpy.ops.mesh.customdata_custom_splitnormals_clear()
    return low, ranges, muzzle


def build_high(name):
    parts, extras, _, collector = build_parts(name, high=True)
    high = k.join(name + "_HIGH", parts + extras)
    if collector:
        import pro_highpoly as ph
        ph.remove_cutters(collector)  # weighted normals already shade it
        mats = high.data.materials
        fallback = next(m for m in mats if m is not None)
        for i, m in enumerate(mats):
            if m is None:  # boolean walls whose cutter had no material: give them the part's own surface
                mats[i] = fallback
        return high
    for p in high.data.polygons:
        p.use_smooth = True
    high.data.set_sharp_from_angle(angle=math.radians(50))
    return high


def unwrap(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    if not obj.data.uv_layers:
        obj.data.uv_layers.new(name="UVMap")
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(55), island_margin=0.003, area_weight=0.0, scale_to_bounds=False)
    bpy.ops.uv.pack_islands(rotate=True, margin=0.003)
    bpy.ops.object.mode_set(mode="OBJECT")


# =====================================================================================================================
# Materials
# =====================================================================================================================
def palette_name(mat):
    return mat.name[2:].split(".")[0] if mat.name.startswith("M_") else mat.name


def high_material(kind_name):
    """Shader for HIGH: only its normal matters (baked into LOW's normal map)."""
    mat = bpy.data.materials.new("HI_" + kind_name)
    mat.use_nodes = True
    g = Nodes(mat.node_tree)
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    kind = SURFACES.get(kind_name, SURFACES["WeaponMetal"])[0]
    co = g.coords()
    bevel = g.add("ShaderNodeBevel", samples=8)
    bevel.inputs["Radius"].default_value = 0.06
    height, strength = None, 0.0
    if kind == "polymer":
        # Moulded stipple: fine cells.
        height = g.math("SUBTRACT", 1.0, g.remap(g.voronoi(co, 14.0), 0.0, 0.45))
        height = g.addf(g.mul(height, 0.7), g.mul(g.noise(co, 30.0, 2.0), 0.3))
        strength = 0.12
    elif kind == "steel":
        # Brushed along the barrel axis.
        height = g.noise(g.vscale(co, (6.0, 0.15, 6.0)), 8.0, 6.0, 0.6)
        strength = 0.05
    elif kind == "paint":
        # Orange peel plus sparse fine scratches.
        peel = g.noise(co, 9.0, 3.0, 0.5)
        scratch = g.remap(g.noise(g.vscale(co, (1.0, 0.08, 1.0)), 7.0, 2.0, 0.5), 0.62, 0.66, 0.0, 1.0)
        height = g.math("SUBTRACT", g.mul(peel, 0.35), g.mul(scratch, 0.6))
        strength = 0.05
    if height is not None:
        bump = g.add("ShaderNodeBump")
        bump.inputs["Strength"].default_value = strength
        bump.inputs["Distance"].default_value = 0.02
        g.put(bump.inputs["Height"], height)
        g.put(bump.inputs["Normal"], bevel.outputs["Normal"])
        g.put(bsdf.inputs["Normal"], bump.outputs["Normal"])
    else:
        g.put(bsdf.inputs["Normal"], bevel.outputs["Normal"])
    return mat


def decal_layer(g, decals, slot):
    """Returns (colour, factor) sockets for the decals on this surface, or None."""
    mine = [d for d in decals if d["surface"] == slot]
    if not mine:
        return None
    co = g.sep(g.coords())
    geo = g.add("ShaderNodeNewGeometry")
    nrm = g.sep(geo.outputs["True Normal"])
    col, fac = (0.0, 0.0, 0.0, 1.0), 0.0
    for d in mine:
        y0, y1, a0, a1 = d["rect"]
        lo, hi = d["slab"]
        if d["axis"] in ("X-", "X+"):
            if d["axis"] == "X-":
                u = g.mul(g.math("SUBTRACT", y1, co[1]), 1.0 / (y1 - y0))
                facing = g.remap(g.mul(nrm[0], -1.0), 0.45, 0.65, smooth=False)
            else:
                u = g.mul(g.math("SUBTRACT", co[1], y0), 1.0 / (y1 - y0))
                facing = g.remap(nrm[0], 0.45, 0.65, smooth=False)
            v = g.mul(g.math("SUBTRACT", co[2], a0), 1.0 / (a1 - a0))
            depth = co[0]
        else:  # Z+: u along +Y, v toward -X (letters lean toward the camera's side)
            u = g.mul(g.math("SUBTRACT", co[1], y0), 1.0 / (y1 - y0))
            v = g.mul(g.math("SUBTRACT", a1, co[0]), 1.0 / (a1 - a0))
            facing = g.remap(nrm[2], 0.6, 0.8, smooth=False)
            depth = co[2]
        inside = g.mul(g.math("GREATER_THAN", depth, lo), g.math("LESS_THAN", depth, hi))
        tex = g.add("ShaderNodeTexImage", extension="CLIP", interpolation="Cubic")
        tex.image = d["image"]
        g.put(tex.inputs["Vector"], g.combine(u, v))
        a = g.mul(g.mul(tex.outputs["Alpha"], facing), inside)
        col = g.mix(a, col, d["color"])
        fac = g.math("MAXIMUM", fac, a)
    return col, fac


def bake_material(slot, ao_image, decals, target):
    """Procedural surface for LOW, exposing base colour, roughness, metallic and emissive mask as sockets."""
    mat = bpy.data.materials.new("BK_" + slot)
    mat.use_nodes = True
    tree = mat.node_tree
    tree.nodes.clear()
    g = Nodes(tree)
    kind, base, rough0, metal0, var = SURFACES.get(slot, SURFACES["WeaponMetal"])
    co = g.coords()
    geo = g.add("ShaderNodeNewGeometry")
    if ao_image is None:
        # Evaluated on HIGH during a selected-to-active bake: occlusion straight from the dense geometry, so the
        # boolean grooves, screw recesses and slots collect grime.
        ao_node = g.add("ShaderNodeAmbientOcclusion", samples=16)
        ao_node.inputs["Distance"].default_value = 2.5
        ao = ao_node.outputs["AO"]
    else:
        uv = g.add("ShaderNodeUVMap")
        uv.uv_map = "UVMap"
        ao_tex = g.add("ShaderNodeTexImage")
        ao_tex.image = ao_image
        g.put(ao_tex.inputs["Vector"], uv.outputs["UV"])
        ao = ao_tex.outputs["Color"]
    # Edge mask from a rounded (bevel) normal against the true face normal.
    bevel = g.add("ShaderNodeBevel", samples=12)
    bevel.inputs["Radius"].default_value = 0.22
    dot = g.add("ShaderNodeVectorMath", operation="DOT_PRODUCT")
    g.put(dot.inputs[0], bevel.outputs["Normal"])
    g.put(dot.inputs[1], geo.outputs["True Normal"])
    edge = g.remap(g.math("SUBTRACT", 1.0, dot.outputs["Value"]), 0.015, 0.16)
    if ao_image is None:
        # On HIGH the edges are already rounded geometry, so the bevel trick sees little: add convexity from an
        # inside-facing AO (rays leave a thin convex edge almost at once), the usual baked "curvature" for wear.
        inside = g.add("ShaderNodeAmbientOcclusion", samples=16, inside=True)
        inside.inputs["Distance"].default_value = 0.6
        convex = g.remap(inside.outputs["AO"], EDGE_AO[0], EDGE_AO[1], 0.0, 1.0)
        edge = g.math("MAXIMUM", edge, convex)
    cavity = g.remap(ao, 0.5, 0.95, 1.0, 0.0)
    part = geo.outputs["Random Per Island"]
    n_big = g.noise(co, 0.09, 3.0, 0.5)
    n_mid = g.noise(co, 0.7, 6.0, 0.6)
    n_fine = g.noise(co, 4.0, 8.0, 0.65)
    z = g.sep(co)[2]
    low_bias = g.remap(z, 4.0, -14.0, 0.0, 1.0, smooth=False)  # more dirt toward the underside
    grime = g.mul(g.addf(g.mul(cavity, 1.0), g.mul(low_bias, 0.25)), g.addf(0.45, g.mul(n_big, 0.9)), clamp=True)
    grime = g.remap(grime, 0.15, 0.9)
    tone = g.addf(1.0, g.addf(g.mul(g.math("SUBTRACT", part, 0.5), var * 1.2), g.mul(g.math("SUBTRACT", n_big, 0.5), var)))
    em = 0.0
    if kind == "paint":
        chip_src = g.addf(g.mul(edge, g.addf(0.35, g.mul(n_mid, 1.1))), g.mul(g.math("SUBTRACT", n_fine, 0.5), 0.35))
        chip = g.remap(chip_src, 0.6, 0.66)
        rim = g.math("SUBTRACT", g.remap(chip_src, 0.5, 0.62), chip, clamp=True)  # darker primer ring round each chip
        scratches = g.remap(g.noise(g.vscale(co, (1.0, 0.07, 1.0)), 6.0, 2.0, 0.5), 0.665, 0.68)
        scratches = g.mul(scratches, g.remap(n_big, 0.35, 0.65))
        col = g.scale_color(base + (1.0,), tone)
        light = sum(base) / 3.0 > 0.25
        if not light:
            col = g.mix(g.mul(edge, 0.25), col, tuple(min(1.0, c * 1.35 + 0.02) for c in base))  # worn-bright edges
        col = g.mix(g.mul(grime, 0.7 if light else 0.45), col, GRIME)
        dl = decal_layer(g, decals, slot)
        if dl:
            col = g.mix(g.mul(dl[1], g.math("SUBTRACT", 1.0, g.mul(chip, 1.0))), col, dl[0])
        col = g.mix(g.mul(rim, 0.5), col, (base[0] * 0.45, base[1] * 0.45, base[2] * 0.45))
        col = g.mix(g.mul(scratches, 0.6), col, tuple(min(1.0, c * 1.25 + 0.03) for c in base))
        col = g.mix(chip, col, DARK_METAL if light else BARE_METAL)
        rough = g.addf(rough0, g.addf(g.mul(g.math("SUBTRACT", n_mid, 0.5), 0.12), g.mul(grime, 0.22)))
        rough = g.addf(rough, g.mul(scratches, 0.12))
        rough = g.math("SUBTRACT", rough, g.mul(edge, 0.08))
        rough = g.mixf(chip, rough, 0.34)
        metal = g.mixf(chip, metal0, 0.7 if light else 1.0)
    elif kind == "polymer":
        scuff = g.mul(edge, g.remap(n_mid, 0.4, 0.7))
        col = g.scale_color(base + (1.0,), tone)
        col = g.mix(g.mul(scuff, 0.8), col, tuple(c * 2.4 + 0.012 for c in base))
        col = g.mix(g.mul(grime, 0.55), col, DUST)
        dl = decal_layer(g, decals, slot)
        if dl:
            col = g.mix(g.mul(dl[1], 0.85), col, dl[0])
        rough = g.addf(rough0, g.addf(g.mul(g.math("SUBTRACT", n_mid, 0.5), 0.1), g.mul(grime, 0.15)))
        rough = g.math("SUBTRACT", rough, g.mul(scuff, 0.15))
        metal = 0.0
    elif kind == "steel":
        brushed = g.noise(g.vscale(co, (6.0, 0.15, 6.0)), 8.0, 6.0, 0.6)
        col = g.scale_color(base + (1.0,), g.addf(tone, g.mul(g.math("SUBTRACT", brushed, 0.5), 0.15)))
        col = g.mix(g.mul(grime, 0.85), col, (0.06, 0.05, 0.04))
        dl = decal_layer(g, decals, slot)
        if dl:  # laser-etched: dark and a little rougher
            col = g.mix(dl[1], col, dl[0])
        rough = g.addf(rough0, g.addf(g.mul(g.math("SUBTRACT", brushed, 0.5), 0.12), g.mul(grime, 0.3)))
        rough = g.math("SUBTRACT", rough, g.mul(edge, 0.1))
        metal = g.mixf(g.mul(grime, 0.8), 1.0, 0.3)
    elif kind == "display":
        # A dim screen with scan rows and a brighter segment bar along its length.
        s = g.sep(co)
        rows = g.remap(g.math("SINE", g.mul(s[2], 40.0)), -0.2, 0.6, 0.55, 1.0)
        col = g.scale_color(base + (1.0,), rows)
        rough, metal, em = rough0, 0.0, var
    elif kind == "lens":
        # Coated glass: dark, with a violet-to-teal tint across it.
        s = g.sep(co)
        tint = g.remap(g.addf(s[0], s[2]), -2.0, 18.0)
        col = g.mix(tint, base, (0.03, 0.012, 0.06))
        rough, metal, em = rough0, 0.0, var
    else:  # glow
        col = g.mix(g.mul(cavity, 0.4), base, tuple(c * 0.6 for c in base))
        rough, metal, em = rough0, 0.0, var
    if kind != "glow" and kind not in ("display", "lens"):
        em = 0.0
    # AO for the ORM pack (not cavity-shaped).
    ao_out = g.mul(ao, 1.0)
    outputs = {"curv": edge, "basecolor": col, "rough": g.math("ADD", rough, 0.0, clamp=True), "metal": g.math("ADD", metal, 0.0, clamp=True),
               "emit": g.math("ADD", em, 0.0, clamp=True), "ao": ao_out}
    emission = g.add("ShaderNodeEmission")
    out = g.add("ShaderNodeOutputMaterial")
    tree.links.new(emission.outputs[0], out.inputs["Surface"])
    target_node = g.add("ShaderNodeTexImage")
    target_node.image = target
    tree.nodes.active = target_node
    mat["sb_outputs"] = 1
    mat_outputs[mat.name] = (outputs, emission)
    return mat


mat_outputs = {}


def route(mat, key):
    outputs, emission = mat_outputs[mat.name]
    sock = outputs[key]
    for link in list(emission.inputs["Color"].links):
        mat.node_tree.links.remove(link)
    mat.node_tree.links.new(sock, emission.inputs["Color"])
    emission.inputs["Strength"].default_value = 1.0


# =====================================================================================================================
# Baking
# =====================================================================================================================
def setup_cycles(samples):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.cycles.use_denoising = False
    scene.render.bake.margin = 12
    scene.render.bake.margin_type = "EXTEND"
    scene.render.bake.target = "IMAGE_TEXTURES"
    scene.view_settings.view_transform = "Standard"
    return scene


def new_image(name, res, data=True):
    img = bpy.data.images.new(name, res, res, alpha=False, float_buffer=True)
    img.colorspace_settings.name = "Non-Color" if data else "Linear Rec.709"
    return img


def pixels(img):
    arr = np.empty(img.size[0] * img.size[1] * 4, dtype=np.float32)
    img.pixels.foreach_get(arr)
    return arr.reshape(img.size[1], img.size[0], 4)


def bake_normal(low, high, res):
    scene = setup_cycles(4)
    img = new_image("N", res)
    for slot in low.material_slots:
        tree = slot.material.node_tree
        node = tree.nodes.new("ShaderNodeTexImage")
        node.image = img
        tree.nodes.active = node
    for i, slot in enumerate(high.material_slots):
        slot.material = high_material(palette_name(slot.material))
    bpy.ops.object.select_all(action="DESELECT")
    high.select_set(True)
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    b = scene.render.bake
    b.use_selected_to_active = True
    b.use_cage = False
    b.cage_extrusion = 0.3
    b.max_ray_distance = 0.9
    b.normal_space = "TANGENT"
    bpy.ops.object.bake(type="NORMAL")
    b.use_selected_to_active = False
    return img


def bake_ao(low, res, samples):
    scene = setup_cycles(samples)
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")
    scene.world.light_settings.distance = 6.0
    img = new_image("AO", res)
    for slot in low.material_slots:
        tree = slot.material.node_tree
        node = tree.nodes.new("ShaderNodeTexImage")
        node.image = img
        tree.nodes.active = node
    bpy.ops.object.select_all(action="DESELECT")
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    bpy.ops.object.bake(type="AO")
    return img


def bake_surfaces(low, ao_img, decals, res, samples):
    setup_cycles(samples)
    target = new_image("SURF", res)
    originals = [s.material for s in low.material_slots]
    for slot in low.material_slots:
        slot.material = bake_material(palette_name(slot.material), ao_img, decals, target)
    bpy.ops.object.select_all(action="DESELECT")
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    result = {}
    for key in ("basecolor", "rough", "metal", "emit", "ao"):
        for slot in low.material_slots:
            route(slot.material, key)
        bpy.ops.object.bake(type="EMIT")
        result[key] = pixels(target).copy()
        print("  baked", key, flush=True)
    return result, originals


def bake_surfaces_from_high(low, high, decals, res, samples):
    """PRO: the procedural materials live on HIGH and are baked onto LOW (selected to active), with AO baked the same
    way. Detail that exists only on HIGH (grooves, recesses, slots) gets its own grime, wear and occlusion."""
    scene = setup_cycles(samples)
    target = new_image("SURF", res)
    for slot in low.material_slots:
        tree = slot.material.node_tree
        node = tree.nodes.new("ShaderNodeTexImage")
        node.image = target
        tree.nodes.active = node
    for slot in high.material_slots:
        name = slot.material.name[3:] if slot.material.name.startswith("HI_") else palette_name(slot.material)
        slot.material = bake_material(name, None, decals, target)
    bpy.ops.object.select_all(action="DESELECT")
    high.select_set(True)
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    b = scene.render.bake
    b.use_selected_to_active = True
    b.cage_extrusion = 0.3
    b.max_ray_distance = 0.9
    result = {}
    for key in ("curv", "basecolor", "rough", "metal", "emit"):
        for slot in high.material_slots:
            route(slot.material, key)
        bpy.ops.object.bake(type="EMIT")
        result[key] = pixels(target).copy()
        print("  baked", key, flush=True)
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")
    scene.world.light_settings.distance = 4.0
    scene.cycles.samples = max(samples * 2, 32)
    bpy.ops.object.bake(type="AO")
    result["ao"] = pixels(target).copy()
    print("  baked ao", flush=True)
    b.use_selected_to_active = False
    return result


def linear_to_srgb(x):
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, x * 12.92, 1.055 * np.power(x, 1 / 2.4) - 0.055)


def save_png(arr, path, mode):
    from PIL import Image
    arr = np.flipud(np.clip(arr, 0.0, 1.0))  # Blender pixels start bottom-left
    Image.fromarray((arr * 255.0 + 0.5).astype(np.uint8), mode).save(path, optimize=True)


def write_textures(name, normal_img, surf, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    short = name.replace("SM_SB_", "")
    n = pixels(normal_img)[:, :, :3].copy()
    n[:, :, 1] = 1.0 - n[:, :, 1]  # OpenGL (Blender) -> DirectX (Unreal) green
    save_png(n, os.path.join(out_dir, "T_SB_%s_N.png" % short), "RGB")
    bc = linear_to_srgb(surf["basecolor"][:, :, :3])
    alpha = surf["emit"][:, :, :1]
    save_png(np.concatenate([bc, alpha], axis=2), os.path.join(out_dir, "T_SB_%s_BC.png" % short), "RGBA")
    orm = np.stack([surf["ao"][:, :, 0], surf["rough"][:, :, 0], surf["metal"][:, :, 0]], axis=2)
    save_png(orm, os.path.join(out_dir, "T_SB_%s_ORM.png" % short), "RGB")
    return short


# =====================================================================================================================
# Export and preview
# =====================================================================================================================
def single_slot(obj, mat_name):
    mat = bpy.data.materials.get(mat_name) or bpy.data.materials.new(mat_name)
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    for p in obj.data.polygons:
        p.material_index = 0
    return mat


def extract(low, name, face_ranges, socket_offset=None):
    """A copy of LOW holding only the faces in face_ranges (list of (start, end))."""
    mesh = low.data.copy()
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.faces.ensure_lookup_table()
    keep = set()
    for a, b in face_ranges:
        keep.update(range(a, b))
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in keep], context="FACES")
    if socket_offset:
        bmesh.ops.translate(bm, verts=bm.verts, vec=tuple(-c for c in socket_offset))
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def export(name, low, ranges, muzzle, report):
    out_dir = os.path.join(EXPORT, "Weapons")
    gun, mag, extra = ranges["gun"], ranges["mag"], ranges["extra"]
    full = extract(low, name, [gun, mag])
    sockets = [("Muzzle", muzzle), ("Trigger", bw.TRIGGERS[name][1])]
    if name in bw.SUPPORT:
        sockets.append(("SupportHand", bw.SUPPORT[name]))
    if name in bw.MAGAZINES:
        sockets.append(("Magazine", bw.MAGAZINES[name][0]))
    for s, loc in sockets:
        k.socket(s, loc, full)
    report.append("%s: %d tris, 1 material (textured), bounds %s" % (name, k.triangle_count(full), k.bounds(full)))
    k.export_fbx(os.path.join(out_dir, name + ".fbx"), [full])
    if name in bw.MAGAZINES:
        socket = bw.MAGAZINES[name][0]
        nomag = extract(low, name + "_NoMag", [gun])
        for s, loc in sockets:
            k.socket(s, loc, nomag)
        magobj = extract(low, name + "_Mag", [mag, extra], socket_offset=socket)
        report.append("%s_NoMag: %d tris; %s_Mag: %d tris, bounds %s" % (name, k.triangle_count(nomag), name,
                      k.triangle_count(magobj), k.bounds(magobj)))
        k.export_fbx(os.path.join(out_dir, name + "_NoMag.fbx"), [nomag])
        k.export_fbx(os.path.join(out_dir, name + "_Mag.fbx"), [magobj])


def preview_material(short, out_dir):
    """Principled material reading the exported PNGs the way Unreal will (DirectX normal, ORM, emissive in BC alpha)."""
    mat = bpy.data.materials.new("PV_" + short)
    mat.use_nodes = True
    g = Nodes(mat.node_tree)
    bsdf = mat.node_tree.nodes["Principled BSDF"]

    def tex(suffix, colorspace):
        node = g.add("ShaderNodeTexImage")
        node.image = bpy.data.images.load(os.path.join(out_dir, "T_SB_%s_%s.png" % (short, suffix)))
        node.image.colorspace_settings.name = colorspace
        node.image.alpha_mode = "CHANNEL_PACKED"  # BC alpha is the emissive mask, not opacity
        return node

    bc = tex("BC", "sRGB")
    orm = g.sep(tex("ORM", "Non-Color").outputs["Color"])
    nrm = g.sep(tex("N", "Non-Color").outputs["Color"])
    g.put(bsdf.inputs["Base Color"], bc.outputs["Color"])
    g.put(bsdf.inputs["Roughness"], orm[1])
    g.put(bsdf.inputs["Metallic"], orm[2])
    g.put(bsdf.inputs["Emission Color"], bc.outputs["Color"])
    g.put(bsdf.inputs["Emission Strength"], g.mul(bc.outputs["Alpha"], 5.0))
    nmap = g.add("ShaderNodeNormalMap")
    g.put(nmap.inputs["Color"], g.combine(nrm[0], g.math("SUBTRACT", 1.0, nrm[1]), nrm[2]))
    g.put(bsdf.inputs["Normal"], nmap.outputs["Normal"])
    return mat


def bake_gun(name, res, ao_samples, surf_samples, do_export, preview_dir):
    import time
    t0 = time.time()
    k.reset_scene()
    report = []
    decals = []
    for i, (img, axis, rect, slab, color, surface) in enumerate(DECALS.get(name, lambda: [])()):
        path = wd.save(img, DECAL_DIR, "%s_%02d" % (name.replace("SM_SB_", ""), i))
        decals.append({"image": bpy.data.images.load(path), "axis": axis, "rect": rect, "slab": slab, "color": color,
                       "surface": surface})
    low, ranges, muzzle = build_low(name)
    high = build_high(name)
    print("%s: LOW %d tris, HIGH %d tris (%.0fs)" % (name, k.triangle_count(low), k.triangle_count(high), time.time() - t0), flush=True)
    report.append("%s bake: LOW %d tris, HIGH %d tris, %d px" % (name, k.triangle_count(low), k.triangle_count(high), res))
    unwrap(low)
    normal = bake_normal(low, high, res)
    print("  normal baked (%.0fs)" % (time.time() - t0), flush=True)
    if name in PRO:
        surf = bake_surfaces_from_high(low, high, decals, res, surf_samples)
        bpy.data.objects.remove(high, do_unlink=True)
    else:
        bpy.data.objects.remove(high, do_unlink=True)
        ao = bake_ao(low, res, ao_samples)
        print("  AO baked (%.0fs)" % (time.time() - t0), flush=True)
        surf, _ = bake_surfaces(low, ao, decals, res, surf_samples)
    short = write_textures(name, normal, surf, TEX_OUT)
    if "curv" in surf:
        save_png(surf["curv"][:, :, :3], os.path.join(TEX_OUT, "..", "..", "..", "blender", "decals", "T_SB_%s_Curvature.png" % short), "RGB")
    print("  textures written (%.0fs)" % (time.time() - t0), flush=True)
    single_slot(low, "M_SBW_" + short)
    if do_export:
        export(name, low, ranges, muzzle, report)
    if preview_dir:
        low.data.materials[0] = preview_material(short, TEX_OUT)
        blend = os.path.join(preview_dir, short + "_textured.blend")
        bpy.ops.wm.save_as_mainfile(filepath=blend, compress=True)
        report.append("preview scene " + blend)
    return report


def main():
    argv = sys.argv[1:]
    res = int(argv[argv.index("--res") + 1]) if "--res" in argv else 2048
    preview = argv[argv.index("--preview") + 1] if "--preview" in argv else None
    names = [a for a in argv if a.startswith("SM_SB_")] or list(DECALS)
    report = []
    for name in names:
        report += bake_gun(name, res, ao_samples=48 if res >= 2048 else 24, surf_samples=16 if res >= 2048 else 8,
                           do_export="--no-export" not in argv, preview_dir=preview)
    print("\n".join(report))
    with open(os.path.join(TEX_OUT, "bake_report.txt"), "a") as f:
        f.write("\n".join(report) + "\n")


if __name__ == "__main__":
    main()
