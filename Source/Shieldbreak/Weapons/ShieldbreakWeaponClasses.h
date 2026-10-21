// The starter sandbox: Battle Rifle (precision burst), SMG (close range, bleeds health) and
// Plasma Rifle (heat, strips shields and delays recharge). Stats live in the constructors.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/ShieldbreakWeapon.h"
#include "ShieldbreakWeaponClasses.generated.h"

UCLASS()
class SHIELDBREAK_API AWeapon_BattleRifle : public AShieldbreakWeapon
{
	GENERATED_BODY()

public:
	AWeapon_BattleRifle();
};

UCLASS()
class SHIELDBREAK_API AWeapon_SMG : public AShieldbreakWeapon
{
	GENERATED_BODY()

public:
	AWeapon_SMG();
};

UCLASS()
class SHIELDBREAK_API AWeapon_PlasmaRifle : public AShieldbreakWeapon
{
	GENERATED_BODY()

public:
	AWeapon_PlasmaRifle();
};
