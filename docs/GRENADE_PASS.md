# Grenade pass (2026-11-05, branch `claude/project-thread-yp1u5u`)

Vincent: the grenade throw, the grenade and the explosion looked poor and buggy; the bar is AAA, Halo-like. No grenade
cooking, ever. Performance first.

## What was wrong
- First person: the whole arm rig pitched 35 degrees so Lyra's overhead toss crossed the screen. The gun swung across
  the view, the throwing (left) arm was hidden and there was no grenade in the hand.
- The grenade spawned at the press, before any arm moved. Throws had no cooldown (all four could leave at once) and
  the gun could fire mid-throw.
- The projectile was Lyra's static mesh with a red laser-like trail; the explosion was Lyra's burst plus a flat
  orange sphere.

## What it does now
- **Timing** (`AShieldbreakGrenade::GetReleaseDelay/GetThrowDuration`, `sb.GrenadeReleaseDelay` 0.15 s,
  `sb.GrenadeThrowTime` 0.6 s): the grenade is spent at the press and leaves at the release, aimed where you look
  then, from in front of the eye (pulled back out of walls). One throw per 0.6 s, no firing during it.
- **First-person throw** (`UShieldbreakBodyAnimator::UpdateFirstPersonThrow`): the gun tips down and rolls right
  around the hand (`sb.FPThrowDip`); the left arm is shown for the throw and driven by a two-bone IK
  (`FShieldbreakBodyAnimProxy::ApplyLeftArm`) along a camera-space path: up from below the view, cocked beside the
  head, snapped forward past the crosshair at the release, followed through and out of view. The fingers take Lyra's
  toss grip (`sb.FPThrowGripTime`), the wrist cocks and snaps (`sb.FPThrowWrist`), the grenade sits in the palm until
  the release, and the gun takes a small jolt as it goes. Tunables: `sb.FPThrowReach`, `sb.HeldGrenadePalm`.
  The left shoulder's camera-space position is logged at calibration for tuning the path.
- **Third-person throw**: Lyra's toss, its play rate set so the hand comes through at the release
  (`sb.TPThrowStart`, `sb.TPThrowRelease`); the grenade shows in the left hand until then. Throw whoosh at the hand.
- **Grenade**: original model `SM_SB_FragGrenade` (`Art/blender/build_grenade.py`, 1.8k tris, Weapons LODs): six
  fragmentation lobes, ribbed band, steel fuse crown with an amber arming light. It tumbles end over end in flight and
  rolls with its motion on the ground. The light blinks slowly in flight and faster and faster once it lands (small
  unshadowed point light, 160 cm). Bounce sounds scale with impact (audio thread's `GrenadeBounce`). No trail.
- **Explosion** (`AShieldbreakExplosionFX`, client-side, not replicated): 30k cd flash light (no shadows) with an
  orange afterglow, a hot core card, three flipbook fireball cards, five smoke cards rising and spreading for about
  3 s, a scorch decal that fades over 14 s, and a distance-scaled camera jolt (view only; aim never moves). Lyra's
  sparks/debris still play underneath (`sb.GrenadeLyraBurst`). Materials: `Tools/CreateGrenadeFX.bat`
  (`M_SB_FXFire`, `M_SB_FXSmoke`, `M_SB_Scorch` in `/Game/Shieldbreak/Art/FX`). FPS A/B: `sb.ExplosionCards 0`,
  `sb.ExplosionFlash 0`, `sb.ExplosionScorch 0`, `sb.ExplosionShake 0`.

## Self-test
"Grenade throw timing": no grenade before the release, grenade in hand meanwhile, no firing, exactly one grenade after
the release and an empty hand, a second press mid-throw ignored, the arming light blinks, and the explosion draws its
cards. Shots: `fp_3b_grenade_release_*` (around the release) and `tp_7_explosion_*` (0.03 to 3 s).
