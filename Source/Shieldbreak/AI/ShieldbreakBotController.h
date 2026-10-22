// Tick-driven bot: perceives enemies by line of sight, walks the arena waypoint graph, strafes
// in fights, aims with skill-scaled error and uses the mastery mechanics at higher skills.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ShieldbreakBotController.generated.h"

class AShieldbreakArena;
class AShieldbreakCharacter;
class AShieldbreakWeapon;

UCLASS()
class SHIELDBREAK_API AShieldbreakBotController : public AAIController
{
	GENERATED_BODY()

public:
	AShieldbreakBotController();

	virtual void Tick(float DeltaSeconds) override;

	/** Aim is driven entirely by UpdateAim; the base version would zero pitch without a focus actor. */
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;

	/** 0 Easy, 1 Normal, 2 Heroic, 3 Legendary. */
	void SetSkill(int32 NewSkill);
	int32 GetSkill() const { return Skill; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	/** Objective modes: where to go, and whether it overrides fighting. False in Slayer. */
	bool GetObjectiveGoal(FVector& OutGoal, bool& bOutUrgent) const;

	struct FSkillProfile
	{
		float Reaction;
		float AimError;
		float TurnSpeed;
		float FireMargin;
	};
	FSkillProfile GetProfile() const;

	AShieldbreakCharacter* GetBot() const;
	AShieldbreakArena* GetArena();
	float Now() const;

	void ResetState();
	void Perceive();
	bool CanSee(const AShieldbreakCharacter* Other, FVector& OutAimPoint) const;

	void UpdateMovement(float DeltaSeconds);
	void UpdateAim(float DeltaSeconds);
	void UpdateCombat(float DeltaSeconds);
	void UpdateMastery();

	void FollowPathTo(const FVector& Goal, float DeltaSeconds);
	void PickRoamGoal();
	void MoveToward(const FVector& Location, float Scale = 1.f);
	void ChooseWeapon(float TargetDistance);
	float ScoreWeapon(const AShieldbreakWeapon* Weapon, float TargetDistance) const;

	UFUNCTION() void HandlePawnDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	int32 Skill = 1;

	UPROPERTY(Transient) TObjectPtr<AShieldbreakArena> CachedArena;

	// Perception.
	TWeakObjectPtr<AShieldbreakCharacter> Target;
	TWeakObjectPtr<AShieldbreakCharacter> LastAttacker;
	float LastAttackedTime = -100.f;
	FVector TargetAimPoint = FVector::ZeroVector;
	FVector LastKnownLocation = FVector::ZeroVector;
	float LastSeenTime = -100.f;
	float TargetAcquiredTime = -100.f;
	bool bTargetVisible = false;
	float PerceiveTimer = 0.f;
	int32 VisibleEnemyCount = 0;

	// Navigation.
	TArray<int32> Path;
	int32 PathIndex = 0;
	FVector PathGoal = FVector::ZeroVector;
	FVector RoamGoal = FVector::ZeroVector;
	bool bHasRoamGoal = false;
	float RepathTimer = 0.f;
	FVector LastProgressLocation = FVector::ZeroVector;
	float StuckTimer = 0.f;
	bool bSliding = false;
	float SlideStartTime = -100.f;

	// Combat.
	float StrafeDir = 1.f;
	float StrafeTimer = 0.f;
	float CrouchUntil = -100.f;
	FRotator AimNoise = FRotator::ZeroRotator;
	float AimNoiseTimer = 0.f;
	float TrackingTime = 0.f;
	float NextWeaponSwapTime = 0.f;
	float NextGrenadeTime = 0.f;
	bool bFiring = false;

	// Mastery.
	float VentPressAt = -1.f;
	bool bVentDecided = false;
	float LastSeenShieldBreak = -1.f;
};
