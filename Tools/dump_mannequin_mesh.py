"""Writes the mannequin's LOD0 skin (positions + triangles) as Saved/ArtExport/SKM_Manny.obj, headless.

The skeletal FBX exporter needs render data and asserts in a commandlet on the laptop, so this reads the mesh
description through Geometry Script instead (GeometryScripting plugin). Coordinates are Unreal mesh space, in cm.
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "ArtExport")
os.makedirs(OUT, exist_ok=True)
REPORT = os.path.join(OUT, "dump_report.txt")
MANNY = "/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny"


def report(text):
    unreal.log("DumpMannequin: " + text)
    with open(REPORT, "a") as f:
        f.write(text + "\n")


open(REPORT, "w").close()
manny = unreal.load_asset(MANNY)
report("loaded %s" % manny)
mesh = unreal.DynamicMesh()
options = unreal.GeometryScriptCopyMeshFromAssetOptions()
lod = unreal.GeometryScriptMeshReadLOD()
lod.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
lod.lod_index = 0
result = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(manny, mesh, options, lod)
report("copy result %s" % (result,))


def unpack_list(value, what):
    """5.8 returns a tuple (e.g. the list plus a has-gaps flag) from these queries; older versions return the list
    itself, possibly wrapped in a struct with a .list member. Logs the shape so a failed run says what came back."""
    items = value if isinstance(value, tuple) else (value,)
    report("%s returned %s" % (what, [type(x).__name__ for x in items]))
    for x in items:
        if hasattr(x, "list"):
            return list(x.list)
        if isinstance(x, (list, unreal.Array)):
            return list(x)
    for x in items:
        if not isinstance(x, (bool, int, float, str, unreal.DynamicMesh)):
            try:
                return list(x)
            except TypeError:
                pass
    raise RuntimeError("no list in %s result" % what)


# Keep the dynamic mesh's vertex IDs (no gap skipping) so triangle indices line up; write only vertices that a
# triangle uses (the copy leaves some unused ones behind).
points = unpack_list(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False), "vertex positions")
tris = unpack_list(unreal.GeometryScript_MeshQueries.get_all_triangle_indices(mesh, True), "triangle indices")
used = sorted({i for t in tris for i in (t.x, t.y, t.z)})
remap = {old: new for new, old in enumerate(used)}
report("vertices %d (%d used), triangles %d" % (len(points), len(used), len(tris)))
with open(os.path.join(OUT, "SKM_Manny.obj"), "w") as f:
    f.write("# SKM_Manny LOD0, Unreal mesh space (cm)\n")
    for i in used:
        p = points[i]
        f.write("v %.3f %.3f %.3f\n" % (p.x, p.y, p.z))
    for t in tris:
        f.write("f %d %d %d\n" % (remap[t.x] + 1, remap[t.y] + 1, remap[t.z] + 1))
report("OK SKM_Manny.obj")
