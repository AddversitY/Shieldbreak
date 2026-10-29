// Native anim instance for the mannequin, so the body can blend without an Animation Blueprint asset.
// UShieldbreakBodyAnimator still picks the locomotion sequence; this instance cross-fades from the previous one
// (no more pops on direction changes) and bends the spine to the aim pitch, so bots and remote players look and
// point their gun where they shoot. On top of that it layers one-shot actions (grenade throw, melee, reload, equip)
// over the upper body, from spine_01 up, fading in and out; the first-person arms use the same instance.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "ShieldbreakBodyAnimInstance.generated.h"

class UAnimSequenceBase;

USTRUCT()
struct SHIELDBREAK_API FShieldbreakBodyAnimProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FShieldbreakBodyAnimProxy() = default;
	explicit FShieldbreakBodyAnimProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}
	float GetCyclePhase() const;

protected:
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	struct FTrack
	{
		const UAnimSequenceBase* Animation = nullptr;
		float Time = 0.f;
		float PlayRate = 1.f;
		bool bLoop = true;
		const UAnimSequenceBase* Directions[4] = {};
		FVector4 Weights = FVector4(1.f, 0.f, 0.f, 0.f);
		bool bDirectional = false;
	};

	static void Advance(FTrack& Track, float DeltaSeconds);
	static void Sample(const FTrack& Track, FPoseContext& Output);
	void ApplyAction(FPoseContext& Output) const;
	void ApplyAimPitch(FPoseContext& Output) const;

	FTrack Current;
	FTrack Previous;
	/** The one-shot action layered over the upper body (Animation null when none). */
	FTrack Action;
	FTrack PreviousAction;
	float ActionTransition = 1.f;
	FVector4 TargetWeights = FVector4(1.f, 0.f, 0.f, 0.f);
	float TargetPlayRate = 1.f;
	float ActionWeight = 0.f;
	float ActionBlendIn = 0.1f;
	float ActionBlendOut = 0.2f;
	int32 SeenActionRequest = 0;
	/** Weight of Current; Previous fades out as it reaches 1. */
	float BlendAlpha = 1.f;
	float BlendTime = 0.18f;
	int32 SeenRequest = -1;
	float AimPitch = 0.f;
	float TargetAimPitch = 0.f;
	FVector AimAxis = FVector::RightVector;
	/** Shot recoil on the upper body: extra spine pitch (degrees) springing back to 0. */
	float KickPitch = 0.f;
	float KickVelocity = 0.f;
	float SeenKickTotal = 0.f;
	/** The last base pose (local bone transforms, before actions and aim), and whether the current cross-fade starts
	 *  from it: a pose change that interrupts a cross-fade fades from what was on screen instead of popping. */
	TArray<FTransform> LastBasePose;
	bool bFadeFromSnapshot = false;
};

UCLASS(Transient, NotBlueprintable)
class SHIELDBREAK_API UShieldbreakBodyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Switch to Animation, cross-fading from what plays now. Calling it again with the same animation does nothing. */
	void PlayPose(UAnimSequenceBase* Animation, bool bLoop);
	/** Shared gait phase and continuous forward/back/left/right blending. */
	void PlayDirectionalPose(UAnimSequenceBase* Forward, UAnimSequenceBase* Backward,
		UAnimSequenceBase* Left, UAnimSequenceBase* Right, const FVector2D& Direction);
	void SetPosePlayRate(float InPlayRate) { PlayRate = InPlayRate; }
	/** Aim pitch in degrees (up is positive) and the pitch axis (the character's right) in mesh component space. */
	void SetAim(float InPitch, const FVector& InAxisComponentSpace) { AimPitch = InPitch; AimAxis = InAxisComponentSpace; }

	UAnimSequenceBase* GetPoseAnimation() const { return RequestedAnimation; }
	float GetAimPitch() const { return AimPitch; }
	/** Diagnostic snapshot; completes any in-flight animation work before reading. */
	float GetLocomotionPhase() const { return GetProxyOnGameThread<FShieldbreakBodyAnimProxy>().GetCyclePhase(); }

	/** A shot: rocks the upper body back by an amount that scales with Strength (about 0.6-2). */
	void AddFireKick(float Strength) { KickTotal += Strength; }

	/** Plays Animation once over the upper body, from StartTime (s) at PlayRate, then fades back to the pose. */
	void PlayAction(UAnimSequenceBase* Animation, float StartTime = 0.f, float InPlayRate = 1.f);
	/** The action playing now (game-thread view: cleared once its time is up), or null. */
	UAnimSequenceBase* GetActionAnimation() const;
	int32 GetActionCount() const { return ActionRequestCount; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FShieldbreakBodyAnimProxy(this); }

private:
	friend struct FShieldbreakBodyAnimProxy;

	/** Held here so the sequences the worker thread samples stay loaded. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> RequestedAnimation;
	/** The one still fading out. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> FadingAnimation;
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequenceBase>> DirectionalAnimations;
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequenceBase>> FadingDirectionalAnimations;
	UPROPERTY(Transient) TObjectPtr<UAnimSequenceBase> FadingActionAnimation;
	FVector4 DirectionWeights = FVector4(1.f, 0.f, 0.f, 0.f);
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ActionAnimation;
	float ActionStartTime = 0.f;
	float ActionPlayRate = 1.f;
	/** World time the action ends (for GetActionAnimation). */
	double ActionEndWorldTime = 0.0;
	int32 ActionRequestCount = 0;
	bool bRequestedLoop = true;
	int32 RequestCount = 0;
	float PlayRate = 1.f;
	float AimPitch = 0.f;
	FVector AimAxis = FVector::RightVector;
	/** Sum of every AddFireKick strength; the proxy applies the difference since its last update. */
	float KickTotal = 0.f;
};
