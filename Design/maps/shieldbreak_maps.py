#!/usr/bin/env python3
"""Shieldbreak map authoring: data, validation, C++ generation and top-down previews.

Every map is greybox geometry (axis-aligned blocks + ramps), gravity lifts, spawns, items and objective spots.
Coordinates are centimetres (Unreal units), Z up, red team plays the -X half, blue the +X half.
Most maps are point-symmetric: anything added with sym=True also gets its (x, y) -> (-x, -y) mirror for blue.

    python3 Design/maps/shieldbreak_maps.py            # validate + write C++ + PNG previews
    python3 Design/maps/shieldbreak_maps.py --check    # validate only

Outputs:
    Source/Shieldbreak/Game/ShieldbreakMapsData.cpp    (generated; do not edit by hand)
    Design/maps/<map>.png                              (top-down previews)

Design rules enforced by validate():
    - ramps no steeper than 30 degrees (the character walks up to ~44, so there is margin);
    - every spawn stands on a surface, with a clear 80 x 184 capsule above it;
    - every item / objective floats 40-120 above a surface;
    - everything sits inside the map walls.
    - cover-pass pieces (Map.cover) keep clear of ramps, lifts, spawns, items and objectives, leave no gap under 2 m,
      and the launch maps keep at most 20% of their floor without nearby cover (validate_fill, openness).
Jump reference: normal jump apex ~258, crouch-jump ~290, full Power Jump ~390 (use these for skill ledges).
"""

import math
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CPP_OUT = os.path.join(ROOT, "Source", "Shieldbreak", "Game", "ShieldbreakMapsData.cpp")
PNG_DIR = os.path.dirname(os.path.abspath(__file__))

# ---- palette (linear-ish RGB for the BasicShapeMaterial "Color" parameter) ----
C = {
    "floor": (0.20, 0.21, 0.23), "wall": (0.16, 0.17, 0.19), "red": (0.55, 0.16, 0.13), "blue": (0.13, 0.24, 0.55),
    "tower": (0.62, 0.62, 0.58), "ramp": (0.70, 0.58, 0.22), "slide": (0.85, 0.40, 0.08), "sky": (0.36, 0.40, 0.30),
    "cover": (0.25, 0.38, 0.28), "metal": (0.45, 0.47, 0.52), "stone": (0.50, 0.46, 0.40), "dark": (0.12, 0.12, 0.14),
    "sand": (0.62, 0.55, 0.40), "grass": (0.22, 0.34, 0.18), "water": (0.10, 0.25, 0.35), "rock": (0.33, 0.30, 0.27),
    "bridge": (0.40, 0.42, 0.46), "glow": (0.20, 0.75, 0.95),
}

WEAPONS = {"sniper": "AWeapon_SniperRifle", "rockets": "AWeapon_RocketLauncher", "rail": "AWeapon_RailLance"}
POWERUPS = {"overshield": "Overshield", "camo": "ActiveCamo", "aegis": "Aegis"}
DEFAULT_RESPAWN = {"sniper": 90, "rockets": 120, "rail": 60, "overshield": 90, "camo": 90, "aegis": 45}
DEFAULT_AMMO = {"sniper": 4, "rockets": 2, "rail": 3}


def mirror(p):
    return (-p[0], -p[1], p[2])


class Map:
    def __init__(self, map_id, name, inspired, half_x, half_y, wall_h=1000.0, floor_z=0.0, max_z=1600.0):
        self.id, self.name, self.inspired = map_id, name, inspired
        self.half_x, self.half_y, self.wall_h, self.floor_z, self.max_z = half_x, half_y, wall_h, floor_z, max_z
        self.blocks, self.ramps, self.lifts = [], [], []
        self.spawns, self.weapons, self.powerups = [], [], []
        self.flags = [None, None]
        self.hills, self.ball = [], None
        self.sun = (-48.0, 35.0)
        self.fog = 0.008
        self.notes = []
        self.fill = []      # indices into self.blocks added by the cover pass (checked by validate_fill)
        self.keepout = []   # (min_xy, max_xy) rectangles the cover pass must leave empty (self-test run-ups, etc.)
        # Floor slab + perimeter walls.
        hx, hy, t = half_x, half_y, 100.0
        self.block((-hx - t, -hy - t, floor_z - 100), (hx + t, hy + t, floor_z), "floor")
        self.block((-hx - t, hy, floor_z), (hx + t, hy + t, floor_z + wall_h), "wall")
        self.block((-hx - t, -hy - t, floor_z), (hx + t, -hy, floor_z + wall_h), "wall")
        self.block((hx, -hy, floor_z), (hx + t, hy, floor_z + wall_h), "wall")
        self.block((-hx - t, -hy, floor_z), (-hx, hy, floor_z + wall_h), "wall")

    # -- authoring --
    def block(self, a, b, color, sym=False):
        mn = tuple(min(a[i], b[i]) for i in range(3))
        mx = tuple(max(a[i], b[i]) for i in range(3))
        self.blocks.append((mn, mx, color))
        if sym:
            ma, mb = mirror(mn), mirror(mx)
            self.block(ma, mb, color)

    def cover(self, a, b, color="cover", sym=False):
        """A cover-pass block: same as block(), but validate_fill() checks its clearances."""
        first = len(self.blocks)
        self.block(a, b, color, sym)
        self.fill.extend(range(first, len(self.blocks)))

    def ramp(self, start, end, width, color="ramp", sym=False, thickness=40.0):
        self.ramps.append((start, end, width, color, thickness))
        if sym:
            self.ramps.append((mirror(start), mirror(end), width, color, thickness))

    def lift(self, base, target, sym=False):
        self.lifts.append((base, target))
        if sym:
            self.lifts.append((mirror(base), mirror(target)))

    def spawn(self, feet, sym=True):
        """Red spawn (and its blue mirror). Yaw faces the map center."""
        self.spawns.append(feet)
        if sym:
            self.spawns.append(mirror(feet))

    def weapon(self, kind, at, sym=False, respawn=None):
        self.weapons.append((kind, at, respawn or DEFAULT_RESPAWN[kind]))
        if sym:
            self.weapons.append((kind, mirror(at), respawn or DEFAULT_RESPAWN[kind]))

    def powerup(self, kind, at, sym=False, respawn=None):
        self.powerups.append((kind, at, respawn or DEFAULT_RESPAWN[kind]))
        if sym:
            self.powerups.append((kind, mirror(at), respawn or DEFAULT_RESPAWN[kind]))

    def flag(self, red_at, blue_at=None):
        self.flags = [red_at, blue_at or mirror(red_at)]

    # -- geometry queries (for validation) --
    def surfaces_at(self, x, y, pad=0.0):
        """All walkable top surfaces (z) over (x, y)."""
        out = []
        for mn, mx, _ in self.blocks:
            if mn[0] - pad <= x <= mx[0] + pad and mn[1] - pad <= y <= mx[1] + pad:
                out.append(mx[2])
        for start, end, width, _, _ in self.ramps:
            dx, dy = end[0] - start[0], end[1] - start[1]
            length2 = dx * dx + dy * dy
            if length2 <= 0:
                continue
            t = ((x - start[0]) * dx + (y - start[1]) * dy) / length2
            if -0.02 <= t <= 1.02:
                lateral = abs((x - start[0]) * dy - (y - start[1]) * dx) / math.sqrt(length2)
                if lateral <= width / 2 + pad:
                    out.append(start[2] + (end[2] - start[2]) * min(max(t, 0.0), 1.0))
        return out

    def solid_overlaps(self, mn, mx):
        """Blocks intersecting the box mn..mx (ramps ignored)."""
        hits = []
        for bmn, bmx, color in self.blocks:
            if all(bmn[i] < mx[i] and bmx[i] > mn[i] for i in range(3)):
                hits.append((bmn, bmx, color))
        return hits


# =====================================================================================================
# Maps
# =====================================================================================================

def crucible():
    """The original prototype arena (kept for the self-test and as a training map)."""
    m = Map("Crucible", "Crucible", "prototype test arena", 3500, 2500, wall_h=900)
    m.block((-3500, -700, 0), (-2500, 700, 300), "red")
    m.block((3500, 700, 0), (2500, -700, 300), "blue")
    m.ramp((-2500, 450, 300), (-1800, 450, 0), 300, sym=True)
    m.block((-2620, -500, 300), (-2580, 100, 410), "red")
    m.block((2620, 500, 300), (2580, -100, 410), "blue")
    m.ramp((-3000, 700, 300), (-3000, 1500, 450), 300, sym=True)
    m.block((-3500, 1500, 410), (-1600, 2500, 450), "sky", sym=True)
    m.ramp((-1600, 1900, 450), (1400, 1900, 0), 400, "slide", sym=True)
    m.ramp((300, 175, 350), (1100, 175, 0), 250, sym=True)
    m.block((1200, -900, 0), (1400, -700, 150), "cover", sym=True)
    m.block((825, 1025, 0), (975, 1175, 150), "cover", sym=True)
    m.block((1940, 940, 0), (2060, 1060, 600), "wall", sym=True)
    m.block((1600, 200, 0), (1700, 300, 400), "wall", sym=True)
    m.block((-300, 1120, 0), (300, 1180, 110), "cover", sym=True)
    m.block((-300, -300, 0), (300, 300, 350), "tower")
    m.block((-300, 0, 350), (0, 300, 500), "tower")
    m.ramp((-150, -280, 350), (-150, 0, 500), 200, thickness=30)
    for s in [(-3000, 0, 300), (-3000, -1900, 0), (-2200, 2000, 450), (-600, 1400, 0), (-2200, -1000, 0), (-1100, -1500, 0)]:
        m.spawn(s)
    m.weapon("rail", (2600, 1900, 70), sym=True)
    m.weapon("rockets", (-150, 150, 570))
    m.weapon("sniper", (-3300, 2300, 520), sym=True)
    m.powerup("overshield", (150, -150, 420))
    m.powerup("aegis", (0, -800, 70))
    m.flag((-3200, 0, 390))
    m.hills = [(0, -800, 220), (-2400, 2000, 670), (0, 800, 220), (2400, -2000, 670)]
    m.ball = (0, 800, 90)
    return m


def keel():
    """Midship principles: two bases face each other across a raised central deck you can also fight under.
    Four ramps climb the deck (two from the ends, two from the side lanes); balconies overlook the lanes."""
    m = Map("Keel", "Keel", "Midship", 3600, 2200, wall_h=1000)
    m.sun = (-52.0, 120.0)
    # Bases: platform (top 280) against each end wall, low lip cover, two ramps down to the floor.
    m.block((-3600, -900, 0), (-2700, 900, 280), "red")
    m.block((3600, 900, 0), (2700, -900, 280), "blue")
    m.block((-2740, -350, 280), (-2700, 350, 390), "red")
    m.block((2740, 350, 280), (2700, -350, 390), "blue")
    m.ramp((-2700, 700, 280), (-2100, 700, 0), 300, sym=True)
    m.ramp((-2700, -700, 280), (-2100, -700, 0), 300, sym=True)
    # Base balcony over the +Y lane (red) / -Y lane (blue): joined to the platform, low wall to fight from.
    m.block((-3600, 900, 0), (-2400, 2200, 280), "red")
    m.block((3600, -900, 0), (2400, -2200, 280), "blue")
    m.block((-2440, 1150, 280), (-2400, 1950, 360), "red")
    m.block((2440, -1150, 280), (2400, -1950, 360), "blue")
    # Central deck: floating slab (top 400) on four pillars; open underneath (360 clearance).
    m.block((-800, -600, 360), (800, 600, 400), "metal")
    for px, py in [(-750, -550), (750, -550), (-750, 550), (750, 550)]:
        m.block((px - 50, py - 50, 0), (px + 50, py + 50, 360), "dark")
    # Deck ramps: from each end lane up the long axis, and from each side lane.
    m.ramp((-1700, 0, 0), (-800, 0, 400), 260, sym=True)
    m.ramp((-300, 1500, 0), (-300, 600, 400), 260, sym=True)
    # Deck rail cover.
    m.block((-500, -620, 400), (-200, -600, 470), "metal", sym=True)
    # Side lanes: crates and a long low wall, plus tall pillars that break the lane sightline.
    m.block((-1500, 1300, 0), (-1300, 1500, 150), "cover", sym=True)
    m.block((-1000, -1700, 0), (-800, -1500, 150), "cover", sym=True)
    m.block((-1900, -1250, 0), (-1300, -1200, 110), "cover", sym=True)
    m.block((-600, 1850, 0), (-450, 2000, 500), "wall", sym=True)
    m.block((-2100, -300, 0), (-1950, -150, 500), "wall", sym=True)
    # Items: rockets on the deck, overshield under it, a Rail Lance in each side lane, Aegis by each balcony stairs.
    m.weapon("rockets", (0, 0, 470))
    m.powerup("overshield", (0, 0, 70))
    m.weapon("rail", (-1200, -1700, 70), sym=True)
    m.powerup("aegis", (-1700, 1650, 70), sym=True)
    for s in [(-3200, 500, 280), (-3200, -500, 280), (-3000, 1600, 280), (-2300, -1500, 0), (-1900, 1200, 0), (-2000, 300, 0)]:
        m.spawn(s)
    m.flag((-3300, 0, 370))
    m.hills = [(0, 0, 620), (-2900, 1700, 500), (0, 0, 220), (2900, -1700, 500)]
    m.ball = (0, 900, 90)
    return m


def bastion():
    """Lockout principles, asymmetric: a sunken pit ringed by plateaus, a tall sniper tower and a shorter battle
    tower in opposite corners, a bridge across the pit and an enclosed library. Teams start in opposite corners."""
    m = Map("Bastion", "Bastion", "Lockout", 3000, 3000, wall_h=1300, max_z=1800)
    m.sun = (-40.0, 210.0)
    m.fog = 0.012
    # Plateau ring (top 300) around a 2400 x 2400 pit.
    m.block((-3000, 1200, 0), (3000, 3000, 300), "stone")
    m.block((-3000, -3000, 0), (3000, -1200, 300), "stone")
    m.block((-3000, -1200, 0), (-1200, 1200, 300), "stone")
    m.block((1200, -1200, 0), (3000, 1200, 300), "stone")
    # Pit access: two ramps (NW, SE) plus a jump step on the east side.
    m.ramp((-600, 500, 0), (-600, 1200, 300), 300)
    m.ramp((600, -500, 0), (600, -1200, 300), 300)
    m.block((850, 800, 0), (1200, 1100, 120), "rock")
    m.block((1050, 400, 0), (1200, 700, 200), "rock")
    # Bridge across the pit (top 300), open underneath, with two waist-high cover posts.
    m.block((-1200, -150, 260), (1200, 150, 300), "bridge")
    m.block((-450, -150, 300), (-400, 0, 390), "bridge")
    m.block((400, 0, 300), (450, 150, 390), "bridge")
    # Pit cover.
    m.block((-200, 500, 0), (200, 900, 260), "rock")
    m.block((-900, -900, 0), (-650, -650, 150), "cover")
    m.block((300, -900, 0), (500, -700, 150), "cover")
    # Sniper tower, NE (top 900): ramp up its west face to 700, then a jump onto the roof.
    m.block((1800, 1800, 300), (2600, 2600, 900), "tower")
    m.ramp((1000, 2400, 300), (1800, 2400, 700), 300)
    m.block((1720, 2250, 700), (1800, 2550, 720), "ramp")
    # Battle tower, SW (top 700): ramp up its east face.
    m.block((-2600, -2600, 300), (-1800, -1800, 700), "tower")
    m.ramp((-1000, -2400, 300), (-1800, -2400, 640), 300)
    # Library, NW: walled room on the plateau with three doorways and a roof you can crouch-jump onto from a crate.
    for a, b in [((-2800, 2700, 300), (-1600, 2760, 600)), ((-2800, 1540, 300), (-2300, 1600, 600)),
                 ((-2100, 1540, 300), (-1600, 1600, 600)), ((-2860, 1540, 300), (-2800, 2760, 600)),
                 ((-1600, 1540, 300), (-1540, 1950, 600)), ((-1600, 2250, 300), (-1540, 2760, 600))]:
        m.block(a, b, "stone")
    m.block((-2860, 1540, 600), (-1540, 2760, 640), "dark")
    m.block((-1400, 1500, 300), (-1250, 1650, 480), "cover")
    # East plateau battlements.
    m.block((2200, -600, 300), (2260, 600, 420), "stone")
    m.block((-2260, -600, 300), (-2200, 300, 420), "stone")
    # Items.
    m.weapon("sniper", (2200, 2200, 970))
    m.weapon("rockets", (0, -700, 70))
    m.weapon("rail", (-2200, -2200, 770))
    m.powerup("camo", (-2200, 2200, 370))
    m.powerup("aegis", (2600, -400, 370))
    # Spawns: red in the SW quarter, blue in the NE quarter (not mirrored; Bastion is asymmetric).
    for s in [(-2600, -1000, 300), (-1500, -2600, 300), (-2600, 600, 300), (-600, -2000, 300), (-2000, -1500, 300)]:
        m.spawn(s, sym=False)
    for s in [(2600, 1000, 300), (1500, 2600, 300), (2600, -600, 300), (600, 2000, 300), (2000, 1500, 300)]:
        m.spawn(s, sym=False)
    m.flag((-2600, -500, 390), (2600, 500, 390))
    m.hills = [(0, -500, 220), (-2200, 2150, 520), (0, 0, 520), (2200, 2200, 1120), (-2200, -2200, 920)]
    m.ball = (0, 0, 390)
    return m


def sanctum():
    """Sanctuary / The Pit principles, rebuilt at full scale (2026-10-29) with three height layers instead of an open bowl.

    Authored directly at its final 152 x 104 m size (no expand_horizontal). Layers: the sunken courtyard (0), the sand
    terraces and temple halls (200), the walkway ring and temple upper floors (550) and the sniper perches (900).
    Each end has a walk-through temple: a colonnaded hall open to the sky in the middle, an upper floor round the
    atrium reached by a central ramp and an outside balcony ramp, and a stepped perch at the back. A ring of
    elevated walkways runs along both flanks and crosses the courtyard twice; a shrine sits under each flank with a
    bridge from its roof to the walkway. The rockets plinth keeps its skill jump (self-test)."""
    m = Map("Sanctum", "Sanctum", "Sanctuary / The Pit", 7600, 5200, wall_h=1200, max_z=1800)
    m.sun = (-35.0, 70.0)
    m.fog = 0.010
    L1, L2, L3 = 200, 550, 900
    slab = 40
    # ---- Terrain: sand terraces at L1 everywhere except the sunken courtyard (5600 x 4000) ----
    m.block((-7600, -5200, 0), (-2800, 5200, L1), "sand", sym=True)
    m.block((-2800, 2000, 0), (2800, 5200, L1), "sand", sym=True)
    # Courtyard steps: a wide ramp from each end terrace and each flank.
    m.ramp((-2800, 0, L1), (-2000, 0, 0), 1000, "stone", sym=True)
    m.ramp((600, 2000, L1), (600, 1200, 0), 900, "stone", sym=True)
    # Rockets plinth (top 320, 600 square): a plain jump can't make it, a crouch-jump or Power Jump can.
    m.block((-300, -300, 0), (300, 300, 320), "stone")

    # ---- Temples (red at -X, mirrored for blue) ----
    tx0, tx1 = -7600, -5400          # hall footprint along X
    for side in (1, -1):              # north and south walls, with a side door
        y0, y1 = sorted((side * 1700, side * 1800))
        m.block((tx0, y0, L1), (-6700, y1, L2 - slab), "red", sym=True)
        m.block((-6300, y0, L1), (tx1, y1, L2 - slab), "red", sym=True)
    # Front colonnade: corner piers and a centre pier, two wide doorways between them.
    for y0, y1 in ((1200, 1800), (-1800, -1200)):
        m.block((-5600, y0, L1), (tx1, y1, L2 - slab), "red", sym=True)
    m.block((-5600, -400, L1), (tx1, 400, L2 - slab), "red", sym=True)
    # Upper floor round an open atrium: the rear gallery and two side wings.
    m.block((tx0, -1800, L2 - slab), (-6600, 1800, L2), "stone", sym=True)
    m.block((-6600, 1000, L2 - slab), (tx1, 1800, L2), "stone", sym=True)
    m.block((-6600, -1800, L2 - slab), (tx1, -1000, L2), "stone", sym=True)
    # Outside balcony off the north wing, reached by a ramp from the sand.
    m.block((-6000, 1800, L2 - slab), (tx1, 2400, L2), "stone", sym=True)
    m.ramp((-4300, 2150, L1), (tx1, 2150, L2), 400, sym=True)
    # Central ramp up through the atrium to the rear gallery.
    m.ramp((-5650, 0, L1), (-6600, 0, L2), 360, sym=True)
    # Parapets along the wings' inner and front edges (waist high).
    m.block((-6600, 1000, L2), (-5600, 1050, L2 + 110), "stone", sym=True)
    m.block((-6600, -1050, L2), (-5600, -1000, L2 + 110), "stone", sym=True)
    m.block((-5440, -1800, L2), (tx1, -1000, L2 + 110), "stone", sym=True)
    # Corner spires on the temple: tall, unwalkable mass that marks each base from across the map.
    for y0, y1 in ((1200, 1800), (-1800, -1200)):
        m.block((-5600, y0, L2), (tx1, y1, 1500), "red", sym=True)
    # Sniper perch at the back of the gallery: two 175 steps.
    m.block((-7500, -500, L2), (-7000, 500, L3), "tower", sym=True)
    m.block((-7000, -500, L2), (-6800, 500, L2 + 175), "tower", sym=True)

    # ---- Walkway ring at L2 ----
    # Flank walkway (red builds the north one; the mirror is blue's south one), with a waist-high rail.
    m.block((-4800, 3800, L2 - slab), (2000, 4400, L2), "bridge", sym=True)
    m.block((-4800, 4360, L2), (2000, 4400, L2 + 110), "bridge", sym=True)
    # Courtyard crossing: flank to flank over the courtyard, on two pairs of piers.
    m.block((-1900, -3800, L2 - slab), (-1300, 3800, L2), "bridge", sym=True)
    for py in (-1000, 1000):
        m.block((-1700, py - 100, 0), (-1500, py + 100, L2 - slab), "dark", sym=True)
    # Ramps between the terraces and the walkway: off the temple end and down the courtyard side.
    m.ramp((-4800, 4100, L2), (-6000, 4100, L1), 500, sym=True)
    m.ramp((-3300, 3800, L2), (-3300, 2700, L1), 400, sym=True)

    # ---- Flank shrines (north and its mirror): a roofed room on the sand with a bridge to the walkway ----
    m.block((-800, 2600, L1), (-200, 2700, L2 - slab), "stone", sym=True)
    m.block((200, 2600, L1), (800, 2700, L2 - slab), "stone", sym=True)
    m.block((-800, 3300, L1), (800, 3400, L2 - slab), "stone", sym=True)
    m.block((-800, 2700, L1), (-700, 3300, L2 - slab), "stone", sym=True)
    m.block((700, 2700, L1), (800, 3100, L2 - slab), "stone", sym=True)
    m.block((-800, 2600, L2 - slab), (800, 3400, L2), "stone", sym=True)
    m.block((-300, 3400, L2 - slab), (300, 3800, L2), "bridge", sym=True)

    # ---- Corner annex (red's south-west; the mirror is blue's north-east): a ruined two-storey hall whose roof
    # deck bridges onto the far flank walkway, closing the L2 ring at both ends ----
    m.block((-5400, -4800, L1), (-5300, -3500, L2 - slab), "stone", sym=True)
    m.block((-5400, -3600, L1), (-4700, -3500, L2 - slab), "stone", sym=True)
    m.block((-4300, -3600, L1), (-3600, -3500, L2 - slab), "stone", sym=True)
    m.block((-3700, -4800, L1), (-3600, -4500, L2 - slab), "stone", sym=True)
    m.block((-5400, -4800, L2 - slab), (-3600, -3500, L2), "stone", sym=True)
    m.block((-5400, -3540, L2), (-4400, -3500, L2 + 110), "stone", sym=True)
    m.block((-3600, -4300, L2 - slab), (-2000, -3900, L2), "bridge", sym=True)
    m.ramp((-6500, -4000, L1), (-5400, -4000, L2), 400, sym=True)

    # ---- Courtyard colossi: four tall pillars round the plinth (above head height; they break the long
    # diagonal lines across the courtyard without walling it in) ----
    for px_, py_ in ((-900, 1400), (900, 1400)):
        m.block((px_ - 200, py_ - 200, 0), (px_ + 200, py_ + 200, 1300), "tower", sym=True)

    # ---- Cover: mid-field ruins between each temple and the courtyard, courtyard rocks, flank clutter ----
    m.cover((-4700, -1500, L1), (-4300, -900, 1100), "stone", sym=True)       # obelisk pair: blocks the
    m.cover((-4700, 900, L1), (-4300, 1500, 800), "stone", sym=True)          # temple-to-temple line
    m.cover((-3900, -300, L1), (-3500, 300, 330), "rock", sym=True)
    m.cover((-5000, -2900, L1), (-4300, -2780, 520), "stone", sym=True)
    m.cover((-3800, -2500, L1), (-3500, -2200, 330), "rock", sym=True)
    m.cover((-6400, -3300, L1), (-6100, -3000, 330), "rock", sym=True)
    m.cover((-2500, -4950, L1), (-2200, -4650, 330), "rock", sym=True)
    m.cover((-6800, 3200, L1), (-6500, 3500, 330), "rock", sym=True)
    m.cover((-4500, 3000, L1), (-4200, 3300, 330), "rock", sym=True)
    m.cover((-2300, 3000, L1), (-2000, 3300, 330), "rock", sym=True)
    m.cover((2200, 2900, L1), (2500, 3200, 330), "rock", sym=True)
    m.cover((-1000, -4800, L1), (-700, -4500, 330), "rock", sym=True)
    m.cover((2200, -4700, L1), (2500, -4400, 330), "rock", sym=True)
    # Courtyard floor (keeping the plinth's run-up along -X clear).
    m.cover((-2600, -1700, 0), (-2300, -1400, 130), "rock", sym=True)
    m.cover((1400, -1800, 0), (1700, -1500, 130), "rock", sym=True)
    m.cover((1000, -600, 0), (1300, -300, 130), "rock", sym=True)
    m.cover((-2500, 1000, 0), (-2200, 1300, 130), "rock", sym=True)
    # Temple hall floor and the gallery.
    m.cover((-6300, -800, L1), (-6000, -500, 330), "stone", sym=True)
    m.cover((-7300, -1400, L2), (-7000, -1100, L2 + 130), "stone", sym=True)
    m.keepout.append(((-900, -500), (900, 500)))  # self-test run-up and landing around the rockets plinth

    # ---- Items and objectives ----
    m.weapon("rockets", (0, 0, 390))
    m.weapon("sniper", (-7250, 0, L3 + 70), sym=True)
    m.weapon("rail", (-3800, 4100, L2 + 70), sym=True)
    m.powerup("overshield", (0, -3000, L1 + 70))
    m.powerup("aegis", (-1600, 0, L2 + 70), sym=True)
    for s in [(-6900, 1300, L1), (-6900, -1300, L1), (-7200, 1300, L2), (-5000, 3200, L1), (-5000, -3300, L1), (-4300, 0, L1)]:
        m.spawn(s)
    m.flag((-6100, 1350, L1 + 90))
    m.hills = [(0, 1200, 220), (-1600, 2600, L2 + 220), (0, -1200, 220), (1600, -2600, L2 + 220), (0, 3000, L1 + 220)]
    m.ball = (0, -1300, 90)
    return m


def creekbed():
    """Beaver Creek principles: small and fast. Two huts with roof access face each other across a shallow creek;
    covered caves run along both sides; a rock in mid-creek holds the rockets."""
    m = Map("Creekbed", "Creekbed", "Beaver Creek", 2800, 1800, wall_h=900, floor_z=-140)
    m.sun = (-55.0, 300.0)
    # Banks (top 0) and the creek: two gentle ramps meeting at -120 in the middle.
    m.block((-2800, -1800, -140), (-800, 1800, 0), "grass", sym=True)
    m.ramp((-800, 0, 0), (0, 0, -120), 3600, "sand", sym=True)
    # Huts: walls with a front door and side doors, roof (top 320) reached by an outside ramp.
    for a, b in [((-2700, -560, 0), (-2640, 560, 300)), ((-2060, -560, 0), (-2000, -160, 300)), ((-2060, 160, 0), (-2000, 560, 300)),
                 ((-2700, -560, 0), (-2450, -500, 300)), ((-2250, -560, 0), (-2000, -500, 300)),
                 ((-2700, 500, 0), (-2450, 560, 300)), ((-2250, 500, 0), (-2000, 560, 300))]:
        m.block(a, b, "red")
        m.block(mirror(a), mirror(b), "blue")
    m.block((-2700, -560, 300), (-2000, 900, 320), "red")
    m.block((2700, 560, 300), (2000, -900, 320), "blue")
    m.ramp((-1550, 750, 0), (-2150, 750, 320), 220, sym=True)
    # Caves: roofs over each side edge (open underneath, 240 clearance at the banks).
    m.block((-1300, 1350, 240), (1300, 1800, 290), "rock", sym=True)
    m.block((-1300, 1300, -140), (-1100, 1350, 240), "rock", sym=True)
    m.block((300, 1300, -140), (500, 1350, 240), "rock", sym=True)
    # Mid-creek rock (top 80): one jump up from the creek bed.
    m.block((-300, -300, -140), (300, 300, 80), "rock")
    # Bank cover.
    m.block((-1500, -400, 0), (-1350, -200, 140), "rock", sym=True)
    m.block((-1700, -1300, 0), (-1500, -1100, 140), "rock", sym=True)
    m.block((-900, 900, -20), (-700, 1050, 120), "rock", sym=True)
    # Items.
    m.weapon("rockets", (0, 0, 150))
    m.powerup("aegis", (0, 1550, -50))
    m.weapon("rail", (0, -1550, -50))
    for s in [(-2350, 200, 0), (-2350, -200, 0), (-2400, 0, 320), (-1600, 1100, 0), (-1600, -1000, 0)]:
        m.spawn(s)
    m.flag((-2450, 0, 90))
    m.hills = [(0, 0, 300), (-2350, 150, 540), (0, 1550, 100), (2350, -150, 540), (0, -1550, 100)]
    m.ball = (0, 900, -50)
    return m


def spire():
    """Narrows / Construct principles: two high bases joined by a long, narrow bridge over a lower level. Gravity
    lifts and ramps climb from the lower level; the rockets sit under the bridge, the camo out on it."""
    m = Map("Spire", "Spire", "Narrows / Construct", 4200, 1600, wall_h=1300, max_z=1800)
    m.sun = (-30.0, 160.0)
    m.fog = 0.006
    # Bases (top 600) spanning each end.
    m.block((-4200, -1600, 0), (-2800, 1600, 600), "red")
    m.block((4200, 1600, 0), (2800, -1600, 600), "blue")
    # Bridge (top 600, 400 wide) with staggered cover.
    m.block((-2800, -200, 560), (2800, 200, 600), "bridge")
    m.block((-1600, -200, 600), (-1400, -120, 690), "metal", sym=True)
    m.block((-500, 120, 600), (-300, 200, 690), "metal", sym=True)
    # Mid platforms (top 300) along each side, ramps up from the lower level and on to the base.
    m.block((-2200, 900, 260), (-1200, 1400, 300), "metal", sym=True)
    m.ramp((-650, 1150, 0), (-1200, 1150, 300), 250, sym=True)
    m.ramp((-2200, 1150, 300), (-2800, 1150, 600), 250, sym=True)
    # Lower-level cover and the support piers under the bridge.
    for x in (-2000, -1000, 1000, 2000):
        m.block((x - 60, -60, 0), (x + 60, 60, 560), "dark")
    m.block((-1600, -1200, 0), (-1350, -950, 160), "cover", sym=True)
    m.block((-400, -1100, 0), (-150, -1050, 120), "cover", sym=True)
    # Gravity lifts: base front to the base top, and lower level to the bridge.
    m.lift((-2600, -900, 0), (-3100, -900, 600), sym=True)
    m.lift((-600, -700, 0), (-600, -100, 600), sym=True)
    # Sniper perch at the back of each base (top 900, two 150 steps).
    m.block((-4100, -300, 600), (-3800, 300, 900), "tower", sym=True)
    m.block((-3800, -300, 600), (-3650, 300, 750), "tower", sym=True)
    # Items.
    m.weapon("rockets", (0, 0, 70))
    m.weapon("sniper", (-3950, 0, 970), sym=True)
    m.weapon("rail", (-1700, 1150, 370), sym=True)
    m.powerup("camo", (0, 0, 670))
    for s in [(-3400, 800, 600), (-3400, -800, 600), (-3100, 0, 600), (-1700, 1150, 300), (-2400, -900, 0)]:
        m.spawn(s)
    m.flag((-3600, 0, 690))
    m.hills = [(0, 0, 820), (0, 900, 220), (-3300, 0, 820), (0, -900, 220), (3300, 0, 820)]
    m.ball = (0, 600, 90)
    return m


def expand_horizontal(m, factor=2.0):
    """First size pass: double X/Y, retain authored Z and human movement scale.

    Transform every gameplay anchor together with its supporting surfaces.
    Widths expand, but ramp thickness, cover heights and lift pads do not.
    Crucible is deliberately excluded so mechanic regression distances stay fixed.
    """
    if factor <= 0:
        raise ValueError('Map scale must be positive')
    def p(v):
        return (v[0] * factor, v[1] * factor, v[2])
    m.half_x *= factor
    m.half_y *= factor
    m.blocks = [(p(a),p(b),style) for a,b,style in m.blocks]
    m.ramps = [(p(a),p(b),width*factor,style,thickness)
               for a,b,width,style,thickness in m.ramps]
    m.lifts = [(p(a),p(b)) for a,b in m.lifts]
    m.spawns = [p(v) for v in m.spawns]
    m.weapons = [(kind,p(at),respawn) for kind,at,respawn in m.weapons]
    m.powerups = [(kind,p(at),respawn) for kind,at,respawn in m.powerups]
    m.flags = [p(v) if v is not None else None for v in m.flags]
    m.hills = [p(v) for v in m.hills]
    m.ball = p(m.ball) if m.ball is not None else None
    m.notes.append('2x length and width / 4x original ground area; original heights retained')
    return m


# =====================================================================================================
# Cover pass (2026-10-29): the 2x expansion kept the original handful of cover pieces, so the larger floors were
# mostly open ground (see openness() below). These add human-scale cover, sightline breakers and small
# high-ground pieces in the expanded coordinates. Heights: low cover 110-140 (shoot over it, crouch behind it),
# tall walls 450-500 (break a lane's sightline). validate_fill() keeps them off ramps, lift paths, spawns,
# items and objectives, and forbids gaps a player could snag in (anything between 0 and 2 m).
# =====================================================================================================

def fill_keel(m):
    # Side lanes: a tall wall halfway down each lane so the lane is two fights, not one 100 m sightline,
    # plus low cover to strafe between.
    m.cover((-2100, 3400, 0), (-1900, 4400, 500), "wall", sym=True)
    m.cover((-4300, -4400, 0), (-4100, -3400, 500), "wall", sym=True)
    m.cover((-4400, 2100, 0), (-4000, 2400, 130), sym=True)
    m.cover((-1700, 2300, 0), (-1300, 2600, 130), sym=True)
    m.cover((-3200, -3900, 0), (-2900, -3600, 130), sym=True)
    m.cover((-1200, -2700, 0), (-900, -2400, 500), "wall", sym=True)
    # Base approach: crates flanking the long deck ramp so pushing out of base has somewhere to go.
    m.cover((-3000, 700, 0), (-2600, 1000, 130), sym=True)
    m.cover((-3000, -1100, 0), (-2600, -800, 130), sym=True)
    m.cover((-5000, -300, 0), (-4800, 300, 140), sym=True)
    m.cover((-3700, 1300, 0), (-3500, 1800, 450), "wall", sym=True)
    m.cover((-2300, -1000, 0), (-2100, -500, 450), "wall", sym=True)
    # Balcony and base tops (280): low cover so defenders aren't standing on an open slab.
    m.cover((-6800, 2400, 280), (-6500, 2700, 410), "metal", sym=True)
    m.cover((-5600, 3800, 280), (-5300, 4100, 410), "metal", sym=True)
    m.cover((-7000, -1400, 280), (-6800, -1100, 410), "metal", sym=True)
    m.cover((-7000, 1100, 280), (-6800, 1400, 410), "metal", sym=True)
    # Back corner of the -Y lane.
    m.cover((-6600, -3400, 0), (-6300, -3100, 130), sym=True)
    m.cover((-5600, -4000, 0), (-5300, -3700, 130), sym=True)
    # Under the deck: low walls to fight around the Overshield (the deck is 360 up, so plenty of headroom).
    m.cover((-800, -300, 0), (-600, 300, 120), "dark", sym=True)
    m.cover((-200, 600, 0), (200, 800, 120), "dark", sym=True)
    # Deck top: two low barriers so the rockets fight isn't a flat open slab.
    m.cover((-1100, 300, 400), (-1000, 900, 510), "metal", sym=True)
    m.cover((300, 300, 400), (700, 400, 510), "metal", sym=True)


def fill_bastion(m):
    # Asymmetric, so both halves are placed by hand. Plateau top is 300.
    # South plateau (red's home) and north plateau (blue's home).
    m.cover((-100, -4700, 300), (100, -3500, 800), "stone")
    m.cover((-100, 3500, 300), (100, 4700, 800), "stone")
    m.cover((2000, -3300, 300), (2400, -3000, 430), "rock")
    m.cover((-2100, 2900, 300), (-1800, 3200, 430), "rock")
    m.cover((3800, -4900, 300), (4200, -4500, 430), "rock")
    m.cover((-1400, 4300, 300), (-1000, 4700, 430), "rock")
    m.cover((-1500, -3400, 300), (-1100, -3100, 430), "rock")
    m.cover((1100, 3100, 300), (1500, 3400, 430), "rock")
    m.cover((1800, -5400, 300), (2000, -4600, 750), "stone")
    m.cover((-2000, 5000, 300), (-1800, 5800, 750), "stone")
    # West and east bands beside the flags.
    m.cover((-3300, -1700, 300), (-3000, -1400, 430), "rock")
    m.cover((-3300, 1300, 300), (-3000, 1600, 430), "rock")
    m.cover((3000, 1400, 300), (3300, 1700, 430), "rock")
    m.cover((3000, -1600, 300), (3300, -1300, 430), "rock")
    m.cover((-5300, 2000, 300), (-5100, 2500, 750), "stone")
    m.cover((5100, -2900, 300), (5300, -2400, 750), "stone")
    # Sniper-tower and battle-tower approaches, and the plateau rim around the pit.
    m.cover((2400, 2500, 300), (2700, 2800, 430), "rock")
    m.cover((3000, 3500, 300), (3200, 4100, 750), "stone")
    m.cover((4600, 2600, 300), (4900, 2900, 430), "rock")
    m.cover((-2600, -4100, 300), (-2300, -3800, 430), "rock")
    m.cover((-800, -5600, 300), (-500, -5300, 430), "rock")
    m.cover((-900, -2900, 300), (-600, -2600, 430), "rock")
    m.cover((600, 2600, 300), (900, 2900, 430), "rock")
    m.cover((3000, -4200, 300), (3300, -3900, 430), "rock")
    m.cover((-4000, -200, 300), (-3700, 100, 430), "rock")
    m.cover((3700, -100, 300), (4000, 200, 430), "rock")
    # Bridge: one more waist-high post near each end.
    m.cover((-1700, -300, 300), (-1600, -50, 410), "bridge")
    m.cover((1600, 50, 300), (1700, 300, 410), "bridge")
    # Pit: two more boulders so crossing under the bridge has cover.
    m.cover((1500, 300, 0), (1800, 600, 140), "rock")
    m.cover((-2100, 600, 0), (-1800, 900, 140), "rock")
    m.cover((-500, -2200, 0), (-200, -1900, 140), "rock")


def fill_creekbed(m):
    # Banks: more rocks between the huts and the creek so the approaches are short hops, not open lawns.
    m.cover((-2400, -700, 0), (-2100, -400, 140), "rock", sym=True)
    m.cover((-3700, 300, 0), (-3400, 600, 140), "rock", sym=True)
    m.cover((-2600, 1700, 0), (-2300, 2000, 140), "rock", sym=True)
    m.cover((-3500, -1500, 0), (-3300, -900, 400), "rock", sym=True)
    m.cover((-4600, 2400, 0), (-4300, 2800, 140), "rock", sym=True)
    m.cover((-4700, -2900, 0), (-4400, -2500, 140), "rock", sym=True)
    m.cover((-2300, -2400, 0), (-2000, -2100, 140), "rock", sym=True)
    # Creek: boulders either side of the mid-creek rock (the creek bed is about -60 to -120 here).
    m.cover((-1200, 1200, -140), (-900, 1500, 60), "rock", sym=True)
    m.cover((-1300, -1500, -140), (-1000, -1200, 60), "rock", sym=True)
    m.cover((-500, 2000, -140), (-300, 2200, 60), "rock", sym=True)


def fill_spire(m):
    # Lower level: tall fins break the full-length lanes on both sides; low cover between them.
    m.cover((-3700, -3200, 0), (-3500, -2500, 500), "metal", sym=True)
    m.cover((-1800, 2700, 0), (-1600, 3200, 500), "metal", sym=True)
    m.cover((-600, -3200, 0), (-400, -2600, 500), "metal", sym=True)
    m.cover((-900, 2600, 0), (-600, 2900, 130), sym=True)
    m.cover((-2800, -900, 0), (-2500, -600, 130), sym=True)
    m.cover((-4700, 600, 0), (-4400, 900, 130), sym=True)
    m.cover((-1900, -2400, 0), (-1600, -2100, 130), sym=True)
    m.cover((-300, 1600, 0), (200, 1700, 120), sym=True)
    m.cover((-3400, 1100, 0), (-3000, 1400, 130), sym=True)
    m.cover((-1700, 1100, 0), (-1400, 1400, 130), sym=True)
    m.cover((-700, 600, 0), (-400, 900, 130), sym=True)
    # Mid platform (top 300): a low wall at its back edge.
    m.cover((-3600, 2600, 300), (-3200, 2800, 410), "metal", sym=True)
    # Base tops (600): the bases were 28 x 64 m of open floor. Low cover around the flag and spawns,
    # and a front lip either side of the bridge.
    m.cover((-7700, -2400, 600), (-7300, -2000, 730), "metal", sym=True)
    m.cover((-7700, 2000, 600), (-7300, 2400, 730), "metal", sym=True)
    m.cover((-6400, -1100, 600), (-6100, -800, 730), "metal", sym=True)
    m.cover((-6400, 800, 600), (-6100, 1100, 730), "metal", sym=True)
    m.cover((-5800, -1200, 600), (-5700, -700, 710), "metal", sym=True)
    m.cover((-5800, 700, 600), (-5700, 1200, 710), "metal", sym=True)
    m.cover((-7000, -3000, 600), (-6800, -2600, 1100), "metal", sym=True)
    m.cover((-7000, 2600, 600), (-6800, 3000, 1100), "metal", sym=True)
    # Bridge: two more staggered barriers, still leaving it the exposed route.
    m.cover((-4500, 150, 600), (-4300, 400, 710), "metal", sym=True)
    m.cover((-2300, -400, 600), (-2100, -150, 710), "metal", sym=True)


FILLS = {"Keel": fill_keel, "Bastion": fill_bastion, "Creekbed": fill_creekbed, "Spire": fill_spire}


def expand_and_fill(m):
    expand_horizontal(m)
    FILLS[m.id](m)
    m.notes.append(f"cover pass: {len(m.fill)} pieces")
    return m


MAPS = [expand_and_fill(keel()), expand_and_fill(bastion()), sanctum(), expand_and_fill(creekbed()), expand_and_fill(spire()), crucible()]
DEFAULT_MAP = "Keel"


# =====================================================================================================
# Validation
# =====================================================================================================

def validate(m):
    errors = []
    for start, end, width, _, _ in m.ramps:
        run = math.hypot(end[0] - start[0], end[1] - start[1])
        rise = abs(end[2] - start[2])
        angle = math.degrees(math.atan2(rise, run))
        if angle > 30.5:
            errors.append(f"ramp {start}->{end} is {angle:.1f} deg (max 30)")
        for p in (start, end):
            if not (-m.half_x <= p[0] <= m.half_x and -m.half_y <= p[1] <= m.half_y):
                errors.append(f"ramp end {p} outside the walls")

    def supported(p, lo, hi, what):
        zs = [z for z in m.surfaces_at(p[0], p[1]) if lo <= p[2] - z <= hi]
        if not zs:
            errors.append(f"{what} at {p} has no surface {lo:.0f}-{hi:.0f} below it (surfaces {sorted(round(z) for z in m.surfaces_at(p[0], p[1]))})")
        return zs

    for s in m.spawns:
        supported(s, -2, 25, "spawn")
        hits = m.solid_overlaps((s[0] - 38, s[1] - 38, s[2] + 25), (s[0] + 38, s[1] + 38, s[2] + 184))
        if hits:
            errors.append(f"spawn {s} capsule hits block {hits[0][0]}..{hits[0][1]}")
    for kind, at, _ in m.weapons:
        supported(at, 40, 130, f"weapon {kind}")
    for kind, at, _ in m.powerups:
        supported(at, 40, 130, f"power-up {kind}")
    for team, at in enumerate(m.flags):
        if at is None:
            errors.append(f"flag {team} missing")
        else:
            supported(at, 40, 130, f"flag {team}")
    if m.ball is None:
        errors.append("ball missing")
    else:
        supported(m.ball, 40, 130, "ball")
    for h in m.hills:
        supported(h, 150, 300, "hill center")
    for base, target in m.lifts:
        supported(base, -2, 25, "lift base")
        supported(target, -2, 25, "lift target")

    errors.extend(validate_fill(m))
    if m.fill:
        exposed = openness(m)["exposed"]
        if exposed > MAX_EXPOSED:
            errors.append(f"{exposed * 100:.1f}% of the walkable floor has no cover within 8 m (max {MAX_EXPOSED * 100:.0f}%)")

    reds = [s for s in m.spawns if s[0] < 0]
    blues = [s for s in m.spawns if s[0] > 0]
    if len(reds) < 4 or len(blues) < 4:
        errors.append(f"needs >= 4 spawns per side (red {len(reds)}, blue {len(blues)})")
    return errors


def rect_gap(amn, amx, bmn, bmx):
    """Horizontal gap between two XY rectangles (0 when they touch or overlap)."""
    dx = max(bmn[0] - amx[0], amn[0] - bmx[0], 0.0)
    dy = max(bmn[1] - amx[1], amn[1] - bmx[1], 0.0)
    return math.hypot(dx, dy)


def rect_segment_distance(mn, mx, a, b):
    """Horizontal distance from an XY rectangle to segment a-b (sampled; fine at authoring scale)."""
    steps = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 25))
    best = float("inf")
    for i in range(steps + 1):
        t = i / steps
        x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
        best = min(best, rect_gap(mn, mx, (x, y), (x, y)))
    return best


MIN_GAP = 200.0       # a gap narrower than this between cover and other geometry snags players and bots
ANCHOR_CLEAR = 200.0  # cover keeps this far from spawns, pickups, flags, the ball and lift ends
TERRAIN_RAMP_WIDTH = 2000.0  # ramps at least this wide are ground slopes, not walkways
HEADROOM = 200.0      # cover under an overhang leaves standing room above it, or meets it


def validate_fill(m):
    errors = []
    anchors = [("spawn", s) for s in m.spawns]
    anchors += [(f"weapon {k}", at) for k, at, _ in m.weapons] + [(f"power-up {k}", at) for k, at, _ in m.powerups]
    anchors += [("flag", at) for at in m.flags if at] + ([("ball", m.ball)] if m.ball else [])
    anchors += [("lift end", p) for lift in m.lifts for p in lift]
    fill = set(m.fill)
    for i in m.fill:
        mn, mx, _ = m.blocks[i]
        name = f"cover {tuple(round(v) for v in mn)}..{tuple(round(v) for v in mx)}"
        if mn[0] < -m.half_x or mx[0] > m.half_x or mn[1] < -m.half_y or mx[1] > m.half_y:
            errors.append(f"{name} is outside the walls")
        cx, cy = (mn[0] + mx[0]) / 2, (mn[1] + mx[1]) / 2
        # Rests on a top surface, or is sunk into a slope with most of its height above it.
        if not any(abs(z - mn[2]) <= 5 or mn[2] < z < mx[2] - 100 for z in m.surfaces_at(cx, cy)):
            errors.append(f"{name} does not stand on a surface")
        for start, end, width, _, _ in m.ramps:
            if width >= TERRAIN_RAMP_WIDTH:
                continue  # a terrain slope (Creekbed's creek), not a walkway: cover may stand on it
            lo, hi = min(start[2], end[2]) - 40, max(start[2], end[2]) + HEADROOM
            if mn[2] < hi and mx[2] > lo and rect_segment_distance(mn, mx, start, end) < width / 2 + 100:
                errors.append(f"{name} blocks the ramp {start}->{end}")
        for what, p in anchors:
            if mn[2] < p[2] + 200 and mx[2] > p[2] - 150 and rect_gap(mn, mx, p, p) < ANCHOR_CLEAR:
                errors.append(f"{name} is within {ANCHOR_CLEAR:.0f} of the {what} at {p}")
        for base, target in m.lifts:
            if mx[2] > min(base[2], target[2]) + 5 and rect_segment_distance(mn, mx, base, target) < ANCHOR_CLEAR:
                errors.append(f"{name} is in the path of the lift {base}->{target}")
        for kmn, kmx in m.keepout:
            if rect_gap(mn, mx, kmn, kmx) == 0.0:
                errors.append(f"{name} is in a keep-out area {kmn}..{kmx}")
        for j, (omn, omx, _) in enumerate(m.blocks):
            if j == i:
                continue
            if omn[2] < mx[2] and omx[2] > mn[2] + 5:
                gap = rect_gap(mn, mx, omn, omx)
                if omn[0] < mx[0] and omx[0] > mn[0] and omn[1] < mx[1] and omx[1] > mn[1] and not (j in fill and j < i):
                    errors.append(f"{name} overlaps the block {omn}..{omx} (place it beside, not inside)")
                if 0.0 < gap < MIN_GAP and not (j in fill and j < i):
                    errors.append(f"{name} leaves a {gap:.0f} gap to the block {omn}..{omx}")
            elif omn[2] >= mx[2] and rect_gap(mn, mx, omn, omx) == 0.0 and (omn[0] < mx[0] and omx[0] > mn[0] and omn[1] < mx[1] and omx[1] > mn[1]):
                clearance = omn[2] - mx[2]
                if 0.0 < clearance < HEADROOM:
                    errors.append(f"{name} leaves only {clearance:.0f} headroom under {omn}..{omx}")
    return errors


MAX_EXPOSED = 0.20  # share of walkable floor allowed with no cover within 8 m (pre-expansion maps were 1-23%)


def openness(m, step=250.0, near=800.0):
    """Coarse cover metrics on a grid of standing points (the highest surface at each point).

    exposed: share of points with no chest-high geometry (top >= feet + 100) within `near` cm.
    sight_mean / sight_p90: the average of 16 horizontal eye-height (150) rays per point (how far you can see,
    on average, from where you stand), in metres; walls end every ray, so it is finite.
    Ramps are ignored as blockers, so these slightly overstate openness.
    """
    points = []
    x = -m.half_x + step / 2
    while x < m.half_x:
        y = -m.half_y + step / 2
        while y < m.half_y:
            zs = m.surfaces_at(x, y)
            if zs:
                points.append((x, y, max(zs)))
            y += step
        x += step
    exposed, sights = 0, []
    for x, y, z in points:
        covered = any(mn[2] <= z + 100 < mx[2] and 0.0 < rect_gap(mn, mx, (x, y), (x, y)) <= near for mn, mx, _ in m.blocks)
        exposed += 0 if covered else 1
        eye, total = z + 150, 0.0
        for k in range(16):
            dx, dy = math.cos(k * math.pi / 8), math.sin(k * math.pi / 8)
            hit = float("inf")
            for mn, mx, _ in m.blocks:
                if not (mn[2] < eye < mx[2]):
                    continue
                t0, t1, ok = -float("inf"), float("inf"), True
                for o, d, lo, hi in ((x, dx, mn[0], mx[0]), (y, dy, mn[1], mx[1])):
                    if abs(d) < 1e-9:
                        ok = ok and lo <= o <= hi
                    else:
                        a, b = (lo - o) / d, (hi - o) / d
                        t0, t1 = max(t0, min(a, b)), min(t1, max(a, b))
                if ok and t1 > 0 and t1 >= t0:
                    hit = min(hit, max(t0, 0.0))
            # From the top of a tall unwalkable piece the ray can clear the walls: cap it at the map's diagonal.
            total += min(hit, 2.0 * math.hypot(m.half_x, m.half_y))
        sights.append(total / 16)
    sights.sort()
    return {"points": len(points), "exposed": exposed / max(1, len(points)),
            "sight_mean": sum(sights) / max(1, len(sights)) / 100, "sight_p90": sights[int(len(sights) * 0.9)] / 100}


# =====================================================================================================
# C++ generation
# =====================================================================================================

def f(v):
    return f"{float(v):.1f}f"


def vec(p):
    return f"FVector({f(p[0])}, {f(p[1])}, {f(p[2])})"


def col(name):
    r, g, b = C[name]
    return f"FLinearColor({r:.2f}f, {g:.2f}f, {b:.2f}f)"


def style(name):
    """The palette name doubles as the surface style (AShieldbreakArena picks the world material from it)."""
    return f'FName(TEXT("{name}"))'


def generate_cpp():
    lines = [
        "// GENERATED by Design/maps/shieldbreak_maps.py -- do not edit by hand; edit the script and rerun it.",
        "// Map layouts as data: blocks, ramps, lifts, spawns, items and objective spots (map-local, red on -X).",
        "",
        '#include "Game/ShieldbreakMaps.h"',
        "",
        '#include "Weapons/ShieldbreakWeaponClasses.h"',
        "",
        "namespace ShieldbreakMapsData",
        "{",
    ]
    for m in MAPS:
        lines.append(f"\tvoid Build{m.id}(FShieldbreakMapLayout& L)")
        lines.append("\t{")
        lines.append(f'\t\tL.DisplayName = TEXT("{m.name}");')
        lines.append(f'\t\tL.InspiredBy = TEXT("{m.inspired}");')
        lines.append(f"\t\tL.HalfExtent = FVector2D({f(m.half_x)}, {f(m.half_y)});")
        lines.append(f"\t\tL.FloorZ = {f(m.floor_z)};")
        lines.append(f"\t\tL.MaxZ = {f(m.max_z)};")
        lines.append(f"\t\tL.SunRotation = FRotator({f(m.sun[0])}, {f(m.sun[1])}, 0.f);")
        lines.append(f"\t\tL.FogDensity = {m.fog:.4f}f;")
        for mn, mx, c in m.blocks:
            lines.append(f"\t\tL.Blocks.Add({{ {vec(mn)}, {vec(mx)}, {col(c)}, {style(c)} }});")
        for start, end, width, c, thick in m.ramps:
            lines.append(f"\t\tL.Ramps.Add({{ {vec(start)}, {vec(end)}, {f(width)}, {f(thick)}, {col(c)}, {style(c)} }});")
        for base, target in m.lifts:
            lines.append(f"\t\tL.Lifts.Add({{ {vec(base)}, {vec(target)} }});")
        for s in m.spawns:
            lines.append(f"\t\tL.Spawns.Add({vec(s)});")
        for kind, at, respawn in m.weapons:
            ammo = DEFAULT_AMMO.get(kind, 0)
            lines.append(f"\t\tL.Weapons.Add({{ {vec(at)}, {WEAPONS[kind]}::StaticClass(), {f(respawn)}, {ammo} }});")
        for kind, at, respawn in m.powerups:
            lines.append(f"\t\tL.Powerups.Add({{ {vec(at)}, EShieldbreakPowerupType::{POWERUPS[kind]}, {f(respawn)} }});")
        lines.append(f"\t\tL.FlagLocations[0] = {vec(m.flags[0])};")
        lines.append(f"\t\tL.FlagLocations[1] = {vec(m.flags[1])};")
        for h in m.hills:
            lines.append(f"\t\tL.HillLocations.Add({vec(h)});")
        lines.append(f"\t\tL.BallLocation = {vec(m.ball)};")
        lines.append("\t}")
        lines.append("")
    lines.append("}")
    lines.append("")
    lines.append("namespace ShieldbreakMaps")
    lines.append("{")
    lines.append("\tconst TArray<FName>& GetMapIds()")
    lines.append("\t{")
    ids = ", ".join(f'FName(TEXT("{m.id}"))' for m in MAPS)
    lines.append(f"\t\tstatic const TArray<FName> Ids = {{ {ids} }};")
    lines.append("\t\treturn Ids;")
    lines.append("\t}")
    lines.append("")
    lines.append("\tFName GetDefaultMapId()")
    lines.append("\t{")
    lines.append(f'\t\treturn FName(TEXT("{DEFAULT_MAP}"));')
    lines.append("\t}")
    lines.append("")
    lines.append("\tFName Build(FName MapId, FShieldbreakMapLayout& OutLayout)")
    lines.append("\t{")
    lines.append("\t\tOutLayout = FShieldbreakMapLayout();")
    for m in MAPS:
        lines.append(f'\t\tif (MapId == FName(TEXT("{m.id}")))')
        lines.append("\t\t{")
        lines.append(f"\t\t\tShieldbreakMapsData::Build{m.id}(OutLayout);")
        lines.append("\t\t\treturn MapId;")
        lines.append("\t\t}")
    lines.append(f"\t\tShieldbreakMapsData::Build{DEFAULT_MAP}(OutLayout);")
    lines.append("\t\treturn GetDefaultMapId();")
    lines.append("\t}")
    lines.append("}")
    return "\n".join(lines) + "\n"


# =====================================================================================================
# Previews
# =====================================================================================================

def render_png(m, path):
    from PIL import Image, ImageDraw, ImageFont

    scale = 0.12  # px per cm
    pad = 60
    w = int(2 * (m.half_x + 100) * scale) + 2 * pad
    h = int(2 * (m.half_y + 100) * scale) + 2 * pad + 70
    img = Image.new("RGB", (w, h), (18, 20, 24))
    d = ImageDraw.Draw(img)
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 13)
        big = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
    except OSError:
        font = big = ImageFont.load_default()

    def px(x, y):
        return (pad + (x + m.half_x + 100) * scale, pad + 70 + (m.half_y + 100 - y) * scale)

    zmin, zmax = m.floor_z, max([b[1][2] for b in m.blocks if b[2] != "wall"] + [m.floor_z + 1])

    def shade(color, z):
        r, g, b = C[color]
        t = 0.45 + 0.55 * (z - zmin) / max(1.0, zmax - zmin)
        return tuple(int(min(255, 255 * min(1.0, c * 1.6 + 0.08) * t)) for c in (r, g, b))

    for mn, mx, c in sorted(m.blocks, key=lambda b: b[1][2]):
        a, b = px(mn[0], mx[1]), px(mx[0], mn[1])
        d.rectangle([a, b], fill=shade(c, mx[2]), outline=(0, 0, 0))
    for start, end, width, c, _ in m.ramps:
        dx, dy = end[0] - start[0], end[1] - start[1]
        length = math.hypot(dx, dy)
        nx, ny = -dy / length * width / 2, dx / length * width / 2
        poly = [px(start[0] + nx, start[1] + ny), px(end[0] + nx, end[1] + ny), px(end[0] - nx, end[1] - ny), px(start[0] - nx, start[1] - ny)]
        d.polygon(poly, fill=shade(c, max(start[2], end[2])), outline=(0, 0, 0))
        # arrow pointing uphill
        lo, hi = (start, end) if start[2] < end[2] else (end, start)
        d.line([px(lo[0], lo[1]), px(hi[0], hi[1])], fill=(255, 255, 255), width=1)
        d.ellipse([px(hi[0], hi[1])[0] - 3, px(hi[0], hi[1])[1] - 3, px(hi[0], hi[1])[0] + 3, px(hi[0], hi[1])[1] + 3], fill=(255, 255, 255))
    for base, target in m.lifts:
        d.line([px(base[0], base[1]), px(target[0], target[1])], fill=(80, 220, 255), width=3)
        bx, by = px(base[0], base[1])
        d.ellipse([bx - 8, by - 8, bx + 8, by + 8], outline=(80, 220, 255), width=3)

    def marker(p, color, label, r=7):
        x, y = px(p[0], p[1])
        d.ellipse([x - r, y - r, x + r, y + r], fill=color, outline=(0, 0, 0))
        if label:
            d.text((x + r + 2, y - 8), label, fill=(240, 240, 240), font=font)

    for hl in m.hills:
        x, y = px(hl[0], hl[1])
        s = 300 * scale
        d.rectangle([x - s, y - s, x + s, y + s], outline=(200, 120, 255), width=2)
    for s in m.spawns:
        marker(s, (230, 70, 60) if s[0] < 0 else (70, 120, 240), "", 5)
    for kind, at, _ in m.weapons:
        marker(at, (255, 200, 40), kind.upper())
    for kind, at, _ in m.powerups:
        marker(at, (90, 230, 160), kind.upper())
    for team, at in enumerate(m.flags):
        marker(at, (255, 80, 60) if team == 0 else (80, 140, 255), "FLAG", 9)
    marker(m.ball, (255, 240, 120), "BALL", 6)

    d.text((pad, 18), f"{m.name}", fill=(255, 255, 255), font=big)
    d.text((pad + 170, 24), f"inspired by {m.inspired}   |   {2 * m.half_x / 100:.0f} m x {2 * m.half_y / 100:.0f} m   |   brighter = higher   red spawns -X, blue +X",
           fill=(190, 190, 190), font=font)
    d.text((pad, h - 22), "yellow = weapons   green = power-ups / equipment   purple squares = hills   white line = ramp (dot = top)   cyan = gravity lift",
           fill=(160, 160, 160), font=font)
    img.save(path)


def main():
    check_only = "--check" in sys.argv
    failed = False
    for m in MAPS:
        errors = validate(m)
        status = "OK" if not errors else f"{len(errors)} problem(s)"
        o = openness(m)
        print(f"{m.id:10s} {status}  ({len(m.blocks)} blocks, {len(m.fill)} cover-pass, {len(m.ramps)} ramps, {len(m.lifts)} lifts, "
              f"{len(m.spawns)} spawns; {o['exposed'] * 100:.0f}% exposed, average view {o['sight_mean']:.0f} m, p90 {o['sight_p90']:.0f} m)")
        for e in errors:
            print("    -", e)
        failed = failed or bool(errors)
    if failed:
        print("validation failed; nothing written")
        return 1
    if check_only:
        return 0
    with open(CPP_OUT, "w") as out:
        out.write(generate_cpp())
    print("wrote", os.path.relpath(CPP_OUT, ROOT))
    for m in MAPS:
        path = os.path.join(PNG_DIR, f"{m.id}.png")
        render_png(m, path)
        print("wrote", os.path.relpath(path, ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
