// Server-authoritative, replicated Halo-style shield + health, plus the shield-based mastery
// mechanics: Shield Shaping, Overcharge Vent, Break Pulse and Shield Bleed. See docs/DESIGN.md.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShieldHealthComponent.generated.h"

class AController;
class UDamageType;
class UShieldTuningData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShieldbreakValueChanged, float, NewValue, float, MaxValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FShieldbreakEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShieldbreakVentResult, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShieldbreakDeath, AController*, Killer);

UCLASS(ClassGroup = (Shieldbreak), meta = (BlueprintSpawnableComponent))
class SHIELDBREAK_API UShieldHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShieldHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- Player inputs (call on the owning client or server) ----

	UFUNCTION(BlueprintCallable, Category = "Shield")
	void SetShaping(bool bNewShaping);

	UFUNCTION(BlueprintCallable, Category = "Shield")
	void TryVent();

	/** Call from the melee input. Does nothing unless the shield popped within the pulse window. */
	UFUNCTION(BlueprintCallable, Category = "Shield")
	void TryBreakPulse();

	// ---- Server-only damage entry points ----

	/** Full damage pipeline. Also invoked automatically via the owner's OnTakeAnyDamage. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Shield")
	float ApplyShieldbreakDamage(float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	/** Strips shield/overshield only; never touches health. Used by Break Pulse. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Shield")
	void ApplyShieldOnlyDamage(float Damage, AController* InstigatedBy);

	// ---- Queries ----

	UFUNCTION(BlueprintPure, Category = "Shield") float GetShield() const { return Shield; }
	UFUNCTION(BlueprintPure, Category = "Shield") float GetMaxShield() const;
	UFUNCTION(BlueprintPure, Category = "Shield") float GetOvershield() const { return Overshield; }
	UFUNCTION(BlueprintPure, Category = "Shield") float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category = "Shield") float GetMaxHealth() const;
	UFUNCTION(BlueprintPure, Category = "Shield") bool IsShaping() const { return bShaping; }
	UFUNCTION(BlueprintPure, Category = "Shield") bool IsRecharging() const { return RechargeStartServerTime >= 0.f; }
	UFUNCTION(BlueprintPure, Category = "Shield") bool IsDead() const { return bDead; }

	/** Vent marker position along the shield bar (0..1), or -1 when venting is unavailable. Drives the HUD. */
	UFUNCTION(BlueprintPure, Category = "Shield")
	float GetVentMarkerPosition() const;

	/** The tuning in effect (asset or class defaults). Never null. C++ only (UHT rejects const UObject returns). */
	const UShieldTuningData* GetTuningData() const { return &GetTuning(); }

	/** Local world time (GetWorld()->GetTimeSeconds()) of the last shield pop on this machine, or -1. */
	UFUNCTION(BlueprintPure, Category = "Shield")
	float GetLastShieldBreakLocalTime() const { return LastShieldBreakLocalTime; }

	// ---- Events (fire on server and clients) ----

	UPROPERTY(BlueprintAssignable, Category = "Shield") FShieldbreakValueChanged OnShieldChanged;
	UPROPERTY(BlueprintAssignable, Category = "Shield") FShieldbreakValueChanged OnOvershieldChanged;
	UPROPERTY(BlueprintAssignable, Category = "Shield") FShieldbreakValueChanged OnHealthChanged;
	UPROPERTY(BlueprintAssignable, Category = "Shield") FShieldbreakEvent OnShieldBroken;
	UPROPERTY(BlueprintAssignable, Category = "Shield") FShieldbreakEvent OnRechargeStarted;
	UPROPERTY(BlueprintAssignable, Category = "Shield") FShieldbreakEvent OnBreakPulse;
	/** Owning client only. */
	UPROPERTY(BlueprintAssignable, Category = "Shield") FShieldbreakVentResult OnVentResult;
	UPROPERTY(BlueprintAssignable, Category = "Shield") FShieldbreakDeath OnDeath;

protected:
	virtual void BeginPlay() override;

	/** Leave empty to use the class defaults of UShieldTuningData. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield")
	TObjectPtr<UShieldTuningData> Tuning;

private:
	const UShieldTuningData& GetTuning() const;
	float GetServerTime() const;
	float GetShapingMultiplier(const AActor* DamageCauser, const AController* InstigatedBy) const;

	/** Returns the raw (pre-multiplier) damage the shield layers did not absorb. */
	float AbsorbWithShield(float RawDamage, float ShieldMultiplier);
	void ResetRechargeDelay(float ExtraDelay);
	void SetShieldInternal(float NewShield);
	void SetOvershieldInternal(float NewOvershield);
	void SetHealthInternal(float NewHealth);
	void Die(AController* Killer);
	void DoBreakPulse();
	float ResolveInputTime(float ClientServerTime) const;

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION(Server, Reliable) void ServerSetShaping(bool bNewShaping);
	UFUNCTION(Server, Reliable) void ServerTryVent(float ClientServerTime);
	UFUNCTION(Server, Reliable) void ServerTryBreakPulse(float ClientServerTime);
	UFUNCTION(Client, Reliable) void ClientVentResult(bool bSuccess);
	UFUNCTION(NetMulticast, Unreliable) void MulticastBreakPulse();

	UFUNCTION() void OnRep_Shield();
	UFUNCTION() void OnRep_ShieldBreakCount();
	UFUNCTION() void OnRep_Overshield();
	UFUNCTION() void OnRep_Health();
	UFUNCTION() void OnRep_RechargeStart(float OldRechargeStart);
	UFUNCTION() void OnRep_Dead();

	UPROPERTY(ReplicatedUsing = OnRep_Shield)
	float Shield = 0.f;

	/** Bumped on every shield pop so clients reliably see OnShieldBroken. */
	UPROPERTY(ReplicatedUsing = OnRep_ShieldBreakCount)
	uint8 ShieldBreakCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Overshield)
	float Overshield = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 0.f;

	UPROPERTY(Replicated)
	bool bShaping = false;

	/** Server time the shield started refilling; -1 while not refilling. */
	UPROPERTY(ReplicatedUsing = OnRep_RechargeStart)
	float RechargeStartServerTime = -1.f;

	UPROPERTY(Replicated)
	bool bVentUsedThisRecharge = false;

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	// Server-only state.
	float NextRechargeServerTime = 0.f;
	float ShieldBrokenServerTime = -1.f;
	float LastBreakPulsePressServerTime = -1.f;

	// Set on every machine when the shield pops (server directly, clients via OnRep_ShieldBreakCount).
	float LastShieldBreakLocalTime = -1.f;
	float PendingRechargePenalty = 0.f;
};
