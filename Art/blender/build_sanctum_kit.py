"""Sanctum's hero set pieces for the scanned look (run headless: python3 Art/blender/build_sanctum_kit.py [--render=<dir>]).

Forerunner-inspired, original shapes: battered (inward-leaning) stone, stepped tiers, angled fins, recessed light
channels and Shieldbreak's own twist, levitating hard-light stone: blocks that hang apart with glowing seams between
them. Nothing here copies a Halo asset or layout.

Stone slots are "Ivory" (cut stone) and "Sand" (rough plinths); with Sanctum's scanned look the game puts the scanned
quarry stone with moss on top on both (ApplyKitSurfaces), so these meshes need no UVs. Glow slots keep their emissive.
Mesh space as in build_map_kit.py: 1 unit = 1 cm, pivot on the ground at the base, local +X faces the arena.

Pieces:
  Gate     a 3400 cm wide gateway: two battered three-tier pylons with fins, a notched lintel, a floating keystone
  Shard    a 4600 cm levitating obelisk: five stone segments hung apart on a hard-light core over a stepped plinth
  Terrace  a 4000 cm long, three-step battered retaining wall with a light channel per step, to stand in the canyon
  Altar    the shell over the centre plinth (600 x 600 x 320 cm of collision): battered stone with light channels, and a
           crown of hard-light stones hanging high above it, out of reach

Exported to Art/Export/MapKit (same import as the rest of the map kit: Tools/ImportMapArt.bat).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit  # noqa: E402  (imports bpy first)
from build_map_kit import OUT, frustum, placed, prism_xz  # noqa: E402

import bmesh  # noqa: E402
import bpy  # noqa: E402


def battered(name, center, size_lo, size_hi, height, mat, bevel=8.0, chamfer=0.14):
    """An eight-sided tapered block: a box footprint (x, y) with its vertical edges cut, smaller at the top."""
    bm = bmesh.new()
    rings = []
    for z, (sx, sy) in ((0.0, size_lo), (height, size_hi)):
        hx, hy = sx * 0.5, sy * 0.5
        c = min(hx, hy) * chamfer * 2.0
        pts = [(hx, -hy + c), (hx, hy - c), (hx - c, hy), (-hx + c, hy), (-hx, hy - c), (-hx, -hy + c), (-hx + c, -hy), (hx - c, -hy)]
        rings.append([bm.verts.new((center[0] + x, center[1] + y, center[2] + z)) for x, y in pts])
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[1])
    for i in range(8):
        j = (i + 1) % 8
        bm.faces.new((rings[0][i], rings[0][j], rings[1][j], rings[1][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    return sb_kit._finish(sb_kit._link(name, mesh), mat, bevel)


def pylon_stack(y, height_scale=1.0):
    """One gate pylon: plinth, a tall battered shaft, a narrower upper tier and a cut cap, with light slots."""
    s = height_scale
    parts = [battered("plinth", (0, y, 0), (1000, 820), (940, 760), 260 * s, "Sand", bevel=12.0)]
    z = 260 * s
    for (lo, hi, h) in (((860, 680), (700, 560), 1250 * s), ((640, 520), (540, 430), 620 * s)):
        parts.append(battered("tier", (0, y, z), lo, hi, h, "Ivory", bevel=10.0))
        # A recessed band of light across the arena face near the top of each tier.
        parts.append(sb_kit.box("slot", (hi[0] * 0.5 + 6, y, z + h - 90), (16, hi[1] * 0.7, 26), "GlowCyan", bevel=0.0))
        z += h
        parts.append(battered("ledge", (0, y, z), (hi[0] + 80, hi[1] + 80), (hi[0] + 40, hi[1] + 40), 60, "Sand", bevel=6.0))
        z += 60
    parts.append(battered("cap", (0, y, z), (520, 410), (90, 70), 380 * s, "Ivory", bevel=6.0, chamfer=0.1))
    return parts, z


def gate():
    """The gateway. Spans Y (3400 cm between the pylons' outer faces), about 3000 cm tall to the cap tips."""
    parts = []
    tops = []
    for side in (-1, 1):
        stack, top = pylon_stack(side * 1250)
        parts += stack
        tops.append(top)
        # Two swept fins on the outer flank, leaning out like buttresses.
        for x, h in ((-200, 1700), (220, 1300)):
            fin = prism_xz("fin", [(0, 0), (420, 0), (120, h), (40, h + 120)], -45, 45, "Ivory", bevel=6.0)
            placed(fin, (x, side * 1660, 0), (0, 0, -90 * side))
            parts.append(fin)
    # The lintel: a slab with a V notch cut under its middle, spanning between the upper tiers.
    z = tops[0] - 520
    outline = [(-1350, 0), (-260, 0), (0, -220), (260, 0), (1350, 0), (1350, 340), (420, 420), (0, 640), (-420, 420), (-1350, 340)]
    lintel = prism_xz("lintel", outline, -230, 230, "Ivory", bevel=8.0)
    placed(lintel, (0, 0, z), (0, 0, 90))
    parts.append(lintel)
    # Light along the notch's two edges, on the arena face.
    for side in (-1, 1):
        length = math.hypot(260, 220)
        edge = sb_kit.box("notch_glow", (0, 0, 0), (14, length, 14), "GlowCyan", bevel=0.0)
        placed(edge, (236, side * 130, z - 110), (side * math.degrees(math.atan2(220, 260)), 0, 0))
        parts.append(edge)
    # The floating keystone (Shieldbreak's hard light): a stone diamond hung in the notch, a glowing core through it.
    parts.append(placed(frustum("key_hi", (0, 0, 0), (200, 200), (10, 10), 180, "Ivory", bevel=4.0), (0, 0, z - 520), (0, 0, 0)))
    parts.append(placed(frustum("key_lo", (0, 0, 0), (200, 200), (10, 10), -220, "Ivory", bevel=4.0), (0, 0, z - 540), (0, 0, 0)))
    parts.append(sb_kit.box("key_core", (0, 0, z - 530), (230, 230, 14), "GlowAmber", bevel=0.0))
    return sb_kit.join("SM_SB_Kit_Gate", parts)


def shard():
    """The levitating obelisk: a stepped plinth, then five diamond-section segments with gaps of glowing core."""
    parts = []
    z = 0.0
    for (lo, hi, h) in (((1500, 1500), (1400, 1400), 160), ((1150, 1150), (1060, 1060), 160), ((820, 820), (760, 760), 140)):
        parts.append(battered("step", (0, 0, z), lo, hi, h, "Sand", bevel=10.0, chamfer=0.08))
        z += h
    widths = (620, 560, 500, 420, 330)
    heights = (900, 820, 700, 560, 760)
    gap = 160.0
    core_lo = z
    for i, (w, h) in enumerate(zip(widths, heights)):
        z += gap
        top_w = widths[i + 1] if i + 1 < len(widths) else 20
        seg = battered("seg", (0, 0, 0), (w, w * 0.8), (top_w, top_w * 0.8), h, "Ivory", bevel=7.0, chamfer=0.22)
        placed(seg, (0, 0, z), (0, 0, 7.0 * i))
        parts.append(seg)
        # A thin light seam round each segment's foot.
        ring = battered("seam", (0, 0, 0), (w * 0.86, w * 0.69), (w * 0.86, w * 0.69), 20, "GlowCyan", bevel=0.0, chamfer=0.22)
        placed(ring, (0, 0, z - gap * 0.5 - 10), (0, 0, 7.0 * i))
        parts.append(ring)
        z += h
    parts.append(sb_kit.box("core", (0, 0, (core_lo + z) * 0.5), (60, 60, z - core_lo - 400), "GlowCyan", bevel=0.0))
    return sb_kit.join("SM_SB_Kit_Shard", parts)


def terrace():
    """Three battered steps of retaining wall, 4000 cm long (along Y), stepping back from the arena (+X faces it)."""
    parts = []
    depth = 0.0
    z = 0.0
    for i, h in enumerate((700, 600, 520)):
        x = -depth
        parts.append(battered("wall", (x - 300, 0, z), (700, 4000 - i * 300), (600, 4000 - i * 300 - 40), h, "Ivory", bevel=10.0, chamfer=0.02))
        parts.append(sb_kit.box("channel", (x + 8, 0, z + h - 70), (16, 3400 - i * 300, 24), "GlowCyan", bevel=0.0))
        # Battered pilasters along the face, breaking the long wall into bays.
        span = (4000 - i * 300) * 0.5 - 300
        count = int(span * 2 // 650)
        for k in range(count + 1):
            y = -span + k * (span * 2 / max(count, 1))
            parts.append(battered("rib", (x + 40, y, z), (150, 170), (90, 140), h - 110, "Ivory", bevel=6.0, chamfer=0.2))
        parts.append(battered("lip", (x - 300, 0, z + h), (720, 4020 - i * 300), (680, 3980 - i * 300), 50, "Sand", bevel=6.0, chamfer=0.02))
        depth += 420.0
        z += h + 50
    return sb_kit.join("SM_SB_Kit_Terrace", parts)


def altar():
    """Wraps the 600 x 600 x 320 cm centre plinth. The shell stays within 20 cm of the collision box at the foot and
    meets it at the top (322 cm, just over the block so the stone top shows), so what you see is what you stand on."""
    parts = []
    top = 322.0
    # The battered body, then a cut lip round the top edge.
    parts.append(battered("body", (0, 0, 0), (640, 640), (612, 612), top - 40, "Ivory", bevel=8.0, chamfer=0.1))
    parts.append(battered("lip", (0, 0, top - 40), (616, 616), (604, 604), 40, "Sand", bevel=5.0, chamfer=0.1))
    # A base course, knee high.
    parts.append(battered("course", (0, 0, 0), (650, 650), (642, 642), 60, "Sand", bevel=6.0, chamfer=0.1))
    # On each face: a recessed vertical light slit in the middle, flanked by two angled light strokes, and a band.
    for yaw in (0, 90, 180, 270):
        a = math.radians(yaw)
        def face_box(name, off_along, z, size, mat, tilt=0.0, out=0.0):
            # size: (thickness, width along the face, height); placed proud of the battered face at height z.
            face = 320.0 - 14.0 * (z / top) + 3.0 + out
            obj = sb_kit.box(name, (0, 0, 0), size, mat, bevel=0.0)
            x = math.cos(a) * face - math.sin(a) * off_along
            y = math.sin(a) * face + math.cos(a) * off_along
            return placed(obj, (x, y, z), (tilt, 0, yaw))
        parts.append(face_box("slit", 0, 170, (8, 22, 180), "GlowCyan"))
        for side in (-1, 1):
            parts.append(face_box("stroke", side * 130, 150, (8, 14, 150), "GlowCyan", tilt=side * 28.0))
            # Stone pilasters at the corners of each face, standing a little proud.
            parts.append(face_box("pilaster", side * 250, 130, (14, 70, 200), "Ivory", out=4.0))
        parts.append(face_box("band", 0, 255, (8, 420, 10), "GlowCyan"))
    # The crown: four hard-light stones hanging high over the plinth (out of reach of a jump from its top), each
    # tilted in toward the middle, with a glowing core between them.
    crown_z = 860.0
    for i, yaw in enumerate((45, 135, 225, 315)):
        a = math.radians(yaw)
        r = 260.0
        stone = frustum("crown_hi", (0, 0, 0), (130, 130), (10, 10), 300, "Ivory", bevel=4.0)
        placed(stone, (math.cos(a) * r, math.sin(a) * r, crown_z + (i % 2) * 40), (0, -18, yaw))
        parts.append(stone)
        tail = frustum("crown_lo", (0, 0, 0), (130, 130), (10, 10), -150, "Ivory", bevel=4.0)
        placed(tail, (math.cos(a) * r, math.sin(a) * r, crown_z - 10 + (i % 2) * 40), (0, -18, yaw))
        parts.append(tail)
    parts.append(placed(frustum("core_hi", (0, 0, 0), (70, 70), (4, 4), 120, "GlowAmber", bevel=0.0), (0, 0, crown_z + 60), (0, 0, 0)))
    parts.append(placed(frustum("core_lo", (0, 0, 0), (70, 70), (4, 4), -120, "GlowAmber", bevel=0.0), (0, 0, crown_z + 60), (0, 0, 0)))
    return sb_kit.join("SM_SB_Kit_Altar", parts)


PIECES = (gate, shard, terrace, altar)


def build(render=None):
    os.makedirs(OUT, exist_ok=True)
    report = []
    for make in PIECES:
        sb_kit.reset_scene()
        obj = make()
        slots = ", ".join(m.name for m in obj.data.materials)
        line = "%s: %d tris, %s, slots %s" % (obj.name, sb_kit.triangle_count(obj), sb_kit.bounds(obj), slots)
        print(line)
        report.append(line)
        sb_kit.export_fbx(os.path.join(OUT, obj.name + ".fbx"), [obj])
        if render:
            import sb_render
            sb_render.render_views([obj], os.path.join(render, "sanctum_%s.png" % make.__name__),
                                   [((1.0, -0.9, 0.45), False), ((1.0, 0.0, 0.0), True)], floor=True)
    with open(os.path.join(OUT, "sanctum_kit_report.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


if __name__ == "__main__":
    build(render=next((a.split("=", 1)[1] for a in sys.argv if a.startswith("--render=")), None))
