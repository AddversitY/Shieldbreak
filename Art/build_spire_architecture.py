"""Original Spire ceiling structure, centimetres. Standard Python; no Blender or external models.

Meshes are authored as closed solids, with validated winding. The arch's lowest point is placed at
19 m in game, above the largest existing lift arc and player head envelope. It does not add collision.
Run this script, then Tools/ImportSpireArchitecture.bat.
"""
from pathlib import Path
import json
from build_spire_modules import Mesh

OUT = Path(__file__).parent / 'Export' / 'SpireArchitecture'


def build():
    report = {}
    # Local X follows the span from the open centre toward the wall. Separate alloy cheeks around a
    # dark mechanical web give a readable silhouette and actual depth, without glowing lines everywhere.
    rib = Mesh()
    cheek = [(0, 30), (2250, 240), (2700, 380), (2700, 520), (2220, 405), (0, 165)]
    rib.beveled_prism(cheek, -95, -62, 4)
    rib.beveled_prism(cheek, 62, 95, 4)
    # A broad mounting shoe at the outside end, largely recessed into the ceiling and perimeter wall.
    rib.panel(2580, 0, 390, 240, 250, 220, 32)
    report['SM_SB_Spire_ArchAlloy'] = rib.save('SM_SB_Spire_ArchAlloy', OUT)

    web = Mesh()
    web.beveled_prism([(25, 48), (2230, 258), (2630, 384), (2630, 456), (2200, 365), (25, 135)], -58, 58, 3)
    # Short transverse blocks attach the cheeks to the web and give the underside a mechanical rhythm.
    for x, z in ((500, 105), (1350, 185), (2150, 270)):
        web.panel(x, 0, z, 95, 150, 135, 12)
    report['SM_SB_Spire_ArchInset'] = web.save('SM_SB_Spire_ArchInset', OUT)

    # The short longitudinal spine ties the repeated transverse ribs into one ceiling framework.
    spine = Mesh()
    spine.beveled_prism([(-1200, 0), (1200, 0), (1200, 100), (1140, 145), (-1140, 145), (-1200, 100)], -65, 65, 3)
    report['SM_SB_Spire_CeilingSpine'] = spine.save('SM_SB_Spire_CeilingSpine', OUT)

    # A recessed physical housing around the existing 1.6 x .5 m light card. The opening stays clear;
    # the frame gives the practical light a manufactured edge instead of a floating white rectangle.
    lamp = Mesh()
    for x, z, width, height in ((0, 32, 200, 16), (0, -32, 200, 16), (-92, 0, 16, 48), (92, 0, 16, 48)):
        lamp.panel(x, 0, z, width, 20, height, 3)
    lamp.vertices = [(x, z, 10-y) for x, y, z in lamp.vertices]  # proper rotation: XY face, bottom at Z=0
    report['SM_SB_Spire_LampHousing'] = lamp.save('SM_SB_Spire_LampHousing', OUT)
    for name, data in report.items():
        assert data['volume'] > 0 and data['triangles'] < 400, name
        assert data['min'][2] >= 0, name
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'architecture.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'{len(report)} original architectural meshes, positive volume, under 400 triangles each')


if __name__ == '__main__':
    build()
