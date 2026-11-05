// Shieldbreak's layered explosion, built from cheap pieces instead of one heavy particle system: a short flash light,
// a hot core, a rolling fireball (flipbook cards), a smoke column that rises and spreads, a ground scorch decal and a
// camera shake for the local player that falls off with distance. Spawned on each machine that shows it (never on a
// dedicated server), not replicated, and gone after a few seconds. The card materials come from
// Tools/create_grenade_fx_materials.py; without them only the light, decal-free shake and Lyra's sparks play.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/Actor.h"
#include "ShieldbreakExplosionFX.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/** A jolt that dies away: two detuned oscillations per axis under a quadratic decay, so it never loops visibly. */
UCLASS()
class SHIELDBREAK_API UShieldbreakJoltShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	float Duration = 0.5f;
	/** Peak degrees (pitch, yaw, roll) and centimetres (forward, right, up) at scale 1. */
	FVector RotationAmplitude = FVector(2.2f, 1.2f, 1.6f);
	FVector LocationAmplitude = FVector(0.f, 1.f, 2.5f);
	float Frequency = 17.f;

protected:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;

private:
	float Time = 0.f;
	float Phases[6] = {};
};

/** The explosion's jolt for the local player (ClientStartCameraShake with a distance scale). It moves the view only;
 *  the aim (control rotation) never moves, as with every other shake in Shieldbreak. */
UCLASS()
class SHIELDBREAK_API UShieldbreakExplosionShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UShieldbreakExplosionShake(const FObjectInitializer& ObjectInitializer);
};

UCLASS(NotBlueprintable)
class SHIELDBREAK_API AShieldbreakExplosionFX : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakExplosionFX();

	/** Spawns the explosion at Location on this machine. Scale 1 is a frag grenade. Returns null on a dedicated server. */
	static AShieldbreakExplosionFX* Spawn(UWorld* World, const FVector& Location, float Scale = 1.f);

	virtual void Tick(float DeltaSeconds) override;

	/** Seconds since it went off. */
	float GetAge() const { return Age; }
	/** How many fire and smoke cards it is drawing (0 when the card materials are missing). */
	int32 GetCardCount() const { return Cards.Num(); }

protected:
	virtual void BeginPlay() override;

private:
	struct FCard
	{
		UStaticMeshComponent* Mesh = nullptr; // kept alive by CardMeshes
		UMaterialInstanceDynamic* MID = nullptr; // kept alive by CardMIDs
		bool bSmoke = false;
		float Delay = 0.f;
		float Life = 1.f;
		float StartSize = 100.f;
		float EndSize = 200.f;
		FVector Offset = FVector::ZeroVector;
		FVector Drift = FVector::ZeroVector;
		float Roll = 0.f;
		float Spin = 0.f;
		float StartFrame = 0.f;
	};

	void Build();
	void AddCard(bool bSmoke, float Delay, float Life, float StartSize, float EndSize, const FVector& Offset, const FVector& Drift);
	void SpawnScorch();
	void ShakeLocalPlayers();

	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Flash;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> CardMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> CardMIDs;
	TArray<FCard> Cards;
	float Age = 0.f;
	float FXScale = 1.f;
	float Duration = 3.f;
};
