"""Preview renders (Cycles CPU) for scripted art: a 3/4 view and a side view side by side."""
import math

import bpy
from mathutils import Vector


def _setup(scene, resolution):
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 48
    scene.cycles.use_denoising = True
    scene.render.resolution_x, scene.render.resolution_y = resolution
    scene.render.film_transparent = False
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"
    world = bpy.data.worlds.new("World")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.42, 0.47, 0.55, 1)
    bg.inputs["Strength"].default_value = 0.55
    for name, rot, energy, color in (("Key", (50, 10, 35), 3.2, (1.0, 0.95, 0.88)), ("Rim", (60, 0, 200), 2.2, (0.75, 0.85, 1.0)),
                                     ("Fill", (75, 0, -70), 0.8, (1, 1, 1))):
        light = bpy.data.lights.new(name, "SUN")
        light.energy = energy
        light.color = color
        light.angle = math.radians(8)
        obj = bpy.data.objects.new(name, light)
        obj.rotation_euler = [math.radians(a) for a in rot]
        scene.collection.objects.link(obj)


def _bbox(objects):
    pts = [o.matrix_world @ Vector(c) for o in objects for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def _camera(scene, name, direction, center, size, ortho):
    cam_data = bpy.data.cameras.new(name)
    cam = bpy.data.objects.new(name, cam_data)
    scene.collection.objects.link(cam)
    direction = Vector(direction).normalized()
    distance = size * 3.0
    cam.location = center + direction * distance
    cam.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
    cam_data.clip_end = distance * 10
    if ortho:
        cam_data.type = "ORTHO"
        cam_data.ortho_scale = size * 1.15
    else:
        cam_data.lens = 85
        cam_data.sensor_width = 36
        cam.location = center + direction * size * 2.9
    return cam


def render_views(objects, path, views, resolution=(900, 600), floor=False):
    """views: list of (direction, ortho). Renders each and stitches them horizontally into path."""
    scene = bpy.context.scene
    _setup(scene, resolution)
    lo, hi = _bbox(objects)
    center = (lo + hi) * 0.5
    ext = hi - lo
    # The camera's field of view spans the frame's longer side; fit the subject's width and height in it.
    w, h = resolution
    if w >= h:
        size = max(ext.x, ext.y, ext.z * w / h * 1.1)
    else:
        size = max(ext.z * 1.1, max(ext.x, ext.y) * h / w)
    if floor:
        bpy.ops.mesh.primitive_plane_add(size=size * 20, location=(center.x, center.y, lo.z))
        plane = bpy.context.active_object
        mat = bpy.data.materials.new("Floor")
        mat.use_nodes = True
        mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.3, 0.32, 0.35, 1)
        plane.data.materials.append(mat)
    images = []
    for i, (direction, ortho) in enumerate(views):
        scene.camera = _camera(scene, "Cam%d" % i, direction, center, size, ortho)
        part = path.replace(".png", "_v%d.png" % i)
        scene.render.filepath = part
        bpy.ops.render.render(write_still=True)
        images.append(part)
    _stitch(images, path)


def _stitch(paths, out):
    imgs = [bpy.data.images.load(p) for p in paths]
    w = sum(i.size[0] for i in imgs)
    h = max(i.size[1] for i in imgs)
    sheet = bpy.data.images.new("sheet", w, h)
    pixels = [0.0] * (w * h * 4)
    x0 = 0
    for img in imgs:
        iw, ih = img.size
        src = list(img.pixels)
        for y in range(ih):
            row = src[y * iw * 4:(y + 1) * iw * 4]
            start = (y * w + x0) * 4
            pixels[start:start + iw * 4] = row
        x0 += iw
    sheet.pixels = pixels
    sheet.filepath_raw = out
    sheet.file_format = "PNG"
    sheet.save()
    import os
    for p in paths:
        os.remove(p)


def render_object(obj, path):
    # 3/4 view from the right-front-above, and a straight side view from +X.
    render_views([obj], path, [((1.0, 0.9, 0.45), False), ((1.0, 0.0, 0.0), True)])
