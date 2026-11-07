"""3/4 close-up of the exported Battle Rifle with its baked textures, as Unreal would show it.

Unreal reads the FBX numbers as-is but is left-handed, so a right-handed Blender render of the same numbers is the
mirror image: render, then flip the image horizontally. Usage: python render_br34.py <ArtExportDir> <out.png>
"""
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

export_dir, out_png = sys.argv[1], sys.argv[2]
tex = os.path.join(export_dir, "Textures", "Weapons")

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
bpy.ops.import_scene.fbx(filepath=os.path.join(export_dir, "Weapons", "SM_SB_BattleRifle.fbx"), axis_forward="-Y", axis_up="Z")
meshes = [o for o in scene.objects if o.type == "MESH"]
for o in scene.objects:
    if o.type != "MESH":
        bpy.data.objects.remove(o, do_unlink=True)
gun = meshes[0]
print("imported", gun.name, "scale", tuple(gun.scale), "loc", tuple(gun.location), "rot", tuple(gun.rotation_euler))
# Undo the exporter's Y mirror so the numbers are the cm values Unreal uses.
if gun.scale.y < 0:
    gun.scale.y = -gun.scale.y
    gun.data.flip_normals()
bpy.context.view_layer.update()
pts = [gun.matrix_world @ v.co for v in gun.data.vertices]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
print("bounds", tuple(lo), tuple(hi))

mat = bpy.data.materials.new("PV")
mat.use_nodes = True
nt = mat.node_tree
bsdf = nt.nodes["Principled BSDF"]


def img(suffix, cs):
    n = nt.nodes.new("ShaderNodeTexImage")
    n.image = bpy.data.images.load(os.path.join(tex, "T_SB_BattleRifle_%s.png" % suffix))
    n.image.colorspace_settings.name = cs
    n.image.alpha_mode = "CHANNEL_PACKED"
    return n


bc, orm, nrm = img("BC", "sRGB"), img("ORM", "Non-Color"), img("N", "Non-Color")
nt.links.new(bsdf.inputs["Base Color"], bc.outputs["Color"])
so = nt.nodes.new("ShaderNodeSeparateColor")
nt.links.new(so.inputs["Color"], orm.outputs["Color"])
nt.links.new(bsdf.inputs["Roughness"], so.outputs[1])
nt.links.new(bsdf.inputs["Metallic"], so.outputs[2])
nt.links.new(bsdf.inputs["Emission Color"], bc.outputs["Color"])
em = nt.nodes.new("ShaderNodeMath")
em.operation = "MULTIPLY"
em.inputs[1].default_value = 5.0
nt.links.new(em.inputs[0], bc.outputs["Alpha"])
nt.links.new(bsdf.inputs["Emission Strength"], em.outputs[0])
sn = nt.nodes.new("ShaderNodeSeparateColor")
nt.links.new(sn.inputs["Color"], nrm.outputs["Color"])
inv = nt.nodes.new("ShaderNodeMath")
inv.operation = "SUBTRACT"
inv.inputs[0].default_value = 1.0
nt.links.new(inv.inputs[1], sn.outputs[1])
cn = nt.nodes.new("ShaderNodeCombineColor")
nt.links.new(cn.inputs[0], sn.outputs[0])
nt.links.new(cn.inputs[1], inv.outputs[0])
nt.links.new(cn.inputs[2], sn.outputs[2])
nm = nt.nodes.new("ShaderNodeNormalMap")
nt.links.new(nm.inputs["Color"], cn.outputs["Color"])
nt.links.new(bsdf.inputs["Normal"], nm.outputs["Normal"])
gun.data.materials.clear()
gun.data.materials.append(mat)

center = (lo + hi) / 2
size = (hi - lo).length
# The first-person camera sees the gun's -X side; 3/4 from the front-left, a little above.
d = Vector((-1.0, 0.75, 0.38)).normalized()
cam_data = bpy.data.cameras.new("cam")
cam_data.lens = 70
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
cam.location = center + d * size * 1.6
cam.rotation_euler = (center - cam.location).to_track_quat("-Z", "Y").to_euler()
cam_data.clip_start = size * 0.01
cam_data.clip_end = size * 20
scene.camera = cam

world = bpy.data.worlds.new("w")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.32, 0.34, 0.37, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
scene.world = world
for name, loc, energy in (("key", d * size * 2 + Vector((0, -size, size * 1.5)), 4.0), ("rim", Vector((size * 2, -size, size)), 2.0)):
    ld = bpy.data.lights.new(name, "AREA")
    ld.size = size
    ld.energy = energy * (size * size) * 40
    lo_ = bpy.data.objects.new(name, ld)
    lo_.location = center + loc
    lo_.rotation_euler = (center - lo_.location).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(lo_)

scene.render.engine = "CYCLES"
scene.cycles.samples = 48
scene.cycles.device = "CPU"
scene.render.resolution_x, scene.render.resolution_y = 1600, 900
scene.view_settings.view_transform = "AgX" if "AgX" in [i.identifier for i in type(scene.view_settings).bl_rna.properties["view_transform"].enum_items] else "Filmic"
tmp = out_png + ".raw.png"
scene.render.filepath = tmp
bpy.ops.render.render(write_still=True)
im = bpy.data.images.load(tmp)
w, h = im.size
px = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4)[:, ::-1, :]
out = bpy.data.images.new("out", w, h, alpha=True)
out.pixels = px.ravel()
out.filepath_raw = out_png
out.file_format = "PNG"
out.save()
print("wrote", out_png)

