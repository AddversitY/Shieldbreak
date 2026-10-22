"""Writes socket and key bone names of the imported meshes to Saved/Logs/Sockets.txt (headless)."""
import os
import unreal

MESHES = [
    "/Game/Weapons/Rifle/Mesh/SM_Rifle.SM_Rifle",
    "/Game/Weapons/Pistol/Mesh/SM_Pistol.SM_Pistol",
    "/Game/Weapons/Shotgun/Mesh/SM_Shotgun.SM_Shotgun",
    "/Game/Weapons/Grenade/Mesh/SM_grenade.SM_grenade",
    "/Game/Weapons/Rifle/Mesh/SK_Rifle.SK_Rifle",
    "/Game/Weapons/Pistol/Mesh/SK_Pistol.SK_Pistol",
    "/Game/Weapons/Shotgun/Mesh/SKM_Shotgun.SKM_Shotgun",
    "/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny",
    "/Game/Characters/Heroes/Mannequin/Meshes/SK_Mannequin.SK_Mannequin",
]

lines = []
for path in MESHES:
    asset = unreal.load_asset(path)
    if asset is None:
        lines.append("%s: NOT FOUND" % path)
        continue
    lines.append("%s (%s)" % (path, asset.get_class().get_name()))
    if isinstance(asset, unreal.StaticMesh):
        bounds = asset.get_bounding_box()
        lines.append("  bounds min=%s max=%s" % (bounds.min, bounds.max))
        found = False
        for name in ["Muzzle", "MuzzleFlash", "muzzle", "Muzzle_Flash", "MuzzleSocket", "Weapon_Muzzle", "SOCKET_Muzzle", "Grip", "grip"]:
            try:
                sock = asset.find_socket(name)
            except Exception as exc:
                lines.append("  find_socket unavailable: %s" % exc)
                break
            if sock:
                found = True
                lines.append("  socket %s loc=%s rot=%s" % (sock.socket_name, sock.relative_location, sock.relative_rotation))
        if not found:
            lines.append("  (no socket with a common muzzle/grip name)")
    elif isinstance(asset, unreal.SkeletalMesh):
        try:
            for i in range(asset.num_sockets()):
                sock = asset.get_socket_by_index(i)
                lines.append("  socket %s bone=%s loc=%s rot=%s" % (sock.socket_name, sock.bone_name, sock.relative_location, sock.relative_rotation))
        except Exception as exc:
            lines.append("  socket listing failed: %s" % exc)
        try:
            bounds = asset.get_bounds()
            lines.append("  bounds origin=%s extent=%s" % (bounds.origin, bounds.box_extent))
        except Exception:
            pass
        skel = asset.get_editor_property("skeleton")
        if skel:
            lines.append("  skeleton %s" % skel.get_path_name())
    elif isinstance(asset, unreal.Skeleton):
        # Skeleton-level sockets are exposed through a preview mesh; list bones of interest instead.
        pass

# Material slots and parameters on the mannequin (for team tinting).
for mesh_path in ["/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny", "/Game/Characters/Heroes/Mannequin/Meshes/SKM_Quinn.SKM_Quinn"]:
    mesh = unreal.load_asset(mesh_path)
    if mesh is None:
        continue
    lines.append("MATERIALS %s" % mesh_path)
    try:
        slots = mesh.get_editor_property("materials")
    except Exception as exc:
        lines.append("  materials unreadable: %s" % exc)
        slots = []
    for index, slot in enumerate(slots):
        mat = slot.get_editor_property("material_interface")
        lines.append("  slot %d '%s': %s" % (index, slot.get_editor_property("material_slot_name"), mat.get_path_name() if mat else None))
        chain = mat
        depth = 0
        while chain is not None and depth < 4:
            for label, getter in (("vector", "get_vector_parameter_names"), ("scalar", "get_scalar_parameter_names"), ("texture", "get_texture_parameter_names")):
                try:
                    names = getattr(unreal.MaterialEditingLibrary, getter)(chain)
                    lines.append("    %s%s params: %s" % ("  " * depth, label, ", ".join(str(n) for n in names)))
                except Exception as exc:
                    lines.append("    %s%s params unreadable: %s" % ("  " * depth, label, exc))
            if isinstance(chain, unreal.MaterialInstance):
                try:
                    for vp in chain.get_editor_property("vector_parameter_values") or []:
                        info = vp.get_editor_property("parameter_info")
                        lines.append("    %soverride %s = %s" % ("  " * depth, info.get_editor_property("name"), vp.get_editor_property("parameter_value")))
                except Exception:
                    pass
                parent = chain.get_editor_property("parent")
                lines.append("    %sparent: %s" % ("  " * depth, parent.get_path_name() if parent else None))
                chain = parent
                depth += 1
            else:
                break

out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "Sockets.txt")
with open(out, "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
unreal.log("Sockets written to " + out)
