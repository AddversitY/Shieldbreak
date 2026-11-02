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
        self.reflect = False  # mirror the blue side across x = 0 instead of through the centre (see mir)
        self.indoor = False  # enclosed: a ceiling over the whole map and interior lighting (set by roof())
        self.palette = {}  # per-map colour overrides for the palette names in C (the map's look)
        self.check_reach = False  # reachability() errors fail validation (the 2026-11 redesigned maps)
        # Floor slab + perimeter walls.
        hx, hy, t = half_x, half_y, 100.0
        self.block((-hx - t, -hy - t, floor_z - 100), (hx + t, hy + t, floor_z), "floor")
        self.block((-hx - t, hy, floor_z), (hx + t, hy + t, floor_z + wall_h), "wall")
        self.block((-hx - t, -hy - t, floor_z), (hx + t, -hy, floor_z + wall_h), "wall")
        self.block((hx, -hy, floor_z), (hx + t, hy, floor_z + wall_h), "wall")
        self.block((-hx - t, -hy, floor_z), (-hx, hy, floor_z + wall_h), "wall")

    # -- authoring --
    def mir(self, p):
        """The blue-side twin of a red-side point: point symmetry (x, y) -> (-x, -y) by default, or a reflection across
        x = 0 (x, y) -> (-x, y) on maps with reflect=True (north and south differ, both teams face the same ones)."""
        return (-p[0], p[1], p[2]) if self.reflect else mirror(p)

    def roof(self, thickness=100.0):
        """Enclose the map: a ceiling at the top of the perimeter walls. Bots scan for floors below it."""
        hx, hy, t = self.half_x, self.half_y, 100.0
        self.block((-hx - t, -hy - t, self.floor_z + self.wall_h), (hx + t, hy + t, self.floor_z + self.wall_h + thickness), "dark")
        self.indoor = True
        self.max_z = min(self.max_z, self.floor_z + self.wall_h - 60.0)

    def wall_x(self, x0, x1, y, z0, z1, gaps=(), t=60.0, color="wall", sym=False):
        """A wall along X at y (thickness t), from x0 to x1, with openings: gaps = [(xa, xb, z_lo, z_hi)].
        Each opening cuts the wall between z_lo and z_hi (a door: z_lo = floor, z_hi = lintel)."""
        self._wall(0, x0, x1, y, z0, z1, gaps, t, color, sym)

    def wall_y(self, y0, y1, x, z0, z1, gaps=(), t=60.0, color="wall", sym=False):
        """A wall along Y at x; see wall_x."""
        self._wall(1, y0, y1, x, z0, z1, gaps, t, color, sym)

    def _wall(self, axis, a0, a1, c, z0, z1, gaps, t, color, sym):
        def box(u0, u1, za, zb):
            if u1 - u0 < 1 or zb - za < 1:
                return
            if axis == 0:
                self.block((u0, c - t / 2, za), (u1, c + t / 2, zb), color, sym)
            else:
                self.block((c - t / 2, u0, za), (c + t / 2, u1, zb), color, sym)
        cuts = sorted(gaps)
        u = a0
        for ga, gb, zl, zh in cuts:
            box(u, ga, z0, z1)
            box(ga, gb, z0, max(z0, zl))   # sill under the opening
            box(ga, gb, min(z1, zh), z1)   # lintel over it
            u = gb
        box(u, a1, z0, z1)

    def block(self, a, b, color, sym=False):
        mn = tuple(min(a[i], b[i]) for i in range(3))
        mx = tuple(max(a[i], b[i]) for i in range(3))
        self.blocks.append((mn, mx, color))
        if sym:
            ma, mb = self.mir(mn), self.mir(mx)
            self.block(ma, mb, color)

    def cover(self, a, b, color="cover", sym=False):
        """A cover-pass block: same as block(), but validate_fill() checks its clearances."""
        first = len(self.blocks)
        self.block(a, b, color, sym)
        self.fill.extend(range(first, len(self.blocks)))

    def ramp(self, start, end, width, color="ramp", sym=False, thickness=40.0):
        self.ramps.append((start, end, width, color, thickness))
        if sym:
            self.ramps.append((self.mir(start), self.mir(end), width, color, thickness))

    def lift(self, base, target, sym=False):
        self.lifts.append((base, target))
        if sym:
            self.lifts.append((self.mir(base), self.mir(target)))

    def spawn(self, feet, sym=True):
        """Red spawn (and its blue mirror). Yaw faces the map center."""
        self.spawns.append(feet)
        if sym:
            self.spawns.append(self.mir(feet))

    def weapon(self, kind, at, sym=False, respawn=None):
        self.weapons.append((kind, at, respawn or DEFAULT_RESPAWN[kind]))
        if sym:
            self.weapons.append((kind, self.mir(at), respawn or DEFAULT_RESPAWN[kind]))

    def powerup(self, kind, at, sym=False, respawn=None):
        self.powerups.append((kind, at, respawn or DEFAULT_RESPAWN[kind]))
        if sym:
            self.powerups.append((kind, self.mir(at), respawn or DEFAULT_RESPAWN[kind]))

    def flag(self, red_at, blue_at=None):
        self.flags = [red_at, blue_at or self.mir(red_at)]

    # -- geometry queries (for validation) --
    def surfaces_at(self, x, y, pad=0.0):
        """All walkable top surfaces (z) over (x, y)."""
        out = []
        roof_z = self.floor_z + self.wall_h if self.indoor else math.inf
        for mn, mx, _ in self.blocks:
            if mn[2] >= roof_z:
                continue  # the ceiling of an enclosed map is not a floor
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
    """Keel (2026-11 redesign): the inside of a derelict warship, fully enclosed. Vertical control is the theme.

    Each team's hangar (deck at 3 m, gantry at 6.5 m) faces down the reactor hall. The rockets sit on top of the reactor
    core (6.5 m) in the middle of the hall: two bridges and two gravity lifts reach it, all in view of each other.
    The Overshield sits under the core on its own timer. Each flank is a cargo hold (ground floor, crates, with a
    6.5 m catwalk along the hull: fast but exposed) at one end and an engine deck (6.5 m, machinery cover) over an
    enclosed maintenance tunnel (protected, but cut off from teammates) at the other. Point-symmetric: red's
    north hold mirrors into blue's south hold, red's south engine deck into blue's north one.
    Levels: hold/hall floor 0, decks 300, catwalks/gantries/core 650, ceiling 1500."""
    m = Map("Keel", "Keel", "Midship", 6800, 4400, wall_h=1500, max_z=1600)
    # Look: gunmetal hull, hazard-orange ramps and amber glow strips.
    m.palette = {"metal": (0.23, 0.24, 0.26), "wall": (0.10, 0.11, 0.12), "floor": (0.14, 0.15, 0.16),
                 "bridge": (0.27, 0.28, 0.30), "ramp": (0.78, 0.42, 0.08), "glow": (1.00, 0.52, 0.12),
                 # Team colours toned down: under the sodium lamps the default red turned whole decks orange.
                 "red": (0.21, 0.15, 0.14), "blue": (0.13, 0.16, 0.21)}
    m.sun = (-52.0, 120.0)
    m.fog = 0.004
    m.check_reach = True
    m.roof()
    L1, L2, S = 300, 650, 40
    WY = 2630  # hall / flank dividing walls (60 thick, y 2600..2660)

    # ---- Hangars (red at -X): deck at L1, gantry at L2 behind the flag, ramps down into the hall ----
    m.block((-6800, -2600, 0), (-5300, 2600, L1), "red", sym=True)
    m.block((-6800, -2600, L2 - S), (-6300, 2600, L2), "metal", sym=True)
    m.block((-6360, -900, L2), (-6300, 900, L2 + 110), "metal", sym=True)          # gantry rail
    m.ramp((-5400, 2250, L1), (-6300, 2250, L2), 400, sym=True)
    m.ramp((-5400, -2250, L1), (-6300, -2250, L2), 400, sym=True)
    m.ramp((-5300, 1500, L1), (-4600, 1500, 0), 500, sym=True)
    m.ramp((-5300, -1500, L1), (-4600, -1500, 0), 500, sym=True)
    # Blast wall across the hangar mouth: the hangar sees the hall only through its three doors (gangway, two ramps),
    # so the flag room is defended from inside and the hall is not one 130 m sightline from hangar to hangar.
    m.wall_y(-2600, 2600, -5270, 0, 1500, [(-350, 350, L1, 900), (1200, 1800, 0, 800), (-1800, -1200, 0, 800)],
             color="red", sym=True)
    # Gangway: the fast, exposed lane from the deck to the hall's loading platform (L1).
    m.block((-5300, -350, L1 - S), (-3400, 350, L1), "bridge", sym=True)
    # Loading platform in the hall (L1): ramps down toward the core and back toward the hangar.
    m.block((-3400, -1100, 0), (-2300, 1100, L1), "metal", sym=True)
    m.ramp((-2300, 850, L1), (-1700, 850, 0), 400, sym=True)
    m.ramp((-3400, -850, L1), (-4000, -850, 0), 400, sym=True)

    # ---- Reactor core: slab at L2 over a reactor housing (a small room with doors on the north and south sides,
    # so the hall's long axis is blocked at every height). The Overshield is inside the housing. ----
    m.block((-600, -600, L2 - S), (600, 600, L2), "metal")
    m.wall_y(-450, 450, -420, 0, L2 - S, t=60, color="dark", sym=True)
    m.wall_x(-450, 450, 420, 0, L2 - S, [(-250, 250, 0, 400)], t=60, color="dark", sym=True)
    m.block((-250, 600, L2 - S), (250, 3800, L2), "bridge", sym=True)   # red's bridge to its north catwalk
    m.lift((-1500, 0, 0), (-400, 0, L2), sym=True)

    # ---- Hall / flank walls, full height, with doors (x0, x1, z_lo, z_hi) ----
    m.wall_x(-6800, 0, WY, 0, 1500, [(-6000, -5500, L1, 700), (-4300, -3800, 0, 400), (-2000, -1500, 0, 400),
                                    (-300, 0, 0, 1000)], sym=True)
    m.block((-6000, 2600, 0), (-5500, 2660, L1), "red", sym=True)  # under the hangar door: deck height
    m.wall_x(-6800, 0, -WY, 0, 1500, [(-6000, -5500, L1, 700), (-4700, -4200, 0, 400), (-2700, -2200, 0, 400),
                                     (-300, 0, 0, 1000)], sym=True)
    m.block((-6000, -2660, 0), (-5500, -2600, L1), "red", sym=True)

    # ---- North flank, red end: cargo hold with a catwalk along the hull ----
    m.block((-6800, 2660, 0), (-5000, 4400, L1), "red", sym=True)                 # dock (L1)
    m.ramp((-5000, 3400, L1), (-4300, 3400, 0), 600, sym=True)
    m.ramp((-6300, 4100, L1), (-5300, 4100, L2), 500, sym=True)                   # dock -> catwalk landing
    m.block((-5300, 3800, L2 - S), (-4400, 4400, L2), "metal", sym=True)
    m.block((-4400, 3800, L2 - S), (300, 4400, L2), "metal", sym=True)            # catwalk
    m.lift((-2200, 3150, 0), (-2200, 4100, L2), sym=True)

    # ---- South flank, red end: engine deck (L2) over an enclosed maintenance tunnel ----
    m.block((-6800, -4400, 0), (-5000, -2660, L1), "red", sym=True)               # dock (L1)
    m.ramp((-5000, -3300, L1), (-4300, -3300, 0), 600, sym=True)
    m.block((-4400, -4400, L2 - S), (-1600, -3200, L2), "metal", sym=True)        # engine deck
    m.block((-1900, -3200, L2 - S), (-1600, -2700, L2), "metal", sym=True)        # its east landing
    m.ramp((-5050, -4000, L1), (-4400, -4000, L2), 400, sym=True)                 # dock -> deck
    m.ramp((-400, -2900, 0), (-1600, -2900, L2), 400, sym=True)                   # hall side -> deck
    m.wall_x(-4400, -1600, -3230, 0, L2 - S, [(-3300, -2800, 0, 400)], sym=True)  # tunnel's inner wall
    m.wall_y(-4400, -3260, -3900, 0, L2 - S, [(-3800, -3260, 0, 400)], sym=True)   # bulkheads: the tunnel doglegs,
    m.wall_y(-4400, -3260, -2000, 0, L2 - S, [(-4400, -3860, 0, 400)], sym=True)   # no straight shot through it

    # ---- Cover: sightline breakers in the hall, low cover on every deck and catwalk ----
    m.cover((-2100, 1250, 0), (-1500, 2000, 1500), "metal", sym=True)      # coolant tanks: break hall diagonals
    m.cover((-2100, -2000, 0), (-1500, -1250, 1500), "metal", sym=True)
    m.cover((-4200, 1000, 0), (-3900, 1300, 130), sym=True)
    m.cover((-1100, -1600, 0), (-800, -1300, 130), sym=True)
    m.cover((-3000, 1800, 0), (-2700, 2100, 130), sym=True)
    m.cover((-3000, -2000, 0), (-2600, -1800, 130), sym=True)
    m.cover((-1000, 1200, 0), (-700, 1500, 130), sym=True)
    m.cover((-3000, -600, L1), (-2700, 600, 1500), "metal", sym=True)       # loading platform: crane column
    m.cover((-450, 250, L2), (-350, 450, L2 + 110), "metal", sym=True)      # core top
    m.cover((-5800, 700, L1), (-5600, 1000, L1 + 110), "metal", sym=True)   # hangar deck
    m.cover((-5800, -1000, L1), (-5600, -700, L1 + 110), "metal", sym=True)
    m.cover((-3600, 2900, 0), (-3300, 3400, 450), "metal", sym=True)        # hold: container + crates
    m.cover((-3000, 3700, 0), (-2700, 4000, 130), sym=True)
    m.cover((-1600, 3250, 0), (-1100, 3750, 450), "metal", sym=True)
    m.cover((-3600, 3800, L2), (-3400, 3950, L2 + 110), "metal", sym=True)  # catwalk
    m.cover((-1400, 4250, L2), (-1200, 4400, L2 + 110), "metal", sym=True)
    m.cover((-3700, -4100, L2), (-3300, -3700, 1100), "metal", sym=True)    # engine deck machinery
    m.cover((-2600, -3600, L2), (-2400, -3300, L2 + 110), "metal", sym=True)
    m.cover((-3600, -4100, 0), (-3400, -3900, 130), sym=True)               # tunnel
    # Hull ribs: 3 m frames off the hull and the hall walls, every 10 m where nothing else stands. They break the
    # lines that hug the walls down the whole ship (long lines over 60 m, 10 m sample: 217 -> 135), so a long
    # corridor is a run of 10 m bays. Split round the catwalk and the engine deck; none stands on a catwalk or bridge.
    for a, b in [((-5030, 2660, L1), (-4970, 2960, 1500)), ((-4030, 4100, 0), (-3970, 4400, 610)),
                 ((-3030, 2660, 0), (-2970, 2960, 1500)), ((-2030, 2660, 0), (-1970, 2960, 1500)),
                 ((-1030, 4100, 0), (-970, 4400, 610)), ((-1030, 2660, 0), (-970, 2960, 1500)),
                 ((-30, 4100, 0), (30, 4400, 610)), ((-30, 2660, 0), (30, 2960, 610)),
                 ((970, 4100, 0), (1030, 4400, 1500)), ((970, 2300, 0), (1030, 2600, 1500)),
                 ((1970, 4100, 0), (2030, 4400, 400)), ((1970, 4100, L2), (2030, 4400, 1500)),
                 ((2970, 4100, 0), (3030, 4400, 610)), ((2970, 2660, 0), (3030, 2960, 1500)),
                 ((3970, 4100, L2), (4030, 4400, 1500))]:
        m.cover(a, b, "metal", sym=True)

    # ---- Items and objectives ----
    m.weapon("rockets", (0, 0, L2 + 70))
    m.powerup("overshield", (0, 0, 70))
    m.weapon("rail", (-700, 3300, 70), sym=True)
    m.powerup("aegis", (-1800, -3700, 70), sym=True)
    for sp in [(-6000, 1300, L1), (-6000, -1300, L1), (-6300, 3300, L1), (-6300, -3400, L1), (-6600, 1500, L2)]:
        m.spawn(sp)
    m.flag((-6050, 0, L1 + 90))
    m.hills = [(0, 0, L2 + 220), (-2850, 850, L1 + 220), (-2700, 3300, 220), (2850, -850, L1 + 220), (2700, -3300, 220)]
    m.ball = (0, 1500, 90)
    return m


def bastion():
    """Bastion (2026-11 redesign): a snowbound fortress, outdoors. North against south: long range against close range.

    Mirror-symmetric across x = 0 (reflect=True): each team holds a keep at its end (flag inside, a rampart roof above
    reached by two outside ramps) and faces the same map. North: open snowfield with a rock ridge per side and the
    neutral sniper tower on the centre line (two exposed ramps and a lift). South: a curtain-wall rampart per side and
    the neutral library (camo inside, close quarters) whose roof is the battle tower (Rail Lance). Middle: the sunken
    pit (rockets, under the north-south bridge). Levels: pit 0, plateau 300, ridges and ramparts 650, roofs 950 / 900,
    sniper tower 1000."""
    m = Map("Bastion", "Bastion", "Lockout", 6400, 5200, wall_h=1300, max_z=1800)
    # Look: packed snow and ice-grey stone, dark rock, weathered timber ramps, warm beacon light.
    m.palette = {"stone": (0.72, 0.74, 0.79), "rock": (0.24, 0.24, 0.28), "tower": (0.44, 0.46, 0.52),
                 "bridge": (0.38, 0.40, 0.46), "ramp": (0.30, 0.32, 0.36), "glow": (1.00, 0.62, 0.28),
                 "red": (0.21, 0.15, 0.14), "blue": (0.13, 0.16, 0.21)}   # bases in near-neutral metal with a hint of their team; the red/blue trim lines carry the colour
    m.reflect = True
    m.check_reach = True
    m.sun = (-22.0, 200.0)   # low dusk sun
    m.fog = 0.012
    P = 300

    # ---- Terrain: plateau at P everywhere except the pit (4800 x 4000) ----
    m.block((-6400, 2000, 0), (6400, 5200, P), "stone")
    m.block((-6400, -5200, 0), (6400, -2000, P), "stone")
    m.block((-6400, -2000, 0), (-2400, 2000, P), "stone", sym=True)
    m.ramp((-2400, 1300, P), (-1600, 1300, 0), 700, "stone", sym=True)        # from each team's side
    m.ramp((-2400, -1300, P), (-1600, -1300, 0), 700, "stone", sym=True)
    m.ramp((-1300, 2000, P), (-1300, 1300, 0), 600, "stone", sym=True)        # from the north and south flanks
    m.ramp((-1300, -2000, P), (-1300, -1300, 0), 600, "stone", sym=True)
    # North-south bridge over the pit (the exposed rotation between the towers), 2.6 m clear underneath.
    m.block((-250, -2000, P - 40), (250, 2000, P), "bridge")

    # ---- Keeps (red at -X): a walled hall with the flag, inside a fortress yard. The yard's outer wall has three
    # gates and a rampart walkway on top (800) that joins the keep roof in a U; defenders climb to it from inside the
    # yard, attackers from the north and south fields at the wall's ends. ----
    K0, K1, KR = -6400, -5200, 800
    m.wall_y(-1200, 1200, K1, P, KR - 40, [(-350, 350, P, P + 400)], color="red", sym=True)
    m.wall_x(K0, K1 + 30, 1200, P, KR - 40, [(-6000, -5600, P, P + 400)], color="red", sym=True)
    m.wall_x(K0, K1 + 30, -1200, P, KR - 40, [(-6000, -5600, P, P + 400)], color="red", sym=True)
    m.block((K0, -1230, KR - 40), (K1 + 30, 1230, KR), "stone", sym=True)                    # keep roof
    m.wall_y(-2000, 2000, -4000, P, KR, [(-400, 400, P, P + 400), (1050, 1550, P, P + 400), (-1550, -1050, P, P + 400)],
             t=400, color="red", sym=True)                                                     # outer wall + walkway
    m.block((K1 + 30, 1200, KR - 40), (-4200, 1700, KR), "stone", sym=True)                   # walkway <-> roof
    m.block((K1 + 30, -1700, KR - 40), (-4200, -1200, KR), "stone", sym=True)
    m.ramp((-4450, 350, P), (-4450, 1300, KR), 400, sym=True)                                 # up from the yard
    m.ramp((-4450, -350, P), (-4450, -1300, KR), 400, sym=True)
    m.ramp((-4000, 2900, P), (-4000, 2000, KR), 400, sym=True)                                # up from the fields
    m.ramp((-4000, -2900, P), (-4000, -2000, KR), 400, sym=True)
    m.block((-3840, -300, KR), (-3800, 300, KR + 110), "stone", sym=True)                     # walkway battlement
    # Pit-corner crags: frame the pit and cut the corner-to-corner lines across it.
    m.block((-2900, 1700, P), (-2400, 2300, 1100), "rock", sym=True)
    m.block((-2900, -2300, P), (-2400, -1700, 1100), "rock", sym=True)

    # ---- North: the ridge per side and the sniper tower on the centre line ----
    m.block((-3600, 2900, P), (-2700, 3900, 650), "rock", sym=True)
    m.ramp((-4650, 3400, P), (-3600, 3400, 650), 400, "rock", sym=True)
    m.ramp((-2700, 3700, 650), (-2100, 3700, P), 300, "rock", sym=True)
    m.block((-700, 3600, P), (700, 4800, 1000), "tower")
    m.ramp((-2100, 4200, P), (-700, 4200, 1000), 400, sym=True)
    m.lift((0, 2800, P), (0, 3800, 1000))
    m.block((-5300, 3300, P), (-4800, 3800, 1300), "rock", sym=True)                          # ice spires
    m.block((-2200, 4400, P), (-1700, 4900, 1300), "rock", sym=True)

    # ---- South: curtain-wall rampart per side and the library / battle tower on the centre line ----
    m.block((-4400, -3200, P), (-2600, -2800, 650), "stone", sym=True)
    m.ramp((-5050, -3000, P), (-4400, -3000, 650), 400, sym=True)
    m.ramp((-2600, -3000, 650), (-1950, -3000, P), 400, sym=True)
    # Ruined barracks per side: roofless walls, a close-quarters shortcut through the south field.
    m.wall_x(-4800, -3400, -3800, P, 700, [(-4300, -3900, P, 700)], color="stone", sym=True)
    m.wall_x(-4800, -3400, -4900, P, 700, color="stone", sym=True)
    m.wall_y(-4900, -3800, -4800, P, 700, [(-4600, -4100, P, 700)], color="stone", sym=True)
    m.wall_y(-4900, -3800, -3400, P, 700, [(-4600, -4100, P, 700)], color="stone", sym=True)
    LW = 900
    m.wall_x(-1600, 1600, -3400, P, LW - 40, [(-1100, -600, P, P + 400), (600, 1100, P, P + 400)])
    m.wall_x(-1600, 1600, -4800, P, LW - 40, [(-300, 300, P, P + 400)])
    m.wall_y(-4800, -3400, -1600, P, LW - 40, [(-4300, -3800, P, P + 400)], sym=True)
    m.block((-1630, -4830, LW - 40), (1630, -3370, LW), "stone")
    m.ramp((-2900, -3700, P), (-1630, -3700, LW), 400, sym=True)
    for px_, py_ in ((-900, -4300), (-900, -3900)):
        m.block((px_ - 100, py_ - 100, P), (px_ + 100, py_ + 100, LW - 40), "stone", sym=True)  # library pillars

    # ---- Cover ----
    m.cover((-250, 700, P), (-150, 900, P + 110), "bridge", sym=True)        # bridge posts
    m.cover((-250, -900, P), (-150, -700, P + 110), "bridge", sym=True)
    m.cover((-1500, -500, 0), (-1200, -200, 140), "rock", sym=True)          # pit
    m.cover((-950, 400, 0), (-550, 800, 1100), "rock", sym=True)             # ice pillars in the pit
    m.cover((-950, -900, 0), (-550, -500, 1100), "rock", sym=True)
    m.cover((-2100, -400, 0), (-1800, 400, 450), "rock", sym=True)
    m.cover((-800, -1700, 0), (-500, -1400, 140), "rock", sym=True)
    m.cover((-3300, -200, P), (-3100, 600, P + 450), "stone", sym=True)
    m.cover((-2000, 2700, P), (-1700, 3000, P + 130), "rock", sym=True)
    m.cover((-3000, 4500, P), (-2700, 4800, P + 130), "rock", sym=True)
    m.cover((-5600, 4200, P), (-5000, 4500, P + 450), "rock", sym=True)
    m.cover((-1500, 4900, P), (-1200, 5200, P + 130), "rock", sym=True)
    m.cover((-700, 3600, 1000), (-450, 3700, 1110), "stone", sym=True)     # sniper tower parapets
    m.cover((-3300, 3700, 650), (-3100, 3900, 760), "rock", sym=True)       # ridge top
    m.cover((-6200, -4600, P), (-5600, -4300, P + 450), "stone", sym=True)
    m.cover((-2300, -4600, P), (-2000, -4300, P + 130), "stone", sym=True)
    m.cover((-3500, -2900, 650), (-3300, -2800, 760), "stone", sym=True)    # rampart lip

    # ---- Items and objectives ----
    m.weapon("rockets", (0, 0, 70))
    m.weapon("sniper", (0, 4300, 1070))
    m.weapon("rail", (0, -4300, LW + 70))
    m.powerup("camo", (0, -4100, P + 70))
    m.powerup("aegis", (-3000, 1200, P + 70), sym=True)
    for sp in [(-5800, 500, P), (-4600, 900, P), (-5800, 2000, P), (-5800, -2000, P), (-5800, 600, KR)]:
        m.spawn(sp)
    m.flag((-5800, -400, P + 90), (5800, -400, P + 90))
    m.hills = [(0, 900, 220), (0, 4200, 1220), (0, -4100, P + 220), (-3150, 3400, 870), (3150, 3400, 870)]
    m.ball = (0, -1000, 90)
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
    """Creekbed (2026-11 redesign; look reworked 2026-11-02): a green, creek-fed sandstone canyon in warm low
    light, outdoors. Two silver Forerunner outposts face each other across a running creek that drops in as a waterfall under the
    arch (the water, grass and trees are the scanned terrain's "Creek" set: ShieldbreakMapArt.cpp, BuildTerrainScan).

    Mirror-symmetric across x = 0 (reflect=True); all three power items sit on the centre line, so neither team is
    closer to any of them. Middle: the creek, 3 m below the banks, with the rockets on a walkable rock in
    mid-creek (fast and exposed). North: a mesa per side joined by a natural arch over the creek's gorge, with the Rail
    Lance on the arch (high, exposed, sees the creek). South: a cave runs under the cliff the whole width of the map,
    with the aegis where it crosses the creek (slow and protected). Each outpost is an open pavilion (2026-11-02, Vincent:
    no small enclosed base rooms): the flag under a roof on pillars, waist-high walls, a drop hole over the flag, two
    ramps and a jump route to the roof. Two creek rocks under the arch give a jump route to the Rail Lance. Levels: creek 0, mid rock 200, banks 300, mesas and arch 650, outpost roofs 740."""
    m = Map("Creekbed", "Creekbed", "Beaver Creek", 6000, 4000, wall_h=1500, max_z=1300)
    # Look: grass-green banks, tan sandstone, pale stone ramps.
    m.palette = {"sand": (0.30, 0.36, 0.20), "rock": (0.58, 0.47, 0.34), "ramp": (0.66, 0.60, 0.50),
                 "glow": (1.00, 0.74, 0.40),
                 "red": (0.44, 0.40, 0.40), "blue": (0.38, 0.41, 0.46)}   # silver Forerunner metal, a hint of team
    m.reflect = True
    m.check_reach = True
    m.sun = (-34.0, 235.0)   # warm, low afternoon sun: lit rims, deep shade in the gorge
    m.fog = 0.004
    P, M = 300, 650

    # ---- Terrain: banks at P, the creek bed at 0 between them, mesas (north) and the cave band (south) ----
    m.block((-6000, -2700, 0), (-1300, 2600, P), "sand", sym=True)
    m.ramp((-1300, -50, P), (-700, -50, 0), 5300, "sand", sym=True)
    m.block((-6000, 2600, 0), (-4400, 4000, P), "sand", sym=True)
    m.block((-4400, 2600, 0), (-600, 4000, M), "rock", sym=True)                 # mesa
    m.block((-600, 2900, M - 120), (600, 3700, M), "rock")                       # the natural arch
    m.ramp((-3600, 1600, P), (-3600, 2600, M), 400, "rock", sym=True)           # up from the bank
    m.ramp((-5400, 3300, P), (-4400, 3300, M), 400, "rock", sym=True)           # up from the outpost yard

    # ---- South: the cave under the cliff, crossing the creek ----
    m.block((-6000, -4000, 0), (-600, -2700, P), "sand", sym=True)
    m.block((-600, -4000, 0), (600, -2700, P), "sand")
    m.block((-4400, -4000, P + 400), (0, -2700, 1500), "rock", sym=True)         # cliff over the cave
    m.wall_x(-4400, 0, -2800, P, P + 400, [(-3700, -3100, P, P + 400), (-2200, -1600, P, P + 400), (-500, 0, P, P + 400)],
             t=200, color="rock", sym=True)                                       # cave face with openings
    m.ramp((0, -1900, 0), (0, -2700, P), 800, "sand")                            # creek bed up into the cave
    m.block((-3000, -3600, P), (-2700, -3300, P + 400), "rock", sym=True)       # cave pillars
    m.block((-1500, -3500, P), (-1200, -3200, P + 400), "rock", sym=True)

    # ---- Outposts (red at -X): open pavilions, not walled rooms. A roof on six pillars over the flag, waist-high
    # walls in short runs (cover you fight over, not doors you funnel through), a drop hole in the roof straight down
    # onto the flag, and three ways up: two outside ramps, and a quick, exposed jump route up the front (crate, then
    # the roof edge). The back stays open onto a path round the outpost: the flag can be taken from any side. ----
    R = P + 440
    T = R - 40                                                                    # roof underside
    for px, py in [(-5650, -1250), (-5650, 1250), (-4350, -1250), (-4350, 1250), (-5650, 0)]:
        m.block((px - 100, py - 100, P), (px + 100, py + 100, T), "red", sym=True)  # pillars
    m.block((-4850, -1250, P), (-4650, -1050, T), "red", sym=True)                # side pillars
    m.block((-4850, 1050, P), (-4650, 1250, T), "red", sym=True)
    # Roof (top R) round a 4 m drop hole over the flag; porches past the pillars.
    m.block((-5750, -1500, T), (-4250, -250, R), "red", sym=True)
    m.block((-5750, 250, T), (-4250, 1500, R), "red", sym=True)
    m.block((-5750, -250, T), (-5250, 250, R), "red", sym=True)
    m.block((-4750, -250, T), (-4250, 250, R), "red", sym=True)
    # Waist-high walls (1.1 m: crouch cover, jump-over), in short runs with gaps.
    m.cover((-4400, -900, P), (-4300, -350, P + 110), "red", sym=True)            # front
    m.cover((-4400, 350, P), (-4300, 900, P + 110), "red", sym=True)
    m.cover((-5700, -800, P), (-5600, -400, P + 110), "red", sym=True)            # back
    m.cover((-5700, 400, P), (-5600, 800, P + 110), "red", sym=True)
    m.cover((-5550, -1150, P), (-5150, -1050, P + 110), "red", sym=True)          # sides (off a pillar)
    m.cover((-5550, 1050, P), (-5150, 1150, P + 110), "red", sym=True)
    m.ramp((-3500, 1250, P), (-4250, 1250, R), 400, sym=True)
    m.ramp((-3500, -1250, P), (-4250, -1250, R), 400, sym=True)
    m.block((-4250, -150, P), (-4000, 150, P + 210), "rock", sym=True)           # jump route: crate (2.1 m), then the roof (2.4 m)

    # ---- Skill route to the Rail Lance: two creek rocks under the arch, a jump each (2.5 m, then 2.2 m, then 1.8 m
    # onto the arch). Fast and exposed against the slow, safe ramp-and-mesa way round. ----
    m.block((-430, 2250, 0), (-180, 2500, 250), "rock", sym=True)
    m.block((-480, 2620, 0), (-230, 2840, 470), "rock", sym=True)

    # ---- The creek: mid rock (rockets) and boulders ----
    m.block((-500, -500, 0), (500, 500, 200), "rock")
    m.ramp((0, 1000, 0), (0, 500, 200), 400, "rock")
    m.ramp((0, -1000, 0), (0, -500, 200), 400, "rock")

    # ---- Bank landmarks: tall hoodoos break the outpost-to-outpost lines ----
    m.block((-2700, -700, P), (-2300, -300, 1300), "rock", sym=True)
    m.block((-3100, 1000, P), (-2700, 1400, 1300), "rock", sym=True)

    # ---- Cover ----
    m.cover((-3700, 300, P), (-3400, 600, P + 140), "rock", sym=True)
    m.cover((-2000, -1800, P), (-1700, -1500, P + 140), "rock", sym=True)
    m.cover((-2000, 1000, P), (-1700, 1300, P + 140), "rock", sym=True)
    m.cover((-3600, -1900, P), (-3300, -1600, P + 400), "rock", sym=True)
    m.cover((-500, 1300, 0), (-250, 1600, 140), "rock", sym=True)                # creek boulders
    m.cover((-700, -1500, 0), (-450, -1200, 450), "rock", sym=True)
    m.cover((-2300, 300, P), (-2000, 600, P + 140), "rock", sym=True)           # bank
    m.cover((-3300, -1000, P), (-3000, -700, P + 140), "rock", sym=True)
    m.cover((-2500, -2400, P), (-2200, -2100, P + 140), "rock", sym=True)
    m.cover((-2500, 2000, P), (-2200, 2300, P + 400), "rock", sym=True)
    m.cover((-1700, -600, P), (-1450, -300, P + 140), "rock", sym=True)
    m.cover((-1700, 1900, P), (-1450, 2200, P + 140), "rock", sym=True)
    m.cover((-3700, 3500, M), (-3400, 3700, M + 130), "rock", sym=True)         # mesa top
    m.cover((-1500, 2900, M), (-1300, 3200, M + 130), "rock", sym=True)
    m.cover((-2600, 3200, M), (-2300, 3500, M + 130), "rock", sym=True)         # mesa top
    m.cover((-3700, -3300, P), (-3400, -3100, P + 130), "rock", sym=True)       # cave
    m.cover((-4900, 1700, P), (-4600, 2000, P + 140), "rock", sym=True)         # outpost yard
    m.cover((-4900, -2000, P), (-4600, -1700, P + 140), "rock", sym=True)

    # ---- Items and objectives (all three power items on the centre line) ----
    m.weapon("rockets", (0, 0, 270))
    m.weapon("rail", (0, 3300, M + 70))
    m.powerup("aegis", (0, -3400, P + 70))
    for sp in [(-5300, 500, P), (-5800, 2200, P), (-5800, -2200, P), (-5400, 900, R), (-5600, -3400, P)]:
        m.spawn(sp)
    m.flag((-5000, 0, P + 90))
    m.hills = [(0, 0, 420), (0, 3300, M + 220), (0, -3400, P + 220), (-3000, -2300, P + 220), (3000, -2300, P + 220)]
    m.ball = (0, 1900, 90)
    return m


def spire():
    """Spire (2026-11 redesign): the hollow inside of a Forerunner tower, fully enclosed (22 m ceiling). High against low,
    split by the energy column in the middle.

    Each base is a deck at 7 m (flag) over an undercroft at floor level; the deck is a room behind a full-height front
    wall with three ways in (bridge + lift, and a ramp from each flank). A 5 m bridge runs from each deck to a ring
    round the column, so the old 56 m bridge sightline is broken by the column itself. Floor-to-ceiling pylons and
    bridge supports cut the 140 m floor into 30-40 m fights. The column's base is a room
    with four doors holding the rockets; its top is the crown (11 m) with the sniper, reached only by two exposed
    gravity lifts from the side galleries. Each flank has a long gallery (3.5 m) at one team's end and a raised vent
    block at the other. Point-symmetric. Levels: floor 0, galleries 350, bridge / ring / decks 700, crown 1100."""
    m = Map("Spire", "Spire", "Narrows / Construct", 7200, 3600, wall_h=2200, max_z=2400)
    # Look: silver-white Forerunner metal, cool grey floor, blue hard light.
    m.palette = {"metal": (0.66, 0.68, 0.71), "wall": (0.50, 0.52, 0.56), "floor": (0.36, 0.38, 0.42),
                 "bridge": (0.60, 0.62, 0.66), "ramp": (0.52, 0.60, 0.70), "glow": (0.22, 0.62, 1.00),
                 "red": (0.21, 0.15, 0.14), "blue": (0.13, 0.16, 0.21)}   # bases in near-neutral metal with a hint of their team; the red/blue trim lines carry the colour
    m.sun = (-30.0, 160.0)
    m.fog = 0.003
    m.check_reach = True
    m.roof()
    G, B, C, S = 350, 700, 1100, 40

    # ---- Bases: deck at B over an undercroft with three doors ----
    m.block((-7200, -3600, B - S), (-5800, 3600, B), "red", sym=True)
    m.wall_y(-3600, 3600, -5800, 0, B - S, [(-2400, -1600, 0, 400), (-400, 400, 0, 400), (1600, 2400, 0, 400)],
             color="red", sym=True)
    m.block((-5830, -900, B), (-5770, 900, B + 110), "red", sym=True)          # deck lip
    m.lift((-5200, 1200, 0), (-6200, 1200, B), sym=True)
    # Deck front: a wall up to the ceiling with three ways in (bridge + lift, two gallery ramps), so the decks are rooms
    # and the 140 m deck-to-deck lines are cut to what the doors allow.
    # Neutral silver above the deck: a 15 m red (blue) wall bounced team colour over the whole ceiling. The deck, its lip
    # and the undercroft wall stay team-coloured.
    m.wall_y(-3600, 3600, -5800, B, 2200, [(-3400, -2800, B, B + 400), (-700, 700, B, B + 400), (950, 1450, B, B + 700), (2800, 3400, B, B + 400)],
             color="wall", sym=True)

    # ---- Bridge from the deck to the ring round the column ----
    m.block((-5800, -250, B - S), (-1400, 250, B), "bridge", sym=True)
    m.block((-1400, 600, B - S), (1400, 1400, B), "bridge", sym=True)          # ring: north / south
    m.block((-1400, -600, B - S), (-600, 600, B), "bridge", sym=True)          # ring: west / east
    m.lift((-3600, -1200, 0), (-3600, 0, B), sym=True)
    # Floor-to-ceiling pylons: bridge supports and lane breakers, so the 140 m floor is a run of 30-40 m fights.
    m.block((-4600, -250, 0), (-4400, 250, B - S), "metal", sym=True)
    m.block((-2400, -250, 0), (-2200, 250, B - S), "metal", sym=True)
    for x0, y0, x1, y1 in [(-4300, 2000, -4000, 2300), (-500, 1800, -200, 2100), (-2300, -2100, -2000, -1800),
                           (-3300, 700, -3000, 1000)]:
        m.block((x0, y0, 0), (x1, y1, 2200), "metal", sym=True)   # structure; only the column glows
    # Two pylons through the ring (one per side of the column) break the diagonal deck-door-to-deck-door lines that
    # run past the column at deck height (deck-to-deck lines 14 -> 7, lines over 100 m 101 -> 70); the ring walkway
    # keeps 4.5 m beside each.
    m.block((-1300, -1350, 0), (-1000, -1050, 2200), "metal", sym=True)
    m.block((-150, -1350, 0), (150, -1050, 2200), "metal", sym=True)

    # ---- The energy column: a room with four doors (rockets), its roof the crown (sniper) ----
    m.wall_y(-600, 600, -570, 0, C - S, [(-200, 200, 0, 400)], color="glow", sym=True)
    m.wall_x(-540, 540, 570, 0, C - S, [(-200, 200, 0, 400)], color="glow", sym=True)
    m.block((-600, -600, C - S), (600, 600, C), "glow")
    m.lift((-2400, 3000, G), (-300, 300, C), sym=True)

    # ---- North flank, red end: the long gallery ----
    m.block((-5200, 2600, G - S), (-800, 3600, G), "metal", sym=True)
    m.ramp((-3200, 1800, 0), (-3200, 2600, G), 500, sym=True)
    m.ramp((-5150, 3100, G), (-5800, 3100, B), 500, sym=True)
    m.ramp((-1000, 2700, G), (-1000, 1400, B), 400, sym=True)

    # ---- South flank, red end: the vent block ----
    m.block((-5200, -3600, 0), (-3200, -2600, G), "metal", sym=True)
    m.ramp((-2400, -3100, 0), (-3200, -3100, G), 500, sym=True)
    m.ramp((-5150, -3100, G), (-5800, -3100, B), 500, sym=True)

    # ---- Cover ----
    m.cover((-4600, -250, B), (-4400, -100, B + 110), "metal", sym=True)       # bridge: staggered
    m.cover((-2600, 100, B), (-2400, 250, B + 110), "metal", sym=True)
    m.cover((-500, -500, C), (-300, -300, C + 110), "metal", sym=True)         # crown
    m.cover((550, 1200, B), (700, 1350, B + 110), "metal", sym=True)       # ring
    m.cover((-4800, 1400, 0), (-4500, 1700, 130), sym=True)                    # floor
    m.cover((-4400, -1800, 0), (-4100, -1500, 130), sym=True)
    m.cover((-2600, 900, 0), (-2300, 1200, 130), sym=True)
    m.cover((-2000, -1500, 0), (-1700, -1200, 450), "metal", sym=True)
    m.cover((-2000, 1100, 0), (-1700, 1600, 450), "metal", sym=True)
    m.cover((-5300, -500, 0), (-5000, 500, 450), "metal", sym=True)
    m.cover((-3000, -2400, 0), (-2700, -2100, 130), sym=True)
    m.cover((-4200, 3000, G), (-4000, 3300, G + 110), "metal", sym=True)       # gallery
    m.cover((-1500, 2900, G), (-1300, 3100, G + 110), "metal", sym=True)
    m.cover((-4600, -3100, G), (-4400, -2700, G + 110), "metal", sym=True)     # vent block
    m.cover((-6900, 2100, B), (-6600, 2400, B + 110), "metal", sym=True)       # deck
    m.cover((-6900, -2400, B), (-6600, -2100, B + 110), "metal", sym=True)

    # ---- Items and objectives ----
    m.weapon("rockets", (0, 0, 70))
    m.weapon("sniper", (0, 0, C + 70))
    m.weapon("rail", (-1800, 3250, G + 70), sym=True)
    m.powerup("aegis", (-3800, -3100, G + 70), sym=True)
    for sp in [(-6600, 1500, B), (-6600, -1500, B), (-6700, 2600, 0), (-6700, -2600, 0), (-6300, 2900, B)]:
        m.spawn(sp)
    m.flag((-6600, 0, B + 90))
    m.hills = [(0, 0, C + 220), (-3600, 0, B + 220), (-3200, 3100, G + 220), (3600, 0, B + 220), (3200, -3100, G + 220)]
    m.ball = (0, 2000, 90)
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

    errors.extend(validate_fill(m))
    if m.fill:
        exposed = openness(m)["exposed"]
        if exposed > MAX_EXPOSED:
            errors.append(f"{exposed * 100:.1f}% of the walkable floor has no cover within 8 m (max {MAX_EXPOSED * 100:.0f}%)")

    if m.check_reach:
        errors.extend(reachability(m)[0])
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
            zs = [z for z in m.surfaces_at(x, y) if z < m.max_z]   # tops above MaxZ are scenery
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
# Reachability (2026-11-01): can a bot walk from the spawns to every part of the map and back?
# Bots walk (step-ups up to 45 cm, ramps), drop off ledges up to 7 m and ride lifts; they never jump. So a raised
# area a bot can't reach has no ramp, stair or lift up to it. Players can also jump, so tops of cover and
# skill ledges are fine as "jump-only" islands as long as they are small.
# =====================================================================================================

REACH_STEP = 50.0      # grid spacing (walls must be at least this thick so the walk can't pass through them)
STEP_UP = 45.0         # the character's step height
RAMP_STEP = 75.0       # rise between neighbouring points when either one is on a ramp
MAX_DROP = 700.0       # matches ShieldbreakArenaPrivate::MaxDropHeight
STAND_H = 184.0        # standing capsule height
ISLAND_OK = 30.0       # m2: unreachable standable patches up to this size are jump-only spots (cover tops, perches)


class _Index:
    def __init__(self, m, cell=500.0):
        self.m, self.cell, self.cells = m, cell, {}
        for i, (mn, mx, _) in enumerate(m.blocks):
            for cx in range(int(math.floor(mn[0] / cell)), int(math.floor(mx[0] / cell)) + 1):
                for cy in range(int(math.floor(mn[1] / cell)), int(math.floor(mx[1] / cell)) + 1):
                    self.cells.setdefault((cx, cy), []).append(i)

    def blocks_at(self, x, y):
        for i in self.cells.get((int(math.floor(x / self.cell)), int(math.floor(y / self.cell))), ()):
            mn, mx, _ = self.m.blocks[i]
            if mn[0] <= x <= mx[0] and mn[1] <= y <= mx[1]:
                yield mn, mx

    def stands(self, x, y):
        """Standable surfaces at (x, y): [(z, on_ramp)] with a free standing capsule above each."""
        cols = list(self.blocks_at(x, y))
        roof_z = self.m.floor_z + self.m.wall_h if self.m.indoor else math.inf
        tops = [(mx[2], False) for mn, mx in cols if mn[2] < roof_z]
        for start, end, width, _, _ in self.m.ramps:
            dx, dy = end[0] - start[0], end[1] - start[1]
            l2 = dx * dx + dy * dy
            t = ((x - start[0]) * dx + (y - start[1]) * dy) / l2
            if 0.0 <= t <= 1.0 and abs((x - start[0]) * dy - (y - start[1]) * dx) / math.sqrt(l2) <= width / 2:
                tops.append((start[2] + (end[2] - start[2]) * t, True))
        out = []
        for z, ramp in tops:
            if any(mn[2] < z + STAND_H and mx[2] > z + 5 for mn, mx in cols):
                continue
            out.append((z, ramp))
        out.sort()
        merged = []
        for z, ramp in out:
            if merged and abs(merged[-1][0] - z) < 5:
                merged[-1] = (max(z, merged[-1][0]), ramp or merged[-1][1])
            else:
                merged.append((z, ramp))
        return merged


class WalkGraph:
    """Bot walk graph over a REACH_STEP grid of standable points: nodes (i, j, z, on_ramp), fwd/back edges."""
    pass


def walk_graph(m):
    idx = _Index(m)
    s = REACH_STEP
    nx, ny = int(2 * m.half_x / s), int(2 * m.half_y / s)
    grid = {}
    for i in range(nx):
        x = -m.half_x + (i + 0.5) * s
        for j in range(ny):
            y = -m.half_y + (j + 0.5) * s
            st = idx.stands(x, y)
            if st:
                grid[(i, j)] = st
    nodes, node_of = [], {}
    for (i, j), st in grid.items():
        for k, (z, ramp) in enumerate(st):
            node_of[(i, j, k)] = len(nodes)
            nodes.append((i, j, z, ramp))
    fwd = [[] for _ in nodes]
    back = [[] for _ in nodes]
    for n, (i, j, z, ramp) in enumerate(nodes):
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            st = grid.get((i + di, j + dj))
            if not st:
                continue
            for k, (z2, ramp2) in enumerate(st):
                dz = z2 - z
                limit = RAMP_STEP if (ramp or ramp2) else STEP_UP
                if dz <= limit and dz >= -MAX_DROP:
                    t = node_of[(i + di, j + dj, k)]
                    fwd[n].append(t)
                    back[t].append(n)
    def nearest(p):
        i, j = int((p[0] + m.half_x) / s), int((p[1] + m.half_y) / s)
        best, bd = None, 1e18
        for di in range(-2, 3):
            for dj in range(-2, 3):
                for k, (z, _) in enumerate(grid.get((i + di, j + dj), ())):
                    d = abs(z - p[2]) + 10 * (abs(di) + abs(dj))
                    if d < bd:
                        best, bd = node_of[(i + di, j + dj, k)], d
        return best
    for base, target in m.lifts:
        a, b = nearest(base), nearest(target)
        if a is not None and b is not None:
            fwd[a].append(b)
            back[b].append(a)
    g = WalkGraph()
    g.m, g.idx, g.step, g.grid, g.nodes, g.node_of, g.fwd, g.back, g.nearest = m, idx, s, grid, nodes, node_of, fwd, back, nearest
    g.pos = lambda n: (-m.half_x + (nodes[n][0] + 0.5) * s, -m.half_y + (nodes[n][1] + 0.5) * s, nodes[n][2])
    return g


def reachability(m, g=None):
    """Returns (errors, stats): every spawn, pickup and objective in one walkable network; no big unreachable areas."""
    g = g or walk_graph(m)
    s, grid, nodes, node_of, fwd, back, nearest = g.step, g.grid, g.nodes, g.node_of, g.fwd, g.back, g.nearest
    def flood(start, edges):
        seen = bytearray(len(nodes))
        stack = [start]
        seen[start] = 1
        while stack:
            n = stack.pop()
            for t in edges[n]:
                if not seen[t]:
                    seen[t] = 1
                    stack.append(t)
        return seen
    errors = []
    root = nearest(m.spawns[0])
    there, home = flood(root, fwd), flood(root, back)
    main = [there[n] and home[n] for n in range(len(nodes))]
    # Anchors: every spawn, pickup, objective must be in the main walkable network.
    anchors = [("spawn", p) for p in m.spawns] + [(f"weapon {k}", p) for k, p, _ in m.weapons]
    anchors += [(f"power-up {k}", p) for k, p, _ in m.powerups] + [("flag", p) for p in m.flags if p]
    anchors += [("ball", m.ball)] if m.ball else []
    anchors += [("hill", (h[0], h[1], h[2] - 220)) for h in m.hills]
    for what, p in anchors:
        n = nearest((p[0], p[1], p[2] - (0 if what in ("spawn", "hill") else 70)))
        if n is None:
            errors.append(f"{what} at {p}: no standable point near it")
        elif not there[n]:
            errors.append(f"{what} at {p}: bots can't walk there from the spawns")
        elif not home[n]:
            errors.append(f"{what} at {p}: bots can't walk back out (a trap)")
    # Large patches outside the main network.
    cell_area = (s / 100.0) ** 2
    off = [n for n in range(len(nodes)) if not main[n]]
    seen = set()
    islands = []
    by_pos = {}
    for n in off:
        i, j, z, _ = nodes[n]
        by_pos.setdefault((i, j), []).append(n)
    for n in off:
        if n in seen:
            continue
        comp, stack = [], [n]
        seen.add(n)
        while stack:
            a = stack.pop()
            comp.append(a)
            i, j, z, _ = nodes[a]
            for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                for b in by_pos.get((i + di, j + dj), ()):
                    if b not in seen and abs(nodes[b][2] - z) <= RAMP_STEP:
                        seen.add(b)
                        stack.append(b)
        area = len(comp) * cell_area
        i, j, z, _ = nodes[comp[0]]
        if area > ISLAND_OK and z < m.max_z:   # tops above MaxZ are scenery (cliffs): the bots' scan starts below them
            xs = [nodes[c][0] for c in comp]
            ys = [nodes[c][1] for c in comp]
            cx = -m.half_x + (sum(xs) / len(xs) + 0.5) * s
            cy = -m.half_y + (sum(ys) / len(ys) + 0.5) * s
            reach_in = any(there[c] for c in comp)
            islands.append((area, cx, cy, z, reach_in))
    for area, cx, cy, z, reach_in in sorted(islands, reverse=True):
        kind = "a one-way trap (no way back out)" if reach_in else "unreachable without jumping"
        errors.append(f"{area:.0f} m2 around ({cx:.0f}, {cy:.0f}, z {z:.0f}) is {kind}")
    total = len(nodes) * cell_area
    walk = sum(main) * cell_area
    levels = {}
    for n in range(len(nodes)):
        if main[n]:
            band = int(nodes[n][2] // 250) * 250
            levels[band] = levels.get(band, 0) + cell_area
    return errors, {"standable_m2": total, "network_m2": walk, "levels": levels}


# =====================================================================================================
# C++ generation
# =====================================================================================================

def f(v):
    return f"{float(v):.1f}f"


def vec(p):
    return f"FVector({f(p[0])}, {f(p[1])}, {f(p[2])})"


def col(name, m=None):
    r, g, b = (m.palette.get(name) if m is not None else None) or C[name]
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
        if m.indoor:
            lines.append("\t\tL.bIndoor = true;")
        for mn, mx, c in m.blocks:
            lines.append(f"\t\tL.Blocks.Add({{ {vec(mn)}, {vec(mx)}, {col(c, m)}, {style(c)} }});")
        for start, end, width, c, thick in m.ramps:
            lines.append(f"\t\tL.Ramps.Add({{ {vec(start)}, {vec(end)}, {f(width)}, {f(thick)}, {col(c, m)}, {style(c)} }});")
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

    roof_z = m.floor_z + m.wall_h if m.indoor else math.inf
    shown = [b for b in m.blocks if b[0][2] < roof_z]  # an enclosed map's ceiling would hide everything
    zmin, zmax = m.floor_z, max([b[1][2] for b in shown if b[2] != "wall"] + [m.floor_z + 1])

    def shade(color, z):
        r, g, b = m.palette.get(color) or C[color]
        t = 0.45 + 0.55 * (z - zmin) / max(1.0, zmax - zmin)
        return tuple(int(min(255, 255 * min(1.0, c * 1.6 + 0.08) * t)) for c in (r, g, b))

    for mn, mx, c in sorted(shown, key=lambda b: b[1][2]):
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
    d.text((pad + 170, 24), f"{'enclosed   |   ' if m.indoor else ''}inspired by {m.inspired}   |   {2 * m.half_x / 100:.0f} m x {2 * m.half_y / 100:.0f} m   |   brighter = higher   red spawns -X, blue +X",
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
