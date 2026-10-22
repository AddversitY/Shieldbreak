// Local-only cosmetic effects built from engine basic shapes: bullet tracers, impact puffs and
// expanding spheres (explosions, Break Pulse). Never replicated.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShieldbreakTracer.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

UCLASS(NotPlaceable, Transient)
class SHIELDBREAK_API AShieldbreakTracer : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakTracer();

	virtual void Tick(float DeltaSeconds) override;

	/** Thin stretched cylinder from Start to End (0.06 s) plus a small impact puff at End (0.12 s). */
	static void Spawn(UWorld* World, const FVector& Start, const FVector& End, const FLinearColor& Color);

	/** Sphere growing from StartRadius to EndRadius over Duration. */
	static void SpawnBurst(UWorld* World, const FVector& Location, float StartRadius, float EndRadius, float Duration, const FLinearColor& Color);

private:
	void InitAsBeam(const FVector& Start, const FVector& End, const FLinearColor& Color, float Duration);
	void InitAsSphere(const FVector& Location, float StartRadius, float EndRadius, float Duration, const FLinearColor& Color);
	static AShieldbreakTracer* SpawnRaw(UWorld* World, const FVector& Location);

	UPROPERTY(VisibleAnywhere, Category = "FX")
	TObjectPtr<UStaticMeshComponent> ShapeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	UPROPERTY(Transient)
	TObjectPtr<class UStaticMesh> CylinderAsset;

	UPROPERTY(Transient)
	TObjectPtr<class UStaticMesh> SphereAsset;

	float Age = 0.f;
	float Lifetime = 0.1f;
	bool bSphere = false;
	float StartScale = 1.f;
	float EndScale = 1.f;
	FVector BeamScale = FVector::OneVector;
};
