"""Original Spire induction condenser, authored in Blender centimetres.

Three swept vanes, recessed rotor/iris, curved conduits, segmented clamps and integrated practical lights.
Ceiling pivot at Z=0; the whole assembly stays above Z=-280 (19.2 m under the current ceiling).
Run portable Blender --background --factory-startup --python Art/blender/build_spire_hero.py.
Only the new mesh/report and two new palette entries are written; existing hero sources/assets are retained.
"""
import json
import math
import os
from pathlib import Path
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit
import build_map_kit as kit
import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'Art/Export/MapKit'
NAME = 'SM_SB_Kit_CondenserV2'


def radial(radius, angle, z):
    a = math.radians(angle)
    return (radius*math.cos(a), radius*math.sin(a), z)


def cylinder(name, radius, depth, location, mat, bevel=3.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=48 if radius > 200 else 24, radius=radius, depth=depth, location=location)
    obj = bpy.context.object; obj.name = name
    for face in obj.data.polygons:
        face.use_smooth = len(face.vertices) == 4
    return sb_kit._finish(obj, mat, bevel, segments=2)


def torus(name, radius, tube, z, mat, segments=72):
    bpy.ops.mesh.primitive_torus_add(major_segments=segments, minor_segments=12,
                                   major_radius=radius, minor_radius=tube, location=(0, 0, z))
    obj = bpy.context.object; obj.name = name
    for face in obj.data.polygons: face.use_smooth = True
    return sb_kit._finish(obj, mat, 0)


def conduit(name, points, radius, mat):
    data = bpy.data.curves.new(name, 'CURVE'); data.dimensions = '3D'
    data.resolution_u = 6; data.bevel_depth = radius; data.bevel_resolution = 1
    data.use_fill_caps = True
    spline = data.splines.new('BEZIER'); spline.bezier_points.add(len(points)-1)
    for knot, point in zip(spline.bezier_points, points):
        knot.co = point; knot.handle_left_type = 'AUTO'; knot.handle_right_type = 'AUTO'
    obj = bpy.data.objects.new(name, data); bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(sb_kit.material(mat))
    return obj


def blade_xy(name, points, z, height, mat):
    bm = bmesh.new()
    bottom = [bm.verts.new((x, y, z)) for x, y in points]
    top = [bm.verts.new((x, y, z+height)) for x, y in points]
    bm.faces.new(list(reversed(bottom))); bm.faces.new(top)
    for i in range(len(points)):
        j = (i+1) % len(points)
        bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name); bm.to_mesh(mesh); bm.free()
    return sb_kit._finish(sb_kit._link(name, mesh), mat, 2.5, segments=2)


def build():
    sb_kit.reset_scene()
    parts = [cylinder('ceiling_mount', 680, 44, (0, 0, -22), 'Gunmetal', 7),
             torus('load_ring', 620, 28, -85, 'Steel'),
             torus('lower_race', 335, 18, -224, 'Steel'),
             torus('induction_channel', 280, 9, -242, 'GlowCyan'),
             cylinder('iris_back', 315, 20, (0, 0, -228), 'Gunmetal', 3),
             cylinder('dark_aperture', 88, 10, (0, 0, -248), 'Gunmetal', 2),
             cylinder('energy_lens', 54, 24, (0, 0, -263), 'GlowCyan', 5)]

    # Segmented clamps have a separate plate, recessed web and small retained fasteners.
    for index in range(12):
        angle = index*30+15
        web = sb_kit.box('clamp_web', (0, 0, 0), (105, 85, 150), 'Gunmetal', bevel=7, segments=2)
        parts.append(kit.placed(web, radial(665, angle, -108), (0, 0, angle)))
        face = sb_kit.box('clamp_face', (0, 0, 0), (28, 67, 116), 'Steel', bevel=5, segments=2)
        parts.append(kit.placed(face, radial(729, angle, -108), (0, 0, angle)))
        for dz in (-40, 40):
            fastener = sb_kit.box('retainer', (0, 0, 0), (12, 30, 12), 'SpireBronze', bevel=2, segments=2)
            parts.append(kit.placed(fastener, radial(746, angle, -108+dz), (0, 0, angle)))

    # Three asymmetrical swept silhouettes establish the condenser's own triad, rather than an outline ring.
    profile = [(415, -55), (755, -38), (1060, -118), (1010, -188), (690, -218), (415, -180)]
    spine = [(425, -170), (700, -235), (1015, -205), (1060, -150), (740, -193), (425, -135)]
    for angle in (0, 120, 240):
        fin = kit.prism_xz('swept_vane', profile, -70, 70, 'Steel', bevel=8)
        parts.append(kit.placed(fin, (0, 0, 0), (0, 0, angle)))
        keel = kit.prism_xz('recessed_keel', spine, -28, 28, 'Gunmetal', bevel=4)
        parts.append(kit.placed(keel, (0, 0, 0), (0, 0, angle)))
        for offset in (-32, 32):
            a = math.radians(angle)
            lateral = (-math.sin(a)*offset, math.cos(a)*offset)
            points = [radial(radius, angle, z) for radius, z in ((285, -198), (490, -134), (715, -158), (966, -180))]
            points = [(x+lateral[0], y+lateral[1], z) for x, y, z in points]
            parts.append(conduit('inductor_braid', points, 6.5, 'SpireBronze'))
        parts.append(conduit('energy_feed', [radial(r, angle+20, z) for r, z in
                                           ((275, -240), (430, -210), (530, -142), (600, -105))], 6, 'GlowCyan'))

        # The inward practical has a visible housing/lens, matching BuildSpireHeroLights' positions.
        at = Vector(radial(840, angle, -210))
        direction = (Vector((0, 0, -110))-at).normalized()
        housing = cylinder('projector_body', 28, 66, (0, 0, 0), 'Gunmetal', 4)
        housing.rotation_euler = direction.to_track_quat('Z', 'Y').to_euler(); housing.location = at
        parts.append(housing)
        lens = cylinder('projector_lens', 22, 3, (0, 0, 0), 'SpireLamp', 1)
        lens.rotation_euler = direction.to_track_quat('Z', 'Y').to_euler(); lens.location = at+direction*35
        parts.append(lens)

    for index in range(12):
        blade = blade_xy('iris_blade', [(90, -25), (255, -90), (300, -15), (135, 35)], -247, 12, 'Steel')
        blade.rotation_euler.z = math.radians(index*30)
        parts.append(blade)

    obj = sb_kit.join(NAME, parts)
    low = min(v.co.z for v in obj.data.vertices); high = max(v.co.z for v in obj.data.vertices)
    triangles = sb_kit.triangle_count(obj)
    if low < -280.1 or high > .1:
        raise RuntimeError(f'Hero exceeded its above-player ceiling envelope: Z {low} .. {high}')
    if triangles > 24000:
        raise RuntimeError(f'Unexpected geometry expansion: {triangles} triangles')
    OUT.mkdir(parents=True, exist_ok=True)
    line = f'{NAME}: {triangles} tris, {sb_kit.bounds(obj)}, slots '+', '.join(m.name for m in obj.data.materials)
    (OUT/'spire_hero_report.txt').write_text(line+'\n', encoding='utf-8')
    sb_kit.export_fbx(str(OUT/(NAME+'.fbx')), [obj])
    # Preserve all existing palette entries, including other scripts' extra scenery colours.
    palette_file = ROOT/'Art/Export/palette.json'
    palette = json.loads(palette_file.read_text())
    for key in ('SpireBronze', 'SpireLamp'):
        colour, metallic, roughness, emission = sb_kit.PALETTE[key]
        palette[key] = [list(colour), metallic, roughness, emission]
    palette_file.write_text(json.dumps(palette, indent=1)+'\n', encoding='utf-8')
    generated = ROOT/'Saved/Art/Generated'; generated.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(generated/'CondenserV2.blend'))
    print(line)


if __name__ == '__main__':
    build()
