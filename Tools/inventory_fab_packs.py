"""Inventory the Fab packs added to this project (headless): every static mesh outside Shieldbreak's own folders, with
triangles at LOD0, LOD count, Nanite, bounds in metres and material count. Writes Saved/Logs/FabPacks.txt and
Saved/Logs/FabPacks.json. The packs' raw files stay git-ignored; this only reads them.

Run with Tools/InventoryFabPacks.bat.
"""
import json
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OURS = {'Shieldbreak', 'Characters', 'Weapons', 'Effects', 'Audio', 'PhysicsMaterials', 'Megascans', 'MSPresets',
        'ParagonWraith', 'Developers', 'Custom', 'Meshes', 'PhysicalMaterials', 'Splash', 'Collections', '__ExternalActors__', '__ExternalObjects__'}
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def main():
    content = os.path.join(PROJECT, 'Content')
    packs = sorted(d for d in os.listdir(content) if os.path.isdir(os.path.join(content, d)) and d not in OURS)
    report, data = [], {}
    for pack in packs:
        assets = registry.get_assets_by_path('/Game/' + pack, recursive=True)
        meshes = [a for a in assets if str(a.asset_class_path.asset_name) == 'StaticMesh']
        maps = [str(a.package_name) for a in assets if str(a.asset_class_path.asset_name) == 'World']
        report.append('== %s: %d assets, %d static meshes, maps %s' % (pack, len(assets), len(meshes), ', '.join(maps) or '-'))
        rows = []
        for asset in meshes:
            mesh = unreal.load_asset(str(asset.package_name))
            if not isinstance(mesh, unreal.StaticMesh):
                continue
            box = mesh.get_bounding_box()
            size = [(getattr(box.max, a) - getattr(box.min, a)) / 100.0 for a in 'xyz']
            nanite = mesh.get_editor_property('nanite_settings').get_editor_property('enabled')
            row = {'path': str(asset.package_name), 'tris': mesh.get_num_triangles(0), 'lods': mesh.get_num_lods(),
                   'nanite': bool(nanite), 'size_m': [round(v, 2) for v in size],
                   'materials': len(mesh.get_editor_property('static_materials'))}
            rows.append(row)
            report.append('%-90s tris %7d  lods %d  nanite %d  %5.2f x %5.2f x %5.2f m  mats %d' % (
                row['path'], row['tris'], row['lods'], row['nanite'], size[0], size[1], size[2], row['materials']))
        data[pack] = {'maps': maps, 'meshes': rows}
    os.makedirs(os.path.join(PROJECT, 'Saved', 'Logs'), exist_ok=True)
    with open(os.path.join(PROJECT, 'Saved', 'Logs', 'FabPacks.txt'), 'w') as f:
        f.write('\n'.join(report + ['RESULT OK: %d packs' % len(packs)]) + '\n')
    with open(os.path.join(PROJECT, 'Saved', 'Logs', 'FabPacks.json'), 'w') as f:
        json.dump(data, f, indent=1)
    unreal.log('FabPacks: %d packs' % len(packs))


main()
