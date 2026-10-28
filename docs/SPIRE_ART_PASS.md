# Spire environment and action animations

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
