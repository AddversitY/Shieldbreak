# Maps

Six maps are built from code: five launch maps plus **Crucible**, the original test arena (the self-test uses it).
Each one is an original layout built on the *principles* of a classic Halo map. They are greyboxes for now;
world materials, trim and scenery are applied, with modular Spire detailing in progress.

## Current size pass (2026-10-27)

All five launch maps now use **2× their original length and width (4× ground
area)**. Heights, player movement and jump tuning are unchanged. Supporting
surfaces, ramps, spawns, items, objectives and lift targets expand together.
Crucible stays at its original size for gameplay regressions. This is the first
playable expansion; cover density and encounter pacing can still be revised
from playtesting and user sketches. Use `Design/map-planner/index.html` to draw
routes and direct further dimensions.

## Cover pass (2026-10-29)

The 2× size pass stretched each layout but kept its original handful of cover pieces, so a third to a half of
every launch map's floor had nothing chest-high within 8 m. The generator now adds a cover pass per map
(`fill_<map>()` in the script, applied after `expand_horizontal`): low cover (110–140 cm, shoot over it, crouch
behind it), tall walls (450–500 cm) that split the long lanes, and cover on the base and balcony tops.
The validator checks every cover-pass piece: it stands on a surface, stays off ramps and lift paths, keeps 2 m
from spawns, pickups, flags, the ball and lift ends, leaves no gap under 2 m (players and bots snag in those),
leaves standing room under overhangs and stays out of keep-out areas (Sanctum's plinth run-up). It also fails a
launch map with more than 20% exposed floor. Each run prints the numbers:

| Map | Exposed floor: original / after 2× / now | Average view distance: original / after 2× / now |
|---|---|---|
| Keel | 8% / 35% / 10% | 21 / 43 / 37 m |
| Bastion | 23% / 48% / 18% | 23 / 45 / 41 m |
| Sanctum | 19% / 43% / 12% (rebuilt: 12%) | 21 / 43 / 38 m (rebuilt: 36 m) |
| Creekbed | 1% / 31% / 16% | 18 / 37 / 35 m |
| Spire | 8% / 43% / 11% | 19 / 38 / 36 m |

"Exposed" is the share of standing points (a 2.5 m grid, highest surface) with no geometry at chest height
within 8 m. "Average view" is the mean length of 16 eye-height rays from each point, so it mostly tracks map
size. Elevated spots (tower tops, Keel's deck, Spire's bridge) stay exposed on purpose. These are layout
metrics, not playtest results: route timing and spawn fairness still need bot and human matches.

- **Source:** `Design/maps/shieldbreak_maps.py`. It validates every map (ramps ≤ 30°, spawns standing on surfaces with
  headroom, items and objectives above a surface, at least 4 spawns per team), generates
  `Source/Shieldbreak/Game/ShieldbreakMapsData.cpp`, and renders the previews below. Edit the script, run it, commit
  both files.
- **Switching maps:** launch option `?Map=Keel` (also `?Mode=CTF|KOTH|Oddball|Slayer`), or in-game console (`~`):
  `SB_Map Bastion KOTH`.
- **Conventions:** red plays the −X half, blue the +X half. Jump references: normal jump ~258 cm, crouch-jump ~292,
  full Power Jump ~390.

| Map | Inspired by | Size | Power weapons / items | Best modes |
|---|---|---|---|---|
| **Keel** (default) | Midship | 144 × 88 m | Rockets on the deck, Overshield under it, Rail Lance per lane, Aegis per balcony | CTF, Slayer |
| **Bastion** | Lockout | 120 × 120 m (asymmetric) | Sniper on the tall tower, Rockets in the pit, Rail Lance on the battle tower, Camo in the library, Aegis on the east wall | KOTH, Oddball, Slayer |
| **Sanctum** | Sanctuary / The Pit | 152 × 104 m | Rockets on the courtyard plinth (crouch-jump or Power Jump), Sniper per temple perch, Rail Lance per flank walkway, Aegis per courtyard crossing, Overshield in the south shrine | CTF, Slayer |
| **Creekbed** | Beaver Creek | 112 × 72 m | Rockets on the mid-creek rock, Aegis in the north cave, Rail Lance in the south cave | Slayer, CTF |
| **Spire** | Narrows / Construct | 168 × 64 m | Rockets under the bridge, Camo out on the bridge, Sniper per base perch, Rail Lance per mid platform; 4 gravity lifts | CTF, KOTH |

## Keel
Two bases face each other across a raised central deck that you can also fight under. Four ramps climb it: two
from the ends and two from the side lanes. Balconies overlook the lanes.

![Keel](../Design/maps/Keel.png)

## Bastion
Asymmetric. A sunken pit is ringed by plateaus, with a bridge across the pit. A tall sniper tower and a shorter
battle tower stand in opposite corners, and there is an enclosed library. Teams start in opposite corners.

![Bastion](../Design/maps/Bastion.png)

## Sanctum
Rebuilt on 2026-10-29 at its full 152 x 104 m size with three height layers: the sunken courtyard (0), the sand
terraces and temple halls (2 m), a walkway ring and the temple upper floors (5.5 m), and sniper perches (9 m).
Each end has a walk-through temple with a colonnaded hall, an upper floor round an open atrium (central ramp and an
outside balcony ramp) and tall corner spires. Elevated walkways run along both flanks and cross the courtyard twice;
a roofed shrine sits under each flank with a bridge from its roof, and a ruined two-storey annex in each corner
closes the walkway ring. Four colossal pillars stand round the rockets plinth, which still needs a crouch-jump or
Power Jump. The layout is original; it borrows Sanctuary's principles (symmetric temples, a central low ground,
high flank routes), not its geometry.

![Sanctum](../Design/maps/Sanctum.png)

## Creekbed
Small and fast. Two huts with roof access face each other across a shallow creek. Covered caves run along both
sides, and a rock in mid-creek holds the rockets.

![Creekbed](../Design/maps/Creekbed.png)

## Spire
Two high bases are joined by a long, narrow bridge over a lower level. Gravity lifts and ramps climb from below;
the rockets sit under the bridge and the camo out on it.

![Spire](../Design/maps/Spire.png)

## Crucible (test arena)
![Crucible](../Design/maps/Crucible.png)
