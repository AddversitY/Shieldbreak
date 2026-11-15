"""Imports the combat sound banks (Art/Export/Audio/SFX/SFX_<Bank>_NN.wav, made by Art/audio/make_sfx.py) as sound
waves in /Game/Shieldbreak/Audio/SFX, headless. FShieldbreakSFX loads them by name; a missing bank falls back to the
procedural synth. Checks every bank in manifest.json is complete and writes Saved/Logs/ImportSFX.txt.

Run with Tools/ImportSFX.bat.
"""
import glob
import json
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "Art", "Export", "Audio", "SFX")
DEST = "/Game/Shieldbreak/Audio/SFX"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
lines = []


def log(text):
    lines.append(text)
    unreal.log("ImportSFX: " + text)


def main():
    files = sorted(glob.glob(os.path.join(SRC, "SFX_*.wav")))
    tasks = []
    for wav in files:
        task = unreal.AssetImportTask()
        task.filename = wav
        task.destination_path = DEST
        task.destination_name = os.path.splitext(os.path.basename(wav))[0]
        task.automated = True
        task.replace_existing = True
        task.save = False
        tasks.append(task)
    tools.import_asset_tasks(tasks)

    ok = 0
    for wav in files:
        name = os.path.splitext(os.path.basename(wav))[0]
        path = "%s/%s" % (DEST, name)
        sound = unreal.load_asset(path)
        if not sound:
            log("FAILED " + name)
            continue
        # Short one-shots stay resident so the first shot of a match never waits on disk.
        try:
            sound.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
        except Exception as error:  # property name differs between engine versions; not fatal
            log("note: loading_behavior not set (%s)" % error)
        eal.save_asset(path, only_if_is_dirty=False)
        ok += 1

    manifest = json.load(open(os.path.join(SRC, "manifest.json")))
    missing = []
    for bank, info in sorted(manifest.items()):
        for index in range(1, info["count"] + 1):
            if not eal.does_asset_exist("%s/SFX_%s_%02d" % (DEST, bank, index)):
                missing.append("SFX_%s_%02d" % (bank, index))
    log("imported %d/%d waves, %d banks, missing: %s" % (ok, len(files), len(manifest), ", ".join(missing) or "none"))
    log("RESULT " + ("PASS" if ok == len(files) and not missing else "FAIL"))
    out = os.path.join(PROJECT, "Saved", "Logs", "ImportSFX.txt")
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")


main()
