// Map power-up: a floating orb that is consumed by walking into it and respawns on a fixed timer. There is
// deliberately no on-screen timer: tracking spawns is part of the competitive skill gap.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "ShieldbreakPowerup.generated.h"

class AShieldbreakCharacter;
class USphereComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EShieldbreakPowerupType : uint8
{
	/** Two extra shield bars on top of a full shield; slowly decays. */
	Overshield,
	/** Near-invisible while still, a flickering shimmer while moving or shooting. */
	ActiveCamo,
	/** Equipment: the Aegis frontal barrier (held, used with Use Equipment). */
	Aegis
};

UCLASS()
class SHIELDBREAK_API AShieldbreakPowerup : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakPowerup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsAvailable() const { return bAvailable; }

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Type, Category = "Power-up")
	EShieldbreakPowerupType Type = EShieldbreakPowerupType::Overshield;

	UPROPERTY(EditAnywhere, Category = "Power-up")
	float RespawnTime = 90.f;

	UPROPERTY(EditAnywhere, Category = "Power-up")
	float CamoDuration = 30.f;

	/** Server, before or right after spawning (updates the orb color on every machine via replication). */
	void SetType(EShieldbreakPowerupType NewType);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Power-up") TObjectPtr<USphereComponent> Trigger;
	UPROPERTY(VisibleAnywhere, Category = "Power-up") TObjectPtr<UStaticMeshComponent> OrbMesh;
	UPROPERTY(VisibleAnywhere, Category = "Power-up") TObjectPtr<UStaticMeshComponent> BaseMesh;

private:
	UFUNCTION() void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION() void OnRep_Available();
	UFUNCTION() void OnRep_Type();
	void ApplyColor();

	/** Server: applies the power-up; false if this character can't take it. */
	bool TryConsume(AShieldbreakCharacter* Character);
	void Respawn();
	void UpdateVisuals();
	FLinearColor GetPowerupColor() const;

	UPROPERTY(ReplicatedUsing = OnRep_Available) bool bAvailable = true;

	FTimerHandle RespawnTimerHandle;
	float SpinAngle = 0.f;
	float BobTime = 0.f;
};
