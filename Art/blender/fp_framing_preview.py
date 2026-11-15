"""Previews the first-person rifle framing in Blender, matching CalibrateFirstPersonArms (78 deg FP FOV, gun origin at
(forward, right, -down) in camera space, barrel at pitch/yaw). Task (17) matched the in-game frame. Usage:
    python fp_framing_preview.py out.png FORWARD RIGHT DOWN PITCH YAW [PALM_RIGHT PALM_FORWARD PALM_UP]"""
import sys, math
sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
import bpy
from mathutils import Matrix, Vector
import sb_kit as k, build_weapons as bw, sb_render
out = sys.argv[1]
F, R, D, P, Y = map(float, sys.argv[2:7])
pr, pf, pu = map(float, sys.argv[7:10]) if len(sys.argv) > 9 else (1.5, 4.0, 1.5)
k.reset_scene()
parts, muzzle = bw.battle_rifle()
blade, rest = bw.TRIGGERS["SM_SB_BattleRifle"]
parts.append(bw.trigger_blade(*blade))
bpy.context.view_layer.update()
gun = k.join("gun", parts)
sx, sy, sz = bw.SUPPORT["SM_SB_BattleRifle"]
palm = Vector((sx + pr, sy + pf, sz + pu))
# A stand-in left hand: palm block under the forend at the palm target, fingers across toward +X.
bpy.ops.mesh.primitive_cube_add(size=1, location=palm + Vector((0, 0, -1.2)))
hand = bpy.context.active_object; hand.scale = (8, 7, 2.6)
m = bpy.data.materials.new("Glove"); m.use_nodes = True
m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.03, 0.03, 0.035, 1)
hand.data.materials.append(m)
bpy.ops.mesh.primitive_cube_add(size=1, location=palm + Vector((-4.5, -5, -2)))
wrist = bpy.context.active_object; wrist.scale = (6, 10, 6); wrist.data.materials.append(m)
# Place: gun origin at camera-space (F forward, R right, -D up); Blender camera looks along +Y, X right, Z up.
rot = Matrix.Rotation(math.radians(-Y), 4, "Z") @ Matrix.Rotation(math.radians(P), 4, "X")
xf = Matrix.Translation((R, F, -D)) @ rot
for o in (gun, hand, wrist):
    o.matrix_world = xf @ o.matrix_world
scene = bpy.context.scene
sb_render._setup(scene, (1280, 720))
cam_data = bpy.data.cameras.new("FP"); cam_data.lens_unit = "FOV"; cam_data.sensor_fit = "HORIZONTAL"
cam_data.angle = math.radians(78); cam_data.clip_start = 1
cam = bpy.data.objects.new("FP", cam_data); scene.collection.objects.link(cam)
cam.rotation_euler = (math.radians(90), 0, 0)  # look along +Y
scene.camera = cam
# Crosshair marker far ahead.
bpy.ops.mesh.primitive_uv_sphere_add(radius=1.5, location=(0, 1000, 0))
bpy.ops.mesh.primitive_plane_add(size=6000, location=(0, 0, -160))
scene.render.filepath = out
bpy.ops.render.render(write_still=True)
