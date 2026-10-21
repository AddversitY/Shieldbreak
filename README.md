# Shieldbreak
Create a FPS in UnrealEngine 5 that mimics Halo2/3 simplicity but adds mechanics that are new and intuitive. I want to retain that shield recharge mechanic, and add new cool but complex and hard to master mechanics that make the game easy to pick up but almost impossible to master.

## Start here
1. [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md): toolchain, project creation, first milestone.
2. [docs/DESIGN.md](docs/DESIGN.md): design pillars and the advanced "mastery" mechanics.
3. [docs/SHIELD_SYSTEM.md](docs/SHIELD_SYSTEM.md): wiring the C++ shield component into your character.
4. [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): code structure of the playable prototype.
5. [docs/ART_DIRECTION.md](docs/ART_DIRECTION.md): style guide and the asset import plan.

## Testing
- `Tools\SelfTest.bat` builds the editor target and runs a scripted gameplay self-test (`-SBSelfTest`):
  shields, recharge timing, Shield Bleed, Shaping, Overcharge Vent, Break Pulse, Battle Rifle hitscan and
  headshots, sprint/slide/Momentum Carry, grenade knockback, melee/assassination, and a 75 s bot match.
  Report: `Saved/Logs/SelfTest.txt` (one PASS/FAIL line per check, plus average FPS). Run it after any
  gameplay change; it needs no input and quits on its own (~6 minutes on a laptop).
- `PlayTest.bat` builds, plays for a few seconds and saves screenshots to `Saved/Screenshots/AutoShot`.
