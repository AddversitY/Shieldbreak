# Spire environment and action animations

## Architectural surface pass (2026-11-03)

The competitive redesign is now the baseline: enclosed 144 x 72 m, six gravity lifts. The four-lift
records below describe the earlier layout. This pass retains the redesigned collision, routes and item anchors.

Spire uses its own cladding recipe: tall piers have continuous vertical panels, broad walls have roughly
6.5 m bays, and decks use 8 m modules. Neutral supports no longer have glowing outlines on both ends.
Team-coloured portals and the central mechanism retain their orientation cues. Keel's service-band recipe
and other maps' cladding remain the existing treatment. `-SBLegacySpirePanels` compares the previous panel
recipe/material bindings and trim; it is a diagnostic, not an exact replay of every earlier palette setting.

`Art/textures/make_spire_surfaces.py` generates an original, deterministic 1024-square packed texture:
colour variation, roughness variation and micro-height. There are no panel seams painted into it. Run
`Tools/MakeSpireSurfaces.bat` to build the Spire-only world-projected master and Alloy/Inset/Deck instances.
The shader uses three triplanar packed-texture samples with filtered surface-gradient bump. Its physical
texture scale is independent of the scaled cladding meshes. Shared world, weapon and armor materials are retained.

Spire's old lamps had an 18 m attenuation radius under a 22 m ceiling, so their direct lighting did not reach
the lower floor. Spire now has a separate 28 m reach and 2200 cd default, with a near-neutral colour. The core
and kit emission are reduced so cyan energy acts as a focal accent rather than the main room light. Keel's
lamp defaults are unchanged; existing lamp comparison arguments still work.

This is a surface and lighting foundation, not the completed architectural section. See
`VISUAL_QUALITY_TARGET.md` for the larger-form, material, player-motion and performance acceptance criteria.
Actual desktop results are recorded in `COORDINATION.md` after verification.

## Ceiling framework (2026-11-03)

`Art/build_spire_architecture.py` authors four original closed-solid modules without external models:

| Module | Triangles | Placement |
|---|---:|---|
| Alloy rib cheeks and mounting shoe | 116 | Ten swept transverse ribs, starting at 19.3 m |
| Recessed mechanical web | 128 | One between each rib's alloy cheeks |
| Longitudinal spine | 44 | Eight sections connecting the ribs |
| Recessed lamp housing | 112 | Around the existing eighteen ceiling light cards |

Broad faces have geometric chamfers so they catch light; the helper now triangulates concave profiles safely.
`Tools/check_spire_architecture.py` checks analytic concave/chamfer volumes, manifold edges, exported winding,
ordinary jump/lift head clearance and continued generation of the earlier three modules. The current conservative
clearance is 70 cm (rib underside 1930 cm; head envelope 1860 cm). This does not establish safety for arbitrary
knockback or grenade-assisted stacks; those remain human playtests. No collision or nav geometry is added.

Run the authoring script, then `Tools/ImportSpireArchitecture.bat`. The importer checks the exported manifest's
axes, bounds and triangle counts. `-SBNoSpireArchitecture` disables ribs, webs, spines and lamp housings for review.
`-SBCleanShots` hides the HUD only in the opt-in screenshot modes; ordinary play keeps its HUD.

Final [spawn](playtests/2026-11-03_spire-architecture-spawn.png) and
[central](playtests/2026-11-03_spire-architecture-central.png) views are verified 1920x1080, 100% screen percentage.
Use `-ForceRes` for requested resolutions larger than the window work area; without it Unreal clamped the first
1080p request to 888x500. That smaller run is excluded from the 1080p observation. Final fixed-view observation:
111.0 FPS over 19.2 seconds with zero bots; no combat/performance benchmark claim. The Spire placement tour passed
29/29 before the final edge chamfers/lamp housings; final rebuilt assets were then checked in the clean captures.

## Earlier pass (2026-10-27)

The desktop synced merged main through `174ffd3` before this pass. Claude's
first-person arms, native body animation, original weapons/armor, world
materials, mountains, trim and action-animation integration are retained.
The fetched `origin/claude/*` branches had no commits outside main at this check.
Local-only or unpushed work on another machine cannot be inspected here.

## New Spire modules

`Art/build_spire_modules.py` generates three original, low-poly hard-surface
meshes in centimetres. No external model pack or generation account is required.

| Module | Geometry | Placement |
|---|---|---|
| Collector fin | Tapered silhouette, chamfered base and four ribs; 160 triangles | Ten perimeter collectors, outside the walls |
| Service panel | Chamfered housing and louvres; 224 triangles | Ten panels at upper wall height |
| Lift surround | Twelve separated ring sectors; 144 triangles | Four floor rings around existing lift pads |

Meshes import into `/Game/Shieldbreak/Art/Spire` using
`Tools/ImportSpireArt.bat`. The import helper checks dimensions and floor
pivots. It enables instanced-mesh use on the existing world/surface materials.
The runtime uses three instanced groups with collision, navigation influence
and shadows disabled. This is a first architectural detail pass, not a completed
environment. A subsequent horizontal size pass expands all launch maps,
including Spire, to twice their original length and width. Decorative placement
follows the new footprint; the modules do not add collision. Further routes are directed
through `Design/map-planner/index.html` or a dimensioned paper sketch.

## Existing action integration

Reload, melee and grenade throwing already use imported non-additive Lyra
action sequences through `UShieldbreakBodyAnimator` and the native body anim
instance. They play on the owner's first-person arms and third-person body.
Reload playback fits the weapon's reload time; pistol-specific reload/melee
sequences are selected where available; throwing uses the rifle toss sequence.
Local input is predicted cosmetically and other machines receive action events.
Gameplay damage, ammunition and grenade authority remain server controlled.

The existing `-SBFPShots` mode captures idle, swap, grenade, melee and reload
frames. The gameplay suite checks the melee swing and blended body animation.
New desktop results are recorded in `COORDINATION.md`; earlier laptop records
are historical and are not a substitute for checking the merged desktop build.

Desktop captures: `playtests/2026-10-27_expanded-spire.png`, plus the matching
`spire-grenade`, `spire-melee` and `spire-reload` files. The existing action
sequences are visible and functional; these are not finished bespoke FPS
animations. Some poses/upper-arm armor occlude much of the first-person view.
The user's next requested pass addresses movement/animation smoothness and
visible shield hit reactions; it is tracked separately from this map expansion.
