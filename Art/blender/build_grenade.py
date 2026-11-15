"""Builds Shieldbreak's frag grenade (original design) and exports Art/Export/Weapons/SM_SB_FragGrenade.fbx.

    python build_grenade.py [--render [preview_dir]]

A squat ovoid shell (about 7 cm across, 8.4 cm tall) split into six fragmentation lobes by recessed seams, a raised
equator band with grip ribs, a steel fuse collar on top with an amber arming light (slot GlowAmber, blinked by the
game through EmissiveStrength) and a steel base plug. Origin at the centre of mass, +Z up (the fuse), so it spins
about its own middle in flight. About 900 triangles; the importer adds the Weapons LODs.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit as k  # noqa: E402

import bmesh  # noqa: E402  (after sb_kit, which imports bpy first)
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402


def lathe(name, profile_rz, mat, segments=24, smooth=True):
    """Surface of revolution about Z from a (radius, z) polyline, bottom to top."""
    bm = bmesh.new()
    rings = []
    for r, z in profile_rz:
        ring = []
        for s in range(segments):
            a = 2.0 * math.pi * s / segments
            ring.append(bm.verts.new((r * math.cos(a), r * math.sin(a), z)))
        rings.append(ring)
    for i in range(len(rings) - 1):
        for s in range(segments):
            t = (s + 1) % segments
            bm.faces.new((rings[i][s], rings[i][t], rings[i + 1][t], rings[i + 1][s]))
    for ring, flip in ((rings[0], True), (rings[-1], False)):
        if ring[0].co.xy.length > 1e-4:
            bm.faces.new(list(reversed(ring)) if flip else ring)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = k._link(name, mesh)
    for p in obj.data.polygons:
        p.use_smooth = smooth
    obj.data.materials.append(k.material(mat))
    return obj


def shell_profile(rings=11, radius=3.5, half_height=3.6, seam_depth=0.0):
    """A squashed sphere, a little fuller below the equator (it sits on its base)."""
    out = []
    for i in range(rings + 1):
        t = i / rings
        phi = -math.pi / 2 + math.pi * t
        r = math.cos(phi) * radius * (1.0 + 0.05 * math.cos(phi) * (1 if phi < 0 else 0.4))
        z = math.sin(phi) * half_height
        out.append((max(r - seam_depth, 0.0), z))
    return out


def grenade():
    p = []
    # Six lobes: the shell is built as six slices with recessed seams between them (a slice is a lathe over 1/6 turn,
    # inset at its edges), so each fragmentation panel catches the light separately.
    lobes = 6
    per = 6
    prof = shell_profile()
    bm = bmesh.new()
    for lobe in range(lobes):
        grid = []
        for i, (r, z) in enumerate(prof):
            row = []
            for s in range(per + 1):
                u = s / per
                a = 2 * math.pi * (lobe + u) / lobes
                # Edge inset: the seams sink 0.28 cm, the lobe crowns bulge 0.1 cm.
                edge = 1.0 - abs(2 * u - 1)
                rr = r - 0.28 * (1 - min(1.0, edge * 4.0)) + 0.1 * math.sin(math.pi * u) * (r / 3.5)
                row.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a), z)))
            grid.append(row)
        for i in range(len(grid) - 1):
            for s in range(per):
                bm.faces.new((grid[i][s], grid[i][s + 1], grid[i + 1][s + 1], grid[i + 1][s]))
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new("shell")
    bm.to_mesh(mesh)
    bm.free()
    shell = k._link("shell", mesh)
    for f in shell.data.polygons:
        f.use_smooth = True
    shell.data.materials.append(k.material("Olive"))
    p.append(shell)

    # Equator band with twelve grip ribs.
    p.append(lathe("band", [(3.55, -0.75), (3.78, -0.6), (3.78, 0.6), (3.55, 0.75)], "WeaponMetal", segments=32))
    for i in range(12):
        a = 2 * math.pi * (i + 0.5) / 12
        rib = k.box("rib", (0, 0, 0), (0.24, 0.5, 1.15), "Polymer", bevel=0)
        rib.matrix_world = Matrix.Rotation(a, 4, "Z") @ Matrix.Translation(Vector((3.84, 0, 0)))
        p.append(rib)

    # Fuse collar, arming light and the spoon-less cap (original: a recessed amber ring under a steel crown).
    p.append(lathe("collar", [(1.9, 3.0), (1.75, 3.55), (1.75, 3.9), (1.45, 4.05)], "Steel", segments=20))
    p.append(lathe("light", [(1.45, 4.05), (1.5, 4.25), (1.35, 4.4)], "GlowAmber", segments=20))
    p.append(lathe("crown", [(1.35, 4.4), (1.2, 4.75), (0.7, 4.95), (0.0, 5.0)], "Steel", segments=20))
    # Pull-ring stub on one side of the crown.
    stub = k.ring("pull", (0, 0, 0), 0.75, 0.16, "Steel", axis="Y", verts=12, tube_verts=5)
    stub.matrix_world = Matrix.Translation(Vector((1.5, 0, 4.55)))
    p.append(stub)

    # Base plug.
    p.append(lathe("plug", [(0.0, -3.85), (1.3, -3.8), (1.55, -3.45), (1.75, -3.25)], "Steel", segments=20))
    return p


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(here, "..", "Export", "Weapons")
    os.makedirs(out_dir, exist_ok=True)
    k.reset_scene()
    obj = k.join("SM_SB_FragGrenade", grenade())
    # Recentre on the shell's middle (the fuse crown makes the bounds taller on top).
    report = "SM_SB_FragGrenade: %d tris, %d materials [%s], bounds %s" % (
        k.triangle_count(obj), len(obj.data.materials), ", ".join(m.name for m in obj.data.materials), k.bounds(obj))
    print(report)
    k.export_fbx(os.path.join(out_dir, "SM_SB_FragGrenade.fbx"), [obj])
    with open(os.path.join(out_dir, "grenade_report.txt"), "w") as f:
        f.write(report + "\n")
    if "--render" in sys.argv:
        args = [a for a in sys.argv[1:] if not a.startswith("--")]
        preview_dir = os.path.abspath(args[0]) if args else os.path.join(here, "previews")
        os.makedirs(preview_dir, exist_ok=True)
        import sb_render
        sb_render.render_object(obj, os.path.join(preview_dir, "SM_SB_FragGrenade.png"))


if __name__ == "__main__":
    main()
