# Visual quality target and first review

Vincent's direction, 2026-11-03: the current graphics are underwhelming and unflattering. Halo Infinite is the visual benchmark, with the ambition to exceed it while developing Shieldbreak's own identity. This is a target, not a statement of achieved quality.

## First map: Spire

Review sources: Claude's existing November 3 desktop capture `Saved/Screenshots/Shots/spire_r54_3_spawn.png` and fresh combined-build captures [spawn](playtests/2026-11-03_spire-baseline-spawn.png) and [central room](playtests/2026-11-03_spire-baseline-central.png), at 1280 x 720. The fresh views confirm the observations below; no new art is claimed. These are automated fixed-camera captures with the fixture's respawn HUD, not normal player-view interaction checks. The current redesigned layout is enclosed, 144 x 72 m, with six lifts. The older four-lift sketch tool does not describe this layout.

Observed problems in the spawn view:

- Panel seams, alternating dark bands and cyan accents repeat at similar intervals on floors, walls and pillars. The repetition competes with the room's larger architectural forms.
- The foreground, pillars and distant walls have similar surface treatment and brightness. The central energy landmark has too little separation from ordinary wall detail.
- Block-shaped supports and flat wall cladding remain apparent. More material detail alone will not produce a finished environment.
- Most visible surfaces look clean and uniformly manufactured. Material response and wear need an authored relationship to how the structure is built and used.

## Art direction for the first finished section

Use the base-to-central-column view as the first section to finish. Establish broad, restrained structural surfaces, a distinct support silhouette, and a central condenser with a strong visual identity. Keep cyan energy concentrated around the condenser and lift machinery; use more restrained illumination for circulation routes. Team identity must remain readable without tinting the entire room.

Author geometry and materials together: structural joints, inset panels and edge bevels should have consistent physical scale; brushed or worn metal, dark recesses and energy glass should respond differently to light. Avoid adding arbitrary scratches or extra glowing seams across every surface.

Lighting should reveal the architecture and guide attention while leaving enemies readable on both teams. Check bright areas, shaded lanes, exposure transitions and weapon visibility from player height. Retain the routes, six lift landings and collision while improving presentation. Any later geometry that changes play requires a separate map review.

## Acceptance evidence

1. Capture matched before/after views at the spawn, central bridge and a shaded flank, with the same resolution, camera, exposure settings and settled shaders.
2. Review the section while moving, reloading, meleeing and throwing a grenade; a still image alone cannot establish presentation quality.
3. Check team silhouettes and shield feedback in both bright and dark areas. Preserve competitive readability and sightlines.
4. Compare frame time and visible stutter on the RTX 2080 Ti with the same scene, resolution and settings. Report the actual settings and workload; screenshot-only FPS is not combat performance.
5. Obtain Vincent's visual feedback on this section before extending the treatment to all five maps.

## First surface and lighting increment

Matched updated views: [spawn](playtests/2026-11-03_spire-surfaces-spawn.png),
[central room](playtests/2026-11-03_spire-surfaces-central.png). Spire now uses larger structural bays,
quieter decks and its own original alloy/inset/deck materials. Most neutral glow outlines are removed.
The ceiling lamps now reach the lower floor, and the energy surfaces no longer provide the main room wash.
The excessive grain seen in the first material experiment was reduced before committing this version.

The views confirm less pattern repetition and a more neutral room palette; they also expose the remaining
box-shaped supports and flat ceiling. Those shapes still need architectural work. No Halo Infinite parity
or completed visual section is claimed. Runtime checks and capture limitations are recorded in `COORDINATION.md`.

## Ceiling framework increment

The [updated spawn view](playtests/2026-11-03_spire-architecture-spawn.png) and
[central view](playtests/2026-11-03_spire-architecture-central.png) show the original swept ceiling ribs,
mechanical webs, spines and recessed light housings. Capture-only HUD suppression keeps these views clean.
Their actual dimensions are 1920x1080, checked from both PNG data and the engine log. The first requested-1080
capture was clamped to 888x500 and is not used as 1080p performance evidence.

The framework adds depth above the routes. The lower supports still need more convincing construction and
contact details, and the central condenser needs a stronger original silhouette. Existing first-person gun/hand
finish and posing remain separate visual weaknesses found in the action captures. See `SPIRE_ART_PASS.md` for
mesh authoring, clearance scope and exact verification limits.

## Original induction condenser

The new [bridge detail](playtests/2026-11-03_spire-condenser-bridge.png) and
[ground-floor sightline](playtests/2026-11-03_spire-condenser-floor.png) show the original three-vane condenser,
iris and curved bronze conduits. The [earlier model](playtests/2026-11-03_spire-condenser-legacy.png) remains
available for comparison. These are actual 1080p game captures, with capture-only HUD suppression.

The first close-up exposed overly strong projector hotspots; the final version uses a darker hero alloy and
400 cd practicals. The first ground camera was under a bridge and was moved to a clear actual floor sightline.
The new sculpture provides a more specific focal point, while lower structural detail and full-roster/moving-view
checks remain open. The finalized four-bot Spire tour passed 29/29; see `COORDINATION.md` for scope and measurements.

## Asset preservation

The desktop contains 725 ignored content files, approximately 19.11 GiB, including the Electric Dreams scan dependencies. These are intentionally local under the existing import/licensing workflow; do not add them wholesale to GitHub. `Tools/GetScanArt.bat` documents the source/import route. An integration manifest and the pre-integration configuration copy are in ignored `Saved/IntegrationBackup-20261103/`. The manifest inventories paths; it is not a second copy or an off-machine backup of the assets.

The original `redesign` tip is retained at `backup/redesign-before-integration-20261103`. Local configuration edits were stashed separately before integrating Claude's published branches. Existing stashes were retained.
