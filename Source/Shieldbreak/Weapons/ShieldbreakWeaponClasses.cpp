#include "Weapons/ShieldbreakWeaponClasses.h"

#include "Weapons/ShieldbreakDamageTypes.h"

AWeapon_BattleRifle::AWeapon_BattleRifle()
{
	Stats.DisplayName = NSLOCTEXT("Shieldbreak", "BattleRifle", "Battle Rifle");
	Stats.FireMode = EShieldbreakFireMode::Burst;
	Stats.BurstCount = 3;
	Stats.BurstInterval = 0.055f;
	Stats.TimeBetweenShots = 0.42f;
	Stats.Damage = 6.5f;
	Stats.bPrecision = true;
	Stats.HeadshotMultiplier = 10.f;
	Stats.BaseSpreadDegrees = 0.25f;
	Stats.SpreadPerShot = 0.1f;
	Stats.MaxSpreadDegrees = 0.6f;
	Stats.SpreadRecoveryPerSecond = 3.f;
	Stats.Range = 12000.f;
	Stats.MagazineSize = 36;
	Stats.MaxReserveAmmo = 108;
	Stats.ReloadTime = 2.f;
	Stats.ZoomFOV = 40.f;
	Stats.RecoilPitch = 0.35f;
	Stats.Color = FLinearColor(0.15f, 0.17f, 0.15f);
	Stats.TracerColor = FLinearColor(1.f, 0.85f, 0.4f);
	Stats.DamageType = UDamageType_Ballistic::StaticClass();
	Stats.MeshScale = FVector(0.7f, 0.1f, 0.14f);
}

AWeapon_SMG::AWeapon_SMG()
{
	Stats.DisplayName = NSLOCTEXT("Shieldbreak", "SMG", "SMG");
	Stats.FireMode = EShieldbreakFireMode::FullAuto;
	Stats.TimeBetweenShots = 0.066f;
	Stats.Damage = 4.f;
	Stats.bPrecision = false;
	Stats.HeadshotMultiplier = 1.f;
	Stats.BaseSpreadDegrees = 2.f;
	Stats.SpreadPerShot = 0.35f;
	Stats.MaxSpreadDegrees = 6.f;
	Stats.SpreadRecoveryPerSecond = 12.f;
	Stats.Range = 3500.f;
	Stats.MagazineSize = 60;
	Stats.MaxReserveAmmo = 180;
	Stats.ReloadTime = 2.f;
	Stats.ZoomFOV = 0.f;
	Stats.RecoilPitch = 0.15f;
	Stats.Color = FLinearColor(0.08f, 0.08f, 0.1f);
	Stats.TracerColor = FLinearColor(1.f, 0.7f, 0.3f);
	Stats.DamageType = UDamageType_BallisticLight::StaticClass();
	Stats.MeshScale = FVector(0.45f, 0.1f, 0.16f);
}

AWeapon_PlasmaRifle::AWeapon_PlasmaRifle()
{
	Stats.DisplayName = NSLOCTEXT("Shieldbreak", "PlasmaRifle", "Plasma Rifle");
	Stats.FireMode = EShieldbreakFireMode::FullAuto;
	Stats.TimeBetweenShots = 0.11f;
	Stats.Damage = 6.f;
	Stats.bPrecision = false;
	Stats.BaseSpreadDegrees = 1.2f;
	Stats.SpreadPerShot = 0.15f;
	Stats.MaxSpreadDegrees = 3.5f;
	Stats.SpreadRecoveryPerSecond = 6.f;
	Stats.Range = 4500.f;
	Stats.bUsesHeat = true;
	Stats.HeatPerShot = 0.075f;
	Stats.HeatCooldownPerSecond = 0.55f;
	Stats.OverheatLockout = 2.5f;
	Stats.MagazineSize = 0;
	Stats.MaxReserveAmmo = 0;
	Stats.ZoomFOV = 0.f;
	Stats.RecoilPitch = 0.1f;
	Stats.Color = FLinearColor(0.25f, 0.1f, 0.45f);
	Stats.TracerColor = FLinearColor(0.2f, 0.9f, 1.f);
	Stats.DamageType = UDamageType_Plasma::StaticClass();
	Stats.MeshScale = FVector(0.5f, 0.18f, 0.16f);
}
