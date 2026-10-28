#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShieldbreakShieldVisual.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;

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
	float GetIntensity() const { return Intensity; }
protected:
	virtual void BeginPlay() override;
private:
	UFUNCTION() void HandleImpact(float AbsorbedDamage, bool bBroken, bool bOvershield);
	UFUNCTION() void HandleBreak();
	void Pulse(float Strength, float Duration, const FLinearColor& Color);
	void ClearOverlay();
	void ApplyOverlay();
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> Material;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PulseMaterial;
	TArray<TWeakObjectPtr<UMeshComponent>> PulseMeshes;
	float StartedAt = -1.f;
	float PulseDuration = 0.f;
	float PeakIntensity = 0.f;
	float Intensity = 0.f;
	int32 ImpactCount = 0;
};
