# Handoff (2026-11-04)

## Movement, characters and guns (Claude, branch `claude/project-thread-ndksis`)

**Latest task (3): verification complete with issues.** `9716ade` + `71047da` build/import pass. Codex fixed a narrow character reimport bug: retained slot display names rebound old Gunmetal instead of the new ArmorMetal/WeaponMetal. Correct bindings now verified; shared map Gunmetal unchanged. Gameplay **142 pass / 1 fail** (camera-easing ratio 5.95/7.93 cm); separate 60 FPS presentation **42/42** does not erase it. FP **7/7**. Measured sprint **1020**, slide **1293.60 at 0.15 s / 1014.16 at 0.45 s**. Previous/current FPS: four bots **130.5/128.5**, fixed FP **146.4/149.9** (single observations). Scope white disc fixed; armor still near-black and gloves still white. See `after3/`, exact reports and the material binding JSON in the review directory. No files reserved; Claude should review camera sampling and the remaining lighting/tint issues before another numbered task. No automatic merge to main.

**Latest desktop follow-up to `eba0b73`: gameplay 142/142 PASS; FP 7/7 PASS.** Claude's buffered-jump and curb-fixture fixes now pass. Build and 19-mesh/three-LOD import pass. Previous/follow-up observations at actual 1280x720: four-bot Crucible **125.4 / 130.5 FPS**; fixed FP Keel **142.6 / 146.4 FPS**. Single script observations, not a controlled benchmark. See `after2/` captures and `reports/after2-*.txt` in the review directory. Manny's two slots, parent chains and all scalar/vector defaults are in `reports/mannequin_material_params.txt`; Claude can use these for glove tinting. Armor still looks dark in the bot captures; pale scope cap and white gloves remain. No Codex production gameplay changes or files reserved; main was not merged. The first-pass failures below are historical, superseded by this follow-up.

Desktop verification complete, with **two failures for Claude/master to resolve before merging**. Build/import/winding pass; 19 character/weapon meshes have three LODs. Gameplay is **140 pass / 2 fail**: buffered jump never takes off, and the curb fixture expects an unjustified >20 cm single-frame capsule jump despite climbing the full 29.95 cm. Step-camera easing, coyote time and existing animation-continuity checks pass. See the detailed diagnosis in `COORDINATION.md` and [review evidence](playtests/2026-11-04_character_pass/README.md).

Before/after at actual 1280x720: four-bot Crucible **127.8 / 125.4 FPS**; zero-bot fixed first-person Keel **144.0 / 142.6 FPS**. These are individual script observations. Codex fixed the test namespace and LOD tooling, preserved the two failing checks, and made no production movement edits. Spire work is already pushed through `e8f905f`; map reservations released on main in `abfe0fa`. No files reserved by Codex; local Android configuration and ignored scans retained.

Vincent's focus away from the maps: smoothness and physics, the plain character, the guns and the first-person view.
The camera now eases over steps, jumps forgive a late press at a ledge (coyote time) and an early press before
landing (jump buffer), the armor is a layered v2 set with a new helmet, the guns got a detail pass, and weapons and
armor get LODs on import. Claude and Codex split the work (see the top of `COORDINATION.md`); Codex builds, imports
and measures FPS before and after on the desktop.

## Current Spire art increment

The central condenser now defaults to an original detailed three-vane/iris assembly, with bronze conduits,
modeled practical projectors and localized shadows. `-SBLegacySpireCondenser` shows the retained earlier model.
Authoring is `Tools/BuildSpireHero.ps1`; its narrow import is `Tools/ImportSpireHero.bat`. The finalized Spire tour
passed 29/29 at verified 1080p (76.3 average FPS in the four-bot fixture). See `SPIRE_ART_PASS.md` for actual
captures, import scope, clearance and measurement limits. Next: actual red/blue actors and full-roster frame pacing.

The ceiling now has original swept alloy ribs, recessed webs, connecting spines and physical lamp housings.
Author/import/check commands are in `SPIRE_ART_PASS.md`; `-SBNoSpireArchitecture` compares them off.
Clean proof captures are verified at 1920x1080; the raw fixed-view observation was 111.0 FPS with no bots.
The Spire placement tour passed 29/29, including six lift landings. Final chamfer/housing refinements were
checked afterward in the captures. The current ordinary-jump/lift clearance check leaves 70 cm; grenade stacks
and arbitrary knockback are not covered. This remains an art increment, not a finished AAA environment.

Spire now has its own larger cladding bays, continuous pier panels, quiet decks and original physically lit
Alloy/Inset/Deck surfaces. Removed the shared painted grid from these surfaces and most neutral glow outlines.
Its 22 m ceiling previously used lamps with an 18 m radius; Spire's lamps now reach 28 m at 2200 cd, and energy
emission is reduced to keep cyan concentrated on the mechanism. Other maps' lamp defaults are retained.
See `SPIRE_ART_PASS.md` for authoring/import commands and `COORDINATION.md` for exact checks and limitations.
The lower structural silhouette is still boxy; construction/contact details, actual-team readability and a moving
player-view review are next.

## Current desktop integration

Main now combines Claude's map/art redesign and ambience through `48e0789` with the combat-feel pass
`6fed6cf`. Both source branches and the previous local `redesign` branch are retained. Do not restart from
the older main art or overwrite the ignored scan dependencies. The previous desktop tip is also saved as
`backup/redesign-before-integration-20261103`.

Editor build passed. The normal-timing rendered offscreen gameplay rerun passed **136/136**, with 124.2 average
FPS during its four-bot fixture at 1280x720 (not a controlled benchmark). The first hidden-window run was
135/136: the sampled gun-kick peak was only 0.40 degrees, with 16.6 average bot-match FPS. A controlled 60 Hz
presentation run passed 36/36; the normal-timing rerun measured 1.44 degrees and passed without code or threshold
changes. Low-frame-rate presentation still needs human review; the initial observation is not erased by the rerun.

Keel and Spire local joins, the 4v4 regression (20 balanced samples), and delayed shield feedback all passed.
No internet or eight-human validation is implied. All 2,021 tracked content files are present as real files,
with no unresolved LFS pointers. Local scan content remains intentionally ignored, approximately 19.11 GiB.

Vincent's priority is a substantial visual-quality improvement, using Halo Infinite as the benchmark with the
ambition to exceed it. The first section to finish is Spire's base-to-central-condenser view. See
`VISUAL_QUALITY_TARGET.md` for the observed problems and acceptance evidence. No new environment art was
authored during integration; the following older sections are historical and contain superseded verification notes.

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

## Latest cloud pass (2026-11-02): competitive map redesign

- Keel, Bastion, Spire and Creekbed redesigned for competitive 4v4 (branch `claude/project-thread-05ljt0`, PR #7).
  Keel and Spire are fully enclosed. Layout tools, measurements and per-map notes are in `MAPS.md`. Keel and
  Bastion passed the desktop build and bot tours; Spire and Creekbed are next (`Saved\RunR3.bat` pattern).
- First look pass: a palette and atmosphere per map (Keel gunmetal and orange, Bastion snow at dusk, Spire
  silver-white and blue, Creekbed red rock in warm light).
- One self-test check, the gun hand through an interrupted fade (501 cm/s), fails on the desktop and is unrelated
  to maps.
## Combat feel pass (2026-11-03)

- No recoil, Halo-style aim assist (bullet magnetism, slowdown, controller tracking), clearer hit markers for every
  damage source, and a bigger shield pop. See `GAME_FEEL.md`.

## Still open

- The redesigned maps need human playtests (checklist in the redesign doc). The preserved
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
