# Keel shell kit (2026-11-25)

Goal: Keel at the quality of Vincent's reference (a fan-made Covenant-style ship interior): glossy, curved, purple armour
metal with sharp highlights, a dark room, small blue-cyan accents, amber back-lit grilles, crystal pylons. Original
pieces only; nothing from Halo is copied.

Previews (Cycles, rendered in the cloud from the scripts below, not in Unreal):
`docs/playtests/2026-11-25_keel_shell/keel_shell_corridor.jpg`, `keel_shell_window.jpg`, `keel_shell_hall.jpg`,
`keel_shell_pieces.jpg`.

## Why Keel looks flat today

Twelve passes (docs/KEEL_AAA_PASS.md) added a lot, but four things in how Keel is built cap how good it can look:

1. **Every room is still a box.** Keel's rooms are the gameplay blocks, and the art is laid on their faces (wall
   panels 1 cm proud, ceiling ribs under a flat ceiling, coves that "barely read under the flat ceiling light"). Walls
   stay vertical, ceilings flat, corners square. The reference and Midship get their look from structure: walls that
   lean, haunches that curve into the ceiling, ribs you can see the shape of.
2. **One hue at one value.** Mauve hull, mauve ceiling skin, violet lamps, violet haze, pink glyphs, rose insets. In the
   2026-11-21 tour almost every pixel is mid-value purple, so nothing is dark enough to be mass and nothing bright
   enough to be a highlight. The only contrast is dozens of small teal glyph plaques, which read as noise.
3. **Matte, dusty materials.** The hull is a world-projected mottled noise (roughness up to .58) that reads as painted
   plaster. The reference is glossy metal: low roughness and strong reflections of small bright lights. Without
   gloss, curves don't show either, because curvature on screen is mostly a moving highlight.
4. **Lighting that evens everything out.** Auto exposure lifts dark rooms back to mid grey (pass 1: "auto exposure
   evened most of it out"), the sky light fills every shadow, and the main lights are large flat ceiling panels and
   unshadowed points. There are no pools of light and dark and no warm/cool split.

## What the kit is

`Art/blender/build_keel_shell_kit.py` builds the room's cross-section itself instead of decorating box faces. Every
solid part is bevelled and then subdivided, so edges come out rounded like moulded armour.

Profile (one side, cm, X into the room): a steel kick skirt leaning out from 55 cm at the floor to a lit crease at
1 m, a wall leaning 120 cm in up to 5.6 m, then a ribbed haunch curving into the ceiling at 7.6 m, 3.62 m into the
room. A 400 cm ceiling spine with a louvred light trough fills the middle, so the standard corridor is 11.24 m wall to
wall. Bays are 400 cm. All constants are at the top of the script, so variants for other room heights and widths are a
re-run, not a remodel.

| Piece (`SM_SB_Kit_KeelShell_*`) | What it is | Tris |
| --- | --- | --- |
| WallBay | skirt, cyan guide line, violet-blue crease, leaning wall with layered lilac plates and inlay, ribbed haunch | 41.7k |
| WallBayVent | the same bay with an amber back-lit grille in the lower half | 43.7k |
| WallBayWindow | the same bay with a heavy rounded window frame and two mullions (glass is the engine's) | 35.8k |
| Rib | structural rib along the whole profile: heavy foot, slim mid wall with a blue vein, thick knee with a lilac plate | 26.4k |
| RibBeam | the rib's crossbeam over the ceiling spine | 6.9k |
| CeilingSpine | 400 x 400 ceiling: lilac panels, round lamp pucks, louvred light trough | 4.1k |
| FloorPlate | 400 x 400 deck: four rounded plates with tread strips | 4.2k |
| FloorGrate | 400 x 400 grate over a dark sub-floor with blue under-glow | 0.7k |
| EdgeStrip | 400 cm fascia for walkable edges and ramp sides, cyan guide line | 3.7k |
| Pillar | 600 cm smooth fluted column, swelling foot, flared capital, blue light in the flutes | 3.6k |
| Pylon | 2.6 m crystal pylon: four teardrop petals round a blue crystal | 8.7k |
| Portal | round-arched surround for a 500 x 350 door: two-layer cowl, steel jamb, cyan line, keystone fin | 22.5k |

Materials (palette keys in `Art/Export/palette.json`): ShellHull (deep purple metal), ShellPlate (lighter lilac
armour), ShellSteel (dark violet gunmetal), ShellGrate (near black), ShellGlowCyan, ShellGlowBlue, ShellGlowAmber.
UV0 is a world-scale box projection (1 UV = 4 m) so tiling materials line up across pieces.

Budget: the wall pieces are heavy for classic LODs on purpose; the rounded edges are real geometry. They are meant for
Nanite, which the 2080 Ti runs. `Tools/ImportKeelShell.bat` imports them and turns Nanite on.

## Run it

- Author: `Tools\BuildKeelShell.ps1` (add `-Render` for the preview images; about 45 minutes on 4 CPU cores).
- Import: `Tools\ImportKeelShell.bat` (narrow import `SM_SB_Kit_KeelShell_*`, then `Tools/enable_keel_shell_nanite.py`).
- In the cloud the same script runs under the `bpy` 4.5 module: `python Art/blender/build_keel_shell_kit.py --render`.

Tested in Unreal on the desktop 2026-11-25/26: see "Desktop results" below.

## Desktop results (2026-11-25/26, DESKTOP-888HMR5, worktree on `claude/keel-shell-kit`)

- **Build:** `Tools\BuildEditor.bat` passed first time; the shell code in `ShieldbreakArena.cpp` needed no fixes.
- **Import:** `Tools\ImportKeelShell.bat` imported all 12 `SM_SB_Kit_KeelShell_*` meshes with Nanite on
  (`Saved\Logs\KeelShellNanite.txt`: `RESULT 12 meshes`) and created the 7 `MI_SB_Shell*` instances. The log ends
  "0 error(s), 4 warning(s)"; the warnings are "nearly zero tangents/binormals" on WallBayVent and WallBayWindow.
- **Self-test:** run 1 was 172/176. The three first-person reload checks failed with "pose not sampled" at 99.5 FPS,
  on the first run in a fresh worktree (shader compiles). Run 2 was **177/178**, 168.4 FPS with 4 bots; the only
  failure is the known `Hands never pass through guns`. Keel's shell doesn't touch those Crucible checks.
- **In Keel:** 36 wall bays, 12 vent bays, 72 ribs, 913 deck plates, 82 edge strips, 22 arched door cowls.
- **Exposure:** EV100 1.5 (the first guess) and -1 both read near black, with only the emissives showing. **-3 is now
  the default**: dark and moody, but the room's structure reads (`ingame/Keel_exposure_*`).
- **Distance fields:** keeping the shell in Lumen's distance-field scene cost 105.6 vs 136.5 fixed-view FPS with no
  visible change in any shot, so it is **off by default** now; `-SBKeelShellDF` puts it back.
- **FPS (fixed view, 1280x720):**

  | Run | FPS |
  | --- | --- |
  | Shell, everything new (brain tanks, deep-space windows) | 113.0 |
  | Old look (`-SBNoKeelShell`) | 129.1 |
  | Shell without tanks and with the flat panorama | 113.1 |
  | Flat panorama only | 96.0 |
  | No tanks only | 109.7 |

  Same-build runs moved by about ±10% through the session (the old look measured 176.2 earlier the same evening), so
  the shell costs roughly 10-15% against the old look, and the tanks and deep-space windows cost nothing measurable.
- **What the fixed cameras show:**
  - The arched door cowls read well.
  - Ribs stand either side of the doors; columns next to doors keep the old panels, as designed (90 cm rule).
  - The bays are visible at grazing angles in Spawn, Door and Galleries.
- **Not covered by the fixed cameras:**
  - No shot frames a corner where two curved bays meet.
  - The clean shots have no players, so whether feet sink into the 55 cm kick skirt is unchecked.
  - Both need a free-roam look.
- Shots: `docs/playtests/2026-11-25_keel_shell/ingame/` (`Keel_shell_*` vs `Keel_old_*`).

## Brain tanks (2026-11-26)

Vincent asked for the teal circuit-glass panel (white-green branching lines in a rounded frame next to a maroon panel)
to become an alien brain behind glass, mid-surgery. That panel is `M_SB_KeelCircuit` (teal), not the small purple
`M_SB_KeelGlyph` plaques. It still appeared with the shell (on columns that don't take a bay). Every teal one is now a
tank (22 in Keel); the amber circuit panels and everything else on the wall are unchanged.

- `Art/blender/build_keel_brain_tank.py` builds the tank: rounded housing with a glass seat, ribbed back plate with a
  soft light ring, top light bar, retractor clamps, five probes with glowing collars, cables and feed tubes.
- The brain is sculpted procedurally:
  - two hemispheres, cerebellum and brain stem;
  - gyri and sulci from the zero crossings of a domain-warped noise field, a lateral fissure, cerebellar folia;
  - tissue colour (pale gyri, dark wet sulci, veins) baked into vertex colours.
- `SM_SB_Kit_KeelBrainTank` (197k tris, Nanite) holds the opaque parts. `SM_SB_Kit_KeelBrainTankGlass` (1.2k tris)
  holds the bulged front glass and three fluid-haze cards, since Nanite doesn't draw translucency.
- Preview: `docs/playtests/2026-11-25_keel_shell/keel_brain_tank_preview.jpg` (Cycles, glass hidden).
- `Tools\ImportKeelBrainTank.bat` imports both meshes, then `Tools/create_keel_brain_tank.py` makes the materials:
  - `M_SB_KeelBrain`: clear coat for wet tissue, vertex colour base, wetter sulci.
  - `M_SB_TankGlass`: translucent, per-pixel lit, refraction by IOR, Fresnel opacity, teal edge tint.
  - `M_SB_TankFluid`: unlit haze panning upward, depth-faded where it cuts the brain.
- Each tank has a 0.5 cd unshadowed light under its top bar (`-SBBrainTankLight=<cd>`); `-SBNoKeelBrainTank` keeps
  the teal panels.
- **Tuned at EV -3:**
  - The first pass (6 cd light, glow 5, haze 0.16) blew the brain out to white.
  - Now: light 0.5 cd, glow 1.5, haze density 0.07, darker tint, glass roughness 0.1.
- **Still to look at in play:**
  - Up close the brain is still on the bright side.
  - Tanks on small panels (scale 0.5) read as small boxes from across a room.
- Shots: `ingame/Keel_tank_console_before/after.jpg`, `ingame/Keel_tank_galleries_before/after.jpg`.

## Deep-space windows (2026-11-26)

The windows were a flat emissive panorama on solid hull blocks, so real geometry behind them would be hidden by the
wall. `M_SB_KeelDeepSpace` (`Tools/create_keel_deep_space.py`) ray-casts a small 3D scene per pixel instead, in each
window's own frame. It therefore has true perspective and parallax as the player moves:
- stars at infinity in two densities, plus glints 120 m out;
- nebula 3 km out, and wisps 600 m out;
- the gas giant: a real sphere 4 km out, lit from the left with a soft terminator and limb darkening, slowly turning
  cloud bands, an atmosphere rim on the lit limb and a halo outside it;
- a cratered moon;
- drifting hull debris and two ship silhouettes 90 m and 250 m out.

Textures come from `Art/textures/make_keel_deep_space.py` (original, procedural). A glass pane 3 cm in front
(`MI_SB_KeelWindowGlass`) adds soft reflections and a teal edge tint. Tools: `Tools\MakeKeelDeepSpace.bat`;
`-SBKeelFlatSpace` keeps the old panorama.

- **Two fixes on the way:**
  - The first in-game pass drew a flat oversized disc: the panel's face normal points out of the room, which flipped
    the frame. The frame is now oriented from the camera side and the ray re-normalised after clamping.
  - Lamps reflected as large pale discs in the mirror-smooth glass. The glass is now roughness 0.16, specular 0.35.
- The scene sits in each window's frame, so some windows show the moon and nebula but not the planet.
- Shots: `ingame/Keel_window_bridge_before/after.jpg`, `ingame/Keel_window_overwatch_before/after.jpg`.

## In Keel (code written 2026-11-25, built and run on the desktop 2026-11-25)

`AShieldbreakArena`, Keel's alien look only, visual only (no collision). It switches on by itself once
`Tools\ImportKeelShell.bat` has imported the meshes; until then Keel looks exactly as before. **`-SBNoKeelShell`**
gives the previous look, geometry and lighting both, for side-by-side comparisons; `Tools\KeelShellShots.bat` takes
matched shots of both (`Keel_shell_*` and `Keel_old_*`, HUD off, hero cameras included).

1. **Wall bays** (`BuildKeelPackKit`). A face column (about 400 cm) takes a `WallBay` when the wall is solid from the
   floor in front up to the ceiling across the bay and 90 cm past both ends (so door surrounds never meet a bay), the
   floor in front is one level, the space the skirt, wall and 3.6 m haunch fill is open, and it is not one of the
   window walls. About one bay in five is a `WallBayVent`. Bays stretch up to the room's height (Keel's rooms are
   8 to 15 m, the kit is authored at 7.6 m), so the haunch still meets the ceiling. A `Rib` stands on every joint and
   closes both ends of each run. Columns that don't qualify keep the earlier panel dressing.
2. **Collision rule, changed:** the bays' wall line is the block face (`-SBKeelShellSetback=<cm>` moves it back).
   Setting it 55 cm back, as planned, would have hidden the crease light and the lower wall inside the block for the
   first 2 to 3 m, so the block face would show there. At 0 the skirt reaches 55 cm into the room at the floor only,
   where a player's feet can overlap it; nothing a player can see at eye height is affected.
3. **Decks and edges:** `FloorPlate` on every walkable top (instead of the alien deck tiles), `EdgeStrip` on every
   drop of a metre or more (instead of the ramp-edge fascia).
4. **Doors** (`BuildMapArt`): the round-arched `Portal` on both faces of the twelve deck doors. Its arch covers the
   door's top corners, which stay open for shots and movement. The two 20 m trench mouths keep their old portals.
5. **Retired where the shell is:** the panel mix, glyph plaques, circuit glass, light seams, skirting slabs, vault
   plates and coves on bay columns; ceiling ribs and coffers where a haunch curves into the ceiling; the Fab pack's
   wall props on the band walls (trays, lamp strips, cables, boxes, panels), which would sit buried inside the bays.
6. **Lighting** (`SpawnLighting`): sky light 1.5 to 0.5, and exposure locked (min = max = EV100 -3, bias 0) instead
   of auto exposure with a -1.1 bias, so a dark room is no longer lifted back to mid grey. **-3 was tuned on the
   desktop** (the first guess, 1.5, was near black). Tune it with `-SBKeelShellEV=<EV100>` (higher is darker), `-SBSky=<x>`, or `-SBKeelShellAutoExposure` to keep auto
   exposure with the shell. `-SBExposure=` still adds a bias.
7. The kit's meshes keep their own palette instances (`MI_SB_Shell*`, no `ApplyKitSurfaces`) and take part in Lumen's
   distance-field scene only with `-SBKeelShellDF` (off by default since the desktop test: about 30 FPS for no
   visible change).

Not used yet: `WallBayWindow` (Keel's window walls keep their views), `CeilingSpine` and `RibBeam` (Keel's rooms are
halls, not 11 m corridors), `FloorGrate`. `Pillar` and `Pylon` turned out not to fit Keel's columns and reactor
pylons: they are rectangular blocks, and a round piece big enough to hide one at its waist would be about 6 m wide
at the foot. Those keep their own sculpted pieces; new versions authored at each block's size would be the fix.

To check first on the desktop: corners where two bays meet at right angles (their haunches cross), the bays next
to the doors, whether vents read, and the exposure value.

## In-engine lighting, material and post changes

Exposure and sky light are in the code above (first guesses); the rest are not made yet and need the desktop to
tune and measure.

- **Lock exposure on Keel.** Set min and max auto-exposure brightness equal (manual exposure), tuned so a room with no
  lamp in it reads near black. This one change is most of the difference between "dark and moody" and "flat".
- **Sky light down** from 1.5 to about 0.3 on Keel, so emissives and a few real lights carry the room.
- **Glossy master material** `M_SB_KeelShell`: metallic about 0.9, roughness 0.12 to 0.3 broken up by a large
  world-space mottle and a fine noise, base colour between two purples by the same mottle, and a Fresnel tint toward
  blue-magenta at grazing angles to stand in for the reference's iridescence. Lumen reflections on; try hardware ray
  traced Lumen on the 2080 Ti for sharper reflections and measure the cost.
- **Lights to reflect:** rect lights in the ceiling troughs (shadowed), small point lights at the lamp pucks (they make
  the highlights travel along the curves), and a warm rect or directional light through each space window for a
  warm/cool split.
- **Fog:** keep the thin height fog; add local fog volumes at the windows for light shafts instead of global
  volumetric fog (that cost about 3% before).
- **Post:** bloom about 0.7 with threshold about 1, contrast 1.1, saturation back near 1.0 (the current 1.35 alien
  grade pushes everything to the same purple), vignette as now.
