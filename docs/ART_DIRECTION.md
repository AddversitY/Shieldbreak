# Art Direction and Asset Plan

## Current direction (2026-10-27)

The user supplied a Halo environment collage as a mood reference and selected
**a different mood for each map**, drawing from the **whole collage**, with
**Spire first**. Carry across monumental scale, substantial material construction,
industrial/natural contrasts and readable landmarks while authoring original
architecture and scenery for Shieldbreak.

Map scale is being redesigned first: the user says the existing maps feel too
small and wants roughly 2–4x size. The first playable pass now doubles each horizontal axis, giving four times
the ground area. Use the overhead sketch planner and `MAP_SCALE_DIRECTION.md`
to direct further route and footprint changes. Runtime maps retain their
original heights and use the synced world-material/trim pass.

Proposed Spire art starting point: a storm-harvesting exchange above a broken
mountain caldera, with layered structural fins, grounded service machinery and
clear upper/lower routes. This theme and its palette are creative proposals,
not a final environment design. No complete environment, foliage or terrain kit is
currently imported; the recorded Lyra assets mostly cover characters, weapons,
audio and effects. Claude's original armor, weapons, materials and mountain ring
are now synced on the desktop. A first set of original Spire collector fins,
service panels and lift surrounds extends that work; see `SPIRE_ART_PASS.md`.
Detailed bridge approaches and lower-route redesign still follow the user's
chosen footprint and route direction.

**Target:** the *feel* of Halo 2 with Halo Infinite-level polish, built from **original designs**.
Mechanics and genre conventions are free to use. Halo's specific characters, weapons, maps, logos,
UI layouts and names are not: a "slightly modified" copy is still a derivative work, and Halo's look is
also protected as a trademark. Every asset in this project must be original or properly licensed.

The subsequent presentation pass extends Claude's animation rig with directional
blending and visible shield-hit overlays; see `SMOOTHING_AND_SHIELD_FEEDBACK.md`.

## Style guide (what to aim for)

| Element | Halo-2 feel we want | Our original take |
|---|---|---|
| Soldiers | Bulky powered armor, readable from 50 m, mirrored visor | Our own armor silhouette: angular shoulder plates, one-piece faceted visor (amber/teal, not gold), armor paint in bold team colors |
| Weapons | Chunky, low-poly-readable, iconic silhouettes | Our own shapes: the battle rifle is a long bullpup with a top rail; the sidearm, sniper, rocket launcher and rail lance are separate silhouettes. The SMG and plasma rifle are not in the game |
| Architecture | Monumental alien structures, silver-grey metal, glowing seams | "Ancient builder" ruins: stacked slab geometry, chamfered edges, warm stone plus cool metal, cyan light strips |
| Lighting | Bright, saturated sky, strong sun, readable shadow | Clear skies and a shadowless fill light. Lumen and virtual shadow maps are off: with them on, shadowed walls went near-black |
| Shields | Flicker and flare on hit, a pop when broken | Hex-pattern energy shell that flares toward the hit direction, with a shatter burst when broken |
| HUD | Minimal: shield bar on top, radar bottom-left, ammo top-right | Same information layout (a genre convention), drawn with our own shapes, fonts and color palette |

Do **not** use: Master Chief/Spartan helmet shapes, the MJOLNIR name, Halo weapon names, Covenant designs,
Halo map layouts (Lockout, Midship, Ascension…), Halo logos or fonts, or ripped game assets.

## Phase 1: free Epic assets

All of these are free and licensed for use in Unreal Engine projects.

1. **Lyra Starter Game** (Epic Games Launcher → Unreal Engine → Samples, or Fab). Create a Lyra project
   *separately*, then **migrate** (right-click asset → Asset Actions → Migrate…) into `Shieldbreak/Content/`:
   - Characters: the Manny/Quinn skeletal meshes and the base locomotion animations (not Lyra's
     animation blueprints, which depend on Lyra C++).
   - Weapons: the rifle, pistol and shotgun skeletal meshes, plus their fire/reload animations.
   - VFX: Niagara muzzle flashes, tracers and impacts.
   - Audio: weapon fire, impact and UI sounds.
2. **Fab free sci-fi environment packs** (Fab's monthly free content and permanently free packs):
   modular sci-fi or ruin kits for arena dressing.
3. **Quixel Megascans** (free with UE via Fab): rock, cliff and ground surfaces for outdoor arenas.

Put migrated content under `Content/Shieldbreak/Imported/<Source>/…` so the origin of every asset is
obvious, and record each pack and its license in `docs/ASSET_CREDITS.md`.

## Phase 2: code hookup (cloud session)

Once the assets are in the repo (via Git LFS, already configured), the C++ is updated to use them:
- Character: first-person arms mesh and third-person body skeletal mesh with a native anim
  instance (locomotion blend driven from the movement component), replacing the cylinder/sphere greybox.
- Weapons: skeletal weapon meshes per class, muzzle sockets for tracers, Niagara muzzle flash and impacts.
- Shields: shield-flare and shatter Niagara or material effects driven by `UShieldHealthComponent` events.
- Arena: a modular-kit layout replacing the cube builder, with gameplay (spawns, waypoints) kept in code.
- Audio: imported sounds replace the synthesized ones where they are better; synthesis stays as a fallback.

The cloud session needs to know the **exact content paths** of imported assets (e.g.
`/Game/Shieldbreak/Imported/Lyra/Characters/Mannequins/Meshes/SKM_Manny`). After migrating, list them in
`docs/ASSET_CREDITS.md`.

## Phase 3: original hero art

Replace the generic pieces with our own designs: soldier armor, BR-class rifle, plasma rifle, an
architecture kit. Use concept sheets, then models by an artist or an AI 3D generator, then import. The code
from Phase 2 keeps working because only the asset paths change.
