"""Preview of the Keel brain tank (Cycles): builds it with build_keel_brain_tank.py, then renders the opaque mesh from the
room side with the glass left out, so the brain, clamps and probes can be checked before the engine import.

    blender --background --factory-startup --python Art/blender/render_keel_brain_tank.py -- <out.png>
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import build_keel_brain_tank as tank  # noqa: E402

out = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "brain_tank_preview.png"
tank.main()
scene = bpy.context.scene
glass = bpy.data.objects.get(tank.PREFIX + "Glass")
if glass:
    glass.hide_render = True
brain_mat = bpy.data.materials.get("M_TankBrain")
if brain_mat:
    nt = brain_mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    col = nt.nodes.new("ShaderNodeVertexColor")
    col.layer_name = "Col"
    nt.links.new(col.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.3
    if "Coat Weight" in bsdf.inputs:
        bsdf.inputs["Coat Weight"].default_value = 1.0
        bsdf.inputs["Coat Roughness"].default_value = 0.05
cam_data = bpy.data.cameras.new("Cam")
cam_data.lens = 35
cam = bpy.data.objects.new("Cam", cam_data)
scene.collection.objects.link(cam)
cam.location = Vector((330.0, 110.0, 40.0))
direction = Vector((30.0, 0.0, 0.0)) - cam.location
cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
scene.camera = cam
for loc, energy, size in (((150.0, -120.0, 160.0), 4.0e6, 120.0), ((60.0, 0.0, 70.0), 6.0e5, 60.0)):
    light = bpy.data.lights.new("L", "AREA")
    light.energy = energy
    light.size = size
    lo = bpy.data.objects.new("L", light)
    lo.location = loc
    lo.rotation_euler = (Vector((30.0, 0.0, 0.0)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(lo)
world = bpy.data.worlds.new("W")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.01, 0.008, 0.015, 1)
scene.world = world
scene.render.engine = "CYCLES"
scene.cycles.samples = 48
scene.cycles.device = "CPU"
scene.render.resolution_x, scene.render.resolution_y = 1280, 720
scene.view_settings.view_transform = "AgX" if "AgX" in [i.identifier for i in bpy.types.ColorManagedViewSettings.bl_rna.properties["view_transform"].enum_items] else "Filmic"
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
print("PREVIEW", out)
