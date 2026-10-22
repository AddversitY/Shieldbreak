"""Exports reference meshes (mannequin + Lyra guns) to Saved/ArtExport/ as FBX, plus the mannequin's bones as JSON.

The cloud session models armor and weapons in Blender around these, so new art lines up with the hand bone and the body.
Run headless with Tools/ExportArtReference.bat.
"""
import json
import os
import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "ArtExport")
os.makedirs(OUT, exist_ok=True)
lines = []

ASSETS = [
    ("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny", "SKM_Manny.fbx", "skel"),
    ("/Game/Weapons/Rifle/Mesh/SM_Rifle.SM_Rifle", "SM_Rifle.fbx", "static"),
    ("/Game/Weapons/Pistol/Mesh/SM_Pistol.SM_Pistol", "SM_Pistol.fbx", "static"),
]


def make_exporter(kind):
    names = ["SkeletalMeshExporterFBX"] if kind == "skel" else ["StaticMeshExporterFBX"]
    for name in names:
        cls = getattr(unreal, name, None)
        if cls:
            return cls()
    return None


for path, filename, kind in ASSETS:
    asset = unreal.load_asset(path)
    if asset is None:
        lines.append("MISSING %s" % path)
        continue
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = os.path.join(OUT, filename)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    exporter = make_exporter(kind)
    if exporter:
        task.exporter = exporter
    options = unreal.FbxExportOption()
    options.ascii = False
    options.level_of_detail = False
    options.collision = False
    task.options = options
    ok = unreal.Exporter.run_asset_export_task(task)
    size = os.path.getsize(task.filename) if os.path.exists(task.filename) else -1
    lines.append("%s %s -> %s (%d bytes)" % ("OK" if ok else "FAILED", path, filename, size))

# Bones in reference pose, as UE sees them (component space comes from the C++ side; here, names and parents).
manny = unreal.load_asset(ASSETS[0][0])
bones = []
try:
    skeleton = manny.get_editor_property("skeleton")
    count = skeleton.get_reference_pose().get_num_bones() if hasattr(skeleton, "get_reference_pose") else 0
    lines.append("skeleton %s, ref pose bones %d" % (skeleton.get_name(), count))
except Exception as exc:
    lines.append("skeleton query failed: %s" % exc)
try:
    bounds = manny.get_bounds()
    lines.append("manny bounds origin=%s extent=%s" % (bounds.origin, bounds.box_extent))
except Exception as exc:
    lines.append("bounds failed: %s" % exc)
for p in ["/Game/Weapons/Rifle/Mesh/SM_Rifle.SM_Rifle", "/Game/Weapons/Pistol/Mesh/SM_Pistol.SM_Pistol"]:
    a = unreal.load_asset(p)
    if a:
        b = a.get_bounding_box()
        lines.append("%s bounds min=%s max=%s" % (p, b.min, b.max))

with open(os.path.join(OUT, "export_report.txt"), "w") as f:
    f.write("\n".join(lines) + "\n")
unreal.log("ArtExport: " + " | ".join(lines))
