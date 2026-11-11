"""Shieldbreak's soldier, v3: a heavy armored infantry suit built around the Lyra mannequin (2026-11-11).

    python build_soldier.py <bones.json> <SKM_Manny.obj> [--render DIR] [--bake RES] [--no-export] [--high-preview]

Vincent's brief: a bulky, layered, battle-worn heavy trooper (grey chipped plate over a dark undersuit, an olive
load-bearing rig with pouches, an asymmetric big left pauldron, a power pack with cells and hoses, an amber visor).
The design here is Shieldbreak's own: every shape is generated below, nothing is traced or imported.

How it fits the game (unchanged contract with UShieldbreakBodyAnimator::AttachArmor): one rigid static mesh per bone,
SM_SB_Armor_<bone>, modeled in the mannequin's reference pose in Unreal mesh space (cm; X = the mannequin's left,
Y = forward, Z = up). The suit therefore follows every existing animation and the ragdoll with no skinning.

What is new against build_armor.py (v2):
 - Limb shells are lofted around each bone from the skin's measured cross-section (Limb), so plates are thick, clean
   designed forms that still never clip the body, instead of convex hulls.
 - One shared texture set for the whole suit (T_SB_Soldier_BC/ORM/N + _M mask) baked from a high-poly twin, and one
   material slot (M_Team_Soldier), so a soldier is 14 draw calls with one material. Right limbs are mirrored copies of
   the left ones and share their texels.
 - The team colour is a mask (T_SB_Soldier_M red channel) over the painted plates rather than separate slots.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sb_kit as k  # noqa: E402
import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
import build_armor as ba  # noqa: E402

# Preview colours (linear) for the surfaces this suit adds; the baked look comes from bake_weapons.SURFACES.
k.PALETTE.update({
    "ArmorPlate": ((0.19, 0.195, 0.2), 0.2, 0.45, 0.0),
    "ArmorTeam": ((0.45, 0.06, 0.05), 0.2, 0.45, 0.0),
    "ArmorDark": ((0.07, 0.073, 0.08), 0.3, 0.5, 0.0),
    "Webbing": ((0.1, 0.11, 0.06), 0.0, 0.85, 0.0),
    "Canvas": ((0.2, 0.17, 0.11), 0.0, 0.9, 0.0),
    "RedCap": ((0.4, 0.035, 0.025), 0.0, 0.5, 0.0),
    "Rubber": ((0.025, 0.025, 0.027), 0.0, 0.8, 0.0),
})
TEAM_SURFACES = ("ArmorTeam", "TeamGlow")

HIGH = False   # set by build(): the high-poly twin doubles the loft resolution and bevel segments and adds floaters
UP = Vector((0, 0, 1))
FWD = Vector((0, 1, 0))


# =====================================================================================================================
# Helpers
# =====================================================================================================================
def _mesh_obj(name, bm, mat, smooth=True, sharp=40.0):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.data.materials.append(k.material(mat))
    if smooth:
        ba.smooth_by_angle(obj, sharp)
    return obj


def bevel(obj, width, segments=1, angle=38.0):
    if width <= 0:
        return obj
    mod = obj.modifiers.new("Bevel", "BEVEL")
    mod.width = width
    mod.segments = segments * 3 if HIGH else segments
    mod.limit_method = "ANGLE"
    mod.angle_limit = math.radians(angle)
    mod.harden_normals = True
    return obj


def superellipse(rx, ry, n=3.0):
    """Radius by angle of a rounded-box cross-section: ry toward the limb's front (angle 0), rx toward its side."""
    def r(ang):
        c, s = abs(math.cos(ang)) / ry, abs(math.sin(ang)) / rx
        return (c ** n + s ** n) ** (-1.0 / n)
    return r


class Limb:
    """A bone segment with the skin's radius measured around it. Angles (degrees in the public calls): 0 = front,
    +90 = side (the outer side for left limbs: axis x front), 180 = back. t runs 0..1 from a to b."""

    def __init__(self, points, a, b, front=FWD, nt=14, na=28):
        self.a, self.b = Vector(a), Vector(b)
        self.axis = (self.b - self.a)
        self.length = self.axis.length
        self.axis.normalize()
        f = Vector(front)
        self.f = (f - self.axis * f.dot(self.axis)).normalized()
        self.s = self.axis.cross(self.f).normalized()
        table = np.full((nt, na), np.nan)
        for p in points:
            d = p - self.a
            t = d.dot(self.axis) / self.length
            if t < 0.0 or t > 1.0:
                continue
            rad = d - self.axis * d.dot(self.axis)
            ang = math.atan2(rad.dot(self.s), rad.dot(self.f)) % (2 * math.pi)
            i = min(nt - 1, int(t * nt))
            j = min(na - 1, int(ang / (2 * math.pi) * na))
            r = rad.length
            if np.isnan(table[i, j]) or r > table[i, j]:
                table[i, j] = r
        for _ in range(60):
            if not np.isnan(table).any():
                break
            pad = np.pad(table, ((1, 1), (0, 0)), mode="edge")
            nb = np.stack([pad[:-2], pad[2:], np.roll(table, 1, 1), np.roll(table, -1, 1)])
            fill = np.nanmean(nb, axis=0) if not np.isnan(nb).all() else table
            table = np.where(np.isnan(table), fill, table)
        table = np.nan_to_num(table, nan=5.0)
        for _ in range(2):
            pad = np.pad(table, ((1, 1), (0, 0)), mode="edge")
            blur = (pad[:-2] + pad[2:] + np.roll(table, 1, 1) + np.roll(table, -1, 1) + table * 2) / 6.0
            table = np.maximum(table * 0.5 + blur * 0.5, table - 0.4)
        self.table, self.nt, self.na = table, nt, na

    def skin(self, t, ang):
        t = min(max(t, 0.0), 1.0) * self.nt - 0.5
        i0 = int(math.floor(t))
        ft = t - i0
        i0, i1 = min(max(i0, 0), self.nt - 1), min(max(i0 + 1, 0), self.nt - 1)
        a = (ang % (2 * math.pi)) / (2 * math.pi) * self.na - 0.5
        j0 = int(math.floor(a))
        fa = a - j0
        j0, j1 = j0 % self.na, (j0 + 1) % self.na
        tb = self.table
        return (tb[i0, j0] * (1 - fa) + tb[i0, j1] * fa) * (1 - ft) + (tb[i1, j0] * (1 - fa) + tb[i1, j1] * fa) * ft

    def point(self, t, ang, r):
        return self.a + self.axis * (t * self.length) + (self.f * math.cos(ang) + self.s * math.sin(ang)) * r

    def out(self, ang_deg):
        ang = math.radians(ang_deg)
        return self.f * math.cos(ang) + self.s * math.sin(ang)

    def surface(self, t, ang_deg, pad=0.0, design=None):
        ang = math.radians(ang_deg)
        r = self.skin(t, ang) + pad
        if design:
            r = max(r, design(t, ang))
        return self.point(t, ang, r)

    def panel(self, name, t0, t1, a0, a1, mat, pad=1.0, thick=1.0, design=None, res=(6, 8), chamfer=0.0, shape=None,
              bev=0.3, sharp=40.0, taper=0.0):
        """A plate over the limb between t0..t1 and angles a0..a1 (degrees). Its outer face sits at
        max(skin + pad, design(t, ang)) + shape(u, v); a rim of `thick` turns in toward the body. A 360-degree span
        makes a closed sleeve with lipped ends. taper narrows the angular span toward t1 (0..1)."""
        full = abs((a1 - a0) - 360.0) < 1e-3
        nt, na = res
        if HIGH:
            nt, na = nt * 2, na * 2
        bm = bmesh.new()
        grid = {}

        def radius(t, ang, u, v):
            r = self.skin(t, ang) + pad
            if design:
                r = max(r, design(t, ang))
            if shape:
                r += shape(u, v)
            return r

        for i in range(nt + 1):
            for j in range(na + (0 if full else 1)):
                v = -1.0 + 2.0 * i / nt
                u = -1.0 + 2.0 * j / na
                uu, vv = u, v
                if not full and chamfer > 0:
                    lim = 2.0 - chamfer
                    if abs(u) + abs(v) > lim:
                        sc = lim / (abs(u) + abs(v))
                        uu, vv = u * sc, v * sc
                if not full and taper:
                    uu *= 1.0 - taper * (vv + 1.0) * 0.5
                t = t0 + (t1 - t0) * (vv + 1.0) * 0.5
                ang = math.radians(a0 + (a1 - a0) * (uu + 1.0) * 0.5)
                grid[(i, j)] = (bm.verts.new(self.point(t, ang, radius(t, ang, uu, vv))), t, ang, uu, vv)
        cols = na if full else na
        for i in range(nt):
            for j in range(cols):
                jn = (j + 1) % na if full else j + 1
                q = [grid[(i, j)][0], grid[(i, jn)][0], grid[(i + 1, jn)][0], grid[(i + 1, j)][0]]
                try:
                    bm.faces.new(q)
                except ValueError:
                    pass

        def inner(key):
            _, t, ang, u, v = grid[key]
            return bm.verts.new(self.point(t, ang, max(radius(t, ang, u, v) - thick, 0.3)))

        if full:
            for i, flip in ((0, False), (nt, True)):
                ring_o = [grid[(i, j)][0] for j in range(na)]
                ring_i = [inner((i, j)) for j in range(na)]
                for j in range(na):
                    jn = (j + 1) % na
                    q = [ring_o[j], ring_i[j], ring_i[jn], ring_o[jn]]
                    bm.faces.new(q if not flip else list(reversed(q)))
        else:
            rim = ([(0, j) for j in range(na)] + [(i, na) for i in range(nt)] + [(nt, j) for j in range(na, 0, -1)]
                   + [(i, 0) for i in range(nt, 0, -1)])
            inn = {key: inner(key) for key in rim}
            for idx, key in enumerate(rim):
                nxt = rim[(idx + 1) % len(rim)]
                try:
                    bm.faces.new((grid[key][0], inn[key], inn[nxt], grid[nxt][0]))
                except ValueError:
                    pass
        bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=0.02)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        # recalc can pick "inward" for an open shell; orient by the first grid face against the limb's radial.
        bm.faces.ensure_lookup_table()
        f0 = bm.faces[0]
        c = f0.calc_center_median()
        radial = (c - self.a) - self.axis * (c - self.a).dot(self.axis)
        if f0.normal.dot(radial) < 0:
            bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
        obj = _mesh_obj(name, bm, mat, sharp=sharp)
        return bevel(obj, bev)


def obox(name, center, size, mat, n=FWD, up=UP, bev=0.5, segments=1, taper=None):
    """A box of size (width, depth along n, height along up), oriented so its +Y face looks along n."""
    obj = k.box(name, (0, 0, 0), size, mat, bevel=0, taper=taper)
    n = Vector(n).normalized()
    u = Vector(up)
    u = (u - n * u.dot(n)).normalized()
    x = n.cross(u) * -1.0
    x = u.cross(n) * -1.0 if False else n.cross(u).normalized()
    # columns: X = n x up would be left-handed with (Y = n, Z = up); use X = up x n ... check handedness below.
    m = Matrix((x, n, u)).transposed()
    if m.determinant() < 0:
        x = -x
        m = Matrix((x, n, u)).transposed()
    obj.matrix_world = Matrix.Translation(Vector(center)) @ m.to_4x4()
    return bevel(obj, bev, segments)


def ocyl(name, center, radius, length, mat, axis=UP, verts=12, bev=0.3, radius2=None):
    if HIGH:
        verts *= 2
    obj = k.cyl(name, (0, 0, 0), radius, length, mat, axis="Z", verts=verts, bevel=0, radius2=radius2)
    rot = UP.rotation_difference(Vector(axis).normalized()).to_matrix().to_4x4()
    obj.matrix_world = Matrix.Translation(Vector(center)) @ rot
    return bevel(obj, bev, 1, angle=60.0)


def cap(name, center, radii, mat, keep, basis=None, thick=1.0, segs=(16, 10), power=(0.8, 0.8, 0.8), bev=0.3,
        sharp=40.0, shape=None):
    """Part of a rounded-box ellipsoid: faces whose local unit position passes keep(Vector) stay, the rest go, and the
    shell is thickened inward. basis = (x, y, z) axes of the ellipsoid in mesh space."""
    su, sv = (segs[0] * 2, segs[1] * 2) if HIGH else segs
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=su, v_segments=sv, radius=1.0)

    def sq(x, p):
        return math.copysign(abs(x) ** p, x)
    bx, by, bz = [Vector(v).normalized() for v in (basis or ((1, 0, 0), (0, 1, 0), (0, 0, 1)))]
    c = Vector(center)
    local = {}
    for v in bm.verts:
        l = Vector((sq(v.co.x, power[0]), sq(v.co.y, power[1]), sq(v.co.z, power[2])))
        if shape:
            l = shape(l)
        local[v] = l
    drop = [f for f in bm.faces if not keep(sum((local[v] for v in f.verts), Vector()) / len(f.verts))]
    for v in bm.verts:
        l = local[v]
        v.co = c + bx * (l.x * radii[0]) + by * (l.y * radii[1]) + bz * (l.z * radii[2])
    bmesh.ops.delete(bm, geom=drop, context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.faces.ensure_lookup_table()
    if bm.faces and bm.faces[0].normal.dot(bm.faces[0].calc_center_median() - c) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    obj = _mesh_obj(name, bm, mat, sharp=sharp)
    if thick > 0:
        mod = obj.modifiers.new("Solidify", "SOLIDIFY")
        mod.thickness = thick
        mod.offset = -1.0
    return bevel(obj, bev)


def tube(name, pts, radius, mat, sides=8, steps=6):
    """A hose through pts (Catmull-Rom), closed at both ends."""
    if HIGH:
        sides, steps = sides * 2, steps * 2
    pts = [Vector(p) for p in pts]
    ext = [pts[0] * 2 - pts[1]] + pts + [pts[-1] * 2 - pts[-2]]
    path = []
    for i in range(1, len(ext) - 2):
        p0, p1, p2, p3 = ext[i - 1], ext[i], ext[i + 1], ext[i + 2]
        for sidx in range(steps):
            t = sidx / steps
            path.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                               + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    path.append(pts[-1])
    bm = bmesh.new()
    rings = []
    ref = Vector((0.3, 0.5, 0.8)).normalized()
    for i, p in enumerate(path):
        d = (path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized()
        u = d.cross(ref).normalized()
        w = d.cross(u).normalized()
        ref = w.cross(d) if False else ref
        rings.append([bm.verts.new(p + (u * math.cos(2 * math.pi * j / sides) + w * math.sin(2 * math.pi * j / sides)) * radius)
                      for j in range(sides)])
    for a, b in zip(rings, rings[1:]):
        for j in range(sides):
            jn = (j + 1) % sides
            bm.faces.new((a[j], a[jn], b[jn], b[j]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return _mesh_obj(name, bm, mat, sharp=60.0)


def pouch(name, center, size, n, up=UP, mat="Webbing", flap=0.45, strap=True):
    """A closed fabric pouch: body, a flap over its top front, and a closing strap. size = (width, depth, height)."""
    w, d, h = size
    n = Vector(n).normalized()
    u = Vector(up)
    u = (u - n * u.dot(n)).normalized()
    c = Vector(center)
    parts = [obox(name, c, (w, d, h), mat, n, u, bev=0.7, segments=2, taper=(0.94, 0.96))]
    parts.append(obox(name + "_flap", c + u * (h * (0.5 - flap * 0.5) + 0.25) + n * 0.35, (w + 0.5, d + 0.5, h * flap), mat, n, u,
                      bev=0.5, segments=2))
    if strap:
        parts.append(obox(name + "_strap", c + u * (h * 0.12) + n * (d * 0.5 + 0.3), (w * 0.3, 0.35, h * 0.55), "Rubber", n, u, bev=0.1))
        if HIGH:
            parts.append(ocyl(name + "_snap", c + u * (h * 0.02) + n * (d * 0.5 + 0.55), 0.38, 0.25, "Steel", n, verts=10, bev=0.06))
    return parts


def bolt(name, pos, normal, r=0.42, mat="Steel"):
    """HIGH only: a fastener head floating on a plate; it bakes into the normal map and catches grime."""
    if not HIGH:
        return []
    n = Vector(normal).normalized()
    return [ocyl(name, Vector(pos) + n * 0.05, r, 0.3, mat, n, verts=6, bev=0.07),
            ocyl(name + "_w", Vector(pos) - n * 0.02, r * 1.5, 0.12, "ArmorDark", n, verts=12, bev=0.03)]


def groove(name, limb, t0, a0, t1, a1, pad, design=None, width=0.28, depth=0.22, steps=10):
    """HIGH only: a V-shaped panel line laid on a limb plate's surface from (t0, a0) to (t1, a1); a floater whose
    concave section bakes into the normal map as a recessed seam."""
    if not HIGH:
        return []
    bm = bmesh.new()
    prev = None
    pts = []
    for i in range(steps + 1):
        f = i / steps
        t, ang = t0 + (t1 - t0) * f, a0 + (a1 - a0) * f
        p = limb.surface(t, ang, pad, design)
        pts.append((p, limb.out(ang)))
    for i, (p, o) in enumerate(pts):
        d = (pts[min(i + 1, steps)][0] - pts[max(i - 1, 0)][0]).normalized()
        side = d.cross(o).normalized()
        row = [bm.verts.new(p + side * width + o * 0.06), bm.verts.new(p - o * depth), bm.verts.new(p - side * width + o * 0.06)]
        if prev:
            bm.faces.new((prev[0], prev[1], row[1], row[0]))
            bm.faces.new((prev[1], prev[2], row[2], row[1]))
        prev = row
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.faces.ensure_lookup_table()
    if bm.faces[0].normal.dot(pts[0][1]) < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    return [_mesh_obj(name, bm, "ArmorDark", sharp=80.0)]


def mirrored(obj, name):
    """A copy of a joined piece mirrored across X = 0 (for the right-hand limbs); UVs are kept, so it shares texels."""
    mesh = obj.data.copy()
    mesh.transform(Matrix.Scale(-1.0, 4, (1, 0, 0)))
    mesh.flip_normals()
    out = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(out)
    return out


def mirror_parts(parts):
    """Mirrors unjoined parts across X = 0 in place (asymmetric pieces built in 'left space' for the right side)."""
    flip = Matrix.Scale(-1.0, 4, (1, 0, 0))
    for obj in parts:
        obj.data.transform(obj.matrix_world)
        obj.matrix_world = Matrix.Identity(4)
        obj.data.transform(flip)
        obj.data.flip_normals()
    return parts


# =====================================================================================================================
# The suit
# =====================================================================================================================
def build(verts, j, high=False):
    """Returns {bone: [objects]} for the unique pieces: torso, head, both upper arms (asymmetric) and the LEFT forearm,
    thigh, shin and boot (their right twins are mirrored after the bake)."""
    global HIGH
    HIGH = high
    P = {}
    X = Vector((1, 0, 0))

    def pts(bones, clip=None):
        return ba.gather(verts, bones, 0.5, clip)

    # ---- Helmet ---------------------------------------------------------------------------------------------------
    hc = j["head"] + Vector((0, 2.0, 8.6))
    R = (11.4, 13.6, 11.6)

    def helm_shape(l):
        out = l.copy()
        if l.z < -0.1 and l.y > 0.0:      # jaw: forward and narrower
            out.y = out.y * (1.0 + 0.16 * min(1.0, -l.z * 1.6))
            out.x *= 1.0 - 0.14 * min(1.0, -l.z * 1.4)
        if l.z > 0.25:                     # flatter crown, sloping down toward the brow
            out.z = 0.25 + (l.z - 0.25) * (0.86 - 0.12 * max(0.0, l.y))
        if l.z > -0.1 and abs(l.x) > 0.55:  # flattened temples
            out.x = math.copysign(0.55 + (abs(l.x) - 0.55) * 0.82, l.x)
        if l.y < -0.2 and l.z < 0.0:      # rear flare over the collar
            out.y *= 1.06
        return out

    helm = [cap("helm_shell", hc, R, "ArmorDark", lambda l: l.z > -0.86, thick=0.0, segs=(24, 16), power=(0.66, 0.8, 0.74),
                shape=helm_shape, bev=0.0, sharp=30)]
    # Crown: three overlapping lames running front to back, the centre one proud (a keel ridge).
    helm.append(cap("crown_mid", hc, (R[0] + 0.9, R[1] + 0.9, R[2] + 1.0), "ArmorTeam",
                    lambda l: abs(l.x) < 0.3 and l.z > -0.05 and (l.y < 0.72 or l.z > 0.45), thick=1.0, segs=(24, 16),
                    power=(0.66, 0.8, 0.74), shape=helm_shape, bev=0.3, sharp=30))
    for sx in (1, -1):
        helm.append(cap("crown_side", hc, (R[0] + 0.45, R[1] + 0.45, R[2] + 0.5), "ArmorPlate",
                        lambda l, sx=sx: 0.3 < l.x * sx < 0.99 and l.z > 0.12 and l.y < 0.6 and l.y > -0.8, thick=0.9,
                        segs=(24, 16), power=(0.66, 0.8, 0.74), shape=helm_shape, bev=0.3, sharp=30))
        # Cheek guard, below the visor line, wrapping to the ear.
        helm.append(cap("cheek", hc, (R[0] + 0.5, R[1] + 0.5, R[2] + 0.4), "ArmorPlate",
                        lambda l, sx=sx: l.x * sx > 0.36 and -0.82 < l.z < -0.2 and l.y > -0.25, thick=0.9, segs=(24, 16),
                        power=(0.66, 0.8, 0.74), shape=helm_shape, bev=0.3, sharp=30))
        ear = hc + Vector((sx * (R[0] + 0.3), -2.0, -0.5))
        helm.append(ocyl("ear", ear, 3.3, 2.2, "ArmorDark", X * sx, verts=10, bev=0.4, radius2=2.6))
        helm.append(ocyl("ear_cap", ear + X * sx * 1.2, 1.7, 0.6, "Steel", X * sx, verts=10, bev=0.15))
        # Filter canister on the jaw, angled forward and down.
        can_axis = Vector((sx * 0.35, 1.0, -0.25)).normalized()
        can = hc + Vector((sx * 6.6, 10.4, -8.2))
        helm.append(ocyl("filter", can, 2.3, 5.0, "ArmorDark", can_axis, verts=12, bev=0.3))
        helm.append(ocyl("filter_cap", can + can_axis * 2.7, 2.0, 0.7, "Steel", can_axis, verts=12, bev=0.15))
        for i in range(3):
            helm.append(ocyl("filter_rib", can + can_axis * (-1.4 + i * 1.1), 2.5, 0.35, "Rubber", can_axis, verts=12, bev=0.08))
    # Rear neck guard.
    helm.append(cap("nape", hc, (R[0] + 0.6, R[1] + 0.8, R[2] + 0.4), "ArmorPlate",
                    lambda l: l.y < -0.35 and -0.86 < l.z < -0.1, thick=1.0, segs=(24, 16), power=(0.66, 0.8, 0.74),
                    shape=helm_shape, bev=0.3, sharp=30))
    # Visor: a narrow amber band under a heavy brow.
    helm.append(cap("visor", hc, (R[0] + 0.7, R[1] + 0.8, R[2] + 0.4), "Visor",
                    lambda l: l.y > 0.4 and -0.22 < l.z < 0.1 and abs(l.x) < 0.84, thick=0.9, segs=(24, 16),
                    power=(0.66, 0.8, 0.74), shape=helm_shape, bev=0.0, sharp=30))
    helm.append(cap("brow", hc, (R[0] + 1.1, R[1] + 1.7, R[2] + 0.8), "ArmorPlate",
                    lambda l: l.y > 0.25 and 0.06 < l.z < 0.5 and abs(l.x) < 0.95, thick=1.6, segs=(24, 16),
                    power=(0.66, 0.8, 0.74), shape=helm_shape, bev=0.35, sharp=30))
    # Respirator: a wedge under the visor with slats, and a chin bar.
    muzzle = hc + Vector((0, 12.6, -6.4))
    helm.append(obox("muzzle", muzzle, (7.6, 5.6, 6.6), "ArmorPlate", FWD, UP, bev=0.9, segments=2, taper=(0.62, 0.7)))
    for i in range(4):
        helm.append(obox("muzzle_slat", muzzle + Vector((0, 2.95, 1.7 - i * 1.15)), (3.4, 0.35, 0.5), "Rubber", bev=0.0))
    helm.append(obox("chin", hc + Vector((0, 10.0, -10.6)), (9.5, 6.0, 2.2), "ArmorDark", bev=0.6, segments=2, taper=(0.7, 0.8)))
    # Asymmetric comms: a boxed antenna base on the wearer's right.
    helm.append(obox("comm", hc + Vector((-R[0] - 0.2, -5.0, 3.2)), (1.6, 4.4, 3.4), "ArmorDark", bev=0.35))
    helm.append(ocyl("antenna", hc + Vector((-R[0] - 0.4, -6.2, 8.5)), 0.28, 8.0, "Rubber", UP, verts=6, bev=0))
    helm.append(obox("helm_light", hc + Vector((R[0] + 0.55, 4.6, 3.2)), (0.5, 2.4, 0.7), "TeamGlow", bev=0))
    helm += bolt("b_brow", hc + Vector((7.4, 9.3, 4.6)), (0.5, 0.7, 0.4)) + bolt("b_brow", hc + Vector((-7.4, 9.3, 4.6)), (-0.5, 0.7, 0.4))
    P["head"] = helm

    # ---- Chest ----------------------------------------------------------------------------------------------------
    s5 = j["spine_05"]
    chest_pts = pts(["spine_04", "spine_05", "clavicle_l", "clavicle_r", "neck_01"], clip=lambda c: abs(c.x) < 17.5)
    T = Limb(chest_pts, (0, 1.2, 117.0), (0, 1.2, 156.0), FWD, nt=12, na=28)

    def cuirass(t, ang):
        w = 20.5 + 3.4 * min(1.0, t * 1.5)
        d = 14.0 + 2.2 * math.sin(min(1.0, t * 1.2) * math.pi * 0.75)
        if t > 0.82:
            w -= (t - 0.82) * 22.0
            d -= (t - 0.82) * 12.0
        return superellipse(w, d, 3.2)(ang)

    torso = [T.panel("cuirass", 0.02, 0.9, 0, 360, "ArmorDark", pad=1.4, thick=2.0, design=cuirass, res=(7, 24), bev=0.4)]

    def plate_d(extra):
        return lambda t, ang: cuirass(t, ang) + extra

    # Upper chest: two heavy pectoral plates with a gap at the sternum; a stencil goes on the wearer's right one.
    for sgn in (1, -1):
        torso.append(T.panel("pec", 0.47, 0.84, sgn * -4 - 60 * (sgn > 0), sgn * -4 + 60 * (sgn < 0), "ArmorTeam", pad=3.0, thick=1.6,
                             design=plate_d(1.9), res=(4, 6), chamfer=0.28, bev=0.4))
    torso.append(T.panel("sternum", 0.4, 0.8, -5, 5, "ArmorDark", pad=3.0, thick=1.2, design=plate_d(2.3), res=(3, 2), chamfer=0.4, bev=0.25))
    torso.append(obox("sternum_light", T.surface(0.74, 0, 3.0, plate_d(2.35)), (1.2, 0.5, 2.8), "TeamGlow", bev=0))
    # Back plate, either side of the pack mount.
    for sgn in (1, -1):
        torso.append(T.panel("backplate", 0.3, 0.86, 180 + sgn * 22, 180 + sgn * 84, "ArmorPlate", pad=2.6, thick=1.4,
                             design=plate_d(1.5), res=(5, 5), chamfer=0.3, bev=0.4))
    # Side plates under the arms.
    for sgn in (1, -1):
        torso.append(T.panel("flank", 0.1, 0.52, sgn * 62, sgn * 118, "ArmorPlate", pad=2.2, thick=1.3, design=plate_d(1.1), res=(4, 4),
                             chamfer=0.4, bev=0.35))
    # Load-bearing rig: an olive front panel over the lower chest with three magazine pouches.
    torso.append(T.panel("rig", 0.06, 0.46, -58, 58, "Webbing", pad=2.6, thick=1.0, design=plate_d(1.5), res=(4, 8), chamfer=0.12, bev=0.25))
    for i, ang in enumerate((-30, 0, 30)):
        c = T.surface(0.25, ang, 2.6, plate_d(1.5))
        torso += pouch("mag_%d" % i, c + T.out(ang) * 1.5, (5.4, 3.0, 8.6), T.out(ang))
    # Rig shoulder straps, front and back, with a webbing ladder (HIGH).
    for sgn in (1, -1):
        torso.append(T.panel("strap_f", 0.44, 0.93, sgn * 30 - 6, sgn * 30 + 6, "Webbing", pad=4.6, thick=0.7, design=plate_d(3.6),
                             res=(5, 2), bev=0.15))
        torso.append(T.panel("strap_b", 0.4, 0.93, 180 + sgn * 32 - 6, 180 + sgn * 32 + 6, "Webbing", pad=3.8, thick=0.7,
                             design=plate_d(2.8), res=(5, 2), bev=0.15))
    for sgn in (1, -1):
        bc = T.surface(0.5, sgn * 30, 4.6, plate_d(3.6))
        torso.append(obox("strap_buckle", bc + T.out(sgn * 30) * 0.5, (4.4, 0.9, 2.2), "ArmorDark", T.out(sgn * 30), bev=0.25))
        torso.append(obox("strap_buckle_bar", bc + T.out(sgn * 30) * 1.0, (3.0, 0.4, 0.7), "Steel", T.out(sgn * 30), bev=0.1))
    # Wearer's left chest: two red-capped signal canisters in loops. Wearer's right: a radio.
    for i in range(2):
        ang = -34 + i * 11  # negative angles are +X (the wearer's left) on the torso limb
        c = T.surface(0.62, ang, 4.6, plate_d(3.6)) + T.out(ang) * 1.6
        torso.append(ocyl("canister_%d" % i, c, 1.5, 6.4, "Webbing", UP, verts=10, bev=0.2))
        torso.append(ocyl("canister_cap_%d" % i, c + UP * 3.7, 1.55, 1.3, "RedCap", UP, verts=10, bev=0.25))
        torso.append(obox("canister_loop_%d" % i, c + T.out(ang) * 0.2 - UP * 0.6, (3.5, 3.4, 1.6), "Rubber", T.out(ang), bev=0.3))
    rc = T.surface(0.64, 30, 4.6, plate_d(3.6)) + T.out(30) * 1.7
    torso.append(obox("radio", rc, (5.0, 2.8, 7.4), "ArmorDark", T.out(30), bev=0.5, segments=2))
    torso.append(obox("radio_face", rc + T.out(30) * 1.45 + UP * 1.2, (3.6, 0.3, 2.6), "Rubber", T.out(30), bev=0.1))
    torso.append(ocyl("radio_knob", rc + UP * 4.1 + T.out(30).cross(UP) * 1.4, 0.6, 1.0, "Steel", UP, verts=8, bev=0.1))
    torso.append(ocyl("radio_aerial", rc + UP * 6.2 - T.out(30).cross(UP) * 1.5, 0.3, 5.0, "Rubber", UP, verts=6, bev=0))
    # Collar: a raised gorget, taller at the back.
    neck = j["neck_01"] + Vector((0, 1.2, -1.0))
    torso.append(cap("gorget", neck, (11.5, 12.0, 8.0), "ArmorPlate",
                     lambda l: -0.15 < l.z < 0.5 + 0.35 * max(0.0, -l.y), thick=1.4, segs=(20, 10), power=(0.8, 0.85, 1.0), bev=0.35))
    # Shoulder yokes bridging the collar and the pauldrons.
    for sx in (1, -1):
        torso.append(obox("yoke", s5 + Vector((sx * 13.5, -1.0, 11.3)), (9.5, 13.0, 3.2), "ArmorDark", FWD,
                          Vector((sx * -0.28, 0, 1)), bev=0.9, segments=2, taper=(0.8, 0.8)))
    # Power pack: a core block, two ribbed cells, a rolled groundsheet on top, exhausts and hoses.
    pk = s5 + Vector((0, -23.5, -3.0))
    BACK = -FWD
    torso.append(obox("pack_mount", pk + Vector((0, 5.5, 0)), (15.0, 5.0, 22.0), "ArmorDark", BACK, bev=0.8))
    torso.append(obox("pack_core", pk, (15.5, 9.5, 27.0), "ArmorPlate", BACK, bev=1.4, segments=2, taper=(0.82, 0.9)))
    torso.append(obox("pack_hatch", pk + Vector((0, -4.6, 3.0)), (9.5, 1.0, 12.0), "ArmorTeam", BACK, bev=0.4, segments=2))
    for sx in (1, -1):
        torso.append(obox("pack_vent", pk + Vector((sx * 2.4, -5.15, 3.0)), (1.1, 0.4, 8.0), "TeamGlow", BACK, bev=0))
        cell = pk + Vector((sx * 11.6, 0.6, -1.0))
        torso.append(ocyl("cell", cell, 4.3, 22.0, "ArmorPlate", UP, verts=14, bev=0.5))
        torso.append(ocyl("cell_top", cell + UP * 11.6, 3.5, 1.6, "ArmorDark", UP, verts=14, bev=0.4, radius2=2.6))
        torso.append(ocyl("cell_base", cell - UP * 11.8, 3.6, 2.0, "ArmorDark", UP, verts=14, bev=0.4))
        for i in range(4):
            torso.append(ocyl("cell_rib", cell + UP * (-7.5 + i * 5.0), 4.6, 0.9, "ArmorDark", UP, verts=14, bev=0.15))
        torso.append(ocyl("exhaust", pk + Vector((sx * 4.2, -1.5, -14.6)), 2.2, 3.4, "Rubber", UP, verts=10, bev=0.3, radius2=2.6))
        # Coolant hose from the cell top round the flank to the chest.
        torso.append(tube("hose", [cell + Vector((sx * 1.5, 1.0, 12.0)), cell + Vector((sx * 5.5, 5.0, 13.5)),
                                   s5 + Vector((sx * 20.5, -7.0, 2.0)), s5 + Vector((sx * 22.0, 3.0, -9.0)),
                                   s5 + Vector((sx * 18.5, 10.0, -13.0))], 1.15, "Rubber", sides=8, steps=5))
        torso.append(ocyl("hose_fit", s5 + Vector((sx * 18.2, 10.6, -13.2)), 1.6, 2.2, "Steel", Vector((sx * -0.5, 0.7, -0.3)), verts=8, bev=0.2))
    roll = pk + Vector((0, -0.5, 17.6))
    torso.append(ocyl("roll", roll, 4.4, 25.0, "Canvas", X, verts=14, bev=0.9))
    for sx in (1, -1):
        torso.append(ocyl("roll_strap", roll + X * sx * 7.0, 4.65, 1.6, "Rubber", X, verts=14, bev=0.1))
    torso += bolt("b_pack", pk + Vector((6.0, -4.9, 11.0)), BACK) + bolt("b_pack", pk + Vector((-6.0, -4.9, 11.0)), BACK)
    torso += bolt("b_pack", pk + Vector((6.2, -4.6, -9.0)), BACK) + bolt("b_pack", pk + Vector((-6.2, -4.6, -9.0)), BACK)
    for sgn in (1, -1):
        torso += groove("g_pec", T, 0.52, sgn * 14, 0.8, sgn * 22, 3.0, plate_d(1.9))
        torso += groove("g_pec2", T, 0.58, sgn * 22, 0.58, sgn * 52, 3.0, plate_d(1.9))
        torso += bolt("b_pec", T.surface(0.79, sgn * 52, 3.0, plate_d(1.95)), T.out(sgn * 52))
        torso += bolt("b_pec", T.surface(0.52, sgn * 52, 3.0, plate_d(1.95)), T.out(sgn * 52))
        torso += groove("g_back", T, 0.36, 180 + sgn * 50, 0.8, 180 + sgn * 44, 2.6, plate_d(1.5))
    P["spine_05"] = torso

    # ---- Abdomen --------------------------------------------------------------------------------------------------
    belly_pts = pts(["spine_01", "spine_02", "spine_03", "spine_04"])
    Bl = Limb(belly_pts, (0, 2.6, 101.0), (0, 2.6, 121.0), FWD, nt=6, na=24)
    wd = superellipse(16.5, 11.5, 2.6)
    abd = [Bl.panel("midriff", 0.0, 1.0, 0, 360, "Undersuit", pad=1.0, thick=1.5, design=lambda t, a: wd(a), res=(3, 20), bev=0.2)]
    for i in range(3):
        t0 = 0.06 + i * 0.3
        abd.append(Bl.panel("ab_%d" % i, t0, t0 + 0.27, -50, 50, "ArmorDark", pad=2.0, thick=1.1, design=lambda t, a: wd(a) + 1.0,
                            res=(2, 8), chamfer=0.35, bev=0.3))
        abd.append(Bl.panel("lumbar_%d" % i, t0, t0 + 0.27, 140, 220, "ArmorDark", pad=1.8, thick=1.0, design=lambda t, a: wd(a) + 0.8,
                            res=(2, 6), chamfer=0.35, bev=0.3))
    for sgn in (1, -1):
        abd.append(Bl.panel("kidney", 0.1, 0.9, sgn * 62, sgn * 128, "Webbing", pad=1.8, thick=0.9, design=lambda t, a: wd(a) + 0.8,
                            res=(3, 5), chamfer=0.25, bev=0.2))
    P["spine_03"] = abd

    # ---- Pelvis ---------------------------------------------------------------------------------------------------
    pv = j["pelvis"]
    hip_pts = pts(["pelvis", "thigh_l", "thigh_r", "spine_01"], clip=lambda c: c.z > 80)
    H = Limb(hip_pts, (0, 2.0, 84.0), (0, 2.0, 103.0), FWD, nt=6, na=24)
    hd = superellipse(18.5, 12.5, 2.8)
    pelvis = [H.panel("belt", 0.62, 0.98, 0, 360, "Webbing", pad=1.6, thick=1.4, design=lambda t, a: hd(a), res=(2, 24), bev=0.3),
              H.panel("brief", 0.28, 0.66, 0, 360, "Undersuit", pad=0.9, thick=1.2, design=lambda t, a: hd(a) - 1.2, res=(2, 20), bev=0.2)]
    pelvis.append(obox("buckle", H.surface(0.8, 0, 1.6, lambda t, a: hd(a)) + FWD * 0.5, (6.5, 1.6, 4.6), "ArmorDark", bev=0.5, segments=2))
    pelvis.append(obox("buckle_plate", H.surface(0.8, 0, 1.6, lambda t, a: hd(a)) + FWD * 1.4, (3.6, 0.5, 2.6), "Steel", bev=0.15))
    # Cod plate and rear plate.
    pelvis.append(H.panel("cod", 0.02, 0.62, -26, 26, "ArmorPlate", pad=2.4, thick=1.4, design=lambda t, a: hd(a) + 0.6, res=(4, 4),
                          chamfer=0.3, taper=-0.5, bev=0.4))
    pelvis.append(H.panel("seat", 0.12, 0.62, 148, 212, "ArmorDark", pad=2.2, thick=1.2, design=lambda t, a: hd(a) + 0.4, res=(3, 5),
                          chamfer=0.35, bev=0.35))
    for sgn in (1, -1):
        # Tassets: hip plates hanging off the belt, standing proud of the thigh.
        pelvis.append(H.panel("tasset", -0.1, 0.68, sgn * 58, sgn * 122, "ArmorPlate", pad=3.4, thick=1.4, design=lambda t, a: hd(a) + 2.4,
                              res=(4, 5), chamfer=0.4, bev=0.4))
        pelvis += bolt("b_tasset", H.surface(0.58, sgn * 90, 3.4, lambda t, a: hd(a) + 2.45), H.out(sgn * 90))
        # Belt pouches at the back quarters.
        for i, ang in enumerate((142, 166)):
            c = H.surface(0.74, sgn * ang, 1.6, lambda t, a: hd(a))
            pelvis += pouch("belt_pouch", c + H.out(sgn * ang) * 1.6, (5.2, 3.2, 6.4), H.out(sgn * ang))
    # Front of the belt: admin pouches either side of the buckle, and two grenades on the soldier's left hip.
    for sgn in (1, -1):
        c = H.surface(0.78, sgn * 30, 1.6, lambda t, a: hd(a))
        pelvis += pouch("admin_pouch", c + H.out(sgn * 30) * 1.4, (5.6, 2.6, 5.4), H.out(sgn * 30))
    for i, ang in enumerate((-52, -70)):
        c = H.surface(0.6, ang, 3.4, lambda t, a: hd(a) + 2.4) + H.out(ang) * 2.6 + UP * 4.4
        pelvis.append(cap("frag_%d" % i, c, (2.5, 2.5, 3.0), "Webbing", lambda l: True, thick=0.0, segs=(10, 8), power=(0.9, 0.9, 0.9), bev=0))
        pelvis.append(ocyl("frag_ring_%d" % i, c, 2.6, 0.5, "ArmorDark", UP, verts=10, bev=0.1))
        pelvis.append(ocyl("frag_fuze_%d" % i, c + UP * 3.4, 1.0, 1.6, "Steel", UP, verts=8, bev=0.15))
        pelvis.append(obox("frag_lever_%d" % i, c + UP * 2.2 + H.out(ang) * 1.9, (0.9, 0.4, 3.6), "Steel", H.out(ang), bev=0.1))
    P["pelvis"] = pelvis

    # ---- Arms -----------------------------------------------------------------------------------------------------
    def upper_arm(s, big):
        """Built in left space; the right arm passes mirrored inputs and is mirrored back by the caller."""
        sh, el = jl["upperarm_" + s], jl["lowerarm_" + s]
        arm_pts = [Vector((abs(p.x), p.y, p.z)) for p in pts(["upperarm_" + s, "upperarm_twist_01_" + s, "upperarm_twist_02_" + s])]
        A = Limb(arm_pts, sh, el, FWD, nt=8, na=20)
        ad = superellipse(6.6, 6.9, 2.6)
        parts = [A.panel("arm_sleeve", 0.3, 0.9, 0, 360, "Undersuit", pad=0.8, thick=1.0, res=(3, 14), bev=0.15)]
        parts.append(A.panel("bicep", 0.5, 0.9, 5, 175, "ArmorPlate", pad=1.9, thick=1.2, design=lambda t, a: ad(a) + 1.0, res=(3, 7),
                             chamfer=0.35, bev=0.35))
        parts.append(A.panel("arm_band", 0.6, 0.72, 0, 360, "Webbing", pad=1.2, thick=0.6, design=lambda t, a: ad(a) + 0.1, res=(1, 14), bev=0.1))
        out = (A.s * 0.8 + UP * 0.6).normalized()
        base = sh + A.s * 1.0 + UP * 1.0
        bx, by, bz = (out.cross(FWD)).normalized(), FWD, out
        bx = by.cross(bz).normalized()
        rr = (14.5, 14.0, 12.5) if big else (11.5, 11.5, 10.0)
        # Shoulder cup: dark, fitted; then the pauldron lames over it.
        parts.append(cap("cup", base, (rr[0] - 2.2, rr[1] - 2.2, rr[2] - 2.0), "ArmorDark", lambda l: l.z > -0.25, (bx, by, bz),
                         thick=1.0, segs=(14, 8), power=(0.85, 0.85, 0.8), bev=0.25))
        parts.append(cap("pauldron", base, rr, "ArmorTeam", lambda l: l.z > 0.02 and l.x > -0.9, (bx, by, bz), thick=1.5, segs=(16, 10),
                         power=(0.72, 0.78, 0.7), bev=0.45, sharp=32))
        low = base - out * 1.0 + A.axis * 3.2
        parts.append(cap("pauldron_lame", low, (rr[0] - 0.8, rr[1] - 0.6, rr[2] - 1.5), "ArmorPlate",
                         lambda l: -0.38 < l.z < 0.3 and l.x > -0.2, (bx, by, bz), thick=1.3, segs=(16, 10), power=(0.72, 0.78, 0.7),
                         bev=0.4, sharp=32))
        parts.append(cap("pauldron_rim", base, (rr[0] + 0.5, rr[1] + 0.5, rr[2] + 0.3), "ArmorDark",
                         lambda l: 0.0 < l.z < 0.3 and l.x > -0.9, (bx, by, bz), thick=1.0, segs=(16, 10), power=(0.72, 0.78, 0.7),
                         bev=0.3, sharp=32))
        if big:
            # A raised blast collar along the neck side and a hard-point rail on top.
            parts.append(obox("blast_lip", base + bz * (rr[2] - 1.0) - bx * (rr[0] * 0.55), (2.0, rr[1] * 1.5, 6.5), "ArmorPlate", by,
                              (bz * 0.8 - bx * 0.6), bev=0.7, segments=2, taper=(1.0, 0.8)))
            parts.append(obox("hardpoint", base + bz * (rr[2] + 0.3) + bx * 1.5, (5.5, 7.0, 1.2), "ArmorDark", by, bz, bev=0.35))
            for sy in (1, -1):
                parts += bolt("b_pauldron", base + bz * (rr[2] + 0.95) + bx * 1.5 + by * sy * 2.4, bz, r=0.5)
        else:
            parts.append(obox("pad_ridge", base + bz * (rr[2] - 0.2), (3.0, rr[1] * 1.3, 1.6), "ArmorDark", by, bz, bev=0.5, segments=2))
        parts.append(A.panel("elbow_gasket", 0.86, 1.02, 0, 360, "Rubber", pad=0.9, thick=0.8, res=(1, 12), bev=0.1))
        return parts

    def mirror_joints(jj):
        out = {}
        for name, v in jj.items():
            out[name] = Vector((abs(v.x), v.y, v.z)) if name.endswith(("_l", "_r")) else v
        return out

    jl = mirror_joints(j)
    P["upperarm_l"] = upper_arm("l", True)
    P["upperarm_r"] = mirror_parts(upper_arm("r", False))

    el, wr = j["lowerarm_l"], j["hand_l"]
    fore_pts = pts(["lowerarm_l", "lowerarm_twist_01_l", "lowerarm_twist_02_l"])
    F = Limb(fore_pts, el, wr, Vector((0, 1, 0.6)), nt=8, na=20)
    fd = superellipse(5.4, 5.2, 2.6)

    def vam(t, a):
        return fd(a) * (1.12 - 0.22 * t)
    fore = [F.panel("fore_sleeve", 0.12, 0.93, 0, 360, "Undersuit", pad=0.7, thick=0.9, res=(3, 14), bev=0.15)]
    fore.append(F.panel("vambrace", 0.2, 0.9, -15, 195, "ArmorTeam", pad=1.6, thick=1.2, design=lambda t, a: vam(t, a) + 0.9, res=(5, 9),
                        chamfer=0.25, bev=0.35))
    fore.append(F.panel("vambrace_in", 0.28, 0.84, 205, 335, "ArmorDark", pad=1.1, thick=0.9, design=lambda t, a: vam(t, a) + 0.2, res=(3, 5),
                        chamfer=0.3, bev=0.25))
    fore.append(F.panel("cuff", 0.84, 0.97, 0, 360, "ArmorDark", pad=1.3, thick=1.0, design=lambda t, a: fd(a) * 0.92 + 0.6, res=(1, 14), bev=0.25))
    fore.append(cap("elbow", el + F.axis * 1.0 - FWD * 2.2 + F.s * 0.8, (6.4, 6.6, 6.8), "ArmorPlate", lambda l: l.y < 0.15 and l.z > -0.75,
                    (F.s, F.f, -F.axis), thick=1.3, segs=(12, 8), power=(0.75, 0.75, 0.8), bev=0.4))
    # Wrist computer on the top of the vambrace.
    wc = F.surface(0.56, 95, 1.6, lambda t, a: vam(t, a) + 0.9)
    fore.append(obox("wristpad", wc + F.out(95) * 0.5, (3.6, 1.2, 6.5), "ArmorDark", F.out(95), F.axis, bev=0.3, segments=2))
    fore.append(obox("wristpad_screen", wc + F.out(95) * 1.12, (2.4, 0.2, 3.6), "TeamGlow", F.out(95), F.axis, bev=0))
    fore += groove("g_vam", F, 0.26, 40, 0.86, 40, 1.6, lambda t, a: vam(t, a) + 0.9)
    fore += groove("g_vam", F, 0.26, 150, 0.86, 150, 1.6, lambda t, a: vam(t, a) + 0.9)
    fore += bolt("b_vam", F.surface(0.3, 20, 1.6, lambda t, a: vam(t, a) + 0.95), F.out(20), r=0.35)
    fore += bolt("b_vam", F.surface(0.82, 20, 1.6, lambda t, a: vam(t, a) + 0.95), F.out(20), r=0.35)
    P["lowerarm_l"] = fore

    # ---- Hands: a knuckle guard on the back of the glove (rigid on the hand bone; the fingers stay free). ----------
    hd0, mid1 = j["hand_l"], j["middle_01_l"]
    hdir = (mid1 - hd0).normalized()
    across = (j["pinky_01_l"] - j["index_01_l"]).normalized()
    hback = hdir.cross(across).normalized()
    if hback.x < 0:
        hback = -hback
    hc0 = hd0 + (mid1 - hd0) * 0.55 + hback * 1.7
    hand = [obox("hand_plate", hc0, (7.0, 1.0, 6.4), "ArmorPlate", hback, -hdir, bev=0.45, segments=2, taper=(0.86, 0.86)),
            obox("knuckle_bar", mid1 + hback * 1.9 + hdir * 0.2, (7.8, 1.3, 2.0), "ArmorDark", hback, -hdir, bev=0.5, segments=2),
            obox("wrist_tab", hd0 + hback * 1.9 + hdir * 0.8, (5.4, 0.9, 2.4), "ArmorDark", hback, -hdir, bev=0.35)]
    for sgn in (-1, 1):
        hand += bolt("b_hand", hc0 + across * sgn * 2.6 + hback * 0.7, hback, r=0.32)
    P["hand_l"] = hand

    # ---- Legs -----------------------------------------------------------------------------------------------------
    hip, knee, ankle, ball = j["thigh_l"], j["calf_l"], j["foot_l"], j["ball_l"]
    thigh_pts = pts(["thigh_l", "thigh_twist_01_l", "thigh_twist_02_l"])
    Th = Limb(thigh_pts, hip, knee, FWD, nt=10, na=24)

    def td(t, a):
        return superellipse(11.2 - 2.6 * t, 11.6 - 2.6 * t, 2.7)(a)
    thigh = [Th.panel("thigh_sleeve", 0.14, 0.93, 0, 360, "Undersuit", pad=0.9, thick=1.2, res=(4, 18), bev=0.15)]
    thigh.append(Th.panel("cuisse", 0.2, 0.86, -62, 78, "ArmorPlate", pad=2.4, thick=1.6, design=lambda t, a: td(t, a) + 1.4, res=(6, 8),
                          chamfer=0.3, taper=0.15, bev=0.45))
    thigh.append(Th.panel("cuisse_side", 0.3, 0.84, 84, 150, "ArmorPlate", pad=2.0, thick=1.3, design=lambda t, a: td(t, a) + 0.9, res=(5, 5),
                          chamfer=0.35, bev=0.4))
    thigh.append(Th.panel("ham", 0.3, 0.8, 158, 262, "ArmorDark", pad=1.6, thick=1.1, design=lambda t, a: td(t, a) + 0.3, res=(4, 6),
                          chamfer=0.35, bev=0.3))
    for t in (0.36, 0.6):
        thigh.append(Th.panel("thigh_strap", t, t + 0.07, 0, 360, "Webbing", pad=2.9, thick=0.6, design=lambda t, a: td(t, a) + 1.9,
                              res=(1, 18), bev=0.1))
    pc = Th.surface(0.5, 112, 2.9, lambda t, a: td(t, a) + 1.9)
    thigh += pouch("thigh_pouch", pc + Th.out(112) * 1.9, (7.4, 3.8, 10.5), Th.out(112), -Th.axis)
    thigh += groove("g_cuisse", Th, 0.26, 8, 0.8, 8, 2.4, lambda t, a: td(t, a) + 1.4)
    thigh += groove("g_cuisse2", Th, 0.42, -50, 0.42, 66, 2.4, lambda t, a: td(t, a) + 1.4)
    for ang in (-48, 64):
        thigh += bolt("b_cuisse", Th.surface(0.27, ang, 2.4, lambda t, a: td(t, a) + 1.45), Th.out(ang))
        thigh += bolt("b_cuisse", Th.surface(0.78, ang * 0.8, 2.4, lambda t, a: td(t, a) + 1.45), Th.out(ang * 0.8))
    P["thigh_l"] = thigh

    shin_pts = pts(["calf_l", "calf_twist_01_l", "calf_twist_02_l"])
    C = Limb(shin_pts, knee, ankle, FWD, nt=10, na=24)

    def cd(t, a):
        bulge = math.sin(min(1.0, max(0.0, t * 1.15)) * math.pi)
        return superellipse(8.0 + 1.5 * bulge - 1.2 * t, 8.4 + 1.8 * bulge - 1.2 * t, 2.7)(a)
    shin = [C.panel("shin_sleeve", 0.06, 0.94, 0, 360, "Undersuit", pad=0.9, thick=1.2, res=(4, 18), bev=0.15)]
    shin.append(C.panel("greave", 0.16, 0.9, -92, 92, "ArmorPlate", pad=2.2, thick=1.6, design=lambda t, a: cd(t, a) + 1.5, res=(6, 9),
                        chamfer=0.22, bev=0.45))
    shin.append(C.panel("calf_plate", 0.14, 0.8, 100, 260, "ArmorDark", pad=1.7, thick=1.2, design=lambda t, a: cd(t, a) + 0.6, res=(5, 8),
                        chamfer=0.3, bev=0.35))
    shin.append(C.panel("ankle_cuff", 0.84, 0.98, 0, 360, "ArmorDark", pad=1.8, thick=1.2, design=lambda t, a: 6.6, res=(1, 16), bev=0.3))
    kc = knee + Vector((0, 5.6, -1.0))
    shin.append(cap("knee", kc, (7.4, 6.2, 8.2), "ArmorTeam", lambda l: l.y > -0.1 and abs(l.z) < 0.82, thick=1.5, segs=(14, 10),
                    power=(0.72, 0.8, 0.75), bev=0.45, sharp=32))
    shin.append(cap("knee_boss", kc + Vector((0, 2.4, 0.4)), (4.2, 4.4, 4.6), "ArmorDark", lambda l: l.y > 0.1, thick=1.0, segs=(10, 8),
                    power=(0.7, 0.8, 0.7), bev=0.3))
    for sx in (1, -1):
        shin.append(ocyl("knee_pivot", knee + Vector((sx * 6.9, 0.8, -0.4)), 2.5, 1.7, "ArmorDark", X * sx, verts=10, bev=0.3))
        shin.append(ocyl("knee_pin", knee + Vector((sx * 7.9, 0.8, -0.4)), 1.0, 0.5, "Steel", X * sx, verts=8, bev=0.1))
    # Outer-calf actuator: a piston between two housings.
    p0 = C.surface(0.3, 96, 1.7, lambda t, a: cd(t, a) + 1.3)
    p1 = C.surface(0.74, 96, 1.7, lambda t, a: cd(t, a) + 1.0)
    pa = (p1 - p0).normalized()
    shin.append(ocyl("piston_body", p0 + pa * 4.0, 1.25, 8.5, "ArmorDark", pa, verts=10, bev=0.2))
    shin.append(ocyl("piston_rod", (p0 + p1) * 0.5 + pa * 3.0, 0.6, (p1 - p0).length * 0.62, "Steel", pa, verts=8, bev=0))
    shin.append(obox("piston_top", p0, (3.0, 2.4, 3.0), "ArmorDark", C.out(96), -pa, bev=0.4))
    shin.append(obox("piston_foot", p1, (2.8, 2.2, 2.6), "ArmorDark", C.out(96), -pa, bev=0.4))
    shin.append(obox("greave_light", C.surface(0.3, 0, 2.2, lambda t, a: cd(t, a) + 1.52), (0.9, 0.4, 3.4), "TeamGlow", FWD, UP, bev=0))
    shin += groove("g_greave", C, 0.22, -40, 0.86, -30, 2.2, lambda t, a: cd(t, a) + 1.5)
    shin += groove("g_greave", C, 0.22, 40, 0.86, 30, 2.2, lambda t, a: cd(t, a) + 1.5)
    shin += groove("g_greave2", C, 0.62, -80, 0.62, 80, 2.2, lambda t, a: cd(t, a) + 1.5)
    for ang in (-74, 74):
        shin += bolt("b_greave", C.surface(0.24, ang, 2.2, lambda t, a: cd(t, a) + 1.55), C.out(ang))
        shin += bolt("b_greave", C.surface(0.84, ang * 0.9, 2.2, lambda t, a: cd(t, a) + 1.55), C.out(ang * 0.9))
    P["calf_l"] = shin

    foot_pts = pts(["foot_l", "ball_l"])
    boot = ba.plate("boot", foot_pts, 1.7, lambda c, n: "Rubber" if c.z < 2.6 else "ArmorDark", cuts=[((0, 0, -0.6), (0, 0, -1))],
                    simplify=9, bevel=0.5)
    cx = (ankle.x + ball.x) * 0.5
    foot = [boot,
            obox("sole", Vector((cx + 0.3, (ankle.y + ball.y) * 0.5 + 2.6, 0.3)), (11.6, 31.0, 2.8), "Rubber", bev=0.7, segments=2, taper=(0.86, 1.0)),
            obox("toecap", ball + Vector((0, 5.6, 3.3)), (10.4, 8.0, 5.6), "ArmorPlate", bev=1.5, segments=2, taper=(0.78, 0.62)),
            obox("instep", Vector((cx + 0.6, ball.y - 3.5, 7.2)), (9.4, 9.0, 3.4), "ArmorPlate", Vector((0, 1, -0.55)), Vector((0, 0.55, 1)),
                 bev=0.9, segments=2, taper=(0.9, 0.7)),
            obox("heel", ankle + Vector((0.2, -4.8, -3.2)), (9.6, 5.0, 8.6), "ArmorPlate", -FWD, bev=1.2, segments=2, taper=(0.8, 0.8))]
    for sx in (1, -1):
        foot.append(obox("ankle_guard", ankle + Vector((sx * 5.4, 0.6, -2.0)), (1.6, 7.0, 6.4), "ArmorDark", FWD, bev=0.5, segments=2))
        foot += bolt("b_ankle", ankle + Vector((sx * 6.3, 0.6, -1.2)), X * sx, r=0.5)
    for i in range(4):
        foot.append(obox("tread", Vector((cx + 0.3, ankle.y - 6.0 + i * 7.6, -0.9)), (11.0, 2.6, 0.9), "Rubber", bev=0.2))
    P["foot_l"] = foot
    return P


MIRRORED = {"hand_l": "hand_r", "lowerarm_l": "lowerarm_r", "thigh_l": "thigh_r", "calf_l": "calf_r", "foot_l": "foot_r"}
ORDER = ["head", "spine_05", "spine_03", "pelvis", "upperarm_l", "upperarm_r", "lowerarm_l", "hand_l", "thigh_l", "calf_l", "foot_l"]


def join_all(pieces, prefix):
    """Joins every bone's parts into one object, in ORDER. Returns (object, {bone: (first face, end face)})."""
    ranges = {}
    joined = []
    start = 0
    for bone in ORDER:
        obj = k.join("%s_%s" % (prefix, bone), pieces[bone])
        n = len(obj.data.polygons)
        ranges[bone] = (start, start + n)
        start += n
        joined.append(obj)
    return k.join(prefix, joined), ranges


def load_body(skin):
    bpy.ops.wm.obj_import(filepath=skin, forward_axis="Y", up_axis="Z", global_scale=1.0)
    body = bpy.context.selected_objects[0]
    body.matrix_world = Matrix.Identity(4)
    body.data.materials.clear()
    body.data.materials.append(k.material("Undersuit"))
    for poly in body.data.polygons:
        poly.use_smooth = True
    return body


VIEWS = [((0.5, 1.0, 0.12), False), ((-0.55, -1.0, 0.18), False), ((1.0, 0.05, 0.05), False)]


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        sys.exit(__doc__)
    k.reset_scene()
    joints = ba.load_bones(args[0])
    verts = ba.load_skin(args[1], joints)
    if "--render" in args:
        import sb_render
        out = args[args.index("--render") + 1]
        os.makedirs(out, exist_ok=True)
        pieces = build(verts, joints, high="--high-preview" in args)
        low, ranges = join_all(pieces, "SOLDIER")
        shown = [low]
        for src, dst in MIRRORED.items():
            a, b = ranges[src]
            import bake_weapons as bkw
            shown.append(mirrored(bkw.extract(low, "tmp_" + src, [(a, b)]), "M_" + dst))
            bpy.data.objects.remove(bpy.data.objects["tmp_" + src], do_unlink=True)
        print("soldier: %d tris (unique), %d with mirrored limbs" % (k.triangle_count(low), sum(k.triangle_count(o) for o in shown)))
        for bone, (a, b) in ranges.items():
            print("  %s: %d faces" % (bone, b - a))
        load_body(args[1])
        sb_render.render_views(shown, os.path.join(out, "SB_Soldier.png"), VIEWS, resolution=(700, 1100), floor=True)
        return
    import bake_soldier
    bake_soldier.run(args, verts, joints)


if __name__ == "__main__":
    main()
