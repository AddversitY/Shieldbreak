# Shield System

Code: `Source/Shieldbreak/Shield/`

| File | What it is |
|---|---|
| `ShieldHealthComponent.h/.cpp` | Replicated shield + health that runs on the server. Also handles Shield Shaping, Overcharge Vent, Break Pulse and Shield Bleed. |
| `ShieldTuningData.h` | Data asset holding every number. Create one in the editor to tune values without recompiling. |
| `ShieldbreakDamageType.h` | Damage type with Shield Bleed properties (shield/health multipliers, recharge-delay extension). |

The code assumes your game module is named `Shieldbreak` (it uses `SHIELDBREAK_API`). That is what the First Person template generates when the project is named Shieldbreak. If your module has a different name, rename the macro in the three headers.

## 1. Add the component to your character

In the template's `ShieldbreakCharacter.h`:

```cpp
class UShieldHealthComponent;

// inside the class:
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shield")
TObjectPtr<UShieldHealthComponent> ShieldHealth;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
TObjectPtr<UInputAction> ShapeAction;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
TObjectPtr<UInputAction> VentAction;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
TObjectPtr<UInputAction> MeleeAction;

void StartShaping() { ShieldHealth->SetShaping(true); }
void StopShaping()  { ShieldHealth->SetShaping(false); }
void Melee();
```

In `ShieldbreakCharacter.cpp`:

```cpp
#include "Shield/ShieldHealthComponent.h"

// constructor
ShieldHealth = CreateDefaultSubobject<UShieldHealthComponent>(TEXT("ShieldHealth"));

// SetupPlayerInputComponent, next to the template's existing BindAction calls
EnhancedInputComponent->BindAction(ShapeAction, ETriggerEvent::Started,   this, &AShieldbreakCharacter::StartShaping);
EnhancedInputComponent->BindAction(ShapeAction, ETriggerEvent::Completed, this, &AShieldbreakCharacter::StopShaping);
EnhancedInputComponent->BindAction(VentAction,  ETriggerEvent::Started,   ShieldHealth.Get(), &UShieldHealthComponent::TryVent);
EnhancedInputComponent->BindAction(MeleeAction, ETriggerEvent::Started,   this, &AShieldbreakCharacter::Melee);

void AShieldbreakCharacter::Melee()
{
	ShieldHealth->TryBreakPulse(); // only fires if your shield popped within the window
	// ...normal melee attack goes here
}
```

## 2. Create the input actions

1. In the Content Browser, go to the template's `Input/Actions` folder and create `IA_Shape`, `IA_Vent` and `IA_Melee` (all Digital/bool).
2. Add them to the template's `IMC_Default` input mapping context. Suggested keys:
   - Shape: Right Mouse / Left Trigger
   - Vent: R / X, shared with reload, so check `GetVentMarkerPosition() >= 0` first
   - Melee: V or F / Right Bumper
3. Assign them to the new slots on `BP_FirstPersonCharacter`.

## 3. Test it

- **Damage:** drop a **Pain Causing Volume** into the level (Place Actors panel). Walk in to drain shields, step out, and the recharge should start after 4.5 s.
- **Tuning:** right-click → *Miscellaneous → Data Asset → ShieldTuningData*, name it `DA_ShieldTuning_Default`, and assign it to the component's **Tuning** slot on `BP_FirstPersonCharacter`.
- **Multiplayer:** Play → Net Mode *Play As Listen Server*, 2 players.

## 4. HUD hooks

Bind these in a UMG widget (get the component from the owning pawn):

| Event / query | Use for |
|---|---|
| `OnShieldChanged`, `OnOvershieldChanged`, `OnHealthChanged` | Bar fill |
| `OnShieldBroken` | Shield-pop sound and screen flash |
| `OnRechargeStarted` | The Halo recharge whine |
| `GetVentMarkerPosition()` (call each tick) | Draws the vent marker; returns -1 when the marker is hidden |
| `OnVentResult` | Success/fail feedback |
| `OnBreakPulse` | Shockwave VFX on every client |
| `OnDeath` | Respawn flow |

## 5. Shield Bleed weapons

Create a Blueprint subclass of `ShieldbreakDamageType`, for example `DT_Plasma` with ShieldDamageMultiplier 1.5, HealthDamageMultiplier 0.5 and RechargeDelayExtension 1.0. Pass it as the damage type class in `ApplyPointDamage` / `ApplyRadialDamage`. Set `bIgnoresShieldShaping` on explosives.

## Damage rules

1. Overshield absorbs damage first, then shield, then health. Damage left over after the shield breaks carries into health.
2. While shaping, the shield takes ×0.6 from the front arc and ×1.5 from everywhere else. Health damage is not affected.
3. Any hit resets the recharge delay (4.5 s, plus the weapon's bleed, plus 1 s if you were shaping, plus any pending Break Pulse penalty).
4. Timing inputs (Vent, Break Pulse) are stamped with the client's estimate of server time. The server accepts that stamp only if it is up to 0.25 s old, so a laggy player still hits a 0.15 s window but cannot rewind further.

Send all damage through Unreal's normal `ApplyDamage` functions. Calling `ApplyShieldbreakDamage` directly as well would count the damage twice.
