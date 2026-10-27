# Shieldbreak development handoff

Claude and Codex share this handoff. The integration branch is **main**; the earlier Claude
session branches have been merged. Read this before starting work, and update it in the same
commit as the work. See `AGENTS.md` for the user's standing GitHub and 4v4 instructions.

| Session | Can do | Can't do | Owns |
|---|---|---|---|
| **PC session** (linked to Vincent's laptop) | Build with UE 5.8, run the game, self-test, screenshots, editor/Python asset work, Epic launcher / Fab downloads, Git LFS content pushes | Fast iteration (each build + test is minutes on an Iris Xe laptop) | `Content/`, `Tools/`, `Source/Shieldbreak/Test/`, `PlayTest.bat`, build/perf config, `docs/ASSET_CREDITS.md` |
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
- 2026-10-26 Codex desktop setup: storage now approximately 127 GiB free; no
  files deleted by this session. Epic UE 5.8.4 installation configured with
  debugging symbols at `C:\Users\addve\Epic Games\UE_5.8` (83.309 GB per launcher).
  Install is waiting on the user's Windows UAC approval; engine directory is
  not yet created. Build, self-test and local network verification remain pending.
- No files currently reserved for this increment. Host-only controls and eight-slot runtime
  enforcement are completed and tested; see the latest verification record and `docs/NETWORKING.md`.
- Standing instruction: update this handoff and push each completed increment so Claude can
  resume. Menu revisions are already on main (`07acef0`, `d027a22`). No overnight automation is active.

## Needs verification (code session → PC)
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
  - Arena: plasma spots are now Rail Lances (60 s); an Aegis sits at (0,-800) (45 s). SMG/Plasma classes stay in code
    only for your Shield Bleed checks; tell me when I can delete them.
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
- **Imported animation modifier:** FootstepEffectTagModifier still references missing Lyra
  types and logs Blueprint/compiler errors. Confirmed in the earlier menu gameplay log and
  the current network run. Gameplay checks pass; clean up the imported editor metadata separately.
- **Performance:** ~15 FPS on the Iris Xe laptop. Lyra's 8K mannequin textures take ~3 min to build on
  first launch. Vincent may move to his desktop.
- **Bots:** locomotion is single-node sequence swapping; a proper anim BP (blend spaces, aim offsets)
  would look much better (code session).
- **Plasma Rifle:** still greybox; no imported plasma art.
- **Arena:** still the cube-built greybox; Phase 2 of `ART_DIRECTION.md` (modular kit) not started.
- **Multiplayer:** arena movement-base network references fixed and verified on Keel and Spire
  (2026-10-25; see "Verified on the PC"). Movement under latency, map travel, and longer multiplayer
  sessions still need coverage.
- **Lighting:** walls in shadow render nearly black (see `docs/playtests/`).
