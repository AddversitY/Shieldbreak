"""Checks that the imported art meshes face outward: writes Saved/Logs/CheckWinding.txt, headless.

Each mesh's signed volume (from its triangle winding) should have the same sign as the engine's cube. An opposite
sign means the mesh is inside out (Art/blender/sb_kit.py export_fbx sets the winding). Needs GeometryScripting.
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "CheckWinding.txt")
lines = []


def report(text):
    unreal.log("CheckWinding: " + text)
    lines.append(text)


def as_list(value):
    """The queries return (mesh, script list, has gaps) on 5.8; the script lists convert through GeometryScript_List."""
    lists = getattr(unreal, "GeometryScript_List", None) or unreal.GeometryScript_ListUtils
    converters = {"GeometryScriptVectorList": "convert_vector_list_to_array",
                  "GeometryScriptTriangleList": "convert_triangle_list_to_array"}
    for x in value if isinstance(value, tuple) else (value,):
        if isinstance(x, (list, unreal.Array)):
            return list(x)
        convert = converters.get(type(x).__name__)
        if convert:
            out = getattr(lists, convert)(x)
            for o in out if isinstance(out, tuple) else (out,):
                if isinstance(o, (list, unreal.Array)):
                    return list(o)
    raise RuntimeError("no list in %s" % (value,))


def measures(path):
    """(signed volume, outward share). The volume is only meaningful for closed meshes; open shells such as a waist
    ring give an origin-dependent sign, so the share of triangle area whose winding normal points away from the
    mesh's centre (heights squashed, so rings and tubes count sideways) decides those."""
    asset = unreal.load_asset(path)
    mesh = unreal.DynamicMesh()
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(asset, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(), lod)
    pts = [(p.x, p.y, p.z) for p in as_list(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False))]
    tris = [(t.x, t.y, t.z) for t in as_list(unreal.GeometryScript_MeshQueries.get_all_triangle_indices(mesh, False))]
    volume, faces, total_area = 0.0, [], 0.0
    cx = cy = cz = 0.0
    for i, j, k in tris:
        a, b, c = pts[i], pts[j], pts[k]
        volume += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6.0
        u = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        v = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        area = (n[0] ** 2 + n[1] ** 2 + n[2] ** 2) ** 0.5 / 2
        m = ((a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3)
        faces.append((n, m, area))
        total_area += area
        cx += m[0] * area
        cy += m[1] * area
        cz += m[2] * area
    if total_area <= 0:
        return volume, 0.5
    cx, cy, cz = cx / total_area, cy / total_area, cz / total_area
    out = sum(area for n, m, area in faces if n[0] * (m[0] - cx) + n[1] * (m[1] - cy) + n[2] * (m[2] - cz) * 0.3 > 0)
    return volume, out / total_area


def signed_volume(path):
    asset = unreal.load_asset(path)
    mesh = unreal.DynamicMesh()
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(asset, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(), lod)
    pts = as_list(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False))
    tris = as_list(unreal.GeometryScript_MeshQueries.get_all_triangle_indices(mesh, False))
    total = 0.0
    for t in tris:
        a, b, c = pts[t.x], pts[t.y], pts[t.z]
        total += (a.x * (b.y * c.z - b.z * c.y) - a.y * (b.x * c.z - b.z * c.x) + a.z * (b.x * c.y - b.y * c.x)) / 6.0
    return total


reference, reference_share = measures("/Engine/BasicShapes/Cube.Cube")
report("reference /Engine/BasicShapes/Cube volume %.0f" % reference)
bad = 0
registry = unreal.AssetRegistryHelpers.get_asset_registry()
for folder in ("/Game/Shieldbreak/Art/Weapons", "/Game/Shieldbreak/Art/Armor"):
    for data in registry.get_assets_by_path(folder, recursive=True):
        if str(data.asset_class_path.asset_name) != "StaticMesh":
            continue
        path = str(data.package_name) + "." + str(data.asset_name)
        volume, share = measures(path)
        ok = (share > 0.5) == (reference_share > 0.5)
        bad += 0 if ok else 1
        note = "" if (volume > 0) == (reference > 0) else " (volume sign disagrees: open shell)"
        report("%s %s outward %.0f%% volume %.0f%s" % ("OK  " if ok else "FLIP", data.asset_name, 100 * share, volume, note))
report("RESULT %s" % ("all outward" if bad == 0 else "%d inside out" % bad))
with open(OUT, "w") as f:
    f.write("\n".join(lines) + "\n")
