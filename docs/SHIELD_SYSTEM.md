# Shield system

Code: `Source/Shieldbreak/Shield/`. The component is already on `AShieldbreakCharacter`. There is no
Blueprint character and no Shield Shaping.

| File | What it is |
|---|---|
| `ShieldHealthComponent.h/.cpp` | Server-authoritative replicated shield, overshield and health. Overcharge Vent, Break Pulse, overshield decay, and per-damage-type multipliers. |
| `ShieldTuningData.h` | Numbers (shield 70, health 45, recharge delay, vent window, and the rest). Used as class defaults unless a data asset is assigned. |
| `ShieldbreakDamageType.h` | `ShieldDamageMultiplier`, `HealthDamageMultiplier`, `RechargeDelayExtension`. |

## What the player can do

- **Recharge.** After the delay with no damage, the shield refills.
- **Overcharge Vent.** Press Reload/Vent while the marker is in the window for a full shield and overshield. A miss restarts the delay.
- **Break Pulse.** Melee inside the short window after the shield pops.

Shield Shaping was removed. The SMG and Plasma Rifle were removed. `UDamageType_Plasma` is still a
damage type with a higher shield multiplier and a recharge-delay extension; nothing in the sandbox
spawns a plasma weapon. The self-test still checks that damage type.

## Rules

1. Overshield absorbs damage first, then shield, then health. Damage left after the shield breaks carries into health.
2. A hit resets the recharge delay, plus any `RechargeDelayExtension` on the damage type.
3. Vent and Break Pulse are stamped with the client's estimate of server time. The server accepts that stamp only inside a short window.
4. Send damage through `ApplyDamage` / `ApplyPointDamage` / `ApplyRadialDamage`. The component listens to `OnTakeAnyDamage`. Calling `ApplyShieldbreakDamage` as well would count the damage twice.

Medals read `WasKilledByHeadshot()` and `GetKillingDamageType()` after death. See `docs/DESIGN.md` for the numbers.
