// Damage type carrying Shield Bleed properties. Weapons pass a subclass of this to
// UGameplayStatics::ApplyDamage / ApplyPointDamage; plain UDamageType is treated as neutral.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "ShieldbreakDamageType.generated.h"

UCLASS(Blueprintable)
class SHIELDBREAK_API UShieldbreakDamageType : public UDamageType
{
	GENERATED_BODY()

public:
	/** Plasma-style weapons > 1, ballistic < 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield")
	float ShieldDamageMultiplier = 1.f;

	/** Ballistic weapons > 1, plasma-style < 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield")
	float HealthDamageMultiplier = 1.f;

	/** Shield Bleed: extra seconds added to the victim's recharge delay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield")
	float RechargeDelayExtension = 0.f;

	/** Explosives etc. hit the whole shield regardless of shaping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield")
	bool bIgnoresShieldShaping = false;
};
