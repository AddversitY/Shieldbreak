# Map art style guide (2026-10-29)

Vincent's direction: get as close to the Halo feel as we can, using Halo Infinite as the reference point, with our
own, more futuristic spin. "Reverse engineering" here means studying and describing the style. We never extract,
trace or copy Halo files, models, textures, names or exact layouts, and there is never a ring or halo in the sky
(Vincent ruled it out as a copyright risk). Every asset is original.

## What the reference does (research notes)

- **Classic forms, modern rendering.** 343's art director (Sparth) aimed to recapture "the original essence of Halo":
  the cleaner, simpler shapes of Halo 1 and 2 with next-generation rendering, plus more punch and contrast from sun,
  fog and colour grading ([Inside Infinite, Dec 2020](https://www.halowaypoint.com/news/inside-infinite-december-2020)).
- **Forerunner architecture is metal, vast and old.** Halo's original art director describes Forerunner tech as
  metal (not stone), "ancient, vast, alive, and a tomb", and criticised a remake for looking shiny and new, out of
  scale and without mystery ([VGC, Aug 2026](https://www.videogameschronicle.com/news/out-of-scale-no-mystery-damn-ugly-halos-original-art-director-shares-his-thoughts-on-campaign-evolveds-architecture/)).
- **Built from trims and tiles.** Infinite's Forerunner surfaces use tile and trim sheets drawn from the legacy games,
  under one master shader ([Sharlene Lin, texture breakdown](https://sharlene.artstation.com/projects/4X1avl)).
  Hexagonal structures and "hex walls" recur in interiors.
- **Readability first.** Multiplayer spaces use landmarks to orient by (Deadlock's central cannon), pillars and rock
  to break long sight lines, multiple exits from strong positions, and bridges for crossing paths between levels
  ([Deadlock analysis](https://uxdesign.cc/halo-infinite-multiplayer-level-design-series-deadlock-bedcceaf6f16)).

## Shieldbreak's rules

| Element | Rule |
|---|---|
| Silhouette | Big, simple shapes that read from across the map: tapered slabs, chamfered (never rounded) corners, blades that sweep upward and lean, stepped plinths. One strong landmark per sight line. |
| Shape language | Angles of 30 to 60 degrees, symmetric pieces set in asymmetric groups, deep recesses between plates. Fins and ribs instead of greebles. |
| Material | Weathered blue-grey metal for structures, darker metal for recesses and inner plates. Natural rock and ground set against it (the industrial/natural contrast). Nothing should look freshly built. |
| Glow | Thin cyan hard-light seams in the recesses, used sparingly so they guide the eye. Amber on old stone (Sanctum). Team colour only on team structures: red trim on the red half, blue on the blue half. |
| Futuristic spin | Hard-light bars that bridge gaps, floating or cantilevered pieces, taller and sharper towers than classic Forerunner, and a clean, bright colour grade. |
| Atmosphere | Each map has its own haze colour and ground. Strong sun, a readable shadowed side (fill light), fog that starts beyond the arena. |
| Scale | Set pieces outside the walls are 2 to 9 times player height so they read over the walls. The playable space stays clean and readable. |
| Never | Rings or halos in the sky, Halo map layouts, Forerunner glyphs copied from the games, Spartan or Covenant shapes, Halo names. |

## Per-map moods

| Map | Mood | Set pieces |
|---|---|---|
| Keel | An orbital drydock: the arena is a ship's deck in its cradle | Hull ribs over the long walls, gantry cranes over the ends, corner beacon pylons, concrete apron |
| Bastion | A cliff fortress in cold high country | Heavy buttresses on every wall, crags, two beacon pylons, cold blue-white haze |
| Sanctum | An overgrown Forerunner-style sanctuary in a canyon (2026-11-01; was a desert sun temple) | Scanned art (below): stone with moss on top, grassy earth underfoot, mossy boulders for the rock cover, ferns and stones at the foot of every wall, a canyon of scanned cliffs round the walls with hornbeams among them, blue-green haze. Kept: the glowing trim lines, the floor inlay and amber beacon over the centre, each team's sigil over its temple |
| Creekbed | A forest valley | Pines crowding the walls, boulders, crags, green-grey haze |
| Spire | A storm-harvesting exchange above a basalt caldera | Collector fins and service panels (Spire pass), a line of tall pylons on each side, stormy blue haze |

Every launch map also gets one far **Monument** (two swept blades with hard-light bars, tips kept apart) as its
skyline landmark.

## How it's built

- `Art/blender/build_map_kit.py` makes the kit: Rib, Gantry, Pylon, Monument, Monolith, Arch, Pine, Crown, Relief,
  Fascia (walkway edge band), Inlay (floor diamond), Sigil (team-tinted cracked shield), Beacon and three rocks,
  exported to `Art/Export/MapKit`. `--render=<folder>` writes Cycles previews.
- `Tools/ImportMapArt.bat` imports only the kit into `/Game/Shieldbreak/Art/MapKit` and checks sizes and pivots
  (`Saved/Logs/MapArtImport.txt`).
- `ShieldbreakMapArt::Plan` (`Source/Shieldbreak/Game/ShieldbreakMapArt.cpp`) places it per map, seeded by map id.
  `AShieldbreakArena::BuildMapArt` draws one instanced component per piece with no collision, no navigation and no
  shadows. Solid slots get the tinted world materials; `M_Glow*` slots keep their emissive. `-SBNoMapArt` turns
  it off for before/after shots.
- `ShieldbreakMapArt::GetLook` holds each map's ground, rock, metal, carved-stone, foliage and haze colours, and
  whether the sci-fi kit clads it (`bClad`, with `CladTint` and `CladAccent`).

### Inside the walls: the sci-fi kit cladding (Sanctum first, 2026-10-30)

Cubes alone read as greybox however they're lit, so the blocks inside the map are clad with real modular panels:
27 pieces of Quaternius' CC0 **Modular Sci-Fi MegaKit** (walls 400 cm wide in 100 cm bands and 200 cm panels,
400 x 400 cm floor plates, a few columns and props), which share four PBR trim sheets.

- `Tools/GetSciFiKit.ps1` downloads the free kit into `Saved/Quaternius/kit`; `Art/blender/build_scifi_kit.py <kit>`
  converts the chosen pieces to our axes (walls face +X on X = 0, floors face +Z on Z = 0) into `Art/Export/SciFiKit`;
  `Tools/ImportSciFiKit.bat` imports them and the textures and rebuilds `M_SB_Trim` (sheet colour x `Tint`, detail
  mask painted and lit in `AccentColor`, normal and packed AO/roughness/metal).
- `ShieldbreakMapArt::Cladding` covers every wall face that looks into the map with rows (a base band, alternating
  panels, a top band) stretched a little to fit whole tiles, 2 cm proud; tops and ramps get plates 1 cm up, laid
  along the long side. Recipes per block style (temples: white plates; towers: dark and metal; walkways: plated
  decks; cover: dark bands); Sanctum's sand plateau tops stay sand. Nothing is added to collision, so routes,
  cover and sight lines stay exactly as authored. `-SBNoCladding` turns it off for comparisons.

### Scanned art: Megascans from Epic's Electric Dreams sample (Sanctum, 2026-11-01)

Vincent compared Sanctum with Halo's Sanctuary: Forerunner stone sitting inside nature (grass, trees, rock, water),
not a metal compound on sand. Hand-modelled or kit art couldn't get close, so Sanctum now uses photo-scanned assets
from Epic's free **Electric Dreams Env** sample (Fab; Epic content, licensed for Unreal Engine projects).

- `Tools/GetScanArt.bat` copies the chosen Megascans folders (mossy boulders and rock faces, forest rock formations,
  quarry cliffs, small stones, beech ferns, ivy, European hornbeams, and the MossyGrass, JungleGround,
  MossyRockyGround, BeachCliff and NordicMoss surfaces) plus `MSPresets` out of the sample into `Content/Megascans`
  and `Content/MSPresets`, keeping their `/Game` paths. They are **not in git** (several GB, and not ours to
  redistribute): every machine that builds the look runs the script after creating the sample project from Fab.
- `Tools/MakeScanArt.bat` checks the copied assets' dependencies, lists mesh bounds (`Saved/Logs/ScanArt.txt`) and
  builds `M_SB_Scan` in `/Game/Shieldbreak/Art/Scan`: two scanned surfaces projected in world space (so stretched
  cubes keep even texels), the side layer on every face and the top layer on faces that look up, its height map
  breaking up the edge. Instances: `Ground` (rocky earth, grass on top), `Stone` (cliff rock, moss on top), `Path`
  (rock, worn rocky ground on top) and `Dark` (rock, deep moss).
- `AShieldbreakArena::BuildScanArt` (maps whose look has `bScan`): puts those on the blocks and ramps (team temples
  tinted warm or cool); hides each rock-cover box (its collision stays) under a mossy boulder fitted to it; stands
  two rows of cliffs round the perimeter, their inner edge behind the walls' faces so nothing reaches into the
  playable area; roots hornbeams on the ground among them; and scatters ferns and small stones at the foot of the
  walls and structures, only where there's ground and no block. All visual, no collision. Missing assets fall back
  to the old surfaces with a warning; `-SBNoScan` turns it off for comparisons.

## Lighting lessons (2026-11-03)

- **Spire is taller than Keel.** Its 22 m ceiling now uses a separate 28 m lamp reach and 2200 cd default;
  Keel keeps the 18 m / 250 cd defaults below. The old 18 m radius left Spire's lower floor outside direct
  lamp reach. Spire also uses a near-neutral lamp colour and 0.8 kit/core emission, with original alloy,
  dark inset and quiet deck materials. Do not restore the shared glowing service-band recipe on its piers.

- **Emissive strips light the room.** Under Lumen, the glow trim along block edges (`BuildTrim`) bounces as much light as
  the lamps do. In amber it turned Keel's whole gunmetal hull orange. A map's dominant tone comes from its trim colour,
  so keep the trim neutral or cool on maps whose surfaces should read as metal, and put warm colours on thin kit accents.
  Bastion's red team strips tint the snow at the red spawn the same way. That is left in as team identity.
- **Enclosed maps' lamps.** `BuildInteriorLights` hangs unshadowed downward spots 40 cm under each ceiling:
  - 24 m apart, 250 cd, 18 m reach. The old grid (17 m, 150 cd, 26 m) looked the same and cost about 12 FPS on Keel.
  - `-SBLampSpacing=`, `-SBLampCd=`, `-SBLampRadius=` and `-SBLampVol=` for comparisons.
  - `-SBNoInteriorLights` turns them off.
- **Sun shadows on scanned maps.** Two cascades over the arena, with distance-field shadows beyond. Sanctum's full cascades
  cost 3.2 ms (8192x2048 atlas). `-SBFullSunShadows` restores them on bScan maps.
- **Kit cladding outdoors costs a lot.** `-SBClad` on Bastion cost about 20 FPS (5,300 instances) and covered the snow,
  so Bastion stays unclad.
- **Measure in one batch.** The desktop's FPS drifts by 10% or more between sessions. Interleave the variants in one run,
  twice each, and compare only within that run.

## Next

Vincent wants one map at a time, sized and filled to Halo Infinite arena scale (Sanctuary as the reference), with a
layout of our own. The kit above dresses the outside of every map; each map's own pass then adds the inside detail
(hex wall panels, trim sheets, team structures) once its layout and scale are settled.
