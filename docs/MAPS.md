# Maps

Six maps are built from code: five launch maps plus **Crucible**, the original test arena (the self-test uses it).
Each one is an original layout built on the *principles* of a classic Halo map. World materials, trim and scenery
are applied; each map has its own palette and atmosphere, and fuller environment art is still ahead.

## Competitive redesign (2026-11-02)

Keel, Bastion, Spire and Creekbed were redesigned for competitive 4v4. Each now has routes at different heights
(fast and exposed, slow and protected, flanks), power items placed so both teams can contest them, and no spawn
in sight of the enemy half. Keel and Spire are fully enclosed (a ceiling block spans the map, `bIndoor`, and
interior lamps replace the sun). The full write-up, with the decision, skill, tradeoff and counterplay behind each
change, is in the doc "Shieldbreak competitive map redesign"; the playtest checklist and metrics are there too.

Spawning: the spawn picker (`AShieldbreakGameMode::ChooseSpawnTransform`) never picks a spawn that a living enemy can
see (map geometry only, within 60 m) while a hidden one is left, so a team pushed back into its half is not spawn-trapped.

Look (visual only, no collision): each map has a hero landmark made in Blender (`Art/blender/build_hero_kit.py`): a
reactor crown hung over Keel's core, a keep-tower beyond Bastion's north wall, a hard-light condenser hung over Spire's
column, and a sandstone arch beyond Creekbed's south cliff. Creekbed's and Bastion's rock and sand blocks wear a
sandstone skin (`Art/blender/build_rock_skin.py`, `FShieldbreakMapLook::bRockSkin`). Kit glow takes each map's colour
(`FShieldbreakMapLook::KitGlow`).

Tools:

- `python3 Design/maps/shieldbreak_maps.py --check` validates without writing. On the redesigned maps it also runs
  a reachability check: a walk graph on a 50 cm grid (45 cm step-up, 7 m drops, ramps, lifts, no jumps) proves bots
  can reach every spawn, item and objective and walk back out, and flags any walkable patch over 30 m2 that is
  cut off. Tops above `max_z` (cliffs, spires) are scenery and are skipped.
- `python3 Design/maps/analyze.py [Map] [--png]` prints sprint times from each team's spawns to every item and
  objective, spawn and pickup exposure, long sightlines and three flag-to-flag routes (`ANALYZE_OUT=dir` for the
  route PNG). These are geometry estimates, not playtest results.
- `m.palette` overrides block colours per map (the look); `ShieldbreakMapArt::GetLook` holds haze, ground and
  interior-light colours.

| Map | Spawn exposure (before / now) | Standing lines over 60 m | Flag-to-flag routes (sprint) |
|---|---|---|---|
| Keel | 89 / 0 | 217 | 14.2 / 15.7 / 17.1 s |
| Bastion | 131 / 0 | 846 (the long-range map) | 13.7 / 15.1 / 15.9 s |
| Spire | 90 / 0 | 763 | 16.4 / 20.3 / 20.6 s |
| Creekbed | 161 / 0 | 555 | 11.7 / 12.8 / 13.4 s |

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
| **Keel** (default) | Midship | 136 × 88 m, enclosed | Rockets on the reactor core, Overshield under it, Rail Lance and Aegis per side (off the spawns) | CTF, Slayer |
| **Bastion** | Lockout | 128 × 104 m | Sniper on the neutral north tower, Rockets in the pit, Rail Lance on the library roof, Camo in the library, Aegis per side | KOTH, Oddball, Slayer |
| **Sanctum** | Sanctuary / The Pit | 152 × 104 m | Rockets on the courtyard plinth (crouch-jump or Power Jump), Sniper per temple perch, Rail Lance per flank walkway, Aegis per courtyard crossing, Overshield in the south shrine | CTF, Slayer |
| **Creekbed** | Beaver Creek | 120 × 80 m | Rockets on the mid-creek rock, Rail Lance on the arch, Aegis in the cave (all on the centre line) | Slayer, CTF, Oddball |
| **Spire** | Narrows / Construct | 144 × 72 m, enclosed | Sniper on the crown, Rockets in the column room, Rail Lance per gallery, Aegis per vent block; 6 gravity lifts | CTF, KOTH |

## Keel
The inside of a derelict warship, fully enclosed (15 m ceiling). Each team's hangar (deck 3 m, gantry 6.5 m)
sits behind a blast wall with three doors. The reactor hall in the middle has the rockets on the core (6.5 m),
reached by two bridges and two lifts, with the overshield in the housing underneath. North: a cargo hold with a
6.5 m catwalk. South: an engine deck over a maintenance tunnel.

![Keel](../Design/maps/Keel.png)

## Bastion
A snowbound fortress at dusk, mirror-symmetric across the middle. Each team holds a keep (flag inside) in a
walled yard whose rampart walkway joins the keep roof. North is long range: a rock ridge per side and the neutral
sniper tower. South is close range: a ruined barracks per side and the neutral library (camo) with the Rail Lance
on its roof. The pit in the middle holds the rockets under a north-south bridge.

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
A red-rock desert canyon, mirror-symmetric across the dry creek, with all three power items on the centre
line. The creek bed (3 m below the banks) has the rockets on a walkable rock. North: a mesa per side joined by a
natural arch over the gorge, with the Rail Lance on the arch. South: a cave under the cliff crossing the whole map,
with the aegis where it passes the creek. Each team's outpost holds its flag, with a roof reached by two ramps.

![Creekbed](../Design/maps/Creekbed.png)

## Spire
The hollow inside of a Forerunner tower, fully enclosed (22 m ceiling). Each base is a deck room at 7 m (flag)
over an undercroft, with three ways in. One bridge runs from each deck to a ring round the energy column; the
column room holds the rockets and its top, the crown (11 m), holds the sniper, reached only by two exposed lifts.
Each flank has a gallery (Rail Lance) and a vent block (Aegis). Floor-to-ceiling pylons break up the floor.

![Spire](../Design/maps/Spire.png)

## Crucible (test arena)
![Crucible](../Design/maps/Crucible.png)
