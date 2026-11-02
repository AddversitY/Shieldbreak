"""Rock skin tiles for the outdoor maps' rock and sand blocks (run headless: python3 Art/blender/build_rock_skin.py
[--render=<folder>]).

The layouts are boxes (collision stays exactly as authored). These tiles dress their faces so the mesas, banks,
hoodoos and boulders read as weathered sandstone instead of greybox. Visual only: ShieldbreakMapArt::Cladding lays
them over each visible face in a grid, stretched to fill it, and the arena draws them instanced with no collision
and no shadow casting.

  RockFaceA/B/C  A 400 x 400 face tile facing +X with its back on x = 0 (the block's face). The edges tuck 8 cm
                 into the block and overlap the neighbouring tiles by 15 cm, so there are no seams; the middle
                 bulges out up to 32 cm with horizontal sandstone strata and noise. Three variants, so the grid
                 does not repeat. 240 triangles each.
  RockCap        A 400 x 400 top tile for small rocks (cover, pillars, hoodoos): a low, faceted dome up to 12 cm
                 high whose rim tucks 6 cm below the top.

Pivot: face tiles on the middle of their bottom edge (y = 0, z = 0); the cap on its centre at the top surface.
One slot, M_Sand, tinted per map at runtime. Exported to Art/Export/MapKit with rockskin_report.txt.
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit  # noqa: E402  (imports bpy first)
import build_map_kit as kit  # noqa: E402

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Vector, noise  # noqa: E402

OUT = kit.OUT
NY, NZ = 7, 21          # grid vertices across and up: 6 x 20 quads, 240 triangles (two rows per sandstone bed)
HALF, TOP, OVER = 200.0, 400.0, 15.0
BULGE, TUCK = 32.0, 8.0


def grid_mesh(name, points, rows, cols):
    bm = bmesh.new()
    verts = [[bm.verts.new(points[r][c]) for c in range(cols)] for r in range(rows)]
    for r in range(rows - 1):
        for c in range(cols - 1):
            bm.faces.new((verts[r][c], verts[r][c + 1], verts[r + 1][c + 1], verts[r + 1][c]))
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = sb_kit._link(name, mesh)
    for p in obj.data.polygons:
        p.use_smooth = False
    obj.data.materials.append(sb_kit.material("Sand"))
    return obj


def face_tile(name, seed):
    rng = random.Random(seed)
    offset = Vector((rng.uniform(-60, 60), rng.uniform(-60, 60), rng.uniform(-60, 60)))
    band_phase = rng.uniform(0, math.tau)
    rows = []
    for r in range(NZ):
        t = r / (NZ - 1)
        row = []
        for c in range(NY):
            s = c / (NY - 1)
            y = -HALF - OVER + s * 2 * (HALF + OVER)
            z = -OVER * 0.5 + t * (TOP + OVER)
            # Jitter the inner vertices so the facets are irregular.
            if 0 < c < NY - 1:
                y += rng.uniform(-14, 14)
            if 0 < r < NZ - 1:
                z += rng.uniform(-4, 4)
            edge = min(s, 1 - s, t, 1 - t)                     # 0 on the rim, 0.5 in the middle
            fall = min(1.0, edge / 0.18)                       # rim tucks in, the body bulges
            p = Vector((0.0, y, z))
            n = noise.noise(p * 0.006 + offset) * 0.6 + noise.noise(p * 0.017 + offset) * 0.3
            # Sandstone strata: each ~80 cm bed leans out towards its top, then steps back (an undercut ledge).
            bed = (z / 80.0 + band_phase + 0.15 * n) % 1.0
            x = (0.25 + 0.3 * n + 0.5 * bed) * BULGE
            row.append((-TUCK + (x + TUCK) * fall, y, z))
        rows.append(row)
    return grid_mesh(name, rows, NZ, NY)


def cap_tile(name, seed):
    rng = random.Random(seed)
    offset = Vector((rng.uniform(-60, 60), rng.uniform(-60, 60), rng.uniform(-60, 60)))
    n_side = 9
    rows = []
    for r in range(n_side):
        row = []
        for c in range(n_side):
            u, v = r / (n_side - 1), c / (n_side - 1)
            x = -HALF - OVER + u * 2 * (HALF + OVER)
            y = -HALF - OVER + v * 2 * (HALF + OVER)
            if 0 < r < n_side - 1:
                x += rng.uniform(-14, 14)
            if 0 < c < n_side - 1:
                y += rng.uniform(-14, 14)
            edge = min(u, 1 - u, v, 1 - v)
            fall = min(1.0, edge / 0.2)
            n = noise.noise(Vector((x, y, 0)) * 0.008 + offset) * 0.5 + 0.5
            row.append((x, y, -6.0 + (6.0 + 12.0 * (0.4 + 0.6 * n)) * fall))
        rows.append(row)
    return grid_mesh(name, rows, n_side, n_side)


def rock_face_a():
    return face_tile("SM_SB_Kit_RockFaceA", 11)


def rock_face_b():
    return face_tile("SM_SB_Kit_RockFaceB", 23)


def rock_face_c():
    return face_tile("SM_SB_Kit_RockFaceC", 37)


def rock_cap():
    return cap_tile("SM_SB_Kit_RockCap", 51)


PIECES = (rock_face_a, rock_face_b, rock_face_c, rock_cap)


def build(render=None):
    os.makedirs(OUT, exist_ok=True)
    report = []
    for make in PIECES:
        sb_kit.reset_scene()
        obj = make()
        slots = ", ".join(m.name for m in obj.data.materials)
        line = "%s: %d tris, %s, slots %s" % (obj.name, sb_kit.triangle_count(obj), sb_kit.bounds(obj), slots)
        print(line)
        report.append(line)
        sb_kit.export_fbx(os.path.join(OUT, obj.name + ".fbx"), [obj])
        if render:
            import sb_render
            sb_render.render_views([obj], os.path.join(render, "rockskin_%s.png" % make.__name__),
                                   [((1.0, -0.9, 0.45), False), ((1.0, 0.0, 0.0), True)], floor=False)
    with open(os.path.join(OUT, "rockskin_report.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


if __name__ == "__main__":
    build(render=next((a.split("=", 1)[1] for a in sys.argv if a.startswith("--render=")), None))
