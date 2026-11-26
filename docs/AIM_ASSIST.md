# Aim assist (2026-11-26)

Classic console-shooter aim assist for human players, on controller **and** mouse, with separate strengths for each.
Bots never get any of it.

## The three effects

| Effect | What it does | Where |
| --- | --- | --- |
| Friction | Look speed slows while the crosshair is on or near an enemy | `Input_Look` / `Input_LookGamepad` |
| Magnetism | The view gently follows an enemy moving across the crosshair, only while the player is aiming or moving | `AShieldbreakCharacter::UpdateAimAssist` |
| Bullet magnetism | A shot that narrowly misses an enemy's body bends onto it (never onto the head, never round a wall) | `AShieldbreakWeapon::ApplyMagnetism` |

**Strength:** all three are scaled by one number (`GetAimAssistStrength`, 0..1). It is how close the crosshair is to
the enemy (inside a cone about a body's width either side of the chest), times the range falloff, the input
profile's strength and the weapon's scale.

**Targets:** only living, visible enemies with a clear line from the eye to the chest. The line-of-sight check blocks
assist through walls, and teammates are ignored.

**Never snaps:** magnetism is speed-limited (degrees per second) and needs look or move input within the last 0.15 s.
An idle player's view never moves.

**Which profile:** the controller profile applies for 2 s after any right-stick input; otherwise the mouse profile.

## Defaults

| | Controller | Mouse |
| --- | --- | --- |
| Strength | 1.0 | 1.0 |
| Friction (look speed on an enemy) | 0.45 | 0.75 |
| Magnetism (deg/s at full strength) | 9 | 5 |
| Bullet magnetism (scale on the weapon's cone) | 1.0 | 0.6 |

Targeting:
- cone radius 90 cm;
- range 60 m (or the weapon's, if shorter);
- full strength to 15 m, fading to 0.35 at 60 m.

| Weapon | Scale | Bullet magnetism cone |
| --- | --- | --- |
| Battle Rifle | 1.0 | 1.1 deg |
| Sidearm | 1.0 | 1.3 deg |
| Sniper | 1.0 scoped, 0.25 unscoped | 0.5 deg |
| Rail Lance | 0.7 | 0.7 deg |
| Rocket Launcher | 0 (none) | none |

## Tuning

- **Config:** Project Settings > Game > Shieldbreak Aim Assist, stored in `Config/DefaultGame.ini` under
  `[/Script/Shieldbreak.ShieldbreakAimAssistSettings]` (`UShieldbreakAimAssistSettings`).
- **Live, in the console:**
  - `sb.AimAssist 0` turns everything off.
  - `sb.AimAssist.ControllerStrength`, `...ControllerFriction`, `...ControllerPull` and `...ControllerBullet` tune
    the controller; the same four with `Mouse` tune the mouse.
  - Each is -1 by default, meaning "use the config".
  - Strength 0 turns that input's assist off; the other input keeps its own.
- **Players:** the Aim assist option in Settings still turns it off.
- **Per weapon:** `AimAssistScale` and `AimAssistUnzoomedScale` in the weapon stats, next to `MagnetismDegrees` and
  `MagnetismRange`.
- **Network:** bullet magnetism is computed on the shooting machine. The server's shot check allows the weapon's full
  cone, so the bullet scale is capped at 1.

## Self-test checks

Existing checks:
- `Controller aim assist tracks a nearby enemy`.
- `Aim slowdown works with a mouse` and `...and never moves an idle mouse player's view`.
- The bullet magnetism checks, including `...and nothing bends with aim assist off`.

New checks:
- `Mouse aim assist follows a nearby enemy`.
- `Mouse bullet magnetism bends a small miss, not a wider one`: 6 cm past the body bends with the mouse profile, 12 cm
  doesn't; the existing bullet magnetism checks now run as a controller player.
- `Aim assist ignores an enemy behind a wall`: a 4 x 4 m slab between player and enemy, so there is no slowdown and
  no pull.
- `sb.AimAssist 0 turns aim assist off`.
- `Mouse strength 0 turns off mouse aim assist` and `...while the controller keeps its own`.
- `Bots and explosives get no aim assist` and `...the rocket launcher has none, the sniper helps less unscoped`.

Not tested yet: how it feels in play with a real controller and mouse; the values above are starting points.
