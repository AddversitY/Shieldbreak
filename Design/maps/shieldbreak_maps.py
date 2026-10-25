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
    """Sanctuary / The Pit principles: an open symmetric bowl. Temples at each end with a sniper perch, a sunken
    courtyard with the rockets on a plinth (crouch-jump or Power Jump to reach), ruined flanks with high walkways."""
    m = Map("Sanctum", "Sanctum", "Sanctuary / The Pit", 3800, 2600, wall_h=1100)
    m.sun = (-35.0, 70.0)
    # Raised ground (top 200) everywhere except the courtyard (2800 x 2000).
    m.block((-3800, -2600, 0), (-1400, 2600, 200), "sand", sym=True)
    m.block((-1400, 1000, 0), (1400, 2600, 200), "sand", sym=True)
    # Courtyard steps: wide ramps from each end and each flank.
    m.ramp((-1400, 0, 200), (-900, 0, 0), 500, "stone", sym=True)
    m.ramp((0, 1000, 200), (0, 600, 0), 500, "stone", sym=True)
    # Plinth (top 320): rockets on top. Falling characters step up ledges within 45, so a plain jump reaches ~303
    # (can't make it), a crouch-jump ~337 and a Power Jump ~430 can.
    m.block((-150, -150, 0), (150, 150, 320), "stone")
    # Courtyard cover.
    m.block((-900, 600, 0), (-750, 850, 160), "rock", sym=True)
    m.block((500, 450, 0), (750, 520, 120), "rock", sym=True)
    # Temples (top 550) with two side ramps and a sniper perch (top 800, two 125 steps).
    m.block((-3800, -800, 200), (-3000, 800, 550), "red")
    m.block((3800, 800, 200), (3000, -800, 550), "blue")
    m.ramp((-2300, 650, 200), (-3000, 650, 550), 250, sym=True)
    m.ramp((-2300, -650, 200), (-3000, -650, 550), 250, sym=True)
    m.block((-3750, -200, 550), (-3450, 200, 800), "stone", sym=True)
    m.block((-3450, -200, 550), (-3300, 200, 675), "stone", sym=True)
    m.block((-3040, -300, 550), (-3000, 300, 640), "stone", sym=True)
    # Ruined flanks: broken walls along the +Y edge (mirrored onto -Y) and a high walkway with a ramp.
    for x0, x1 in [(-2600, -2000), (-1700, -1100), (-800, -300)]:
        m.block((x0, 1650, 200), (x1, 1720, 520), "stone", sym=True)
    m.block((-2200, 2050, 200), (-600, 2450, 450), "stone", sym=True)
    m.ramp((-600, 2250, 450), (200, 2250, 200), 300, sym=True)
    # Items.
    m.weapon("rockets", (0, 0, 390))
    m.weapon("sniper", (-3600, 0, 870), sym=True)
    m.weapon("rail", (-1400, 2250, 520), sym=True)
    m.powerup("overshield", (0, -1800, 270))
    for s in [(-3400, 500, 550), (-3400, -500, 550), (-2600, 1200, 200), (-2600, -1300, 200), (-1800, 0, 200), (-1600, 2250, 450)]:
        m.spawn(s)
    m.flag((-3200, 0, 640))
    m.hills = [(0, 600, 220), (-3400, 0, 770), (0, -600, 220), (3400, 0, 770), (-1400, 2250, 670)]
    m.ball = (0, -600, 90)
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


MAPS = [keel(), bastion(), sanctum(), creekbed(), spire(), crucible()]
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

    reds = [s for s in m.spawns if s[0] < 0]
    blues = [s for s in m.spawns if s[0] > 0]
    if len(reds) < 4 or len(blues) < 4:
        errors.append(f"needs >= 4 spawns per side (red {len(reds)}, blue {len(blues)})")
    return errors


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
        print(f"{m.id:10s} {status}  ({len(m.blocks)} blocks, {len(m.ramps)} ramps, {len(m.lifts)} lifts, {len(m.spawns)} spawns)")
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
