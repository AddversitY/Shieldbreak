"""Headless check of imported content. Run with:
UnrealEditor-Cmd.exe Shieldbreak.uproject -run=pythonscript -script=Tools/check_imported_assets.py -unattended -nosplash -nullrhi

Writes Saved/Logs/ImportCheck.txt listing, for the imported folders:
  - /Game packages referenced but missing from Content/ (need copying too)
  - /Script modules referenced (to spot required plugins / Lyra-only C++)
  - assets of the classes we care about, with load success
"""
import os
import unreal

ROOTS = ["/Game/Characters/Heroes/Mannequin", "/Game/Weapons", "/Game/Effects", "/Game/Audio"]
WANTED = {"SkeletalMesh", "StaticMesh", "AnimSequence", "AnimMontage", "BlendSpace", "BlendSpace1D",
          "AimOffsetBlendSpace", "NiagaraSystem", "SoundWave", "SoundCue", "MetaSoundSource", "Skeleton", "PhysicsAsset"}

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(["/Game"], True)

opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False,
                                             include_hard_package_references=True,
                                             include_searchable_names=False,
                                             include_soft_management_references=False,
                                             include_hard_management_references=False)

missing = {}
scripts = {}
wanted_assets = []
for root in ROOTS:
    for data in registry.get_assets_by_path(root, recursive=True):
        pkg = str(data.package_name)
        cls = str(data.asset_class_path.asset_name)
        if cls in WANTED:
            wanted_assets.append((cls, str(data.package_name) + "." + str(data.asset_name)))
        deps = registry.get_dependencies(pkg, opts) or []
        for dep in deps:
            dep = str(dep)
            if dep.startswith("/Script/"):
                scripts.setdefault(dep, set()).add(pkg)
            elif dep.startswith("/Game/"):
                if not registry.get_assets_by_package_name(dep):
                    missing.setdefault(dep, set()).add(pkg)
            elif not dep.startswith("/Engine/"):
                # plugin content, e.g. /Niagara/..., /ShooterCore/...
                if not registry.get_assets_by_package_name(dep):
                    missing.setdefault(dep, set()).add(pkg)

load_fail = []
for cls, path in wanted_assets:
    if unreal.load_asset(path) is None:
        load_fail.append(path)

out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "ImportCheck.txt")
with open(out, "w", encoding="utf-8") as f:
    f.write("MISSING %d\n" % len(missing))
    for dep in sorted(missing):
        f.write("  %s  <- %s\n" % (dep, sorted(missing[dep])[0]))
    f.write("SCRIPT %d\n" % len(scripts))
    for dep in sorted(scripts):
        f.write("  %s (%d users)\n" % (dep, len(scripts[dep])))
    f.write("WANTED %d\n" % len(wanted_assets))
    for cls, path in sorted(wanted_assets):
        f.write("  %s %s\n" % (cls, path))
    f.write("LOADFAIL %d\n" % len(load_fail))
    for path in load_fail:
        f.write("  %s\n" % path)
unreal.log("ImportCheck written to " + out)
