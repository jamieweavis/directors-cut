"""Director's Cut: import Content/RawAudio/*.wav into /Game/DC/Audio as USoundWave assets.

Headless:
  lockf /private/tmp/claude-501/dc_ue_run.lock "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/DirectorsCut.uproject" -run=pythonscript -script="$PWD/Tools/audio_import.py" -unattended \
    -nosplash -abslog="$PWD/Saved/Logs/audio_import.log" \
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocol.ModelContextProtocolSettings]:bAutoStartServer=False'

Reads Content/RawAudio/manifest.json (written by Tools/audio_gen.py), deletes any asset under
/Game/DC/Audio that is not in the manifest (old music / theatre sounds), replaces existing assets,
sets looping on loops, and saves.
"""
import json
import os

import unreal

DEST = "/Game/DC/Audio"


def log(msg):
    unreal.log("[DCAudioImport] " + msg)


def main():
    proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    raw = os.path.join(proj, "Content", "RawAudio")
    with open(os.path.join(raw, "manifest.json")) as f:
        manifest = json.load(f)

    keep = set(os.path.splitext(s["file"])[0] for s in manifest["sounds"])
    removed = []
    for path in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True, include_folder=False):
        name = path.rsplit("/", 1)[-1].split(".")[0]
        if name not in keep:
            unreal.EditorAssetLibrary.delete_asset(path.split(".")[0])
            # In a commandlet the package file can survive delete_asset; remove it explicitly.
            f = os.path.join(proj, "Content", "DC", "Audio", name + ".uasset")
            if os.path.exists(f):
                os.remove(f)
            removed.append(name)
    log("removed stale assets: %s" % (", ".join(sorted(removed)) if removed else "none"))

    tasks = []
    for s in manifest["sounds"]:
        name = os.path.splitext(s["file"])[0]
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(raw, s["file"]))
        t.set_editor_property("destination_path", DEST)
        t.set_editor_property("destination_name", name)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("automated", True)
        t.set_editor_property("save", False)
        tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    ok, bad = 0, []
    for s in manifest["sounds"]:
        name = os.path.splitext(s["file"])[0]
        path = DEST + "/" + name
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(asset, unreal.SoundWave):
            bad.append(name)
            continue
        asset.set_editor_property("looping", bool(s["loop"]))
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        ok += 1
    log("imported %d assets; failed: %s" % (ok, ", ".join(bad) if bad else "none"))


main()
