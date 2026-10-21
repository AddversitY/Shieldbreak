// Frag grenade: bounces, short fuse after the first bounce, radial damage with falloff and a
// knockback impulse that also launches the thrower (grenade jumps).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "ShieldbreakGrenade.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class SHIELDBREAK_API AShieldbreakGrenade : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakGrenade();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: sets velocity and starts the long fuse. */
	void Launch(const FVector& Direction, const FVector& InheritedVelocity);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Grenade") TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, Category = "Grenade") TObjectPtr<UStaticMeshComponent> GrenadeMesh;
	UPROPERTY(VisibleAnywhere, Category = "Grenade") TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, Category = "Grenade") float LaunchSpeed = 1900.f;
	UPROPERTY(EditDefaultsOnly, Category = "Grenade") float FuseAfterBounce = 1.1f;
	UPROPERTY(EditDefaultsOnly, Category = "Grenade") float MaxFuse = 3.f;
	UPROPERTY(EditDefaultsOnly, Category = "Grenade") float BaseDamage = 110.f;
	UPROPERTY(EditDefaultsOnly, Category = "Grenade") float MinDamage = 10.f;
	UPROPERTY(EditDefaultsOnly, Category = "Grenade") float InnerRadius = 180.f;
	UPROPERTY(EditDefaultsOnly, Category = "Grenade") float OuterRadius = 520.f;
	UPROPERTY(EditDefaultsOnly, Category = "Grenade") float KnockbackStrength = 900.f;
	UPROPERTY(EditDefaultsOnly, Category = "Grenade") float KnockbackUp = 450.f;

private:
	UFUNCTION() void HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);
	UFUNCTION() void OnRep_Exploded();

	void Explode();
	void PlayExplosionFX();

	UPROPERTY(ReplicatedUsing = OnRep_Exploded) bool bExploded = false;

	FTimerHandle FuseTimerHandle;
	bool bBounced = false;
};
