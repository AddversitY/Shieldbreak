# Maps

Six maps are built from code: five launch maps plus **Crucible**, the original test arena (the self-test uses it).
Each one is an original layout built on the *principles* of a classic Halo map. They are greyboxes for now;
the art pass replaces the blocks with a modular kit later.

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
| **Keel** (default) | Midship | 72 × 44 m | Rockets on the deck, Overshield under it, Rail Lance per lane, Aegis per balcony | CTF, Slayer |
| **Bastion** | Lockout | 60 × 60 m (asymmetric) | Sniper on the tall tower, Rockets in the pit, Rail Lance on the battle tower, Camo in the library, Aegis on the east wall | KOTH, Oddball, Slayer |
| **Sanctum** | Sanctuary / The Pit | 76 × 52 m | Rockets on the courtyard plinth (crouch-jump or Power Jump), Sniper per temple perch, Rail Lance per flank walkway, Overshield on the south flank | CTF, Slayer |
| **Creekbed** | Beaver Creek | 56 × 36 m | Rockets on the mid-creek rock, Aegis in the north cave, Rail Lance in the south cave | Slayer, CTF |
| **Spire** | Narrows / Construct | 84 × 32 m | Rockets under the bridge, Camo out on the bridge, Sniper per base perch, Rail Lance per mid platform; 4 gravity lifts | CTF, KOTH |

## Keel
Two bases face each other across a raised central deck that you can also fight under. Four ramps climb it: two
from the ends and two from the side lanes. Balconies overlook the lanes.

![Keel](../Design/maps/Keel.png)

## Bastion
Asymmetric. A sunken pit is ringed by plateaus, with a bridge across the pit. A tall sniper tower and a shorter
battle tower stand in opposite corners, and there is an enclosed library. Teams start in opposite corners.

![Bastion](../Design/maps/Bastion.png)

## Sanctum
An open, symmetric bowl. Each end has a temple with a sniper perch; in the sunken courtyard, the rockets sit on a
plinth that takes skill to reach. Ruined flanks carry high walkways.

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
