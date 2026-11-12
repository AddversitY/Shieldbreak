"""Builds SK_SB_Soldier: the AI-generated soldier suit skinned to the mannequin skeleton, headless.

The suit (SM_SB_SoldierSuit, written by Art/blender/import_meshy.py and imported by ImportArt with the other armor) is
modeled in the mannequin's reference pose and mesh space. Worn as rigid per-bone pieces it tore open at the waist and
shoulders when the body bent (desktop S4/S5 shots). Here it gets SKM_Manny's skin weights by closest point
(Geometry Script TransferBoneWeightsFromMesh) and becomes a skeletal mesh on the mannequin's skeleton, which the game
drives with a leader pose (UShieldbreakBodyAnimator, sb.SkinnedSuit). Report: Saved/Logs/MakeSoldierSuit.txt.

Needs the GeometryScripting plugin. The Geometry Script API moved between engine versions, so each step tries the
known names and logs what it found; a failed run says which call is missing.
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "MakeSoldierSuit.txt")
MANNY = "/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny"
SUIT = "/Game/Shieldbreak/Art/Armor/SM_SB_SoldierSuit.SM_SB_SoldierSuit"
TARGET = "/Game/Shieldbreak/Art/Armor/SK_SB_Soldier"
MATERIAL = "/Game/Shieldbreak/Art/Materials/MI_SB_Soldier.MI_SB_Soldier"
SLOT = "M_Team_Soldier"
lines = []


def report(text):
    unreal.log("MakeSoldierSuit: " + text)
    lines.append(str(text))
    with open(OUT, "w") as f:
        f.write("\n".join(lines) + "\n")


def first(obj, *names):
    for n in names:
        fn = getattr(obj, n, None)
        if fn is not None:
            return fn
    raise RuntimeError("none of %s on %s" % (names, obj))


def find_lib(*names):
    for n in names:
        lib = getattr(unreal, n, None)
        if lib is not None:
            return lib
    raise RuntimeError("no library among %s" % (names,))


def mesh_out(result, fallback):
    """Geometry Script calls return the mesh, or a tuple with it; either way the passed-in mesh holds the data."""
    for x in result if isinstance(result, tuple) else (result,):
        if isinstance(x, unreal.DynamicMesh):
            return x
    return fallback


PACK_MAX_Y = -24.0     # cm: behind the mannequin's back (mesh space, +Y is forward)
PACK_MIN_Z = 95.0      # above the belt
PACK_BONE = "spine_05"


def rigid_pack(bones, mesh):
    """The power pack is one rigid box: closest-point weights gave its lower side some arm and pelvis influence, which
    smeared it while aiming (S6). Every vertex behind the back and above the belt follows spine_05 alone. Optional:
    a missing API is logged and the transferred weights are kept."""
    try:
        positions = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)
        items = positions if isinstance(positions, tuple) else (positions,)
        pts = None
        for x in items:
            if isinstance(x, (list, unreal.Array)):
                pts = list(x)
            elif type(x).__name__ == "GeometryScriptVectorList":
                lists = getattr(unreal, "GeometryScript_List", None) or unreal.GeometryScript_ListUtils
                out = lists.convert_vector_list_to_array(x)
                for o in out if isinstance(out, tuple) else (out,):
                    if isinstance(o, (list, unreal.Array)):
                        pts = list(o)
        if pts is None:
            raise RuntimeError("no vertex positions")
        index = None
        for name in ("get_bone_index", "find_bone_index"):
            fn = getattr(bones, name, None)
            if fn:
                r = fn(mesh, unreal.Name(PACK_BONE))
                for x in r if isinstance(r, tuple) else (r,):
                    if isinstance(x, int) and not isinstance(x, bool):
                        index = x
        if index is None or index < 0:
            raise RuntimeError("no bone index for %s (bone weight API: %s)" % (PACK_BONE, [n for n in dir(bones) if "bone" in n.lower()]))
        setter = first(bones, "set_vertex_bone_weights")
        weight = unreal.GeometryScriptBoneWeight()
        weight.set_editor_property("bone_index", index)
        weight.set_editor_property("weight", 1.0)
        n = 0
        for vid, p in enumerate(pts):
            if p.y < PACK_MAX_Y and p.z > PACK_MIN_Z:
                setter(mesh, vid, [weight])
                n += 1
        report("rigid pack: %d vertices on %s (bone %d)" % (n, PACK_BONE, index))
    except Exception as e:  # noqa: BLE001
        report("rigid pack skipped: %s" % e)


def main():
    asset_utils = unreal.GeometryScript_AssetUtils
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.lod_type = unreal.GeometryScriptLODType.SOURCE_MODEL
    lod.lod_index = 0
    copy_opts = unreal.GeometryScriptCopyMeshFromAssetOptions()

    manny = unreal.load_asset(MANNY)
    suit_sm = unreal.load_asset(SUIT)
    material = unreal.load_asset(MATERIAL)
    report("manny %s, suit %s, material %s" % (manny, suit_sm, material))
    if not manny or not suit_sm:
        raise RuntimeError("missing input asset")

    source = unreal.DynamicMesh()
    source = mesh_out(asset_utils.copy_mesh_from_skeletal_mesh(manny, source, copy_opts, lod), source)
    target = unreal.DynamicMesh()
    target = mesh_out(asset_utils.copy_mesh_from_static_mesh(suit_sm, target, copy_opts, lod), target)
    report("source tris %d, suit tris %d" % (source.get_triangle_count(), target.get_triangle_count()))

    bones = find_lib("GeometryScript_BoneWeights", "GeometryScriptLibrary_MeshBoneWeightFunctions")
    report("bone weight library: %s" % [n for n in dir(bones) if not n.startswith("_")][:40])
    # The target needs the skeleton's bones before weights can refer to them.
    copy_bones = getattr(bones, "copy_bones_from_mesh", None)
    if copy_bones:
        res = copy_bones(source, target)
        report("copy_bones_from_mesh -> %s" % (res,))
    transfer = first(bones, "transfer_bone_weights_from_mesh")
    opts_cls = getattr(unreal, "GeometryScriptTransferBoneWeightsOptions", None)
    topts = opts_cls() if opts_cls else None
    if topts is not None:
        report("transfer options: %s" % [n for n in dir(topts) if not n.startswith("_")][:30])
        # Closest point onto the body; our plates sit a few cm off the skin, so inpainting is not needed.
        for prop, value in (("transfer_method", getattr(getattr(unreal, "TransferBoneWeightsMethod", None), "CLOSEST_POINT_ON_SURFACE", None)),):
            if value is not None:
                try:
                    topts.set_editor_property(prop, value)
                except Exception as e:  # noqa: BLE001
                    report("option %s: %s" % (prop, e))
    try:
        res = transfer(source, target, topts) if topts is not None else transfer(source, target)
    except TypeError:
        res = transfer(source, target, topts, None)
    report("transfer_bone_weights_from_mesh -> %s" % (res,))
    has = getattr(bones, "mesh_has_bone_weights", None)
    if has:
        report("target has bone weights: %s" % (has(target),))
    rigid_pack(bones, target)

    create = find_lib("GeometryScript_NewAssetUtils", "GeometryScript_CreateNewAssetUtils", "GeometryScriptLibrary_CreateNewAssetFunctions")
    make = first(create, "create_new_skeletal_mesh_asset_from_mesh")
    skeleton = manny.skeleton if hasattr(manny, "skeleton") else manny.get_editor_property("skeleton")
    opts_cls = getattr(unreal, "GeometryScriptCreateNewSkeletalMeshAssetOptions")
    sopts = opts_cls()
    for prop, value in (("enable_recompute_normals", False), ("enable_recompute_tangents", True), ("use_mesh_bone_proportions", False)):
        try:
            sopts.set_editor_property(prop, value)
        except Exception as e:  # noqa: BLE001
            report("option %s: %s" % (prop, e))
    if material:
        try:
            sopts.set_editor_property("materials", {unreal.Name(SLOT): material})
        except Exception as e:  # noqa: BLE001
            report("materials option: %s" % e)
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        unreal.EditorAssetLibrary.delete_asset(TARGET)
    res = make(target, skeleton, TARGET, sopts)
    report("create_new_skeletal_mesh_asset_from_mesh -> %s" % (res,))
    sk = next((x for x in (res if isinstance(res, tuple) else (res,)) if isinstance(x, unreal.SkeletalMesh)), None) \
        or unreal.load_asset(TARGET + ".SK_SB_Soldier")
    if not sk:
        raise RuntimeError("no skeletal mesh created")
    # One material slot named like the static pieces, so the game's M_Team* team-colour hook finds it.
    mats = sk.get_editor_property("materials")
    report("slots before: %s" % [(str(m.material_slot_name), m.material_interface.get_name() if m.material_interface else None) for m in mats])
    if material and mats:
        fixed = []
        for m in mats:
            m.set_editor_property("material_interface", material)
            m.set_editor_property("material_slot_name", unreal.Name(SLOT))
            fixed.append(m)
        sk.set_editor_property("materials", fixed)
    # LODs: Unreal's skeletal reduction, 50% and 25% of the triangles.
    try:
        sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        report("regenerate_lod: %s" % (sub.regenerate_lod(sk, 3, True, True),))
    except Exception as e:  # noqa: BLE001
        report("LODs skipped: %s" % e)
    unreal.EditorAssetLibrary.save_asset(TARGET)
    report("slots: %s" % [(str(m.material_slot_name), m.material_interface.get_name() if m.material_interface else None) for m in sk.get_editor_property("materials")])
    report("RESULT OK %s" % TARGET)


try:
    open(OUT, "w").close()
    main()
except Exception as e:  # noqa: BLE001
    report("RESULT FAILED %s" % e)
