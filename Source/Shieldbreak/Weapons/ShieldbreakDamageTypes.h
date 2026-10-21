// Concrete Shield Bleed damage types used by the weapon sandbox. Values are set in constructors
// so they can be tuned per class (or overridden by Blueprint subclasses).

#pragma once

#include "CoreMinimal.h"
#include "Shield/ShieldbreakDamageType.h"
#include "ShieldbreakDamageTypes.generated.h"

/** Rifles: neutral against shields and health. */
UCLASS()
class SHIELDBREAK_API UDamageType_Ballistic : public UShieldbreakDamageType
{
	GENERATED_BODY()

public:
	UDamageType_Ballistic()
	{
		ShieldDamageMultiplier = 1.f;
		HealthDamageMultiplier = 1.f;
	}
};

/** SMG-class bullets: weak into shields, strong into health. */
UCLASS()
class SHIELDBREAK_API UDamageType_BallisticLight : public UShieldbreakDamageType
{
	GENERATED_BODY()

public:
	UDamageType_BallisticLight()
	{
		ShieldDamageMultiplier = 0.8f;
		HealthDamageMultiplier = 1.25f;
	}
};

/** Plasma: strips shields and delays their recharge (Shield Bleed), weak into health. */
UCLASS()
class SHIELDBREAK_API UDamageType_Plasma : public UShieldbreakDamageType
{
	GENERATED_BODY()

public:
	UDamageType_Plasma()
	{
		ShieldDamageMultiplier = 1.6f;
		HealthDamageMultiplier = 0.5f;
		RechargeDelayExtension = 1.f;
	}
};

/** Grenades and rockets: neutral, a separate type so effects and the Aegis can tell explosions apart. */
UCLASS()
class SHIELDBREAK_API UDamageType_Explosive : public UShieldbreakDamageType
{
	GENERATED_BODY()

public:
	UDamageType_Explosive()
	{
	}
};

/** Melee: neutral. */
UCLASS()
class SHIELDBREAK_API UDamageType_Melee : public UShieldbreakDamageType
{
	GENERATED_BODY()

public:
	UDamageType_Melee()
	{
	}
};
