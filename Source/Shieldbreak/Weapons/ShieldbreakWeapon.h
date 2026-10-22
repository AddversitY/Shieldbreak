// Hitscan weapon base. The owning machine runs fire timing, spread and cosmetics; the server
// validates every shot, traces ECC_Weapon and applies damage through UGameplayStatics.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Audio/ShieldbreakSFX.h"
#include "UObject/SoftObjectPath.h"
#include "ShieldbreakWeapon.generated.h"

class AShieldbreakCharacter;
class UDamageType;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EShieldbreakFireMode : uint8
{
	SemiAuto,
	Burst,
	FullAuto,
	/** Hold to charge, release to fire; only a full charge does full damage (Rail Lance). */
	Charge
};

USTRUCT(BlueprintType)
struct SHIELDBREAK_API FShieldbreakWeaponStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") EShieldbreakFireMode FireMode = EShieldbreakFireMode::SemiAuto;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float Damage = 10.f;
	/** Headshots on unshielded targets multiply damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") bool bPrecision = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float HeadshotMultiplier = 1.f;
	/** Sniper rule: headshots get the multiplier even through shields. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") bool bHeadshotIgnoresShields = false;
	/** When set, each shot spawns this projectile (it owns the damage) instead of a hitscan trace. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") TSubclassOf<AActor> ProjectileClass;
	/** Charge weapons: seconds to a full charge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float ChargeTime = 0.8f;
	/** Charge weapons: damage fraction of a shot released before full charge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float MinChargeDamageFraction = 0.33f;
	/** Between shots (auto/semi) or between bursts (burst). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float TimeBetweenShots = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") int32 BurstCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float BurstInterval = 0.06f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float BaseSpreadDegrees = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float SpreadPerShot = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float MaxSpreadDegrees = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float SpreadRecoveryPerSecond = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float Range = 10000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") int32 MagazineSize = 30;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") int32 MaxReserveAmmo = 90;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float ReloadTime = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") bool bUsesHeat = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float HeatPerShot = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float HeatCooldownPerSecond = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float OverheatLockout = 2.f;
	/** 0 = no zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float ZoomFOV = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") float RecoilPitch = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") FLinearColor Color = FLinearColor(0.2f, 0.2f, 0.2f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") FLinearColor TracerColor = FLinearColor(1.f, 0.85f, 0.4f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") TSubclassOf<UDamageType> DamageType;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") FVector MeshScale = FVector(0.6f, 0.1f, 0.12f);

	// ---- Imported art (optional; anything missing falls back to the greybox / synthesized sound) ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Art") FSoftObjectPath ArtMesh;
	/** First-person length (cm) the art mesh is scaled to along its barrel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Art") float ArtViewLength = 55.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Art") FSoftObjectPath FireSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Art") FSoftObjectPath MuzzleFlash;
};

UCLASS(Abstract)
class SHIELDBREAK_API AShieldbreakWeapon : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	const FShieldbreakWeaponStats& GetStats() const { return Stats; }

	AShieldbreakCharacter* GetOwningCharacter() const;
	/** Server: sets Owner and Instigator. */
	void SetOwningCharacter(AShieldbreakCharacter* NewOwner);

	/** Every machine: attach to the owner's camera and show / hide. */
	void OnEquipped();
	void OnUnequipped();
	/** True when the attachment OnEquipped would pick now (hand vs camera viewmodel, art or not) differs from the last one. */
	bool NeedsReattach() const;

	// ---- Local (owning client or server AI) ----
	void StartFire();
	void StopFire();
	void StartReload();
	bool CanReload() const;
	bool IsReloading() const { return bReloading; }
	/** 0..1 while reloading on the owning machine, else -1. */
	float GetReloadProgress() const;
	bool IsFiring() const { return bWantsToFire || BurstShotsRemaining > 0 || bCharging; }

	/** Charge weapons: charging now (every machine; drives the glow and sound tell). */
	bool IsCharging() const { return bCharging; }
	/** Charge weapons: 0..1 progress of the current charge on this machine. */
	float GetChargeFraction() const;
	/** Charge weapons: drop the charge without firing. */
	void CancelCharge();

	int32 GetAmmoInMagazine() const { return AmmoInMagazine; }
	int32 GetReserveAmmo() const { return ReserveAmmo; }
	float GetHeat() const { return Heat; }
	bool IsOverheated() const { return bOverheated; }
	bool HasAmmo() const;
	float GetCurrentSpreadDegrees() const;

	/** Server. */
	void AddReserveAmmo(int32 Amount);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FShieldbreakWeaponStats Stats;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> BarrelMesh;

	/** Charge weapons: glowing orb at the muzzle that grows while charging (visible to everyone). */
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> ChargeGlowMesh;

private:
	bool CanFireShotLocal() const;
	void FireShotLocal();
	FVector GetMuzzleLocation() const;
	FVector GetBarrelDirection() const;
	void ApplyArt();
	void PlayShotEffects(bool bLocalView);
	void SpawnImpactFX(const FHitResult& Hit) const;
	void ApplyRecoil();
	EShieldbreakSound GetShotSound() const;

	UFUNCTION(Server, Reliable) void ServerFireShot(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction);
	UFUNCTION(Server, Reliable) void ServerStartReload();
	UFUNCTION(Server, Reliable) void ServerSetCharging(bool bNewCharging);
	UFUNCTION() void OnRep_Charging();
	void ReleaseCharge();
	void UpdateChargeGlow();
	UFUNCTION(NetMulticast, Unreliable) void MulticastShotFX(FVector_NetQuantize End);

	void BeginReload();
	void FinishReload();
	float GetServerTime() const;

	UFUNCTION() void OnRep_Reloading();

	UPROPERTY(Replicated) int32 AmmoInMagazine = 0;
	UPROPERTY(Replicated) int32 ReserveAmmo = 0;
	UPROPERTY(Replicated) float Heat = 0.f;
	UPROPERTY(Replicated) bool bOverheated = false;
	UPROPERTY(ReplicatedUsing = OnRep_Reloading) bool bReloading = false;
	/** Skips the owner, who predicts it locally. */
	UPROPERTY(ReplicatedUsing = OnRep_Charging) bool bCharging = false;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BodyMID;

	enum class EEquippedLayout : uint8 { None, Hand, CameraArt, CameraPlain };
	EEquippedLayout GetDesiredLayout() const;
	/** What OnEquipped last attached as. */
	EEquippedLayout EquippedLayout = EEquippedLayout::None;

	// Imported art layout, derived from the mesh in ApplyArt().
	bool bHasArt = false;
	FName MuzzleSocket = NAME_None;
	FVector ArtCenterLocal = FVector::ZeroVector;
	FVector ArtBarrelLocal = FVector::ForwardVector;
	float ArtHalfLength = 30.f;
	float ArtViewScale = 1.f;
	FRotator ArtAlign = FRotator::ZeroRotator;

	// Local fire loop.
	bool bWantsToFire = false;
	bool bSemiAutoLatched = false;
	int32 BurstShotsRemaining = 0;
	float NextShotLocalTime = 0.f;
	float CurrentSpread = 0.f;
	float LastShotLocalTime = -100.f;
	float ReloadStartLocalTime = -1.f;
	float OverheatEndLocalTime = -1.f;
	float ChargeStartLocalTime = -1.f;

	// Server validation.
	float ServerShotTokens = 0.f;
	float LastServerTokenTime = -1.f;
	float LastServerShotTime = -100.f;
	float ServerOverheatEndTime = -1.f;
	float ServerChargeStartTime = -1.f;
	FTimerHandle ReloadTimerHandle;
};
