"""Keel shell kit: hull-section modules that give Keel's rooms an alien silhouette instead of dressed boxes.

Earlier Keel passes laid panels on the faces of the gameplay blocks, so every room kept the block's rectangle. This kit
builds the room's cross-section itself: a kick skirt that leans out, a wall that leans in, a ribbed haunch that curves
into the ceiling, and a light trough down the spine. Collision stays the gameplay blocks; these pieces are visual only.

Units: 1 Blender unit = 1 cm, Unreal mesh space (X right, Y forward, Z up), like sb_kit.

Grid: 400 cm bays along a wall. A wall bay's pivot is the wall line at floor level, centred on the bay (Y -200..200);
local +X points into the room. A corridor built from it is 1124 cm wall to wall: each haunch ends 362 cm into the room,
and the 400 cm ceiling spine fills the middle. Ribs sit on the bay joints (Y = 0 of the rib).

Pieces (all SM_SB_Kit_KeelShell_*):
  WallBay        skirt, crease light, leaning wall with chamfered plates and inlay, ribbed haunch to the ceiling line
  WallBayVent    the same bay with a louvred vent and a violet under-glow in the lower plates
  WallBayWindow  the same bay with a framed window opening (two faceted mullions) in place of the plates
  Rib            heavy structural rib that follows the whole profile, thick at the foot and the knee, violet vein
  RibBeam        the rib's ceiling crossbeam over the 400 cm spine
  CeilingSpine   400 x 400 ceiling with a recessed light trough broken by louvres
  FloorPlate     400 x 400 deck of four chamfered plates
  FloorGrate     400 x 400 grated deck over a dark sub-floor with violet under-glow strips
  EdgeStrip      400 cm fascia for walkable edges and ramp sides, steel lip and cyan guide line
  Pillar         600 cm faceted tapering column, flared foot and capital, violet slits, rose band
  Portal         angular door surround for a 500 x 350 opening, stepped jamb, violet inner line, rose keystone

Materials (slots, palette keys): ShellHull, ShellPlate, ShellSteel, ShellGrate, ShellGlowCyan, ShellGlowBlue, ShellGlowAmber.
Most solid parts are bevelled and then subdivided, so edges and corners come out rounded like moulded armour.

Run headless:  blender --background --factory-startup --python Art/blender/build_keel_shell_kit.py [-- --render]
or with the bpy module: python Art/blender/build_keel_shell_kit.py [--render]
Exports FBX to Art/Export/MapKit (+ keel_shell_report.txt), palette keys to Art/Export/palette.json. --render also
writes preview images to docs/playtests/2026-11-25_keel_shell/. Import: Tools/ImportKeelShell.bat.
"""
import json
import math
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402  (bpy before bmesh when run as a module)
import bmesh  # noqa: E402
from mathutils import Vector  # noqa: E402

import sb_kit  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Art/Export/MapKit"
PREFIX = "SM_SB_Kit_KeelShell_"

# name: (base colour linear, metallic, roughness, emissive strength). Engine instances come from these values.
SHELL_PALETTE = {
    # Target (Vincent's reference, 2026-11-25): glossy deep-purple armour metal with sharp highlights, a dark room, small
    # blue-cyan emissive accents and amber back-lit grilles. Metallic and glossy on purpose: the look lives in the
    # reflections, so it needs Lumen reflections (or SSR) and a dark exposure to read.
    "ShellHull": ((0.100, 0.028, 0.150), 0.90, 0.22, 0.0),      # deep purple hull metal: the room's mass
    "ShellPlate": ((0.270, 0.110, 0.360), 0.90, 0.17, 0.0),     # lighter lilac armour plates: carry the highlights
    "ShellSteel": ((0.100, 0.070, 0.125), 1.00, 0.34, 0.0),     # dark violet-gunmetal trim and frames
    "ShellGrate": ((0.018, 0.016, 0.024), 0.80, 0.45, 0.0),     # near-black grate and sub-floor
    "ShellGlowCyan": ((0.30, 0.62, 1.00), 0.0, 0.30, 8.0),     # guide lines on walkable edges, crystal tips
    "ShellGlowBlue": ((0.12, 0.28, 1.00), 0.0, 0.30, 6.0),      # deep blue accent veins and under-glow
    "ShellGlowAmber": ((1.00, 0.52, 0.14), 0.0, 0.30, 7.0),     # back-lit grilles
}
sb_kit.PALETTE.update(SHELL_PALETTE)

# ---- The hull profile (X into the room, Z up) ----------------------------------------------------------------------
FLOOR_X = 55.0          # where the skirt meets the floor
CREASE = (10.0, 100.0)  # the skirt leans back to here, then the wall leans in
WALL_TOP = (130.0, 560.0)
CEIL_Z = 760.0
HAUNCH_END_X = 362.0
BAY = 400.0
HALF = BAY / 2


def _bezier(p0, p1, p2, n):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) for t in (i / n for i in range(n + 1))]


def _lerp(a, b, n):
    return [(a[0] + (b[0] - a[0]) * i / n, a[1] + (b[1] - a[1]) * i / n) for i in range(n + 1)]


def haunch_points(n=14):
    d = Vector((WALL_TOP[0] - CREASE[0], WALL_TOP[1] - CREASE[1])).normalized()
    t = (CEIL_Z - WALL_TOP[1]) / d.y
    ctrl = (WALL_TOP[0] + d.x * t, CEIL_Z)
    return _bezier(WALL_TOP, ctrl, (HAUNCH_END_X, CEIL_Z), n)


def profile_segments():
    """The inner hull surface as (material, points) runs, bottom to top."""
    skirt = _lerp((FLOOR_X, 0.0), CREASE, 3)
    wall = _lerp(CREASE, WALL_TOP, 6)
    return [("ShellSteel", skirt), ("ShellHull", wall), ("ShellHull", haunch_points())]


def full_profile():
    pts = []
    for _, seg in profile_segments():
        pts += seg if not pts else seg[1:]
    return pts


def normals_of(pts):
    """Per-point normals pointing into the room (tangent turned -90 degrees in XZ)."""
    out = []
    for i in range(len(pts)):
        a = Vector(pts[max(i - 1, 0)])
        b = Vector(pts[min(i + 1, len(pts) - 1)])
        t = (b - a).normalized()
        out.append(Vector((t.y, -t.x)))
    return out


# ---- Mesh helpers ---------------------------------------------------------------------------------------------------
def _obj(name, bm, mat, bevel=0.0, segments=1, angle=35.0, smooth=False):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb_kit._link(name, me)
    if smooth:
        for p in me.polygons:
            p.use_smooth = True
    return sb_kit._finish(obj, mat, bevel, segments, angle)


def loft(name, sections, mat, bevel=0.0, cap=True, smooth=False, closed=True):
    """Bridges a list of equal-length vertex rings (lists of 3D points) with quads; caps both ends."""
    bm = bmesh.new()
    rings = [[bm.verts.new(p) for p in ring] for ring in sections]
    n = len(sections[0])
    for a, b in zip(rings, rings[1:]):
        for i in range(n if closed else n - 1):
            j = (i + 1) % n
            bm.faces.new((a[i], a[j], b[j], b[i]))
    if cap and closed:
        bm.faces.new(rings[0])
        bm.faces.new(list(reversed(rings[-1])))
    return _obj(name, bm, mat, bevel, smooth=smooth)


def sweep_profile(name, pts, y0, y1, thickness, mat, offset=0.0, bevel=0.8):
    """Extrudes the open polyline pts (XZ) along Y from y0 to y1 as a shell `thickness` thick behind the surface
    (offset > 0 lifts it into the room)."""
    ns = normals_of(pts)
    front = [Vector((p[0], 0, p[1])) + Vector((n.x, 0, n.y)) * offset for p, n in zip(pts, ns)]
    back = [f - Vector((n.x, 0, n.y)) * thickness for f, n in zip(front, ns)]
    ring_pts = front + list(reversed(back))
    sections = [[Vector((p.x, y, p.z)) for p in ring_pts] for y in (y0, y1)]
    return loft(name, sections, mat, bevel=bevel)


def rib_sweep(name, pts, depth, width, mat, sink=6.0, chamfer=0.22, bevel=1.0):
    """A structural member following pts (XZ) out of the surface; depth(i) and width(i) per path point. The section
    is a chamfered trapezoid (narrower on its room face), centred on Y = 0."""
    ns = normals_of(pts)
    sections = []
    for i, (p, n) in enumerate(zip(pts, ns)):
        base = Vector((p[0], 0, p[1]))
        nv = Vector((n.x, 0, n.y))
        d, w = depth(i), width(i)
        c = w * chamfer
        inner = base - nv * sink
        outer = base + nv * d
        mid = base + nv * (d * 0.72)
        sections.append([inner + Vector((0, -w / 2, 0)), mid + Vector((0, -w / 2, 0)), outer + Vector((0, -w / 2 + c, 0)),
                         outer + Vector((0, w / 2 - c, 0)), mid + Vector((0, w / 2, 0)), inner + Vector((0, w / 2, 0))])
    return loft(name, sections, mat, bevel=bevel)


def planar_plate(name, origin, u, v, outline, thickness, mat, bevel=1.2, normal=None):
    """A flat plate: outline is a list of (s, t) in cm along unit vectors u and v from origin; extruded along the
    plane normal (u x v unless given) by thickness."""
    nrm = normal or u.cross(v).normalized()
    bm = bmesh.new()
    verts = [bm.verts.new(origin + u * s + v * t) for s, t in outline]
    face = bm.faces.new(verts)
    ext = bmesh.ops.extrude_face_region(bm, geom=[face])
    moved = [e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, vec=nrm * thickness, verts=moved)
    return _obj(name, bm, mat, bevel, segments=2, angle=40)


def chamfer_rect(s0, s1, t0, t1, c, corners=(1, 1, 1, 1)):
    """Rectangle outline (s, t) with 45-degree corner cuts of size c (corners: s0t0, s1t0, s1t1, s0t1)."""
    pts = []
    for (s, t), on, (ds, dt) in zip(((s0, t0), (s1, t0), (s1, t1), (s0, t1)), corners,
                                    ((1, 1), (-1, 1), (-1, -1), (1, -1))):
        if on:
            # entering and leaving the corner, keeping counter-clockwise order
            if (s, t) == (s0, t0):
                pts += [(s0, t0 + c), (s0 + c, t0)]
            elif (s, t) == (s1, t0):
                pts += [(s1 - c, t0), (s1, t0 + c)]
            elif (s, t) == (s1, t1):
                pts += [(s1, t1 - c), (s1 - c, t1)]
            else:
                pts += [(s0 + c, t1), (s0, t1 - c)]
        else:
            pts.append((s, t))
    return pts


def wall_frame():
    """Origin, along-wall-up unit (u), along-bay unit (v) and the room normal of the leaning wall plane."""
    a = Vector((CREASE[0], 0, CREASE[1]))
    b = Vector((WALL_TOP[0], 0, WALL_TOP[1]))
    u = (b - a).normalized()
    v = Vector((0, 1, 0))
    n = Vector((u.z, 0, -u.x))  # into the room
    return a, u, v, n, (b - a).length


def box(name, center, size, mat, bevel=0.6):
    return sb_kit.box(name, center, size, mat, bevel=bevel)


def soft(obj, levels=2, bevel=None, segments=2):
    """Moulded-armour edges: the bevel holds the shape, a subdivision pass rounds every edge and corner."""
    mod = obj.modifiers.get("Bevel")
    if mod is None and bevel:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.limit_method = "ANGLE"
        mod.angle_limit = math.radians(35)
    if mod is not None:
        if bevel:
            mod.width = bevel
        mod.segments = segments
        mod.harden_normals = False
    sub = obj.modifiers.new("Subsurf", "SUBSURF")
    sub.levels = sub.render_levels = levels
    for p in obj.data.polygons:
        p.use_smooth = True
    return obj


# ---- Pieces ---------------------------------------------------------------------------------------------------------
def hull_backing(prefix, include_wall=True):
    parts = []
    for i, (mat, seg) in enumerate(profile_segments()):
        if mat == "ShellHull" and i == 1 and not include_wall:
            continue
        parts.append(sweep_profile(f"{prefix}_back{i}", seg, -HALF, HALF, 22.0, mat, bevel=0.0))
    return parts


def haunch_ribbing(prefix):
    """Three small ribs following the haunch per bay, and a steel lip where the haunch meets the ceiling."""
    pts = haunch_points(12)
    parts = []
    for k, y in enumerate((-100.0, 0.0, 100.0)):
        r = rib_sweep(f"{prefix}_hrib{k}", pts, lambda i: 9.0 + 4.0 * math.sin(math.pi * i / 12), lambda i: 14.0,
                      "ShellHull", sink=4.0, bevel=0.8)
        r.location.y = y
        parts.append(soft(r, bevel=2.0))
    parts.append(box(f"{prefix}_ceillip", (HAUNCH_END_X - 6, 0, CEIL_Z - 4), (12, BAY, 8), "ShellSteel", bevel=0.8))
    return parts


def skirt_details(prefix):
    parts = []
    # Cyan guide line where the skirt meets the floor, on a small steel shelf.
    parts.append(box(f"{prefix}_shelf", (FLOOR_X + 4, 0, 3), (12, BAY, 6), "ShellSteel", bevel=0.8))
    parts.append(box(f"{prefix}_guide", (FLOOR_X + 9, 0, 7), (3, BAY - 4, 2.4), "ShellGlowCyan", bevel=0.0))
    # Dim violet line in the crease, set back so it reads as a glow from a slot.
    parts.append(box(f"{prefix}_crease", (CREASE[0] + 3, 0, CREASE[1] + 1), (4, BAY - 30, 3), "ShellGlowBlue", bevel=0.0))
    # Skirt ribs: short steel fins every 100 cm, so the kick plate has rhythm.
    skirt = _lerp((FLOOR_X, 0.0), CREASE, 4)
    for k, y in enumerate((-150.0, -50.0, 50.0, 150.0)):
        fin = rib_sweep(f"{prefix}_fin{k}", skirt, lambda i: 7.0, lambda i: 10.0, "ShellSteel", sink=3.0, bevel=0.6)
        fin.location.y = y
        parts.append(fin)
    return parts


def wall_plates(prefix):
    a, u, v, n, length = wall_frame()
    parts = []
    lo0, lo1, up0, up1 = 14.0, 200.0, 214.0, length - 12.0
    # Lower pair, split at the middle, small chamfers.
    parts.append(planar_plate(f"{prefix}_lo0", a, u, v, chamfer_rect(lo0, lo1, -186, -8, 18, (1, 0, 0, 1)), 9.0, "ShellPlate", normal=n))
    parts.append(planar_plate(f"{prefix}_lo1", a, u, v, chamfer_rect(lo0, lo1, 8, 186, 18, (0, 1, 1, 0)), 9.0, "ShellPlate", normal=n))
    # Upper plate, full width, big chamfers top and bottom.
    parts.append(planar_plate(f"{prefix}_up", a, u, v, chamfer_rect(up0, up1, -186, 186, 46), 9.0, "ShellPlate", normal=n))
    # Recessed inset on the upper plate, darker, and a steel inlay band across it.
    parts.append(planar_plate(f"{prefix}_inset", a + n * 9.0, u, v, chamfer_rect(up0 + 60, up1 - 70, -120, 120, 30), 2.5, "ShellHull",
                              bevel=0.6, normal=n))
    parts.append(planar_plate(f"{prefix}_inlay", a + n * 11.0, u, v, [(330, -150), (330, 150), (336, 150), (336, -150)][::-1],
                              2.0, "ShellSteel", bevel=0.3, normal=n))
    for obj in parts:
        soft(obj, bevel=2.5)
    # Bolts / clamps on the plate corners: small steel blocks.
    for s, t in ((lo1 - 14, -170), (lo1 - 14, 170), (up0 + 14, -150), (up0 + 14, 150)):
        c = a + u * s + v * t + n * 12
        parts.append(box(f"{prefix}_clamp{s:.0f}_{t:.0f}", tuple(c), (6, 16, 6), "ShellSteel", bevel=0.8))
    return parts


def build_wall_bay(prefix=PREFIX + "WallBay"):
    return hull_backing(prefix) + haunch_ribbing(prefix) + skirt_details(prefix) + wall_plates(prefix)


def build_wall_bay_vent(prefix=PREFIX + "WallBayVent"):
    """The plain bay with an amber back-lit grille in place of the lower plates."""
    a, u, v, n, length = wall_frame()
    parts = hull_backing(prefix) + haunch_ribbing(prefix) + skirt_details(prefix)
    parts.append(soft(planar_plate(f"{prefix}_up", a, u, v, chamfer_rect(214, length - 12, -186, 186, 46), 9.0, "ShellPlate", normal=n),
                      bevel=2.5))
    # Rounded frame (a ring of plate) round the grille.
    outer, inner = chamfer_rect(14, 200, -186, 186, 30), chamfer_rect(34, 180, -160, 160, 18)
    bm = bmesh.new()
    def ring(outline, off):
        return [bm.verts.new(a + u * s_ + v * t_ + n * off) for s_, t_ in outline]
    o0, o1, i0, i1 = ring(outer, 0), ring(outer, 12), ring(inner, -4), ring(inner, 10)
    for k in range(len(o0)):
        j = (k + 1) % len(o0)
        bm.faces.new((o1[k], o1[j], i1[j], i1[k]))
        bm.faces.new((o0[k], o0[j], o1[j], o1[k]))
        bm.faces.new((i1[k], i1[j], i0[j], i0[k]))
    parts.append(soft(_obj(f"{prefix}_frame", bm, "ShellPlate"), bevel=2.0))
    # Amber glow set back behind the bars.
    parts.append(planar_plate(f"{prefix}_glow", a + n * 0.4, u, v, chamfer_rect(30, 184, -164, 164, 16), 0.6, "ShellGlowAmber",
                              bevel=0.0, normal=n))
    # Grille: a lattice of slim rounded bars, horizontal and vertical, in front of the glow.
    for k in range(10):
        s_ = 44 + k * 14.5
        parts.append(soft(planar_plate(f"{prefix}_bar{k}", a + n * 3.0, u, v, [(s_, -160), (s_ + 4.5, -160), (s_ + 4.5, 160), (s_, 160)],
                                       5.0, "ShellSteel", bevel=0.0, normal=n), levels=1, bevel=1.0))
    for k in range(9):
        t_ = -144 + k * 36
        parts.append(soft(planar_plate(f"{prefix}_post{k}", a + n * 2.0, u, v, [(34, t_ - 2.5), (180, t_ - 2.5), (180, t_ + 2.5), (34, t_ + 2.5)],
                                       7.0, "ShellSteel", bevel=0.0, normal=n), levels=1, bevel=1.0))
    return parts


def build_wall_bay_window(prefix=PREFIX + "WallBayWindow"):
    """Skirt, haunch and a framed opening in the wall plane (glass is the engine's window material)."""
    a, u, v, n, length = wall_frame()
    parts = hull_backing(prefix, include_wall=False) + haunch_ribbing(prefix) + skirt_details(prefix)
    s0, s1, t0, t1 = 70.0, length - 40.0, -170.0, 170.0
    # Wall backing around the opening: four strips.
    for nm, (ps0, ps1, pt0, pt1) in (("sill", (0, s0, -HALF, HALF)), ("head", (s1, length, -HALF, HALF)),
                                     ("jl", (s0, s1, -HALF, t0)), ("jr", (s0, s1, t1, HALF))):
        parts.append(planar_plate(f"{prefix}_{nm}", a - n * 22, u, v, [(ps0, pt0), (ps1, pt0), (ps1, pt1), (ps0, pt1)], 22.0, "ShellHull",
                                  bevel=0.0, normal=n))
    # Heavy faceted frame: four chamfered bars standing 24 cm proud.
    fw = 26.0
    outline_outer = chamfer_rect(s0 - fw, s1 + fw, t0 - fw, t1 + fw, 60)
    outline_inner = chamfer_rect(s0, s1, t0, t1, 44)
    bm = bmesh.new()
    def ring(outline, off):
        return [bm.verts.new(a + u * s + v * t + n * off) for s, t in outline]
    o0, o1, i0, i1 = ring(outline_outer, 0), ring(outline_outer, 18), ring(outline_inner, -18), ring(outline_inner, 24)
    m = len(o0)
    for k in range(m):
        j = (k + 1) % m
        bm.faces.new((o1[k], o1[j], i1[j], i1[k]))  # face
        bm.faces.new((o0[k], o0[j], o1[j], o1[k]))  # outer side
        bm.faces.new((i1[k], i1[j], i0[j], i0[k]))  # reveal
    parts.append(soft(_obj(f"{prefix}_frame", bm, "ShellPlate"), bevel=3.0))
    # Inner violet line round the reveal.
    for k, (p, q) in enumerate(zip(outline_inner, outline_inner[1:] + outline_inner[:1])):
        mid_s, mid_t = (p[0] + q[0]) / 2, (p[1] + q[1]) / 2
        seg_len = math.hypot(q[0] - p[0], q[1] - p[1])
        dirv = (u * (q[0] - p[0]) + v * (q[1] - p[1])).normalized()
        bm2 = bmesh.new()
        c = a + u * mid_s + v * mid_t + n * 20
        side = dirv.cross(n).normalized()
        pts = [c - dirv * seg_len / 2 - side * 1.5, c + dirv * seg_len / 2 - side * 1.5,
               c + dirv * seg_len / 2 + side * 1.5, c - dirv * seg_len / 2 + side * 1.5]
        f = bm2.faces.new([bm2.verts.new(p_) for p_ in pts])
        ext = bmesh.ops.extrude_face_region(bm2, geom=[f])
        bmesh.ops.translate(bm2, vec=n * 2.0, verts=[e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)])
        parts.append(_obj(f"{prefix}_line{k}", bm2, "ShellGlowBlue"))
    # Two faceted mullions splitting the opening in three.
    for k, t in enumerate((-57.0, 57.0)):
        parts.append(soft(planar_plate(f"{prefix}_mull{k}", a - n * 10, u, v, [(s0, t - 7), (s1, t - 7), (s1, t + 7), (s0, t + 7)], 26.0,
                                       "ShellSteel", bevel=0.0, normal=n), bevel=2.5))
        parts.append(planar_plate(f"{prefix}_mullcap{k}", a + n * 16, u, v, [(s0, t - 4), (s1, t - 4), (s1, t + 4), (s0, t + 4)], 4.0,
                                  "ShellHull", bevel=0.8, normal=n))
    return parts


def build_rib(prefix=PREFIX + "Rib"):
    pts = full_profile()
    n_pts = len(pts)
    skirt_n = 3
    wall_end = skirt_n + 6

    def depth(i):
        if i <= skirt_n:
            return 70.0 - 30.0 * i / skirt_n                   # heavy foot
        if i <= wall_end:
            return 40.0 - 8.0 * math.sin(math.pi * (i - skirt_n) / 6)  # slimmer mid wall
        k = (i - wall_end) / max(n_pts - 1 - wall_end, 1)
        return 40.0 + 22.0 * math.sin(math.pi * k)           # thick knee in the haunch

    def width(i):
        if i <= skirt_n:
            return 74.0 - 22.0 * i / skirt_n
        if i <= wall_end:
            return 52.0 - 8.0 * math.sin(math.pi * (i - skirt_n) / 6)
        k = (i - wall_end) / max(n_pts - 1 - wall_end, 1)
        return 52.0 + 8.0 * math.sin(math.pi * k)

    parts = [soft(rib_sweep(f"{prefix}_main", pts, depth, width, "ShellHull", bevel=0.0), bevel=4.0)]
    # Rounded shoe over the foot.
    parts.append(soft(rib_sweep(f"{prefix}_shoe", pts[:skirt_n + 1], lambda i: depth(i) + 3, lambda i: width(i) + 6, "ShellSteel",
                                sink=2.0, bevel=0.0), bevel=3.0))
    # Blue vein down the wall part of the rib's face, in a dark channel.
    vein_pts = pts[skirt_n + 1:wall_end]
    parts.append(soft(rib_sweep(f"{prefix}_channel", vein_pts, lambda i: depth(i + skirt_n + 1) + 1.0, lambda i: 16.0, "ShellGrate",
                                sink=2.0, bevel=0.0), levels=1, bevel=1.5))
    parts.append(rib_sweep(f"{prefix}_vein", vein_pts, lambda i: depth(i + skirt_n + 1) + 1.8, lambda i: 5.0, "ShellGlowBlue",
                           sink=2.0, chamfer=0.0, bevel=0.0))
    # Lilac armour plate over the knee.
    knee = pts[wall_end + 2:n_pts - 3]
    parts.append(soft(rib_sweep(f"{prefix}_knee", knee, lambda i: depth(i + wall_end + 2) + 4.0, lambda i: 40.0, "ShellPlate", sink=2.0,
                                bevel=0.0), bevel=3.0))
    return parts


def build_rib_beam(prefix=PREFIX + "RibBeam"):
    """Pivot: the ceiling line at the corridor centre; spans X -200..200, hangs 40 cm. Ends meet the ribs' knees."""
    parts = [soft(box(f"{prefix}_beam", (0, 0, -21), (BAY + 20, 56, 42), "ShellHull", bevel=0.0), bevel=5.0),
             soft(box(f"{prefix}_plate", (0, 0, -43), (BAY - 60, 34, 4), "ShellPlate", bevel=0.0), bevel=1.5),
             box(f"{prefix}_glow", (0, 0, -45.6), (BAY - 140, 4, 1.2), "ShellGlowBlue", bevel=0.0)]
    for x in (-150, 150):
        parts.append(soft(box(f"{prefix}_collar{x}", (x, 0, -21), (14, 62, 46), "ShellSteel", bevel=0.0), bevel=2.0))
    return parts


def build_ceiling_spine(prefix=PREFIX + "CeilingSpine"):
    """Pivot: the ceiling line, centre of the spine; X -200..200 across, Y -200..200 along. Trough up to z +40."""
    parts = []
    for side in (-1, 1):
        parts.append(box(f"{prefix}_panel{side}", (side * 130, 0, 7.5), (140, BAY, 15), "ShellHull", bevel=1.0))
        parts.append(soft(box(f"{prefix}_rose{side}", (side * 130, 0, -2.5), (104, BAY - 40, 5), "ShellPlate", bevel=0.0), bevel=2.0))
        parts.append(box(f"{prefix}_wall{side}", (side * 62, 0, 20), (6, BAY, 40), "ShellSteel", bevel=0.6))
        parts.append(box(f"{prefix}_lip{side}", (side * 64, 0, 1), (16, BAY, 4), "ShellSteel", bevel=0.6))
    parts.append(box(f"{prefix}_top", (0, 0, 42), (130, BAY, 4), "ShellGrate", bevel=0.0))
    # Small round lamps in the rose panels: bright points that the glossy hull picks up as highlights.
    for side in (-1, 1):
        for y in (-100.0, 100.0):
            parts.append(sb_kit.cyl(f"{prefix}_lamprim{side}_{y}", (side * 130, y, -5.5), 9.0, 3.0, "ShellSteel", axis="Z", verts=20, bevel=0.5))
            parts.append(sb_kit.cyl(f"{prefix}_lamp{side}_{y}", (side * 130, y, -7.2), 5.5, 0.8, "ShellGlowCyan", axis="Z", verts=20, bevel=0.0))
    parts.append(box(f"{prefix}_light", (0, 0, 39.4), (34, BAY - 6, 1.2), "ShellGlowCyan", bevel=0.0))
    for k in range(8):
        y = -175 + k * 50
        parts.append(box(f"{prefix}_louvre{k}", (0, y, 22), (118, 5, 34), "ShellHull", bevel=0.5))
    return parts


def build_floor_plate(prefix=PREFIX + "FloorPlate"):
    parts = [box(f"{prefix}_base", (0, 0, -9), (BAY, BAY, 14), "ShellGrate", bevel=0.0)]
    o = Vector((0, 0, -2))
    for k, (sx, sy) in enumerate(((-1, -1), (1, -1), (1, 1), (-1, 1))):
        s0, s1 = (-196, -4) if sx < 0 else (4, 196)
        t0, t1 = (-196, -4) if sy < 0 else (4, 196)
        parts.append(soft(planar_plate(f"{prefix}_p{k}", o, Vector((1, 0, 0)), Vector((0, 1, 0)), chamfer_rect(s0, s1, t0, t1, 22), 2.0,
                                       "ShellHull", bevel=0.0), levels=1, bevel=0.8))
        # Raised tread strips across each plate.
        for j in range(4):
            c = 36 + j * 40
            sgn_t = -1 if sy < 0 else 1
            parts.append(box(f"{prefix}_tread{k}_{j}", ((s0 + s1) / 2, sgn_t * c, 0.4), (150, 6, 0.8), "ShellSteel", bevel=0.2))
    return parts


def build_floor_grate(prefix=PREFIX + "FloorGrate"):
    parts = [box(f"{prefix}_sub", (0, 0, -20), (BAY, BAY, 8), "ShellGrate", bevel=0.0)]
    # Frame.
    for nm, c, s in (("fx0", (-185, 0, -6), (30, BAY, 12)), ("fx1", (185, 0, -6), (30, BAY, 12)),
                     ("fy0", (0, -185, -6), (340, 30, 12)), ("fy1", (0, 185, -6), (340, 30, 12))):
        parts.append(box(f"{prefix}_{nm}", c, s, "ShellHull", bevel=1.2))
    # Under-glow strips in the sub-floor.
    for x in (-110, 0, 110):
        parts.append(box(f"{prefix}_glow{x}", (x, 0, -15.6), (6, 330, 0.8), "ShellGlowBlue", bevel=0.0))
    # Slats along Y, cross bars along X.
    bm = bmesh.new()
    for k in range(38):
        x = -166.5 + k * 9.0
        bmesh.ops.create_cube(bm, size=1.0, matrix=_scale_matrix((x, 0, -5), (3.0, 340, 10)))
    for y in (-85.0, 0.0, 85.0):
        bmesh.ops.create_cube(bm, size=1.0, matrix=_scale_matrix((0, y, -7), (340, 6, 12)))
    parts.append(_obj(f"{prefix}_slats", bm, "ShellGrate"))
    return parts


def _scale_matrix(center, size):
    from mathutils import Matrix
    m = Matrix.Diagonal((size[0], size[1], size[2], 1.0))
    m.translation = Vector(center)
    return m


def build_edge_strip(prefix=PREFIX + "EdgeStrip"):
    """Pivot: the walkable edge at floor level; the fascia hangs down the drop (local -X is the drop side face)."""
    parts = [soft(box(f"{prefix}_fascia", (-6, 0, -18), (12, BAY, 36), "ShellHull", bevel=0.0), bevel=2.0),
             soft(box(f"{prefix}_lip", (-7, 0, 1.5), (22, BAY, 6), "ShellSteel", bevel=0.0), bevel=1.5),
             box(f"{prefix}_line", (-13.2, 0, -7), (1.2, BAY - 8, 3), "ShellGlowCyan", bevel=0.0),
             box(f"{prefix}_chin", (-9, 0, -34), (14, BAY, 6), "ShellSteel", bevel=0.8)]
    for y in (-150, -50, 50, 150):
        parts.append(box(f"{prefix}_rib{y}", (-12.5, y, -20), (3, 10, 26), "ShellSteel", bevel=0.4))
    return parts


def build_pillar(prefix=PREFIX + "Pillar"):
    """A smooth fluted column, 600 cm: swelling foot, waisted shaft, flaring capital into the ceiling."""
    sides = 24
    prof = [(0, 92), (14, 92), (30, 80), (70, 64), (160, 54), (300, 48), (440, 54), (520, 66), (570, 86), (592, 98), (600, 98)]
    sections = []
    for z, r in prof:
        ring = []
        for k in range(sides):
            ang = 2 * math.pi * k / sides
            flute = 1.0 - 0.07 * (0.5 + 0.5 * math.cos(ang * 6)) if 40 < z < 560 else 1.0  # six soft flutes
            ring.append(Vector((r * flute * math.cos(ang) * 1.15, r * flute * math.sin(ang), z)))
        sections.append(ring)
    parts = [soft(loft(f"{prefix}_shaft", sections, "ShellHull", bevel=0.0, smooth=True), levels=1)]

    def torus_band(nm, z0, z1, r0, r1, mat):
        secs = []
        for z, r in ((z0, r0), ((z0 + z1) / 2, r1), (z1, r0)):
            secs.append([Vector((r * math.cos(2 * math.pi * k / sides) * 1.15, r * math.sin(2 * math.pi * k / sides), z)) for k in range(sides)])
        return soft(loft(nm, secs, mat, smooth=True), levels=1)
    parts.append(torus_band(f"{prefix}_foot", 8, 34, 90, 97, "ShellSteel"))
    parts.append(torus_band(f"{prefix}_band", 286, 334, 47, 55, "ShellPlate"))
    parts.append(torus_band(f"{prefix}_cap", 556, 584, 84, 94, "ShellSteel"))
    # Blue light in the six flute hollows, two runs each.
    for k in range(6):
        ang = 2 * math.pi * (k + 0.5) / 6
        for z0, z1 in ((90, 260), (360, 510)):
            zc = (z0 + z1) / 2
            r = 48 + (54 - 48) * abs(zc - 300) / 140
            c = Vector((math.cos(ang) * r * 1.15 * 0.935, math.sin(ang) * r * 0.935, zc))
            slit = box(f"{prefix}_slit{k}_{z0}", tuple(c), (2.0, 4.0, z1 - z0), "ShellGlowBlue", bevel=0.0)
            slit.rotation_euler.z = ang
            parts.append(slit)
    return parts


def build_pylon(prefix=PREFIX + "Pylon"):
    """A low crystal pylon (about 2.6 m): a smooth teardrop hull pod split by four slots, a blue crystal inside and
    rising out of the top. Pivot: base centre. Pairs well as a set piece or cover dressing (not walkable)."""
    sides = 32
    prof = [(0, 84), (10, 90), (40, 94), (90, 88), (140, 72), (190, 50), (230, 28), (256, 10), (266, 0.5)]
    parts = []
    # Four petals: the pod is cut into quarters with 9 cm gaps so the crystal light shows through.
    gap = math.radians(7)
    for q in range(4):
        a0 = q * math.pi / 2 + gap
        a1 = (q + 1) * math.pi / 2 - gap
        n = 10
        secs = []
        for z, r in prof:
            outer = [Vector((r * math.cos(a0 + (a1 - a0) * i / n), r * math.sin(a0 + (a1 - a0) * i / n), z)) for i in range(n + 1)]
            ri = max(r - 7.0, 0.3)
            inner = [Vector((ri * math.cos(a0 + (a1 - a0) * i / n), ri * math.sin(a0 + (a1 - a0) * i / n), z)) for i in reversed(range(n + 1))]
            secs.append(outer + inner)
        parts.append(soft(loft(f"{prefix}_petal{q}", secs, "ShellHull", bevel=0.0, smooth=True), levels=1))
        # Lilac armour scale on each petal.
        sc = []
        for z, r in ((50, 95), (90, 89), (130, 76), (160, 64)):
            sc.append([Vector(((r + 1.5) * math.cos(a0 + 0.12 + (a1 - a0 - 0.24) * i / 8), (r + 1.5) * math.sin(a0 + 0.12 + (a1 - a0 - 0.24) * i / 8), z))
                       for i in range(9)] + [Vector(((r - 3) * math.cos(a0 + 0.12 + (a1 - a0 - 0.24) * i / 8), (r - 3) * math.sin(a0 + 0.12 + (a1 - a0 - 0.24) * i / 8), z))
                                             for i in reversed(range(9))])
        parts.append(soft(loft(f"{prefix}_scale{q}", sc, "ShellPlate", smooth=True), levels=1))
    # Base ring.
    secs = [[Vector((r * math.cos(2 * math.pi * k / sides), r * math.sin(2 * math.pi * k / sides), z)) for k in range(sides)]
            for z, r in ((0, 98), (6, 101), (16, 94))]
    parts.append(soft(loft(f"{prefix}_base", secs, "ShellSteel", smooth=True), levels=1))
    # Crystal: an elongated six-sided shard from inside the pod to 330 cm.
    crystal = []
    for z, r in ((20, 0.5), (60, 30), (200, 24), (262, 12), (300, 0.5)):
        crystal.append([Vector((r * math.cos(math.pi * k / 3), r * math.sin(math.pi * k / 3), z)) for k in range(6)])
    parts.append(loft(f"{prefix}_crystal", crystal, "ShellGlowBlue"))
    return parts


def build_portal(prefix=PREFIX + "Portal"):
    """Door surround for a 500 x 350 opening. Pivot: the opening's bottom centre on the wall face; room is +Y.
    The head is a smooth arch (no hard corners), the surround a thick rounded cowl in two layers."""
    def arch(half_w, spring, top, n=12, x_scale=1.0):
        pts = [(-half_w, 0.0), (-half_w, spring)]
        for i in range(1, n):
            a = math.pi - math.pi * i / n
            pts.append((half_w * math.cos(a) * x_scale, spring + (top - spring) * math.sin(a)))
        pts += [(half_w, spring), (half_w, 0.0)]
        return pts
    inner = arch(250, 220, 350)
    outer = arch(340, 240, 450)
    jamb = arch(234, 216, 334)
    lip = arch(300, 230, 404)
    parts = [soft(_band(f"{prefix}_surround", inner, outer, -12, 46, "ShellHull", bevel=0.0), bevel=4.0),
             soft(_band(f"{prefix}_face", [(x * 1.01, z + (4 if z > 0 else 0)) for x, z in inner], lip, 46, 62, "ShellPlate", bevel=0.0),
                  bevel=3.0),
             soft(_band(f"{prefix}_jamb", jamb, inner, -40, 26, "ShellSteel", bevel=0.0), bevel=2.0)]
    line_in = arch(231, 215, 331)
    parts.append(_band(f"{prefix}_line", line_in, jamb, 18, 22, "ShellGlowCyan", bevel=0.0))
    # Keystone fin over the crown.
    parts.append(soft(_band(f"{prefix}_key", [(-36, 404), (36, 404)], [(-20, 470), (20, 470)], 30, 74, "ShellPlate", bevel=0.0),
                      bevel=3.0))
    parts.append(box(f"{prefix}_keyglow", (0, 75, 430), (8, 1.0, 40), "ShellGlowAmber", bevel=0.0))
    # Floor threshold with a guide line.
    parts.append(soft(box(f"{prefix}_sill", (0, 10, 1.5), (500, 100, 3), "ShellSteel", bevel=0.0), levels=1, bevel=1.0))
    parts.append(box(f"{prefix}_sillglow", (0, 10, 3.2), (470, 3, 0.6), "ShellGlowCyan", bevel=0.0))
    return parts


def _band(name, inner, outer, y0, y1, mat, bevel=1.5, open_ends=True):
    """Extrudes the region between two matching open polylines (X, Z) along Y from y0 to y1."""
    bm = bmesh.new()
    def v(p, y):
        return bm.verts.new((p[0], y, p[1]))
    i0, i1 = [v(p, y0) for p in inner], [v(p, y1) for p in inner]
    o0, o1 = [v(p, y0) for p in outer], [v(p, y1) for p in outer]
    n = len(inner)
    for k in range(n - 1):
        bm.faces.new((i1[k], i1[k + 1], o1[k + 1], o1[k]))
        bm.faces.new((o0[k], o0[k + 1], i0[k + 1], i0[k]))
        bm.faces.new((i0[k], i0[k + 1], i1[k + 1], i1[k]))
        bm.faces.new((o1[k], o1[k + 1], o0[k + 1], o0[k]))
    for k in (0, n - 1):
        bm.faces.new((i0[k], i1[k], o1[k], o0[k]))
    return _obj(name, bm, mat, bevel)


PIECES = (
    ("WallBay", build_wall_bay), ("WallBayVent", build_wall_bay_vent), ("WallBayWindow", build_wall_bay_window),
    ("Rib", build_rib), ("RibBeam", build_rib_beam), ("CeilingSpine", build_ceiling_spine),
    ("FloorPlate", build_floor_plate), ("FloorGrate", build_floor_grate), ("EdgeStrip", build_edge_strip),
    ("Pillar", build_pillar), ("Pylon", build_pylon), ("Portal", build_portal),
)


def box_uvs(obj, scale=1.0 / 400.0):
    """World-scale box projection into UV0 (1 UV = 4 m), so tiling materials line up across pieces."""
    me = obj.data
    uv = me.uv_layers.new(name="UVMap") if not me.uv_layers else me.uv_layers[0]
    for poly in me.polygons:
        nx, ny, nz = (abs(c) for c in poly.normal)
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            if nz >= nx and nz >= ny:
                uv.data[li].uv = (co.x * scale, co.y * scale)
            elif nx >= ny:
                uv.data[li].uv = (co.y * scale, co.z * scale)
            else:
                uv.data[li].uv = (co.x * scale, co.z * scale)


def build_all():
    sb_kit.reset_scene()
    objs = {}
    for short, fn in PIECES:
        name = PREFIX + short
        obj = sb_kit.join(name, fn())
        box_uvs(obj)
        objs[short] = obj
    return objs


def export(objs):
    OUT.mkdir(parents=True, exist_ok=True)
    lines = []
    for short, obj in objs.items():
        line = f"{obj.name}: {sb_kit.triangle_count(obj)} tris, {sb_kit.bounds(obj)}, slots " + ", ".join(m.name for m in obj.data.materials)
        sb_kit.export_fbx(str(OUT / (obj.name + ".fbx")), [obj])
        lines.append(line)
        print(line)
    (OUT / "keel_shell_report.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    palette_file = ROOT / "Art/Export/palette.json"
    palette = json.loads(palette_file.read_text())
    for key, (colour, metallic, roughness, emission) in SHELL_PALETTE.items():
        palette[key] = [list(colour), metallic, roughness, emission]
    # Keep the CRLF endings the desktop checkout writes (sb_kit.write_palette), so a rerun only shows real edits.
    with open(palette_file, "w", newline="") as f:
        f.write(json.dumps(palette, indent=1).replace("\n", "\r\n") + "\r\n")


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    pieces = build_all()
    export(pieces)
    if "--render" in argv:
        import render_keel_shell_kit
        render_keel_shell_kit.render_all(pieces, argv)
