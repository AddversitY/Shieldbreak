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
| Sanctum | A sun temple in a desert basin | Monoliths with amber seams along the long walls, broken arches behind each temple, sand and warm haze. Inside: carved reliefs on tall walls, crowns on spires and colossi, amber edge bands on raised walkways, a glowing floor inlay and a hanging amber beacon over the centre, each team's sigil floating over its temple atrium |
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
- `ShieldbreakMapArt::GetLook` holds each map's ground, rock, metal, carved-stone, foliage and haze colours.

## Next

Vincent wants one map at a time, sized and filled to Halo Infinite arena scale (Sanctuary as the reference), with a
layout of our own. The kit above dresses the outside of every map; each map's own pass then adds the inside detail
(hex wall panels, trim sheets, team structures) once its layout and scale are settled.
