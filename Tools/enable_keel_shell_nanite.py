"""Turns Nanite on for the Keel shell kit meshes (/Game/Shieldbreak/Art/MapKit/SM_SB_Kit_KeelShell_*), headless.

The kit's moulded edges come from real geometry (bevel + subdivision), 4k-44k triangles a piece, so it is built for
Nanite rather than LODs. Writes the result to Saved/Logs/KeelShellNanite.txt. Run from Tools/ImportKeelShell.bat.
"""
import os

import unreal

DEST = "/Game/Shieldbreak/Art/MapKit"
eal = unreal.EditorAssetLibrary
lines = []
for path in eal.list_assets(DEST, recursive=False):
    name = path.split("/")[-1].split(".")[0]
    if not name.startswith("SM_SB_Kit_KeelShell_"):
        continue
    mesh = unreal.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        continue
    settings = mesh.get_editor_property("nanite_settings")
    if not settings.enabled:
        settings.enabled = True
        mesh.set_editor_property("nanite_settings", settings)
        eal.save_loaded_asset(mesh)
        lines.append(name + ": nanite on")
    else:
        lines.append(name + ": nanite already on")
lines.append("RESULT %d meshes" % len(lines))
out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Saved", "Logs", "KeelShellNanite.txt")
with open(out, "w") as f:
    f.write("\n".join(lines) + "\n")
