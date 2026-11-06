"""Builds Shieldbreak's weapon models and exports one FBX per weapon.

    python build_weapons.py [--render [preview_dir]]

FBX files go to Art/Export/Weapons (committed, imported on the PC by Tools/ImportArt.bat); previews to preview_dir.

Mesh space matches Lyra's guns (and so the hand bone the code attaches to): origin at the top of the pistol grip,
barrel along +Y, up +Z, X across the gun. Every gun has a SOCKET_Muzzle (Unreal socket "Muzzle") at the barrel tip.
Material slots are named after the palette in sb_kit.py; Tools/import_art.py builds matching Unreal materials.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb_kit as k  # noqa: E402


def rail_teeth(p, y0, y1, z, width, step=1.6, mat="WeaponMetal"):
    """Cross-slots along a top rail: small raised teeth every step cm (reads as an accessory rail up close)."""
    y = y0
    while y <= y1:
        p.append(k.box("tooth", (0, y, z), (width, 0.8, 0.6), mat, bevel=0.1))
        y += step


def grooves(p, x, y, z0, z1, depth, length, step, mat="WeaponMetal"):
    """Horizontal ribs down a grip or magazine face (both sides when x != 0)."""
    z = z0
    while z >= z1:
        for side in ((x, -x) if x else (0,)):
            p.append(k.box("groove", (side, y, z), (depth, length, 0.45), mat, bevel=0.08))
        z -= step


def battle_rifle():
    """Vanguard BR (v4, 2026-11-06): a full redesign. Layered hard-surface shell: a gunmetal upper that sweeps into an
    angular forward shroud, pale ceramic armor panels over it, a dark polymer lower and skeleton stock, a magazine
    ahead of the trigger, a compact enclosed optic and thin cyan light lines. Original design; Halo Infinite is only
    the fidelity bar. The grip, trigger and origin stay where the first-person hand fit expects them (TRIGGERS)."""
    p = []
    S = 3  # bevel segments on the big shapes: rounded highlights on every edge at first-person distance
    # ---- Upper receiver and forward shroud (one gunmetal mass, chamfered nose) ----
    p.append(k.profile("upper", [(-14, 0.2), (-14, 6.0), (-8, 8.6), (16, 8.6), (21, 7.6), (21, 0.2)], 5.0, "WeaponMetal",
                       bevel=0.6, segments=S))
    p.append(k.profile("shroud", [(20, -2.6), (20, 7.4), (30, 7.8), (44, 6.0), (50, 3.9), (52.5, 1.8), (50, -0.6), (43, -3.4), (30, -3.6)], 5.6,
                       "WeaponMetal", bevel=0.7, segments=S))
    # Ceramic armor panels, raised off both sides of the upper and the shroud, with a forward-swept cut.
    for x in (2.75, -2.75):
        p.append(k.profile("panel_upper", [(-11, 1.4), (-11, 6.8), (12, 6.8), (16, 4.6), (16, 1.4)], 0.6, "Ceramic",
                           x=x, bevel=0.22, segments=S))
    for x in (3.05, -3.05):
        p.append(k.profile("panel_shroud", [(22.5, 0.2), (22.5, 6.6), (32, 6.9), (42, 5.4), (48.5, 3.0), (47.5, 0.6), (40, -2.4), (27, -2.6)],
                           0.6, "Ceramic", x=x, bevel=0.22, segments=S))
        # Dark vent window through the panel, and a light line under it.
        p.append(k.profile("vent_window", [(29, 1.6), (29, 4.6), (36, 4.4), (38.5, 3.0), (38.5, 1.6)], 0.62, "Polymer",
                           x=x + (0.02 if x > 0 else -0.02), bevel=0.1))
        for i in range(4):
            p.append(k.box("vent_fin", (x + (0.33 if x > 0 else -0.33), 30.5 + i * 2.0, 3.1), (0.22, 0.5, 2.6), "Steel", bevel=0.05))
        p.append(k.profile("light_line", [(26, -1.0), (39, -1.0), (46.5, 1.4), (46.0, 1.8), (38.8, -0.6), (26, -0.6)], 0.14, "GlowCyan",
                           x=x + (0.32 if x > 0 else -0.32), bevel=0))
    # Stepped cross-section: a narrower raised layer along the tops, so the silhouette reads layered, not slab-sided.
    p.append(k.profile("shroud_top", [(21, 6.6), (21, 8.4), (30, 8.7), (42, 7.0), (46.5, 4.8), (45, 4.4), (30, 7.0)], 4.2,
                       "WeaponMetal", bevel=0.4, segments=S))
    p.append(k.profile("upper_top", [(-7, 8.0), (-5, 9.2), (15, 9.2), (19, 8.0)], 4.0, "WeaponMetal", bevel=0.35, segments=S))
    # Top spine of the shroud: a rail with low teeth.
    p.append(k.box("shroud_spine", (0, 30, 9.0), (2.4, 16, 0.7), "WeaponMetal", bevel=0.25, segments=S))
    rail_teeth(p, 23, 37, 9.55, 2.5, step=2.0)
    # ---- Lower receiver, grip, guard ----
    p.append(k.profile("lower", [(-12.5, -3.8), (-12.5, 0.6), (20.5, 0.6), (20.5, -2.4), (17, -3.8)], 4.6, "Polymer",
                       bevel=0.5, segments=S))
    p.append(k.profile("grip", [(-3.5, -3.5), (3, -3.5), (1.6, -15.2), (0.4, -16.2), (-4.6, -15.6), (-5.0, -14.6)], 3.4,
                       "Polymer", bevel=0.7, segments=S))
    grooves(p, 1.75, -1.2, -6.0, -13.0, 0.3, 3.2, 1.4, "WeaponMetal")
    p.append(k.box("grip_cap", (0, -1.8, -15.9), (3.0, 4.8, 0.6), "WeaponMetal", bevel=0.2))
    p.append(k.profile("guard", [(3, -3.9), (9.5, -3.9), (9.5, -9.2), (7.9, -9.2), (7.9, -5.2), (3, -5.2)], 1.1,
                       "WeaponMetal", bevel=0.25, segments=S))
    # Magazine ahead of the guard, raked forward, with a ceramic base plate and a cyan round-count window.
    p.append(k.profile("mag", [(10.2, -3.6), (16.4, -3.6), (18.6, -15.0), (12.0, -15.8)], 3.6, "Polymer", bevel=0.6, segments=S))
    p.append(k.profile("mag_base", [(11.8, -15.4), (18.8, -14.6), (19.2, -16.2), (11.6, -17.0)], 4.0, "Ceramic", bevel=0.3, segments=S))
    p.append(k.box("mag_well", (0, 13.3, -3.9), (4.8, 7.6, 1.2), "WeaponMetal", bevel=0.3))
    for x in (1.82, -1.82):
        p.append(k.box("mag_window", (x, 14.6, -8.5), (0.12, 2.2, 4.0), "Display", bevel=0))
    # ---- Barrel and muzzle ----
    p.append(k.cyl("barrel", (0, 53.5, 1.6), 1.15, 3, "Steel", verts=12))
    p.append(k.box("muzzle", (0, 56.6, 1.6), (2.8, 3.6, 2.6), "WeaponMetal", bevel=0.5, segments=S, taper=(0.8, 0.8)))
    for y in (55.9, 57.4):
        for x in (1.32, -1.32):
            p.append(k.box("muzzle_port", (x, y, 1.6), (0.2, 0.6, 1.4), "Polymer", bevel=0))
    p.append(k.cyl("muzzle_ring", (0, 58.5, 1.6), 0.9, 0.3, "GlowCyan", verts=12, bevel=0))
    # Chin blade under the shroud's nose.
    p.append(k.profile("chin", [(34, -3.4), (49, -0.8), (47.5, -2.6), (40, -5.4), (35.5, -5.2)], 1.8, "Ceramic", bevel=0.3, segments=S))
    # ---- Stock: skeleton frame in polymer with a ceramic cheek rest and a rubber pad ----
    p.append(k.profile("stock_top", [(-14, 1.5), (-14, 7.0), (-24, 6.6), (-35, 5.4), (-37.5, 3.6), (-33, 3.0), (-24, 3.6), (-14, 1.5)][:-1],
                       4.0, "Polymer", bevel=0.6, segments=S))
    p.append(k.profile("stock_low", [(-14, -3.4), (-14, 0.6), (-24, 0.4), (-34, -3.2), (-37.5, -6.4), (-35, -7.4), (-24, -4.2)],
                       3.8, "Polymer", bevel=0.6, segments=S))
    p.append(k.profile("stock_strut", [(-34.5, -6.5), (-37.5, -6.4), (-37.5, 4.0), (-34.5, 4.6)], 4.2, "WeaponMetal",
                       bevel=0.4, segments=S))
    p.append(k.box("cheek", (0, -24, 7.0), (3.4, 13, 1.0), "Ceramic", bevel=0.35, segments=S))
    p.append(k.box("butt_pad", (0, -38.4, -1.2), (4.4, 1.4, 12.6), "Polymer", bevel=0.4, segments=S))
    # ---- Optic: a raised scope (v6, 2026-11-06: Vincent wanted a raised scope, not the low box) ----
    battle_rifle_scope(p, S)
    # ---- Details the first-person view sees: ammo counter, charging handle, ejection port, seams, bolts ----
    p.append(k.box("counter_bezel", (0, -9.5, 8.85), (3.6, 4.0, 0.5), "Polymer", bevel=0.15))
    p.append(k.box("counter", (0, -9.5, 9.15), (3.0, 3.4, 0.25), "Display", bevel=0.05))
    p.append(k.box("charging_handle", (-2.9, -9.5, 5.6), (0.8, 3.2, 1.2), "Steel", bevel=0.25))
    p.append(k.box("ejection_port", (2.55, -2, 3.6), (0.3, 6.0, 2.2), "Polymer", bevel=0.15))
    p.append(k.box("selector", (-2.6, -1.5, -1.6), (0.4, 1.6, 0.9), "Steel", bevel=0.1))
    for x in (3.1, -3.1):
        p.append(k.box("seam", (x * 0.9, 18.5, 3.6), (0.12, 0.3, 6.5), "Polymer", bevel=0))
        p.append(k.box("upper_line", (x * 0.92, 2, 7.6), (0.1, 20, 0.22), "GlowCyan", bevel=0))
    battle_rifle_detail(p)
    return p, (0, 58.8, 1.6)


def battle_rifle_scope(p, S):
    """The BR's raised scope: a round tube on two angular clamp risers, a flared objective with a cyan ring, a ribbed
    eyepiece, a pale ceramic armour hood over the top with a swept nose, a ranging display and a side turret. It sits
    high enough that the first-person view sees its whole left side above the receiver, like a real raised optic."""
    TZ = 14.4  # tube axis height
    # Clamp risers: angular blocks from the rail up to the tube, each with a half-ring cap.
    for y in (-0.5, 8.0):
        p.append(k.profile("scope_riser", [(y - 1.5, 9.4), (y + 1.5, 9.4), (y + 1.0, TZ - 0.6), (y - 1.0, TZ - 0.6)], 2.6,
                           "WeaponMetal", bevel=0.3, segments=2))
        p.append(k.cyl("scope_ring", (0, y, TZ), 1.95, 1.8, "WeaponMetal", verts=20, bevel=0.15))
        for x in (1.35, -1.35):
            screw(p, x, y, 10.6, 0.24)
        p.append(k.box("riser_lever", (-1.55, y, 11.2), (0.5, 1.6, 0.8), "Steel", bevel=0.15))
    # Tube, objective bell and eyepiece.
    p.append(k.cyl("scope_tube", (0, 4.0, TZ), 1.55, 17.0, "WeaponMetal", verts=20, bevel=0.1))
    p.append(k.cyl("scope_bell", (0, 14.3, TZ), 1.55, 3.6, "WeaponMetal", verts=20, bevel=0.15, radius2=2.35))
    p.append(k.cyl("scope_bezel", (0, 16.35, TZ), 2.45, 0.6, "WeaponMetal", verts=24, bevel=0.12))
    p.append(k.cyl("scope_glow", (0, 16.68, TZ), 2.2, 0.08, "GlowCyan", verts=24, bevel=0))
    p.append(k.cyl("scope_lens", (0, 16.7, TZ), 1.95, 0.06, "Lens", verts=24, bevel=0))
    p.append(k.cyl("eye_bell", (0, -5.7, TZ), 2.0, 2.4, "WeaponMetal", verts=20, bevel=0.15, radius2=1.55))
    p.append(k.cyl("eyepiece", (0, -7.6, TZ), 2.05, 1.6, "Polymer", verts=20, bevel=0.2))
    for i in range(4):
        p.append(k.cyl("eye_rib", (0, -8.1 + i * 0.4, TZ), 2.12, 0.16, "Polymer", verts=20, bevel=0))
    p.append(k.cyl("eye_lens", (0, -8.45, TZ), 1.6, 0.06, "Lens", verts=20, bevel=0))
    # Ceramic armour hood over the top: a swept shell with a raised spine and a nose that overhangs the lens.
    p.append(k.profile("scope_hood", [(-4.5, TZ + 1.3), (-3.5, TZ + 2.1), (11.0, TZ + 2.1), (15.0, TZ + 2.7), (17.2, TZ + 2.4),
                                      (17.0, TZ + 1.9), (13.5, TZ + 1.5), (-4.0, TZ + 1.2)], 3.4, "Ceramic", bevel=0.3, segments=S))
    p.append(k.box("hood_spine", (0, 5.5, TZ + 2.3), (1.0, 14.0, 0.5), "WeaponMetal", bevel=0.15))
    for x in (1.71, -1.71):
        p.append(k.box("hood_line", (x, 6.0, TZ + 1.65), (0.08, 15.0, 0.18), "GlowCyan", bevel=0))
    # Side turret (left) with knurling, a battery cap (right) and a slim ranging display on the left of the tube.
    p.append(k.cyl("turret", (-2.0, 3.0, TZ), 0.95, 1.0, "WeaponMetal", axis="X", verts=20, bevel=0.12))
    p.append(k.cyl("turret_cap", (-2.55, 3.0, TZ), 0.7, 0.2, "Steel", axis="X", verts=20, bevel=0.04))
    for i in range(10):
        a = i * math.pi / 5
        p.append(k.box("turret_knurl", (-2.0, 3.0 + 0.98 * math.cos(a), TZ + 0.98 * math.sin(a)), (0.85, 0.13, 0.13), "Steel", bevel=0))
    p.append(k.cyl("battery", (1.9, 3.0, TZ), 0.8, 0.7, "WeaponMetal", axis="X", verts=16, bevel=0.1))
    p.append(k.box("range_display", (-1.62, 10.5, TZ - 0.4), (0.1, 3.0, 1.0), "Display", bevel=0))
    p.append(k.box("range_housing", (-1.45, 10.5, TZ - 0.4), (0.4, 3.6, 1.4), "Polymer", bevel=0.12))


def screw(p, x, y, z, r=0.32, mat="Steel"):
    """A socket-head screw on a side face (x != 0): a domed head with a dark hex recess."""
    out = 1 if x > 0 else -1
    p.append(k.cyl("screw", (x, y, z), r, 0.22, mat, axis="X", verts=10, bevel=0.06))
    p.append(k.cyl("screw_hex", (x + out * 0.1, y, z), r * 0.45, 0.06, "Polymer", axis="X", verts=6, bevel=0))


def seam(p, x, a, b, w=0.07, mat="Polymer"):
    """A recessed panel line from a=(y,z) to b=(y,z) on a side face: a thin dark strip just proud of it."""
    (y0, z0), (y1, z1) = a, b
    length = math.hypot(y1 - y0, z1 - z0)
    obj = k.box("seam", (0, 0, 0), (0.04, length, w), mat, bevel=0)  # built at the origin so it rotates about its own centre
    obj.rotation_euler = (math.atan2(z1 - z0, y1 - y0), 0, 0)
    obj.location = (x, (y0 + y1) * 0.5, (z0 + z1) * 0.5)
    p.append(obj)


def battle_rifle_detail(p):
    """Vanguard BR v5 detail layer (2026-11-06): the parts a first-person camera reads up close. Panel lines, screws,
    cooling slots, a ribbed forend grip where the left hand sits, a ported muzzle brake, optic knobs and bezels,
    magazine ribs and a witness ladder, takedown pins, a bolt release and stamped data plates. The camera sees the
    gun's left side (-X) and top most, so that is where the density goes."""
    # ---- Ceramic panel seams and fasteners, both sides ----
    for x in (3.36, -3.36):
        out = 1 if x > 0 else -1
        seam(p, x, (27.0, -2.4), (27.0, 6.75))
        seam(p, x, (41.0, -2.0), (41.0, 5.6))
        seam(p, x, (22.8, 5.0), (27.0, 5.0))
        seam(p, x, (41.0, 1.0), (47.6, 1.0))
        for y, z in ((24.0, 5.6), (24.0, -1.4), (44.0, 4.0), (39.0, -1.5)):
            screw(p, x + out * 0.05, y, z, 0.3)
    for x in (3.06, -3.06):
        out = 1 if x > 0 else -1
        seam(p, x, (-4.0, 1.5), (-4.0, 6.7))
        seam(p, x, (6.0, 1.5), (6.0, 6.7))
        seam(p, x, (-11.0, 4.2), (-4.0, 4.2))
        for y, z in ((-9.6, 2.3), (-9.6, 5.9), (14.2, 2.3), (14.2, 5.9), (1.0, 2.3)):
            screw(p, x + out * 0.05, y, z, 0.28)
    # ---- Top cooling slots down the shroud's sloped spine, and side slots on the raised top layer ----
    for i in range(7):
        y = 38.0 + i * 1.15
        z = 8.7 - (y - 30.0) * (1.7 / 12.0)
        if y > 42.0:
            z = 7.0 - (y - 42.0) * (2.2 / 4.5)
        p.append(k.box("cool_slot", (0, y, z + 0.06), (2.6, 0.45, 0.2), "Polymer", bevel=0))
    for x in (2.12, -2.12):
        for i in range(5):
            p.append(k.box("side_slot", (x, 22.5 + i * 1.3, 7.7), (0.08, 0.7, 0.45), "Polymer", bevel=0))
    # ---- Forend underside: the support hand's grip. A polymer pad with transverse ribs and accessory slots ----
    p.append(k.box("forend_pad", (0, 26.5, -3.85), (5.0, 11.0, 0.9), "Polymer", bevel=0.3, segments=2))
    for i in range(11):
        p.append(k.box("forend_rib", (0, 21.8 + i * 0.95, -4.38), (4.6, 0.38, 0.28), "Polymer", bevel=0.08))
    for x in (2.55, -2.55):
        for y in (23.0, 26.5, 30.0):
            p.append(k.box("mlok", (x, y, -3.6), (0.1, 2.2, 0.6), "WeaponMetal", bevel=0))
        p.append(k.box("hand_stop", (x * 0.7, 32.4, -4.6), (1.0, 0.8, 1.4), "WeaponMetal", bevel=0.25))
    # ---- Muzzle brake: crown, three rows of ports, top vents ----
    p.append(k.cyl("crown", (0, 58.55, 1.6), 1.25, 0.25, "Steel", verts=16, bevel=0.05))
    p.append(k.cyl("barrel_collar", (0, 54.6, 1.6), 1.45, 0.6, "WeaponMetal", verts=16, bevel=0.1))
    for y in (55.3, 56.3, 57.3):
        p.append(k.box("brake_top", (0, y, 2.85), (1.4, 0.4, 0.1), "Polymer", bevel=0))
        for x in (1.33, -1.33):
            p.append(k.box("brake_low", (x, y + 0.15, 0.9), (0.1, 0.4, 0.5), "Polymer", bevel=0))
    # ---- Rails ahead of and behind the optic ----
    rail_teeth(p, -6.0, -3.0, 9.45, 3.0, step=1.0)
    rail_teeth(p, 9.6, 14.0, 9.45, 3.0, step=1.0)
    # ---- Charging handle: ribbed latch in a channel, ejection port cover with ribs and a deflector ----
    p.append(k.box("ch_channel", (-2.56, -8.5, 5.6), (0.08, 9.0, 0.7), "Polymer", bevel=0))
    for i in range(3):
        p.append(k.box("ch_rib", (-3.35, -10.6 + i * 0.6, 5.6), (0.25, 0.25, 1.3), "Steel", bevel=0.05))
    p.append(k.box("port_cover", (2.62, -2, 3.6), (0.12, 5.4, 1.7), "WeaponMetal", bevel=0.05))
    for i in range(6):
        p.append(k.box("port_rib", (2.7, -4.2 + i * 0.9, 3.6), (0.08, 0.3, 1.5), "Steel", bevel=0))
    p.append(k.box("deflector", (2.7, 1.6, 4.8), (0.6, 1.2, 1.2), "WeaponMetal", bevel=0.25))
    # ---- Lower: takedown pins, bolt release paddle, ambidextrous selector, sling points ----
    for y in (-10.5, 18.6):
        for x in (2.33, -2.33):
            p.append(k.cyl("pin", (x, y, -1.6), 0.42, 0.18, "Steel", axis="X", verts=12, bevel=0.04))
    p.append(k.box("bolt_release", (-2.55, 8.6, -2.3), (0.4, 2.6, 1.2), "Steel", bevel=0.15))
    for i in range(3):
        p.append(k.box("bolt_release_rib", (-2.8, 7.8 + i * 0.6, -2.3), (0.1, 0.2, 1.1), "WeaponMetal", bevel=0))
    p.append(k.box("selector_r", (2.6, -1.5, -1.6), (0.4, 1.6, 0.9), "Steel", bevel=0.1))
    p.append(k.ring("sling_front", (-2.9, 34.0, -2.6), 0.6, 0.14, "Steel", axis="X", verts=12, tube_verts=6))
    p.append(k.cyl("qd_cup", (-2.35, -16.0, 2.0), 0.7, 0.4, "Steel", axis="X", verts=12, bevel=0.06))
    # ---- Magazine: vertical ribs, a witness-hole ladder and a pull tab ----
    for x in (1.86, -1.86):
        for i in range(3):
            seam(p, x, (12.6 + i * 1.5, -4.6), (13.8 + i * 1.6, -14.4), w=0.1)
    for i in range(5):
        z = -5.4 - i * 1.6
        p.append(k.cyl("witness", (-1.86, 16.4 + (z + 3.6) * -0.19, z), 0.22, 0.08, "Display", axis="X", verts=8, bevel=0))
    p.append(k.box("mag_tab", (0, 15.4, -17.2), (2.4, 2.2, 0.5), "Polymer", bevel=0.15))
    # ---- Stamped data plates on the left: a ceramic tag with dark text bars ----
    p.append(k.box("data_plate", (-3.12, -6.5, 1.4), (0.06, 4.2, 1.2), "Ceramic", bevel=0.03))
    for i, w in enumerate((3.2, 2.1, 2.8)):
        p.append(k.box("plate_text", (-3.16, -6.5 - (3.2 - w) * 0.5, 1.8 - i * 0.38), (0.03, w, 0.16), "Polymer", bevel=0))
    p.append(k.box("warn_tag", (-3.4, 36.0, 5.6), (0.04, 2.4, 0.5), "GlowCyan", bevel=0))
    # ---- Stock: adjustment holes and a cheek rest seam ----
    for y in (-18.0, -20.0, -22.0, -24.0):
        for x in (2.02, -2.02):
            p.append(k.cyl("adj_hole", (x, y, 0.4), 0.35, 0.06, "Polymer", axis="X", verts=10, bevel=0))
    seam(p, 1.72, (-30.0, 6.4), (-18.0, 6.4))
    seam(p, -1.72, (-30.0, 6.4), (-18.0, 6.4))


def sidearm():
    """Kestrel sidearm: long slab-sided slide, compensator cuts, a cyan strip down each side."""
    p = []
    p.append(k.profile("slide", [(-6.5, 0), (-6.5, 4.6), (-5.5, 5.4), (16, 5.4), (17.5, 4), (17.5, 0)], 3.4, "Steel", bevel=0.45))
    p.append(k.profile("frame", [(-5.5, -2.2), (-5.5, 0.2), (17, 0.2), (17, -1.6), (14, -2.6), (4, -2.6)], 3.0, "WeaponMetal", bevel=0.4))
    p.append(k.profile("grip", [(-5.5, -2.2), (1.5, -2.2), (0.2, -11), (-1.2, -12), (-7.2, -11.3)], 3.3, "Polymer", bevel=0.6))
    p.append(k.profile("guard", [(1.5, -2.4), (6.5, -2.4), (6.5, -6.0), (5.2, -6.0), (5.2, -3.5), (1.5, -3.5)], 0.9, "WeaponMetal", bevel=0.2))
    p.append(k.box("serrations", (0, -4.2, 3), (3.55, 2.5, 3.6), "WeaponMetal", bevel=0.2))
    p.append(k.cyl("barrel", (0, 17.7, 2.2), 0.8, 1.2, "Polymer", verts=10, bevel=0.1))
    for y in (11.5, 13.5):
        p.append(k.box("top_vent", (0, y, 5.45), (1.6, 1.0, 0.2), "Polymer", bevel=0))
    p.append(k.box("front_sight", (0, 15.8, 5.9), (0.7, 1.2, 1.0), "WeaponMetal", bevel=0.15))
    p.append(k.box("rear_sight", (0, -5.6, 5.9), (2.6, 1.2, 1.0), "WeaponMetal", bevel=0.15))
    for x in (1.72, -1.72):
        p.append(k.box("strip", (x, 5.5, 3.4), (0.12, 14, 0.45), "GlowCyan", bevel=0))
    p.append(k.box("mag_base", (0, -3.4, -11.8), (3.6, 7.0, 1.2), "WeaponMetal", bevel=0.3))
    # v2 detail: rear slide serrations, sight dots, an ejection port and grip ribs.
    for i in range(5):
        p.append(k.box("serration", (0, -5.2 + i * 0.8, 3.0), (3.6, 0.3, 3.0), "Polymer", bevel=0))
    for x in (0.7, -0.7):
        p.append(k.box("sight_dot", (x, -6.25, 6.0), (0.4, 0.1, 0.4), "GlowCyan", bevel=0))
    p.append(k.box("sight_dot_front", (0, 16.45, 6.15), (0.35, 0.1, 0.35), "GlowCyan", bevel=0))
    p.append(k.box("ejection_port", (-1.6, 4.0, 3.6), (0.3, 4.0, 1.8), "Polymer", bevel=0.1))
    grooves(p, 1.62, -3.6, -4.5, -10.0, 0.25, 5.4, 1.1, "WeaponMetal")
    return p, (0, 18.4, 2.2)


def sniper():
    """Longshot SR (v3, 2026-11-06): a white long rifle in the Vanguard BR's language. A slim ceramic receiver and a
    long faceted barrel shroud over a gunmetal frame, an exposed fluted barrel ending in a slotted brake, a boxy
    angular scope with a large objective, a skeleton stock with a ceramic cheek riser, and a folded bipod. Original
    design; the grip, trigger and origin stay where the first-person hand fit expects them (TRIGGERS)."""
    p = []
    S = 3
    BZ = 2.4  # bore height
    # ---- Frame and receiver: a gunmetal spine with ceramic shells over it ----
    p.append(k.profile("frame", [(-16, -3.6), (-16, 6.4), (30, 6.4), (32, 4.8), (32, -1.4), (20, -3.6)], 4.6, "WeaponMetal",
                       bevel=0.5, segments=S))
    for x in (2.6, -2.6):
        out = 1 if x > 0 else -1
        p.append(k.profile("receiver_shell", [(-13, -0.4), (-13, 6.0), (-6, 7.2), (24, 7.2), (29, 5.6), (29, 0.4), (20, -1.6), (2, -1.6)],
                           0.7, "Ceramic", x=x, bevel=0.25, segments=S))
        seam(p, x + out * 0.36, (-2.0, -1.4), (-2.0, 7.0))
        seam(p, x + out * 0.36, (14.0, -1.4), (14.0, 7.0))
        seam(p, x + out * 0.36, (-12.6, 3.0), (-2.0, 3.0))
        p.append(k.box("rx_line", (x + out * 0.37, 6.0, 5.6), (0.1, 15.0, 0.22), "GlowCyan", bevel=0))
        for y, z in ((-11.4, 0.6), (-11.4, 5.2), (27.6, 0.8), (27.6, 4.8), (6.0, -0.8)):
            screw(p, x + out * 0.4, y, z, 0.3)
    p.append(k.profile("receiver_top", [(-12, 6.2), (-10, 7.8), (26, 7.8), (28.5, 6.2)], 4.2, "Ceramic", bevel=0.35, segments=S))
    # ---- Barrel shroud: long, faceted, tapering toward the muzzle ----
    p.append(k.profile("shroud", [(30, -2.4), (30, 7.0), (60, 6.4), (82, 5.2), (86, 3.6), (86, 0.8), (82, -1.0), (60, -1.8)], 4.6,
                       "Ceramic", bevel=0.6, segments=S))
    p.append(k.profile("shroud_top", [(31, 6.2), (31, 7.8), (60, 7.2), (80, 6.0), (79, 5.2), (60, 6.4)], 3.0, "WeaponMetal",
                       bevel=0.3, segments=S))
    for x in (2.36, -2.36):
        out = 1 if x > 0 else -1
        p.append(k.profile("shroud_inlay", [(34, 0.0), (34, 4.6), (58, 4.3), (76, 3.6), (76, 1.2), (58, 0.4)], 0.2, "WeaponMetal",
                           x=x, bevel=0.05))
        for i in range(9):
            p.append(k.box("shroud_vent", (x + out * 0.1, 40.0 + i * 3.6, 2.4), (0.1, 2.0, 1.6), "Polymer", bevel=0))
        p.append(k.box("shroud_line", (x + out * 0.12, 57.0, 5.0), (0.08, 38.0, 0.2), "GlowCyan", bevel=0))
        seam(p, x + out * 0.02, (46.0, -1.6), (46.0, 6.8))
        seam(p, x + out * 0.02, (68.0, -1.4), (68.0, 5.9))
        for y, z in ((32.0, 0.0), (32.0, 5.6), (84.0, 1.4)):
            screw(p, x + out * 0.05, y, z, 0.28)
    # Underside: a gunmetal accessory rail with slots, and the forend pad the left hand holds.
    p.append(k.box("under_rail", (0, 52.0, -2.4), (2.6, 40.0, 0.8), "WeaponMetal", bevel=0.2))
    for i in range(16):
        p.append(k.box("under_slot", (0, 34.0 + i * 2.4, -2.85), (1.6, 1.2, 0.1), "Polymer", bevel=0))
    p.append(k.box("forend_pad", (0, 33.0, -2.9), (4.2, 10.0, 0.9), "Polymer", bevel=0.3, segments=2))
    for i in range(10):
        p.append(k.box("forend_rib", (0, 28.6 + i * 0.95, -3.42), (3.8, 0.38, 0.26), "Polymer", bevel=0.08))
    # Folded bipod under the shroud's nose.
    p.append(k.box("bipod_hinge", (0, 77.0, -2.6), (3.0, 2.4, 1.4), "Steel", bevel=0.3))
    for x in (0.9, -0.9):
        p.append(k.box("bipod_leg", (x, 66.0, -3.3), (0.7, 20.0, 0.7), "Steel", bevel=0.15))
        p.append(k.box("bipod_foot", (x, 55.6, -3.5), (1.0, 1.4, 1.0), "Polymer", bevel=0.2))
    # ---- Exposed fluted barrel and a long slotted brake ----
    p.append(k.cyl("barrel", (0, 92.0, BZ), 1.25, 14.0, "Steel", verts=16))
    for i in range(6):
        a = i * math.pi / 3
        p.append(k.box("flute", (1.2 * math.cos(a), 92.0, BZ + 1.2 * math.sin(a)), (0.18, 12.0, 0.18), "Polymer", bevel=0))
    p.append(k.cyl("barrel_nut", (0, 86.6, BZ), 1.8, 1.2, "WeaponMetal", verts=16, bevel=0.15))
    p.append(k.box("brake", (0, 102.0, BZ), (3.0, 6.4, 2.6), "WeaponMetal", bevel=0.4, segments=2, taper=(0.85, 0.85)))
    for i in range(4):
        for x in (1.42, -1.42):
            p.append(k.box("brake_slot", (x, 99.6 + i * 1.5, BZ), (0.12, 0.7, 1.7), "Polymer", bevel=0))
        p.append(k.box("brake_top", (0, 99.6 + i * 1.5, BZ + 1.24), (1.6, 0.6, 0.1), "Polymer", bevel=0))
    p.append(k.cyl("brake_crown", (0, 105.3, BZ), 1.0, 0.25, "GlowCyan", verts=16, bevel=0))
    # ---- Lower: grip, guard, magazine (same fit as the BR) ----
    p.append(k.profile("lower", [(-12.5, -3.8), (-12.5, -0.4), (20, -0.4), (20, -3.6)], 4.4, "Polymer", bevel=0.5, segments=S))
    p.append(k.profile("grip", [(-3.5, -3.5), (3, -3.5), (1.6, -15.2), (0.4, -16.2), (-4.6, -15.6), (-5.0, -14.6)], 3.4,
                       "Polymer", bevel=0.7, segments=S))
    grooves(p, 1.75, -1.2, -6.0, -13.0, 0.3, 3.2, 1.4, "WeaponMetal")
    p.append(k.box("grip_cap", (0, -1.8, -15.9), (3.0, 4.8, 0.6), "Ceramic", bevel=0.2))
    p.append(k.profile("guard", [(2.5, -4), (8.5, -4), (8.5, -9), (7, -9), (7, -5.2), (2.5, -5.2)], 1.0, "WeaponMetal",
                       bevel=0.2, segments=2))
    p.append(k.profile("mag", [(10.5, -3.6), (18.5, -3.6), (18.0, -11.6), (11.0, -11.6)], 3.6, "Polymer", bevel=0.5, segments=S))
    p.append(k.box("mag_base", (0, 14.5, -12.2), (4.0, 8.4, 1.2), "Ceramic", bevel=0.3, segments=2))
    for x in (1.84, -1.84):
        p.append(k.box("mag_window", (x, 14.6, -7.4), (0.1, 4.6, 1.6), "Display", bevel=0))
    p.append(k.box("mag_release", (-2.3, 9.6, -2.6), (0.4, 1.4, 1.0), "Steel", bevel=0.1))
    # ---- Bolt: a short handle on the right with a ribbed knob ----
    p.append(k.box("bolt_slot", (2.34, -6.0, 4.0), (0.1, 10.0, 1.0), "Polymer", bevel=0))
    p.append(k.cyl("bolt", (3.8, -9.0, 4.0), 0.55, 3.2, "Steel", axis="X", verts=10, bevel=0.08))
    p.append(k.cyl("bolt_knob", (5.6, -9.0, 4.0), 1.0, 1.4, "Polymer", axis="X", verts=12, bevel=0.2))
    for i in range(6):
        a = i * math.pi / 3
        p.append(k.box("knob_rib", (5.6, -9.0 + 1.02 * math.cos(a), 4.0 + 1.02 * math.sin(a)), (1.2, 0.14, 0.14), "Steel", bevel=0))
    # ---- Skeleton stock with a ceramic cheek riser and a pad ----
    p.append(k.profile("stock_top", [(-16, 2.0), (-16, 6.6), (-30, 6.4), (-44, 5.4), (-47, 3.6), (-42, 3.0), (-30, 3.6)], 3.8,
                       "Polymer", bevel=0.6, segments=S))
    p.append(k.profile("stock_low", [(-16, -3.6), (-16, -0.4), (-30, -1.0), (-42, -4.4), (-47, -8.6), (-44, -9.6), (-30, -5.0)],
                       3.6, "Polymer", bevel=0.6, segments=S))
    p.append(k.profile("stock_strut", [(-43.5, -8.6), (-47, -8.6), (-47, 4.0), (-43.5, 4.8)], 4.0, "WeaponMetal",
                       bevel=0.4, segments=S))
    p.append(k.profile("stock_brace", [(-24, -2.4), (-22, -1.6), (-33, 4.2), (-35, 3.4)], 3.0, "WeaponMetal", bevel=0.3))
    p.append(k.profile("cheek", [(-18, 6.4), (-18, 8.4), (-34, 8.8), (-40, 7.0), (-40, 5.6)], 3.8, "Ceramic", bevel=0.4, segments=S))
    p.append(k.box("cheek_post", (0, -27.0, 6.9), (1.2, 1.2, 1.6), "Steel", bevel=0.15))
    p.append(k.box("butt_pad", (0, -48.0, -2.2), (4.2, 1.6, 14.4), "Polymer", bevel=0.4, segments=2))
    for i in range(6):
        p.append(k.box("pad_rib", (0, -48.85, 3.6 - i * 2.3), (4.0, 0.2, 0.5), "Polymer", bevel=0.05))
    p.append(k.cyl("qd_cup", (-2.0, -14.4, 2.4), 0.7, 0.4, "Steel", axis="X", verts=12, bevel=0.06))
    # ---- Scope: a boxy, angular long-range optic on two clamp mounts ----
    p.append(k.box("scope_rail", (0, 6.0, 8.3), (2.8, 30.0, 1.0), "WeaponMetal", bevel=0.25))
    rail_teeth(p, -8.0, 20.0, 8.95, 2.9, step=1.2)
    for y in (-2.0, 14.0):
        p.append(k.profile("scope_mount", [(y - 1.6, 8.8), (y + 1.6, 8.8), (y + 1.2, 11.2), (y - 1.2, 11.2)], 2.8, "WeaponMetal",
                           bevel=0.3, segments=2))
        screw(p, -1.45, y, 9.6, 0.26)
    p.append(k.profile("scope_body", [(-10, 11.0), (-10, 15.6), (-7, 17.0), (12, 17.0), (17, 18.2), (24, 18.2), (24, 10.4),
                                       (17, 10.4), (12, 11.0)], 4.4, "Ceramic", bevel=0.45, segments=S))
    p.append(k.profile("scope_frame", [(-9.6, 13.0), (-9.6, 14.6), (23.6, 14.6), (23.6, 13.0)], 4.6, "WeaponMetal", bevel=0.15))
    p.append(k.profile("scope_roof", [(-6.6, 17.0), (-6.0, 17.6), (11.6, 17.6), (12.2, 17.0)], 3.0, "WeaponMetal", bevel=0.15))
    p.append(k.box("objective_bezel", (0, 24.3, 14.3), (5.2, 0.6, 8.4), "WeaponMetal", bevel=0.25, segments=2))
    p.append(k.box("objective", (0, 24.65, 14.3), (4.0, 0.1, 6.6), "Lens", bevel=0))
    p.append(k.box("ocular_bezel", (0, -10.4, 13.3), (4.4, 0.8, 4.8), "Polymer", bevel=0.3, segments=2))
    p.append(k.box("ocular", (0, -10.85, 13.3), (3.2, 0.1, 3.4), "Lens", bevel=0))
    for x in (2.25, -2.25):
        out = 1 if x > 0 else -1
        p.append(k.box("scope_line", (x + out * 0.04, 6.0, 16.0), (0.08, 28.0, 0.2), "GlowCyan", bevel=0))
        for y in (-6.0, 20.0):
            screw(p, x + out * 0.05, y, 12.2, 0.26)
    p.append(k.box("scope_display", (-2.32, 3.0, 12.0), (0.08, 8.0, 1.4), "Display", bevel=0))
    p.append(k.cyl("turret_top", (0, 3.0, 18.0), 1.3, 1.6, "WeaponMetal", axis="Z", verts=16, bevel=0.15))
    p.append(k.cyl("turret_side", (2.9, 3.0, 13.8), 1.3, 1.6, "WeaponMetal", axis="X", verts=16, bevel=0.15))
    for i in range(10):
        a = i * math.pi / 5
        p.append(k.box("turret_knurl", (1.32 * math.cos(a), 3.0 + 1.32 * math.sin(a), 18.0), (0.14, 0.14, 1.3), "Steel", bevel=0))
    p.append(k.cyl("turret_cap", (0, 3.0, 18.85), 0.9, 0.15, "GlowCyan", axis="Z", verts=16, bevel=0))
    p.append(k.box("rangefinder", (-2.6, 19.0, 16.4), (1.2, 5.0, 2.4), "WeaponMetal", bevel=0.3))
    p.append(k.box("rangefinder_lens", (-2.6, 21.55, 16.4), (0.9, 0.1, 1.4), "Lens", bevel=0))
    # ---- Data plate on the left and an ammo readout on the receiver top ----
    p.append(k.box("data_plate", (-3.0, 20.0, 1.4), (0.06, 4.0, 1.1), "WeaponMetal", bevel=0.03))
    for i, w in enumerate((3.0, 1.9, 2.6)):
        p.append(k.box("plate_text", (-3.05, 20.0 - (3.0 - w) * 0.5, 1.75 - i * 0.35), (0.03, w, 0.14), "Polymer", bevel=0))
    p.append(k.box("counter_bezel", (0, -12.0, 8.2), (3.2, 3.2, 0.5), "Polymer", bevel=0.15))
    p.append(k.box("counter", (0, -12.0, 8.5), (2.6, 2.6, 0.2), "Display", bevel=0.05))
    return p, (0, 105.6, BZ)


def rocket_launcher():
    """Hammerhead launcher: one fat octagonal tube, hazard bands, side optic, two grips."""
    p = []
    tube_z = 6.5
    p.append(k.cyl("tube", (0, 6, tube_z), 6.2, 88, "Olive", verts=8, bevel=0.8))
    p.append(k.cyl("muzzle_ring", (0, 49, tube_z), 7.0, 5, "WeaponMetal", verts=8, bevel=0.6))
    p.append(k.cyl("muzzle_hole", (0, 51.6, tube_z), 5.0, 0.4, "Polymer", verts=8, bevel=0))
    p.append(k.cyl("rear_ring", (0, -37, tube_z), 7.0, 5, "WeaponMetal", verts=8, bevel=0.6))
    p.append(k.cyl("rear_hole", (0, -39.6, tube_z), 5.0, 0.4, "Polymer", verts=8, bevel=0))
    for y in (40, 43.5):
        p.append(k.cyl("hazard", (0, y, tube_z), 6.35, 2, "Hazard", verts=8, bevel=0.2))
    # Underslung body: grip, trigger guard, magazine housing for 2 rockets.
    p.append(k.profile("body", [(-14, 1), (12, 1), (12, -1), (-14, -1)], 6.6, "WeaponMetal", bevel=0.5))
    p.append(k.profile("grip", [(-3.5, -1), (3, -1), (1.5, -14), (-4.5, -13.5)], 3.6, "Polymer", bevel=0.6))
    p.append(k.profile("guard", [(3, -1.5), (9, -1.5), (9, -7), (7.5, -7), (7.5, -2.8), (3, -2.8)], 1.0, "WeaponMetal", bevel=0.2))
    p.append(k.profile("foregrip", [(18, 0.5), (24, 0.5), (23, -11), (18.5, -11)], 3.6, "Polymer", bevel=0.6))
    p.append(k.box("breech", (0, -22, tube_z), (14.2, 10, 10), "WeaponMetal", bevel=0.8))
    p.append(k.box("shoulder", (0, -30, 0), (5, 8, 4), "Polymer", bevel=0.5))
    # Optic on the left side of the tube.
    p.append(k.box("optic_arm", (-6.6, 4, tube_z + 4), (2, 4, 2), "WeaponMetal", bevel=0.3))
    p.append(k.box("optic", (-8.6, 4, tube_z + 4.5), (3.6, 12, 4.6), "WeaponMetal", bevel=0.6))
    p.append(k.box("optic_lens", (-8.6, 10.05, tube_z + 4.5), (2.6, 0.2, 3.2), "Lens", bevel=0))
    p.append(k.box("optic_glow", (-8.6, -2.05, tube_z + 4.5), (2.4, 0.2, 2.8), "GlowAmber", bevel=0))
    for x in (7.05, -7.05):
        p.append(k.box("ammo_light", (x, -22, tube_z + 2), (0.15, 3, 1.2), "GlowAmber", bevel=0))
        p.append(k.box("ammo_light2", (x, -22, tube_z - 1), (0.15, 3, 1.2), "GlowAmber", bevel=0))
    # v2 detail: grip ribs around the tube, a carry handle and a rear blast shroud.
    for y in (-6, 14, 30):
        p.append(k.cyl("tube_band", (0, y, tube_z), 6.6, 2.2, "WeaponMetal", verts=8, bevel=0.3))
    p.append(k.box("handle_post", (0, -10, tube_z + 7.6), (2.0, 2.0, 3.0), "WeaponMetal", bevel=0.3))
    p.append(k.box("handle_post", (0, 8, tube_z + 7.6), (2.0, 2.0, 3.0), "WeaponMetal", bevel=0.3))
    p.append(k.box("handle", (0, -1, tube_z + 9.6), (2.4, 21, 1.8), "Polymer", bevel=0.5, segments=2))
    p.append(k.cyl("shroud", (0, -41.5, tube_z), 7.4, 3, "WeaponMetal", verts=8, bevel=0.4, radius2=8.2))
    return p, (0, 52, tube_z)


def rail_lance():
    """Rail Lance: smooth ivory shell, two forward prongs with a cyan rail glowing between them."""
    p = []
    p.append(k.profile("shell", [(-34, -6), (-34, 4), (-28, 8), (12, 9), (20, 7), (24, 3), (24, -3), (10, -4), (-6, -4), (-10, -3), (-26, -3), (-30, -6)],
                       6.4, "Ivory", bevel=1.2, segments=2))
    p.append(k.profile("core", [(-26, -1.5), (-26, 5), (10, 6), (22, 3), (22, -1.5)], 7.2, "WeaponMetal", bevel=0.6))
    p.append(k.profile("grip", [(-3.5, -3.5), (3, -3.5), (1.5, -15), (-4.5, -14.5)], 3.4, "Polymer", bevel=0.6))
    p.append(k.profile("guard", [(3, -4), (9, -4), (9, -8.5), (7.5, -8.5), (7.5, -5.2), (3, -5.2)], 1.0, "WeaponMetal", bevel=0.2))
    # Prongs (upper and lower, mirrored about z = 2) reaching forward; the rail glows between them.
    upper = [(20, 3.2), (20, 7.2), (44, 6.2), (52, 4.8), (52, 3.4)]
    for name, pts in (("prong_up", upper), ("prong_down", [(y, 4.0 - z) for y, z in reversed(upper)])):
        p.append(k.profile(name, pts, 3.0, "Ivory", bevel=0.6, segments=2))
    for z in (4.1, -0.1):
        p.append(k.box("prong_tip", (0, 51.2, z), (3.3, 2.0, 1.2), "GlowCyan", bevel=0.2))
    p.append(k.box("rail_glow", (0, 36, 2), (1.0, 30, 1.0), "GlowCyan", bevel=0.15))
    for y in (-18, -12, -6):
        for x in (3.25, -3.25):
            p.append(k.box("vent", (x, y, 3), (0.2, 3.5, 3.2), "GlowCyan", bevel=0))
    p.append(k.box("sight", (0, 0, 9.6), (1.8, 10, 1.6), "WeaponMetal", bevel=0.4))
    p.append(k.box("sight_glow", (0, -5.05, 9.6), (1.2, 0.2, 1.0), "GlowCyan", bevel=0))
    return p, (0, 50, 2)


# Trigger blade inside each guard (y front of blade, top z, bottom z) and where the index finger's pad rests on it
# (SOCKET_Trigger, Unreal socket "Trigger": the first-person arms curl the trigger finger onto it). Before this the
# guns had no trigger, and the first-person index finger stuck straight out through the receiver (playtest
# 2026-11-05).
TRIGGERS = {
    "SM_SB_BattleRifle": ((5.4, -5.2, -8.2), (0, 3.7, -7.0)),
    "SM_SB_Sidearm": ((3.9, -3.5, -5.6), (0, 2.4, -4.8)),
    "SM_SB_Sniper": ((5.4, -5.2, -8.2), (0, 3.7, -7.0)),
    "SM_SB_RocketLauncher": ((5.4, -2.8, -5.6), (0, 3.7, -4.6)),
    "SM_SB_RailLance": ((5.4, -5.2, -8.0), (0, 3.7, -7.0)),
}


# Where the first-person LEFT hand's wrist (hand_l) sits when it holds the forend from below (SOCKET_SupportHand,
# Unreal socket "SupportHand"). Rifles only: the sidearm keeps its one-handed hold. The game's two-bone IK puts
# hand_l here (Vincent 2026-11-06: on rifles the visible hand is the left one on the front of the gun).
SUPPORT = {
    "SM_SB_BattleRifle": (-2.5, 24.0, -7.5),
    "SM_SB_Sniper": (-2.5, 30.0, -6.6),
    "SM_SB_RocketLauncher": (-2.0, 20.0, -7.0),
    "SM_SB_RailLance": (-3.0, 18.0, -7.5),
}


# First-person reloads (Source/Shieldbreak/Visuals/ShieldbreakViewmodelMoves.cpp) take the magazine out, so each gun
# with one also exports SM_SB_<Gun>_NoMag (the gun without it) and SM_SB_<Gun>_Mag (the magazine, origin at the gun's
# SOCKET_Magazine: the top centre of the magazine where it meets the well). The whole gun stays the main export for
# third person and pickups. MAGAZINES: socket, then the gun-space box (min, max corners) that holds every magazine part
# (a part goes with the magazine only when its bounds are entirely inside), then extra parts that only the magazine
# mesh gets: the feed lips / body that are hidden inside the gun when seated and seen when it is pulled.
MAGAZINES = {
    "SM_SB_BattleRifle": ((0, 13.3, -3.6), ((-3.0, 9.6, -18.0), (3.0, 20.0, -3.5)),
                          lambda: [k.box("mag_feed", (0, 13.3, -2.9), (3.0, 6.0, 1.4), "Polymer", bevel=0.2)]),
    "SM_SB_Sniper": ((0, 14.5, -3.6), ((-3.0, 9.6, -13.5), (3.0, 19.5, -3.5)),
                     lambda: [k.box("mag_feed", (0, 14.5, -2.9), (3.0, 6.4, 1.4), "Polymer", bevel=0.2)]),
    "SM_SB_Sidearm": ((0, -2.2, -2.4), ((-2.2, -7.2, -12.6), (2.2, 0.4, -11.0)),
                      lambda: [k.profile("mag_body", [(-4.6, -2.6), (0.2, -2.6), (-0.9, -11.3), (-5.9, -11.2)], 2.6,
                                         "Polymer", bevel=0.3),
                               k.box("mag_round", (0, -2.2, -2.3), (1.0, 3.4, 0.9), "Steel", bevel=0.3)]),
}


def _inside(obj, box_min, box_max):
    import bpy
    from mathutils import Vector
    bpy.context.view_layer.update()
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    return all(box_min[i] <= min(c[i] for c in corners) and max(c[i] for c in corners) <= box_max[i] for i in range(3))


def export_split(name, build, out_dir, report):
    """Exports SM_SB_<Gun>_NoMag and SM_SB_<Gun>_Mag (see MAGAZINES)."""
    from mathutils import Matrix
    socket, (box_min, box_max), extras = MAGAZINES[name]
    k.reset_scene()
    parts, muzzle = build()
    blade, rest = TRIGGERS[name]
    parts.append(trigger_blade(*blade))
    mag_parts = [p for p in parts if _inside(p, box_min, box_max)]
    gun_parts = [p for p in parts if p not in mag_parts]
    gun = k.join(name + "_NoMag", gun_parts)
    k.socket("Muzzle", muzzle, gun)
    k.socket("Trigger", rest, gun)
    k.socket("Magazine", socket, gun)
    if name in SUPPORT:
        k.socket("SupportHand", SUPPORT[name], gun)
    mag = k.join(name + "_Mag", mag_parts + extras())
    mag.data.transform(Matrix.Translation((-socket[0], -socket[1], -socket[2])))
    report.append("%s_NoMag: %d tris; %s_Mag: %d tris from %d parts, bounds %s" % (
        name, k.triangle_count(gun), name, k.triangle_count(mag), len(mag_parts), k.bounds(mag)))
    k.export_fbx(os.path.join(out_dir, name + "_NoMag.fbx"), [gun])
    k.export_fbx(os.path.join(out_dir, name + "_Mag.fbx"), [mag])


def scope_display(p, scope_center, scope_radius, length):
    """A slim range display clamped to the scope's left side: housing, bezel and a dim screen with two readout bars."""
    x, y, z = scope_center
    side = x - scope_radius - 0.7
    p.append(k.box("display_housing", (side, y, z), (1.4, length, scope_radius * 1.5), "WeaponMetal", bevel=0.3))
    p.append(k.box("display_bezel", (side - 0.75, y, z), (0.2, length - 1.0, scope_radius * 1.2), "Polymer", bevel=0))
    p.append(k.box("display", (side - 0.86, y, z), (0.05, length - 2.0, scope_radius * 0.9), "Display", bevel=0))
    for dz, w in ((0.35, 0.55), (-0.35, 0.3)):
        p.append(k.box("readout", (side - 0.9, y - length * 0.1, z + dz * scope_radius), (0.04, (length - 3.0) * w, 0.25),
                       "GlowCyan", bevel=0))


def trigger_blade(front_y, top, bottom):
    """A curved trigger: a blade raked back toward its tip, 0.8 cm thick."""
    mid = (top + bottom) * 0.5
    return k.profile("trigger", [(front_y - 0.9, top), (front_y, top), (front_y - 0.1, mid), (front_y - 0.6, bottom),
                                 (front_y - 1.2, bottom + 0.3), (front_y - 0.9, mid)], 0.8, "Steel", bevel=0.15)


WEAPONS = {
    "SM_SB_BattleRifle": battle_rifle,
    "SM_SB_Sidearm": sidearm,
    "SM_SB_Sniper": sniper,
    "SM_SB_RocketLauncher": rocket_launcher,
    "SM_SB_RailLance": rail_lance,
}


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    export_root = os.path.join(here, "..", "Export")
    out_dir = os.path.join(export_root, "Weapons")
    os.makedirs(out_dir, exist_ok=True)
    k.write_palette(os.path.join(export_root, "palette.json"))
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    preview_dir = os.path.abspath(args[0]) if args else os.path.join(here, "previews")
    os.makedirs(preview_dir, exist_ok=True)
    report = []
    for name, build in WEAPONS.items():
        k.reset_scene()
        parts, muzzle = build()
        blade, rest = TRIGGERS[name]
        parts.append(trigger_blade(*blade))
        obj = k.join(name, parts)
        k.socket("Muzzle", muzzle, obj)
        k.socket("Trigger", rest, obj)
        if name in SUPPORT:
            k.socket("SupportHand", SUPPORT[name], obj)
        if name in MAGAZINES:
            k.socket("Magazine", MAGAZINES[name][0], obj)
        report.append("%s: %d tris, %d materials, bounds %s" % (name, k.triangle_count(obj), len(obj.data.materials), k.bounds(obj)))
        k.export_fbx(os.path.join(out_dir, name + ".fbx"), [obj])
        if "--render" in sys.argv:
            import sb_render
            sb_render.render_object(obj, os.path.join(preview_dir, name + ".png"))
        if name in MAGAZINES:
            export_split(name, build, out_dir, report)
    print("\n".join(report))
    with open(os.path.join(out_dir, "weapons_report.txt"), "w") as f:
        f.write("\n".join(report) + "\n")


def lineup(path):
    """All weapons in one picture, stacked like a weapon rack."""
    import sb_render
    k.reset_scene()
    objs = []
    z = 0.0
    for name, build in WEAPONS.items():
        parts, _ = build()
        obj = k.join(name, parts)
        lo = min(v.co.z for v in obj.data.vertices)
        hi = max(v.co.z for v in obj.data.vertices)
        obj.location = (0, 0, z - hi)
        z -= (hi - lo) + 8.0
        objs.append(obj)
    sb_render.render_views(objs, path, [((1.0, 0.35, 0.25), False)], resolution=(1400, 1400))


if __name__ == "__main__":
    if "--lineup" in sys.argv:
        lineup(os.path.abspath(sys.argv[-1]))
        sys.exit(0)
    main()
