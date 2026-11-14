# Trooper character: Epic files are local only (2026-11-14)

The third-person trooper (`sb.Trooper`, `UShieldbreakTrooperAnimInstance`, `/Game/Characters/Trooper/IK_Trooper` and
`RTG_Trooper`) uses Epic's free Fab character **Paragon: Wraith**, Default skin. Vincent chose to keep Epic's files out
of GitHub. `Content/ParagonWraith/` is in `.gitignore` and exists only on the desktop PC.

**Without those files:** every soldier falls back to the previous skinned suit (`SK_SB_Soldier`). There's no crash and
no broken reference in game; this was checked on the desktop by moving the folder out, 2026-11-14. `IK_Trooper` and
`RTG_Trooper` load only when the trooper mesh exists. Opening them in the editor without the files shows a missing-mesh
warning.

**To restore on another machine:**
1. Add **Paragon: Wraith** (Epic Games, free) from Fab to this project, using the Epic Games Launcher's Fab library >
   Add to Project, or the editor's Fab plugin. It installs to `Content/ParagonWraith/` at the paths the code expects.
2. Only the Default skin's mesh, skeleton, physics asset, materials and textures are used. The rest of the pack
   (animations, FX, sounds, other skins) can be deleted locally.

# Handoff: rifle reload reference pass; visual review pending (2026-11-11)

Branch `handoff-2026-11-10`, starting at `ee4c366`. Vincent rejected that clip and asked to mimic the realism of [this reference](https://www.youtube.com/watch?v=XI90HrSe3pc). New authored keys add a side-grip wrist, per-joint finger closure/release, old-magazine release and fresh pickup, a seating impulse and forend recovery. The former cross-receiver sweep is gone. Receiver and magazine contact fitting protects the thumb. Magazine events consume once; cancellation blending preserves the refreshed hand frame. Gameplay duration/ammunition and imported art are unchanged.

**Technical validation complete; visual acceptance pending.** Build PASS. Final fixed-30-fps moves **15/15**, wrist turn **65.4 degrees**, middle bend **50.4 degrees**, new-grasp drift **0.000 cm**. Full gameplay **176/177**: the reload and final five-weapon / BR reload / TP BR clipping check pass. The sole failure is the documented camera-step fixture: eye **5.35 cm**, curb **29.99 cm**, trail **8.80 cm**, below its existing `>10 cm` trail requirement. No camera/threshold changes. Cross-weapon FP captures **9/9**. An earlier receiver-aware candidate separately passed clipping **3/3**; the final full suite repeated that check after the final fresh-grasp correction.

Configured matching Keel fixed-view fixture, 1280x720 rendered offscreen, no bots: **108.8 FPS before / 174.3 after**. Background desktop load was uncontrolled; do not attribute the increase to this animation work. Final bot fixture **165.8 FPS**, broader FP capture **177.3** (after only). Earlier failed thumb/hitch observations are retained in the review reports.

[Review videos, comparison and reports](playtests/2026-11-11_reload_realism/README.md). Final clip: actual game frames, 1280x720, 30 fps, 3.33 s, silent. Audio bank/timing was not revised; finer finger contact, other-weapon animation and audio timing remain open. Passing state/bone tests is not Vincent's visual approval. No network/internet/eight-human validation claimed.

No files reserved or engine/build jobs running. Await Vincent's feedback; no automatic pickup or automation setting changes. Local Android settings and `fx1run/` preserved.

# Previous reload handoff (2026-11-11)

Branch `handoff-2026-11-10`, starting at `5548bb6`. Vincent reviewed the reload video and authorized this follow-up. **Completed; no files reserved or build/game jobs running. Wait for his next instruction.** Local Android settings and `fx1run/` are preserved; automation settings were not changed.

The magazine follows the current evaluated palm instead of the prior frame. Hand anchors refresh after the current gun pose; grab captures the actual grasp; seating blends back into the socket. BR framing uses a smaller cant and brings the rifle forward/left/up. The magazine clears the well, moves into view to the viewer's left, then aligns for insertion. No imported art, gameplay reload timing, ammunition or network behavior changed.

**Final validation:** build PASS; full gameplay **173/173**; fixed-30-fps reload fixture **11/11**, with **0.000 cm** carry drift and palm/magazine in view. Sidearm/sniper FP-capture suite **9/9** on the shared carry fix before the final BR-only key refinement. Matching fixed-view Keel, 1280x720 rendered offscreen, no bots: **91.5 FPS before / 93.7 after**. Single desktop observations, not a controlled benchmark; these do not settle the older grip-performance question below. Final four-bot Crucible **97.3 FPS**, after only.

**Keep the camera-step issue open:** intermediate full **172/173** and isolated recheck **4 pass / 1 fail** failed the existing eye-trails-step check (eye **12.66 / 17.36 cm**). Final full passes at **9.23 cm** against a **30.00 cm** curb. Failures are retained; no threshold or movement code was changed. More hand/animation polish and prior texture/sidearm idle-framing issues remain. No online/eight-human or network-suite validation in this cosmetic pass.

Review the [before/after videos, images and exact reports](playtests/2026-11-11_reload/README.md). Final MP4 is 1280x720, 30 fps, 3.33 seconds, no audio; actual rendered game frames. The palm centre is visible; this is not a claim that the entire glove stays in frame in every pose.

# Previous grip handoff (2026-11-10)

Branch `handoff-2026-11-10`, starting at `9c72567`. **Vincent requested finishing this pass and then pausing. Do not pick up the next task until he resumes.** No files reserved or build/game jobs left running by this pass. Local Android settings and `fx1run/` were preserved.

Fixed the merged hand-through-gun failure with shared FP/TP grip placement, reachable support-palm fitting, forward-bending trigger IK and corrected distal-finger contact probes. Palm fitting is cached in gun space; escape candidates use rays while closing predictions retain skin sweeps. No imported art changed and no clipping threshold was relaxed. The reload test now reports untestable geometry as a failure. Also isolated the grenade-light fixture from older lingering explosions; production grenade timing is unchanged.

**Final desktop validation:** editor build succeeded; full gameplay **171 passed / 0 failed** (before **170/171**); first-person capture suite **9 passed / 0 failed**. All five FP weapons, sampled BR reload and TP BR pass the contact check. Earlier focused candidate: **3/3**. Exact reports and before/after JPEGs: [grip review](playtests/2026-11-10_grip/README.md).

**FPS remains inconclusive:** four-bot Crucible **172.8 before / 97.0 after**; fixed-view Keel **153.0 before / 104.3 after**, same machine, 1280x720, rendered offscreen. WoW started at 23:07 local, after both baseline reports, and was consuming CPU alongside final verification (`reports/run-load.txt`). Do not call these controlled performance comparisons or claim restored performance. Intermediate lower observations and the initial grenade-fixture failure are retained. An idle-desktop before/after recheck remains open; the user's game was left running.

**Remaining:** animation/hold polish beyond the sampled poses (sidearm glove now mostly below frame; TP support farther back by the magazine); the texture issues listed below; performance verification under matching load. No network scripts were rerun for this cosmetic/test pass; no online matchmaking, internet hosting or eight-human testing claimed. Desktop UE is installed at `C:\Users\addve\Epic Games\UE_5.8` (5.8.4); builds used `-MaxParallelActions=2`, with expensive jobs sequential.

# Previous consolidated handoff (2026-11-10)

Branch `handoff-2026-11-10` = `origin/main` + every pushed feature branch merged (newer work preferred).
Build: **Succeeded** (`Tools/BuildEditor.bat`). SelfTest on this branch: **RESULT 170 passed, 1 failed**:
`Hands never pass through guns (first person, reload, third person)` (BR 3 joints, Sniper 1, Sidearm 11). That
check comes from the first-person moves work and has not been run against the reshaped guns before. Fix the hand
poses/sockets or the check next. Bot FPS here 111.8 (4 bots, PC idle). It was ~129 on `gun-textures` alone, so
the merge (audio/impact FX/grenade work) may cost frame time. Needs a controlled recheck.

## Which branch holds what
- `gun-textures`: baked gun textures, Vanguard BR v7, Longshot v4 sniper, Kestrel v3 sidearm, polish pass,
  extended FP captures, `-SBNoHelp`. Includes `claude/project-thread-ndksis` (support hand / rifle grip).
- `claude/project-thread-1yjl1q`: first-person moves (M1-M5: reload, melee, pistol magazine).
- `claude/project-thread-yp1u5u`: grenade throw/merge (G1-G11). Its older throw dip was dropped for the newer
  `Pitch` dip from `gun-textures`. Its left-finger cvar is now `CVarThrowCurl` (`sb.FPThrowCurl`).
- `claude/project-thread-z6rvy1`: impact FX. `claude/project-thread-clu4v1`: layered weapon/footstep SFX.
- `claude/project-thread-e45jnz`: Keel rebuilt from the Midship brief, bridge spawn fix, ramp validator.

## Gun pipeline
- Venv: `%USERPROFILE%\sb-bake-venv` (Python 3.13 + bpy). From `Art/blender`:
  `"%USERPROFILE%\sb-bake-venv\Scripts\python.exe" bake_weapons.py SM_SB_BattleRifle --res 2048` (also
  `SM_SB_Sniper`, `SM_SB_Sidearm`; ~3-15 min each, run one at a time on 8 GB).
- Import: `powershell -ExecutionPolicy Bypass -File Tools\ImportArt.ps1 -Folders Weapons`. Exit 1 is the known
  Armor LOD count check on a narrow import. Grep `Saved\Logs\ImportArt.txt` for "Failed to compile".
- Captures: add `-SBNoHelp` to keep the F1 overlay off. `-SBSelfTest -SBFPShots -SBShotTag=<tag>` shoots BR,
  sidearm and sniper idle/reload after the fixed FP FPS figure. 3/4 close-up:
  `python -I Toolsender_weapon_closeup.py <Art\Export> <out.png> [BattleRifle|Sniper|Sidearm] [distance, 3.4 for the sidearm]`.

## Gun status
- Vanguard BR v7 (swept shell, integrated MKS-4X): LOW 14,584 / NoMag 14,096. Forend blotches gone. Markings read correctly.
- Longshot v4 sniper: LOW 15,132 / NoMag 14,768. Slimmer scope, ~10% of the frame at idle and mid-reload (was 15-25%).
- Kestrel v3 sidearm: LOW 3,964 / NoMag 3,832. KESTREL reads correctly. Grip blotches fixed.
- Open: smeared grey/white edge mid-slide on the sidearm; cloudy rear sight insert on the sidearm; blurry
  sniper magazine texture in the reload; no hands in frame at sniper idle.
- Flaky self-tests (seen on and off, not gun related): "The camera never sits inside a character" (bot match)
  and "The eye trails the step".
- FPS on an idle PC with `gun-textures`: ~146 fixed first person, ~129 with 4 bots. Runs while the Claude
  desktop app was busy read ~107-126 and aren't comparable.

# Earlier handoff (2026-11-04)

## Movement, characters and guns (Claude, branch `claude/project-thread-ndksis`)

**Latest automatic task (5), `aad1a24`: gameplay 144/144 PASS, FP 7/7 PASS after a material-usage fix.** Polymer initially lacked SkeletalMesh usage and rendered Default Material; Codex stopped that invalid run, enabled/saved the required usage on `M_SB_Surface` through the importer, then reimported and reran. Final logs have no usage/fallback warnings; all three palette values/bindings verified, Gunmetal instance unchanged. Raw previous/current FPS: bots **132.2/120.6**, FP **145.7/113.4**; lower figures need review, not a controlled benchmark. Normal glove still clips white, but the otherwise matched `sb.FPLight 0` diagnostic gives a dark matte glove. **Next focus: FP fill/exposure**, not more mannequin parameter guessing; production light unchanged. Gun mid-grey goal remains partial; bot suit is matte dark. See `after5/`, its `fill-off/` comparison and exact reports. No jobs/files reserved or main merge.

**Latest automatic task (4), `4d1bfa6`: gameplay 143/143 PASS, FP 7/7 PASS; visual goals still incomplete.** Build and narrow import pass; actual updated ArmorMetal/WeaponMetal values and bindings verified, shared map Gunmetal unchanged. Step assertion passes under Claude's new curb-relative criterion (eye 5.75 cm, curb 29.93, capsule frame 6.66, trail 12.63); no new production movement changes by Codex. Previous/current raw FPS: four bots **128.5/132.2**, fixed FP **149.9/145.7**. `after4/` and exact reports are available in the review directory. Gloves remain white, armor remains very dark, and gun-body grey/detail is partial; scope stays dark. Claude should inspect runtime material/lighting rather than assuming the palette/tint changes visually solved those items. No files reserved/jobs running; no main merge.

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
