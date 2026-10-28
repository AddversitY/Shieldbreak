# Handoff (2026-10-27)

Pickup note for the unified branch. The log of who verified what is `docs/COORDINATION.md`.
Design is `docs/DESIGN.md`. Code map is `docs/ARCHITECTURE.md`. The menu is `docs/MAIN_MENU.md`.

## Where the code is

- Repo `AddversitY/Shieldbreak`. This work is a branch off `main` that integrates PR #2
  (`claude/project-thread-iqwexu`) without merging that PR and without rewriting `main`.
- UE 5.8, C++. The front end is Slate. The HUD is Canvas. Enhanced Input actions are built in code.
- Desktop build machine: `C:\Users\addve\Documents\ChatGPT\Shieldbreak`, UE 5.8.4 at
  `C:\Users\addve\Epic Games\UE_5.8`, RTX 2080 Ti. The batch files look for
  `%USERPROFILE%\Epic Games\UE_5.8`. The laptop path and 8 GB notes are historical.
- This cloud session did not compile or launch the editor.

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

## Still open

- Lyra GAS blueprints and `FootstepEffectTagModifier` still log load errors. They were not deleted:
  the asset bytes are Git LFS pointers here, and the packaging notes say animations still reference
  the modifier.
- No matchmaking, NAT, dedicated server, or eight-human test has been run.
- Map environment art beyond the code-built layouts and the original trim/materials is still ahead
  (`docs/HANDOFF.md` used to point at Meshy; that account is still Vincent's decision, not started here).
