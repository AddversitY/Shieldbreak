"""Keel's hero set: the Cradle's cargo pod on its crane yoke, authored in Blender centimetres.

The pod wraps the gameplay block of Design/maps/shieldbreak_maps.py keel() (x -700..700, y -400..400, z 400..650;
visual only, the block keeps the collision). Pivot at the pod's bottom centre, so the game places it at (0, 0, 400).
Its top deck sits 3 cm over the block top; ribs, rails and lugs stay on the sides, never on the walkable top.
Four cables rise from the corner lugs to a two-beam yoke hung under the 15 m ceiling (local z 1100).

Run: Tools/BuildKeelHero.ps1 (portable Blender, headless), then Tools/ImportKeelHero.bat.
Writes Art/Export/MapKit/SM_SB_Kit_CradlePod.fbx, keel_hero_report.txt, and the HullHazard palette entry.
"""
import json
import os
from pathlib import Path
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit
import build_map_kit as kit
import bpy

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Art/Export/MapKit'
NAME = 'SM_SB_Kit_CradlePod'
HX, HY, H = 720.0, 420.0, 253.0       # the shell stands 2 cm outside the block's sides and 3 cm over its top (clear of its cladding)
CEILING = 1100.0                       # the ceiling above the pod, in pod space (1500 - 400)

# Worn hazard-orange paint for the pod's end bands (the game swaps in MI_SB_KeelHazard on Keel).
sb_kit.PALETTE['HullHazard'] = ((0.40, 0.15, 0.03), 0.15, 0.6, 0.0)


def stripes_on_end(sign, parts):
    """A hazard band across one end: alternating orange and dark parallelograms, 4 cm proud of the end face."""
    x0, x1 = (HX, HX + 4.0) if sign > 0 else (-HX - 4.0, -HX)
    z0, z1, width, lean = 40.0, 120.0, 60.0, 50.0
    y = -HY + 30.0
    index = 0
    while y < HY - 30.0 - 1.0:
        y_next = min(y + width, HY - 30.0)
        points = [(y, z0), (y_next, z0), (min(y_next + lean, HY - 30.0), z1), (min(y + lean, HY - 30.0), z1)]
        if points[2][0] - points[3][0] > 1.0:
            bar = sb_kit.profile('hazard_bar', points, x1 - x0, 'HullHazard' if index % 2 == 0 else 'HullFrame',
                                 x=(x0 + x1) / 2.0, bevel=0.0)
            parts.append(bar)
        y, index = y_next, index + 1


def build():
    sb_kit.reset_scene()
    parts = []
    # Body: dark frame shell with inset plate panels, and the tread deck on top.
    parts.append(sb_kit.box('core', (0, 0, H / 2.0 - 6.0), (2 * HX - 40.0, 2 * HY - 40.0, H - 12.0), 'HullFrame', bevel=3))
    parts.append(sb_kit.box('deck', (0, 0, H - 6.0), (2 * HX, 2 * HY, 12.0), 'HullDeck', bevel=2, segments=2))
    parts.append(sb_kit.box('keel_plate', (0, 0, 6.0), (2 * HX - 10.0, 2 * HY - 10.0, 12.0), 'HullFrame', bevel=3, segments=2))
    # Side panels between ribs, slightly inset from the rails: the plates the eye reads from the bridges.
    rib_x = [-HX + 20.0 + i * (2 * HX - 40.0) / 10.0 for i in range(11)]
    for side in (-1, 1):
        y_face = side * (HY - 14.0)
        for a, b in zip(rib_x[:-1], rib_x[1:]):
            parts.append(sb_kit.box('side_panel', ((a + b) / 2.0, y_face, H / 2.0), (b - a - 22.0, 16.0, H - 70.0), 'HullPlate', bevel=2, segments=2))
        for x in rib_x:
            parts.append(sb_kit.box('rib', (x, side * (HY - 6.0), H / 2.0), (18.0, 16.0, H - 24.0), 'HullFrame', bevel=3, segments=2))
        # Rails along the top and bottom edges.
        for z in (14.0, H - 22.0):
            parts.append(sb_kit.box('rail', (0, side * (HY - 8.0), z), (2 * HX, 16.0, 20.0), 'HullFrame', bevel=3, segments=2))
        # Under-glow: a cold light strip along each bottom edge, shining down into the trench.
        parts.append(sb_kit.box('underglow', (0, side * (HY - 40.0), -1.0), (2 * HX - 160.0, 14.0, 3.0), 'HullLight', bevel=0))
    for sign in (-1, 1):
        x_face = sign * (HX - 14.0)
        parts.append(sb_kit.box('end_panel', (x_face, 0, H / 2.0), (16.0, 2 * HY - 60.0, H - 70.0), 'HullPlate', bevel=2, segments=2))
        stripes_on_end(sign, parts)
        for z in (14.0, H - 22.0):
            parts.append(sb_kit.box('end_rail', (sign * (HX - 8.0), 0, z), (16.0, 2 * HY, 20.0), 'HullFrame', bevel=3, segments=2))
    # Corner posts, lifting lugs (on the outside of the posts, below the deck) and the four cables.
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(sb_kit.box('post', (sx * (HX - 20.0), sy * (HY - 20.0), H / 2.0), (44.0, 44.0, H), 'HullFrame', bevel=4, segments=2))
            lug = sb_kit.box('lug', (sx * (HX - 20.0), sy * (HY + 14.0), H - 60.0), (40.0, 28.0, 60.0), 'HullFrame', bevel=5, segments=2)
            parts.append(lug)
            parts.append(sb_kit.cyl('cable', (sx * (HX - 20.0), sy * (HY + 14.0), (H - 30.0 + CEILING - 60.0) / 2.0),
                                    4.0, CEILING - 60.0 - (H - 30.0), 'HullFrame', axis='Z', verts=8, bevel=0))
    # The yoke under the ceiling: two box beams along X over the cable lines, a cross beam, and hanger blocks.
    for sy in (-1, 1):
        parts.append(sb_kit.box('yoke', (0, sy * (HY + 14.0), CEILING - 40.0), (2 * HX + 200.0, 50.0, 70.0), 'HullFrame', bevel=5, segments=2))
        parts.append(sb_kit.box('yoke_stripe', (0, sy * (HY + 14.0 + 26.0), CEILING - 40.0), (2 * HX, 3.0, 20.0), 'HullHazard', bevel=0))
    for sx in (-1, 0, 1):
        parts.append(sb_kit.box('cross', (sx * HX * 0.9, 0, CEILING - 25.0), (50.0, 2 * HY + 80.0, 50.0), 'HullFrame', bevel=5, segments=2))

    obj = sb_kit.join(NAME, parts)
    low = min(v.co.z for v in obj.data.vertices); high = max(v.co.z for v in obj.data.vertices)
    top_inside = max(v.co.z for v in obj.data.vertices if abs(v.co.x) < HX - 30.0 and abs(v.co.y) < HY - 30.0 and v.co.z < 400.0)
    triangles = sb_kit.triangle_count(obj)
    if low < -5.0 or high > CEILING + 0.1 or top_inside > H + 0.5:
        raise RuntimeError(f'Pod left its envelope: Z {low} .. {high}, deck top {top_inside}')
    if triangles > 20000:
        raise RuntimeError(f'Unexpected geometry expansion: {triangles} triangles')
    OUT.mkdir(parents=True, exist_ok=True)
    line = f'{NAME}: {triangles} tris, {sb_kit.bounds(obj)}, slots ' + ', '.join(m.name for m in obj.data.materials)
    (OUT / 'keel_hero_report.txt').write_text(line + '\n', encoding='utf-8')
    sb_kit.export_fbx(str(OUT / (NAME + '.fbx')), [obj])
    palette_file = ROOT / 'Art/Export/palette.json'
    palette = json.loads(palette_file.read_text())
    colour, metallic, roughness, emission = sb_kit.PALETTE['HullHazard']
    palette['HullHazard'] = [list(colour), metallic, roughness, emission]
    palette_file.write_text(json.dumps(palette, indent=1) + '\n', encoding='utf-8')
    generated = ROOT / 'Saved/Art/Generated'; generated.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(generated / 'CradlePod.blend'))
    print(line)


if __name__ == '__main__':
    build()
