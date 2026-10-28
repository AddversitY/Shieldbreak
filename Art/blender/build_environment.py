"""Environment art for the space outside the map walls (run headless: python3 Art/blender/build_environment.py).

SM_SB_Mountains: a faceted, low-poly ring of mountains around the origin. The inner edge sits at 100 m (10000 cm)
and AShieldbreakArena scales the ring per map so it starts just past the walls. Heights come from periodic noise
around the ring (so it closes without a seam) under an envelope that rises from the inner edge to the main range
and falls to lower hills behind it. Exported to Art/Export/Environment for Tools/ImportArt.bat.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit  # noqa: E402  (imports bpy first)

import bmesh  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Export", "Environment")
INNER, OUTER = 10000.0, 40000.0
ANGULAR, RADIAL = 192, 22
rng = np.random.default_rng(11)


def ring_noise(theta, octaves):
    """Periodic in theta: a sum of sines with whole-number frequencies and random phases, in about -1..1."""
    total = np.zeros_like(theta)
    amplitude = 1.0
    for frequency in octaves:
        total += amplitude * np.sin(frequency * theta + rng.uniform(0, 2 * math.pi))
        amplitude *= 0.55
    return total / 2.0


def heights():
    theta = np.linspace(0, 2 * math.pi, ANGULAR, endpoint=False)[:, None]
    t = np.linspace(0, 1, RADIAL)[None, :]  # 0 at the inner edge, 1 at the outer edge
    # Main range peaks about a third of the way out; hills behind it.
    envelope = np.sin(np.clip(t / 0.38, 0, 1) * math.pi / 2) ** 1.5 * (1.0 - 0.6 * np.clip((t - 0.38) / 0.62, 0, 1))
    envelope[:, 0] = 0.0
    ridge = 1.0 - np.abs(ring_noise(theta + 0 * t, [3, 7, 13, 29]) + 0.25 * np.sin(t * 9 + theta * 5))
    peaks = 0.55 + 0.45 * ring_noise(theta + 0 * t, [2, 5, 11])
    h = 9000.0 * envelope * np.clip(ridge * peaks, 0.08, 1.2)
    h += rng.uniform(-150, 150, h.shape) * (h > 300)
    return np.maximum(h, 0.0)


def build():
    sb_kit.reset_scene()
    h = heights()
    mesh = bpy.data.meshes.new("SM_SB_Mountains")
    bm = bmesh.new()
    verts = []
    for i in range(ANGULAR):
        row = []
        for j in range(RADIAL):
            r = INNER + (OUTER - INNER) * j / (RADIAL - 1)
            # A little jitter so the facets don't line up on a polar grid.
            a = 2 * math.pi * (i + (rng.uniform(-0.25, 0.25) if 0 < j < RADIAL - 1 else 0)) / ANGULAR
            r += rng.uniform(-250, 250) if 0 < j < RADIAL - 1 else 0
            row.append(bm.verts.new((r * math.cos(a), r * math.sin(a), float(h[i, j]))))
        verts.append(row)
    for i in range(ANGULAR):
        n = (i + 1) % ANGULAR
        for j in range(RADIAL - 1):
            a, b, c, d = verts[i][j], verts[n][j], verts[n][j + 1], verts[i][j + 1]
            # Faces wound counter-clockwise seen from above (normals up).
            bm.faces.new((a, c, b))
            bm.faces.new((a, d, c))
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("SM_SB_Mountains", mesh)
    bpy.context.scene.collection.objects.link(obj)
    for poly in mesh.polygons:
        poly.use_smooth = False  # faceted
    mesh.materials.append(sb_kit.material("Sand"))
    up = sum(1 for p in mesh.polygons if p.normal.z > 0)
    print("SM_SB_Mountains: %d tris (%d facing up), %s, peak %.0f cm" % (sb_kit.triangle_count(obj), up, sb_kit.bounds(obj), h.max()))
    os.makedirs(OUT, exist_ok=True)
    sb_kit.export_fbx(os.path.join(OUT, "SM_SB_Mountains.fbx"), [obj])
    return obj


if __name__ == "__main__":
    build()
