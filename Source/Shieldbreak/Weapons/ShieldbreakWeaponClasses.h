// The sandbox: Battle Rifle (precision burst), SMG (close range, bleeds health), Plasma Rifle (heat, strips
// shields and delays recharge), and the map power weapons Sniper Rifle and Rocket Launcher.
// Stats live in the constructors.

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

/** Power weapon: two body shots kill, a headshot kills through shields. Slow bolt, small magazine. */
UCLASS()
class SHIELDBREAK_API AWeapon_SniperRifle : public AShieldbreakWeapon
{
	GENERATED_BODY()

public:
	AWeapon_SniperRifle();
};

/** Power weapon: slow rockets with lethal splash and rocket-jump knockback. */
UCLASS()
class SHIELDBREAK_API AWeapon_RocketLauncher : public AShieldbreakWeapon
{
	GENERATED_BODY()

public:
	AWeapon_RocketLauncher();
};
