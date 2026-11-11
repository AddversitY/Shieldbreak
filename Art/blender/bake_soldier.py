"""Bakes and exports the v3 soldier (build_soldier.py): one texture set for the whole suit, one material slot.

Called by build_soldier.py:  python build_soldier.py <bones.json> <SKM_Manny.obj> --bake 4096 [--preview DIR] [--no-export]

Outputs: Art/Export/Armor/SM_SB_Armor_<bone>.fbx (14 pieces, slot M_Team_Soldier, UV0 into the shared atlas) and
Art/Export/Textures/Armor/T_SB_Soldier_{BC,ORM,N,M}.png. M's red channel is the team-colour mask (painted plates and
team lights); Tools/import_art.py builds M_SB_ArmorPBR / MI_SB_Soldier from them.
"""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sb_kit as k  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
import bake_weapons as bw  # noqa: E402
import build_soldier as bs  # noqa: E402

TEX_OUT = os.path.join(bw.EXPORT, "Textures", "Armor")
SLOT = "M_Team_Soldier"
# Surface looks for the suit (kind, base colour, roughness, metallic, tone variation); see bake_weapons.SURFACES.
bw.SURFACES.update({
    "ArmorPlate": ("paint", (0.135, 0.14, 0.145), 0.47, 0.25, 0.12),
    # Team plates bake as a paler neutral paint; the material multiplies the team colour in through the mask.
    "ArmorTeam": ("paint", (0.3, 0.3, 0.3), 0.45, 0.1, 0.10),
    "ArmorDark": ("paint", (0.075, 0.078, 0.085), 0.48, 0.35, 0.10),
    "Webbing": ("fabric", (0.085, 0.095, 0.05), 0.88, 0.0, 0.10),
    "Canvas": ("fabric", (0.2, 0.165, 0.105), 0.9, 0.0, 0.10),
    "RedCap": ("paint", (0.36, 0.03, 0.022), 0.5, 0.0, 0.06),
    "Rubber": ("polymer", (0.024, 0.024, 0.026), 0.78, 0.0, 0.08),
    "Undersuit": ("fabric", (0.028, 0.029, 0.032), 0.75, 0.0, 0.06),
    "Visor": ("glow", (0.95, 0.42, 0.05), 0.12, 0.0, 1.0),
    # Team lights bake near white: the material tints them (and the plates) with the team colour through the mask.
    "TeamGlow": ("glow", (0.85, 0.85, 0.85), 0.3, 0.0, 1.0),
})


bw.CHIP_BIAS = 0.16
bw.GRIME_GAIN = 1.7
WHITE = (0.62, 0.62, 0.6)
YELLOW = (0.62, 0.42, 0.04)


def soldier_decals():
    """Original unit markings (image, axis, rect, slab, colour, surface); see bake_weapons.decal_layer. Mesh space,
    cm: X = the soldier's left, Y = forward. Trooper number 21, a Shieldbreak emblem and service stencils."""
    import weapon_decals as wd
    return [
        # Big left pauldron, outer face: trooper number over a bar; right pauldron: rank chevrons.
        (wd.text(8.0, 5.0, "21", "display"), "X+", (-7.5, 0.5, 145.5, 150.5), (24.0, 42.0), WHITE, "ArmorTeam"),
        (wd.bar(9.0, 0.5), "X+", (-8.0, 1.0, 144.2, 144.7), (24.0, 42.0), WHITE, "ArmorTeam"),
        (wd.chevrons(5.0, 3.2), "X-", (-6.0, -1.0, 144.5, 147.7), (-42.0, -24.0), WHITE, "ArmorTeam"),
        # Chest: emblem on the soldier's right pectoral, service plate on the left.
        (wd.emblem(4.2, 4.2), "Y+", (-18.5, -14.3, 140.0, 144.2), (8.0, 24.0), WHITE, "ArmorTeam"),
        (wd.lines(5.0, 1.6, ["TRP 21 / 3 PLT", "O POS  NKA"]), "Y+", (13.2, 18.2, 141.6, 143.2), (8.0, 24.0), WHITE, "ArmorTeam"),
        # Power pack: hatch warning, cell label, barcode.
        (wd.warning(7.0, 1.5, "HIGH ENERGY"), "Y-", (-3.5, 3.5, 143.2, 144.7), (-34.0, -24.0), YELLOW, "ArmorTeam"),
        (wd.text(8.0, 1.0, "PWR UNIT 4 / VENT BEFORE SERVICE", "bold"), "Y-", (-4.0, 4.0, 126.6, 127.6), (-34.0, -24.0), WHITE, "ArmorPlate"),
        (wd.barcode(5.0, 1.0), "Y-", (-2.5, 2.5, 128.4, 129.4), (-34.0, -24.0), WHITE, "ArmorPlate"),
        # Helmet: number on the back of the crown stripe, hazard ticks on the brow.
        (wd.text(3.6, 2.2, "21", "display"), "Y-", (-1.8, 1.8, 172.5, 174.7), (-16.0, -6.0), WHITE, "ArmorTeam"),
        # Left leg (mirrored onto the right, so symmetric marks only): bar and ticks on the greave, bar on the cuisse.
        (wd.bar(6.0, 0.6), "Y+", (9.6, 15.6, 40.6, 41.2), (4.0, 18.0), YELLOW, "ArmorPlate"),
        (wd.ticks(6.0, 0.8, 9), "Y+", (9.6, 15.6, 39.2, 40.0), (4.0, 18.0), WHITE, "ArmorPlate"),
        (wd.bar(7.0, 0.7), "Y+", (7.5, 14.5, 80.0, 80.7), (6.0, 20.0), WHITE, "ArmorPlate"),
    ]


def bake_team_mask(low, high, res):
    """One more selected-to-active EMIT bake: 1 where HIGH's surface is team-coloured."""
    scene = bw.setup_cycles(4)
    b = scene.render.bake
    b.use_selected_to_active = True
    b.cage_extrusion = 0.3
    b.max_ray_distance = 0.9
    target = None
    for slot in high.material_slots:
        mat = slot.material
        outputs, emission = bw.mat_outputs[mat.name]
        tree = mat.node_tree
        val = tree.nodes.new("ShaderNodeValue")
        val.outputs[0].default_value = 1.0 if mat.name[3:].split(".")[0] in bs.TEAM_SURFACES else 0.0
        for link in list(emission.inputs["Color"].links):
            tree.links.remove(link)
        tree.links.new(val.outputs[0], emission.inputs["Color"])
    for slot in low.material_slots:
        target = slot.material.node_tree.nodes.active.image
    bpy.ops.object.select_all(action="DESELECT")
    high.select_set(True)
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    bpy.ops.object.bake(type="EMIT")
    b.use_selected_to_active = False
    return bw.pixels(target).copy()


def preview_material(team, gain=3.2):
    """What M_SB_ArmorPBR does in Unreal: base colour and glow tinted by the team colour where the mask is set."""
    mat = bw.preview_material("Soldier", TEX_OUT)
    mat.name = "PV_Soldier_%s" % ("none" if team is None else "%.1f_%.1f_%.1f" % team)
    if team is None:
        return mat
    g = bw.Nodes(mat.node_tree)
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    mask = g.add("ShaderNodeTexImage")
    mask.image = bpy.data.images.load(os.path.join(TEX_OUT, "T_SB_Soldier_M.png"))
    mask.image.colorspace_settings.name = "Non-Color"
    bc = bsdf.inputs["Base Color"].links[0].from_socket
    tinted = g.scale_color(bc, 1.0)
    sb = next(s for s in tinted.node.inputs if s.identifier == "B_Color")
    sb.default_value = (team[0] * gain, team[1] * gain, team[2] * gain, 1.0)
    out = g.mix(g.sep(mask.outputs["Color"])[0], bc, tinted)
    g.put(bsdf.inputs["Base Color"], out)
    g.put(bsdf.inputs["Emission Color"], out)
    return mat


def run(args, verts, joints):
    t0 = time.time()
    res = int(args[args.index("--bake") + 1]) if "--bake" in args else 2048
    preview = args[args.index("--preview") + 1] if "--preview" in args else None
    low, ranges = bs.join_all(bs.build(verts, joints, high=False), "LOW")
    high, _ = bs.join_all(bs.build(verts, joints, high=True), "HIGH")
    for poly in high.data.polygons:
        poly.use_smooth = True
    print("soldier: LOW %d tris, HIGH %d tris (%.0fs)" % (k.triangle_count(low), k.triangle_count(high), time.time() - t0), flush=True)
    bw.unwrap(low)
    normal = bw.bake_normal(low, high, res)
    print("  normal baked (%.0fs)" % (time.time() - t0), flush=True)
    import weapon_decals as wd
    decals = []
    for i, (img, axis, rect, slab, colour, surface) in enumerate(soldier_decals()):
        path = wd.save(img, bw.DECAL_DIR, "Soldier_%02d" % i)
        decals.append({"image": bpy.data.images.load(path), "axis": axis, "rect": rect, "slab": slab, "color": colour, "surface": surface})
    surf = bw.bake_surfaces_from_high(low, high, decals, res, 16 if res >= 2048 else 8)
    team = bake_team_mask(low, high, res)
    bpy.data.objects.remove(high, do_unlink=True)
    bw.write_textures("SM_SB_Soldier", normal, surf, TEX_OUT)
    mask = np.stack([team[:, :, 0], surf["emit"][:, :, 0], surf["curv"][:, :, 0]], axis=2)
    bw.save_png(mask, os.path.join(TEX_OUT, "T_SB_Soldier_M.png"), "RGB")
    print("  textures written (%.0fs)" % (time.time() - t0), flush=True)
    bw.single_slot(low, SLOT)
    pieces = []
    for bone, (a, b) in ranges.items():
        piece = bw.extract(low, "SM_SB_Armor_" + bone, [(a, b)])
        pieces.append(piece)
        if bone in bs.MIRRORED:
            pieces.append(bs.mirrored(piece, "SM_SB_Armor_" + bs.MIRRORED[bone]))
    report = ["source: SKM_Manny.obj (build_soldier.py v3, %d px atlas, 1 material)" % res]
    for piece in pieces:
        report.append("%s: %d tris, bounds %s" % (piece.name, k.triangle_count(piece), k.bounds(piece)))
    report.append("total: %d tris" % sum(k.triangle_count(p) for p in pieces))
    print("\n".join(report))
    if "--no-export" not in args:
        out_dir = os.path.join(bw.EXPORT, "Armor")
        for piece in pieces:
            k.export_fbx(os.path.join(out_dir, piece.name + ".fbx"), [piece])
        with open(os.path.join(out_dir, "armor_report.txt"), "w") as f:
            f.write("\n".join(report) + "\n")
    if preview:
        import sb_render
        os.makedirs(preview, exist_ok=True)
        bpy.data.objects.remove(low, do_unlink=True)
        bs.load_body(args[1])
        for label, colour in (("grey", None), ("red", (0.55, 0.05, 0.04)), ("blue", (0.05, 0.16, 0.55))):
            mat = preview_material(colour)
            for piece in pieces:
                piece.data.materials[0] = mat
            for name in list(bpy.data.objects.keys()):
                if name.startswith(("Cam", "Key", "Rim", "Fill", "Plane")):
                    bpy.data.objects.remove(bpy.data.objects[name], do_unlink=True)
            out = os.path.join(preview, "SB_Soldier_%s.png" % label)
            sb_render.render_views(pieces, out, bs.VIEWS, resolution=(800, 1200), floor=True)
            if bw.DECAL_MIRROR:
                # Unreal shows these meshes mirrored against Blender (see DECAL_MIRROR), so flip each view to match.
                from PIL import Image, ImageOps
                sheet = Image.open(out)
                w = sheet.width // len(bs.VIEWS)
                for v in range(len(bs.VIEWS)):
                    sheet.paste(ImageOps.mirror(sheet.crop((v * w, 0, (v + 1) * w, sheet.height))), (v * w, 0))
                sheet.save(out)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(preview, "Soldier_textured.blend"), compress=True)
