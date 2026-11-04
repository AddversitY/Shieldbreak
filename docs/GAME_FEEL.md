# Combat feel: no recoil, aim assist, hit markers, shield pop (2026-11-03)

Vincent's call: **no recoil**, and **Halo-style aim assist**. This pass also makes hits and shield pops read clearly.

## No recoil
- Shots never move the aim. `AShieldbreakWeapon::ApplyRecoil` and the character's eased view kick
  (`AddViewKick`) are gone.
- `RecoilPitch` is now cosmetic only: the gun model jumps in the first-person hands, and other players' upper
  bodies rock with the shot. Spread (the BR's small bloom, for example) is unchanged.
- Self-test: "Firing never moves the aim (no recoil)", two BR bursts with control rotation unchanged.

## Aim assist (setting: Gameplay > Aim assist, on by default; `sb.AimAssist 0` turns all of it off)
1. **Bullet magnetism** (every input, human players only; bots keep their own aim model).
   `AShieldbreakWeapon::ApplyMagnetism` runs on the shooter's machine after spread and before `ServerFireShot`.
   A shot that already hits a character is left exactly as aimed. A near miss on an enemy's body (the hips to the
   neck, not the head, so headshots still need aim) bends onto it, just inside the edge it passed. The extra reach
   is `MagnetismDegrees` wide, capped at 40 cm, at full strength out to half of `MagnetismRange` and fading to zero
   at it. The bent path must be clear of walls. The server's direction check allows `MagnetismDegrees` more.

   | Weapon | MagnetismDegrees | MagnetismRange |
   |---|---|---|
   | Battle Rifle | 1.1 | 70 m |
   | Sidearm | 1.3 | 45 m |
   | Sniper Rifle | 0.5 | 120 m |
   | Rail Lance | 0.7 | 90 m |
   | Rocket Launcher | none (projectile) | |

2. **Slowdown** over an enemy (the existing friction). The stick slows to 45% dead on, the mouse to 75%.
3. **Tracking pull** (controller only, unchanged): the view eases along with a strafing target while the stick
   or movement is in use. Mouse players never get pulled.

Self-tests: "Bullet magnetism bends a near miss onto the enemy" (plus a wide miss, a direct hit, and assist off),
"Aim slowdown works with a mouse" (strength above 0.5 with no view pull), and the existing controller tracking test.

## Hit markers
- Every damage source now confirms hits from one place, `UShieldHealthComponent::ConfirmHit` (server), so
  rockets and grenades get markers too, not only bullets and melee. Hurting yourself or a teammate gives none.
- `ClientHitConfirmed(uint8 Flags)`, with `EShieldbreakHitFlags` Kill, Headshot (killing), Shield and ShieldBreak.
- HUD: the X pops out about a third bigger and settles while fading over 0.28 s. It is white on health, shield blue
  on a shielded hit, bigger and red on a kill. A shield pop adds a blue ring bursting outward (0.35 s).
- Sounds (synthesized, no assets): the old hit tick on health, a glassier tick on shield (`ShieldHitMarker`), a
  bright crack when you pop a shield (`ShieldBreakMarker`), and the kill tone.

## Shield pop
- Richer pop sound (`ShieldPop`): an electric crack, a falling two-tone note, and a low thump.
- Other players see sparks and a shell of shield energy burst outward from the body and fade (0.3 s). It is one
  reused engine sphere per character with `M_SB_ShieldImpact`. It has no collision and no shadow, it is hidden from
  its owner, and it only draws while bursting.
- Your own shield popping flashes the view blue for 0.3 s (one HUD tile).
- Self-test: "Hit markers report shield hits, shield pops and kills" (also checks the burst and self-damage).

## Performance
The per-frame cost is one actor loop over the 8 players for slowdown, as before. Magnetism adds one trace per shot
and at most one more per nearby candidate. The pop shell draws for 0.3 s per pop. FPS was measured on the desktop
before and after; see `COORDINATION.md`.
