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

out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "Sockets.txt")
with open(out, "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
unreal.log("Sockets written to " + out)
