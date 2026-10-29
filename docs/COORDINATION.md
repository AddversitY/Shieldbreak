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
- 2026-10-29 Claude (cloud): map cover pass, PR from `claude/project-thread-1xgiqk`. Touches only
  `Design/maps/`, `Design/map-planner/` outputs, the generated maps data and docs.
- 2026-10-27 Codex: map expansion/Spire modules/planner and the subsequent
  animation/movement-presentation/shield-hit pass are complete. See desktop
  verification records below. No files reserved; main is the shared handoff.

## Needs verification (code session → PC)
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
- Renderer: Lumen GI off (`r.DynamicGlobalIlluminationMethod=0`), reflections screen-space
  (`r.ReflectionMethod=2`), virtual shadow maps off, motion blur off. The arena also adds a
  shadowless fill light. This is the dark-walls fix. It has not been re-screenshotted here.

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
  controller tick the test freezes. Still open: work out whether real rocket jumps are broken or only the fixture.

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
