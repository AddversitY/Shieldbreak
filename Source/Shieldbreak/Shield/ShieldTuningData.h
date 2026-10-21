// Shieldbreak shield/health tuning. Every gameplay number for the shield system lives here
// so designers can iterate in-editor without recompiling.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShieldTuningData.generated.h"

UCLASS(BlueprintType)
class SHIELDBREAK_API UShieldTuningData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- Core (Halo baseline) ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "1"))
	float MaxShield = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "1"))
	float MaxHealth = 45.f;

	/** Seconds after the last hit before shields start refilling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0"))
	float RechargeDelay = 4.5f;

	/** Seconds to refill a fully empty shield once recharge starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0.1"))
	float RechargeDuration = 2.f;

	/** Halo 2 style: health only refills once shields are full. Off = health never regenerates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	bool bRegenHealthWhenShieldFull = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0"))
	float HealthRegenPerSecond = 30.f;

	// ---- Shield Shaping ----

	/** Full width of the frontal arc protected while shaping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shaping", meta = (ClampMin = "0", ClampMax = "360"))
	float ShapingFrontArcDegrees = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shaping", meta = (ClampMin = "0"))
	float ShapingFrontMultiplier = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shaping", meta = (ClampMin = "0"))
	float ShapingRearMultiplier = 1.5f;

	/** Extra recharge delay for hits taken while shaping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shaping", meta = (ClampMin = "0"))
	float ShapingRechargeDelayBonus = 1.f;

	// ---- Overcharge Vent ----

	/** Seconds for the vent marker to sweep across the shield bar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vent", meta = (ClampMin = "0.1"))
	float VentSweepDuration = 2.f;

	/** Sweet spot start along the sweep, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vent", meta = (ClampMin = "0", ClampMax = "1"))
	float VentSweetSpotStart = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vent", meta = (ClampMin = "0", ClampMax = "1"))
	float VentSweetSpotWidth = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vent", meta = (ClampMin = "0"))
	float VentOvershieldGain = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vent", meta = (ClampMin = "0"))
	float MaxOvershield = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vent", meta = (ClampMin = "0"))
	float OvershieldDecayPerSecond = 2.f;

	/** Overshield granted by the Overshield power-up (two extra shield bars); also the absolute overshield cap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power-ups", meta = (ClampMin = "0"))
	float PowerupOvershield = 140.f;

	// ---- Break Pulse ----

	/** Seconds after your shield pops in which a melee press triggers a pulse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Pulse", meta = (ClampMin = "0"))
	float BreakPulseWindow = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Pulse", meta = (ClampMin = "0"))
	float BreakPulseRadius = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Pulse", meta = (ClampMin = "0"))
	float BreakPulseShieldDamage = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Pulse", meta = (ClampMin = "0"))
	float BreakPulseImpulse = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Pulse", meta = (ClampMin = "0"))
	float BreakPulseUpImpulse = 300.f;

	/** Added to your next recharge delay after pulsing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Pulse", meta = (ClampMin = "0"))
	float BreakPulseRechargePenalty = 2.f;

	// ---- Networking ----

	/** Max age (s) of a client-stamped input the server will honor for timing checks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Networking", meta = (ClampMin = "0"))
	float MaxInputLagCompensation = 0.25f;
};
