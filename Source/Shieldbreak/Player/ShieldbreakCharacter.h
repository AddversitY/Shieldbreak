// The Spartan-style player/bot pawn: greybox body, first-person camera, shield + health,
// two-weapon inventory, grenades, melee (with lunge and assassinations) and the input hooks
// for every mastery mechanic.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ShieldbreakCharacter.generated.h"

class AShieldbreakCarriable;
class AShieldbreakGrenade;
class AShieldbreakWeapon;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UShieldHealthComponent;
class UShieldbreakBodyAnimator;
class UShieldbreakMovementComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/** Map-pickup equipment, used with the Use Equipment input. */
UENUM(BlueprintType)
enum class EShieldbreakEquipment : uint8
{
	None,
	/** Frontal barrier that blocks bullets (not explosives or melee); no firing while raised. */
	Aegis
};

UCLASS(Config = Game)
class SHIELDBREAK_API AShieldbreakCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AShieldbreakCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void NotifyControllerChanged() override;
	virtual bool ShouldTakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) const override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// ---- Accessors ----

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	UShieldHealthComponent* GetShieldHealth() const { return ShieldHealth; }
	UShieldbreakMovementComponent* GetShieldbreakMovement() const;
	UStaticMeshComponent* GetHeadMesh() const { return HeadMesh; }
	UShieldbreakBodyAnimator* GetBodyAnimator() const { return BodyAnimator; }
	AShieldbreakWeapon* GetCurrentWeapon() const { return CurrentWeapon; }
	AShieldbreakWeapon* GetHolsteredWeapon() const;
	int32 GetGrenadeCount() const { return GrenadeCount; }
	int32 GetMaxGrenades() const { return MaxGrenades; }
	bool IsAlive() const;
	bool IsZoomed() const { return bZoomed; }
	FVector GetAimOrigin() const;
	FRotator GetAimRotation() const;
	float GetSpreadMultiplier() const;
	FLinearColor GetPlayerColor() const;

	// ---- Equipment and power-ups ----

	EShieldbreakEquipment GetEquipment() const { return Equipment; }
	bool IsAegisRaised() const { return bAegisRaised; }
	/** Owning client / server: remaining barrier strength and uptime, 0..1. */
	float GetAegisHealthFraction() const { return AegisMaxHealth > 0.f ? AegisHealth / AegisMaxHealth : 0.f; }
	float GetAegisUptimeFraction() const { return AegisMaxUptime > 0.f ? AegisUptime / AegisMaxUptime : 0.f; }
	UStaticMeshComponent* GetAegisMesh() const { return AegisMesh; }
	bool IsCamouflaged() const;
	/** Seconds of Active Camo left (server time based; 0 when none). */
	float GetCamoTimeRemaining() const;

	// ---- Objectives ----

	/** Flag or ball being carried, if any. Carriers move slower; flag carriers may only use the sidearm, ball carriers only melee. */
	AShieldbreakCarriable* GetCarriedObjective() const { return CarriedObjective; }
	/** Server, from AShieldbreakCarriable. */
	void SetCarriedObjective(AShieldbreakCarriable* NewObjective);
	/** Whether the current weapon may fire given what is being carried. */
	bool CanFireWhileCarrying() const;

	// ---- Actions (owning client input or server AI) ----

	void StartFire();
	void StopFire();
	void Melee();
	void ThrowGrenade();
	void ReloadOrVent();
	void SwapWeapon();
	void Interact();
	void SetSprinting(bool bSprint);
	/** Hold Use Equipment: raise / lower the Aegis barrier. */
	void SetUsingEquipment(bool bUse);
	void SetZoomed(bool bZoom);
	void ToggleZoom();
	void StartCrouchOrSlide();
	void StopCrouchOrSlide();

	// ---- Server only ----

	void GiveWeapon(TSubclassOf<AShieldbreakWeapon> WeaponClass);
	void EquipWeapon(AShieldbreakWeapon* Weapon);
	void AddGrenades(int32 Count);
	/** False if already carrying that equipment (the pickup then stays). */
	bool GiveEquipment(EShieldbreakEquipment NewEquipment);
	/** A bullet hit the raised barrier: drain it instead of hurting the character. */
	void AbsorbWithAegis(float Damage, const FVector& ImpactPoint);
	void GrantCamo(float Duration);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool CanJumpInternal_Implementation() const override;

	// ---- Components ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shieldbreak")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shieldbreak")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Headshot hitbox. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shieldbreak")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shieldbreak")
	TObjectPtr<UStaticMeshComponent> VisorMesh;

	/** Aegis barrier: visible to everyone else while raised, and the only thing bullets hit then. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shieldbreak")
	TObjectPtr<UStaticMeshComponent> AegisMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shieldbreak")
	TObjectPtr<UShieldHealthComponent> ShieldHealth;

	/** Imported mannequin body + locomotion (falls back to the greybox meshes when the art is missing). */
	UPROPERTY(VisibleAnywhere, Category = "Character")
	TObjectPtr<UShieldbreakBodyAnimator> BodyAnimator;

	// ---- Loadout ----

	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Loadout")
	TSubclassOf<AShieldbreakWeapon> PrimaryWeaponClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Loadout")
	TSubclassOf<AShieldbreakWeapon> SecondaryWeaponClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Loadout")
	TSubclassOf<AShieldbreakGrenade> GrenadeClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Loadout")
	int32 StartingGrenades = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Loadout")
	int32 MaxGrenades = 4;

	// ---- Melee ----

	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Melee")
	float MeleeDamage = 70.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Melee")
	float MeleeRange = 190.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Melee")
	float MeleeLungeRange = 320.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Melee")
	float MeleeCooldown = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Melee")
	float AssassinationDamage = 999.f;

	/** Target forward · (attacker->target) above this counts as "from behind". */
	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Melee")
	float AssassinationDot = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Camera")
	float DefaultFOV = 78.f;

	// ---- Equipment ----

	/** Damage the barrier soaks per pickup. */
	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Equipment")
	float AegisMaxHealth = 200.f;

	/** Seconds the barrier can be held up per pickup. */
	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Equipment")
	float AegisMaxUptime = 6.f;

	/** Below this 2D speed a camouflaged player is (almost) invisible; above it they shimmer. */
	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Equipment")
	float CamoStillSpeed = 60.f;

private:
	// ---- Input handlers ----
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpStarted();
	void Input_JumpCompleted();
	void Input_FireStarted();
	void Input_FireCompleted();
	void Input_Zoom();
	void Input_EquipmentStarted();
	void Input_EquipmentCompleted();
	void Input_Melee();
	void Input_Grenade();
	void Input_ReloadVent();
	void Input_Swap();
	void Input_Interact();
	void Input_SprintStarted();
	void Input_SprintCompleted();
	void Input_CrouchStarted();
	void Input_CrouchCompleted();

	// ---- Server RPCs ----
	UFUNCTION(Server, Reliable) void ServerMelee();
	UFUNCTION(Server, Reliable) void ServerThrowGrenade();
	UFUNCTION(Server, Reliable) void ServerSwapWeapon();
	UFUNCTION(Server, Reliable) void ServerInteract();
	UFUNCTION(Server, Reliable) void ServerSetUsingEquipment(bool bUse);

	void SetAegisRaisedInternal(bool bRaise);
	void UpdateCamoVisuals();
	UFUNCTION() void OnRep_AegisRaised();
	UFUNCTION() void OnRep_CamoEnd();

	UFUNCTION(NetMulticast, Unreliable) void MulticastMeleeFX(FVector_NetQuantize ImpactLocation, bool bAssassination);

	void DoMelee();
	void DoThrowGrenade();
	void DoSwapWeapon();
	void DoInteract();

	UFUNCTION() void OnRep_CurrentWeapon();
	/** Equips visuals for CurrentWeapon on this machine, unequipping whatever was shown before. */
	void RefreshEquippedVisuals();

	UFUNCTION() void HandleDeath(AController* Killer);
	UFUNCTION() void HandleShieldChanged(float NewValue, float MaxValue);
	UFUNCTION() void HandleBreakPulse();

	void SpawnDefaultInventory();
	void DestroyInventory();
	void ApplyPlayerColor();
	void TintMesh(UStaticMeshComponent* MeshComponent, const FLinearColor& Color, TObjectPtr<UMaterialInstanceDynamic>& OutMID);

	// ---- Replicated state ----

	UPROPERTY(Replicated)
	TArray<TObjectPtr<AShieldbreakWeapon>> Inventory;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<AShieldbreakWeapon> CurrentWeapon;

	UPROPERTY(Replicated)
	int32 GrenadeCount = 0;

	UPROPERTY(Replicated)
	EShieldbreakEquipment Equipment = EShieldbreakEquipment::None;

	UPROPERTY(Replicated)
	float AegisHealth = 0.f;

	UPROPERTY(Replicated)
	float AegisUptime = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_AegisRaised)
	bool bAegisRaised = false;

	UPROPERTY(Replicated)
	TObjectPtr<AShieldbreakCarriable> CarriedObjective;

	/** Server time Active Camo ends (-1 = none). */
	UPROPERTY(ReplicatedUsing = OnRep_CamoEnd)
	float CamoEndServerTime = -1.f;

	// ---- Local state ----

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BodyMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HeadMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> VisorMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> AegisMID;

	TWeakObjectPtr<AShieldbreakWeapon> VisualEquippedWeapon;
	FLinearColor AppliedColor = FLinearColor(-1.f, -1.f, -1.f, -1.f);
	float LastLocalMeleeTime = -100.f;
	float LastServerMeleeTime = -100.f;
	float LastShieldValue = -1.f;
	bool bZoomed = false;
	bool bDeathHandled = false;
	bool bCamoHidden = false;
	bool bWantsEquipment = false;
};
