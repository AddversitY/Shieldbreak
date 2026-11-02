"""Rock skin tiles for the outdoor maps' rock and sand blocks (run headless: python3 Art/blender/build_rock_skin.py
[--render=<folder>]).

The layouts are boxes (collision stays exactly as authored). These tiles dress their faces so the mesas, banks,
hoodoos and boulders read as weathered sandstone instead of greybox. Visual only: ShieldbreakMapArt::Cladding lays
them over each visible face in a grid, stretched to fill it, and the arena draws them instanced with no collision
and no shadow casting.

  RockFaceA/B/C  A 400 x 400 face tile facing +X with its back on x = 0 (the block's face). The edges tuck 8 cm
                 into the block and overlap the neighbouring tiles by 15 cm, so there are no seams; the middle
                 bulges out up to about 35 cm in broad lumps, with uneven sandstone beds and the odd vertical joint. Three variants, so the grid
                 does not repeat. 320 triangles each.
  RockCorner     A noisy vertical column (radius ~45 cm, 400 tall, 160 triangles) on each vertical edge, so the
                 boxes lose their sharp corners.
  RockCap        A 400 x 400 top tile for small rocks (cover, pillars, hoodoos): a low, faceted dome up to 12 cm
                 high whose rim tucks 6 cm below the top.

  StoneFaceA/B   Dressed-stone (ashlar) face tiles for the fortress blocks: four courses of blocks of
                 uneven length, 4-9 cm proud, over a backing plate. Same grid and pivot as the rock faces.

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
NY, NZ = 9, 21          # grid vertices across and up: 8 x 20 quads, 320 triangles
HALF, TOP, OVER = 200.0, 400.0, 15.0
BULGE, TUCK = 60.0, 8.0


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
            big = noise.noise(p * 0.004 + offset) * 0.5 + 0.5                    # broad lumps, 0..1
            mid = noise.noise(p * 0.013 + offset * 1.7)                           # -1..1
            # Sandstone beds of uneven thickness (~1.1 m, warped), stepping back at each bed's top; how strongly they
            # show varies across the face, so they read as rock strata, not planks.
            warped = z + 45.0 * noise.noise(p * 0.005 + offset * 0.5)
            bed = (warped / 110.0 + band_phase) % 1.0
            show = 0.5 + 0.5 * noise.noise(Vector((0.0, y * 0.006, 3.0)) + offset)
            # The odd vertical joint, pressed in.
            joint = max(0.0, 1.0 - abs(noise.noise(Vector((0.0, y * 0.011, z * 0.0015)) + offset)) * 9.0)
            x = (0.15 + 0.5 * big + 0.12 * mid + 0.28 * bed * show - 0.3 * joint) * BULGE
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


def corner_tile(name, seed):
    """A rounded, noisy vertical column centred on a block's corner (pivot on the corner, base z = 0, 400 tall), so
    the box's sharp vertical edge reads as a weathered rock edge. Radius about 45 cm; the inner part is hidden."""
    rng = random.Random(seed)
    offset = Vector((rng.uniform(-60, 60), rng.uniform(-60, 60), rng.uniform(-60, 60)))
    sides, levels = 8, 11
    bm = bmesh.new()
    rings = []
    for r in range(levels):
        z = -OVER + r / (levels - 1) * (TOP + OVER)        # flush with the block's top, never above it
        ring = []
        for k in range(sides):
            a = math.tau * (k + 0.5) / sides
            d = Vector((math.cos(a), math.sin(a), 0.0))
            n = noise.noise(Vector((d.x * 2.0, d.y * 2.0, z * 0.012)) + offset)
            rad = 45.0 * (1.0 + 0.35 * n)
            ring.append(bm.verts.new((d.x * rad, d.y * rad, z)))
        rings.append(ring)
    for r in range(levels - 1):
        for k in range(sides):
            j = (k + 1) % sides
            bm.faces.new((rings[r][k], rings[r][j], rings[r + 1][j], rings[r + 1][k]))
    bm.faces.new(rings[-1])                                       # closed top, seen from above
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = sb_kit._link(name, mesh)
    for p in obj.data.polygons:
        p.use_smooth = False
    obj.data.materials.append(sb_kit.material("Sand"))
    return obj


def rock_corner():
    return corner_tile("SM_SB_Kit_RockCorner", 67)


def rock_face_a():
    return face_tile("SM_SB_Kit_RockFaceA", 11)


def rock_face_b():
    return face_tile("SM_SB_Kit_RockFaceB", 23)


def rock_face_c():
    return face_tile("SM_SB_Kit_RockFaceC", 37)


def rock_cap():
    return cap_tile("SM_SB_Kit_RockCap", 51)


def ashlar_tile(name, seed):
    """Dressed stone: four courses of blocks of uneven length, each a slab 4-9 cm proud of the face with a
    mortar gap round it, over a backing plate at x = 0. Courses run past the tile's sides by OVER so tiles interlock."""
    rng = random.Random(seed)
    parts = [sb_kit.box("backing", (-2.0, 0.0, TOP / 2.0), (4.0, 2 * (HALF + OVER), TOP + OVER), "Sand", bevel=0.0)]
    courses = 4
    gap = 5.0
    course_h = TOP / courses
    for c in range(courses):
        z0 = c * course_h
        y = -HALF - OVER - rng.uniform(0, 60)
        while y < HALF + OVER:
            length = rng.uniform(70, 150)
            y1 = min(y + length, HALF + OVER)
            y = max(y, -HALF - OVER)
            if y1 - y > 25:
                depth = rng.uniform(4.0, 9.0)
                stone = sb_kit.box("stone", (0.0, 0.0, 0.0), (depth, y1 - y - gap, course_h - gap), "Sand", bevel=0.0)
                kit.placed(stone, (depth / 2.0, (y + y1) / 2.0, z0 + course_h / 2.0), (0, 0, 0))
                parts.append(stone)
            y = y1
    return sb_kit.join(name, parts)


def stone_face_a():
    return ashlar_tile("SM_SB_Kit_StoneFaceA", 71)


def stone_face_b():
    return ashlar_tile("SM_SB_Kit_StoneFaceB", 83)


PIECES = (rock_face_a, rock_face_b, rock_face_c, rock_cap, rock_corner, stone_face_a, stone_face_b)


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
