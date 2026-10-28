# Shieldbreak
Create a FPS in UnrealEngine 5 that mimics Halo2/3 simplicity but adds mechanics that are new and intuitive. I want to retain that shield recharge mechanic, and add new cool but complex and hard to master mechanics that make the game easy to pick up but almost impossible to master.

## Start here
1. [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md): toolchain, project creation, first milestone.
2. [docs/DESIGN.md](docs/DESIGN.md): design pillars and the advanced "mastery" mechanics.
3. [docs/SHIELD_SYSTEM.md](docs/SHIELD_SYSTEM.md): wiring the C++ shield component into your character.
4. [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): code structure of the playable prototype.
5. [docs/ART_DIRECTION.md](docs/ART_DIRECTION.md): style guide and the asset import plan.
6. [docs/MAPS.md](docs/MAPS.md): the launch maps with overview images, and how to switch maps.

## Main menu

A normal game launch opens the front end: Play, Join, Settings, Controls and Exit.
Play shows only Multiplayer, Host Match, Practice and Back. Settings and Controls are
full screens (graphics, audio, field of view, sensitivity, key rebinding). See
[docs/MAIN_MENU.md](docs/MAIN_MENU.md).

## Testing
- `Tools\SelfTest.bat` builds the editor target and runs the scripted gameplay self-test (`-SBSelfTest`):
  shields, recharge, Overcharge Vent, Break Pulse, the spawn kit (Battle Rifle and Sidearm),
  sniper, rockets, Rail Lance, Aegis, Active Camo, sprint/slide/Momentum Carry, Power Jump,
  grenades, melee, medals, objective modes, and a bot match.
  Shield Shaping is not in the game. The SMG and Plasma Rifle are not in the game.
  Report: `Saved/Logs/SelfTest.txt` (one PASS/FAIL line per check, plus average FPS). Run it after any
  gameplay change; it needs no input and quits on its own.
- `PlayTest.bat` builds, opens Keel Slayer, plays for a few seconds and saves screenshots to `Saved/Screenshots/AutoShot`.
- After building, `Tools\TestArenaNetwork.ps1 -Map Keel` runs a headless listen server and joining
  client and checks for arena network-reference errors. Repeat with `-Map Spire` for a non-default
  map. Logs: `Saved/Logs/ArenaNetwork-<Map>-Server.log` and `ArenaNetwork-<Map>-Client.log`.

- After building, `Tools/Test4v4Network.ps1` checks host permissions, the eight-player cap,
  bot replacement on join and refill on disconnect using a real local client. See
  [docs/NETWORKING.md](docs/NETWORKING.md) for the current network scope and test reports.
- `Tools/TestConnectionRecovery.ps1` checks lost-host recovery, a failed join retaining
  its address, and a successful local retry without stale error feedback.
