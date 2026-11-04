"""Offline geometry and conservative ordinary-jump/lift clearance checks for Spire's new ceiling art."""
from collections import Counter
from pathlib import Path
import json
import math
import runpy
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'Art'))
import build_spire_modules as geometry


def signed_volume(vertices, faces):
    result = 0.0
    for i, j, k in faces:
        a, b, c = (vertices[n] for n in (i, j, k))
        result += (a[0]*(b[1]*c[2]-b[2]*c[1]) + a[1]*(b[2]*c[0]-b[0]*c[2])
                   + a[2]*(b[0]*c[1]-b[1]*c[0]))/6
    return result


def main():
    # A C-shaped section cannot be safely triangulated as a fan from its first corner.
    profile = [(0, 0), (3, 0), (3, 1), (1, 1), (1, 2), (3, 2), (3, 3), (0, 3)]
    triangles = geometry.triangulate(profile)
    area = 0
    for i, j, k in triangles:
        a, b, c = (profile[n] for n in (i, j, k))
        cross = (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
        assert cross > 0, 'Triangulation reversed or overlapped a face'
        area += cross/2
    assert math.isclose(area, 7), f'Concave face area {area}, expected 7'
    section = geometry.Mesh(); section.prism(profile, -1, 1)
    assert math.isclose(signed_volume(section.vertices, section.faces), 14), 'Concave extrusion volume'
    print('PASS: concave C-section area 7, closed-solid volume 14, outward faces')

    beveled = geometry.Mesh()
    beveled.beveled_prism([(0, 0), (4, 0), (4, 4), (0, 4)], 0, 4, 1)
    # Two square frusta (height 1, areas 4 and 16) and a 4x4x2 middle section.
    assert math.isclose(signed_volume(beveled.vertices, beveled.faces), 32+2*(4+16+8)/3)
    print('PASS: chamfered prism volume matches two analytic square frusta plus the middle section')

    manifest = json.loads((ROOT/'Art/Export/SpireArchitecture/architecture.json').read_text())
    for name, expected in manifest.items():
        vertices, faces = [], []
        for line in (ROOT/'Art/Export/SpireArchitecture'/(name+'.obj')).read_text().splitlines():
            parts = line.split()
            if parts and parts[0] == 'v': vertices.append(tuple(map(float, parts[1:])))
            if parts and parts[0] == 'f': faces.append(tuple(int(n)-1 for n in parts[1:]))
        edges = Counter(tuple(sorted((face[i], face[(i+1) % 3]))) for face in faces for i in range(3))
        assert all(count == 2 for count in edges.values()), name+': open/non-manifold solid'
        assert math.isclose(signed_volume(vertices, faces), expected['volume'], rel_tol=1e-7), name+': export winding/volume'
        print(f'PASS: {name}, manifold components and mirrored-export volume')

    layout = runpy.run_path(str(ROOT/'Design/maps/shieldbreak_maps.py'))['spire']()
    ceiling = min(lo[2] for lo, hi, style in layout.blocks
                  if lo[2] > 0 and hi[0]-lo[0] >= layout.half_x*2 and hi[1]-lo[1] >= layout.half_y*2)
    top = max(hi[2] for lo, hi, style in layout.blocks if hi[2] < ceiling)
    # 450 cm is above the current full Power Jump (~390); 200 cm includes the standing capsule/head.
    ordinary_head = top + 450 + 200
    lift_head = max(max(start[2], end[2]) + max(150, abs(end[2]-start[2])*.6) + 200
                    for start, end in layout.lifts)
    lowest = min(ceiling-300+manifest[name]['min'][2] for name in ('SM_SB_Spire_ArchAlloy', 'SM_SB_Spire_ArchInset'))
    margin = lowest-max(ordinary_head, lift_head)
    assert margin >= 50, f'Ceiling art needs reevaluation: only {margin:.1f} cm clearance'
    print(f'PASS: lowest rib {lowest:.0f} cm, conservative ordinary-jump/lift head {max(ordinary_head,lift_head):.0f} cm; margin {margin:.0f} cm')
    print('Grenade-assisted stacks, arbitrary knockback and human visual clipping remain separate playtests.')

    # The shared helper must still generate the original collector, panel and collar without invalid solids.
    geometry.OUT = ROOT/'Saved/SpireGeometryQA/LegacyModules'
    geometry.build()
    print('PASS: original three-module authoring still generates valid outward-wound solids')


if __name__ == '__main__':
    main()
