"""Import only the original Spire ceiling architecture and validate axes, dimensions and triangle counts."""
from pathlib import Path
import json
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE = ROOT / 'Art/Export/SpireArchitecture'
DEST = '/Game/Shieldbreak/Art/Spire'
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.OBJ 0')
manifest = json.loads((SOURCE/'architecture.json').read_text(encoding='utf-8'))
lines = []
for name, expected in manifest.items():
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE/(name+'.obj'))
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.import_as_skeletal = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.auto_generate_collision = False
    options.static_mesh_import_data.generate_lightmap_u_vs = False
    options.static_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS
    task.options = options
    task.factory = unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(DEST+'/'+name)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError('Import failed: '+name)
    bounds = mesh.get_bounding_box()
    for actual, want in ((bounds.min, expected['min']), (bounds.max, expected['max'])):
        if any(abs(a-b) > .1 for a,b in zip((actual.x, actual.y, actual.z), want)):
            raise RuntimeError(f'{name}: axes/bounds differ: {bounds.min} .. {bounds.max}')
    if mesh.get_num_triangles(0) != expected['triangles']:
        raise RuntimeError(name+': triangle count changed during import')
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    lines.append(f"{name}: {expected['triangles']} triangles, bounds verified")
lines.append(f'RESULT OK: {len(manifest)} original architecture meshes')
(ROOT/'Saved/Logs/SpireArchitectureImport.txt').write_text('\n'.join(lines)+'\n', encoding='utf-8')
unreal.log('\n'.join(lines))
