"""Keel's original alien kit for the bridges (first room: the Red Bridge), authored in Blender centimetres.

Visual only, laid over the unchanged gameplay blocks. Pivot of every piece: the bridge's window-wall line (world
x = -3900 on the red side, y = 0, z = 0); local -X runs back into the room (back wall at local x -1300). The blue
bridge uses the same pieces at x = +3900 turned 180 degrees.

  SM_SB_Kit_AlienRib     one vault rib, 45 cm thick, rising from the overwatch (z 700) up the back wall, curving into
                         the ceiling (under the 55 cm ceiling tiles) and down the window wall to just over the window
                         (z 840), with a glowing vein on its inner face. 25 cm wide along Y; placed every 3 m.
  SM_SB_Kit_AlienCove    the curved coves filling both wall/ceiling corners between two ribs (2.75 m along Y).
  SM_SB_Kit_AlienBridge  the one-off pieces: an oval portal rim round the long window, arched rims round both side
                         doors, a rounded cowl lip on the overwatch front (with an amber under-strip), and glowing
                         channels and a ring inlay on the deck.

Run: Tools/BuildKeelAlien.ps1 (portable Blender), then Tools/ImportKeelAlien.bat.
"""
import json
import math
import os
from pathlib import Path
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit
import build_map_kit as kit
import bmesh
import bpy

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Art/Export/MapKit'
NEW_PALETTE = {
    'AlienHull': ((0.08, 0.03, 0.14), 0.85, 0.22, 0.0),
    'AlienTeal': ((0.10, 1.0, 0.80), 0.0, 0.3, 8.0),
    'AlienViolet': ((0.60, 0.25, 1.0), 0.0, 0.3, 7.0),
}
sb_kit.PALETTE.update(NEW_PALETTE)

BACK, WIN, CEIL, THICK, R = -1300.0, -30.0, 1445.0, 45.0, 300.0
BOTTOM_BACK, BOTTOM_WIN = 700.0, 840.0


def arc(cx, cz, r, a0, a1, n=10):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cz + r * math.sin(math.radians(a0 + (a1 - a0) * i / n)))
            for i in range(n + 1)]


def vault_path(inset):
    """The rib's path in XZ, inset from the walls and ceiling by `inset` cm."""
    b, w, c, r = BACK + inset, WIN - inset, CEIL - inset, R - inset
    pts = [(b, BOTTOM_BACK)]
    pts += arc(BACK + R, CEIL - R, r, 180, 90)
    pts += arc(WIN - R, CEIL - R, r, 90, 0)
    pts += [(w, BOTTOM_WIN)]
    return pts


def band(name, outer, inner, y0, y1, mat):
    return kit.prism_xz(name, outer + list(reversed(inner)), y0, y1, mat, bevel=1.5)


def rr_loop(cy, cz, w, h, r, n=8):
    """Rounded rectangle in the YZ plane, centred at (cy, cz)."""
    hw, hh, r = w / 2, h / 2, min(r, w / 2, h / 2)
    pts = []
    for (sy, sz, a0) in ((1, 1, 0), (-1, 1, 90), (-1, -1, 180), (1, -1, 270)):
        ccy, ccz = cy + sy * (hw - r), cz + sz * (hh - r)
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n)
            pts.append((ccy + r * math.cos(a), ccz + r * math.sin(a)))
    return pts


def ring_yz(name, inner, outer, x0, x1, mat):
    """A flat frame between two closed YZ loops (same point count), extruded along X from x0 to x1."""
    bm = bmesh.new()
    def layer(x):
        return [bm.verts.new((x, y, z)) for y, z in inner], [bm.verts.new((x, y, z)) for y, z in outer]
    fi, fo = layer(x0)
    bi, bo = layer(x1)
    n = len(inner)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((fi[i], fi[j], fo[j], fo[i]))
        bm.faces.new((bo[i], bo[j], bi[j], bi[i]))
        bm.faces.new((fo[i], fo[j], bo[j], bo[i]))
        bm.faces.new((bi[i], bi[j], fi[j], fi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = sb_kit._link(name, me)
    ob.data.materials.append(sb_kit.material(mat))
    for p in me.polygons:
        p.use_smooth = True
    return ob


def build_rib():
    sb_kit.reset_scene()
    parts = [band('rib', vault_path(0), vault_path(THICK), -12.5, 12.5, 'AlienHull'),
             band('vein', vault_path(THICK), vault_path(THICK + 3), -3, 3, 'AlienTeal')]
    return parts


def build_cove():
    sb_kit.reset_scene()
    parts = []
    for cx, a0, a1, corner in ((BACK + R, 180, 90, (BACK, CEIL)), (WIN - R, 90, 0, (WIN, CEIL))):
        curve = arc(cx, CEIL - R, R, a0, a1, 12)
        parts.append(kit.prism_xz('cove', curve + [corner], -137.5, 137.5, 'AlienHull', bevel=0))
    return parts


def build_bridge():
    sb_kit.reset_scene()
    parts = []
    # Window: inner 1800 x 360 centred z 640; rim 50 wide, 25 deep, and a glowing inner lip.
    inner = rr_loop(0, 640, 1810, 370, 150, 10)
    outer = rr_loop(0, 640, 1910, 470, 200, 10)
    parts.append(ring_yz('window_rim', inner, outer, WIN, WIN - 25, 'AlienHull'))
    parts.append(ring_yz('window_glow', inner, rr_loop(0, 640, 1830, 390, 160, 10), WIN - 25, WIN - 28, 'AlienTeal'))
    # Doors at y +-1550: inner 510 x 355 from the deck (z 350), arched top.
    for cy in (-1550, 1550):
        inner = rr_loop(cy, 350 + 177.5, 510, 355, 170, 10)
        outer = rr_loop(cy, 350 + 177.5 + 25, 590, 405, 200, 10)
        parts.append(ring_yz('door_rim', inner, outer, WIN, WIN - 25, 'AlienHull'))
        parts.append(ring_yz('door_glow', inner, rr_loop(cy, 350 + 177.5 + 4, 530, 363, 175, 10), WIN - 25, WIN - 28, 'AlienViolet'))
    # Overwatch cowl lip (front face at local x -900), centre span only, so the ramps' tops stay clear.
    lip = [(-900, 700), (-882, 696), (-872, 680), (-874, 640), (-886, 612), (-900, 604)]
    parts.append(kit.prism_xz('cowl', lip, -1150, 1150, 'AlienHull', bevel=0))
    parts.append(sb_kit.box('cowl_glow', (-884, 0, 602), (10, 2300, 3), 'HullAmber', bevel=0))
    # Deck channels and a ring inlay (just over the 3.5 m deck tiles).
    for x in (-760, -150):
        parts.append(sb_kit.box('channel', (x, 0, 351.5), (6, 3700, 2), 'AlienTeal', bevel=0))
    ring = sb_kit.ring('inlay', (-450, 0, 351.5), 260, 3, 'AlienViolet', axis='Z', verts=48, tube_verts=6)
    parts.append(ring)
    return parts


def main():
    lines = []
    for name, build in (('SM_SB_Kit_AlienRib', build_rib), ('SM_SB_Kit_AlienCove', build_cove),
                        ('SM_SB_Kit_AlienBridge', build_bridge)):
        obj = sb_kit.join(name, build())
        line = f'{name}: {sb_kit.triangle_count(obj)} tris, {sb_kit.bounds(obj)}, slots ' + ', '.join(m.name for m in obj.data.materials)
        sb_kit.export_fbx(str(OUT / (name + '.fbx')), [obj])
        lines.append(line)
        print(line)
    (OUT / 'keel_alien_report.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    palette_file = ROOT / 'Art/Export/palette.json'
    palette = json.loads(palette_file.read_text())
    for key, (colour, metallic, roughness, emission) in NEW_PALETTE.items():
        palette[key] = [list(colour), metallic, roughness, emission]
    palette_file.write_text(json.dumps(palette, indent=1) + '\n', encoding='utf-8')


main()
