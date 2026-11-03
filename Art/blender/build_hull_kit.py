"""Keel's own cladding kit: heavy warship hull plating (run headless:
python3 Art/blender/build_hull_kit.py [--render=<folder>]).

Original pieces, drawn here, like Spire's Forerunner kit (build_forerunner_kit.py): the same piece names, sizes and
pivots as the Quaternius panels, so ShieldbreakMapArt::Cladding's recipes place them unchanged. A map whose look sets
CladKit = "H" loads SM_H_<piece> from Art/MapKit (AShieldbreakArena::BuildCladding).

Where Spire's plates are smooth, pale and leaning, Keel's are a ship's insides: thick rectangular hull plates in
horizontal courses, structural frames standing proud every couple of metres, louvred amber light slots and
alternating tread bands. Low poly: plain boxes and extrusions, no bevels on the small parts.

Mesh space as in build_forerunner_kit.py: walls face +X from X = 0 (400 cm wide, centred on Y, base Z = 0), floors
have their walking surface at Z = 0. Slots M_HullPlate, M_HullFrame and M_HullDeck are fixed dark palette colours
(sb_kit.PALETTE) that the game does not retint; M_HullLight is the (dimmer) amber light.

Exported to Art/Export/MapKit (SM_H_<piece>.fbx) with hullkit_report.txt.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit  # noqa: E402  (imports bpy first)
from build_forerunner_kit import OUT, chamfer, plate  # noqa: E402
from build_forerunner_kit import tile as _tile  # noqa: E402


def tile(name, x0, y0, x1, y1, mat="HullDeck", depth=4.0, bevel=1.0):
    return _tile(name, x0, y0, x1, y1, mat, depth, bevel)


def floor_backing():
    """The dark deck under the seams, its top 1 cm below the plates'."""
    return sb_kit.box("fback", (0.0, 0.0, -3.5), (400.0, 400.0, 5.0), "HullFrame", bevel=0.0)

W = 200.0       # half width of a wall piece
GLOW = "HullLight"


def back(height):
    """The dark hull skin behind everything (2 cm), which the seams between the plates show."""
    return plate("back", [(-W, 0), (W, 0), (W, height), (-W, height)], "HullFrame", 2.0, bevel=0.0)


def rect(name, y0, z0, y1, z1, mat="HullPlate", depth=6.0, bevel=1.0):
    return plate(name, [(y0, z0), (y1, z0), (y1, z1), (y0, z1)], mat, depth, bevel)


def frame(name, y, height, width=16.0, depth=10.0):
    """A structural frame standing proud of the plates, the full height of the piece."""
    return rect(name, y - width * 0.5, 0.0, y + width * 0.5, height, "HullFrame", depth, bevel=1.5)


# ---- Wall rows (200 cm) ----

def wall_metal():
    """Two courses of hull plates, offset like brickwork, with a frame at each end."""
    parts = [back(200),
             rect("a", -184, 4, -2, 98), rect("b", 2, 4, 184, 98),
             rect("c", -184, 102, -82, 196), rect("d", -78, 102, 184, 196)]
    return parts + [frame("fl", -192, 200), frame("fr", 192, 200)]


def wall_band():
    """Plates above and below a louvred amber light slot."""
    parts = [back(200), rect("low", -196, 4, 196, 78), rect("high", -196, 122, 196, 196),
             rect("slot", -186, 86, 186, 114, GLOW, 3.0, bevel=0.0)]
    for i in range(12):
        y = -176 + i * 32.0
        parts.append(rect("louvre%d" % i, y - 4, 82, y + 4, 118, "HullFrame", 8.0, bevel=0.0))
    return parts


def wall_dark():
    """Dark hull skin with a frame down the middle: the lower, recessed walls."""
    return [back(200), rect("dark", -196, 4, 196, 196, "HullFrame", 5.0), frame("f", 0, 200, 20.0, 12.0)]


def wall_plate():
    """A pressure hatch: a dark recessed door plate in a heavy steel surround (the team bases)."""
    return [back(200),
            plate("door", chamfer(-120, 20, 120, 180, 22), "HullFrame", 3.0, bevel=0.8),
            rect("sb", -196, 4, 196, 20, depth=9.0), rect("st", -196, 180, 196, 196, depth=9.0),
            rect("sl", -196, 20, -120, 180, depth=9.0), rect("sr", 120, 20, 196, 180, depth=9.0),
            rect("lamp", -60, 186, 60, 192, GLOW, 10.0, bevel=0.0)]


# ---- Band rows (100 cm) ----

def band_plates():
    return [back(100), rect("a", -196, 4, -4, 96), rect("b", 4, 4, 196, 96)]


def band_accent():
    """A plate with an amber line under a heavy rail: the tops of walls and slab edges."""
    return [back(100), rect("plate", -196, 4, 196, 62),
            rect("line", -196, 68, 196, 76, GLOW, 4.0, bevel=0.0),
            rect("rail", -200, 82, 200, 100, "HullFrame", 10.0, bevel=1.5)]


def band_dark():
    return [back(100), rect("dark", -196, 4, 196, 96, "HullFrame", 5.0)]


def band_triangles():
    """Tread bands: alternating light and dark slanted stripes, the ship's hazard marking in its own two tones."""
    parts = [back(100)]
    for i in range(8):
        y = -200 + i * 50.0
        mat = "HullPlate" if i % 2 == 0 else "HullFrame"
        pts = [(y, 4), (y + 50, 4), (y + 50 - 30, 96), (y - 30, 96)]
        parts.append(plate("s%d" % i, [(max(min(p[0], W), -W), p[1]) for p in pts], mat, 5.0, bevel=0.0))
    return parts


# ---- Floor plates (400 x 400, top at Z = 0) ----

def floor_metal():
    """Four deck plates round a dark seam."""
    return [floor_backing()] + [tile("q%d" % i, *r) for i, r in
                                enumerate([(-198, -198, -4, -4), (4, -198, 198, -4), (-198, 4, -4, 198), (4, 4, 198, 198)])]


def floor_metal2():
    """Grating: eight slats over the dark deck."""
    return [floor_backing()] + [tile("g%d" % i, -198, y, 198, y + 34, depth=4.0, bevel=0.0)
                                for i, y in enumerate(range(-196, 196, 49))]


def floor_center():
    """A dark hatch in a steel surround."""
    return [floor_backing(), tile("c", -120, -120, 120, 120, "HullFrame", 2.0),
            tile("n", -198, 126, 198, 198), tile("s", -198, -198, 198, -126),
            tile("w", -198, -120, -126, 120), tile("e", 126, -120, 198, 120)]


def floor_x():
    """Plates either side of a wide dark walkway."""
    return [floor_backing(), tile("a", -198, -198, -40, 198), tile("b", 40, -198, 198, 198)]


def floor_plates():
    """Long plates across the deck."""
    return [floor_backing(), tile("a", -198, -198, 198, -102), tile("b", -198, -98, 198, -2),
            tile("c", -198, 2, 198, 98), tile("d", -198, 102, 198, 198)]


def floor_squares():
    """Nine tread squares."""
    parts = [floor_backing()]
    for i in range(3):
        for j in range(3):
            x0, y0 = -198 + i * 133.0, -198 + j * 133.0
            parts.append(tile("t%d%d" % (i, j), x0, y0, x0 + 129, y0 + 129, bevel=0.0))
    return parts


def floor_dark():
    return [tile("dark", -200, -200, 200, 200, "HullFrame", 4.0, bevel=0.0)]


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
        obj = sb_kit.join("SM_H_" + name, make())
        line = "%s: %d tris, %s, slots %s" % (obj.name, sb_kit.triangle_count(obj), sb_kit.bounds(obj),
                                              ", ".join(m.name for m in obj.data.materials))
        print(line)
        report.append(line)
        sb_kit.export_fbx(os.path.join(OUT, obj.name + ".fbx"), [obj])
        if render:
            import sb_render
            sb_render.render_views([obj], os.path.join(render, "hull_%s.png" % name),
                                   [((1.0, -0.6, 0.35), False)] if name.startswith(("Wall", "Band")) else [((0.4, -0.5, 1.0), False)])
    with open(os.path.join(OUT, "hullkit_report.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


if __name__ == "__main__":
    build(render=next((a.split("=", 1)[1] for a in sys.argv if a.startswith("--render=")), None))
