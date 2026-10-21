# Working Together: Two Claude Sessions

Two Claude sessions build Shieldbreak on the branch `claude/peaceful-hamilton-xcl6c8`. They can't share a
chat, so this file is the shared to-do list and the rules of the road. Read it before starting work,
and update it in the same commit as the work.

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

## In progress
- PC: self-test now covers Team Slayer friendly fire and takes third-person bot screenshots during the
  bot match (Saved/Screenshots/SelfTest).

## Needs verification (code session → PC)
- `d1204c4` **Power weapons + Overshield.** Please build and add self-test checks:
  - Sniper Rifle: a body shot does 60, so two kill from full; a headshot kills from full shield (`bHeadshotIgnoresShields`).
  - Rocket Launcher: firing spawns an `AShieldbreakRocket` 80 uu ahead; on impact, radial damage kills within ~180 uu and
    knocks the shooter up when fired at their feet (rocket jump); it explodes after 6 s if it hits nothing.
  - Overshield (`AShieldbreakPowerup` on tower level 1, (150,-150,420)): walking in sets overshield to 140, which decays
    at 2/s; the orb hides and respawns after 90 s.
  - Pickups: rockets at the tower top (120 s), snipers on both sky platforms (90 s), plasma rifles as before.
  - Mannequin tint now uses `TeamColor` + `EmissiveColor1-3`: red/blue teams should be visible in the bot screenshots.

## Verified on the PC
- 2026-10-21 `2a586f6` (includes Team Slayer `dae211e`): self-test **34/34 PASS**, ~15 FPS with 4 bots on
  Intel Iris Xe at the auto-detected scalability.

## Known issues / backlog
- **Performance:** ~15 FPS on the Iris Xe laptop. Lyra's 8K mannequin textures take ~3 min to build on
  first launch. Vincent may move to his desktop.
- **Bots:** locomotion is single-node sequence swapping; a proper anim BP (blend spaces, aim offsets)
  would look much better (code session).
- **Plasma Rifle:** still greybox; no imported plasma art.
- **Arena:** still the cube-built greybox; Phase 2 of `ART_DIRECTION.md` (modular kit) not started.
- **Multiplayer:** listen-server play with two real clients has never been tested.
