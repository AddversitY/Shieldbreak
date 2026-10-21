// Puts the imported mannequin on the owning ACharacter's mesh and drives it without an Animation Blueprint:
// it picks a locomotion sequence from the movement state and plays it in single-node mode. Death switches the
// mesh to a ragdoll. If the art is missing, nothing changes and the greybox body stays.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShieldbreakBodyAnimator.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

enum class EShieldbreakBodyPose : uint8
{
	None,
	Idle,
	WalkFwd, WalkBwd, WalkLeft, WalkRight,
	JogFwd, JogBwd, JogLeft, JogRight,
	CrouchIdle, CrouchFwd, CrouchBwd, CrouchLeft, CrouchRight,
	Rising, Apex, Falling
};

UCLASS(ClassGroup = (Shieldbreak), meta = (BlueprintSpawnableComponent))
class SHIELDBREAK_API UShieldbreakBodyAnimator : public UActorComponent
{
	GENERATED_BODY()

public:
	UShieldbreakBodyAnimator();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True once the mannequin mesh is on the character (the owner then hides its greybox). */
	bool HasBodyArt() const { return bHasBodyArt; }

	/** Every machine, on death: ragdoll the body with an initial velocity change. */
	void StartRagdoll(const FVector& Impulse);

	/** Below this 2D speed the body plays walk cycles; above it, jogs. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float JogThreshold = 380.f;

	/** Speeds the sequences were authored at; play rate scales with actual speed / these. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float WalkAnimSpeed = 220.f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float JogAnimSpeed = 480.f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float CrouchAnimSpeed = 200.f;

	/** Minimum time between pose switches, so direction changes don't flicker. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float MinPoseTime = 0.12f;

protected:
	virtual void BeginPlay() override;

private:
	USkeletalMeshComponent* GetBodyMesh() const;
	EShieldbreakBodyPose ChoosePose(float& OutPlayRate) const;
	UAnimSequenceBase* GetPoseAnimation(EShieldbreakBodyPose Pose) const;

	EShieldbreakBodyPose CurrentPose = EShieldbreakBodyPose::None;
	float LastPoseChangeTime = -100.f;
	bool bHasBodyArt = false;
	bool bRagdoll = false;
};
