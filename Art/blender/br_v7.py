"""Vanguard BR v7 (2026-11-07): the battle rifle reshaped around one flowing body.

v4-v6 read as boxes stacked together: separate upper, shroud, panels, scope on risers. v7 keeps everything the hands
and the game rely on (grip, trigger, magazine and its split box, support-hand forend, muzzle line, optic axis at
z 14.4 cm) and replaces the rest with:
 - a single swept white ceramic shell from the receiver to the nose, rounded in section (wide bevels), over a dark
   alloy core that shows as a band along the waist;
 - a forend armour piece that wraps under the shell, with a rubberised pad where the left hand sits;
 - a chunky integrated optic: one rounded housing on a low mount with a visor over a big objective lens, a ribbed
   eyecup and a side ammo/range display (instead of a tube on clamp risers);
 - a skeletal stock drawn as one swept frame with a cheek riser and a ribbed butt pad.
Original design. Halo Infinite is the fidelity bar only; nothing is traced or copied.

Mesh space as build_weapons.py: X across (the first-person camera sees -X), Y forward, Z up, cm.
"""
import math

import sb_kit as k
import build_weapons as bw

TZ = 14.4  # optic axis height (task 21/22 framing)


def smooth(points, samples=6, closed=True):
    """Catmull-Rom through the points: a flowing outline from a few control points. Corners stay where a point is
    repeated."""
    pts = list(points)
    n = len(pts)
    out = []
    rng = range(n) if closed else range(n - 1)
    for i in rng:
        p0 = pts[(i - 1) % n] if closed or i > 0 else pts[i]
        p1 = pts[i]
        p2 = pts[(i + 1) % n]
        p3 = pts[(i + 2) % n] if closed or i + 2 < n else p2
        if p1 == p2:
            continue
        for s in range(samples):
            t = s / samples
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[j]) + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t2 +
                                    (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t3) for j in range(2)))
    # drop near-duplicates (bmesh faces need distinct verts)
    clean = []
    for p in out:
        if not clean or math.hypot(p[0] - clean[-1][0], p[1] - clean[-1][1]) > 0.05:
            clean.append(p)
    if math.hypot(clean[0][0] - clean[-1][0], clean[0][1] - clean[-1][1]) < 0.05:
        clean.pop()
    return clean


def shape(name, points, thickness, mat, x=0.0, bevel=0.6, segments=3, samples=6):
    return k.profile(name, smooth(points, samples), thickness, mat, x=x, bevel=bevel, segments=segments)


def battle_rifle_v7():
    p = []
    S = 3
    # ---- Core: the dark alloy chassis. Narrower than the shell; shows as a band at the waist and under the nose ----
    p.append(shape("core", [(-13.0, -0.4), (-13.0, 6.0), (20, 6.2), (46, 4.2), (51.5, 1.6), (49, -1.6), (36, -2.6), (21, -2.6), (20.5, -0.4)],
                   4.7, "WeaponMetal", bevel=0.5, segments=S))
    # ---- Upper shell: one swept ceramic piece from the receiver to the nose ----
    upper = [(-13.6, 3.8), (-13.6, 7.0), (-10.0, 9.2), (0.0, 9.9), (17.0, 9.9), (28.0, 9.0), (39.0, 7.3), (47.5, 5.0), (52.6, 2.7),
             (53.2, 1.5), (49.0, 1.0), (40.0, 2.6), (30.0, 3.6), (18.0, 3.7), (3.0, 3.5)]
    p.append(shape("shell_upper", upper, 6.0, "Ceramic", bevel=1.25, segments=S + 1))
    # ---- Forend armour: wraps under the shell's front half, the left hand's grip ----
    fore = [(20.6, 1.6), (34.0, 1.2), (45.0, 0.6), (50.6, -0.6), (48.0, -2.8), (40.0, -4.4), (30.0, -4.7), (21.0, -4.6)]
    p.append(shape("forend", fore, 5.6, "Ceramic", bevel=1.0, segments=S + 1))
    p.append(shape("forend_pad", [(21.6, -4.2), (33.0, -4.3), (33.6, -5.25), (21.6, -5.15)], 4.6, "Polymer", bevel=0.35, segments=2,
                   samples=3))
    for i in range(10):
        p.append(k.box("forend_rib", (0, 22.6 + i * 1.05, -5.35), (4.2, 0.4, 0.3), "Polymer", bevel=0.08))
    p.append(k.box("hand_stop", (0, 34.4, -5.0), (4.0, 1.0, 1.8), "WeaponMetal", bevel=0.35, segments=2))
    for x in (2.82, -2.82):
        for y in (37.0, 40.5):
            p.append(k.box("mlok", (x, y, -2.2), (0.1, 2.4, 0.7), "WeaponMetal", bevel=0))
    # ---- Lower receiver, grip, guard (same fit as v6) ----
    p.append(k.profile("lower", [(-12.5, -3.8), (-12.5, 0.6), (20.5, 0.6), (20.5, -2.4), (17, -3.8)], 4.6, "Polymer", bevel=0.5, segments=S))
    p.append(k.profile("grip", [(-3.5, -3.5), (3, -3.5), (1.6, -15.2), (0.4, -16.2), (-4.6, -15.6), (-5.0, -14.6)], 3.4,
                       "Polymer", bevel=0.7, segments=S))
    p.append(k.box("grip_cap", (0, -1.8, -15.9), (3.0, 4.8, 0.6), "WeaponMetal", bevel=0.2))
    p.append(k.profile("guard", [(3, -3.9), (9.5, -3.9), (9.5, -9.2), (7.9, -9.2), (7.9, -5.2), (3, -5.2)], 1.1,
                       "WeaponMetal", bevel=0.25, segments=S))
    # ---- Magazine (same box as v6 for the reload split) ----
    p.append(k.profile("mag", [(10.2, -3.6), (16.4, -3.6), (18.6, -15.0), (12.0, -15.8)], 3.6, "Polymer", bevel=0.6, segments=S))
    p.append(k.profile("mag_base", [(11.8, -15.4), (18.8, -14.6), (19.2, -16.2), (11.6, -17.0)], 4.0, "Ceramic", bevel=0.3, segments=S))
    p.append(k.box("mag_well", (0, 13.3, -3.9), (4.8, 7.6, 1.2), "WeaponMetal", bevel=0.3))
    for x in (1.82, -1.82):
        p.append(k.box("mag_window", (x, 14.6, -8.5), (0.12, 2.2, 4.0), "Display", bevel=0))
        for i in range(3):
            bw.seam(p, x + (0.04 if x > 0 else -0.04), (12.6 + i * 1.5, -4.6), (13.8 + i * 1.6, -14.4), w=0.1)
    p.append(k.box("mag_tab", (0, 15.4, -17.2), (2.4, 2.2, 0.5), "Polymer", bevel=0.15))
    # ---- Barrel and muzzle brake ----
    p.append(k.cyl("barrel", (0, 54.5, 1.6), 1.05, 4.0, "Steel", verts=16))
    p.append(shape("brake", [(55.8, -0.2), (55.8, 3.4), (58.4, 3.1), (60.2, 2.5), (60.2, 0.7), (58.4, 0.1)], 3.0, "WeaponMetal",
                   bevel=0.5, segments=S, samples=3))
    for y in (56.6, 57.7, 58.8):
        for x in (1.52, -1.52):
            p.append(k.box("muzzle_port", (x, y, 1.6), (0.2, 0.6, 1.6), "Polymer", bevel=0))
        p.append(k.box("brake_top", (0, y, 3.35), (1.6, 0.45, 0.12), "Polymer", bevel=0))
    p.append(k.cyl("crown", (0, 60.3, 1.6), 0.95, 0.2, "GlowCyan", verts=16, bevel=0))
    # ---- Stock: one swept skeletal frame, cheek riser, ribbed butt pad ----
    p.append(shape("stock_top", [(-13.0, 2.4), (-13.0, 6.6), (-22.0, 6.7), (-32.0, 6.0), (-38.0, 4.6), (-39.2, 2.6), (-36.4, 1.6),
                                 (-30.0, 3.0), (-21.0, 3.6)], 4.2, "Polymer", bevel=0.8, segments=S))
    p.append(shape("stock_low", [(-13.0, -3.8), (-13.0, 0.0), (-22.0, -1.4), (-33.0, -5.2), (-39.2, -6.0), (-39.2, -8.4), (-36.0, -8.8),
                                 (-30.0, -6.8), (-21.0, -4.6)], 4.0, "Polymer", bevel=0.8, segments=S))
    p.append(shape("stock_butt", [(-36.6, -8.6), (-36.6, 4.6), (-39.4, 4.0), (-39.4, -8.4)], 4.4, "Ceramic", bevel=0.6, segments=S, samples=2))
    p.append(shape("cheek", [(-14.0, 6.2), (-15.0, 8.0), (-28.0, 7.8), (-31.0, 6.4)], 3.6, "Ceramic", bevel=0.6, segments=S, samples=4))
    p.append(k.box("butt_pad", (0, -40.1, -1.9), (4.6, 1.4, 13.2), "Polymer", bevel=0.45, segments=S))
    for i in range(7):
        p.append(k.box("butt_rib", (0, -40.85, -7.4 + i * 1.8), (4.2, 0.2, 0.5), "Polymer", bevel=0.06))
    # ---- Integrated optic ----
    battle_rifle_v7_optic(p, S)
    # ---- Glow lines, counter, controls ----
    for x in (3.02, -3.02):
        p.append(k.profile("light_line", [(24.0, 4.35), (44.0, 3.0), (44.3, 3.32), (24.0, 4.67)], 0.12, "GlowCyan", x=x, bevel=0))
    p.append(k.box("counter_bezel", (0, -9.5, 9.55), (3.6, 4.0, 0.5), "Polymer", bevel=0.15))
    p.append(k.box("counter", (0, -9.5, 9.85), (3.0, 3.4, 0.25), "Display", bevel=0.05))
    p.append(k.box("charging_handle", (-3.15, -9.0, 6.0), (0.8, 3.2, 1.2), "Steel", bevel=0.25))
    p.append(k.box("ch_channel", (-3.02, -6.5, 6.0), (0.08, 9.0, 0.7), "Polymer", bevel=0))
    p.append(k.box("ejection_port", (2.95, -2, 5.0), (0.3, 6.0, 1.8), "Polymer", bevel=0.15))
    p.append(k.box("deflector", (3.1, 1.8, 6.2), (0.6, 1.2, 1.2), "WeaponMetal", bevel=0.25))
    p.append(k.box("selector", (-2.6, -1.5, -1.6), (0.4, 1.6, 0.9), "Steel", bevel=0.1))
    p.append(k.box("selector_r", (2.6, -1.5, -1.6), (0.4, 1.6, 0.9), "Steel", bevel=0.1))
    p.append(k.box("bolt_release", (-2.55, 8.6, -2.3), (0.4, 2.6, 1.2), "Steel", bevel=0.15))
    for y in (-10.5, 18.6):
        for x in (2.33, -2.33):
            p.append(k.cyl("pin", (x, y, -1.6), 0.42, 0.18, "Steel", axis="X", verts=12, bevel=0.04))
    p.append(k.cyl("qd_cup", (-2.15, -16.0, 2.0), 0.7, 0.4, "Steel", axis="X", verts=12, bevel=0.06))
    for y in (-6.0, -4.0, 12.0, 14.0):
        rail_tooth = k.box("tooth", (0, y if y < 0 else y + 9.0, 10.15), (3.0, 0.8, 0.6), "WeaponMetal", bevel=0.1)
        p.append(rail_tooth)
    # ---- Seams (grooves on the high poly) and screws ----
    for x in (3.02, -3.02):
        out = 1 if x > 0 else -1
        bw.seam(p, x, (-4.0, 4.0), (-4.0, 8.9))
        bw.seam(p, x, (21.0, 3.9), (21.0, 9.0))
        bw.seam(p, x, (36.0, 3.7), (36.0, 7.6))
        bw.seam(p, x, (-12.8, 6.0), (21.0, 6.0))
        for y, z in ((-11.6, 4.6), (-11.6, 7.4), (19.6, 4.5), (19.6, 8.2), (34.6, 4.3)):
            bw.screw(p, x + out * 0.02, y, z, 0.3)
    for x in (2.82, -2.82):
        out = 1 if x > 0 else -1
        bw.seam(p, x, (33.0, 1.1), (33.0, -4.5))
        for y, z in ((23.0, -1.0), (31.2, -1.2), (46.0, -0.6)):
            bw.screw(p, x + out * 0.02, y, z, 0.28)
    for x in (2.12, -2.12):
        bw.seam(p, x, (-30.0, 6.6), (-16.0, 6.6))
        for y in (-18.0, -20.0, -22.0):
            p.append(k.cyl("adj_hole", (x, y, 1.0), 0.35, 0.06, "Polymer", axis="X", verts=10, bevel=0))
    # Vents: side slots near the nose, cooling slots across the shell's top.
    for x in (3.02, -3.02):
        for i in range(6):
            y = 39.5 + i * 1.3
            z = 6.1 - (y - 39.5) * 0.24
            p.append(k.box("side_slot", (x, y, z), (0.08, 0.6, 1.4), "Polymer", bevel=0))
    for i in range(7):
        y = 26.0 + i * 1.2
        p.append(k.box("cool_slot", (0, y, 9.25 - (y - 26.0) * 0.12), (3.0, 0.5, 0.2), "Polymer", bevel=0))
    return p, (0, 60.5, 1.6)


def battle_rifle_v7_optic(p, S):
    """A compact sight (v7b, slimmed after the 2026-11-07 PC shots showed the first housing filling 17% of the screen):
    a low wedge housing 3.8 cm wide on a short mount, a small hooded objective and a slim eyecup."""
    p.append(shape("optic_mount", [(4.0, 9.4), (16.0, 9.4), (15.0, 11.2), (5.0, 11.2)], 2.8, "WeaponMetal", bevel=0.3, segments=2,
                   samples=2))
    for x in (1.5, -1.5):
        bw.screw(p, x, 6.5, 10.3, 0.22)
        bw.screw(p, x, 13.5, 10.3, 0.22)
    # Housing: a low wedge, rising toward the objective.
    p.append(shape("optic_body", [(2.6, 11.0), (2.2, 13.6), (4.6, 15.6), (13.0, 16.4), (17.4, 16.3), (18.6, 15.4), (18.8, 12.6),
                                  (17.2, 10.9), (4.0, 10.8)], 3.8, "Ceramic", bevel=0.75, segments=S + 1))
    # Short hood over the objective.
    p.append(shape("optic_visor", [(14.0, 16.0), (14.8, 16.8), (19.6, 16.9), (20.6, 16.2), (20.2, 15.8), (17.6, 16.1)], 4.0, "Ceramic",
                   bevel=0.3, segments=S, samples=4))
    p.append(k.cyl("objective_bezel", (0, 18.9, TZ), 1.75, 0.7, "WeaponMetal", verts=32, bevel=0.12))
    p.append(k.cyl("objective_ring", (0, 19.27, TZ), 1.55, 0.08, "GlowCyan", verts=32, bevel=0))
    p.append(k.cyl("objective_lens", (0, 19.2, TZ), 1.45, 0.06, "Lens", verts=32, bevel=0))
    # Slim eyecup.
    p.append(k.cyl("eyecup", (0, 1.9, TZ), 1.35, 1.2, "Polymer", verts=24, bevel=0.2, radius2=1.5))
    for i in range(2):
        p.append(k.cyl("eye_rib", (0, 1.55 + i * 0.5, TZ), 1.58, 0.14, "Polymer", verts=24, bevel=0))
    p.append(k.cyl("eye_lens", (0, 1.25, TZ), 1.05, 0.05, "Lens", verts=24, bevel=0))
    # Side controls (left: illumination knob and range display; right: battery cap).
    p.append(k.cyl("turret", (-2.1, 9.0, TZ + 0.4), 0.7, 0.6, "WeaponMetal", axis="X", verts=24, bevel=0.1))
    p.append(k.cyl("turret_cap", (-2.45, 9.0, TZ + 0.4), 0.5, 0.14, "Steel", axis="X", verts=24, bevel=0.03))
    p.append(k.cyl("battery", (2.05, 9.0, TZ), 0.65, 0.5, "WeaponMetal", axis="X", verts=20, bevel=0.08))
    p.append(k.box("range_housing", (-1.98, 14.4, TZ - 1.4), (0.3, 3.6, 1.2), "Polymer", bevel=0.1))
    p.append(k.box("range_display", (-2.14, 14.4, TZ - 1.4), (0.06, 3.0, 0.8), "Display", bevel=0))
    for x in (1.92, -1.92):
        out = 1 if x > 0 else -1
        p.append(shape("optic_band", [(3.6, 11.0), (17.6, 11.0), (18.2, 11.9), (17.6, 12.5), (4.4, 12.5)], 0.24, "WeaponMetal",
                       x=x + out * 0.06, bevel=0.08, segments=2, samples=2))
        bw.seam(p, x, (11.5, 12.6), (11.5, 16.0))
        p.append(k.box("optic_line", (x + out * 0.02, 8.0, 15.3), (0.07, 8.0, 0.13), "GlowCyan", bevel=0))


def extra_cuts(c):
    """Machined detail for the high poly (pro_highpoly.Collector)."""
    for s in (1, -1):
        # Stock: lightening pockets in both arms.
        c.cutter_box((s * 2.1, -27.0, 4.6), (0.6, 7.0, 1.0), rot_x=math.radians(-8))
        c.cutter_box((s * 2.0, -26.0, -3.3), (0.6, 6.0, 1.0), rot_x=math.radians(-18))
        # Lower receiver: relief groove and a pocket above the grip.
        c.cutter_box((s * 2.3, 4.0, -0.6), (0.4, 26.0, 0.14))
        c.cutter_box((s * 2.3, -7.5, -2.4), (0.36, 5.0, 1.2))
        # Grip: finger-ridge grooves on the front strap.
        for i, z in enumerate((-8.0, -10.6, -13.2)):
            c.cutter_box((0.0, 2.2 - i * 0.25, z), (4.0, 1.2, 0.18), rot_x=math.radians(-8))
        # Magazine side panel.
        c.cutter_box((s * 1.95, 15.4, -10.0), (0.3, 4.2, 6.0), rot_x=math.radians(-11))
        # Shell: a long swept groove under the light line, and a recessed panel behind the vents.
        c.cutter_box((s * 3.05, 30.0, 6.9), (0.3, 16.0, 0.14), rot_x=math.radians(-7.5))
        c.cutter_box((s * 3.05, 9.0, 8.0), (0.3, 11.0, 1.2))
        # Forend: a recessed grip panel.
        c.cutter_box((s * 2.85, 27.0, -2.6), (0.3, 9.0, 2.0))
        # Optic body: a recessed window strip.
        c.cutter_box((s * 1.95, 9.0, 13.4), (0.24, 6.0, 0.5))
    c.cutter_box((0.0, -9.5, 9.95), (4.2, 4.6, 0.25))  # channel round the counter bezel
    for i in range(3):
        c.cutter_cyl((0.0, 3.4 + i * 0.4, TZ), 1.9, 0.1, "Y", 48)  # grip rings on the optic's rear
