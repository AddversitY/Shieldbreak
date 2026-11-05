# Shieldbreak development handoff

Claude and Codex share this handoff. **main** is the integration branch. This branch unifies
main (ChatGPT's menu, 4v4 rules, connection recovery, desktop verification) with PR #2
(`claude/project-thread-iqwexu`: settings, key rebinding, medals, music, original art, emblem,
packaging, and the shadowed-wall lighting fix). It was merged through PR #4 (174ffd3). Do not force-push main or delete either source branch.

Read this before starting work, and update it in the same commit as the work. See `AGENTS.md`
for the user's standing GitHub and 4v4 instructions. `docs/HANDOFF.md` is the short pickup
note; this file is the log.

| Session | Can do | Can't do | Owns |
|---|---|---|---|
| **PC session** (desktop is the build machine; the laptop notes below are historical) | Build with UE 5.8, run the game, self-test, screenshots, editor/Python asset work, Epic launcher / Fab downloads, Git LFS content pushes | This cloud session cannot compile or launch Unreal | `Content/`, `Tools/`, `Source/Shieldbreak/Test/`, `PlayTest.bat`, build/perf config, `docs/ASSET_CREDITS.md` |
| **Code session** (cloud only) | Fast C++ feature work, design docs | Compiling, running, opening assets | Gameplay features: `Game/`, `Player/`, `Weapons/`, `AI/`, `UI/`, `Visuals/`, `Audio/`, `Shield/` |

## Who does what: Claude and Codex (agreed with Vincent, 2026-11-04)
Vincent splits the work between Claude (cloud) and Codex (his desktop) to stretch his usage limits.
- **Claude (cloud):** design, gameplay C++, scripted Blender art (weapons, armor, kits) and the self-test checks
  for its own changes. Works on a `claude/...` branch and opens a PR; Vincent merges.
- **Codex (desktop):** builds, `Tools\SelfTest.bat`, network scripts, map tours, imports (`Tools\ImportArt.bat`,
  `Tools\ImportCharacterArt.bat`), FPS before/after measurements, screenshots, Git LFS content pushes, and narrow,
  fully specified tasks that a "Codex task" entry below spells out (files, exact change, how to check it).
  Codex fixes compile errors in Claude's code directly and records what it changed.
- **Areas:** Claude's map thread owns the maps (`Design/maps/`, `Game/ShieldbreakArena.*`, `Game/ShieldbreakMapArt.*`,
  map kits). Codex reserves map files here before an art pass and the map thread checks this file before editing them.
  Movement, characters, weapons and the first-person view are Claude's current focus (branch
  `claude/project-thread-ndksis`).
- **Performance rule (Vincent):** every change gets an FPS figure before and after, on the same machine, map,
  resolution and bot count. Report both numbers, not one.

## Automatic desktop pickup (enabled 2026-11-04)

Storage cleanup at Vincent's request (2026-11-04 local): freed **6.44 GiB**, about **74 GiB free** immediately afterward. Removed only verified untracked `Intermediate/`, project `DerivedDataCache/`, **1,976 generated screenshot files older than 2026-11-04** under `Saved/Screenshots/`, and the redundant Blender 4.5.15 ZIP after confirming the extracted executable remains. Some historical raw `Saved/Screenshots` references below are now obsolete; committed review images and all current character verification evidence remain. Source, all 2,760 Content files including ignored scans, Git history/stashes, local config/backups, binaries, Unreal and extracted Blender retained. Git LFS fsck PASS; no tracked deletions. Next build regenerates its cache/PCH and may take longer. No build/game test run after cleanup; overnight automation remains PAUSED. Itemized local deletion manifest: `Saved/StorageCleanup-20261104.json`. External ElectricDreams sample (~56 GiB logical) and Windows.old (~47 GiB logical, includes old Users) were measured only and left untouched pending user direction.

**RESUMED (Vincent, 2026-11-05 20:59 UTC: "Resume development of shield break").** Automation can run again; task (6) is READY. The Codex chat's own pause should be lifted too.

Codex automation **Shieldbreak — pick up Claude tasks** (`shieldbreak-pick-up-claude-tasks`) checks this queue every **15 minutes** in the existing desktop chat. Vincent no longer needs to relay each task. The PC must be available and the desktop app running for local work; usage/access limits still apply.

- Claude pushes a numbered, ready task to the shared working branch, with branch name, prerequisite commits, exact desktop steps and expected results. If work moves to another branch, leave its queue/pointer on the current shared branch or main so the monitor can find it. Drafts and tasks with missing prerequisites wait.
- Codex fetches, picks the oldest ready unfinished task, records its number/source revision and in-progress status, then performs the agreed desktop verification. Existing fully specified open entries are eligible once their prerequisite commits are present. Read the numbered task status below for actual results; enabling the automation does not certify a task as tested.
- No overlapping expensive jobs, duplicate task runs or interruption of Vincent's playtesting. Defer while the desktop is busy. Preserve other sessions' changes and the existing local config/scan assets.
- Codex writes exact results, artifacts and unresolved failures here; marks verification complete (including any failed checks), or blocked with the reason; commits and pushes to the task's branch for Claude review. Do not rerun an unchanged completed/blocked entry until Claude explicitly reopens it or supplies a relevant fix. No automatic merges to main or independent gameplay/art design work.

## Codex tasks (from Claude; take the oldest open one)
Task (5) desktop status: **VERIFICATION COMPLETE; FP LIGHTING REVIEW NEEDED**, source `aad1a24`, claim `352910d`, branch `claude/project-thread-ndksis`. Build/import PASS; final gameplay **144/144 PASS**, FP **7/7 PASS** after fixing missing SkeletalMesh material usage. Three actual palette values/bindings and unchanged Gunmetal verified. FPS **132.2 -> 120.6** four bots, **145.7 -> 113.4** normal fixed FP (single observations; lower figures need review). Normal glove still clips white; a labeled `sb.FPLight 0` diagnostic restores a dark glove, isolating fill lighting as a contributor. Production light settings unchanged. No files reserved/jobs running; do not repeat this entry unchanged. Ready for Claude review/next task.

- **2026-11-05 (6), READY, same branch, at Claude's commit "Fix inverted normals on guns and armor; dimmer FP fill"
  (includes "Trigger finger: curl the first-person index finger onto a real trigger"):** Claude reviewed task (5), thanks for
  the SkeletalMesh usage fix and the `sb.FPLight 0` diagnostic. Root cause of the black guns/armor: `Tools/import_art.py`
  imported the FBX normals, which `export_fbx`'s mirror leaves pointing inward (the map importers already compute
  them; see `import_map_art.py`). It now uses `FBXNIM_COMPUTE_NORMALS` too. `sb.FPLight` default 45 -> 15 cd (the
  glove clipped). New `sb.ArmorSuit 0` for an FPS A/B of the suit material. Also (6) as first written below:
  Vincent saw the first-person index finger go through the gun.
  All five guns now have a trigger blade and a `SOCKET_Trigger` (Unreal socket "Trigger"); the first-person arms
  curl `index_01/02_r` onto it (CCD in the anim proxy, eased out during actions/swaps). Calibration now logs
  `First-person hand joints in grip space for <idle>: ...` (thumb/index/middle/ring/pinky positions in gun space).
  Steps: rebuild; `Tools\ImportCharacterArt.bat` (check each weapon lists a `Trigger` socket); self-test (expect
  145/145, new "...with the index finger on the trigger", report its cm); FPS runs (compare with task (5));
  FPS: task (5) measured 120.6 / 113.4 (was 132.2 / 145.7); please run each FPS measurement twice, and once more
  each with `sb.ArmorSuit 0`, so we can tell noise from a real cost. `VisualShots.bat FP` and copy every "hand joints in grip space" log line (BR and sidearm at least) into
  `docs/playtests/2026-11-04_character_pass/reports/after6-hand-joints.txt`; shots into `after6/`, plus one
  close crop of the hand on the BR if the shot tool allows. Look at: finger on the trigger, nothing through the gun.
- **2026-11-05 (5), READY, same branch, after Claude's commit "Matte suit under the armor; grey guns":** Claude
  reviewed task (4), thanks. Armor reads fine when lit (Keel third-person shots are grey/silver); Crucible bot_1 is
  backlit, not a material fault. The white glove was M_Mannequin itself, so armored soldiers now wear
  `MI_SB_Polymer` on every mannequin slot (first-person arms copy it). Gun palette raised: `WeaponMetal` 0.24 albedo
  metallic 0.2, `Polymer` 0.06 (both weapons/armor only, now in `CHARACTER_PALETTE` so a narrow import rewrites them).
  Also fixed: a pawn without a PlayerState never re-tinted once its armor attached.
  Steps: rebuild; `Tools\ImportCharacterArt.bat` (log should show the three character palette values;
  `MI_SB_Gunmetal` SHA unchanged); self-test (expect 144/144, new check "...over a matte suit"); the same two FPS
  runs (compare 132.2 / 145.7); shots into `docs/playtests/2026-11-04_character_pass/after5/`. Look at: FP glove
  dark and matte, FP gun mid grey with darker grips (not a black silhouette), bots' suit matte dark between plates.
Task (4) desktop status: **VERIFICATION COMPLETE; VISUAL ISSUES REMAIN**, source `4d1bfa6`, claim `1147723`, branch `claude/project-thread-ndksis`. Build/import PASS; gameplay **143/143 PASS**, FP **7/7 PASS**. FPS **128.5 -> 132.2** four bots, **149.9 -> 145.7** fixed FP. Actual palette values/bindings and unchanged map Gunmetal verified. After4 captures still show white gloves and very dark armor/gun surfaces; those visual goals are not fully met. No production source edits by Codex, no jobs/files reserved. Do not repeat this completed entry unchanged; Claude can review/reopen or add the next task.

- **2026-11-04 (4), READY, same branch, after Claude's commit "Grey armor and guns, matte gloves, frame-rate-proof step check":**
  Claude reviewed task (3): sprint/slide accepted; importer slot fix accepted, thanks. Fixes for what (3) found:
  armor/weapon palette raised to mid grey (`ArmorMetal` 0.2 albedo metallic 0.15, `WeaponMetal` 0.13 metallic 0.3);
  `Tools/import_art.py` now rewrites those two instances even on a narrow import (`CHARACTER_PALETTE`); the gloves'
  carbon-fibre clear coat/diffraction are turned off on armored soldiers (they mirrored the fill light: the white
  hand); the step-camera check now compares the eye's largest frame change with the curb height instead of the
  capsule's per-frame change (failed at 144 FPS while the eye did trail).
  Steps: rebuild; `Tools\ImportCharacterArt.bat` (check the log shows `MI_SB_ArmorMetal` / `MI_SB_WeaponMetal` with
  the new values, `MI_SB_Gunmetal` SHA unchanged); self-test (expect 143/143, report the step-camera line); the same
  two FPS runs (compare 128.5 / 149.9); shots via `shots_to_jpg.ps1` into
  `docs/playtests/2026-11-04_character_pass/after4/`. Look at: bot armor reads grey with blue/red plates (not a
  black silhouette), first-person glove dark and matte, gun body dark grey with visible detail (not pure black).
Task (3) desktop status: **VERIFICATION COMPLETE WITH ISSUES** on `claude/project-thread-ndksis`, code `9716ade` + `71047da`, claim `032fd79`. Build/import PASS; gameplay **142 PASS / 1 FAIL** (camera-easing ratio); FP **7/7 PASS**. Additional focused 60 FPS presentation **42/42 PASS** does not erase the full-run failure. Sprint/slide checks PASS. Four-bot FPS **130.5 -> 128.5**; fixed FP **146.4 -> 149.9**. After3 evidence and importer fix delivered for Claude review. Dark armor and white gloves remain; BR scope white disc is resolved. No files reserved or jobs running. Do not rerun this entry unchanged; Claude should review/reopen or add a new task after fixes.

- **2026-11-04 (3), same branch, after Claude's lighting commit and the sprint/slide tuning commit (one run covers
  both):** Vincent asked for faster sprint and farther (not longer) slides: SprintSpeed 950 -> 1020, slide entry boost
  350 -> 450, slide cap 1150 -> 1450, slide braking 350 -> 530 (same ~1.3 s slide, ~19% more distance). Rebuild, `Tools\ImportCharacterArt.bat` (armor and
  weapons now use new `ArmorMetal` / `WeaponMetal` palette slots; check the import log shows no unmatched slots and
  that `MI_SB_ArmorMetal` / `MI_SB_WeaponMetal` were created; map kits' `MI_SB_Gunmetal` must be unchanged), then the
  same two FPS runs (compare with 130.5 / 146.4) and the self-test (expect 143/143: new check "A slide starts faster than a sprint"; also report the
  "Sprint reaches sprint speed" and slide speed numbers from the log). Export shots to
  `docs/playtests/2026-11-04_character_pass/after3/`. What to look at: armor readable (not near black) in the
  Crucible bot shots, the first-person hand dark instead of white, no white disc on the BR scope's eyepiece.

Task (2) desktop status: **COMPLETE against `eba0b73`**. Build/import PASS, gameplay **142/142 PASS**, FP **7/7 PASS**. Four-bot FPS **125.4 -> 130.5**, fixed FP **142.6 -> 146.4** (individual matched script observations, actual 1280x720). After2 captures, exact reports and Manny parameter defaults are delivered below. No files reserved by Codex; ready for Claude/master review. Dark armor, pale scope cap and white gloves remain visual polish items.

- **2026-11-04 (2), same branch, after Claude's fix commit:** rebuild, run `Tools\ImportCharacterArt.bat` again (the
  BR and Sniper lost their rear reticle glow, the white disc in the first-person view), then the same two FPS runs as
  before (Crucible 4-bot self-test and `VisualShots.bat FP`) and record both against the 125.4 / 142.6 from the
  first pass. Expect 142/142 gameplay checks (curb fixture and buffered jump fixed). Export the FP and bot shots
  with `shots_to_jpg.ps1` into `docs/playtests/2026-11-04_character_pass/after2/`.
  Also, for the white first-person hand: write the parameter names of the mannequin's materials (the materials on
  `SKM_Manny`'s slots and their parents: every scalar/vector parameter name and default) into
  `docs/playtests/2026-11-04_character_pass/reports/mannequin_material_params.txt`, so Claude can tint the gloves.

Historical first-pass status, before `eba0b73` (superseded by task (2) above): the task below completed with two reported test failures. Spire reservations were released on main (`abfe0fa`); all Spire source/assets were already pushed through `e8f905f`. Character results, imported assets and tool fixes were delivered on `claude/project-thread-ndksis`. The buffered jump and curb fixture were then fixed by Claude and now pass.

- **2026-11-04, branch `claude/project-thread-ndksis` (PR: movement feel, armor v2, weapons v2, LODs).** Not compiled
  in the cloud. Don't run while Vincent is playtesting on the desktop.
  1. **Baseline on main first** (before checking out the branch): `Tools\SelfTest.bat` (note the "average N FPS ...
     with 4 bots" line in `Saved\Logs\SelfTest.txt`) and `Tools\VisualShots.bat FP` (the "fixed-view FPS" line in
     `Saved\Logs\SelfTest-Shots-Keel-fp.txt`). Same window size for both runs (the scripts use 1280x720).
  2. Check out the branch, build (fix compile errors if any and say what you changed), then run
     `Tools\ImportCharacterArt.bat` (weapons + armor only, with LODs; check `Saved\Logs\ImportArt.txt` lists
     "LODs 3" for each mesh and no unmatched slots) and `Tools\CheckWinding.bat` (expects all outward).
  3. Repeat both runs from step 1 on the branch. Record all four FPS numbers below under "Needs verification", plus
     the self-test result (expect 3 new steps: step smoothing, coyote time, jump buffer).
  4. `powershell -ExecutionPolicy Bypass -File Tools\shots_to_jpg.ps1 -Out docs\playtests\2026-11-04_character_pass`
     so Claude can see the first-person and bot-match shots (JPG is plain git), then commit the JPGs and the imported
     `Content/Shieldbreak/Art/{Weapons,Armor}` assets to the branch and push it (not main).

## Rules
1. **Pull before you start; push small commits.** `git pull --rebase` before every push.
2. **Reserve files before big edits.** Add a line under "In progress" below (and message the other
   session if you can). Small fixes to the other session's files are fine; say so in the commit message.
3. **Code session:** after a gameplay change, add a line to "Needs verification". The PC session
   builds, runs `Tools/SelfTest.bat`, fixes compile errors, and records the result here.
4. **Every gameplay change gets a self-test check** when it's testable without a human
   (`Source/Shieldbreak/Test/ShieldbreakSelfTest.cpp`). The PC session adds checks for features
   the code session lands, or the code session adds them itself.
5. **Content goes through Git LFS**, pushed only from the PC (`Tools/PushContent.bat`). The cloud clones with
   `GIT_LFS_SKIP_SMUDGE=1`. GitHub Free allows 10 GiB of LFS storage and 10 GiB of downloads a month.
6. **Commits carry the session link** in their trailer, so either side can tell who did what.

## Launch roadmap (decided by Vincent, 2026-10-21; details in docs/DESIGN.md)
Target: 5 maps × 4 modes (Slayer, CTF, KOTH, Oddball), a small competitive sandbox, strafing that matters.

| # | Work | Owner |
|---|---|---|
| 1 | Mechanics: remove Shield Shaping; walk 660 + snappier acceleration; spawn BR + **Sidearm**; SMG/Plasma out of play; **Rail Lance** (charge weapon); **Aegis Shield** (frontal barrier item); **Active Camo** power-up | Code |
| 2 | Mode framework + **CTF**, **King of the Hill**, **Oddball** (+ HUD, bots play objectives) | Code |
| 3 | Map framework (code-built layouts, per-map weapon/power-up/objective placement) + 5 greybox maps: **Keel, Bastion, Sanctum, Creekbed, Spire** | Code |
| 4 | Self-test coverage for 1–3; screenshots of every map/mode; FPS per map | PC |
| 5 | Find free, properly licensed art for the art pass: a modular sci-fi/ruins environment kit (Fab), a sidearm/rail weapon/rocket model, shield/barrier, flag and hill VFX. Record licenses in ASSET_CREDITS.md. Import only after Vincent approves the pick. | PC |
| 6 | First real **2-client listen-server test** (two game windows on one PC: `-game -server` + `-game 127.0.0.1`) to catch replication bugs | PC |

## In progress
- 2026-11-04 Codex: task (2) against `eba0b73` complete; no files reserved. Build, narrow character import and both script checks finished; Manny parameters exported read-only. No production gameplay, map or material edits. Local Android config retained; no engine/build process running at handoff.
- 2026-11-04 Codex: desktop character verification finished; yielding the checkout to Claude/master on `claude/project-thread-ndksis`. No build/game/import processes left running at handoff. The pre-existing local Android settings in `Config/DefaultEngine.ini` and ignored scan dependencies are retained. Movement, physics and animation smoothness remain Vincent's highest priority.
- 2026-11-04 Claude (cloud, branch `claude/project-thread-ndksis`): movement feel, armor and weapon art, first-person
  view. Reserving `Player/ShieldbreakCharacter.*`, `Player/ShieldbreakMovementComponent.*`, `Art/blender/build_armor.py`,
  `Art/blender/build_weapons.py`, `Art/Export/{Armor,Weapons}`, `Tools/import_art.py` until the PR merges.
- 2026-11-04 Codex: Spire work is fully committed and pushed through `e8f905f` (surfaces `d37e9a3`, ceiling `51e923b`, condenser `e8f905f`). All map/arena/art reservations from this session are RELEASED so Claude's map thread can complete its FPS check. No unfinished Spire source or assets remain in the working tree.
- 2026-11-04 Codex: taking the desktop verification task from `origin/claude/project-thread-ndksis` (coordination commit `f7d3adc`): baseline on main, branch build/import/winding, matched after measurements, screenshots and content push to Claude's branch. Movement/physics/animation smoothness is Vincent's latest priority; no overlapping gameplay or map feature work planned during this handoff. Existing local Android configuration and ignored scan content are preserved.
- 2026-11-03 Codex: Claude map/art redesign, ambience and combat-feel integration complete on main; see current desktop results in `HANDOFF.md`. No source files reserved. Next art work: Spire base-to-condenser section, per `VISUAL_QUALITY_TARGET.md`.
- 2026-11-02 Claude (cloud, branch `claude/project-thread-05ljt0`, PR #7): competitive redesign of Keel, Bastion,
  Spire and Creekbed (`Design/maps/shieldbreak_maps.py`, `Design/maps/analyze.py`, enclosed maps via `bIndoor` and
  `BuildInteriorLights` in `ShieldbreakArena.cpp`), plus a first per-map look pass. See `docs/MAPS.md`.
- 2026-10-29 Claude (cloud, branch `claude/project-thread-km43pg`): map art kit outside the walls for all five
  launch maps, per-map ground/rock/haze colours (`Game/ShieldbreakMapArt.*`, `BuildMapArt`, `BuildBackdrop`, fog
  colour in `ShieldbreakArena.cpp`), `Art/blender/build_map_kit.py`, `Tools/ImportMapArt.bat`. See
  `docs/MAP_ART_STYLE.md`. Sanctum is rebuilt at 152x104 m and now gets inside detail too: carved reliefs, crowns,
  amber edge bands on the walkways, a floor inlay and beacon at the centre, and a team-tinted sigil over each temple.
- 2026-10-27 Codex: map expansion/Spire modules/planner and the subsequent
  animation/movement-presentation/shield-hit pass are complete. See desktop
  verification records below. No files reserved; main is the shared handoff.

## Needs verification (code session → PC)
- 2026-11-05 Codex, **automatic task (5), `aad1a24`**: editor build PASS, no C++ fixes. Narrow import **19 meshes / three LODs / all slots matched**; actual palette values match source. ArmorMetal RGB **(0.20,0.21,0.23)** / metallic **0.15** / roughness **0.50**; WeaponMetal **(0.24,0.245,0.26)** / **0.20** / **0.42**; Polymer **(0.06,0.062,0.068)** / **0.00** / **0.62**. Shared map Gunmetal SHA256 remains `53d2bd0ab8bf4a97ededbcab88db49199e3f4b986db903e47ea25d610fab7158`.
  Caught a real rendering integration failure: first test logged `MI_SB_Polymer ... missing usage flag SkeletalMesh! Default Material will be used in game.` Stopped only that owned test, preserved its log, and took no completed result/FPS from it. Small importer fix enables/saves SkeletalMesh usage on existing/new `M_SB_Surface`, recompiles, then imports as before. This intentionally changes the shared master usage flag, not its graph/parameters or existing usage flags; Gunmetal instance remains byte-identical. Reimport/read-only verification confirms Polymer supports skeletal meshes. Final gameplay and FP logs have **zero missing-usage/default-material-fallback warnings**. No production gameplay C++ or lighting tuning changed.
  Corrected full rerun **144/144 PASS**, including `...over a matte suit ... -- MI_SB_Polymer`; step-camera eye **5.17 cm**, curb **29.95**, capsule frame **8.23**, trail **13.20**. FP **7/7 PASS**. Same actual **1280x720** observations: Crucible four bots **132.2 -> 120.6 FPS** (74 / 76 reported real seconds); Keel stationary FP **145.7 -> 113.4** (20.4 / 20.9 s). These lower single observations warrant follow-up, but do not establish causation or a controlled regression. Initial invalid-rendering run is excluded.
  **Actionable visual diagnosis:** normal FP glove remains overexposed white, now without the mannequin's carbon-fibre pattern. With the only diagnostic setting `sb.FPLight 0`, the same idle view shows a dark matte/cyan-lit glove. This demonstrates the fill light contributes to white clipping; Claude should tune/investigate FP fill/exposure instead of further mannequin tint guesses. The material swap and shader usage are verified. Default light value remains unchanged. Diagnostic FP **7/7 PASS**, raw 120.1 FPS / 19.6 s is a different lighting condition, excluded from the normal before/after pair. Gun surfaces remain very dark overall despite higher grey albedo, with rails/edges visible; mid-grey appearance is partial. Bot suit appears matte dark between plates; backlit body views remain dark, consistent with Claude's review of task (4), not a newly established material fault.
  **23 normal JPGs** in `docs/playtests/2026-11-04_character_pass/after5/`; one labeled fill-off idle comparison in `after5/fill-off/`. Reports `after5-{selftest,fp,import}.txt`, `after5-material-verification.json`, `after5-initial-rendering-fallback.txt`, `after5-fp-filloff.txt` in sibling `reports/`. Originals/full initial failure log retained in `Saved/CharacterVerification-20261104/After5/`. No network/latency/human movement acceptance or main merge in this task. No active jobs/reservations; Claude review requested for lighting and lower FPS observations.
- 2026-11-05 Claude, task (6): trigger blades + `Trigger` sockets on all guns (FBX re-exported), first-person trigger
  finger IK (`FShieldbreakBodyAnimProxy::ApplyTriggerFinger`), hand-joint log at calibration, self-test check for the
  finger. Not compiled.
- 2026-11-05 Claude, task (5): armored soldiers' mannequin slots use `MI_SB_Polymer` (replaces the M_Mannequin glove
  tuning, which had no visible effect); `ApplyPlayerColor` re-runs when the armor state changes, PlayerState or not;
  `WeaponMetal` (0.24, 0.245, 0.26) m 0.2 r 0.42, `Polymer` (0.06, 0.062, 0.068) r 0.62. Not compiled.
- 2026-11-04 Codex, **automatic task (4), `4d1bfa6`**: build PASS, no compile fixes; narrow import **19 meshes / three LODs / no unmatched slots**. Verified the updated existing instances, not merely their presence: ArmorMetal BaseColor **(0.20,0.21,0.23)**, metallic **0.15**, roughness **0.50**; WeaponMetal **(0.13,0.135,0.15)**, metallic **0.30**, roughness **0.40**. Both actual values match `palette.json`, with actual mesh bindings retained. Only two character material instances changed; shared map `MI_SB_Gunmetal` SHA256 remains `53d2bd0ab8bf4a97ededbcab88db49199e3f4b986db903e47ea25d610fab7158`. Codex added three logging lines in `Tools/import_art.py` so the requested import report explicitly records character palette values; no C++/design changes.
  Gameplay **143/143 PASS** under Claude's revised curb-relative camera assertion. Exact step line: eye change **5.75 cm**, curb **29.93 cm**, capsule frame **6.66 cm**, trail **12.63 cm**. This checks a different criterion from task (3); it is not a new production camera behavior change or proof of every frame rate. Sprint **1004.93 uu/s** in its sampling window; slide at 0.15 s **1293.94 uu/s**, faster-than-sprint check PASS. No stuck bots.
  Same desktop, actual **1280x720**: Crucible four-bot observation **128.5 -> 132.2 FPS** (reported **76 / 74 real seconds**); Keel stationary no-bot FP **149.9 -> 145.7** (**19.5 / 20.4 s**). Single script observations, not a controlled benchmark or proven performance gain/loss. FP **7/7 PASS**. Reports and 23 dimension-verified JPGs are in `docs/playtests/2026-11-04_character_pass/after4/` and sibling `reports/after4-{selftest,fp,import}.txt`, `after4-material-verification.json`. Local originals/read-only check under `Saved/CharacterVerification-20261104/After4/`.
  **Visual acceptance remains incomplete:** body armor is still very dark/near-silhouette in the Crucible bot views despite verified lighter palette values, with blue/red accents readable more than surfaces. FP glove is still bright white in the actual idle capture despite the matte parameter changes. Rails/edges and some gun highlights read, but large gun-body areas remain near black; grey/detail goal only partial. BR eyepiece remains dark, retaining the task (3) fix. Do not mark gloves/armor resolved. Runtime material/lighting investigation belongs to Claude's next task. No human feel, network/latency or broader map verification in this task; no main merge. No active desktop jobs or reservations at handoff.
- 2026-11-04 Claude, after task (3) review: mid-grey `ArmorMetal`/`WeaponMetal`, narrow imports refresh them,
  matte gloves (Carbon_CC 0, R_carbon 0.75-0.85, S_carbon 0.15-0.25, diffraction 0) on armored soldiers, step-camera
  check judged against curb height (eye frame change < 0.4 x curb, trail > 10 cm). Not compiled; Codex task (4).
- 2026-11-04 Codex, **task (3), sprint/slide + lighting**: build PASS; no production C++ edits. Imported 19 character meshes, all three LODs/no unmatched slots, new `MI_SB_ArmorMetal` and `MI_SB_WeaponMetal` created and loaded (metallic **0.30 / 0.40**, roughness **0.45 / 0.38**). Found a real reimport failure before gameplay: FBX contained the new palette names, but Unreal retained old `M_Gunmetal` display slot names, so successful imports silently rebound old material. Narrow fix in `Tools/import_art.py`: character reimports opt into FBX conflict reset, normalize display names from `imported_material_slot_name`, then bind the palette; import reports now list actual slot/material paths. Final read-only check confirms new materials assigned and all 19 meshes have three LODs. Shared map `MI_SB_Gunmetal` is byte-identical before/after: SHA256 `53d2bd0ab8bf4a97ededbcab88db49199e3f4b986db903e47ea25d610fab7158`. No map assets or other existing material assets changed.
  Gameplay **142 PASS / 1 FAIL** of expected 143. Sprint **1020.00 uu/s** (previous fixture **924.03**, previous tuning cap 950); slide **1293.60 uu/s at 0.15 s**, faster than sprint PASS; **1014.16 at 0.45 s** (previous fixture **1036.96 / 835.13**). Slide-jump/Momentum Carry and buffered jump pass. Slide distance/duration were not separately instrumented; the ~19% distance/same-duration description remains tuning intent, not a new measured result.
  Lone failure: camera step easing reports eye **5.95 cm**, capsule **7.93 cm**, trail **13.66 cm**, narrowly missing `eye < capsule * 0.75` (rounded values). Preserved report and thresholds. One focused **60 FPS capped** rendered-offscreen presentation run passes **42/42**, eye **11.65 cm** vs capsule **16.45 cm**, trail **16.45 cm**. Different sampling conditions matter; this does not prove the full-run failure harmless or establish perfect camera/physics. Claude should review the assertion and presentation across frame rates.
  Same desktop, verified **1280x720**: four-bot Crucible **130.5 -> 128.5 FPS**, 76 real seconds each; stationary zero-bot Keel FP **146.4 -> 149.9**, reported 20.3 / 19.5 s. Individual script observations, not a controlled benchmark. FP **7/7 PASS**. **23 JPGs** exported to `docs/playtests/2026-11-04_character_pass/after3/`; reports `after3-{selftest,fp,import,presentation60}.txt` plus `after3-material-verification.json` in its sibling `reports/`. Visual review: BR eyepiece dark (white disc resolved), gloves still white, Crucible armor still near-black despite the verified new bindings. Those two visual acceptance items remain open. No human smoothness approval or latency/network test in this task. Original/stale-slot diagnostics remain under `Saved/CharacterVerification-20261104/After3/`. Review handoff only; not merged to main.
- 2026-11-04 Claude, sprint/slide tuning (Vincent: faster sprint, slides farther not longer): SprintSpeed 1020,
  SlideEntryBoost 450, SlideMaxSpeed 1450, SlideBraking 530 (friction 0.35, exit 380 unchanged). New self-test check
  "A slide starts faster than a sprint". Not compiled; covered by Codex task (3).
- 2026-11-04 Claude, lighting pass for the character and guns (after Codex's `9502252`): armor and weapons use new
  palette slots `ArmorMetal` / `WeaponMetal` (same dark grey, metallic 0.3 / 0.4 instead of 0.8: with Lumen off,
  metal showed only reflections and read black in shade); map kits keep `Gunmetal`. The first-person fill light moved
  from 30 cm to about a metre from the eye (`sb.FPLight` 45 cd, radius 250), so the eyepiece and hand no longer burn
  out. Armored soldiers' mannequin gets `RubberBrightness` 0.35, `MetalBrighness`/`PlasticBrightness` 0.4 and a dark
  `CarbonfiberTint`, so the gloves are dark. `sb_kit.write_palette` now merges and keeps CRLF. Not compiled.
- 2026-11-04 Codex, **task (2) follow-up to `eba0b73`**: editor build PASS with no Codex compile fixes; character reimport **19 meshes / three LODs each / all slots matched**. Gameplay **142/142 PASS**, resolving both first-pass failures: curb rise **29.79 cm**, eye frame change **5.46 cm** versus capsule **9.24 cm**, trail **14.26 cm**; buffered jump **891.60 uu/s**, coyote jump **891.67**, expired-window rejection PASS. Existing presentation assertions also pass.
  Same desktop and actual **1280x720**: Crucible four-bot run **125.4 FPS first pass -> 130.5 follow-up** (76 real seconds); Keel no-bot stationary FP **142.6 -> 146.4 FPS** (20.4 / 20.3 s). FP **7/7 PASS**. These are individual script observations, not a controlled benchmark. Full reports: `docs/playtests/2026-11-04_character_pass/reports/after2-{selftest,fp,import}.txt`. **23** JPGs in `after2/`, dimensions verified. FP BR capture confirms lowered/further gun and removed central glow dot; rear scope cap remains pale. Crucible bot captures still show near-black armor despite the distance-field flag change. White gloves remain, as expected before a tint implementation. No claims of resolved character readability, perfect physics, human animation acceptance or visual movement under latency.
  Requested Manny report: `reports/mannequin_material_params.txt` in the same review directory. Both slots and their full parent chains, **80 scalar / seven vector names per material interface**, effective inherited instance defaults, authored master defaults and local overrides (association/index) included. Read-only tool: `Tools/dump_mannequin_materials.py`. First export completed its report but crashed at Python shutdown with transient wrappers retained globally; moved Unreal wrappers into function scope and collected them before shutdown. Final rerun **exit 0, 0 errors / 3 warnings**, complete report verified; first failure log preserved in `Saved/CharacterVerification-20261104/After2/MannyParams-FirstShutdownFailure.log`. No Unreal material assets saved/changed. All original captures/logs retained in the After2 folder. No new network tests required by this task or run in this follow-up. Ready for Claude/master review on this branch, not merged to main.
- 2026-11-04 Claude, fixes after Codex's desktop pass (`b5a09e2`): (1) buffered jump: `Landed()` now only marks the
  jump, and `AShieldbreakCharacter::OnMovementModeChanged` issues `Jump()` after the engine's walking transition has
  reset jump input. (2) Curb fixture: the engine spreads the step-up over frames (9 cm max), so only the climb is
  required; the eye checks are unchanged. (3) Armor pieces and weapon meshes no longer affect distance-field lighting
  (the suit rendered near black in the bot shots). (4) First-person gun sits a little further and lower
  (`sb.FPGripForward` 38, `sb.FPGripDown` 17.5) and the BR/Sniper rear reticle glow is gone. Not compiled.
- 2026-11-04 Codex, **character branch desktop verification**: build PASS after adding the missing `ShieldbreakSelfTestPrivate` namespace import in `BuildPresentationSteps` (no production movement edits). Narrow import **19/19 meshes**, **three LODs each**, no unmatched slots; winding **all outward**. Fixed LOD import's absent commandlet subsystem by running unattended editor mode and using UE 5.8 `StaticMeshReductionOptions/Settings`. Both art launchers share `Tools/ImportArt.ps1`; only the narrow character import was executed. Narrow imports now reuse existing palette materials; initial unrelated material resaves were restored. No map materials, geometry or layout changed.
  Matched desktop script observations, actual **1280x720**: Crucible four-bot gameplay **127.8 FPS before / 125.4 after** (76 real seconds each); Keel stationary first-person, no bots **144.0 before / 142.6 after** (20.3 / 20.4 s). Single observations, not a controlled benchmark. The FP harness previously had no FPS line; opt-in measurement added on main as `0a2f52b` and merged into this branch for both captures.
  Main baseline **136/136 PASS**; branch **140 PASS / 2 FAIL**; FP **7/7 PASS** both. Failures retained: (1) curb rises **29.95 cm**, but the test requires a single-frame capsule change >20 cm; observed **9.05 cm**. Eye easing itself PASS (**5.22 cm** maximum change, **15.28 cm** trail, settles to zero). Review the fixture's extra snap assumption. (2) buffered jump **0.00 uu/s**, genuine takeoff failure. Engine `ProcessLanded` calls `Landed()` before changing movement mode; `ACharacter::OnMovementModeChanged` then calls `ResetJumpState`, clearing the `Jump()` requested by Claude's `Landed`. Move/rework buffer consumption after the walking transition; prediction/replay needs review. No thresholds or production behavior changed to conceal either failure.
  Coyote late jump **891.95 uu/s**, expired-window rejection PASS; interrupted-fade hand speed **210.22 cm/s**, pelvis drift **6.25 cm**, gait continuity PASS. JPGs and exact reports are committed under `docs/playtests/2026-11-04_character_pass/` (23 before + 23 after, all dimensions verified). Review shows armor remains dark in the Crucible in both runs; new model detail is visible, but FPS grip/posing and character readability still need work. Screenshot converter now supports a focused source folder to exclude 1,968 older captures. Local originals/failure logs retained in `Saved/CharacterVerification-20261104/`. Network/latency visual review and human feel acceptance were not rerun for this branch. **Not ready to declare perfect physics/animations or a fully passing release.**
- 2026-11-04 Codex desktop baseline: main `abfe0fa` gameplay **136/136 PASS**, four-bot fixture **127.8 FPS** at actual 1280x720. The requested FP script initially passed 7/7 but had no FPS measurement. Added a 20-second stationary first-person observation to this opt-in test; build PASS, baseline FP **7/7 PASS**, **144.0 FPS over 20.3 s**, actual 1280x720, no bots. The identical harness will be used on Claude's branch. Reports and pre-import captures retained in `Saved/CharacterVerification-20261104/Baseline`.
- 2026-11-04 Claude (cloud, branch `claude/project-thread-ndksis`): **movement feel + character and weapon art v2**.
  Not compiled. (1) `AShieldbreakCharacter::HandleMovementUpdated` eases the first-person camera over steps and curbs
  (`StepCameraOffset`, `sb.StepSmoothing`); crouch changes are excluded. (2) Coyote time: a jump within 0.12 s of
  walking or sliding off a ledge takes off (`UShieldbreakMovementComponent::CoyoteTime`, saved per move,
  `sb.CoyoteTime`). (3) Jump buffer: a jump pressed up to 0.12 s before landing fires on touchdown
  (`PressJump`/`Landed`). (4) Armor v2 (`Art/blender/build_armor.py`, 20.3k tris for the set, was 4.5k) and a
  weapons detail pass (`build_weapons.py`); same bones, sockets and slots. (5) `Tools/import_art.py` adds LODs for
  Weapons and Armor; `Tools/ImportCharacterArt.bat` imports only those folders. New self-test steps: "The camera
  rides over a step instead of snapping", "Coyote time: ...", "A jump pressed just before landing ...". FPS before
  and after: see the Codex task above. Previews: `/mnt/project-files/art/previews/v2/` (Claude's project files).
- 2026-11-03 Codex, induction condenser V2: Blender authoring, narrow one-mesh import and editor builds PASS. New mesh **17,456 triangles**, five material slots, imported bounds match the report. Existing palette entries verified unchanged; two new Spire entries added. Shared material master save side effects from the first import were restored, and the importer now skips existing usage flags; those masters remain unchanged. Original condenser retained under `-SBLegacySpireCondenser`.
  Final Spire tour **29/29 PASS**, six lifts and no stuck bots, **76.3 average FPS** with four bots at verified **1920x1080 / 100%**. Empty-view new/retained observations **101.7 / 102.8 FPS**, one sample each, not a benchmark. New hero minimum 1925 cm leaves **65 cm** against the conservative ordinary-jump/lift head estimate; arbitrary knockback/grenade stacks remain unverified. Bridge/floor close views and the retained model were inspected; the obstructed first floor camera was corrected. Reports: `SpireHeroBuild.log`, `MapArtImport.txt`, `SpireHeroFinalBuild.log`, `SpireHero-herofinal.txt`, `SpireHero-herolegacy.txt`, `SpireHeroGeometryQA.txt`, `SpireHeroTour.txt` under `Saved/Logs`. Proof: `docs/playtests/2026-11-03_spire-condenser-{bridge,floor,legacy}.png`. Full-roster frame pacing, real-team readability and human movement review remain next.
- 2026-11-03 Codex, Spire ceiling framework: four original meshes imported with matching bounds/triangle counts; editor builds PASS. Offline geometry QA passes concave/chamfer analytic volumes, manifold edges, winding and a 70 cm conservative ordinary-jump/lift head clearance. Initial placement tour **29/29 PASS**, no stuck bots, six lift landings checked; final edge chamfers and lamp housings were added afterward and reviewed in clean captures. FP idle/reload/melee/grenade captures reviewed; the existing gun/character finish and grip posing still need polish.
  Final PNG dimensions verified **1920x1080**, screen percentage 100, `-ForceRes`; zero-bot fixed-view observation **111.0 FPS over 19.2 s**. The earlier requested-1080 run was actually 888x500 (223.4 FPS) and is NOT 1080p evidence. Initial 720p architecture on/off observations (168.4 / 182.8 FPS) were single samples and do not establish a benchmark. Reports: `SpireArchitectureFinalBuild.log`, `SpireArchitectureImport.txt`, `SpireArchitectureTour.txt`, `SpireArchitectureFP.txt`, `SpireArchitecture1080.txt` in `Saved/Logs`. Proof: `docs/playtests/2026-11-03_spire-architecture-{spawn,central}.png`. Runtime layout/collision unchanged; arbitrary knockback and grenade stacks remain unverified.
- 2026-11-03 Codex, Spire surface/lighting increment: editor builds PASS, original material/texture import PASS (final import 0 errors / 0 warnings), Spire tour **29/29 PASS** including all six lift landings, Keel tour **27/27 PASS**. No stuck bots in either 75-second fixture. Tours exercised the new cladding/material bindings before the final cosmetic lamp/emission and microstructure tuning; final rebuilt lighting/materials were reviewed in six matched screenshots with no material compile failures. Layout generator/data, collision and movement tuning are unchanged.
  Final fixed-view observation: **175.0 FPS over 19.8 s**, zero bots, 1280x720, rendered offscreen on the RTX 2080 Ti. This is not a controlled before/after comparison or a combat/frame-pacing benchmark. Reports: `SpireLightingBuild.log`, `SpireSurfacesImport.log`, `SpireSurfaceTour-Spire.txt`, `SpireSurfaceTour-Keel.txt`, `SpireSurfaceFinalShots.txt` under `Saved/Logs`. Proof: `docs/playtests/2026-11-03_spire-surfaces-{spawn,central}.png`. New surface source, five Unreal assets and the packed original mask texture are included; no licensed scan content added to Git. Next: overhead structural meshes and player-view/readability review.
- 2026-11-03 Codex integration supersedes older pending-build notes below: editor build PASS; rendered normal-timing offscreen gameplay **136/136 PASS**; controlled 60 Hz presentation **36/36 PASS**; Keel/Spire local joins PASS; 4v4 PASS (20 balanced samples); shield events with configured 80 ms lag / 20 ms variance PASS. Reports: `Saved/Logs/IntegrationBuild.log`, `IntegrationSelfTestFinal.txt`, `SelfTest-Presentation.txt`, `IntegrationNetworkKeel.txt`, `IntegrationNetworkSpire.txt`, `Integration4v4.txt`, `IntegrationShieldNetwork.txt`.
  Initial hidden-window gameplay was **135/136**, gun-kick peak 0.40 degrees and 16.6 average four-bot FPS. The normal-timing offscreen rerun passed with a 1.44-degree kick and 124.2 average four-bot FPS; no code or thresholds changed. These fixtures are not a performance benchmark or proof of low-FPS presentation quality. Preserve both reports (`IntegrationSelfTestInitial.txt` and `IntegrationSelfTestFinal.txt`). User playtesting and the first finished art section remain next.
- Map art kit (branch `claude/project-thread-km43pg`): verified on the desktop 2026-10-30. Build OK, import
  `RESULT OK, 16 meshes`, self-test 126/126, every map tour passes (Sanctum 25/25), Sanctum shots in
  `Saved/Screenshots/Shots`. The imported kit and `Design/maps/Sanctum.png` are committed on the branch. Kit lighting
  notes: the kit ISMs stay out of distance-field lighting (they rendered near black inside its range), and pieces
  laid flush on blocks (Relief, Fascia, Inlay) get `SelfLight` 0.35.
- 2026-11-03 Claude (cloud): **combat feel: no recoil, Halo-style aim assist, hit markers, shield pop** (Vincent's
  request; see `docs/GAME_FEEL.md`). Shots never move the aim. Bullet magnetism applies to every input, slowdown to
  stick and mouse, and tracking pull to the controller only. A setting turns it off. Hit markers come from
  `UShieldHealthComponent::ConfirmHit` for every damage source. The shield pop has a new sound, sparks, a burst shell
  and a view flash. New self-tests: no recoil, magnetism, mouse slowdown, hit-marker flags. Measure FPS before and after.
- 2026-10-29 Claude (cloud): **characters skipping as they move** (Vincent: "not playable"; same branch, PR #5). Not
  compiled here. Two fixes. (1) `ShieldbreakBodyAnimInstance.cpp` `LockRoot` pins the root bone to the ref pose
  after every sample. The Lyra locomotion clips can carry root motion, which nothing extracted, so the body slid
  ahead of its capsule and snapped back on each loop and cross-fade. (2) `ShieldbreakBotController.cpp` `FollowPathTo`
  skips waypoints already passed after each once-a-second repath (bots used to turn back toward the nearest node
  behind them) and rounds corners near a node. New self-test: "Locomotion plays in place" (pelvis drift under 25 cm
  while jogging on the spot). **Please report the measured drift before this change if you can.** It shows whether
  root motion was the cause. If skipping remains, say whether it shows offline with bots or only over the network.
- 2026-10-29 Claude (cloud): **first-person feel and animation smoothing** (same branch, PR #5). Not compiled here.
  `Visuals/ShieldbreakBodyAnimator.*` (first-person bob/sway/lean/sprint/landing/kick), `ShieldbreakBodyAnimInstance.*`
  (fade from snapshot when a fade is interrupted; per-shot upper-body kick), `Weapons/ShieldbreakWeapon.cpp` (kick hooks;
  recoil goes through `AShieldbreakCharacter::AddViewKick`), `Player/ShieldbreakCharacter.*` (eased recoil). Please build,
  run `Tools/SelfTest.bat` (four new presentation checks), and play Keel with bots for a few minutes. Judge whether
  the walk bob, sway and kick feel right. The amplitudes are in `UpdateFirstPersonMotion` and `AddFirstPersonKick`,
  and `sb.FPMotion` scales them all. Details: `docs/SMOOTHING_AND_SHIELD_FEEDBACK.md`.
- 2026-10-29 Claude (cloud): **cover pass on the five enlarged launch maps** (branch `claude/project-thread-1xgiqk`).
  Data only: `Design/maps/shieldbreak_maps.py` adds 20-44 blocks per map, and the regenerated
  `Game/ShieldbreakMapsData.cpp` is purely additive (Crucible unchanged). Not compiled here. Please build, then run
  `Tools/SelfTest.bat` and the map tours (`Tools/MapTour.bat` for Keel, Bastion, Sanctum, Creekbed, Spire, plus
  Keel CTF and Bastion KOTH). Watch for: bots stuck on the new cover, the Sanctum plinth jump checks, Spire's four
  lift landings, and any spawn reported blocked. Screenshot one lane per map. Cover placement rules and the
  before/after openness numbers are in `docs/MAPS.md`.
  The cloud can't push LFS objects, so `Design/maps/*.png` still show the stretched layouts: run
  `python Design/maps/shieldbreak_maps.py` on the desktop and commit the regenerated previews.
- Full manual Settings/Controls/music walkthrough of the merged menu is still
  separate from the automated gameplay and networking results below.
- Further human playtesting should judge movement feel, bespoke FPS poses,
  foot contact/IK, map pacing/cover density, and visual movement under latency.
  Local delayed-packet shield delivery is tested; internet/eight-human play is not.
- Historical requests below are preserved for context, not new outstanding work.

## Current game (this branch)
- Spawn kit is Battle Rifle, Sidearm and grenades. Map pickups are Sniper, Rocket Launcher,
  Rail Lance, Overshield, Aegis and Active Camo. No SMG, no Plasma Rifle, no Shield Shaping.
- Modes: Team Slayer, CTF, King of the Hill, Oddball. Maps: Keel, Bastion, Sanctum, Creekbed,
  Spire, plus the Crucible test layout. Session cap is 4v4 (8).
- Front end is `SShieldbreakMainMenu`: Play (Multiplayer, Host Match, Practice, Back), Join,
  Settings, Controls, Exit. Settings and Controls are the PR #2 screens (graphics, window,
  VSync, frame cap, volumes, FOV, sensitivity, invert, tips, key rebinding). There is one
  `UShieldbreakGameInstance`, in `Game/ShieldbreakGameInstance.*`, for connection recovery.
- Menu music plays on the front end. The cracked-shield emblem is `Content/Splash/Emblem.png`.
- Renderer: the arena's post-process volume now enables Lumen GI and reflections by default,
  overriding the older config defaults. `-SBNoLumen` compares the previous path. Virtual
  shadow maps and motion blur remain off. See `MAP_ART_STYLE.md` for the November 3 lighting work.

### Historical requests (already covered by later PC records, or obsolete)
- **Roadmap item 3: maps** (commit "Map framework and five launch maps"). See docs/MAPS.md for the overview images.
  - Maps are data (`Game/ShieldbreakMaps.h`, generated `Game/ShieldbreakMapsData.cpp` from `Design/maps/shieldbreak_maps.py`).
    `AShieldbreakArena` builds whichever map the GameState's replicated `MapId` names (`?Map=Keel|Bastion|Sanctum|Creekbed|Spire|Crucible`;
    default **Keel**). **When `-SBSelfTest` is on and there's no `?Map=`, the map is Crucible**, so the existing checks keep their layout.
  - New: `AShieldbreakGravLift` (Spire's 4 lifts), built locally on every machine; it launches the character to a landing
    spot (server + owning client).
  - The bot waypoint graph is now generated for any map: a grid scan drops nodes on every walkable surface, with walk links,
    one-way drop links off ledges, and one-way lift links. The log line "<Map> waypoint graph: N nodes, ..." shows the counts.
  - Console `SB_Map <map> <mode>` server-travels to the same level with new options.
  - Please: run each map in Slayer with bots for ~60 s (do bots move around without getting stuck? FPS?); take a few
    screenshots per map; check Spire's lifts land you on the base / bridge; and run CTF on Keel and KOTH on Bastion.
    Layout bugs (gaps, unreachable spots, z-fighting) go to me with coordinates; I fix them in the generator.
- **Roadmap item 2: modes** (commit "Objective modes"). Launch with `?Mode=CTF`, `?Mode=KOTH` or `?Mode=Oddball` (default Slayer).
  Objective modes force teams; kills don't score the team there.
  - **CTF** (`AShieldbreakCarriable` Flag): flags on each base platform ((-3200,0,390) red, (3200,0,390) blue).
    Walk over the enemy flag to take it (you auto-swap to the sidearm; only the sidearm fires; 85% speed). Bring it to
    your own flag *while it's home* to capture (+1). Dying drops it; your team touching a dropped flag returns it;
    it auto-returns after 15 s. First to 3.
  - **KOTH** (`AShieldbreakHill`, 600x600 zone): +1 per second while only one team is inside; the zone moves every 60 s
    through 4 spots. First to 100.
  - **Oddball** (`AShieldbreakCarriable` Ball at (0,800,90)): +1 per second while held; melee only while carrying;
    drops on death; resets after 30 s dropped. First to 100.
  - HUD: mode header, objective markers clamped to the screen edge with distance, a carrier banner, event lines in the
    kill feed (`FShieldbreakKillFeedEntry::bEvent`). Bots play objectives (attack/defend split, carry home, chase
    stolen flags, hold the hill, keep the ball).
  - Please add checks: capture scoring, flag drop/return/auto-return, the carrier fire restriction, hill scoring and
    contesting, ball scoring. Also screenshots of each mode.
- **Roadmap item 1: mechanics and sandbox** (commit "Competitive mechanics pass"). Please build, add checks and screenshot:
  - Shield Shaping is gone: `SetShaping`/`IsShaping`, its tuning and `bIgnoresShieldShaping` are deleted. I removed the
    "Shield Shaping" self-test step and made the arena check count only `Overshield`-type power-ups (small edits to your file).
  - Movement: walk 660, accel 6000, braking 3200, sprint 950.
  - **Power Jump**: when crouched on the ground, Jump starts charging (speed drops to 140) and release launches. A full 0.5 s
    charge gives 1.5x height (Z velocity = 900 * sqrt(1.5) = 1102); a tap is about a normal jump. Uncrouched jumps are unchanged.
  - Spawn kit **BR + Sidearm** (`AWeapon_Sidearm`: 12 damage, headshot x5 on an unshielded target, 12-round magazine).
  - **Rail Lance** (`AWeapon_RailLance`, new `EShieldbreakFireMode::Charge`): hold to charge 0.8 s, release. Full charge
    does 200 (one-hit kill); early release does 66. Charge is timed on the server. 3 shots. Glow orb + whine tell for others.
  - **Aegis** (`AShieldbreakPowerup` type `Aegis`, then `AShieldbreakCharacter::SetUsingEquipment`): a raised barrier blocks
    hitscan (bullets drain `AegisHealth` 200, uptime 6 s) but not rockets, grenades or melee; no firing while raised.
  - **Active Camo** (`AShieldbreakPowerup` type `ActiveCamo`, 30 s): invisible to others when still, shimmering when
    moving or firing; bots only spot it within 450 uu (still) or 1800 uu (moving).
  - Arena: plasma spots are now Rail Lances (60 s); an Aegis sits at (0,-800) (45 s). The SMG and Plasma Rifle classes were deleted after this note; do not add them back.
- **Rocket self-damage (answer to the design question).** Your own rockets now deal 0.4x damage to you
  (`SelfDamageScale`) with 0.65x knockback (`SelfKnockbackScale`), so a rocket jump at your feet from full
  shields costs 88 and leaves ~27 health. Enemies still take the full 220. Please turn the rocket-jump INFO into
  a check: survives from full shield, and lands with 20-40 health.

## Design questions from the PC session (code session to decide)
- (Answered by the code session: self-damage scaled; see Needs verification.) **Rocket jumps kill you without an overshield.** A rocket at your own feet does the full 220 (shield 70 +
  health 45 = 115), so the rocket jump is only survivable with an overshield (then: 34.9 health left and a
  ~2000 uu/s launch, which is very high). If rocket jumping should be a real skill move, consider scaling
  self-damage from your own rockets (e.g. 0.5x, as many arena shooters do) and/or lowering `KnockbackUp` for the
  shooter. Self-test currently checks the overshield case and logs the plain case as INFO.
- **Test harness note:** `AAIController::UpdateControlRotation` zeroes pitch unless focused on a pawn. Bots override
  it, so they're fine; self-test shooters now freeze their AI tick before aiming.

## Verified on the PC
- 2026-11-02 Claude, driving the desktop remotely: **Spire and Creekbed redesign** (`Saved\RunR3.bat`, commit
  `b93e9e1`). Build OK, self-test 126/126. Tours: Spire Slayer 29/29 (115 FPS, 66 interior lamps), Spire CTF 39/39,
  Creekbed Slayer 23/23 (114 FPS), Creekbed Oddball 29/29, no bot stuck for 20 s. **Look pass** (`RunR4.bat`,
  `b5c3b9d`): build OK, self-test 126/126, shots `*_r4_*`. The interrupted-fade hand check that missed in R2 passed
  in both runs. The desktop checkout has a local edit to `Config/DefaultEngine.ini`, left alone; the run scripts now
  push with `git pull --rebase --autostash`.
- 2026-11-02 Claude, driving the desktop remotely (`Saved\RunR2.bat`, commit `0c12bef`): **Keel and Bastion
  redesign**. Editor build OK. Self-test 125/126: the miss is "The gun hand moves smoothly through an interrupted
  fade" (fastest hand movement 501 cm/s), unrelated to maps. Tours: Keel Slayer 27/27 (92 FPS, 66 interior lamps),
  Keel CTF 37/37, Bastion Slayer 24/24 (102 FPS), Bastion KOTH 28/28, no bot stuck for 20 s anywhere. Shots in
  `Saved/Screenshots/Shots/keel_r2_*` and `bastion_r2_*`.
- 2026-10-29 Claude, driving the desktop (`desktop-888hmr5`) remotely: **PR #5 built and tested**. The editor build
  compiled first time (only the existing `RemoteViewPitch` deprecation warning). Full gameplay suite **124/126**.
  All new presentation checks pass:
  - shot kick peaks at 1.33 deg and settles
  - recoil is spread (0.65 of 2 deg in the first frame), with the full 2 deg in 0.3 s
  - interrupted fade: hand peaks at 279 cm/s
  - jogging in place: the pelvis wanders 6.3 cm

  The two failures are both "...and launches you upward" in the Rocket jump step (peak up-velocity 0.00; the
  damage itself is right, health 27). **This is not from PR #5.** Main (`eec415c`) and the cover-pass commit fail
  identically when built today (115/117), although Codex recorded 117/117 for `eec415c` on 10-27. The grenade jump
  on an uncontrolled dummy still launches (526 uu/s). The shooter in this step is an AI-controlled dummy whose
  controller tick the test freezes. Moot since then: Vincent dropped rocket jumps (2026-10-29), so own rockets no longer knock you back and the step now checks that.

  Map tours: Keel 23/23 (92 FPS), Bastion 23/23, Sanctum 25/25, Creekbed 23/23, Spire 27/27, Bastion KOTH 27/27.
  Keel CTF first failed 31/33: all four bots stood on their own bases for the whole match with 0 kills. It fails
  the same way before PR #5's bot change. CTF bots split attack/defend on their object id's parity, and this match
  made all four defenders. Fixed in `ae0ef76`, which alternates by rank within the team, and `9a413cd`, where
  defenders move between guard spots every 6 s. Keel CTF is now **33/33** (12 kills, 0 stuck).
  With 4 bots, FPS ran 84-117 across the tours.
- 2026-10-27 Codex desktop: **smoother presentation and visible shield hits**.
  Built on Claude's native animation/first-person rig. Ground movement now blends
  four directional clips continuously, preserves gait phase through turns and
  gait changes, and smooths aim pitch/playback rate. Interrupted actions cross-fade
  for 0.1 s. Ordered movement -> animator -> body/arms ticks and warmed the fixed
  animation set on first spawn. Removed first-person upper-arm plates that were
  crossing the camera; world armor is retained. Further FPS-specific posing and
  foot IK remain polish work, not a claim of AAA parity.
  Client visual corrections use exponential smoothing and 256/384 cm bounds
  (previously 92/140), with 0.1/0.05 s location/rotation windows. Collision, input,
  sprint/jump tuning, server authority and saved-move prediction remain intact.
  Local crouch/stand camera now compensates and eases capsule-height changes,
  with a static-geometry sphere sweep and death-camera exclusion. First camera
  check exposed a 34 cm snap: the usual setter lost its offset inside Unreal's
  deferred capsule movement. Matching the engine's direct mesh-offset pattern
  fixed it. Full-suite frame samples were **2.89/2.86 cm** for crouch/stand;
  these are fixture observations, not a bound for all frame rates/scenarios.
  Added actual absorbed-damage cosmetic events and an original additive Fresnel
  overlay: cyan shield hits (~0.32 s), brighter breaks (0.5 s), gold overshields.
  Health-only hits, recharge and passive overshield decay do not flash. Overlays
  clear after pulses; no permanent extra mesh draws. Hits briefly reveal camo
  during the pulse. Shield break replication supplies a dropped-hit-RPC fallback.
  No shield/health damage math changed.
  UE 5.8 editor builds **PASS**. Full gameplay suite **117/117 PASS**; focused
  presentation suite **17/17 PASS** again after final visual tuning. Tests cover
  gait continuity, interrupted hand continuity, crouch/stand, overlay application,
  fade/cleanup, break, health-only and passive-decay exclusions. Initial reports
  and camera diagnostics remain under `Saved/Logs/Smoothing-*` and `Presentation*`.
  Real local host/client shield regression **PASS** with **80 ms packet lag and
  20 ms variance configured on both processes**, matching all four events and
  one break on authority and remote roles; no health-only/decay events. This is
  delayed local delivery verification, not measured movement quality online.
  Final 4v4 regression **PASS** with host controls/remote rejection, join bot
  replacement, disconnect refill and **20** balanced roster samples.
  Rendered Spire checks cover cyan hit, white/cyan break, gold overshield, normal
  return, and first-person action poses. Fixed capture timing: writing a preceding
  PNG stalled a frame until the short hit expired; capture-settle steps now record
  active hits at ~40 ms. Removed the unnecessary experimental overlay warm-up.
  Reports: `SelfTest.txt`, `SelfTest-Presentation.txt`, `ShieldFeedback-FinalReport.txt`,
  `ShieldFeedback-{Server,Client}.log`, `Smooth4v4-Report.txt`, `ShieldVisualShots.log`,
  `SmoothActionShots.log`. Captures: `docs/playtests/2026-10-27_shield-*.png`.
  See `docs/SMOOTHING_AND_SHIELD_FEEDBACK.md`. No dedicated/internet/eight-human
  verification or controlled performance comparison claimed. Known batch run-end
  log append-sharing messages remain; completed reports and independent checks
  are the evidence.
  Editor captures still show shader-preparation messages; cold shader/PSO pacing
  and packaged performance are not verified by this pass.
- 2026-10-27 Codex desktop: **4x ground-area map pass and Spire modules**.
  Synced merged main at `174ffd3` before editing; both checks of fetched
  `origin/claude/*` found no commits outside main. Preserved Claude's original
  weapons/armor, world art, body/first-person animation and menu/settings work.
  Fixed a unity-build collision between private menu helper namespaces; UE 5.8
  final editor build **PASS**. All five launch maps now have **2x X/Y** and their
  original Z heights: Keel **144x88 m**, Bastion **120x120 m**, Sanctum **152x104 m**,
  Creekbed **112x72 m**, Spire **168x64 m**. All gameplay anchors and supporting
  surfaces transform together. Crucible and movement/jump tuning are unchanged.
  Authoring validation and generated-data equality **PASS** for all six maps.
  Final general gameplay suite **109/109 PASS**. Map/mode tours: Keel **23/23**,
  Bastion **23/23**, Sanctum **25/25**, Creekbed **23/23**, Spire **27/27**,
  Keel CTF **33/33**, Bastion KOTH **27/27**; all seven 75-second four-bot matches
  passed stuck detection and combat checks. Sanctum's test run-up now follows
  the larger plinth edge and still distinguishes normal from charged jumps.
  First Spire run was **26/27**, failing only the shared aim-assist fixture:
  its falling pawns can encounter different map geometry. The fixture now holds
  both pawns stationary in flying mode; thresholds/gameplay assistance unchanged.
  Rerun **27/27**, including all four lift landings (within ~20 cm). First report
  retained as `Saved/Logs/Spire-FirstPass-AimFixtureFailure.txt`.
  Added three original, low-poly Spire meshes (collector, service panel, lift
  collar), import-axis/size/pivot checks, and instanced material usage flags.
  Runtime uses ten collectors, ten panels and four collars with no collision/nav
  influence. Visual check: enlarged Spire overview, readable shaded walls,
  first-person reload/melee/grenade and third-person throw. Current action poses
  still need FPS polish; this is an initial architectural pass, not finished art.
  Captures: `docs/playtests/2026-10-27_expanded-spire.png` and `spire-*` captures.
  Preserved/completed the overhead sketch planner from this chat: actual map data,
  drawing/moving marks, one-metre nudges, independent dimensions, saved sketches
  and direction briefs. Browser checks **PASS** at 320/736 px and in the inline
  runtime; regenerated outputs **PASS**. Defaults show the enlarged current size.
  Keel and Spire local server/client joins **PASS** (30 joined game seconds each).
  4v4 regression **PASS**, including bot replacement/refill, host-only commands
  and **20** balanced roster samples. Connection recovery **PASS**: host loss
  restores retry; failed join retains its address; one retry joins without stale
  errors. Full manual Settings/Controls/music check remains separate.
  No internet, latency, dedicated-hosting or eight-human claim. Bot FPS figures
  are not controlled benchmarks. Reports: `Saved/Logs/SelfTest*.txt`,
  `MapTour-*.log`, `SpireFinalMap.log`, `SpireImport.txt`, `SpirePass*.txt`.
  See `docs/SPIRE_ART_PASS.md`, `docs/MAPS.md`, and `Design/map-planner/README.md`.
- 2026-10-27 Codex: **Connection failure recovery and retry**. Added a persistent
  game instance that retains sanitized feedback and the last host address across
  Unreal's disconnect travel/controller replacement. Failed joins and lost-host
  connections open a usable Join screen; successful joins clear stale errors.
  Pending-driver ownership handles failures with no World and direct startup URLs;
  listen hosts ignore ordinary client departures. No custom reconnect loop or
  automatic retry added, and the Play/Multiplayer layout is unchanged.
  UE 5.8 editor build **PASS**. `Tools/TestConnectionRecovery.ps1` **PASS** with real
  local processes: joined client recovers after host termination; an unavailable
  join retains its address; one subsequent attempt joins a live host with no stale
  widget/game-instance error. Existing 4v4 regression **PASS**, including bot join
  replacement, disconnect refill, host-only controls and **21** balanced roster samples.
  General gameplay suite finished **86/86 PASS** on the unchanged rerun. The first
  run was **85/86**, missing only the existing plain rocket-jump upward-velocity
  sample (0 versus >400); survival/health and overshield launch passed. Both launches
  sampled >1200 on the rerun. No rocket behavior or thresholds changed; a timing
  hitch is plausible but the cause is not established. First report preserved as
  `Saved/Logs/ConnectionGameplay-First.txt` and `ConnectionGameplay-FirstRun.log`.
  Visually verified the 1280x720 timeout retry screen, retained address, readable
  feedback and unclipped actions after a failed direct startup join. Screenshot:
  `docs/playtests/2026-10-27_connection-recovery.png`. Reports:
  `Saved/Logs/ConnectionRecovery-*.log`, `ConnectionRecovery-Report.txt`,
  `Connection4v4-Report.txt`, `SelfTest.txt`; the known batch run-end log append
  sharing messages remain. No eight-human, internet, NAT or dedicated-hosting claim.
- 2026-10-27 Codex: **Desktop build environment ready** at
  `C:\Users\addve\Documents\ChatGPT\Shieldbreak`. Installed and verified UE 5.8.4
  with debugging symbols at `C:\Users\addve\Epic Games\UE_5.8`; VS 2022/MSVC 14.44
  and SDK 26100 built ShieldbreakEditor successfully (exit 0, 103.17 s).
  Gameplay self-test **85/85 PASS**, reported 116.6 average FPS with four bots
  (not a controlled benchmark). Keel and Spire local server/client regressions
  both **PASS**, each with 30 joined game seconds and no arena reference errors.
  4v4 regression **PASS**, with host controls/remote rejection, join replacement,
  disconnect refill and all **21** roster samples balanced within 4v4 limits.
  Visually verified Home -> Play -> Multiplayer -> Done -> Practice -> Keel Slayer,
  then Escape -> Leave confirmation -> Home -> Exit at 1280x720. Unreal Editor
  also opened the project successfully. Per-user engine/file associations and
  desktop game/editor/playtest shortcuts configured; Windows `.uproject` launch
  tested. No assets edited; discarded only editor-generated Android settings.
  Full LFS integrity check passed earlier. About 39 GiB free; no personal files
  or games deleted. Test processes closed. Source gameplay/config unchanged.
  Reports: `Saved/Logs/SelfTest.txt`, `SelfTestRun.log`, `DesktopProjectFiles.log`,
  `DesktopNetwork*.txt`, `ArenaNetwork-*`, `4v4Network-*`, `DesktopVisual.log`,
  `DesktopEditor.log`. Batch run-end log append had file-sharing messages; the
  completed gameplay report and independent network checks above are the evidence.
  Internet hosting/eight-human tests and laptop-only files/preferences remain
  unverified. See `docs/DESKTOP_SETUP.md` for installed tools and setup caveats.
- 2026-10-26 Codex: **Desktop setup, partial** at
  `C:\Users\addve\Documents\ChatGPT\Shieldbreak`. Synced main through `a9ce29b`;
  installed standalone Git 2.56.0.windows.3 / LFS 3.8.1, initialized local LFS
  hooks and commit identity. Full asset `git lfs fsck` passed. Existing VS 2022
  17.14.43 has both required C++ workloads, MSVC 14.44 and SDK 26100; Epic Launcher
  is installed. Actual desktop hardware is 16 GB RAM / RTX 2080 Ti (the 8 GB
  laptop constraints above are historical). UE 5.8 is missing and C: has only
  about 25 GiB free; waiting for user storage choice. No files/games deleted,
  no engine build or gameplay/network tests run here. See `docs/DESKTOP_SETUP.md`.
- 2026-10-26 Codex: **host-only controls + 4v4 roster regression**. UE 5.8 build passed;
  general gameplay suite **85/85 PASS** with a MaxPlayers=16 URL (live session still eight).
  Server RPCs for SB_Bots/SB_Skill/SB_Map now authorize the local host on the authority.
  A real local client sent all three requests; all were rejected and host settings stayed intact.
  `Tools/Test4v4Network.ps1` passed: oversized limit/bot requests still yield balanced 4v4;
  a human joins and replaces a bot (2 humans + 6 bots), then disconnects and the slot refills
  (1 human + 7 bots). All **82 roster samples** kept four per team and at most eight total.
  Runtime cap and bot-slot calculation share `Game/ShieldbreakMatchRules.h` constants.
  Test helpers are opt-in only (-SBNetworkTest), with no test timers in ordinary games.
  Logs: `Saved/Logs/4v4Network-Server.log`, `4v4Network-Client.log`, `HostControlsSelfTest.log`;
  gameplay report: `Saved/Logs/SelfTest.txt`. See `docs/NETWORKING.md` for limits and commands.
  Eight-human/internet play, lobby flow and dedicated administration are not verified here.
- 2026-10-26 Codex: **Multiplayer browser navigation revision**. Play now contains only
  Multiplayer, Host Match, Practice and Back. Multiplayer lists the five maps with their supported
  modes; a second column selects the game type. Host/Practice open this browser before launch.
  Selections persist; Done/Back/Escape return to Play. Fixed doubled style/content padding found
  during visual QA so all maps and action buttons fit at 1280x720. UE 5.8 build passed; in-game
  checks covered the four-item Play screen, opening Multiplayer, the map/type list and unclipped
  layout. Full selection/return/launch click-through remains pending; the timed review auto-closed.
  Screenshot: `docs/playtests/2026-10-26_multiplayer-browser.png`.
  No gameplay/network code changed in this revision; the earlier 84-check and network results
  below remain historical, rather than newly rerun. See `docs/MAIN_MENU.md` for the current flow.
- 2026-10-26 Codex: **main menu + initial 4v4 foundations**. UE 5.8 build passed; gameplay
  self-test **84/84 PASS**, exit code 0. Keel and Spire listen-server/client regressions both
  **PASS**, with 30 game seconds joined on each map and no arena network-reference errors.
  Main menu offers Play (map/mode, Host/Practice), Join by address, graphics presets and Exit;
  Escape opens the in-match menu. Session configuration is eight human slots, with bots capped
  to remaining slots and removed before assigning an arriving human's team.
  Efficiency: no match-rule tick or arena/bot/objective creation in the front end; menu labels
  update on events instead of allocating text from per-frame bindings. PlayTest.bat explicitly
  opens a Keel Slayer match so the new default menu does not change its purpose.
  Visually inspected home/Play at 1280x720 and clicked Play/map cycling (Keel -> Bastion).
  Full Host/Join/Settings/Leave click-through, gamepad navigation, and eight-human/internet
  tests are still pending. See `docs/MAIN_MENU.md` and `docs/playtests/2026-10-26_menu.png`.
  Gameplay report: `Saved/Logs/SelfTest.txt`; run: `Saved/Logs/MenuGameplaySelfTest.log`.
  Bot run averaged **10.9 FPS** under this machine's current load; this is not a controlled
  benchmark and no gameplay FPS improvement is claimed. Network logs: `Saved/Logs/ArenaNetwork-*`.
  Earlier arena fix was pushed as `e5ed484`.
- 2026-10-25 Codex: baseline build succeeded and mechanics self-test **79/79 PASS** before changes.
  Reproduced the listen-server bug locally: `FNetGUIDCache::SupportsObject` rejected arena
  `StaticMeshComponent_*` movement-base references after a client joined. Fixed by spawning the
  local arena with a deterministic map-specific name and giving each collision piece a deterministic
  layout-index name; both actor and pieces are explicitly net-addressable before construction/registration.
  Geometry remains locally built, with no geometry replication added.
  `Tools/TestArenaNetwork.ps1 -Map Keel` and `-Map Spire` both **PASS**: client joins, both sides build
  the selected map, and the client runs for 30 game seconds without arena network-reference errors.
  These are headless functional tests, not visual or latency tests. Keel's client logged a saved-move
  backlog during startup on this 8 GB machine. Logs: `Saved/Logs/ArenaNetwork-<Map>-Server.log` and
  `ArenaNetwork-<Map>-Client.log`. Two stable-network-path checks added to the gameplay self-test.
  Post-fix gameplay suite **81/81 PASS**, exit code 0, with 4 bots averaging 28.6 FPS.
  Report: `Saved/Logs/SelfTest.txt`; run log: `Saved/Logs/ArenaFixSelfTestRun.log`.
- 2026-10-22 `ab39f6f` (objective modes `5116512`; maps `dd66cf7` had not landed on the PC yet), **all three objective
  modes PASS**: CTF 12/12 again, **KOTH 6/6** (bots actively play the hill, 29-23), **Oddball 8/8**. Bots also captured
  a flag in the CTF bot match.
- 2026-10-22 **first 2-window listen-server test** (`Tools/ListenTest.bat`): the client connected ("Welcomed by
  server") and weapons replicated. **One real issue** in the server log: the arena's runtime-built floor and wall
  pieces can't be referenced over the network (they aren't stably named/replicated, so the client can't resolve
  them). Not yet investigated further; this goes to the code session (`AShieldbreakArena`). Screenshots from that run:
  `docs/playtests/2026-10-22_listen_test_a.jpg`, `_b.jpg`. They also show the open lighting problem: walls in
  shadow render nearly black.
- 2026-10-21 `c49f9a7` (objective modes `5116512`), **CTF: 12/12 PASS**: both flags spawn home; walking over the enemy flag
  takes it and swaps to the Sidearm (which may fire); capture at your home flag scores +1 and sends the flag home; dying
  drops it; a defender's touch returns it; a dropped flag auto-returns at 15.0 s; 4 bots join and fight (4 kills in 75 s,
  0-0 on captures). KOTH and Oddball checks are written but not run yet (the first mode batch stopped after CTF; fixed in
  `ab39f6f`).
- 2026-10-21 `c8a8876` (mechanics pass `e85654d` + rocket self-damage `458b8d7`): compiled clean first time; self-test
  **78/79**. All new checks pass: spawn kit BR + Sidearm; walk 660 reached in <0.25 s; Sidearm body 12 / unshielded
  headshot kill; Rail Lance early release 66 (survives) / full charge kills; Aegis pickup equips, raised barrier stops
  hitscan (barrier 0.90) and lowering lets bullets through; Active Camo 29.7 s; arena has 2 Rail Lances, 1 Aegis, no
  SMG/Plasma pickups; rocket jump from full shield leaves 27 health and launches 1330 uu/s. The miss was the Power Jump
  speed check sampling a frame late at 17 FPS (1038 vs normal 830 = x1.25, i.e. ~1.5x height): the mechanic is right,
  the check now compares against the normal jump. 17.6 FPS with 4 bots.
- 2026-10-21 `58d28e5` (power weapons + Overshield `d1204c4`): self-test **59/59 PASS**. New checks: arena places
  2 snipers, 1 rocket launcher, 1 Overshield; sniper body shot 60, two kill, headshot kills through full shield;
  rocket spawns ahead, direct hit kills, shooter 700 uu away unharmed; rocket explodes on its own at 6.1 s;
  Overshield grants 140, decays 1.99/s, drains before the shield, orb respawns on its timer; rocket jump with an
  overshield survives and launches (see design question above). Red/blue mannequins visible in bot screenshots.
  13 FPS with 4 bots (first run after the build compiled new shaders and dropped to 6 FPS; one Vent-timing check
  flaked at that frame rate and passed on both later runs).
- 2026-10-21 `716fd64`: self-test **37/38** (new Team Slayer checks pass: teammates take no damage with
  friendly fire off, own explosives still hurt, enemies take damage). The one miss was a frame-rate-sensitive
  threshold in the Momentum Carry air-speed check, now relative to take-off speed. Third-person screenshots
  confirm red/blue mannequins, rifles in hand, running/crouch animation and ragdoll deaths. ~24 FPS with 4 bots.
- 2026-10-21 `2a586f6` (includes Team Slayer `dae211e`): self-test **34/34 PASS**, ~15 FPS with 4 bots on
  Intel Iris Xe at the auto-detected scalability.

## Known issues / backlog
- **Unification desktop results** are recorded above. A complete manual pass of the
  Settings/Controls screens and music remains separate from the automated tests.
- **Imported Lyra Blueprints were left in place.** `Weapons/B_Weapon`, `Weapons/GA_*`,
  `Weapons/*/B_*`, the Lyra anim blueprints, and `FootstepEffectTagModifier` log missing
  Lyra-type errors. Content here is Git LFS pointers, so inbound asset references could not
  be read. `docs/ASSET_CREDITS.md` and the packaging notes say anim modifiers and layers
  are still referenced by animations that the game loads. They were not deleted.
- **Performance:** the Iris Xe laptop history is about 15–35 FPS. The desktop (RTX 2080 Ti)
  reported about 116 FPS on main's last self-test. That number is not a result for this branch.
- **Bots:** locomotion is single-node sequence swapping. PR #2 added `UShieldbreakBodyAnimInstance`
  as a step toward a real anim blueprint; blend spaces and aim offsets are still open.
- **Arena art:** layouts are still code-built. Original weapon, armor, trim and world materials
  from PR #2 load by path when the assets are present and fall back to flat colours otherwise.
- **Multiplayer:** movement under latency, long sessions, NAT, matchmaking, dedicated hosting
  and eight-human play are not verified. Do not claim them.
- **Lighting:** the near-black shadowed-wall bug is addressed in config and in the arena fill
  light on this branch. Confirm it visually on the desktop; it was not re-run here.
