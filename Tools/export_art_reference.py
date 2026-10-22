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

MANNY = "/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny"
SKELETON = "/Game/Characters/Heroes/Mannequin/Meshes/SK_Mannequin.SK_Mannequin"
# The mannequin goes last: its FBX export needs the skinned render data, which can fail on a low-memory machine.
ASSETS = [
    ("/Game/Weapons/Rifle/Mesh/SM_Rifle.SM_Rifle", "SM_Rifle.fbx", "static"),
    ("/Game/Weapons/Pistol/Mesh/SM_Pistol.SM_Pistol", "SM_Pistol.fbx", "static"),
    (MANNY, "SKM_Manny.fbx", "skel"),
]


def write_report():
    with open(os.path.join(OUT, "export_report.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")


def dump_bones():
    """Reference-pose bones in component space (from the skeleton asset; no render data needed) to bones.json."""
    skeleton = unreal.load_asset(SKELETON)
    if skeleton is None:
        lines.append("MISSING " + SKELETON)
        return
    try:
        pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
        names = unreal.AnimPoseExtensions.get_bone_names(pose)
        bones = {}
        for name in names:
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
            loc, rot = t.translation, t.rotation
            bones[str(name)] = {"loc": [loc.x, loc.y, loc.z], "quat": [rot.x, rot.y, rot.z, rot.w],
                                "parent": str(unreal.AnimPoseExtensions.get_parent_bone_name(pose, name))
                                if hasattr(unreal.AnimPoseExtensions, "get_parent_bone_name") else ""}
        with open(os.path.join(OUT, "bones.json"), "w") as f:
            json.dump(bones, f, indent=1)
        lines.append("OK bones.json (%d bones)" % len(bones))
    except Exception as exc:
        lines.append("bone dump failed: %s" % exc)
    write_report()


dump_bones()


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
    write_report()

for p in ["/Game/Weapons/Rifle/Mesh/SM_Rifle.SM_Rifle", "/Game/Weapons/Pistol/Mesh/SM_Pistol.SM_Pistol"]:
    a = unreal.load_asset(p)
    if a:
        b = a.get_bounding_box()
        lines.append("%s bounds min=%s max=%s" % (p, b.min, b.max))

write_report()
unreal.log("ArtExport: " + " | ".join(lines))
