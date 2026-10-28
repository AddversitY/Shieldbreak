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


reference = signed_volume("/Engine/BasicShapes/Cube.Cube")
report("reference /Engine/BasicShapes/Cube volume %.0f" % reference)
bad = 0
registry = unreal.AssetRegistryHelpers.get_asset_registry()
for folder in ("/Game/Shieldbreak/Art/Weapons", "/Game/Shieldbreak/Art/Armor"):
    for data in registry.get_assets_by_path(folder, recursive=True):
        if str(data.asset_class_path.asset_name) != "StaticMesh":
            continue
        path = str(data.package_name) + "." + str(data.asset_name)
        volume = signed_volume(path)
        ok = (volume > 0) == (reference > 0)
        bad += 0 if ok else 1
        report("%s %s volume %.0f" % ("OK  " if ok else "FLIP", data.asset_name, volume))
report("RESULT %s" % ("all outward" if bad == 0 else "%d inside out" % bad))
with open(OUT, "w") as f:
    f.write("\n".join(lines) + "\n")
