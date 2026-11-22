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
    'AlienSeam': ((0.45, 0.15, 0.85), 0.0, 0.4, 1.4),
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


# Low cover on Keel (W x D x H): crates, consoles, coolant blocks and hull debris, each authored at its exact size.
PLINTHS = [(200, 150, 110), (200, 300, 110), (200, 300, 130), (200, 400, 130), (300, 100, 130), (300, 300, 130), (400, 200, 130)]


def plinth(W, D, H):
    """Sculpted alien cover (option B): a cast block whose sides lean out to a dark foot 12 cm beyond the gameplay
    block, a proud dark band at half height, and a top bevelled 8 cm in, so the walkable top stays at the block's
    height. Pivot: the block's bottom centre."""
    sb_kit.reset_scene()
    hw, hd = W / 2.0, D / 2.0

    def at(o, c):
        return ring(hw + o, hd + o, max(4.0, min(c, hw + o - 2.0, hd + o - 2.0)))

    mid = H * 0.5
    parts = [
        loft('foot', [(0.0, at(12, 22)), (16.0, at(12, 22)), (22.0, at(6, 18))], 'AlienDark'),
        loft('body', [(22.0, at(6, 18)), (H - 14.0, at(1, 14)), (H, at(-8, 16))], 'AlienHull'),
        loft('band', [(mid - 9.0, at(6, 18)), (mid + 9.0, at(5, 17))], 'AlienDark'),
    ]
    # A dark recessed top plate.
    parts.append(loft('top', [(H - 0.5, at(-14, 12)), (H + 0.6, at(-14, 12))], 'AlienDark'))
    return parts


def alien_pod():
    """The Cradle's hung pod as alien hull (option B): the 14 x 8 x 2.5 m gameplay block wrapped in a faceted shell
    that narrows to a keel underneath, swells 10 cm past the block at its waist and closes flush with the walkable top;
    a dark waist band, dark ribs down the long sides with rose plates between them, and a dark belly plate. The ends
    stay within the block so the crane-arm ramps meet it cleanly. Pivot: the block's bottom centre (z 400 in Keel)."""
    sb_kit.reset_scene()
    hw, hd, H = 700.0, 400.0, 250.0
    shell = [(0.0, ring(hw - 50, hd - 90, 120)), (30.0, ring(hw - 10, hd - 30, 120)), (120.0, ring(hw + 10, hd + 10, 100)),
             (240.0, ring(hw + 6, hd + 6, 90)), (H, ring(hw, hd, 80))]
    parts = [loft('shell', shell, 'AlienHull')]
    parts.append(loft('waist', [(112.0, ring(hw + 22, hd + 22, 104)), (134.0, ring(hw + 22, hd + 22, 104))], 'AlienDark'))
    parts.append(loft('belly', [(-2.0, ring(hw - 130, hd - 160, 90)), (0.5, ring(hw - 130, hd - 160, 90))], 'AlienDark'))
    for side in (-1, 1):
        for i in range(-3, 4):
            x = i * 175.0
            parts.append(sb_kit.box('rib', (x, side * (hd + 14), 185), (16, 12, 100), 'AlienDark', bevel=1.5))
            if i < 3:
                parts.append(sb_kit.box('plate', (x + 87.5, side * (hd + 11), 190), (120, 4, 66), 'AlienRose', bevel=2))
    # Four hangers up to the ceiling (11 m above the pivot): faceted struts clamped to the long sides, so the pod reads
    # as hung, not floating. Outside the walkable top.
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * 520.0, sy * (hd + 34.0)
            def sq(r, c, x=x, y=y):
                return [(px + x, py + y) for px, py in ring(r, r, c)]
            parts.append(loft('clamp', [(120.0, sq(26, 10)), (230.0, sq(26, 10)), (262.0, sq(17, 6))], 'AlienDark'))
            parts.append(loft('hanger', [(262.0, sq(15, 5)), (1060.0, sq(13, 4)), (1100.0, sq(30, 10))], 'AlienHull'))
    return parts


def ramp_edge():
    """The ramps' edge fascia (option B): a 1 m run (scaled along each ramp) of heavy hull lining the ramp's side, with a
    9 cm curb over the walking surface's edge and a faceted face down 62 cm below it, a dark band along it. Pivot: the
    walking surface's edge line (y 0), the ramp's inside toward -Y, outside +Y."""
    sb_kit.reset_scene()
    body = [(-6.0, 9.0), (-6.0, -62.0), (10.0, -62.0), (19.0, -30.0), (16.0, 0.0), (10.0, 9.0)]
    parts = [extrude_x('edge', body, -50.0, 50.0, 'AlienHull')]
    parts.append(extrude_x('band', [(17.5, -26.0), (19.5, -38.0), (21.5, -38.0), (19.5, -26.0)], -50.0, 50.0, 'AlienDark'))
    return parts


def alien_pylon():
    """The Galleries' reactor pylons as sculpted alien obelisks (option B, replacing the light-column pylons): a dark
    flared foot, a faceted shaft tapering from 124 to 114 cm half width, a flared crown with a dark cap, and one thin
    dim violet slot set into each face (no light beam). Pivot: the 2 x 2 x 6 m block's bottom centre."""
    sb_kit.reset_scene()
    H = 600.0
    # The shaft must stay outside the 2 x 2 m gameplay block, corners included (cut line 2w - ch >= 200).
    w0, w1, z0, z1 = 124.0, 114.0, 60.0, 520.0
    parts = [
        loft('foot', [(0.0, ring(146, 146, 50)), (30.0, ring(146, 146, 50)), (60.0, ring(w0, w0, 40))], 'AlienDark'),
        loft('shaft', [(60.0, ring(w0, w0, 40)), (z1, ring(w1, w1, 26))], 'AlienHull'),
        loft('crown', [(z1, ring(w1, w1, 26)), (588.0, ring(140, 140, 56)), (H, ring(124, 124, 44))], 'AlienHull'),
        loft('cap', [(H - 0.5, ring(106, 106, 12)), (H + 1.0, ring(106, 106, 12))], 'AlienDark'),
        loft('band', [(150.0, ring(128, 128, 44)), (168.0, ring(127, 127, 43))], 'AlienDark'),
    ]

    def half(z):
        return w0 + (w1 - w0) * (z - z0) / (z1 - z0)

    za, zb = 190.0, 470.0
    for k in range(4):
        def turn(x, y, z, k=k):
            return [(x, y, z), (-y, x, z), (-x, -y, z), (y, -x, z)][k]
        a = [turn(-7.0, half(za) + 0.6, za), turn(-7.0, half(zb) + 0.6, zb)]
        b = [turn(7.0, half(za) + 0.6, za), turn(7.0, half(zb) + 0.6, zb)]
        n = turn(0.0, 1.0, 0.0)
        parts.append(strip('slot', a, b, 'AlienSeam', lambda p, n=n: n))
    return parts


def vault_plate():
    """Leaning upper wall (option B, Midship's trapezoid hull section): a 4 m wide hull plate (scaled to each wall cell)
    that leans from the wall 5 m under the ceiling up and 1.5 m into the room where it meets the ceiling, 20 cm thick,
    with a heavy dark lip along its foot and a raised spine up its middle. Pivot: the wall's face at the plate's foot
    (y 0 the wall, room +Y, z 0 the foot, z 500 the ceiling)."""
    sb_kit.reset_scene()
    # Plate (YZ profile): its room-side face leans from (22, 0) to (165, 476); the wedge behind it is closed to the wall.
    plate = [(0.0, -24.0), (22.0, -24.0), (22.0, 0.0), (165.0, 476.0), (165.0, 500.0), (0.0, 500.0)]
    parts = [extrude_x('plate', plate, -200.0, 200.0, 'AlienHull')]
    parts.append(extrude_x('lip', [(0.0, -46.0), (34.0, -46.0), (40.0, -20.0), (26.0, 4.0), (0.0, 4.0)], -200.0, 200.0, 'AlienDark'))
    # Spine: a raised rib up the plate's face, following the lean (0.3 cm out per cm up).
    parts.append(loft('spine', [(z, [(px, py) for px, py in [(-9.0, 22.0 + 0.3 * z), (9.0, 22.0 + 0.3 * z), (9.0, 34.0 + 0.3 * z), (-9.0, 34.0 + 0.3 * z)]])
                                for z in (12.0, 486.0)], 'AlienDark'))
    return parts


def ceiling_coffer():
    """A ceiling coffer (option B): the 6 x 4 m bay between two ceiling ribs as a dark recessed panel inside a heavy
    faceted hull frame hanging 16 cm, so the ceiling reads as deep structure, not a flat skin. Pivot: the ceiling (top
    centre, z 0); the bay runs along X between ribs, Y along the rib."""
    sb_kit.reset_scene()
    outer, inner = ring(250, 190, 44), ring(218, 158, 32)
    bm = bmesh.new()
    def layer(loop, z):
        return [bm.verts.new((x, y, z)) for x, y in loop]
    ot, it, ob, ib = layer(outer, 0.0), layer(inner, 0.0), layer(outer, -12.0), layer(inner, -16.0)
    n = len(outer)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((ob[i], ob[j], ib[j], ib[i]))      # sloped underside of the frame
        bm.faces.new((ot[i], ot[j], ob[j], ob[i]))      # outer face
        bm.faces.new((ib[i], ib[j], it[j], it[i]))      # inner face
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new('frame'); bm.to_mesh(me); bm.free()
    parts = [sb_kit._finish(sb_kit._link('frame', me), 'AlienHull', 0.0)]
    parts.append(loft('panel', [(-3.0, ring(220, 160, 33)), (-1.0, ring(220, 160, 33))], 'AlienDark'))
    return parts


def build_one(name, parts, lines):
    obj = sb_kit.join(name, parts)
    line = f'{name}: {sb_kit.triangle_count(obj)} tris, {sb_kit.bounds(obj)}, slots ' + ', '.join(m.name for m in obj.data.materials)
    sb_kit.export_fbx(str(OUT / (name + '.fbx')), [obj])
    lines.append(line)
    print(line)


def main():
    lines = []
    build_one('SM_SB_Kit_AlienPod', alien_pod(), lines)
    build_one('SM_SB_Kit_AlienPylon', alien_pylon(), lines)
    build_one('SM_SB_Kit_AlienVaultPlate', vault_plate(), lines)
    build_one('SM_SB_Kit_AlienCoffer', ceiling_coffer(), lines)
    build_one('SM_SB_Kit_AlienRampEdge', ramp_edge(), lines)
    for W, D, H in PLINTHS:
        build_one('SM_SB_Kit_AlienPlinth_%dx%dx%d' % (W, D, H), plinth(W, D, H), lines)
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
