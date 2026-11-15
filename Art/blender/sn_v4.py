"""Longshot SR v4 (2026-11-07): the sniper reshaped in the Vanguard BR v7 language.

v3 read as a receiver box, a separate shroud box and a scope box on clamps. v4 keeps what the hands and the game rely on
(grip, trigger, magazine and its split box, the support hand at y 30, bore height 2.4 cm, muzzle at y 105.6) and makes:
 - one long swept white ceramic shell from the receiver to the shroud nose over a dark alloy core;
 - a forend armour lobe under the front half with a ribbed pad where the left hand sits, and a folded bipod under it;
 - a long slim integrated scope: one wedge housing on a low saddle, a hooded big objective, a slim eyecup, a top
   turret and a rangefinder pod;
 - a long skeletal stock drawn as one swept frame, with a ceramic cheek riser and butt.
Original design. Halo Infinite is the fidelity bar only; nothing is traced or copied.

Mesh space as build_weapons.py: X across (the first-person camera sees -X), Y forward, Z up, cm.
"""
import math

import sb_kit as k
import build_weapons as bw
from br_v7 import shape

BZ = 2.4   # bore height
SZ = 12.9  # scope axis height


def sniper_v4():
    p = []
    S = 3
    # ---- Core: dark alloy chassis; shows as a waist band under the shell ----
    p.append(shape("core", [(-15.5, -0.6), (-15.5, 6.0), (30, 6.0), (70, 4.6), (85.5, 3.4), (85.5, 0.6), (70, -0.4), (30, -1.0),
                            (20.5, -1.0), (20.5, -0.6)], 3.6, "WeaponMetal", bevel=0.5, segments=S))
    # ---- Upper shell: one long swept ceramic piece ----
    upper = [(-15.8, 3.0), (-15.8, 6.2), (-12.0, 8.2), (0.0, 8.8), (24.0, 8.8), (40.0, 8.0), (62.0, 6.9), (78.0, 5.8), (84.6, 4.6),
             (85.6, 3.0), (80.0, 2.2), (60.0, 2.6), (36.0, 3.1), (20.0, 3.2), (2.0, 3.0)]
    p.append(shape("shell_upper", upper, 5.4, "Ceramic", bevel=1.1, segments=S + 1))
    # ---- Forend armour: wraps under the shell's front, the left hand's grip ----
    fore = [(21.0, 2.0), (40.0, 1.8), (60.0, 1.4), (74.0, 0.8), (78.0, -0.4), (74.0, -2.2), (56.0, -2.8), (36.0, -3.0),
            (22.0, -2.8)]
    # Uncut on the high poly ("plain_"): booleans on its long curved side baked as dark streaks.
    p.append(shape("plain_forend", fore, 5.0, "Ceramic", bevel=0.9, segments=S + 1))
    p.append(shape("forend_pad", [(25.0, -2.6), (37.0, -2.7), (37.6, -3.55), (25.0, -3.45)], 4.2, "Polymer", bevel=0.3,
                   segments=2, samples=3))
    for i in range(11):
        p.append(k.box("forend_rib", (0, 25.8 + i * 1.05, -3.65), (3.8, 0.38, 0.26), "Polymer", bevel=0.08))
    p.append(k.box("hand_stop", (0, 38.6, -3.4), (3.6, 1.0, 1.6), "WeaponMetal", bevel=0.3, segments=2))
    for x in (2.52, -2.52):
        for y in (44.0, 48.0, 52.0):
            p.append(k.box("rail_plate", (x, y, -1.0), (0.1, 2.4, 0.7), "WeaponMetal", bevel=0))
    # Folded bipod under the forend nose, held clear of its skin (overlaps bake as dark marks).
    p.append(k.box("bipod_hinge", (0, 74.0, -3.1), (2.8, 2.4, 1.2), "Steel", bevel=0.3))
    for x in (0.9, -0.9):
        p.append(k.box("bipod_leg", (x, 64.0, -4.0), (0.7, 18.0, 0.7), "Steel", bevel=0.15))
        p.append(k.box("bipod_foot", (x, 54.6, -4.1), (1.0, 1.4, 1.0), "Polymer", bevel=0.2))
    # ---- Exposed fluted barrel and a swept slotted brake ----
    p.append(k.cyl("barrel_nut", (0, 86.4, BZ), 1.7, 1.2, "WeaponMetal", verts=20, bevel=0.15))
    p.append(k.cyl("barrel", (0, 92.5, BZ), 1.2, 12.0, "Steel", verts=16))
    for i in range(6):
        a = i * math.pi / 3
        p.append(k.box("flute", (1.16 * math.cos(a), 92.5, BZ + 1.16 * math.sin(a)), (0.18, 10.0, 0.18), "Polymer", bevel=0))
    p.append(shape("brake", [(98.4, 0.6), (98.4, 4.0), (102.0, 3.9), (105.4, 3.3), (105.4, 1.5), (102.0, 0.9)], 3.0, "WeaponMetal",
                   bevel=0.45, segments=S, samples=3))
    for i in range(4):
        y = 99.5 + i * 1.4
        for x in (1.52, -1.52):
            p.append(k.box("muzzle_port", (x, y, BZ), (0.2, 0.6, 1.6), "Polymer", bevel=0))
        p.append(k.box("brake_top", (0, y, 3.95), (1.6, 0.45, 0.12), "Polymer", bevel=0))
    p.append(k.cyl("crown", (0, 105.45, BZ), 0.95, 0.2, "GlowCyan", verts=16, bevel=0))
    # ---- Lower, grip, guard, magazine (same fit as v3) ----
    p.append(k.profile("lower", [(-12.5, -3.8), (-12.5, -0.4), (20, -0.4), (20, -3.6)], 4.4, "Polymer", bevel=0.5, segments=S))
    p.append(k.profile("grip", [(-3.5, -3.5), (3, -3.5), (1.6, -15.2), (0.4, -16.2), (-4.6, -15.6), (-5.0, -14.6)], 3.4,
                       "Polymer", bevel=0.7, segments=S))
    bw.grooves(p, 1.75, -1.2, -6.0, -13.0, 0.3, 3.2, 1.4, "WeaponMetal")
    p.append(k.box("grip_cap", (0, -1.8, -15.9), (3.0, 4.8, 0.6), "Ceramic", bevel=0.2))
    p.append(k.profile("guard", [(2.5, -4), (8.5, -4), (8.5, -9), (7, -9), (7, -5.2), (2.5, -5.2)], 1.0, "WeaponMetal",
                       bevel=0.2, segments=2))
    p.append(k.profile("mag", [(10.5, -3.6), (18.5, -3.6), (18.0, -11.6), (11.0, -11.6)], 3.6, "Polymer", bevel=0.5, segments=S))
    p.append(k.box("mag_base", (0, 14.5, -12.2), (4.0, 8.4, 1.2), "Ceramic", bevel=0.3, segments=2))
    for x in (1.84, -1.84):
        p.append(k.box("mag_window", (x, 14.6, -7.4), (0.1, 4.6, 1.6), "Display", bevel=0))
    p.append(k.box("mag_release", (-2.3, 9.6, -2.6), (0.4, 1.4, 1.0), "Steel", bevel=0.1))
    # ---- Bolt: handle on the right with a ribbed knob ----
    p.append(k.box("bolt_slot", (2.72, -6.0, 4.6), (0.1, 10.0, 1.0), "Polymer", bevel=0))
    p.append(k.cyl("bolt", (4.1, -9.0, 4.6), 0.55, 3.0, "Steel", axis="X", verts=12, bevel=0.08))
    p.append(k.cyl("bolt_knob", (5.8, -9.0, 4.6), 1.0, 1.4, "Polymer", axis="X", verts=16, bevel=0.2))
    for i in range(6):
        a = i * math.pi / 3
        p.append(k.box("knob_rib", (5.8, -9.0 + 1.02 * math.cos(a), 4.6 + 1.02 * math.sin(a)), (1.2, 0.14, 0.14), "Steel", bevel=0))
    # ---- Stock: one long swept skeletal frame, cheek riser, ceramic butt ----
    p.append(shape("stock_top", [(-15.0, 2.2), (-15.0, 6.4), (-26.0, 6.6), (-38.0, 5.8), (-45.4, 4.4), (-46.6, 2.4), (-43.6, 1.6),
                                 (-36.0, 3.0), (-24.0, 3.4)], 3.8, "Polymer", bevel=0.7, segments=S))
    p.append(shape("stock_low", [(-15.0, -3.8), (-15.0, 0.0), (-26.0, -1.6), (-38.0, -5.6), (-46.6, -7.0), (-46.6, -9.2),
                                 (-43.0, -9.6), (-35.0, -7.4), (-24.0, -4.8)], 3.6, "Polymer", bevel=0.7, segments=S))
    p.append(shape("stock_butt", [(-44.0, -9.4), (-44.0, 4.6), (-47.0, 4.0), (-47.0, -9.2)], 4.0, "Ceramic", bevel=0.55, segments=S,
                   samples=2))
    p.append(shape("cheek", [(-17.0, 6.2), (-18.0, 8.6), (-34.0, 8.8), (-38.0, 6.6)], 3.4, "Ceramic", bevel=0.55, segments=S,
                   samples=4))
    p.append(k.box("cheek_post", (0, -30.0, 6.0), (1.1, 1.1, 1.4), "Steel", bevel=0.15))
    p.append(k.box("butt_pad", (0, -47.8, -2.4), (4.2, 1.4, 14.4), "Polymer", bevel=0.4, segments=S))
    for i in range(7):
        p.append(k.box("butt_rib", (0, -48.55, -8.4 + i * 1.9), (3.8, 0.2, 0.5), "Polymer", bevel=0.06))
    p.append(k.cyl("qd_cup", (-1.95, -18.0, 2.2), 0.7, 0.4, "Steel", axis="X", verts=12, bevel=0.06))
    for y in (-22.0, -24.0, -26.0):
        for x in (1.92, -1.92):
            p.append(k.cyl("adj_hole", (x, y, 4.6), 0.32, 0.06, "Polymer", axis="X", verts=10, bevel=0))
    # ---- Integrated long scope ----
    sniper_v4_scope(p, S)
    # ---- Glow lines, counter ----
    for x in (2.72, -2.72):
        p.append(k.profile("light_line", [(30.0, 3.85), (72.0, 2.9), (72.2, 3.22), (30.0, 4.17)], 0.12, "GlowCyan", x=x, bevel=0))
        p.append(k.box("rx_line", (x, 6.0, 7.1), (0.12, 12.0, 0.16), "GlowCyan", bevel=0))
    p.append(k.box("counter_bezel", (0, -13.6, 8.3), (3.2, 3.2, 0.5), "Polymer", bevel=0.15))
    p.append(k.box("counter", (0, -13.6, 8.6), (2.6, 2.6, 0.2), "Display", bevel=0.05))
    # ---- Seams (grooves on the high poly), screws, vents ----
    for x in (2.72, -2.72):
        out = 1 if x > 0 else -1
        bw.seam(p, x, (-3.0, 3.6), (-3.0, 8.0))
        bw.seam(p, x, (20.0, 3.6), (20.0, 8.2))
        bw.seam(p, x, (46.0, 2.9), (46.0, 7.3))
        bw.seam(p, x, (66.0, 2.5), (66.0, 6.4))
        bw.seam(p, x, (-15.0, 5.4), (20.0, 5.4))
        for y, z in ((-13.6, 4.4), (-13.6, 7.0), (18.6, 4.2), (18.6, 7.6), (44.6, 3.6), (84.0, 3.2)):
            bw.screw(p, x + out * 0.02, y, z, 0.28)
        for i in range(8):
            y = 48.0 + i * 2.0
            z = 5.4 - (y - 48.0) * 0.06
            p.append(k.box("side_slot", (x, y, z), (0.08, 0.9, 1.3), "Polymer", bevel=0))
    for x in (2.52, -2.52):
        out = 1 if x > 0 else -1
        bw.seam(p, x, (38.0, 1.6), (38.0, -2.8))
        bw.seam(p, x, (60.0, 1.2), (60.0, -2.6))
        for y, z in ((23.0, -0.6), (36.0, -0.8), (58.0, -0.6), (74.0, -0.6)):
            bw.screw(p, x + out * 0.02, y, z, 0.26)
    for i in range(10):
        y = 26.0 + i * 1.4
        p.append(k.box("cool_slot", (0, y, 8.75 - max(0.0, y - 24.0) * 0.05), (2.6, 0.5, 0.2), "Polymer", bevel=0))
    return p, (0, 105.6, BZ)


def sniper_v4_scope(p, S):
    """A slim wedge housing on a low saddle (v4b: narrowed after the PC shots showed the first one filling 15% of the
    frame): hooded objective, slim eyecup, top turret and a rangefinder pod."""
    p.append(shape("scope_mount", [(0.0, 8.4), (13.0, 8.4), (12.0, 10.6), (1.0, 10.6)], 1.6, "WeaponMetal", bevel=0.3, segments=2,
                   samples=2))
    for x in (0.85, -0.85):  # mount 1.6 wide: >= 0.8 cm inside the housing's skin, or the bake reads it as worn edges
        bw.screw(p, x, 2.5, 9.4, 0.2)
        bw.screw(p, x, 10.5, 9.4, 0.2)
    # Uncut on the high poly ("plain_"): booleans on this curved side baked as glassy streaks.
    p.append(shape("plain_scope_body", [(-2.4, 10.6), (-3.0, 12.4), (-0.8, 14.5), (10.0, 15.0), (14.6, 15.2), (17.0, 14.7), (17.4, 11.2),
                                  (15.4, 10.3), (1.0, 10.4)], 3.2, "Ceramic", bevel=0.6, segments=S + 1))
    p.append(shape("scope_visor", [(12.0, 14.9), (12.8, 15.7), (18.0, 15.8), (18.9, 15.2), (18.6, 14.8), (15.6, 15.1)], 3.5, "Ceramic",
                   bevel=0.25, segments=S, samples=4))
    p.append(k.cyl("objective_bezel", (0, 17.6, SZ), 1.7, 0.7, "WeaponMetal", verts=32, bevel=0.12))
    p.append(k.cyl("objective_ring", (0, 17.97, SZ), 1.52, 0.07, "GlowCyan", verts=32, bevel=0))
    p.append(k.cyl("objective_lens", (0, 17.9, SZ), 1.42, 0.05, "Lens", verts=32, bevel=0))
    p.append(k.cyl("eyecup", (0, -3.3, SZ - 0.4), 1.1, 1.1, "Polymer", verts=24, bevel=0.18, radius2=1.24))
    for i in range(2):
        p.append(k.cyl("eye_rib", (0, -3.6 + i * 0.45, SZ - 0.4), 1.3, 0.12, "Polymer", verts=24, bevel=0))
    p.append(k.cyl("eye_lens", (0, -3.88, SZ - 0.4), 0.85, 0.05, "Lens", verts=24, bevel=0))
    # Top elevation turret with a knurled ring and a glowing cap; right battery cap; left rangefinder pod.
    p.append(k.cyl("turret_top", (0, 5.0, 15.3), 0.9, 0.9, "WeaponMetal", axis="Z", verts=20, bevel=0.1))
    for i in range(10):
        a = i * math.pi / 5
        p.append(k.box("turret_knurl", (0.92 * math.cos(a), 5.0 + 0.92 * math.sin(a), 15.35), (0.1, 0.1, 0.7), "Steel", bevel=0))
    p.append(k.cyl("turret_cap", (0, 5.0, 15.78), 0.62, 0.1, "GlowCyan", axis="Z", verts=16, bevel=0))
    p.append(k.cyl("battery", (1.72, 5.0, SZ - 0.4), 0.6, 0.4, "WeaponMetal", axis="X", verts=20, bevel=0.07))
    p.append(shape("rangefinder", [(10.0, 10.8), (15.8, 10.8), (16.4, 11.6), (15.8, 12.4), (10.4, 12.4)], 0.7, "WeaponMetal",
                   x=-1.9, bevel=0.15, segments=2, samples=2))
    p.append(k.box("rangefinder_lens", (-1.9, 16.42, 11.6), (0.45, 0.08, 0.7), "Lens", bevel=0))
    p.append(k.box("scope_display", (-1.66, 4.0, 11.5), (0.05, 4.4, 0.7), "Display", bevel=0))
    for x in (1.62, -1.62):
        out = 1 if x > 0 else -1
        p.append(shape("scope_band", [(-1.6, 10.6), (15.0, 10.6), (15.6, 11.1), (15.0, 11.5), (-1.2, 11.5)], 0.2, "WeaponMetal",
                       x=x + out * 0.05, bevel=0.07, segments=2, samples=2))
        bw.seam(p, x, (8.5, 11.6), (8.5, 14.8))
        p.append(k.box("scope_line", (x + out * 0.02, 3.5, 14.0), (0.06, 9.0, 0.11), "GlowCyan", bevel=0))


def extra_cuts(c):
    """Machined detail for the high poly (pro_highpoly.Collector)."""
    for s in (1, -1):
        c.cutter_box((s * 1.95, -30.0, 4.6), (0.6, 8.0, 0.9), rot_x=math.radians(-6))      # stock top pocket
        c.cutter_box((s * 1.85, -31.0, -4.0), (0.6, 7.0, 0.9), rot_x=math.radians(-20))    # stock low pocket
        c.cutter_box((s * 2.2, 4.0, -1.8), (0.4, 26.0, 0.14))                              # lower relief groove
        c.cutter_box((s * 1.85, 14.5, -9.6), (0.3, 5.0, 1.2))                              # magazine panel
        c.cutter_box((s * 2.75, 32.0, 6.6), (0.3, 18.0, 0.14), rot_x=math.radians(-3.5))   # shell groove
        c.cutter_box((s * 2.75, 8.0, 7.4), (0.3, 10.0, 1.0))                               # receiver recessed panel
