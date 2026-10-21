// Asset-free sound effects. Every sound is synthesized once into 16-bit PCM and played through a fresh
// USoundWaveProcedural, so the prototype has audio without any .uasset files.

#pragma once

#include "CoreMinimal.h"

class UAudioComponent;
class USceneComponent;

enum class EShieldbreakSound : uint8
{
	BattleRifleShot,
	SMGShot,
	PlasmaShot,
	GrenadeExplosion,
	ShieldRecharge,
	ShieldPop,
	ShieldLowBeep,
	HitMarker,
	KillMarker,
	VentSuccess,
	VentFail,
	BreakPulse,
	Melee,
	Death,
	RailCharge,
	RailFire,
	AegisRaise,
	AegisHit,
	CamoOn,
	Count
};

class SHIELDBREAK_API FShieldbreakSFX
{
public:
	/** Non-spatialized (UI / first-person) sound. Returns null on dedicated servers. */
	static UAudioComponent* Play2D(const UObject* WorldContext, EShieldbreakSound Sound, float Volume = 1.f, float Pitch = 1.f);

	/** Spatialized one-shot at a world location. */
	static UAudioComponent* PlayAt(const UObject* WorldContext, EShieldbreakSound Sound, const FVector& Location, float Volume = 1.f, float Pitch = 1.f);

	/** Length in seconds of a synthesized sound at pitch 1. */
	static float GetDuration(EShieldbreakSound Sound);
};
