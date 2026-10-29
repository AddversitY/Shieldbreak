# Handoff (2026-10-27)

Pickup note for the unified branch. The log of who verified what is `docs/COORDINATION.md`.
Design is `docs/DESIGN.md`. Code map is `docs/ARCHITECTURE.md`. The menu is `docs/MAIN_MENU.md`.

## Where the code is

- Repo `AddversitY/Shieldbreak`. PR #4 merged the integration of main and PR #2
  (`claude/project-thread-iqwexu`) into **main** at `174ffd3`.
- UE 5.8, C++. The front end is Slate. The HUD is Canvas. Enhanced Input actions are built in code.
- Desktop build machine: `C:\Users\addve\Documents\ChatGPT\Shieldbreak`, UE 5.8.4 at
  `C:\Users\addve\Epic Games\UE_5.8`, RTX 2080 Ti. The batch files look for
  `%USERPROFILE%\Epic Games\UE_5.8`. The laptop path and 8 GB notes are historical.
- The earlier cloud unification was not compiled there. The desktop subsequently
  fixed a unity-build namespace collision in `SShieldbreakMenus.cpp` and built it.
  Current runtime results are in `COORDINATION.md`.

## What this branch kept

- From main: the concise menu (Play, then Multiplayer / Host / Practice), 4v4 session rules,
  host-only match commands, arena movement-base names, and connection-failure recovery
  (`UShieldbreakGameInstance` in `Game/`).
- From PR #2: settings, key rebinding, medals, menu music, original weapon/armor/world art,
  the cracked-shield emblem, packaging (`Tools/Package.bat`), the body anim instance, and the
  lighting fix (Lumen and virtual shadow maps off, plus the arena fill light).
- One `SShieldbreakMainMenu`. Settings and Controls open on top of it. The separate menu game
  mode and the second game instance were not kept.

## What is not in the game

Shield Shaping, the SMG, and the Plasma Rifle. Do not describe them as current weapons or mechanics.

## What to run on the desktop

See the pull request for the exact order. Short version: `git lfs pull`, then `Tools\SelfTest.bat`,
then the arena, 4v4 and connection-recovery scripts one at a time, then look at Settings, Controls,
and a shadowed wall.

## Latest desktop pass

- Launch maps now have four times their original ground area (2x X/Y), with
  original heights and movement tuning. Spire has original modular detail.
- Claude's action rig now has continuous directional blending, gait continuity,
  action cross-fades, smoother crouch-camera presentation and bounded network
  visual corrections. Shields visibly flash on absorbed hits and breaks.
- Actual tests and limits are in `COORDINATION.md`; implementation and test
  commands are in `SMOOTHING_AND_SHIELD_FEEDBACK.md`. Complete AAA presentation,
  FPS-specific animation authoring, foot IK and more environment art remain ahead.

## Latest cloud pass (2026-10-29)

- Cover pass on the five enlarged maps: low cover, lane-splitting walls and base-top cover, placed by
  `fill_<map>()` in `Design/maps/shieldbreak_maps.py` and checked by its validator (exposed floor now 10-18%,
  down from 31-48% after the 2× stretch). Needs a desktop build, self-test and map tours. See `MAPS.md`.

## Still open

- All five launch maps have a first expansion to 2x X/Y (4x ground area), keeping
  original heights. Further route/cover/pacing iteration is open. The preserved
  overhead sketch tool is `Design/map-planner/index.html`.
- Spire has original collector, service-panel and lift-surround modules; see
  `SPIRE_ART_PASS.md`. Claude's existing reload, melee and grenade animations are
  retained, not reimplemented. Complete environment artwork remains ahead.
- `Tools/PushContent.bat` still names a historical Claude branch. For this desktop
  pass, commit on main and use `git pull --rebase` before `git push origin main`;
  do not use that legacy helper until its branch handling is updated.

- Lyra GAS blueprints and `FootstepEffectTagModifier` still log load errors. They were not deleted:
  the asset bytes are Git LFS pointers here, and the packaging notes say animations still reference
  the modifier.
- No matchmaking, NAT, dedicated server, or eight-human test has been run.
- Map environment art beyond the code-built layouts and the original trim/materials is still ahead
  (`docs/HANDOFF.md` used to point at Meshy; that account is still Vincent's decision, not started here).
