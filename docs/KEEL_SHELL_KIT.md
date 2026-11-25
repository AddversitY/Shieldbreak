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

Not tested in Unreal yet: the import, the Nanite step and any in-game look. Nothing in the game uses these meshes yet.

## Putting it in Keel (next pass, on the desktop)

Code in `AShieldbreakArena`, Keel only, behind a flag like the earlier passes (`-SBNoKeelShell` for the old look):

1. Along each long wall run, place `WallBay` every 400 cm on the block face, a `Rib` on every joint, and mix in
   `WallBayVent` about one bay in five and `WallBayWindow` where Keel already has windows.
2. Over each corridor, a `CeilingSpine` and `RibBeam` per bay, scaled in X to fill the width between the two haunches
   (width - 724 cm). Rooms of another height get a re-run of the script with that height, not a Z scale.
3. **Collision rule:** the skirt reaches 55 cm into the room at the floor. Set the shell's wall line 55 cm behind the
   block face, so a player's body never sinks into the skirt. The lean then only crosses the collision face above
   about 3 m, over head height.
4. Floors: `FloorPlate` on walkable tops, `FloorGrate` down main walkways, `EdgeStrip` on every drop and ramp side.
5. Set pieces: `Pylon` rings in the Galleries in place of the obelisks, `Pillar` shrouds on the ten columns,
   `Portal` round the twelve deck doors.
6. Retire what fights the new look: glyph plaques, circuit-glass panels, the mauve panel mix and the coffered ceiling
   where the shell covers them.

## In-engine lighting, material and post changes

None of these are made on this branch; all need the desktop to tune and measure.

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
