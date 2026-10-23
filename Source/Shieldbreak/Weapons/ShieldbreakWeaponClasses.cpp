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

AWeapon_Sidearm::AWeapon_Sidearm()
{
	Stats.DisplayName = NSLOCTEXT("Shieldbreak", "Sidearm", "Sidearm");
	Stats.FireMode = EShieldbreakFireMode::SemiAuto;
	Stats.TimeBetweenShots = 0.24f;
	// 70 shield: six body shots strip it; then one headshot (x5) kills.
	Stats.Damage = 12.f;
	Stats.bPrecision = true;
	Stats.HeadshotMultiplier = 5.f;
	Stats.BaseSpreadDegrees = 0.2f;
	Stats.SpreadPerShot = 0.25f;
	Stats.MaxSpreadDegrees = 1.2f;
	Stats.SpreadRecoveryPerSecond = 4.f;
	Stats.Range = 6000.f;
	Stats.MagazineSize = 12;
	Stats.MaxReserveAmmo = 48;
	Stats.ReloadTime = 1.4f;
	Stats.ZoomFOV = 55.f;
	Stats.RecoilPitch = 0.6f;
	Stats.Color = FLinearColor(0.22f, 0.22f, 0.25f);
	Stats.TracerColor = FLinearColor(1.f, 0.85f, 0.45f);
	Stats.DamageType = UDamageType_Ballistic::StaticClass();
	Stats.MeshScale = FVector(0.26f, 0.05f, 0.1f);
	Stats.ArtMesh = FSoftObjectPath(FString(ShieldbreakArt::PistolMesh));
	Stats.ArtViewLength = 26.f;
	Stats.FireSound = FSoftObjectPath(FString(ShieldbreakArt::PistolFire));
	Stats.MuzzleFlash = FSoftObjectPath(FString(ShieldbreakArt::MuzzleFlashRifle));
}

AWeapon_RailLance::AWeapon_RailLance()
{
	Stats.DisplayName = NSLOCTEXT("Shieldbreak", "RailLance", "Rail Lance");
	Stats.FireMode = EShieldbreakFireMode::Charge;
	Stats.ChargeTime = 0.8f;
	// A full charge (200) kills from full shield + health (115); an early release does a third (66).
	Stats.Damage = 200.f;
	Stats.MinChargeDamageFraction = 0.33f;
	Stats.TimeBetweenShots = 0.6f;
	Stats.bPrecision = false;
	Stats.BaseSpreadDegrees = 0.f;
	Stats.SpreadPerShot = 0.f;
	Stats.MaxSpreadDegrees = 0.f;
	Stats.Range = 15000.f;
	Stats.MagazineSize = 3;
	Stats.MaxReserveAmmo = 0;
	Stats.ReloadTime = 0.f;
	Stats.ZoomFOV = 45.f;
	Stats.RecoilPitch = 1.5f;
	// No imported art yet: the distinctive glowing greybox reads as exotic tech.
	Stats.Color = FLinearColor(0.85f, 0.88f, 0.92f);
	Stats.TracerColor = FLinearColor(0.2f, 0.95f, 1.f);
	Stats.DamageType = UDamageType_Ballistic::StaticClass();
	Stats.MeshScale = FVector(0.6f, 0.07f, 0.09f);
}
