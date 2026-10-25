"""Writes length, skeleton, additive type and how much the legs/arms move for Lyra's action animations to
Saved/Logs/LyraActions.txt (headless; run with Tools/ListLyraActions.bat)."""
import os
import unreal

ROOT = "/Game/Characters/Heroes/Mannequin/Animations/"
NAMES = [
    "Actions/MM_Rifle_GrenadeToss", "Actions/MM_Rifle_GrenadeToss_Additive", "Actions/AM_MM_Rifle_GrenadeToss",
    "Actions/MM_Pistol_GrenadeToss",
    "Actions/MM_Rifle_Melee", "Actions/MM_Rifle_Melee_Additive", "Actions/AM_MM_Rifle_Melee",
    "Actions/MM_Pistol_Melee", "Actions/MM_Pistol_Melee_Additive", "Actions/AM_MM_Pistol_Melee",
    "Actions/MM_Rifle_Reload", "Actions/MM_Rifle_Reload_Additive",
    "Actions/MM_Pistol_Reload", "Actions/MM_Pistol_Reload_Additive",
    "Actions/MM_Rifle_Equip", "Actions/MM_Rifle_Equip_Additive",
    "Actions/MM_Pistol_Equip", "Actions/MM_Pistol_Equip_Additive",
    "Actions/AM_MM_Generic_Unequip",
    "Actions/MM_Rifle_Fire", "Actions/MM_Pistol_Fire",
    "Actions/MM_Rifle_DryFire", "Actions/MM_Rifle_DryFire_Additive",
    "Actions/MM_Pistol_DryFire", "Actions/MM_Pistol_DryFire_Additive",
    "Locomotion/Rifle/MM_Rifle_Idle_Hipfire", "Locomotion/Rifle/MM_Rifle_Idle_ADS",
    "Locomotion/Rifle/MM_Rifle_Hipfire_OverridePose",
    "Locomotion/Pistol/MM_Pistol_Idle_Hipfire", "Locomotion/Pistol/MM_Pistol_Idle_ADS",
    "Locomotion/Pistol/MF_Pistol_Hipfire_OverridePose",
]
BONES = ["pelvis", "foot_l", "foot_r", "spine_03", "hand_r", "hand_l"]
SAMPLES = 12


def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None


def motion(seq, length):
    """Largest translation (cm) / rotation (deg) change of each bone across the clip, component-free (local)."""
    out = []
    lib = unreal.AnimationLibrary
    for bone in BONES:
        poses = []
        for i in range(SAMPLES + 1):
            t = length * i / SAMPLES
            try:
                poses.append(lib.get_bone_pose_for_time(seq, bone, t, False))
            except Exception as e:
                out.append("%s ?(%s)" % (bone, e))
                poses = None
                break
        if not poses:
            continue
        t0 = poses[0]
        dt = max((p.translation - t0.translation).length() for p in poses)
        dr = 0.0
        r0 = t0.rotation.rotator()
        for p in poses:
            d = unreal.MathLibrary.normalized_delta_rotator(p.rotation.rotator(), r0)
            dr = max(dr, abs(d.roll), abs(d.pitch), abs(d.yaw))
        out.append("%s %.1fcm/%.0fdeg" % (bone, dt, dr))
    return ", ".join(out)


lines = []
for name in NAMES:
    path = ROOT + name + "." + name.split("/")[-1]
    asset = unreal.load_asset(path)
    if asset is None:
        lines.append("%s: NOT FOUND" % (ROOT + name))
        continue
    cls = asset.get_class().get_name()
    length = asset.get_play_length() if hasattr(asset, "get_play_length") else prop(asset, "sequence_length")
    skel = asset.get_skeleton() if hasattr(asset, "get_skeleton") else prop(asset, "skeleton")
    head = "%s [%s] %.3f s, skeleton %s" % (ROOT + name, cls, length or 0.0, skel.get_path_name() if skel else "?")
    if isinstance(asset, unreal.AnimSequence):
        head += ", additive %s" % prop(asset, "additive_anim_type")
        ref = prop(asset, "ref_pose_seq")
        if ref:
            head += " (ref %s, %s)" % (ref.get_name(), prop(asset, "ref_pose_type"))
        rate = prop(asset, "rate_scale")
        if rate and abs(rate - 1.0) > 1e-3:
            head += ", rate %.2f" % rate
        lines.append(head)
        lines.append("    motion: " + motion(asset, length or 0.0))
    elif isinstance(asset, unreal.AnimMontage):
        lines.append(head)
        for track in prop(asset, "slot_anim_tracks") or []:
            segs = prop(prop(track, "anim_track"), "anim_segments") or []
            refs = ", ".join("%s %.2f-%.2f x%.2f" % (
                prop(s, "anim_reference").get_name() if prop(s, "anim_reference") else "?",
                prop(s, "anim_start_time") or 0.0, prop(s, "anim_end_time") or 0.0, prop(s, "anim_play_rate") or 1.0)
                for s in segs)
            lines.append("    slot %s: %s" % (prop(track, "slot_name"), refs))
        lines.append("    blend in %s out %s" % (prop(prop(asset, "blend_in"), "blend_time"), prop(prop(asset, "blend_out"), "blend_time")))
    else:
        lines.append(head)

out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "LyraActions.txt")
with open(out, "w") as f:
    f.write("\n".join(lines) + "\n")
unreal.log("wrote " + out)
