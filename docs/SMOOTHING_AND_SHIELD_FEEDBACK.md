# Animation, movement presentation and shield-hit feedback

User direction: improve skipping in both the local player's view and other
characters, and show a visible shield response to bullet damage. This builds on
Claude's native body/first-person animation system, rather than replacing it.

## Animation changes

- Ground movement continuously blends the four authored directional clips. A
  turn changes weights instead of restarting a different clip at frame zero.
- Walk/jog/crouch gait changes retain normalized cycle phase. Aim pitch,
  directional weights and playback rate follow their targets with time-based
  smoothing. Movement inputs, speed and jump tuning are unchanged.
- An interrupted upper-body action cross-fades for 0.1 seconds from the previous
  action; it no longer cuts straight from one pose to another.
- Animation inputs tick after character movement and before body/arms evaluation.
  The shared locomotion/action clips preload on the first character spawn to
  reduce first-use synchronous animation loads during combat.
- First-person armor uses forearm plates; upper-arm plates were crossing the
  camera during full-body-derived melee/throw clips. The original third-person
  armor stays intact. Bespoke FPS animation authoring and foot IK remain future
  polish; this is a first smoothing pass, not a claim of AAA parity.

## Movement presentation

Character movement retains server authority and Unreal's saved-move prediction.
Client visual correction uses exponential smoothing with a 0.1-second location
window and 0.05-second rotation window. The old 92/140 cm correction thresholds
were small for 660 cm/s walking and 950 cm/s sprinting; bounds now use 256/384 cm.
Large corrections/teleports can still snap. This does not make clients authoritative.

The local crouch camera compensates capsule height changes and eases to its new
height. A small sphere sweep keeps the compensating offset from crossing static
geometry. Capsule collision changes immediately; the visual offset does not delay
the crouch action. Death-camera positioning is excluded.

## Shield response

`UShieldHealthComponent` emits a cosmetic, unreliable multicast only when actual
damage is absorbed by shield/overshield. The payload carries absorbed amount,
break and overshield state. Health-only damage, recharge and passive overshield
decay do not emit shield hits. Health and damage calculations are unchanged.

`UShieldbreakShieldVisual` applies the original additive Fresnel material
`/Game/Shieldbreak/Art/Materials/M_SB_ShieldImpact` to the body and armor: cyan on
normal hits, gold on overshield hits, brighter/longer on a break. Replicated shield
breaks provide fallback feedback if a cosmetic hit RPC drops. The overlay fades
and is removed after the pulse; it does not leave permanent extra mesh draws or
change collision. Shield damage briefly reveals camouflaged characters during
the pulse. First-person forearms receive the owner's feedback too.

Regenerate the material with Unreal's Python commandlet and
`Tools/create_shield_material.py`. It uses no downloaded textures or models.

## Verification

`Tools/SelfTest.bat` includes gait-phase continuity, interrupted-action hand
continuity, crouch/stand frame changes, shield overlay application/fade, break,
health-only and passive-decay checks. `-SBSelfTest -SBPresentationTest` runs just
the presentation checks. `-SBSelfTest -SBShieldShots` captures intact, hit, faded,
broken and overshield states. `-SBFPShots` retains the existing action captures.

`Tools/TestShieldFeedbackNetwork.ps1` uses real local host/client processes with
configurable simulated lag/variance (80/20 ms by default). It verifies four
matching absorbed-hit events, including break and overshield, and excludes
health-only/decay events. The probe is opt-in (`-SBShieldFeedbackTest`) and adds
no timers to ordinary matches. This tests delivery under local emulation; it
does not establish internet quality or measured visual smoothness under latency.

Actual desktop outcomes, remaining limitations and capture paths belong in
`COORDINATION.md`.

Editor captures can still show shader preparation. This pass does not establish
cold-cache frame pacing or packaged-build performance; shader/PSO profiling and
FPS-specific animation authoring remain separate polish work.
