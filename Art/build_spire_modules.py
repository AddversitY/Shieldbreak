"""Original Spire hard-surface modules; standard Python, centimetres, no downloads.

OBJ coordinates (x, -y, z) compensate for the legacy importer's Y reflection.
Verified by the imported bounding boxes, not assumed from OBJ's axis conventions.
Run this script, then Tools/ImportSpireArt.bat. Geometry is decorative only.
"""
from pathlib import Path
import math

OUT = Path(__file__).parent / 'Export' / 'Spire'


class Mesh:
    def __init__(self):
        self.vertices, self.faces = [], []

    def prism(self, profile, front, back):
        """CCW X/Z silhouette, extruded along Y; outward triangle winding."""
        base, n = len(self.vertices), len(profile)
        self.vertices += [(x, y, z) for y in (front, back) for x, z in profile]
        for i in range(1, n - 1):
            self.faces += [(base, base+i, base+i+1), (base+n, base+n+i+1, base+n+i)]
        for i in range(n):
            j = (i + 1) % n
            self.faces += [(base+i, base+n+i, base+n+j), (base+i, base+n+j, base+j)]

    def panel(self, x, y, z, width, depth, height, bevel):
        l, r, b, t = x-width/2, x+width/2, z-height/2, z+height/2
        self.prism([(l+bevel,b),(r-bevel,b),(r,b+bevel),(r,t-bevel),
                    (r-bevel,t),(l+bevel,t),(l,t-bevel),(l,b+bevel)], y-depth/2, y+depth/2)

    def save(self, name):
        # A closed, outward-wound solid has positive signed volume. Catch an
        # accidental mirrored silhouette before producing an imported asset.
        volume6 = 0.0
        for i,j,k in self.faces:
            a,b,c = (self.vertices[n] for n in (i,j,k))
            volume6 += (a[0]*(b[1]*c[2]-b[2]*c[1])
                        + a[1]*(b[2]*c[0]-b[0]*c[2])
                        + a[2]*(b[0]*c[1]-b[1]*c[0]))
        if volume6 <= 0:
            raise ValueError('Incorrect face winding: '+name)
        OUT.mkdir(parents=True, exist_ok=True)
        lines = ['# Shieldbreak original modular environment art, centimetres', 'o '+name]
        lines += [f'v {x:.4f} {-y:.4f} {z:.4f}' for x,y,z in self.vertices]
        lines += ['f '+' '.join(str(i+1) for i in reversed(face)) for face in self.faces]
        (OUT / (name+'.obj')).write_text('\n'.join(lines)+'\n', encoding='ascii')
        print(name, len(self.vertices), 'vertices', len(self.faces), 'triangles')


def build():
    fin = Mesh()
    fin.prism([(-110,0),(110,0),(85,550),(25,1000),(-35,940),(-100,440)], -55,55)
    fin.panel(0,0,65,320,230,130,25)
    for z in (240,400,560,720):
        fin.panel(-8,-65,z,135-(z-240)*.12,30,45,10)
    fin.save('SM_SB_Spire_Collector')
    vent = Mesh()
    vent.panel(0,0,0,220,24,320,28)
    for z in range(-100,101,40):
        vent.panel(0,-20,z,160,22,18,5)
    vent.panel(0,-20,132,100,22,12,3)
    vent.save('SM_SB_Spire_ServicePanel')
    ring = Mesh()
    # Segmented low-profile collar: central opening remains visually clear.
    for i in range(12):
        a,b = 2*math.pi*(i+.06)/12, 2*math.pi*(i+.94)/12
        # prism is X/Z with Y as extrusion, so write a floor-ring directly.
        points=[(r*math.cos(t),r*math.sin(t)) for r,t in ((125,a),(155,a),(155,b),(125,b))]
        offset=len(ring.vertices)
        ring.vertices += [(x,y,z) for z in (0,8) for x,y in points]
        for tri in ((0,2,1),(0,3,2),(4,5,6),(4,6,7),(0,1,5),(0,5,4),
                    (1,2,6),(1,6,5),(2,3,7),(2,7,6),(3,0,4),(3,4,7)):
            ring.faces.append(tuple(offset+j for j in tri))
    ring.save('SM_SB_Spire_LiftCollar')


if __name__ == '__main__':
    build()
