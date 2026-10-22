# Working Together: Shieldbreak sessions

## Handoff (2026-10-22): start here
**Work branch: `claude/project-thread-iqwexu` (PR #2 into `main`).** It holds everything: `main` (PR #1) plus every
commit from the old branch `claude/peaceful-hamilton-xcl6c8` (up to `887e99d`) plus the fixes below. Don't use the old
branch any more. PR #2 is not merged; Vincent decides when.

**State.** Competitive Halo-2-style FPS in UE 5.8 C++ (code-built greybox; Lyra art in `Content/` via Git LFS). Shields
with recharge + Vent / Break Pulse; BR + Sidearm spawn kit; Sniper, Rockets, Rail Lance (charge), Aegis (frontal
barrier), Active Camo, Overshield, frags; Power Jump, sprint/slide; Team Slayer, CTF, KOTH, Oddball with bots; five
launch maps (Keel, Bastion, Sanctum, Creekbed, Spire) plus the Crucible test map, defined in
`Design/maps/shieldbreak_maps.py` (generates `Game/ShieldbreakMapsData.cpp`). Design: `docs/DESIGN.md`, maps: `docs/MAPS.md`.

**Last laptop results (2026-10-22, details under "Verified on the PC"):** clean build; self-test 83/83; CTF 13/13,
KOTH 7/7, Oddball 9/9; map tour passes on every map (Keel, Bastion, Sanctum 7/7, Creekbed, Spire 9/9, Keel CTF, Bastion
KOTH); listen-server test clean (0 net warnings on server and client, client viewmodel correct).

**Next steps, in order:**
0. Build and check the new main menu and Esc menu (see "Needs verification").
1. Build `HEAD` and look at any map run's screenshots: the sky light is now 6 (was 1.5) and motion blur is off. Shaded
   walls went from black to grey in the comparison (`docs/playtests/2026-10-22_lighting_sky1.5_vs_sky6.jpg`), but a few
   faces were still dark. If they still look too dark, try `-SBSky=10` (and `-SBFill=0`) on the command line first.
2. Roadmap items 4-5 below: screenshots/FPS per map and mode. Art is in progress in the "Weapon and character art"
   thread (branch `claude/project-thread-o2y3iw`, based on this one): original guns and armor, scripted in Blender.
3. Backlog: proper anim BP for the mannequin, the environment art kit once Vincent has playtested the map layouts,
   per-map feedback from Vincent.

**How to run things on the laptop** (Windows, UE at `%USERPROFILE%\Epic Games\UE_5.8`, repo at
`C:\Users\vince\development\Shieldbreak`, 8 GB RAM so builds compile one file at a time): `Tools/SelfTest.bat`,
`Tools/SelfTestModes.bat [modes]`, `Tools/MapTour.bat`, `Tools/ListenTest.bat`, `Tools/BuildEditor.bat`. Reports land in
`Saved/Logs/SelfTest*.txt`. A cloud agent can't run Windows binaries; it can write a .bat and start it from File Explorer.

---

Shieldbreak was built by two Claude sessions: a cloud code session and a PC session on Vincent's laptop. They can't
share a chat, so this file is the shared to-do list and the rules of the road. Read it before starting work, and
update it in the same commit as the work.

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
- (none) Next PC run: build `claude/project-thread-iqwexu` (PR #2: maps + the arena networking fix + the map tour), then
  `Tools/SelfTest.bat`, `Tools/SelfTestModes.bat`, `Tools/MapTour.bat` and `Tools/ListenTest.bat`. The 2026-10-22 results
  below were recovered from the PC session's chat after the project moved.

## Needs verification (code session → PC)
- **Main menu and Esc menu** (commit "Main menu and in-game Esc menu"). A bare `-game` launch (or `PlayTest.bat` without
  `-SBAutoShot`) should open the menu: pick map/mode/bots/skill, PLAY starts the match, HOST LAN GAME starts it with
  `?listen`, JOIN connects to the typed IP. In a match, Esc opens Resume / Leave match / Quit; Leave match returns to the
  menu. Both menus also have Sensitivity / Field of view / Invert look (try them in a match). The Crucible self-test
  gains "Main menu launch rules" (4 checks). Screenshot the menu once: `-game -SBMenuShot=6 -SBAutoQuit` saves
  `Saved/Screenshots/AutoShot/menu.png`.
- **Listen-server fix: arena pieces are net addressable** (commit "Arena: stable names for networking"). The arena now
  spawns as `ShieldbreakArena_<MapId>` on every machine and its pieces are named `Piece_0..N` in layout order, all marked
  net addressable (`SetNetAddressable`), so the floor a character stands on can travel as its movement base.
  - New self-test step "Arena pieces are net addressable" (small edit to your file). The PC-side code session had
    written the same fix (`78b3f15`); the two are merged into one, and `Tools/MapTour.bat` (`a9d8f90`) rides along.
  - Also re-run the Power Jump check: its "compare against a normal jump" fix has not been run since it landed.
- **Listen-server fix: the client's own gun lay sideways** (`listen_test_b.jpg`). On the owning client the weapon could be
  attached to the mannequin's hand (the layout for other players) before the client knew it controlled the pawn, and
  nothing re-attached it. `AShieldbreakWeapon::NeedsReattach` now compares the layout OnEquipped would pick (hand,
  camera with art, plain camera) with the last one, and the character's Tick re-attaches when it changes. Please check
  the client's viewmodel in `Tools/ListenTest.bat` screenshots matches the host's.
  - Please re-run `Tools/ListenTest.bat`: the server log should no longer show
    `FNetGUIDCache::SupportsObject: StaticMeshComponent ... ShieldbreakArena_0.StaticMeshComponent_0 NOT Supported`,
    and the client should walk, jump and ride ramps without stutter. Also try it on Spire (`?Map=Spire`) for the lifts.
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

## Bugs found by the PC session (code session to fix)
- **Multiplayer (first 2-client listen-server test, 2026-10-21, `Tools/ListenTest.bat`).** The client joins
  ("Welcomed by server"), sees the arena, bots and the HUD, and plays. Found:
  1. (Fixed and verified 2026-10-22 by `bb69ab9`; `97647c5` adds a second guard, untested yet.) **Client first-person weapon is mis-oriented**: on the client the held Battle Rifle viewmodel lies sideways/rotated
     across the bottom of the screen (`Saved/Screenshots/AutoShot/client_01.png`); the host's viewmodel is correct
     (`server_01.png`). Probably the viewmodel attach/art transform is applied before the replicated weapon's art exists,
     or uses a server-only path (`OnRep_CurrentWeapon` / `RefreshEquippedVisuals` on a remote-owned pawn).
  2. (Fixed and verified 2026-10-22.) **Arena components aren't net-addressable**: the server logs 56x `FNetGUIDCache::SupportsObject: StaticMeshComponent
     ...ShieldbreakArena_0.StaticMeshComponent_N NOT Supported`. Characters standing on arena pieces replicate their
     movement *base*, which the client can't resolve, so based movement corrections break (client log: 5x `CreateSavedMove:
     Hit limit of 96 saved moves`). Fix: build the layout identically on server and client and call
     `SetNetAddressable()` on each piece (stable names), or make the pieces a non-replicated actor built on both sides.
     Relevant to the new map framework.
  3. *(Fixed in `97647c5`: re-attach once BodyAnimator sets the mesh.)* Minor: client `GetSocketInfoByName(weapon_r): No SkeletalMesh for CharacterMesh0` (weapon attached before the
     mannequin mesh is set on the client).
  - Noise, mine to clean up: Lyra's `FootstepEffectTagModifier` anim-modifier Blueprint fails to compile (needs Lyra C++);
    harmless, I'll remove it from `Content/`.

## Design questions from the PC session (code session to decide)
- (Answered by the code session: self-damage scaled; see Needs verification.) **Rocket jumps kill you without an overshield.** A rocket at your own feet does the full 220 (shield 70 +
  health 45 = 115), so the rocket jump is only survivable with an overshield (then: 34.9 health left and a
  ~2000 uu/s launch, which is very high). If rocket jumping should be a real skill move, consider scaling
  self-damage from your own rockets (e.g. 0.5x, as many arena shooters do) and/or lowering `KnockbackUp` for the
  shooter. Self-test currently checks the overshield case and logs the plain case as INFO.
- **Test harness note:** `AAIController::UpdateControlRotation` zeroes pitch unless focused on a pawn. Bots override
  it, so they're fine; self-test shooters now freeze their AI tick before aiming.

## Verified on the PC
- 2026-10-22 `a29d7cf` (art branch with this branch merged), run through Remote Control. Clean build. Crucible self-test
  **92/93**: the miss was the art thread's armor check (fixed in their `f8d78bc`). Menu launch rules and settings clamp
  pass. Keel 10/11 (same armor check), 21.2 FPS. The main menu screenshot fits at 1280x720.
  - Lumen on vs off on Keel: **21.4 vs 20.0 FPS** (no gain), but without Lumen shaded walls are soft blue-grey instead of
    black, so Lumen is now off by default (`r.DynamicGlobalIlluminationMethod=0`, SSR reflections).
- 2026-10-22 `d0afb80` → `5a4a19a`, run by the code session through the laptop (`Saved/RunNext.bat`,
  `RunSanctum.bat`, `RunLighting.bat`). Clean builds.
  - Map tour: **Spire 9/9** (26.9 FPS; all four gravity lifts launch now), **Bastion 5/5** (23.6 FPS; the 7.9 was a
    one-off), **Sanctum 7/7** (17.3 FPS) after two fixes: the plinth check's dummy walked off the far side after landing,
    and a plain jump *did* reach the 280 plinth (feet z 282: falling characters step up ledges within MaxStepHeight 45).
    The plinth is now 320, and the test checks both that a plain jump fails and that a Power Jump lands.
  - `Tools/ListenTest.bat`: **0** "NOT Supported" warnings on server and client.
  - Lighting comparison on Keel (sun/fill/sky): 8/1.5/1.5 shaded walls black; **8/1.5/6 readable grey (now the
    default)**; 8/5/1.5 darker still (exposure adapts to the brighter faces); 4/3/4 still black. The fill light's
    "Multiple directional lights are competing" on-screen warning is gone (the sun has forward-shading priority).
- 2026-10-22 `bb69ab9` (PR #2: maps + arena networking fix + map tour + client viewmodel fix), run by the code session
  through the laptop (`Saved/RunPR2.bat`). **Compiled clean first time** (maps `dd66cf7` included).
  - Self-test (Crucible) **83/83**, incl. the new "Arena pieces are net addressable" (0 of 32 unstable) and the Power Jump
    check (now passes). CTF **13/13**, KOTH **7/7**, Oddball **8/9**: the stuck-bot check flagged the Oddball carrier
    standing still at its base with both escorts on top of it (fixed in code: carrier moves around its base, escorts
    spread out; needs a re-run).
  - Map tour: **Keel 5/5** (16.5 FPS), **Bastion 5/5** (7.9 FPS, the slowest map: look at its draw cost), **Sanctum 5/5**
    (21.8), **Creekbed 5/5** (22.1), **Spire 5/9** (19.2): all 4 gravity lifts failed, the test dummy stayed on the pad
    (a begin-overlap didn't fire for a character already inside the trigger; fixed in code: the lift also polls the
    pad every 0.1 s; needs a re-run). **Keel CTF 15/15** (21.0 FPS), **Bastion KOTH 9/9** (21.9 FPS). No bot stuck on
    any map in Slayer.
  - Re-run after the fixes (`c5f9ca1`): **Oddball 9/9** (no bot stuck). **Spire 7/9**: lifts 3-4 (to the bridge) land
    within 4 uu, but lifts 1-2 (base front to base top) clip the base's front face (landed 340 uu short, back on the
    floor). Fixed in code: the arc's clearance scales with the climb (0.6x the height, min 150); needs one more Spire run.
  - **Listen test: fixed.** The server log has **0** `FNetGUIDCache ... NOT Supported` lines (was 56), the client has
    **0** `Hit limit of 96 saved moves` (was 5), and the client's Battle Rifle now sits correctly in first person
    (`docs/playtests/2026-10-22_listen_test_c_client.jpg`, on Keel).
- 2026-10-21 `ab39f6f` (objective modes `5116512`): **all modes pass. CTF 12/12, KOTH 6/6, Oddball 8/8.**
  CTF: flags spawn home; taking the enemy flag swaps to the Sidearm (which may fire); capture at your home flag +1 and the
  flag goes home; dying drops it; a defender's touch returns it; auto-return at 15.0 s. KOTH: one team alone scores +3 in
  3.5 s and controls the hill; with both teams inside it scores nobody and shows contested (254). Oddball: pickup, carrier
  can't shoot, +4 in 3.5 s while held, dying drops it, a dropped ball scores nothing and resets at 29.6 s. Bots play the
  objectives: after 75 s CTF 1-0 (a real capture), KOTH 29-23, Oddball 35-28. 20-27 FPS with 4 bots. Screenshots:
  `Saved/Screenshots/SelfTest/{ctf,koth,oddball}_N.png`. SMG/Plasma: the self-test no longer names `AWeapon_SMG` or
  `AWeapon_PlasmaRifle` (Shield Bleed uses `UDamageType_Plasma`; the arena check accepts only the sandbox classes), so you
  can delete those classes. Keep `UDamageType_Plasma`, or tell me if it goes.
- 2026-10-22 **first 2-window listen-server test** (`Tools/ListenTest.bat`): the client connected ("Welcomed by
  server") and weapons replicated. **One real issue** in the server log: the arena's runtime-built floor and wall
  pieces can't be referenced over the network (they aren't stably named/replicated, so the client can't resolve
  them). Not yet investigated further; this goes to the code session (`AShieldbreakArena`). Screenshots from that run:
  `docs/playtests/2026-10-22_listen_test_a.jpg`, `_b.jpg`. They also show the open lighting problem: walls in
  shadow render nearly black.
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
- **Performance:** ~15 FPS on the Iris Xe laptop. Lyra's 8K mannequin textures take ~3 min to build on
  first launch. Vincent may move to his desktop.
- **Bots:** locomotion is single-node sequence swapping; a proper anim BP (blend spaces, aim offsets)
  would look much better (code session).
- **Plasma Rifle:** still greybox; no imported plasma art.
- **Arena:** still the cube-built greybox; Phase 2 of `ART_DIRECTION.md` (modular kit) not started.
- **Multiplayer:** a client joins a listen server, weapons replicate, the arena pieces resolve over the network and
  the client's viewmodel is right (all verified 2026-10-22).
- **Lighting:** shaded walls were black; sky light 6 fixed most of it (2026-10-22), a few faces still look dark.
