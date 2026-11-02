"""Spire's own cladding kit: smooth, angular Forerunner-style plates (run headless:
python3 Art/blender/build_forerunner_kit.py [--render=<folder>]).

Original pieces, drawn here. They stand in for the Quaternius panels (build_scifi_kit.py) piece for piece, with the
same names, sizes and pivots, so ShieldbreakMapArt::Cladding's recipes place them unchanged. A map whose look sets
CladKit = "F" loads SM_F_<piece> from Art/MapKit instead of SM_Q_<piece> (AShieldbreakArena::BuildCladding). Keel
keeps the panelled hull plating; Spire gets these large silver plates split by angled dark seams, with a few
hard-light grooves.

Mesh space: 1 unit = 1 cm, Unreal axes (sb_kit.export_fbx converts).
  Wall pieces  face on X = 0 looking +X, centred on Y (400 cm wide), base at Z = 0; rows 200 cm (Wall*) or 100 cm (Band*).
  Floor pieces walking surface on Z = 0 looking +Z, centred on X and Y (400 x 400 cm).
Material slots are palette names: M_Steel (the map's Metal), M_Gunmetal (DarkMetal, the seams and backing) and
M_GlowCyan (recoloured with the map's KitGlow), so the game's tinted world material covers them like the hero kit.

Exported to Art/Export/MapKit (SM_F_<piece>.fbx) with forerunnerkit_report.txt.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit  # noqa: E402  (imports bpy first)

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Export", "MapKit")

PLATE = 6.0     # front plate depth (cm proud of the face)
BACK = 2.0      # backing plate depth: the seams between plates show it, dark
GLOW = 4.0      # glow strips sit between the two
W = 200.0       # half width of a wall piece


def plate(name, poly, mat="Steel", depth=PLATE, bevel=1.2):
    """A wall plate: polygon (y, z) on the face, depth cm proud of it."""
    return sb_kit.profile(name, poly, depth, mat, x=depth * 0.5, bevel=bevel)


def backing(height):
    return plate("back", [(-W, 0), (W, 0), (W, height), (-W, height)], "Gunmetal", BACK, bevel=0.0)


def chamfer(y0, z0, y1, z1, c, corners="tl,tr,bl,br"):
    """Rectangle (y0, z0)-(y1, z1) with its listed corners cut by c, counter-clockwise from bottom left."""
    pts = []
    pts += [(y0, z0 + c), (y0 + c, z0)] if "bl" in corners else [(y0, z0)]
    pts += [(y1 - c, z0), (y1, z0 + c)] if "br" in corners else [(y1, z0)]
    pts += [(y1, z1 - c), (y1 - c, z1)] if "tr" in corners else [(y1, z1)]
    pts += [(y0 + c, z1), (y0, z1 - c)] if "tl" in corners else [(y0, z1)]
    return pts


# ---- Wall rows (200 cm) ----

def wall_metal():
    """Two silver plates split by a leaning seam, outer corners cut."""
    return [backing(200),
            plate("left", [(-196, 16), (-184, 4), (14, 4), (-26, 196), (-184, 196), (-196, 184)]),
            plate("right", [(22, 4), (184, 4), (196, 16), (196, 184), (184, 196), (-18, 196)])]


def wall_band():
    """Upper and lower plates round a hard-light groove that dips into a chevron at the middle."""
    return [backing(200),
            plate("low", [(-196, 4), (196, 4), (196, 86), (34, 86), (0, 70), (-34, 86), (-196, 86)]),
            plate("groove", [(-196, 92), (-34, 92), (0, 76), (34, 92), (196, 92), (196, 104), (34, 104), (0, 88), (-34, 104), (-196, 104)],
                  "GlowCyan", GLOW, bevel=0.0),
            plate("high", [(-196, 110), (-34, 110), (0, 94), (34, 110), (196, 110), (196, 196), (-196, 196)])]


def wall_dark():
    """A single dark plate with its top corners cut, for the lower, recessed walls."""
    return [backing(200), plate("dark", chamfer(-196, 4, 196, 196, 26, "tl,tr"), "Gunmetal")]


def wall_plate():
    """A framed panel: four angled frame plates proud of a recessed centre plate (the team bases)."""
    inner = (-150, 40, 150, 160)
    y0, z0, y1, z1 = inner
    return [backing(200),
            plate("centre", chamfer(y0 + 4, z0 + 4, y1 - 4, z1 - 4, 18), "Steel", 3.0),
            plate("frame_b", [(-196, 4), (196, 4), (y1, z0), (y0, z0)], bevel=1.0),
            plate("frame_t", [(y0, z1), (y1, z1), (196, 196), (-196, 196)], bevel=1.0),
            plate("frame_l", [(-196, 10), (y0 - 4, z0 + 4), (y0 - 4, z1 - 4), (-196, 190)], bevel=1.0),
            plate("frame_r", [(y1 + 4, z0 + 4), (196, 10), (196, 190), (y1 + 4, z1 - 4)], bevel=1.0)]


# ---- Band rows (100 cm) ----

def band_plates():
    return [backing(100),
            plate("a", [(-196, 4), (-6, 4), (-6, 96), (-180, 96), (-196, 80)]),
            plate("b", [(6, 4), (196, 4), (196, 80), (180, 96), (6, 96)])]


def band_accent():
    """A plate with a level hard-light line above it and a thin cap: the tops of walls and slab edges."""
    return [backing(100),
            plate("plate", [(-196, 4), (196, 4), (196, 66), (-196, 66)]),
            plate("line", [(-196, 72), (196, 72), (196, 80), (-196, 80)], "GlowCyan", GLOW, bevel=0.0),
            plate("cap", [(-196, 86), (196, 86), (196, 96), (-196, 96)], bevel=0.8)]


def band_dark():
    return [backing(100), plate("dark", [(-196, 4), (196, 4), (196, 96), (-196, 96)], "Gunmetal")]


def band_triangles():
    """Four leaning plates, alternately tipped, for the plinths at the foot of the walls."""
    parts = [backing(100)]
    for i, y in enumerate((-196, -98, 0, 98)):
        lean = 14 if i % 2 == 0 else -14
        lo, hi = (y + 4, y + 94), (y + 4 - lean, y + 94 - lean)
        parts.append(plate("lean%d" % i, [(max(lo[0], -196), 4), (min(lo[1], 196), 4), (min(hi[1], 196), 96), (max(hi[0], -196), 96)]))
    return parts


# ---- Floor plates (400 x 400, top at Z = 0) ----

def tile(name, x0, y0, x1, y1, mat="Steel", depth=4.0, bevel=1.0):
    return sb_kit.box(name, ((x0 + x1) * 0.5, (y0 + y1) * 0.5, -depth * 0.5), (x1 - x0, y1 - y0, depth), mat, bevel=bevel)


def floor_backing():
    """The dark plate under the seams, its top 1 cm below the plates' (coplanar tops would flicker)."""
    return sb_kit.box("fback", (0.0, 0.0, -3.5), (400.0, 400.0, 5.0), "Gunmetal", bevel=0.0)


def floor_metal():
    """Four plates round a dark cross seam."""
    return [floor_backing()] + [tile("q%d" % i, x0, y0, x1, y1) for i, (x0, y0, x1, y1) in
                                enumerate([(-198, -198, -5, -5), (5, -198, 198, -5), (-198, 5, -5, 198), (5, 5, 198, 198)])]


def floor_metal2():
    """Two long plates with a narrow dark seam down the middle."""
    return [floor_backing(), tile("a", -198, -198, -6, 198), tile("b", 6, -198, 198, 198)]


def floor_center():
    """A frame of plates round a recessed dark centre plate."""
    return [floor_backing(), tile("c", -110, -110, 110, 110, "Gunmetal", 2.0),
            tile("n", -198, 116, 198, 198), tile("s", -198, -198, 198, -116),
            tile("w", -198, -110, -116, 110), tile("e", 116, -110, 198, 110)]


def flat(name, pts_xy, mat="Steel", depth=4.0, bevel=1.0):
    """A floor plate: polygon (x, y) on the floor, from depth cm below Z = 0 up to it."""
    import bmesh
    import bpy
    bm = bmesh.new()
    top = [bm.verts.new((x, y, 0.0)) for x, y in pts_xy]
    bottom = [bm.verts.new((x, y, -depth)) for x, y in pts_xy]
    bm.faces.new(top)
    bm.faces.new(list(reversed(bottom)))
    n = len(pts_xy)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((top[j], top[i], bottom[i], bottom[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    return sb_kit._finish(sb_kit._link(name, mesh), mat, bevel)


def floor_x():
    """Plates split by a dark X: four triangles from the edges to the middle."""
    return [floor_backing(),
            flat("s", [(-186, -198), (186, -198), (0, -12)]), flat("n", [(186, 198), (-186, 198), (0, 12)]),
            flat("w", [(-198, 186), (-198, -186), (-12, 0)]), flat("e", [(198, -186), (198, 186), (12, 0)])]


def floor_plates():
    """Three long strips."""
    return [floor_backing(), tile("a", -198, -198, 198, -70), tile("b", -198, -64, 198, 64), tile("c", -198, 70, 198, 198)]


def floor_squares():
    """Four squares with their outer corners cut."""
    parts = [floor_backing()]
    for i, (x0, y0) in enumerate([(-198, -198), (5, -198), (-198, 5), (5, 5)]):
        parts.append(tile("s%d" % i, x0, y0, x0 + 193, y0 + 193, bevel=6.0))
    return parts


def floor_dark():
    return [tile("dark", -200, -200, 200, 200, "Gunmetal", 4.0, bevel=0.0)]


PIECES = {
    "WallMetal": wall_metal, "WallBand": wall_band, "WallDark": wall_dark, "WallPlate": wall_plate,
    "BandPlates": band_plates, "BandAccent": band_accent, "BandDark": band_dark, "BandTriangles": band_triangles,
    "FloorMetal": floor_metal, "FloorMetal2": floor_metal2, "FloorCenter": floor_center, "FloorX": floor_x,
    "FloorPlates": floor_plates, "FloorSquares": floor_squares, "FloorDark": floor_dark,
}


def build(render=None):
    os.makedirs(OUT, exist_ok=True)
    report = []
    for name, make in PIECES.items():
        sb_kit.reset_scene()
        obj = sb_kit.join("SM_F_" + name, make())
        line = "%s: %d tris, %s, slots %s" % (obj.name, sb_kit.triangle_count(obj), sb_kit.bounds(obj),
                                              ", ".join(m.name for m in obj.data.materials))
        print(line)
        report.append(line)
        sb_kit.export_fbx(os.path.join(OUT, obj.name + ".fbx"), [obj])
        if render:
            import sb_render
            sb_render.render_views([obj], os.path.join(render, "forerunner_%s.png" % name),
                                   [((1.0, -0.6, 0.35), False)] if name.startswith(("Wall", "Band")) else [((0.4, -0.5, 1.0), False)])
    with open(os.path.join(OUT, "forerunnerkit_report.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


if __name__ == "__main__":
    build(render=next((a.split("=", 1)[1] for a in sys.argv if a.startswith("--render=")), None))
