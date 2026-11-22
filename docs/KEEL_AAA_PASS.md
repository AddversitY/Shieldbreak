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

## Done in pass 2 (same branch)

- **Terracotta fix:** the bridges' red cast was the red team trim lines' bounce light (`-SBTrimGlow=0` shot the
  pillars near black). Team trim now lerps 70% toward the hall's cold white (`-SBKeelTeamTrim` for full colour).
- **Landmark paint:** original stencil digits and chevrons (`Art/textures/make_keel_stencils.py`, drawn from
  rectangles, worn-paint alpha), a deferred-decal master and instances (`Tools/MakeKeelDecals.bat`): "01" (red) and
  "02" (blue) over each bridge window, read from the Cradle, and hazard chevrons along the trench walls
  (`-SBNoKeelDecals`).
- **Views out:** an original procedural space panorama (`Art/textures/make_keel_space.py`: star field, faint nebula,
  a banded gas giant, no ring), shown unlit with parallax by `M_SB_KeelSpace` (`Tools/MakeKeelSpace.bat`). A 64 m
  framed window over the Breach catwalk and one over each bridge's overwatch, in front of the cladding, visual only
  (`-SBNoKeelWindow`).
- **Galleries set piece:** `SM_SB_Kit_ReactorPylon` (1.4k tris) dresses the six pylon blocks round the Camo with
  framed panels, cold light slots, a hazard band and a light column to the ceiling.
- **Shots:** `-SBHeroShots` adds `10_hero_landmark`, `11_hero_breach`, `12_hero_galleries`, `13_hero_bridge`.
- Gameplay self-test 177/178 (the known hands-through-guns check). Fixed-view Keel 168-185 FPS across pass 2 runs
  (final 177.4); baseline 156.5, so still no measurable regression. Contact sheet `keel_aaa_pass2.jpg`.

## Done in pass 3 (same branch)

- **Structure overhead:** `SM_SB_Kit_CeilingBeam` (72 m I-beam, 388 tris) eight times under the ceiling, clear of
  the lamp rows and the pod yoke; `SM_SB_Kit_PipeRun` (8 m, 728 tris) at 12.5 m along the Galleries hull and both
  faces of the band walls. Visual only, far above any jump.
- **Darker gunmetal plates** (albedo 0.10 from 0.15).
- **Tried and dropped:** 160 wall-base soot and pipe-leak decals cost about 5% FPS (168.7 with, 177.1 without) and
  barely showed in the shots, so they are not in. A higher auto-exposure floor (`-SBExposureMin=3` / `4.5`) blacked
  the map out; the default exposure stays.
- Gameplay self-test 177/178 (known hands check). Fixed-view Keel 173.8 / 179.8 on the final build.

## Done in pass 4 (same branch)

- **Reactor pulse:** `M_SB_KeelPulse` (`Tools/MakeKeelFX.bat`) breathes the reactor pylons' light slots and columns on
  a sine of game time, out of step by world X (`-SBNoKeelPulse`). GPU only, no ticking.
- **Vent fans:** `SM_SB_Kit_FanHousing` and `SM_SB_Kit_FanBlades` (Art/blender/build_keel_hero.py): six fans on the
  Galleries hull under the pipework, rotors turned by rotating movement (`-SBNoKeelFans`).
- Floor-level props were left out on purpose: the brief keeps detail off the floor for readability, and any prop
  without collision would let players walk through it.
- Self-test 177/178 (known hands check). Fixed-view Keel 170.1 / 184.6 with pass 4, 176.2 with it off.

## Done in pass 5 (same branch)

- **Positional sound** (synthesized in `ShieldbreakAmbience`, like the rest of the ambience): a whirring loop at each
  of the six Galleries fans (3 Hz blade thrum, soft motor whine) and a deep reactor drone at the pylon ring that
  swells every 4 s, on top of the ship's hum. 8 looping sources on Keel.
- **Zone light:** cold starlight from the Breach window (three unshadowed points) and a cyan glow over the reactor
  ring, so the two flanks read differently at a glance (`-SBNoKeelZoneLights`).
- **Screenshot tour:** `keel_tour.jpg`, nine player-eye views with the HUD off (`-SBShots -SBHeroShots -SBCleanShots`).
- Self-test 177/178 (known hands check). Fixed-view Keel 170.2 / 172.9 / 173.3.

## Done in pass 6 (same branch)

- **Spawn and trench mood:** a faint team hint in each bridge (warm on red, cool on blue) and low amber work lamps on
  the trench walls against the pod's cold under-glow. Ten unshadowed zone lights in all.
- **Sound levels checked numerically** (`Tools/check_keel_ambience_levels.py`, an offline replica of the loops): hum
  -28.8 dBFS RMS everywhere; after rebalancing, a fan -27.1 and the reactor drone -26.4 inside their inner radius, so
  each stands only 2-3 dB over the hum close up (the drone was 9 dB over before).
- Tour refreshed (`keel_tour.jpg`). Self-test 177/178 (known hands check). Fixed-view Keel 167.3 / 171.3.

## Done in pass 7 (2026-11-15)

- **Bridge wall consoles:** `SM_SB_Kit_WallConsole` (192 tris, 9 cm deep, no cover), glass displays with scanlines,
  two per bridge side wall at deck eye height (`15_hero_console`).
- **Fab pack tooling:** `Tools/InventoryFabPacks.bat` lists the packs' meshes once they are in the project. The packs
  are still not on the desktop (desktop control of the Launcher was refused; another thread owns that fix).
- **Keel CTF map tour with four bots:** 59/60, no bot stuck for 20 s, 108.5 FPS average (the known hands check is the
  only failure).
- **Why FPS is lower today (~117 fixed-view against ~170 on 2026-11-14):** the same build measures 116.6 with every
  Keel art layer off, so it is the PC, not the art. At idle the GPU sits at 32-38% busy: the Claude desktop app
  ~29% of the 3D engine, Chrome Remote Desktop (`remoting_desktop`) ~7%, the desktop compositor ~13%. Close the Claude
  app's busy view (or minimise it) and end the remote-desktop session before comparing FPS with earlier runs.

## Done in pass 8 (2026-11-15): the Fab Mechanic Base in Keel

- **Packs on disk (local only):** Modular Scifi Mechanic Base (UE 5.6 build) and Modular SciFi Season 1 Starter Bundle
  (5.4 build), Epic Fab content on Vincent's personal licence, staged in `Documents/Unreal Projects/WraithStaging` and
  copied to `Content/Modular_Scifi_Mechanic_Base` and `Content/StarterBundle`. Both folders are git-ignored; both
  load in 5.8 (`Tools/InventoryFabPacks.bat`: 120 and 289 static meshes, none Nanite, no LODs).
- **To restore on another PC:** add both packs from the Fab library to any 5.8 project with the Launcher, then copy the
  two folders into Shieldbreak's `Content/`. Without them Keel shows pass 1-7 art only (no errors).
- **Used in Keel (code only, loaded by path, visual only, no collision or shadows):** 7 x 7 m coffered ceiling tiles over
  the whole ship (`SM_CEILING_05`), the pack ceiling lamp in place of the glow slab (`SM_LAMP_01`), conduit trays
  (`SM_WALL_INSTALLATION_02`) and strip lights (`SM_LAMP_04`) high on the Cradle band walls, cable bundles in its
  corners, junction boxes and panels on the approach-deck walls, and door frames round all twelve 5 x 3.5 m deck doors
  (`SM_DOOR_FRAME_01_OUTSIDE`, widened 1.55x). `-SBNoKeelPacks` turns it all off for comparisons.
- Season 1 (Starter Bundle) is on disk but not used yet.
- Self-test 177/178 (known hands check). Fixed-view Keel with the pack 126.2-134.0, without 131.4 / 134.5 in the same
  session: a cost of 0-3%, inside this desktop's noise. Tour `keel_tour_packs.jpg`.

## Done in pass 9 (2026-11-15): Keel rebuilt on the Fab Mechanic Base grid

- `AShieldbreakArena::BuildKeelPackKit` replaces the hull-kit cladding on Keel when the pack is on disk: the pack's
  4 m wall modules (`SM_WALL_07/09/12/13`) at full size on every exposed block face, stretched to fill each span, the
  detailed face 1 cm proud of the block and the depth set into the block (at most half its thickness, so a thin
  wall's two faces never cross). The pack's 3 m floor tiles (`SM_FLOOR_05/07/08`) on every walkable top and along
  every ramp, flush with the collision surface. 1126 wall modules, 1019 floor tiles. Hero shells (pod, pylons) kept;
  ramp slabs dark steel under the tiles. Collision, layout and gameplay unchanged. `-SBNoKeelPackKit` = hull kit.
- Gameplay 177/178, Keel CTF tour with 4 bots 59/60 (known hands check only), no stuck bots, 127.6 FPS average.
  Fixed-view Keel 125.9 / 131.6 with the pack grid, 129.0 without (same session). Tour `keel_tour_packgrid.jpg`.
- Next: floors read pale (the pack's own floor material); darker floor variants or a tint, corner pieces at block
  corners, pack stairs on the steeper ramps.

## Done in pass 10 (2026-11-16): the alien ship (Vincent picked direction B from his Midship video)

- Original sculpted wall panel `SM_SB_Kit_AlienPanel` (Art/blender/build_keel_alien.py) on every wall cell, mottled
  mauve hull (`MI_SB_KeelMauve`, `MI_SB_KeelMauveDark`), a mauve ceiling skin, dark decks, leaning skirting with a
  purple under-glow, and glowing glyph plaques from an original procedural script (Art/textures/make_keel_glyphs.py).
- The human hull is restyled in this look: plate slots go mauve, frame and hazard slots dark mauve, light and amber
  accents violet, the reactor pylons pulse violet, chevron decals are off, and the amber lamps are rose and violet.
- Flags: `-SBNoKeelAlienSlice` (old look), `-SBKeelAlienBridgesOnly` (bridges only).
- Walls mix three sculpted panel types (`SM_SB_Kit_AlienPanel`, `B` with vents, `C` with a rose inset;
  `-SBKeelOnePanel` for one). Walkable tops get the alien deck tile `SM_SB_Kit_AlienDeck` (rounded plates, dim violet
  seams; `-SBKeelFlatDecks` for the flat skin). A/B runs: both within FPS noise (2-3%).
- Heavy hull ribs across the ceiling every 6 m (`SM_SB_Kit_AlienCeilingRib`, `-SBNoKeelCeilingRibs`), within FPS noise.
- The pack's door frames, trays, boxes and panels take the dark mauve hull in the alien look (lamps keep theirs); ramp
  tops get the alien deck tile. Wall-to-ceiling coves exist (`SM_SB_Kit_AlienWallCove`) but are opt-in
  (`-SBKeelCoves`): under the flat ceiling light they barely read.
- Vertical violet light seams in the panel gaps every fourth column (`-SBNoKeelLightSeams`), and a dimmer sky light on
  Keel (1.5 instead of 3) so the rooms take depth from their lamps and glows.
- Midship comparison (2026-11-17): darker, dustier, more mottled mauve hull so the glows carry the mood, and teal
  circuit-glass panels high on the walls (original trace pattern, Art/textures/make_keel_circuits.py,
  `M_SB_KeelCircuit`; `-SBNoKeelCircuits`), within FPS noise.
- Violet volumetric haze on the alien ship (`-SBKeelNeutralHaze` for the grey haze), within FPS noise. Tried and
  dropped: curved knee braces where the ceiling ribs meet the walls (they read flat under the ceiling light, like the
  coves).
- Two deck tiles in three are the plain `SM_SB_Kit_AlienDeckPlain` (ramps all plain): a light cross on every tile read
  as a grid of marks.
- One circuit-glass panel in three glows amber (Midship mixes teal with warm traces).
- Pale lavender ceiling lamps on the alien ship (`-SBKeelWhiteLamps` for the cool white).
- Hands-through-guns self-test (known failure, not Keel): traced on 2026-11-17 with `-SBGripDebug`. Sidearm: the
  wrist-to-index-knuckle palm line cuts the top of the grip, and the ring and pinky tips wrap round to the back strap;
  sniper: left index tip (intermittent); third-person BR: the right thumb's first bone. (Finger order in
  ShieldbreakGrip::RightBones is index, middle, ring, pinky, thumb: the servo's skipped right finger 0 is the trigger
  index, not the thumb.) Opening stuck fingers in the servo made the sidearm worse (reverted). A real fix needs the
  sidearm re-seated in the hand and the thumb pose reworked, judged on first-person shots.
- Fixed-view FPS: 176-179 with the alien look, 155 without (the panels replace heavier pack walls).

## Done in pass 11 (2026-11-21): option B hand-modelled modules

- Sculpted column shrouds on Keel's ten floor-to-ceiling pillars (`Art/blender/build_keel_columns.py`,
  `SM_SB_Kit_AlienColumn_<W>x<D>x<H>`, authored at each pillar's exact size): chamfered tapering shaft 25 cm outside
  the block, sloped skirt, flared capital into the ceiling, dark joint bands, fins and a rose inset. `-SBNoKeelColumns`.
- Angular alien portals round the twelve deck doors and both trench mouths (`SM_SB_Kit_AlienPortal_500x350`,
  `_2000x450`), replacing the pack door frames: a heavy surround sloping from 58 cm proud at the opening to 30 cm (it
  must clear the wall's 27 cm panels, or they hide it), cut top corners, dark reveal, rose keystone. `-SBNoKeelPortals`.
  Import: `Tools/ImportKeelColumns.bat`. New hero views `16_hero_door`, `17_hero_mouth`.
- Faceted alien mullions and heavy angled sills and heads on the three space windows
  (`SM_SB_Kit_AlienWindowMullion_620` / `_580`, `SM_SB_Kit_AlienWindowSill`), replacing the plain bars.
  `-SBNoKeelWindowFrames`. Fixed-view 169 FPS after.
- Sculpted cast blocks on all low cover (crates, consoles, coolant blocks, debris; `SM_SB_Kit_AlienPlinth_<W>x<D>x<H>`,
  seven sizes): sides leaning out to a dark foot, a dark mid band, a bevelled top at the gameplay height, and no light
  line on their edges any more (BuildTrim skips Keel's low cover). `-SBNoKeelPlinths`. Fixed-view 175 FPS after.
- The Cradle's pod as faceted alien hull (`SM_SB_Kit_AlienPod`, replaces the Cradle pod hero and its yoke in the alien
  look; `-SBNoKeelAlienPod`): a shell narrowing to a keel underneath, dark waist band, ribs and rose plates down the
  long sides, a dark belly plate, and four faceted hangers to the ceiling. Fixed-view 171-175 FPS after.
- Faceted hull fascia with a low curb along both sides of every ramp and every walkable ledge that drops a metre or
  more (`SM_SB_Kit_AlienRampEdge`; `-SBNoKeelRampEdges`, `-SBNoKeelLedges`).
- The Galleries' reactor pylons as sculpted alien obelisks (`SM_SB_Kit_AlienPylon`; `-SBNoKeelAlienPylons`): flared
  foot and crown, a faceted shaft that fully encloses the 2 x 2 m block, one dim violet slot per face, and no light
  beams to the ceiling; the reactor zone light goes dimmer violet. Fixed-view 170-176 FPS after.
- FPS, every option B module on vs all off (`-SBNoKeelColumns -SBNoKeelPortals -SBNoKeelWindowFrames -SBNoKeelPlinths
  -SBNoKeelAlienPod -SBNoKeelRampEdges -SBNoKeelLedges`): first 168 vs 176 (two pairs); bisected to the portals and
  window frames, which were feeding Lumen's distance fields. With `bAffectDistanceFieldLighting` off on them: on
  160.8 / 175.7, off 170.1 / 163.5, no measurable cost.
- FPS A/B (desktop busier today, all runs lower): 134/141 with, 138/123 without; no measurable cost.

## Performance (fixed-view Keel, 1280x720, no bots, single runs)

Baseline 156.5 FPS. Materials 153.6 / 159.5; hazard 152.6; pod 158.9 / 172.2 / 185.4; final (scanned block faces) 181.8. The spread between identical
builds (153-185) is larger than any change, so the pass shows no measurable regression, not a gain.

## Still open

- **Fab packs:** Modular Scifi Mechanic Base is in Vincent's Fab library but not on the desktop: adding it to a project
  needs the Epic Games Launcher (desktop control of the Launcher was refused). The two Modular SciFi Season bundles
  still need Vincent to accept Epic's licence on fab.com. Plan once on disk: stage in a throwaway project outside the
  repo (like `WraithStaging`), check they load in 5.8, migrate only the pieces Keel uses, restyle them onto
  `M_SB_KeelHull`, and keep the raw files git-ignored.
- Landmark signs for the Breach and Galleries; props and floor clutter kept off on purpose (readability).
- Fixed in pass 2: the terracotta pillars were the red team blocks themselves (a strong team tint on the block surface), not light.
