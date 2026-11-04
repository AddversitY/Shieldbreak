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

## Asset preservation

The desktop contains 725 ignored content files, approximately 19.11 GiB, including the Electric Dreams scan dependencies. These are intentionally local under the existing import/licensing workflow; do not add them wholesale to GitHub. `Tools/GetScanArt.bat` documents the source/import route. An integration manifest and the pre-integration configuration copy are in ignored `Saved/IntegrationBackup-20261103/`. The manifest inventories paths; it is not a second copy or an off-machine backup of the assets.

The original `redesign` tip is retained at `backup/redesign-before-integration-20261103`. Local configuration edits were stashed separately before integrating Claude's published branches. Existing stashes were retained.
