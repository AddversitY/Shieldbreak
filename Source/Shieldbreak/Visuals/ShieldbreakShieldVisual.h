#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShieldbreakShieldVisual.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UStaticMeshComponent;

/** A short, replicated-hit-driven energy overlay; never participates in collision. */
UCLASS(ClassGroup = (Shieldbreak))
class SHIELDBREAK_API UShieldbreakShieldVisual : public UActorComponent
{
	GENERATED_BODY()
public:
	UShieldbreakShieldVisual();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	bool IsPulseActive() const;
	int32 GetImpactCount() const { return ImpactCount; }
	/** True while the shield-pop bubble is bursting outward (for the self-test). */
	bool IsBurstActive() const;
	float GetIntensity() const { return Intensity; }
protected:
	virtual void BeginPlay() override;
private:
	UFUNCTION() void HandleImpact(float AbsorbedDamage, bool bBroken, bool bOvershield);
	UFUNCTION() void HandleBreak();
	void Pulse(float Strength, float Duration, const FLinearColor& Color);
	void ClearOverlay();
	void ApplyOverlay();
	/** Shield pop: sparks plus a shell of shield energy that bursts outward from the body and fades (others only). */
	void StartBurst();
	void UpdateBurst();
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> BurstMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BurstMaterial;
	float BurstStartedAt = -1.f;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Material;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PulseMaterial;
	TArray<TWeakObjectPtr<UMeshComponent>> PulseMeshes;
	float StartedAt = -1.f;
	float PulseDuration = 0.f;
	float PeakIntensity = 0.f;
	float Intensity = 0.f;
	int32 ImpactCount = 0;
};
