"""Preview renders for the Keel shell kit (Cycles CPU): a contact sheet of the pieces and an assembled corridor.

Called by build_keel_shell_kit.py --render. Writes JPGs to docs/playtests/2026-11-25_keel_shell/.

The preview materials add what the engine material will need to match: large-scale tonal mottling, gloss variation,
a faint thin-film sheen on the purple metal (the reference's iridescence), and crevice darkening. They are preview
stand-ins; the engine instances come from the flat palette values in build_keel_shell_kit.SHELL_PALETTE.

Flags: --quick (low samples), --only=sheet|corridor|window|hall.
"""
import math
import os
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
SHOTS = ROOT / "docs/playtests/2026-11-25_keel_shell"
RES_SCALE = 100
CM = 0.01  # the kit is authored in cm; the render scene works in metres so Cycles' light falloff is physical


# ---- Preview materials ----------------------------------------------------------------------------------------------
def _nodes(mat):
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    return nt, nt.nodes, nt.links


def metal_material(mat, base, base2, metallic, rough, rough2, film=0.0, mottle_scale=0.55, bump=0.04):
    nt, N, L = _nodes(mat)
    out = N.new("ShaderNodeOutputMaterial")
    bsdf = N.new("ShaderNodeBsdfPrincipled")
    geo = N.new("ShaderNodeNewGeometry")
    big = N.new("ShaderNodeTexNoise")
    big.inputs["Scale"].default_value = mottle_scale
    big.inputs["Detail"].default_value = 3.0
    fine = N.new("ShaderNodeTexNoise")
    fine.inputs["Scale"].default_value = 14.0
    fine.inputs["Detail"].default_value = 6.0
    L.new(geo.outputs["Position"], big.inputs["Vector"])
    L.new(geo.outputs["Position"], fine.inputs["Vector"])
    ramp = N.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[0].color = (*base, 1)
    ramp.color_ramp.elements[1].position = 0.68
    ramp.color_ramp.elements[1].color = (*base2, 1)
    L.new(big.outputs["Fac"], ramp.inputs["Fac"])
    ao = N.new("ShaderNodeAmbientOcclusion")
    ao.inputs["Distance"].default_value = 0.25
    ao.samples = 8
    aomap = N.new("ShaderNodeMapRange")
    aomap.inputs["To Min"].default_value = 0.45
    L.new(ao.outputs["AO"], aomap.inputs["Value"])
    mul = N.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    L.new(ramp.outputs["Color"], mul.inputs["A"])
    L.new(aomap.outputs["Result"], mul.inputs["B"])
    L.new(mul.outputs["Result"], bsdf.inputs["Base Color"])
    rr = N.new("ShaderNodeMapRange")
    rr.inputs["To Min"].default_value = rough
    rr.inputs["To Max"].default_value = rough2
    L.new(fine.outputs["Fac"], rr.inputs["Value"])
    L.new(rr.outputs["Result"], bsdf.inputs["Roughness"])
    bmp = N.new("ShaderNodeBump")
    bmp.inputs["Strength"].default_value = bump
    bmp.inputs["Distance"].default_value = 0.002
    L.new(fine.outputs["Fac"], bmp.inputs["Height"])
    L.new(bmp.outputs["Normal"], bsdf.inputs["Normal"])
    bsdf.inputs["Metallic"].default_value = metallic
    if film > 0 and "Thin Film Thickness" in bsdf.inputs:
        bsdf.inputs["Thin Film Thickness"].default_value = film
        bsdf.inputs["Thin Film IOR"].default_value = 1.45
    L.new(bsdf.outputs["BSDF"], out.inputs["Surface"])


def glow_material(mat, color, strength):
    nt, N, L = _nodes(mat)
    out = N.new("ShaderNodeOutputMaterial")
    em = N.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (*color, 1)
    em.inputs["Strength"].default_value = strength
    L.new(em.outputs["Emission"], out.inputs["Surface"])


def setup_materials():
    spec = {
        "M_ShellHull": lambda m: metal_material(m, (0.075, 0.018, 0.110), (0.140, 0.040, 0.190), 0.92, 0.14, 0.28, film=320),
        "M_ShellPlate": lambda m: metal_material(m, (0.220, 0.080, 0.300), (0.340, 0.140, 0.420), 0.92, 0.09, 0.18, film=380,
                                                 mottle_scale=0.9),
        "M_ShellSteel": lambda m: metal_material(m, (0.070, 0.045, 0.090), (0.120, 0.085, 0.150), 1.0, 0.22, 0.38),
        "M_ShellGrate": lambda m: metal_material(m, (0.012, 0.010, 0.018), (0.026, 0.022, 0.034), 0.8, 0.35, 0.55),
        "M_ShellGlowCyan": lambda m: glow_material(m, (0.30, 0.62, 1.00), 6.0),
        "M_ShellGlowBlue": lambda m: glow_material(m, (0.12, 0.28, 1.00), 7.0),
        "M_ShellGlowAmber": lambda m: glow_material(m, (1.00, 0.52, 0.14), 9.0),
    }
    for name, fn in spec.items():
        mat = bpy.data.materials.get(name)
        if mat:
            fn(mat)


def plain_material(name, color, metallic=0.0, rough=0.5, emission=None, strength=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    b = mat.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    if emission:
        b.inputs["Emission Color"].default_value = (*emission, 1)
        b.inputs["Emission Strength"].default_value = strength
    return mat


# ---- Scene helpers --------------------------------------------------------------------------------------------------
def render_settings(scene, res, samples):
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.cycles.use_adaptive_sampling = True
    scene.cycles.adaptive_threshold = 0.02
    scene.cycles.use_denoising = True
    scene.cycles.max_bounces = 6
    scene.cycles.glossy_bounces = 4
    scene.cycles.diffuse_bounces = 3
    scene.cycles.volume_bounces = 1
    scene.cycles.sample_clamp_indirect = 8.0
    scene.cycles.blur_glossy = 0.5
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.resolution_percentage = RES_SCALE
    # Filmic rather than AgX: AgX turns saturated purple toward blue, Unreal's filmic curve keeps it purple.
    scene.view_settings.view_transform = "Filmic"
    scene.view_settings.look = "Medium High Contrast"
    scene.render.image_settings.file_format = "JPEG"
    scene.render.image_settings.quality = 92
    scene.unit_settings.scale_length = 1.0


def world(scene, color, strength):
    w = bpy.data.worlds.new("World")
    scene.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (*color, 1)
    bg.inputs["Strength"].default_value = strength


def place(src, name, loc_cm, rot_deg=(0, 0, 0), scale=(1, 1, 1)):
    obj = bpy.data.objects.new(name, src.data)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = Vector(loc_cm) * CM
    obj.rotation_euler = [math.radians(a) for a in rot_deg]
    obj.scale = Vector(scale) * CM
    return obj


def camera(scene, loc, target, lens=20.0, name="Cam"):
    data = bpy.data.cameras.new(name)
    data.lens = lens
    data.sensor_width = 36
    data.clip_start = 0.05
    data.clip_end = 200
    cam = bpy.data.objects.new(name, data)
    scene.collection.objects.link(cam)
    cam.location = Vector(loc)
    cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    return cam


def area(scene, name, loc, size, power, color, rot=(0, 0, 0), shape="RECTANGLE"):
    d = bpy.data.lights.new(name, "AREA")
    d.shape = shape
    d.size = size[0]
    d.size_y = size[1]
    d.energy = power
    d.color = color
    o = bpy.data.objects.new(name, d)
    o.location = loc
    o.rotation_euler = [math.radians(a) for a in rot]
    scene.collection.objects.link(o)
    return o


def point(scene, name, loc, power, color, radius=0.05):
    d = bpy.data.lights.new(name, "POINT")
    d.energy = power
    d.color = color
    d.shadow_soft_size = radius
    o = bpy.data.objects.new(name, d)
    o.location = loc
    scene.collection.objects.link(o)
    return o


def hide_sources(pieces):
    for obj in pieces.values():
        obj.hide_render = True
        obj.hide_viewport = True


def clear_scene_except(pieces):
    keep = set(id(o) for o in pieces.values())
    for o in list(bpy.data.objects):
        if id(o) not in keep:
            bpy.data.objects.remove(o, do_unlink=True)
    for coll in (bpy.data.lights, bpy.data.cameras, bpy.data.worlds):
        for item in list(coll):
            if item.users == 0:
                coll.remove(item)


# ---- Contact sheet --------------------------------------------------------------------------------------------------
SHEET_ORDER = ("WallBay", "WallBayVent", "WallBayWindow", "Rib", "Pillar", "Pylon", "Portal", "CeilingSpine", "RibBeam",
               "FloorPlate", "FloorGrate", "EdgeStrip")


def render_piece(scene, pieces, short, path, samples):
    clear_scene_except(pieces)
    src = pieces[short]
    obj = place(src, short + "_shot", (0, 0, 0))
    obj.hide_render = False
    bpy.context.view_layer.update()
    # Dark studio: low violet ambient, a cool key and a warm rim so the gloss and the curvature read.
    world(scene, (0.010, 0.008, 0.016), 1.0)
    pts = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    centre = (lo + hi) / 2
    size = max((hi - lo).length, 0.5)
    floor_mat = plain_material("Studio", (0.012, 0.010, 0.016), 0.6, 0.35)
    bpy.ops.mesh.primitive_plane_add(size=60, location=(centre.x, centre.y, lo.z - 0.002))
    bpy.context.active_object.data.materials.append(floor_mat)
    # Wall pieces face +X; the portal faces +Y; floor and ceiling pieces are seen from above or below.
    view = {"WallBay": (1.0, -0.9, 0.35), "WallBayVent": (1.0, -0.9, 0.35), "WallBayWindow": (1.0, -0.9, 0.35),
            "Rib": (1.0, -1.1, 0.30), "Portal": (0.55, 1.0, 0.35), "CeilingSpine": (0.6, -0.9, -0.9),
            "RibBeam": (0.4, -1.0, -0.6), "FloorPlate": (0.6, -0.9, 1.1), "FloorGrate": (0.6, -0.9, 1.1),
            "EdgeStrip": (-1.0, -0.8, 0.45)}.get(short, (0.9, -1.0, 0.35))
    d = Vector(view).normalized()
    if short in ("CeilingSpine", "RibBeam"):
        bpy.data.objects.remove(bpy.context.active_object, do_unlink=True)
    cam = camera(scene, centre + d * size * 1.45, centre, lens=40)
    key_dir = Vector((0.6, -1.0, 1.2)).normalized() if d.z >= 0 else Vector((0.6, -1.0, -1.2)).normalized()
    area(scene, "Key", centre + key_dir * size * 1.2, (size * 0.7, size * 0.7), 140 * size * size, (0.80, 0.86, 1.0),
         rot=(0, 0, 0)).rotation_euler = (-key_dir).to_track_quat("-Z", "Y").to_euler()
    rim_dir = Vector((-0.9, 0.8, 0.5)).normalized()
    area(scene, "Rim", centre + rim_dir * size * 1.2, (size * 0.5, size * 0.12), 220 * size * size, (1.0, 0.62, 0.36)
         ).rotation_euler = (-rim_dir).to_track_quat("-Z", "Y").to_euler()
    top = Vector((0.0, 0.3, 1.0)).normalized()
    area(scene, "Top", centre + top * size * 1.3, (size * 1.2, size * 0.3), 50 * size * size, (0.75, 0.55, 1.0)
         ).rotation_euler = (-top).to_track_quat("-Z", "Y").to_euler()
    render_settings(scene, (640, 480), samples)
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    return path


def contact_sheet(paths, out):
    from PIL import Image, ImageDraw, ImageFont
    cols = 4
    w, h = 640, 480
    rows = (len(paths) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w, rows * h), (10, 8, 14))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 20)
    except OSError:
        font = ImageFont.load_default()
    for i, (label, p) in enumerate(paths):
        im = Image.open(p).convert("RGB")
        x, y = (i % cols) * w, (i // cols) * h
        sheet.paste(im, (x, y))
        draw.text((x + 12, y + 10), label, fill=(225, 220, 235), font=font)
    sheet.save(out, quality=90)


# ---- Assembled corridor ---------------------------------------------------------------------------------------------
HALF_W = 562.0       # wall line, cm from the corridor centre
BAYS = (-600.0, -200.0, 200.0, 600.0)  # bay centres along Y
CEIL = 760.0


def assemble_corridor(pieces, window_bay=200.0, vent_bays=(-200.0,)):
    objs = []
    for y in BAYS:
        # Left wall (local +X into the room already), right wall turned 180 degrees.
        left = "WallBayVent" if y in vent_bays else "WallBay"
        right = "WallBayWindow" if y == window_bay else ("WallBayVent" if y == 600.0 else "WallBay")
        objs.append(place(pieces[left], f"L{y}", (-HALF_W, y, 0)))
        objs.append(place(pieces[right], f"R{y}", (HALF_W, y, 0), (0, 0, 180)))
        objs.append(place(pieces["CeilingSpine"], f"C{y}", (0, y, CEIL)))
        objs.append(place(pieces["FloorGrate"], f"FG{y}", (0, y, 0)))
        for x in (-400.0, 400.0):
            objs.append(place(pieces["FloorPlate"], f"FP{x}{y}", (x, y, 0)))
    for y in (-800.0, -400.0, 0.0, 400.0, 800.0):
        objs.append(place(pieces["Rib"], f"RL{y}", (-HALF_W, y, 0)))
        objs.append(place(pieces["Rib"], f"RR{y}", (HALF_W, y, 0), (0, 0, 180)))
        objs.append(place(pieces["RibBeam"], f"RB{y}", (0, y, CEIL)))
    # End wall with the portal and a lit room beyond it; a closed wall behind the camera.
    objs.append(place(pieces["Portal"], "Portal", (0, 800, 0), (0, 0, 180)))
    objs.append(place(pieces["Pylon"], "PylonL", (-330, 640, 0)))
    objs.append(place(pieces["Pylon"], "PylonR", (330, 640, 0)))
    objs.append(place(pieces["Pillar"], "PillarL", (-350, -500, 0), scale=(1, 1, CEIL / 600.0)))
    return objs


def end_walls(scene):
    hull = bpy.data.materials["M_ShellHull"]
    for name, verts in (
            ("EndWallL", [(-5.62, 8.0, 0), (-2.5, 8.0, 0), (-2.5, 8.0, 7.8), (-5.62, 8.0, 7.8)]),
            ("EndWallR", [(2.5, 8.0, 0), (5.62, 8.0, 0), (5.62, 8.0, 7.8), (2.5, 8.0, 7.8)]),
            ("EndWallT", [(-2.5, 8.0, 3.5), (2.5, 8.0, 3.5), (2.5, 8.0, 7.8), (-2.5, 8.0, 7.8)]),
            ("BackWall", [(-5.62, -8.05, 0), (5.62, -8.05, 0), (5.62, -8.05, 7.8), (-5.62, -8.05, 7.8)]),
            ("Roof", [(-7, -9, 8.3), (7, -9, 8.3), (7, 14, 8.3), (-7, 14, 8.3)])):
        me = bpy.data.meshes.new(name)
        me.from_pydata(verts, [], [(0, 1, 2, 3)])
        o = bpy.data.objects.new(name, me)
        o.data.materials.append(hull)
        scene.collection.objects.link(o)
    # Room beyond the portal: a dark floor and back wall lit teal.
    room_mat = plain_material("RoomBeyond", (0.03, 0.02, 0.05), 0.7, 0.3)
    for name, verts in (("RoomFloor", [(-6, 8.0, 0), (6, 8.0, 0), (6, 16, 0), (-6, 16, 0)]),
                        ("RoomBack", [(-6, 15, 0), (6, 15, 0), (6, 15, 8), (-6, 15, 8)])):
        me = bpy.data.meshes.new(name)
        me.from_pydata(verts, [], [(0, 1, 2, 3)])
        o = bpy.data.objects.new(name, me)
        o.data.materials.append(room_mat)
        scene.collection.objects.link(o)
    area(scene, "Beyond", (0, 13.0, 5.5), (5, 3), 900, (0.35, 0.75, 1.0), rot=(40, 0, 0))


def space_backdrop(scene):
    """A star field and a large warm planet outside the right-hand window (emission plane)."""
    mat = bpy.data.materials.new("Space")
    mat.use_nodes = True
    nt = mat.node_tree
    N, L = nt.nodes, nt.links
    N.clear()
    out = N.new("ShaderNodeOutputMaterial")
    em = N.new("ShaderNodeEmission")
    tc = N.new("ShaderNodeTexCoord")
    vor = N.new("ShaderNodeTexVoronoi")
    vor.feature = "F1"
    vor.inputs["Scale"].default_value = 160.0
    stars = N.new("ShaderNodeMapRange")
    stars.inputs["From Min"].default_value = 0.0
    stars.inputs["From Max"].default_value = 0.04
    stars.inputs["To Min"].default_value = 1.0
    stars.inputs["To Max"].default_value = 0.0
    L.new(tc.outputs["UV"], vor.inputs["Vector"])
    L.new(vor.outputs["Distance"], stars.inputs["Value"])
    # Planet disc centred low in the window.
    sep = N.new("ShaderNodeSeparateXYZ")
    L.new(tc.outputs["UV"], sep.inputs["Vector"])
    dx = N.new("ShaderNodeMath"); dx.operation = "SUBTRACT"; dx.inputs[1].default_value = 0.47
    dy = N.new("ShaderNodeMath"); dy.operation = "SUBTRACT"; dy.inputs[1].default_value = 0.30
    L.new(sep.outputs["X"], dx.inputs[0]); L.new(sep.outputs["Y"], dy.inputs[0])
    comb = N.new("ShaderNodeCombineXYZ")
    L.new(dx.outputs[0], comb.inputs["X"]); L.new(dy.outputs[0], comb.inputs["Y"])
    ln = N.new("ShaderNodeVectorMath"); ln.operation = "LENGTH"
    L.new(comb.outputs["Vector"], ln.inputs[0])
    disc = N.new("ShaderNodeMapRange")
    disc.inputs["From Min"].default_value = 0.13
    disc.inputs["From Max"].default_value = 0.135
    disc.inputs["To Min"].default_value = 1.0
    disc.inputs["To Max"].default_value = 0.0
    L.new(ln.outputs["Value"], disc.inputs["Value"])
    bands = N.new("ShaderNodeTexWave")
    bands.wave_type = "BANDS"
    bands.bands_direction = "Y"
    bands.inputs["Scale"].default_value = 6.0
    bands.inputs["Distortion"].default_value = 3.0
    L.new(tc.outputs["UV"], bands.inputs["Vector"])
    pcol = N.new("ShaderNodeValToRGB")
    pcol.color_ramp.elements[0].color = (0.55, 0.20, 0.07, 1)
    pcol.color_ramp.elements[1].color = (1.0, 0.70, 0.40, 1)
    L.new(bands.outputs["Fac"], pcol.inputs["Fac"])
    mix = N.new("ShaderNodeMix"); mix.data_type = "RGBA"
    L.new(disc.outputs["Result"], mix.inputs["Factor"])
    starcol = N.new("ShaderNodeMix"); starcol.data_type = "RGBA"
    starcol.inputs["A"].default_value = (0.004, 0.004, 0.012, 1)
    starcol.inputs["B"].default_value = (1.0, 1.0, 1.0, 1)
    L.new(stars.outputs["Result"], starcol.inputs["Factor"])
    L.new(starcol.outputs["Result"], mix.inputs["A"])
    L.new(pcol.outputs["Color"], mix.inputs["B"])
    L.new(mix.outputs["Result"], em.inputs["Color"])
    em.inputs["Strength"].default_value = 1.6
    L.new(em.outputs["Emission"], out.inputs["Surface"])
    bpy.ops.mesh.primitive_plane_add(size=1, location=(14.0, 4.0, 2.0), rotation=(math.radians(90), 0, math.radians(-90)))
    plane = bpy.context.active_object
    plane.scale = (40, 22, 1)
    plane.data.materials.append(mat)
    plane.visible_shadow = False
    plane.visible_diffuse = False


def haze(scene, density=0.0045):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 1.0, 4.0))
    cube = bpy.context.active_object
    cube.scale = (11.4, 18.2, 8.0)
    mat = bpy.data.materials.new("Haze")
    mat.use_nodes = True
    N = mat.node_tree.nodes
    N.remove(N["Principled BSDF"])
    vol = N.new("ShaderNodeVolumePrincipled")
    vol.inputs["Density"].default_value = density
    vol.inputs["Color"].default_value = (0.75, 0.70, 0.95, 1)
    vol.inputs["Anisotropy"].default_value = 0.55
    mat.node_tree.links.new(vol.outputs["Volume"], N["Material Output"].inputs["Volume"])
    cube.data.materials.append(mat)


def corridor_lights(scene, sun=True):
    world(scene, (0.006, 0.004, 0.012), 1.0)
    # Spine troughs: one soft strip per bay (the trough mesh glows too).
    for y in BAYS:
        area(scene, f"Trough{y}", (0, y * CM, 7.97), (0.32, 3.8), 200, (0.85, 0.72, 1.0), rot=(0, 0, 0))
        for sx in (-1.3, 1.3):
            for dy in (-1.0, 1.0):
                point(scene, f"Lamp{y}{sx}{dy}", (sx, y * CM + dy, 7.50), 12, (0.8, 0.85, 1.0), radius=0.05)
    # Warm planet light through the window (right wall, bay at y = 2 m): a sun from +X, low and slightly back.
    if sun:
        d = bpy.data.lights.new("Planet", "SUN")
        d.energy = 10.0
        d.color = (1.0, 0.58, 0.30)
        d.angle = math.radians(1.5)
        o = bpy.data.objects.new("Planet", d)
        dirv = Vector((-1.0, -0.32, -0.42)).normalized()   # direction the light travels
        o.rotation_euler = dirv.to_track_quat("-Z", "Y").to_euler()
        scene.collection.objects.link(o)
    # Pylon crystals throw a cool pool each; the pillar's slits a faint one.
    for x in (-3.3, 3.3):
        point(scene, f"Crystal{x}", (x, 6.4, 2.6), 60, (0.3, 0.7, 1.0), radius=0.15)


def render_corridor_views(scene, pieces, samples, only):
    clear_scene_except(pieces)
    assemble_corridor(pieces)
    end_walls(scene)
    space_backdrop(scene)
    corridor_lights(scene)
    haze(scene)
    render_settings(scene, (1600, 900), samples)
    scene.cycles.volume_step_rate = 8.0
    views = {
        "corridor": ((-1.4, -7.4, 1.75), (0.7, 6.0, 2.7), 18.0),
        "window": ((-3.6, -3.4, 1.75), (3.8, 2.4, 3.0), 20.0),
        "hall": ((2.6, -6.8, 1.75), (-2.2, 1.0, 4.6), 18.0),
    }
    written = []
    for key, (loc, target, lens) in views.items():
        if only and key not in only:
            continue
        for c in [o for o in bpy.data.objects if o.type == "CAMERA"]:
            bpy.data.objects.remove(c, do_unlink=True)
        camera(scene, loc, target, lens)
        path = SHOTS / f"keel_shell_{key}.jpg"
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        written.append(path)
        print("wrote", path)
    return written


def render_all(pieces, argv):
    SHOTS.mkdir(parents=True, exist_ok=True)
    quick = "--quick" in argv
    global RES_SCALE
    RES_SCALE = 50 if "--draft" in argv else 100
    only = set()
    for a in argv:
        if a.startswith("--only="):
            only |= set(a.split("=", 1)[1].split(","))
    scene = bpy.context.scene
    setup_materials()
    hide_sources(pieces)
    if not only or "sheet" in only:
        shots = []
        for short in SHEET_ORDER:
            p = SHOTS / f"piece_{short}.jpg"
            render_piece(scene, pieces, short, p, 24 if quick else 96)
            shots.append((short, p))
        contact_sheet(shots, SHOTS / "keel_shell_pieces.jpg")
        for _, p in shots:
            p.unlink()
    if not only or only - {"sheet"}:
        render_corridor_views(scene, pieces, 32 if quick else 96, only - {"sheet"})
