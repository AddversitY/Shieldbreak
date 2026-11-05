// Frag grenade: bounces, short fuse after the first bounce, radial damage with falloff and a knockback impulse that
// also launches the thrower (grenade jumps). It tumbles in flight, blinks an amber arming light (faster once armed)
// and goes off as a layered AShieldbreakExplosionFX.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "ShieldbreakGrenade.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UProjectileMovementComponent;
class UStaticMesh;
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

	/** Seconds from the throw press until the hand lets go (sb.GrenadeReleaseDelay). The server spawns the grenade
	 *  then; the first-person arm and the third-person toss are timed to the same moment. */
	static float GetReleaseDelay();
	/** Seconds a throw lasts (sb.GrenadeThrowTime): the minimum time between throws and the no-fire window. */
	static float GetThrowDuration();
	/** The grenade model (ours, else Lyra's, else null). Shared with the grenade shown in the throwing hand. */
	static UStaticMesh* GetGrenadeMesh();

	virtual void Tick(float DeltaSeconds) override;

	/** True once it has hit something and the short fuse is running (the arming light blinks fast). */
	bool IsArmed() const { return bArmed; }
	/** The arming light's current brightness, 0..1 (for the self-test). */
	float GetBlinkLevel() const { return BlinkLevel; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Grenade") TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, Category = "Grenade") TObjectPtr<UStaticMeshComponent> GrenadeMesh;
	UPROPERTY(VisibleAnywhere, Category = "Grenade") TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
	/** The arming light: a small unshadowed point light that blinks with the glow strip. */
	UPROPERTY(VisibleAnywhere, Category = "Grenade") TObjectPtr<UPointLightComponent> ArmingLight;

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
	UFUNCTION() void OnRep_Armed();
	/** Everyone: a bounce's sound, scaled by how hard it hit. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastBounce(FVector_NetQuantize Location, float Strength);
	void UpdateVisuals(float DeltaSeconds);

	void Explode();
	void PlayExplosionFX();

	UPROPERTY(ReplicatedUsing = OnRep_Exploded) bool bExploded = false;
	UPROPERTY(ReplicatedUsing = OnRep_Armed) bool bArmed = false;

	FTimerHandle FuseTimerHandle;
	bool bBounced = false;
	float LastBounceTime = -100.f;
	/** Cosmetic, every machine: the model tumbles about its own centre, rolls when on the ground. */
	FQuat Spin = FQuat::Identity;
	FVector SpinAxis = FVector::RightVector;
	float SpinRate = 0.f;
	FVector LastLocation = FVector::ZeroVector;
	bool bHasLastLocation = false;
	float Age = 0.f;
	float ArmedAge = 0.f;
	float BlinkLevel = 0.f;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GlowMID;
};
