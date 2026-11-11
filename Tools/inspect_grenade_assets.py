"""Facts the grenade pass needs from the Lyra content, written to Saved/Logs/GrenadeAssets.txt (headless; run with
Tools/InspectGrenadeAssets.bat): the toss clip's length and where the left hand travels (component space, so the
release frame can be found), the grenade mesh's bounds and sockets, and the size/format of the effect textures and
the parameters of the effect materials the explosion may reuse."""
import os
import unreal

lines = []
lib = unreal.AnimationLibrary
TOSS = "/Game/Characters/Heroes/Mannequin/Animations/Actions/MM_Rifle_GrenadeToss.MM_Rifle_GrenadeToss"
CHAIN = ["root", "pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "clavicle_l", "upperarm_l",
         "lowerarm_l", "hand_l"]


def component_space(seq, t, upto):
    xf = unreal.Transform()
    for bone in CHAIN[:CHAIN.index(upto) + 1]:
        local = lib.get_bone_pose_for_time(seq, bone, t, False)
        xf = unreal.MathLibrary.compose_transforms(local, xf)
    return xf


seq = unreal.load_asset(TOSS)
if seq:
    length = seq.get_play_length()
    lines.append("toss %.3f s, %s frames" % (length, lib.get_num_frames(seq)))
    prev = None
    for i in range(0, 41):
        t = length * i / 40.0
        try:
            hand = component_space(seq, t, "hand_l")
            loc = hand.translation
            speed = 0.0 if prev is None else (loc - prev).length() / (length / 40.0)
            prev = loc
            lines.append("  t=%.3f hand_l (%.1f, %.1f, %.1f) speed %.0f cm/s" % (t, loc.x, loc.y, loc.z, speed))
        except Exception as e:
            lines.append("  t=%.3f error %s" % (t, e))
            break
    try:
        for n in lib.get_animation_notify_event_names(seq):
            lines.append("  notify %s" % n)
    except Exception as e:
        lines.append("  notifies ?(%s)" % e)
else:
    lines.append("toss NOT FOUND")

mesh = unreal.load_asset("/Game/Weapons/Grenade/Mesh/SM_grenade.SM_grenade")
if mesh:
    b = mesh.get_bounds()
    lines.append("SM_grenade bounds origin %s extent %s, %d tris LOD0, %d LODs, materials %s" % (
        b.origin, b.box_extent, mesh.get_num_triangles(0), mesh.get_num_lods(),
        [m.material_slot_name for m in mesh.static_materials]))

for path in ["/Game/Effects/Textures/Flipbooks/T_Explosion_Areal_E64", "/Game/Effects/Textures/Flipbooks/SmokeSwirl_3_Flipbook_CHANNELPACK",
             "/Game/Effects/Textures/Flipbooks/LDR_Squib_2_rop_comp1_0001_flipbook", "/Game/Effects/Textures/General/T_Inky_Smoke_Tile",
             "/Game/Effects/Textures/Decals/T_ExplosionDecal", "/Game/Effects/Textures/BlastGraphics/T_ExplosionFlare",
             "/Game/Effects/Textures/General/blurry_texture"]:
    tex = unreal.load_asset(path)
    if not tex:
        lines.append("%s NOT FOUND" % path)
        continue
    try:
        lines.append("%s %dx%d srgb=%s compression=%s" % (path, tex.blueprint_get_size_x(), tex.blueprint_get_size_y(),
            tex.get_editor_property("srgb"), tex.get_editor_property("compression_settings")))
    except Exception as e:
        lines.append("%s ?(%s)" % (path, e))

for path in ["/Game/Effects/Materials/Pyro/M_Flipbook_Pyro_Base", "/Game/Effects/Materials/Pyro/Instances/MI_Flipbook_Pyro_Grenade",
             "/Game/Effects/Materials/Decals/M_ExplosionDecal", "/Game/Effects/Materials/BlastGraphics/M_Grenade_EX_Flare",
             "/Game/Effects/Materials/BlastGraphics/M_Grenade_Blast"]:
    mat = unreal.load_asset(path)
    if not mat:
        lines.append("%s NOT FOUND" % path)
        continue
    base = mat.get_base_material() if hasattr(mat, "get_base_material") else mat
    info = "%s domain=%s blend=%s" % (path, base.get_editor_property("material_domain"), base.get_editor_property("blend_mode"))
    mel = unreal.MaterialEditingLibrary
    try:
        info += " scalars=%s vectors=%s textures=%s" % (mel.get_scalar_parameter_names(mat), mel.get_vector_parameter_names(mat),
                                                       mel.get_texture_parameter_names(mat))
    except Exception as e:
        info += " params ?(%s)" % e
    lines.append(info)

out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "GrenadeAssets.txt")
with open(out, "w") as f:
    f.write("\n".join(lines) + "\n")
unreal.log("wrote " + out)
