"""Import only the original Spire modules; does not reimport existing art."""
from pathlib import Path
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.OBJ 0')
report = []
for name in ('M_SB_Surface', 'M_SB_World'):
    material = unreal.load_asset('/Game/Shieldbreak/Art/Materials/'+name)
    if material:
        unreal.MaterialEditingLibrary.set_material_usage(
            material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
for source in sorted((root / 'Art/Export/Spire').glob('*.obj')):
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = '/Game/Shieldbreak/Art/Spire'
    task.destination_name = source.stem
    task.automated = True
    task.replace_existing = True
    task.save = True
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.import_as_skeletal = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.auto_generate_collision = False
    task.options = options
    task.factory = unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(task.destination_path+'/'+source.stem)
    if not isinstance(asset, unreal.StaticMesh):
        raise RuntimeError('Import failed: '+source.stem)
    bounds = asset.get_bounding_box()
    expected = {'SM_SB_Spire_Collector': (320,230,1000),
                'SM_SB_Spire_LiftCollar': (309.847,309.847,8),
                'SM_SB_Spire_ServicePanel': (220,43,320)}[source.stem]
    actual = bounds.max - bounds.min
    if any(abs(a-b) > 0.1 for a,b in zip((actual.x,actual.y,actual.z),expected)):
        raise RuntimeError(f'Incorrect import axes or scale: {source.stem}: {actual}')
    if source.stem != 'SM_SB_Spire_ServicePanel' and abs(bounds.min.z) > 0.1:
        raise RuntimeError('Module must stand above its floor pivot: '+source.stem)
    report.append(f'{source.stem}: {bounds.min} -> {bounds.max}')
if len(report) != 3:
    raise RuntimeError('Expected three Spire modules')
(root / 'Saved/Logs/SpireImport.txt').write_text('\n'.join(report), encoding='utf-8')
unreal.log('\n'.join(report))
