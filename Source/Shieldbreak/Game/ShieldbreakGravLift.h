// Gravity lift: a glowing pad that throws anyone who steps on it to a landing spot (Construct-style vertical
// routes). Built locally on every machine by the arena, so the owning client predicts the launch.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShieldbreakGravLift.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS(NotPlaceable)
class SHIELDBREAK_API AShieldbreakGravLift : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakGravLift();

	/** Before BeginPlay: where the lift lands you (feet position, world space). */
	void SetTarget(const FVector& InTarget) { Target = InTarget; }

	/** Launch velocity that lands a character standing at From on To, peaking ApexClearance above the higher end. */
	static FVector ComputeLaunchVelocity(const FVector& From, const FVector& To, float GravityZ, float ApexClearance = 150.f);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Lift") TObjectPtr<UBoxComponent> Trigger;
	UPROPERTY(VisibleAnywhere, Category = "Lift") TObjectPtr<UStaticMeshComponent> PadMesh;
	UPROPERTY(VisibleAnywhere, Category = "Lift") TObjectPtr<UStaticMeshComponent> BeamMesh;

private:
	UFUNCTION() void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	FVector Target = FVector::ZeroVector;
};
