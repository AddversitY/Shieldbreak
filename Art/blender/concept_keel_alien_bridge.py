"""Concept render of Keel's Red Bridge rebuilt in an original alien shape language (not game art yet).

Same footprint as the game room (Design/maps/shieldbreak_maps.py keel(): x -52..-39 m, y -20..20 m, deck 3.5 m,
ceiling 15 m, window wall at x -39 with the long window and two side doors). Ribbed arches sweep from the back wall
over the room into the window wall; the window and doors are rounded portals with glowing rims; walls are glossy
iridescent violet-teal metal with glowing vein lines; the deck is dark gloss with glowing channels.

Run: blender --background --factory-startup --python Art/blender/concept_keel_alien_bridge.py
Writes Saved/Art/Concept/keel_alien_bridge_{a,b}.png
"""
import math
import os
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/Art/Concept'
SPACE = ROOT / 'Art/Export/Textures/T_SB_KeelSpace.png'

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene


def mat_metal(name, a, b):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; n = nt.nodes; l = nt.links
    bsdf = next(x for x in n if x.type == 'BSDF_PRINCIPLED')
    fres = n.new('ShaderNodeLayerWeight'); fres.inputs['Blend'].default_value = 0.35
    mix = n.new('ShaderNodeMix'); mix.data_type = 'RGBA'
    mix.inputs[6].default_value = (*a, 1); mix.inputs[7].default_value = (*b, 1)
    l.new(fres.outputs['Facing'], mix.inputs['Factor'])
    l.new(mix.outputs[2], bsdf.inputs['Base Color'])
    bsdf.inputs['Metallic'].default_value = 0.85
    bsdf.inputs['Roughness'].default_value = 0.22
    return m


def mat_glow(name, c, strength):
    m = bpy.data.materials.new(name); m.use_nodes = True
    bsdf = next(x for x in m.node_tree.nodes if x.type == 'BSDF_PRINCIPLED')
    bsdf.inputs['Base Color'].default_value = (*c, 1)
    bsdf.inputs['Emission Color'].default_value = (*c, 1)
    bsdf.inputs['Emission Strength'].default_value = strength
    return m


def mat_space():
    m = bpy.data.materials.new('Space'); m.use_nodes = True
    nt = m.node_tree; n = nt.nodes
    for x in list(n):
        if x.type == 'BSDF_PRINCIPLED':
            n.remove(x)
    out = next(x for x in n if x.type == 'OUTPUT_MATERIAL')
    em = n.new('ShaderNodeEmission'); em.inputs['Strength'].default_value = 1.2
    if SPACE.exists():
        tex = n.new('ShaderNodeTexImage'); tex.image = bpy.data.images.load(str(SPACE))
        nt.links.new(tex.outputs['Color'], em.inputs['Color'])
    nt.links.new(em.outputs['Emission'], out.inputs['Surface'])
    return m


HULL = mat_metal('Hull', (0.16, 0.05, 0.26), (0.02, 0.32, 0.36))
RIB = mat_metal('Rib', (0.05, 0.03, 0.09), (0.10, 0.06, 0.18))
DECK = mat_metal('Deck', (0.02, 0.02, 0.035), (0.05, 0.03, 0.08))
TEAL = mat_glow('GlowTeal', (0.1, 1.0, 0.8), 9.0)
VIOLET = mat_glow('GlowViolet', (0.6, 0.25, 1.0), 7.0)
AMBER = mat_glow('GlowAmber', (1.0, 0.55, 0.15), 6.0)


def link(obj, mat):
    obj.data.materials.append(mat)
    bpy.context.scene.collection.objects.link(obj) if obj.name not in bpy.context.scene.collection.objects else None
    return obj


def sweep(name, points, radius, mat, res=24, scale=(1, 1)):
    """A tube or flattened band along a smooth curve."""
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '3D'
    cu.bevel_depth = radius; cu.bevel_resolution = 4; cu.resolution_u = res; cu.use_fill_caps = True
    sp = cu.splines.new('BEZIER'); sp.bezier_points.add(len(points) - 1)
    for k, p in zip(sp.bezier_points, points):
        k.co = p; k.handle_left_type = k.handle_right_type = 'AUTO'
    ob = bpy.data.objects.new(name, cu); bpy.context.scene.collection.objects.link(ob)
    ob.scale = (1, 1, 1)
    cu.materials.append(mat)
    return ob


def shell(name, profile_xz, y0, y1, mat, thickness=0.4):
    """Extrude a 2D profile (x, z) along y, as a solid shell (curved wall/ceiling)."""
    bm = bmesh.new()
    ring0 = [bm.verts.new((x, y0, z)) for x, z in profile_xz]
    ring1 = [bm.verts.new((x, y1, z)) for x, z in profile_xz]
    for i in range(len(profile_xz) - 1):
        bm.faces.new((ring0[i], ring0[i + 1], ring1[i + 1], ring1[i]))
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
    mod = ob.modifiers.new('solid', 'SOLIDIFY'); mod.thickness = thickness
    ob.data.materials.append(mat)
    for p in me.polygons:
        p.use_smooth = True
    return ob


# Room frame: x from -52 (back) to -39 (window wall); y from -20 to 20; deck at 3.5; vault up to 15.
X0, X1, DECKZ, TOP = -52.0, -39.0, 3.5, 15.0

# Deck: dark gloss slab with glowing channel lines and a ring inlay.
bpy.ops.mesh.primitive_cube_add(size=1, location=((X0 + X1) / 2, 0, DECKZ - 0.25))
deck = bpy.context.object; deck.scale = (X1 - X0, 40, 0.5); deck.data.materials.append(DECK)
for y in (-12, -6, 0, 6, 12):
    bpy.ops.mesh.primitive_cube_add(size=1, location=((X0 + X1) / 2, y, DECKZ + 0.01))
    s = bpy.context.object; s.scale = (X1 - X0 - 2, 0.08, 0.02); s.data.materials.append(TEAL)
bpy.ops.mesh.primitive_torus_add(major_radius=3.0, minor_radius=0.06, location=(-45.5, 0, DECKZ + 0.02))
bpy.context.object.data.materials.append(VIOLET)

# Hull vault: a single swept profile from the back wall up over the ceiling and down into the window wall, leaning
# in at the top like an egg cross-section.
prof = []
for i in range(41):
    t = i / 40.0
    ang = math.pi * t
    x = (X0 + X1) / 2 - math.cos(ang) * (X1 - X0) / 2 * (1.0 + 0.18 * math.sin(ang))
    z = DECKZ + math.sin(ang) ** 0.8 * (TOP - DECKZ)
    prof.append((x, z))
vault = shell('Vault', prof, -20, 20, HULL, 0.5)

# Ribs every 3 m along the room, following the vault, slightly proud of it; each carries a glowing vein.
for y in range(-18, 19, 3):
    pts = [Vector((x + (0.35 if x < -45.5 else -0.35), y, z - 0.25)) for x, z in prof[::4]]
    sweep(f'Rib{y}', pts, 0.32, RIB)
    vein = [Vector((p.x + (0.32 if p.x < -45.5 else -0.32), p.y, p.z)) for p in pts]
    sweep(f'Vein{y}', vein, 0.05, TEAL if y % 2 == 0 else VIOLET)

# The window: an oval portal cut through the window side of the vault, showing space, with a glowing rim.
bpy.ops.mesh.primitive_plane_add(size=1, location=(X1 + 1.2, 0, 6.6), rotation=(0, math.pi / 2, 0))
win = bpy.context.object; win.scale = (6.0, 22.0, 1); win.data.materials.append(mat_space())
rim = [Vector((X1 - 0.4, 9.0 * math.cos(a), 6.6 + 2.6 * math.sin(a))) for a in [i * math.pi / 12 for i in range(25)]]
sweep('WindowRim', rim, 0.22, RIB); sweep('WindowGlow', [p + Vector((-0.2, 0, 0)) for p in rim], 0.06, TEAL)
# Darken the vault where the window is: a dark recess ring.
# Side portals (doors) at y +-15.5: tall rounded arches with violet rims.
for y in (-15.5, 15.5):
    arch = [Vector((X1 - 0.5, y + 1.6 * math.cos(a), DECKZ + 1.8 + 2.0 * math.sin(a) if a <= math.pi else DECKZ))
            for a in [i * math.pi / 12 for i in range(13)]]
    arch = [Vector((X1 - 0.5, y + 1.6, DECKZ))] + arch + [Vector((X1 - 0.5, y - 1.6, DECKZ))]
    sweep(f'Door{y}', arch, 0.25, RIB); sweep(f'DoorGlow{y}', [p + Vector((-0.25, 0, 0)) for p in arch], 0.06, VIOLET)

# Overwatch: a sculpted cowl ledge at the back, curved front edge, amber under-light.
ledge = [(X0, 7.0), (X0 + 3.2, 7.0), (X0 + 4.0, 6.4), (X0 + 3.6, 5.6), (X0, 5.2)]
shell('Overwatch', ledge, -17, 17, RIB, 0.3)
sweep('OverwatchGlow', [Vector((X0 + 3.7, y, 5.9)) for y in range(-17, 18, 4)], 0.05, AMBER)

# Sculpted cover: tapered finned pylons on the deck.
for (x, y) in ((-47.0, -8.0), (-47.0, 8.0), (-43.0, 0.0)):
    bpy.ops.mesh.primitive_cone_add(vertices=6, radius1=0.9, radius2=0.35, depth=1.6, location=(x, y, DECKZ + 0.8))
    c = bpy.context.object; c.data.materials.append(HULL)
    bpy.ops.object.shade_smooth()
    bpy.ops.mesh.primitive_torus_add(major_radius=0.62, minor_radius=0.04, location=(x, y, DECKZ + 0.9))
    bpy.context.object.data.materials.append(TEAL)

# Glyph panels: glowing original glyphs on the back wall (simple stroke patterns).
for i, y in enumerate((-10, -4, 4, 10)):
    for k in range(4):
        a = (i * 37 + k * 71) % 180
        p0 = Vector((X0 + 0.6, y + 0.8 * math.cos(math.radians(a)), 9.5 + 0.8 * math.sin(math.radians(a))))
        p1 = Vector((X0 + 0.6, y - 0.8 * math.cos(math.radians(a + 40)), 9.5 - 0.6 * math.sin(math.radians(a + 40))))
        sweep(f'Glyph{i}{k}', [p0, (p0 + p1) / 2 + Vector((0, 0.3, 0.2)), p1], 0.04, VIOLET)

# Lighting: dark world, violet and teal area lights, a warm planet light through the window.
world = bpy.data.worlds.new('W'); scene.world = world; world.use_nodes = True
bg = next(x for x in world.node_tree.nodes if x.type == 'BACKGROUND'); bg.inputs['Color'].default_value = (0.004, 0.003, 0.008, 1)
for loc, col, e in (((-45.5, -8, 13), (0.5, 0.3, 1.0), 2500), ((-45.5, 8, 13), (0.2, 0.9, 1.0), 2500),
                    ((-37, 0, 7), (1.0, 0.75, 0.5), 4000), ((-50, 0, 6), (0.6, 0.3, 1.0), 1200)):
    bpy.ops.object.light_add(type='AREA', location=loc)
    L = bpy.context.object; L.data.energy = e; L.data.color = col; L.data.size = 6
    L.rotation_euler = (Vector((-45.5, 0, DECKZ)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()

try:
    scene.render.engine = 'BLENDER_EEVEE_NEXT'
except TypeError:
    scene.render.engine = 'BLENDER_EEVEE'
eevee = scene.eevee
for attr, val in (('use_bloom', True), ('use_gtao', True), ('use_raytracing', True)):
    if hasattr(eevee, attr):
        setattr(eevee, attr, val)
scene.render.resolution_x, scene.render.resolution_y = 1600, 900
scene.view_settings.view_transform = 'AgX' if 'AgX' in [i.identifier for i in bpy.types.ColorManagedViewSettings.bl_rna.properties['view_transform'].enum_items] else 'Filmic'
scene.view_settings.look = 'None'
if 'AgX' in scene.view_settings.view_transform:
    try:
        scene.view_settings.look = 'AgX - Punchy'
    except TypeError:
        pass

OUT.mkdir(parents=True, exist_ok=True)
for tag, loc, target in (('a', (-50.5, -14, 5.4), (-40, 2, 6.5)), ('b', (-41.5, 12, 5.2), (-50, -4, 8))):
    bpy.ops.object.camera_add(location=loc)
    cam = bpy.context.object; cam.data.lens = 18
    cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam
    scene.render.filepath = str(OUT / f'keel_alien_bridge_{tag}.png')
    bpy.ops.render.render(write_still=True)
print('rendered', OUT)
