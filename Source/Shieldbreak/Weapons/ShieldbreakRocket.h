// Rocket Launcher projectile: flies straight, explodes on impact (or after MaxLifetime) with heavy splash
// damage and knockback on everyone but the shooter. Server-authoritative; clients see the replicated flight.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShieldbreakRocket.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class SHIELDBREAK_API AShieldbreakRocket : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakRocket();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, right after spawning. */
	void Launch(const FVector& Direction);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Rocket") TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, Category = "Rocket") TObjectPtr<UStaticMeshComponent> BodyMesh;
	UPROPERTY(VisibleAnywhere, Category = "Rocket") TObjectPtr<UStaticMeshComponent> NoseMesh;
	UPROPERTY(VisibleAnywhere, Category = "Rocket") TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket") float Speed = 2600.f;
	UPROPERTY(EditDefaultsOnly, Category = "Rocket") float MaxLifetime = 6.f;
	UPROPERTY(EditDefaultsOnly, Category = "Rocket") float BaseDamage = 220.f;
	UPROPERTY(EditDefaultsOnly, Category = "Rocket") float MinDamage = 20.f;
	UPROPERTY(EditDefaultsOnly, Category = "Rocket") float InnerRadius = 180.f;
	UPROPERTY(EditDefaultsOnly, Category = "Rocket") float OuterRadius = 520.f;
	UPROPERTY(EditDefaultsOnly, Category = "Rocket") float KnockbackStrength = 1400.f;
	UPROPERTY(EditDefaultsOnly, Category = "Rocket") float KnockbackUp = 700.f;
	/** Your own rockets hurt you less than they hurt enemies. */
	UPROPERTY(EditDefaultsOnly, Category = "Rocket") float SelfDamageScale = 0.4f;

private:
	UFUNCTION() void HandleStop(const FHitResult& ImpactResult);
	UFUNCTION() void OnRep_Exploded();

	void Explode();
	void PlayExplosionFX();

	UPROPERTY(ReplicatedUsing = OnRep_Exploded) bool bExploded = false;

	FTimerHandle LifetimeTimerHandle;
};
