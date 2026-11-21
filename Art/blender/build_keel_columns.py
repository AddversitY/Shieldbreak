"""Keel's sculpted alien column shrouds (option B, 2026-11-21; reference: Vincent's Midship video).

Midship's structure reads as thick, faceted, sloped hull, not neon: so each floor-to-ceiling pillar on Keel gets a
hand-modelled shroud authored at that pillar's exact size. Visual only (the gameplay block is unchanged inside it).

  - a chamfered (octagonal) shaft 25 cm outside the block's faces, so the block's corners stay hidden;
  - a sloped skirt at the foot (35 cm out at the floor) and a heavy flared capital into the ceiling (70 cm out);
  - dark bands where skirt and capital meet the shaft, and two raised vertical fins on every face;
  - a recessed rose inset panel on the two long faces at chest-to-head height.

Pivot: the block's bottom centre. +X is the block's X size (W), +Y its Y size (D). One mesh per pillar size:
SM_SB_Kit_AlienColumn_<W>x<D>x<H> (the four sizes Keel's layout uses, mirrored to both sides).

Run: portable Blender, `blender --background --factory-startup --python Art/blender/build_keel_columns.py`;
import with `-SBMapArtPrefix=SM_SB_Kit_AlienColumn` (Tools/ImportKeelColumns.bat).
"""
import json
import os
from pathlib import Path
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit
import bmesh
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Art/Export/MapKit'
PALETTE = {
    'AlienHull': ((0.08, 0.03, 0.14), 0.85, 0.22, 0.0),
    'AlienDark': ((0.03, 0.02, 0.04), 0.6, 0.3, 0.0),
    'AlienRose': ((0.22, 0.08, 0.15), 0.8, 0.28, 0.0),
}
sb_kit.PALETTE.update(PALETTE)

# (W, D, H): crane masts on the approach decks, the Breach's fallen frame and torn hull frame, the Galleries' coolant stack.
SIZES = [(400, 500, 1150), (400, 300, 1500), (400, 200, 1500), (300, 300, 1500)]


def ring(hw, hd, ch):
    """Footprint loop: a rectangle of half sizes hw x hd with its corners cut by ch (counter-clockwise from +X)."""
    return [(hw, -hd + ch), (hw, hd - ch), (hw - ch, hd), (-hw + ch, hd), (-hw, hd - ch), (-hw, -hd + ch), (-hw + ch, -hd), (hw - ch, -hd)]


def loft(name, rings, mat):
    """A closed solid through footprint loops [(z, loop)], bottom to top, all loops with the same point count."""
    bm = bmesh.new()
    layers = [[bm.verts.new((x, y, z)) for x, y in loop] for z, loop in rings]
    n = len(layers[0])
    bm.faces.new(list(reversed(layers[0])))
    bm.faces.new(layers[-1])
    for a, b in zip(layers, layers[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((a[i], a[j], b[j], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return sb_kit._finish(sb_kit._link(name, me), mat, 0.0)


def column(W, D, H):
    sb_kit.reset_scene()
    hw, hd = W / 2.0, D / 2.0
    out, ch = 25.0, 40.0   # shaft offset outside the block; corner chamfer (its cut line, ch + 0.4 out, must stay <= 2 x out
    #                        so the block's corner stays inside)

    def at(o, c=None):
        return ring(hw + o, hd + o, ch + o * 0.4 if c is None else c)

    cap0 = H - 230.0
    parts = [
        # Skirt: 35 cm out at the floor, sloping in to the shaft by 70 cm up.
        loft('skirt', [(0.0, at(35)), (24.0, at(35)), (70.0, at(out))], 'AlienDark'),
        # Shaft, then the capital flaring out into the ceiling.
        # A slight taper (6 cm wider at the foot) so the shaft reads as cast hull, not a box.
        loft('shaft', [(70.0, at(out + 6)), (cap0, at(out)), (H - 70.0, at(62)), (H, at(70))], 'AlienHull'),
        # A dark joint band a little above head height splits the shaft into two cast sections.
        loft('band_mid', [(cap0 * 0.5 + 60.0, at(out + 5)), (cap0 * 0.5 + 78.0, at(out + 5))], 'AlienDark'),
        # Dark bands at the skirt and under the capital.
        loft('band_lo', [(70.0, at(out + 10)), (96.0, at(out + 9))], 'AlienDark'),
        loft('band_hi', [(cap0 - 34.0, at(out + 4)), (cap0, at(out + 4))], 'AlienDark'),
    ]
    # Two raised fins on each face (8 cm proud of the shaft, 14 cm wide), between the bands.
    z0, z1 = 96.0, cap0 - 34.0
    for axis, half, span in (('x', hw, hd), ('y', hd, hw)):
        for side in (-1, 1):
            for t in (-0.42, 0.42):
                c = t * (span - ch)
                if axis == 'x':
                    centre, size = (side * (half + out + 4), c, (z0 + z1) / 2), (8, 14, z1 - z0)
                else:
                    centre, size = (c, side * (half + out + 4), (z0 + z1) / 2), (14, 8, z1 - z0)
                parts.append(sb_kit.box('fin', centre, size, 'AlienHull', bevel=1.5))
    # Rose inset panels on the two faces along the longer side, 1.2-2.3 m up, between the fins.
    for side in (-1, 1):
        if W >= D:
            span = 0.84 * (hw - ch)
            parts.append(sb_kit.box('inset', (0, side * (hd + out + 1), 175), (span * 2 * 0.62, 4, 110), 'AlienRose', bevel=2))
        else:
            span = 0.84 * (hd - ch)
            parts.append(sb_kit.box('inset', (side * (hw + out + 1), 0, 175), (4, span * 2 * 0.62, 110), 'AlienRose', bevel=2))
    return parts


# Door openings (W x H) in Keel's 60 cm walls: the 5 x 3.5 m deck doors and the 20 x 4.5 m trench mouths.
PORTALS = [(500, 350), (2000, 450)]
WALL_T = 60.0


def strip(name, a, b, mat, toward):
    """Quads between two open polylines a and b of 3D points (same count), one-sided: each face is turned so its normal
    points along toward(face centre) (the side the player sees)."""
    bm = bmesh.new()
    va = [bm.verts.new(p) for p in a]
    vb = [bm.verts.new(p) for p in b]
    for i in range(len(a) - 1):
        f = bm.faces.new((va[i], va[i + 1], vb[i + 1], vb[i]))
        f.normal_update()
        want = Vector(toward(f.calc_center_median()))
        if f.normal.dot(want) < 0.0:
            f.normal_flip()
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return sb_kit._finish(sb_kit._link(name, me), mat, 0.0)


def portal(W, H):
    """Sculpted alien portal round a W x H door in a 60 cm wall (option B): on both faces a heavy angular surround,
    90 cm wide, sloping from 58 cm proud at the opening to 30 cm (just over the wall's 27 cm panels), then a lip to the wall, its top corners cut at 45 degrees; a dark
    reveal lining the opening's sides and lintel; a rose keystone over the door. Pivot: the opening's bottom centre on
    the wall's centre line; the opening runs along X, the wall's faces look along +-Y. Visual only, and nothing
    narrows the opening by more than 1 cm (the lintel corners' 10 cm chamfers sit above head height)."""
    sb_kit.reset_scene()
    hw, t = W / 2.0, WALL_T / 2.0
    c = 10.0
    inner = [(-hw, 0.0), (-hw, H - c), (-hw + c, H), (hw - c, H), (hw, H - c), (hw, 0.0)]
    o, oc = 90.0, 70.0
    outer = [(-hw - o, 0.0), (-hw - o, H + o - oc), (-hw - o + oc, H + o), (hw + o - oc, H + o), (hw + o, H + o - oc), (hw + o, 0.0)]
    parts = []
    for side in (-1.0, 1.0):
        # The wall's alien panels stand 27 cm proud, so the surround starts above them (30) and rises to 58 at the opening.
        y_in, y_out = side * (t + 58.0), side * (t + 30.0)
        # The sloped surround, its outer lip down to the wall, and its inner face back to the wall's face.
        away = lambda p: (p.x, 0.0, p.z - H * 0.5)         # out from the opening's middle
        into = lambda p: (-p.x, 0.0, H * 0.5 - p.z)        # in toward it
        parts.append(strip('surround', [(x, y_in, z) for x, z in inner], [(x, y_out, z) for x, z in outer], 'AlienHull',
                           lambda p, s=side: (0.0, s, 0.0)))
        parts.append(strip('lip', [(x, y_out, z) for x, z in outer], [(x, side * t, z) for x, z in outer], 'AlienDark', away))
        parts.append(strip('cheek', [(x, side * t, z) for x, z in inner], [(x, y_in, z) for x, z in inner], 'AlienDark', into))
        # Rose keystone: a faceted block over the lintel.
        parts.append(sb_kit.box('keystone', (0.0, side * (t + 46.0), H + 46.0), (min(140.0, W * 0.3), 30.0, 56.0), 'AlienRose',
                                bevel=6.0, taper=(0.7, 1.0) if side > 0 else None))
    # Reveal: the opening's sides and lintel, 1 cm inside the opening, through the wall.
    rin = [(x - (1.0 if x > 0 else -1.0), z - (1.0 if z >= H - c else 0.0)) for x, z in inner]
    parts.append(strip('reveal', [(x, t, z) for x, z in rin], [(x, -t, z) for x, z in rin], 'AlienDark',
                       lambda p: (-p.x, 0.0, H * 0.5 - p.z)))
    for obj in parts:
        for poly in obj.data.polygons:
            poly.use_smooth = False
    return parts


# Space windows (height, bay width): the Breach's long window (6.2 m tall bays) and the bridges' end-wall windows (5.8 m).
WINDOW_HEIGHTS = [620, 580]


def extrude_x(name, points_yz, x0, x1, mat):
    """A closed YZ profile (counter-clockwise seen from +X) extruded along X from x0 to x1."""
    rings = []
    bm = bmesh.new()
    a = [bm.verts.new((x0, y, z)) for y, z in points_yz]
    b = [bm.verts.new((x1, y, z)) for y, z in points_yz]
    bm.faces.new(list(reversed(a)))
    bm.faces.new(b)
    n = len(points_yz)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((a[i], a[j], b[j], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return sb_kit._finish(sb_kit._link(name, me), mat, 0.0)


def window_mullion(H):
    """One mullion of a space window, option B: a faceted fin standing in front of the glass (its plane is y 0, the
    room toward -Y), 60 cm wide at the glass narrowing to a 22 cm blade 48 cm into the room, flaring wider and deeper
    where it meets the sill and head. Pivot: the window's middle, at the glass."""
    sb_kit.reset_scene()

    def fin(w0, w1, d):
        return [(-w0 / 2, 0.0), (-w1 / 2 - 6, -d * 0.55), (-w1 / 2, -d), (w1 / 2, -d), (w1 / 2 + 6, -d * 0.55), (w0 / 2, 0.0)]

    h = H / 2.0
    rings = [(-h - 60, fin(90, 40, 62)), (-h + 30, fin(60, 22, 48)), (h - 30, fin(60, 22, 48)), (h + 60, fin(90, 40, 62))]
    parts = [loft('mullion', rings, 'AlienHull')]
    # A dark spine groove down the blade.
    parts.append(sb_kit.box('spine', (0, -49, 0), (6, 3, H - 120), 'AlienDark', bevel=0))
    return parts


def window_sill():
    """The window's sill and head, option B: an 8 m run (scaled to the window's bays) of heavy angled hull that
    frames the glass top and bottom. Built as the bottom sill; the head is the same piece turned upside down.
    Pivot: the glass's bottom edge at its middle (y 0 glass, room -Y)."""
    sb_kit.reset_scene()
    profile = [(0.0, 12.0), (0.0, -70.0), (-36.0, -60.0), (-58.0, -18.0), (-46.0, 6.0), (-20.0, 12.0)]
    parts = [extrude_x('sill', profile, -400.0, 400.0, 'AlienHull')]
    parts.append(extrude_x('lip', [(-46.0, 6.0), (-58.0, -18.0), (-61.0, -17.0), (-49.0, 8.0)], -400.0, 400.0, 'AlienDark'))
    return parts


def build_one(name, parts, lines):
    obj = sb_kit.join(name, parts)
    line = f'{name}: {sb_kit.triangle_count(obj)} tris, {sb_kit.bounds(obj)}, slots ' + ', '.join(m.name for m in obj.data.materials)
    sb_kit.export_fbx(str(OUT / (name + '.fbx')), [obj])
    lines.append(line)
    print(line)


def main():
    lines = []
    for H in WINDOW_HEIGHTS:
        build_one('SM_SB_Kit_AlienWindowMullion_%d' % H, window_mullion(H), lines)
    build_one('SM_SB_Kit_AlienWindowSill', window_sill(), lines)
    for W, H in PORTALS:
        build_one('SM_SB_Kit_AlienPortal_%dx%d' % (W, H), portal(W, H), lines)
    for W, D, H in SIZES:
        name = 'SM_SB_Kit_AlienColumn_%dx%dx%d' % (W, D, H)
        obj = sb_kit.join(name, column(W, D, H))
        line = f'{name}: {sb_kit.triangle_count(obj)} tris, {sb_kit.bounds(obj)}, slots ' + ', '.join(m.name for m in obj.data.materials)
        sb_kit.export_fbx(str(OUT / (name + '.fbx')), [obj])
        lines.append(line)
        print(line)
    (OUT / 'keel_columns_report.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    palette_file = ROOT / 'Art/Export/palette.json'
    palette = json.loads(palette_file.read_text())
    for key, (colour, metallic, roughness, emission) in PALETTE.items():
        palette.setdefault(key, [list(colour), metallic, roughness, emission])
    palette_file.write_text(json.dumps(palette, indent=1) + '\n', encoding='utf-8')


main()
