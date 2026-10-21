// The competitive sandbox (docs/DESIGN.md): spawn with the Battle Rifle and Sidearm; on the map, the Sniper
// Rifle, Rocket Launcher and Rail Lance. The SMG and Plasma Rifle are out of play (kept only until the
// self-test stops referencing them). Stats live in the constructors.

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

/** Spawn sidearm: semi-auto precision pistol for finishing shots; headshot kills an unshielded target. */
UCLASS()
class SHIELDBREAK_API AWeapon_Sidearm : public AShieldbreakWeapon
{
	GENERATED_BODY()

public:
	AWeapon_Sidearm();
};

/** Medium-power pickup: hold to charge (0.8 s, glow + whine tell), release for a one-hit-kill bolt. */
UCLASS()
class SHIELDBREAK_API AWeapon_RailLance : public AShieldbreakWeapon
{
	GENERATED_BODY()

public:
	AWeapon_RailLance();
};
