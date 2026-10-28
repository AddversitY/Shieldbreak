// Map weapon pickup: walking over it with the same weapon tops up reserve ammo; otherwise the
// player presses Interact to swap it in. Respawns after a cooldown.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "ShieldbreakWeaponPickup.generated.h"

class AShieldbreakCharacter;
class AShieldbreakWeapon;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class SHIELDBREAK_API AShieldbreakWeaponPickup : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakWeaponPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server: swap this weapon into the character's inventory. */
	bool TryGiveTo(AShieldbreakCharacter* Character);

	TSubclassOf<AShieldbreakWeapon> GetWeaponClass() const { return WeaponClass; }
	bool IsAvailable() const { return bAvailable; }

	/** Server, before or right after spawning. */
	void SetWeaponClass(TSubclassOf<AShieldbreakWeapon> NewClass);

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_WeaponClass, Category = "Pickup")
	TSubclassOf<AShieldbreakWeapon> WeaponClass;

	UPROPERTY(EditAnywhere, Category = "Pickup")
	float RespawnTime = 30.f;

	/** Reserve ammo granted when walked over by someone already carrying this weapon. */
	UPROPERTY(EditAnywhere, Category = "Pickup")
	int32 AmmoGrant = 36;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Pickup") TObjectPtr<USphereComponent> Trigger;
	UPROPERTY(VisibleAnywhere, Category = "Pickup") TObjectPtr<UStaticMeshComponent> DisplayMesh;
	UPROPERTY(VisibleAnywhere, Category = "Pickup") TObjectPtr<UStaticMeshComponent> BaseMesh;

private:
	UFUNCTION() void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION() void OnRep_Available();
	UFUNCTION() void OnRep_WeaponClass();

	void SetAvailable(bool bNewAvailable);
	void Respawn();
	void UpdateVisuals();

	UPROPERTY(ReplicatedUsing = OnRep_Available) bool bAvailable = true;

	FTimerHandle RespawnTimerHandle;
	float SpinAngle = 0.f;
	float BobTime = 0.f;
	/** Center of the display mesh in its own space (scaled), so art meshes spin around their middle, not the grip. */
	FVector DisplayCenter = FVector::ZeroVector;
};
