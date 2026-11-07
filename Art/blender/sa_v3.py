"""Kestrel sidearm v3 (2026-11-07): reshaped in the Vanguard BR v7 language.

v2 was a slab slide on a slab frame. v3 keeps the grip, trigger guard, magazine (and its split box) and the hand fit,
and makes:
 - a dark steel slide with a swept, chamfered nose and a white ceramic cover over its front half;
 - a sculpted frame with a long dust cover, an accessory rail and a beavertail over the web of the hand;
 - a short ported compensator, rear slide serrations and a cyan line on the cover.
Original design. Halo Infinite is the fidelity bar only; nothing is traced or copied.

Mesh space as build_weapons.py: X across (the first-person camera sees -X), Y forward, Z up, cm.
"""
import math

import sb_kit as k
import build_weapons as bw
from br_v7 import shape

BZ = 2.2  # bore height


def sidearm_v3():
    p = []
    S = 3
    # ---- Slide: dark steel, swept nose; ceramic cover over the front ----
    # The slide stops inside the cover: a part overlapping just under another's skin reads as worn edges in the bake.
    p.append(shape("slide", [(-6.6, 0.4), (-6.6, 4.2), (-5.8, 5.3), (6.0, 5.4), (8.0, 5.4), (8.0, 0.4)], 3.2, "Steel",
                   bevel=0.45, segments=S, samples=3))
    p.append(shape("slide_cover", [(6.4, 0.45), (6.4, 5.1), (7.4, 5.62), (13.0, 5.7), (16.4, 5.15), (18.0, 3.8), (18.0, 0.45),
                                   (12.0, 0.45)], 3.5, "Ceramic", bevel=0.5, segments=S, samples=4))
    p.append(k.cyl("barrel", (0, 18.2, BZ), 0.75, 0.8, "WeaponMetal", verts=16, bevel=0.1))
    # Compensator: a short ported block ahead of the slide.
    p.append(shape("comp", [(18.3, 0.2), (18.3, 4.4), (20.4, 4.0), (21.2, 3.0), (21.2, 0.6), (20.0, 0.2)], 3.0, "WeaponMetal",
                   bevel=0.4, segments=S, samples=3))
    for y in (19.2, 20.2):
        p.append(k.box("brake_top", (0, y, 4.15), (1.4, 0.45, 0.12), "Polymer", bevel=0))
        for x in (1.52, -1.52):
            p.append(k.box("muzzle_port", (x, y, 2.6), (0.2, 0.5, 1.2), "Polymer", bevel=0))
    p.append(k.cyl("crown", (0, 21.25, BZ), 0.7, 0.12, "GlowCyan", verts=16, bevel=0))
    # ---- Frame: sculpted, with a long dust cover, rail and beavertail ----
    p.append(shape("frame", [(-6.0, -2.2), (-6.4, 0.4), (17.4, 0.4), (17.6, -0.8), (15.0, -2.4), (4.0, -2.6)], 3.0,
                   "WeaponMetal", bevel=0.4, segments=S, samples=3))
    p.append(shape("beavertail", [(-5.6, -0.2), (-7.6, -0.3), (-8.4, -1.0), (-7.8, -1.7), (-5.6, -2.4)], 2.6, "WeaponMetal",
                   bevel=0.35, segments=S, samples=3))
    p.append(k.box("rail", (0, 12.0, -2.75), (2.2, 6.0, 0.5), "WeaponMetal", bevel=0.12))
    bw.rail_teeth(p, 9.8, 14.4, -3.1, 2.0, step=1.1)
    p.append(k.profile("grip", [(-5.5, -2.2), (1.5, -2.2), (0.2, -11), (-1.2, -12), (-7.2, -11.3)], 3.3, "Polymer", bevel=0.6,
                       segments=S))
    p.append(k.profile("guard", [(1.5, -2.4), (6.5, -2.4), (6.5, -6.0), (5.2, -6.0), (5.2, -3.5), (1.5, -3.5)], 0.9,
                       "WeaponMetal", bevel=0.2, segments=2))
    bw.grooves(p, 1.62, -0.9, -4.6, -9.0, 0.2, 2.2, 1.1, "WeaponMetal")
    p.append(k.box("mag_base", (0, -3.4, -11.8), (3.6, 7.0, 1.2), "WeaponMetal", bevel=0.3))
    # ---- Sights, controls ----
    p.append(k.box("front_sight", (0, 15.6, 6.0), (0.7, 1.2, 0.9), "WeaponMetal", bevel=0.15))
    p.append(k.box("rear_sight", (0, -5.4, 5.95), (2.6, 1.3, 1.0), "WeaponMetal", bevel=0.15))
    for x in (0.7, -0.7):
        p.append(k.box("sight_dot", (x, -6.08, 6.05), (0.4, 0.1, 0.4), "GlowCyan", bevel=0))
    p.append(k.box("sight_dot_front", (0, 16.23, 6.15), (0.35, 0.1, 0.35), "GlowCyan", bevel=0))
    p.append(k.box("ejection_port", (-1.6, 4.0, 3.6), (0.3, 3.6, 1.6), "Polymer", bevel=0.1))
    p.append(k.box("slide_stop", (-1.7, 3.0, 0.2), (0.4, 3.0, 0.7), "Steel", bevel=0.12))
    p.append(k.box("safety", (-1.7, -4.4, 0.9), (0.4, 1.4, 0.7), "Steel", bevel=0.12))
    p.append(k.box("mag_release", (-1.7, 1.0, -3.2), (0.4, 1.0, 0.9), "Steel", bevel=0.12))
    p.append(k.box("counter", (0, -2.6, 5.5), (1.6, 2.2, 0.15), "Display", bevel=0.03))
    for x in (1.77, -1.77):
        out = 1 if x > 0 else -1
        p.append(k.box("strip", (x + out * 0.02, 12.0, 2.85), (0.08, 10.0, 0.16), "GlowCyan", bevel=0))
        # Serrations: rear (on the steel) and front (through the cover's edge).
        for i in range(6):
            p.append(k.box("side_slot", (x - out * 0.17, -5.4 + i * 0.6, 3.0), (0.08, 0.26, 2.4), "Polymer", bevel=0))
        bw.seam(p, x, (16.0, 1.0), (16.0, 5.0))
        bw.screw(p, x, 15.2, 1.2, 0.22)
        bw.screw(p, x - out * 0.17, -4.6, -0.8, 0.22)
        bw.seam(p, x - out * 0.27, (-5.6, -0.4), (16.6, -0.4))
    return p, (0, 21.4, BZ)


def extra_cuts(c):
    """Machined detail for the high poly (pro_highpoly.Collector)."""
    for s in (1, -1):
        c.cutter_box((s * 1.8, 14.6, 4.7), (0.24, 4.0, 0.4))                           # cover lightening groove
        c.cutter_box((s * 1.55, 6.0, -1.5), (0.24, 12.0, 0.14))                        # frame relief groove
        c.cutter_box((s * 1.55, 13.0, -1.2), (0.24, 4.0, 0.9))                         # dust cover pocket
    c.cutter_box((0.0, 9.0, 5.7), (1.2, 7.0, 0.16))                                    # top flat channel
