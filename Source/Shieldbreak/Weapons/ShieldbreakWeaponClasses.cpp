#include "Weapons/ShieldbreakWeaponClasses.h"

#include "Visuals/ShieldbreakArt.h"
#include "Weapons/ShieldbreakDamageTypes.h"
#include "Weapons/ShieldbreakRocket.h"

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
	Stats.Color = FLinearColor(0.28f, 0.32f, 0.26f);
	Stats.TracerColor = FLinearColor(1.f, 0.85f, 0.4f);
	Stats.DamageType = UDamageType_Ballistic::StaticClass();
	Stats.MeshScale = FVector(0.48f, 0.05f, 0.075f);
	Stats.ArtMesh = FSoftObjectPath(FString(ShieldbreakArt::RifleMesh));
	Stats.ArtViewLength = 50.f;
	Stats.FireSound = FSoftObjectPath(FString(ShieldbreakArt::RifleFire));
	Stats.MuzzleFlash = FSoftObjectPath(FString(ShieldbreakArt::MuzzleFlashRifle));
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
	Stats.Color = FLinearColor(0.2f, 0.2f, 0.24f);
	Stats.TracerColor = FLinearColor(1.f, 0.7f, 0.3f);
	Stats.DamageType = UDamageType_BallisticLight::StaticClass();
	Stats.MeshScale = FVector(0.38f, 0.07f, 0.11f);
	// Compact pistol-frame mesh until the SMG gets original art.
	Stats.ArtMesh = FSoftObjectPath(FString(ShieldbreakArt::PistolMesh));
	Stats.ArtViewLength = 30.f;
	Stats.FireSound = FSoftObjectPath(FString(ShieldbreakArt::Rifle2Fire));
	Stats.MuzzleFlash = FSoftObjectPath(FString(ShieldbreakArt::MuzzleFlashRifle));
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
	Stats.MeshScale = FVector(0.42f, 0.11f, 0.1f);
	// No imported plasma art: it keeps the purple greybox and the synthesized plasma sound so it reads as alien tech.
}

AWeapon_SniperRifle::AWeapon_SniperRifle()
{
	Stats.DisplayName = NSLOCTEXT("Shieldbreak", "SniperRifle", "Sniper Rifle");
	Stats.FireMode = EShieldbreakFireMode::SemiAuto;
	Stats.TimeBetweenShots = 0.7f;
	// 70 shield + 45 health = 115: two body shots kill; a headshot (x3) kills outright.
	Stats.Damage = 60.f;
	Stats.bPrecision = true;
	Stats.HeadshotMultiplier = 3.f;
	Stats.bHeadshotIgnoresShields = true;
	Stats.BaseSpreadDegrees = 0.05f;
	Stats.SpreadPerShot = 0.f;
	Stats.MaxSpreadDegrees = 0.05f;
	Stats.SpreadRecoveryPerSecond = 1.f;
	Stats.Range = 30000.f;
	Stats.MagazineSize = 4;
	Stats.MaxReserveAmmo = 8;
	Stats.ReloadTime = 2.6f;
	Stats.ZoomFOV = 18.f;
	Stats.RecoilPitch = 1.8f;
	Stats.Color = FLinearColor(0.12f, 0.12f, 0.14f);
	Stats.TracerColor = FLinearColor(1.f, 1.f, 0.9f);
	Stats.DamageType = UDamageType_Ballistic::StaticClass();
	Stats.MeshScale = FVector(0.7f, 0.05f, 0.08f);
	// Long rifle frame until the sniper gets original art.
	Stats.ArtMesh = FSoftObjectPath(FString(ShieldbreakArt::RifleMesh));
	Stats.ArtViewLength = 68.f;
	Stats.FireSound = FSoftObjectPath(FString(ShieldbreakArt::ShotgunFire));
	Stats.MuzzleFlash = FSoftObjectPath(FString(ShieldbreakArt::MuzzleFlashRifle));
}

AWeapon_RocketLauncher::AWeapon_RocketLauncher()
{
	Stats.DisplayName = NSLOCTEXT("Shieldbreak", "RocketLauncher", "Rocket Launcher");
	Stats.FireMode = EShieldbreakFireMode::SemiAuto;
	Stats.TimeBetweenShots = 0.9f;
	Stats.Damage = 0.f; // the rocket's explosion does the damage
	Stats.bPrecision = false;
	Stats.BaseSpreadDegrees = 0.f;
	Stats.SpreadPerShot = 0.f;
	Stats.MaxSpreadDegrees = 0.f;
	Stats.Range = 20000.f;
	Stats.MagazineSize = 2;
	Stats.MaxReserveAmmo = 4;
	Stats.ReloadTime = 3.f;
	Stats.ZoomFOV = 50.f;
	Stats.RecoilPitch = 2.5f;
	Stats.Color = FLinearColor(0.25f, 0.3f, 0.2f);
	Stats.TracerColor = FLinearColor(1.f, 0.6f, 0.2f);
	Stats.DamageType = UDamageType_Explosive::StaticClass();
	Stats.MeshScale = FVector(0.7f, 0.14f, 0.14f);
	Stats.ProjectileClass = AShieldbreakRocket::StaticClass();
	// Chunky shotgun frame until the launcher gets original art.
	Stats.ArtMesh = FSoftObjectPath(FString(ShieldbreakArt::ShotgunMesh));
	Stats.ArtViewLength = 70.f;
	Stats.FireSound = FSoftObjectPath(FString(ShieldbreakArt::ShotgunFire));
	Stats.MuzzleFlash = FSoftObjectPath(FString(ShieldbreakArt::MuzzleFlashRifle));
}
