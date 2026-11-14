# Keel AAA pass (started 2026-11-14)

Goal: take Keel to the fidelity of the new soldier, following `docs/KEEL_MIDSHIP_STUDY.md`. Original layout and
shapes; professional free source material only, restyled; no paid tools; performance measured at every step.

## Done in pass 1 (branch `claude/project-thread-kwjmqj`)

- **Layout:** kept the Midship-brief rebuild already on the integration branch (`91c9332`, `62d904f`). A tighter
  96 x 72 m alternative was drafted (51 sightlines over 60 m against 127, flag-to-flag 10.9 s) but is not in the
  game; it can be revisited after playtests.
- **Scanned hull materials.** `Art/textures/make_keel_surfaces.py` packs three CC0 Poly Haven scans
  (`blue_metal_plate`, `metal_plate`, `metal_plate_02`) into one masks texture each (wear detail, roughness, height).
  `Tools/MakeKeelSurfaces.bat` builds `M_SB_KeelHull` (world-projected, three samples, surface-gradient relief, no
  normal maps) and `MI_SB_KeelPlate/Deck/Frame/Hazard`. The game puts them on Keel's hull kit slots;
  `-SBLegacyKeelHull` shows the old flat colours.
- **Bare block faces** (team pillars, walls, ceilings, cover) take the same scanned plate, floors the tread deck, and
  ramps worn hazard-orange paint (`MI_SB_KeelHazard`), so no flat panel-pattern block shows between the plating.
  The wall plates' big amber panels take the gunmetal plate (`-SBKeelHazardPlates` paints them orange instead).
- **The Cradle hero set:** `Art/blender/build_keel_hero.py` (`Tools/BuildKeelHero.ps1`, `Tools/ImportKeelHero.bat`)
  makes `SM_SB_Kit_CradlePod`, 7.7k triangles: a ribbed cargo pod wrapped round the pod block, hazard bands on its
  ends, a cold under-glow into the trench, four cables and a two-beam yoke under the ceiling. Visual only; the block
  keeps the collision. It replaces the reactor crown (`-SBLegacyKeelHero` for the old one).
- **Shots:** `-SBShots -SBHeroShots` on Keel adds four Cradle views (`6_hero_deck`, `7_hero_trench`, `8_hero_pod`,
  `9_hero_window`) before the spawn view, so the fixed-view FPS camera is unchanged.
- **Lighting:** tried darker sky light, stronger lamps and lower exposure (`-SBSky`, `-SBLampCd`, `-SBExposure`).
  Auto exposure evened most of it out, and the brightest variant cost about 5% FPS, so the defaults stay.

## Performance (fixed-view Keel, 1280x720, no bots, single runs)

Baseline 156.5 FPS. Materials 153.6 / 159.5; hazard 152.6; pod 158.9 / 172.2 / 185.4; final (scanned block faces) 181.8. The spread between identical
builds (153-185) is larger than any change, so the pass shows no measurable regression, not a gain.

## Still open

- **Fab packs:** Modular Scifi Mechanic Base is in Vincent's Fab library but not on the desktop: adding it to a project
  needs the Epic Games Launcher (desktop control of the Launcher was refused). The two Modular SciFi Season bundles
  still need Vincent to accept Epic's licence on fab.com. Plan once on disk: stage in a throwaway project outside the
  repo (like `WraithStaging`), check they load in 5.8, migrate only the pieces Keel uses, restyle them onto
  `M_SB_KeelHull`, and keep the raw files git-ignored.
- Bridge windows, Breach view to space, landmark signs per zone, reactor pylon ring in the Galleries.
- The bridges' pillars still read terracotta red on their front faces in every variant tried (kit, block and hazard
  materials changed, the colour did not), so it is probably coloured light (team glow or its bounce), not a surface.
  Next step: a `-SBNoCladding` / no-team-glow comparison to find the source.
