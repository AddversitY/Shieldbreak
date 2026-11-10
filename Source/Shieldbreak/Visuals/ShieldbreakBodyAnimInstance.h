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

/** Grip fingers (index, middle, ring, pinky, thumb; joints _01.._03), the order every per-joint curl array uses. */
namespace ShieldbreakGrip
{
	constexpr int32 NumJoints = 15;
	inline const TCHAR* const LeftBones[NumJoints] = {
		TEXT("index_01_l"), TEXT("index_02_l"), TEXT("index_03_l"), TEXT("middle_01_l"), TEXT("middle_02_l"), TEXT("middle_03_l"),
		TEXT("ring_01_l"), TEXT("ring_02_l"), TEXT("ring_03_l"), TEXT("pinky_01_l"), TEXT("pinky_02_l"), TEXT("pinky_03_l"),
		TEXT("thumb_01_l"), TEXT("thumb_02_l"), TEXT("thumb_03_l") };
	inline const TCHAR* const RightBones[NumJoints] = {
		TEXT("index_01_r"), TEXT("index_02_r"), TEXT("index_03_r"), TEXT("middle_01_r"), TEXT("middle_02_r"), TEXT("middle_03_r"),
		TEXT("ring_01_r"), TEXT("ring_02_r"), TEXT("ring_03_r"), TEXT("pinky_01_r"), TEXT("pinky_02_r"), TEXT("pinky_03_r"),
		TEXT("thumb_01_r"), TEXT("thumb_02_r"), TEXT("thumb_03_r") };
	/** The left hand's forend wrap at sb.FPFingerCurl 1 (degrees, toward the palm), before contact correction. */
	inline constexpr float LeftNominal[NumJoints] = { 30.f, 55.f, 35.f, 38.f, 60.f, 38.f, 42.f, 62.f, 40.f, 48.f, 62.f, 40.f, 8.f, 18.f, 18.f };
	/** cm from a finger's _03 joint to its tip. */
	inline constexpr float TipLength[5] = { 2.0f, 2.1f, 2.0f, 1.8f, 2.2f };
}

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
	/** Holds the root bone at its ref pose, so clips with root motion play in place. */
	static void LockRoot(FPoseContext& Output);
	void ApplyAction(FPoseContext& Output) const;
	void ApplyAimPitch(FPoseContext& Output) const;
	/** Curls the right index finger (CCD over index_01/02_r) so its pad rests on TriggerTarget (hand_r space). */
	void ApplyTriggerFinger(FPoseContext& Output) const;
	/** First-person grenade throw: moves the left shoulder to LeftShoulderTarget (the arms' rig holds it far right of
	 *  the eye), curls the left fingers round the grenade, then a two-bone IK puts hand_l at LeftHandTarget with the
	 *  elbow toward LeftElbowPole and flexes the wrist (all component space). */
	void ApplyLeftArm(FPoseContext& Output) const;
	/** Two-bone IK of the left arm so hand_l reaches SupportTarget (component space); the hand keeps its animated
	 *  component-space rotation, so it still holds the gun the way the rifle idle does. */
	void ApplySupportHand(FPoseContext& Output) const;
	/** Turns each grip-finger joint of one hand toward that hand's palm by Curls (degrees, negative opens) on top of the
	 *  pose, scaled by Weight. The right index finger is left to ApplyTriggerFinger. */
	void ApplyFingerCurls(FPoseContext& Output, bool bLeft, const float* Curls, float Weight) const;

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
	float TriggerWeight = 0.f;
	FVector TriggerTarget = FVector::ZeroVector;
	float LeftArmWeight = 0.f;
	FVector LeftHandTarget = FVector::ZeroVector;
	FVector LeftElbowPole = FVector::ZeroVector;
	float LeftWristFlex = 0.f;
	FVector LeftShoulderTarget = FVector::ZeroVector;
	float LeftFingerCurl = 0.f;
	float LeftHandRoll = 0.f;
	const UAnimSequenceBase* GripAnimation = nullptr;
	float GripTime = 0.f;
	float SupportWeight = 0.f;
	FVector SupportTarget = FVector::ZeroVector;
	FVector SupportFingers = FVector::ForwardVector;
	FVector SupportPalmNormal = FVector::UpVector;
	float SupportCurl = 0.f;
	/** Per-joint finger curls from the game thread's contact solve (UShieldbreakBodyAnimator::SolveGripContact). */
	float LeftCurls[ShieldbreakGrip::NumJoints] = {};
	float RightCurls[ShieldbreakGrip::NumJoints] = {};
	bool bLeftCurls = false;
	float RightCurlWeight = 0.f;
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

	/** First-person arms: Weight 0-1 of curling the index finger onto the gun's trigger, at Target in hand_r bone
	 *  space. 0 leaves the finger as the animation has it. */
	void SetTriggerFinger(float Weight, const FVector& TargetInHandSpace) { TriggerWeight = Weight; TriggerTarget = TargetInHandSpace; }
	const FVector& GetTriggerTarget() const { return TriggerTarget; }
	/** First-person rifles: Weight 0-1 of reaching the left hand (hand_l) to Target in component space (the gun's
	 *  SupportHand socket). 0 leaves the arm as the animation has it. */
	void SetSupportHand(float Weight, const FVector& PalmComponentSpace, const FVector& FingersComponentSpace, const FVector& PalmNormalComponentSpace,
		float Curl)
	{
		SupportWeight = Weight;
		SupportTarget = PalmComponentSpace;
		SupportFingers = FingersComponentSpace;
		SupportPalmNormal = PalmNormalComponentSpace;
		SupportCurl = Curl;
	}
	const FVector& GetSupportTarget() const { return SupportTarget; }
	const FVector& GetSupportFingers() const { return SupportFingers; }
	const FVector& GetSupportPalmNormal() const { return SupportPalmNormal; }
	float GetSupportCurl() const { return SupportCurl; }
	/** Contact-solved finger curls (ShieldbreakGrip order, degrees). Left: replaces the nominal wrap while the support
	 *  hand holds (null = nominal x Curl). Right: added to the pose's own fingers with Weight. */
	void SetLeftFingerCurls(const float* Curls)
	{
		bLeftCurls = Curls != nullptr;
		if (Curls) { FMemory::Memcpy(LeftCurls, Curls, sizeof(LeftCurls)); }
	}
	void SetRightFingerCurls(const float* Curls, float Weight)
	{
		RightCurlWeight = Curls ? Weight : 0.f;
		if (Curls) { FMemory::Memcpy(RightCurls, Curls, sizeof(RightCurls)); }
	}

	/** First-person arms, grenade throw: Weight 0-1 of driving the left arm. The shoulder moves to ShoulderTarget, the
	 *  hand goes to HandTarget with the elbow bent toward ElbowPole (all mesh component space), the wrist flexes by
	 *  WristFlex degrees about the elbow's hinge and the fingers curl in by FingerCurl (1 = a grip round a grenade).
	 *  Grip, when set, first takes that clip's finger pose at GripTime. */
	void SetLeftArm(float Weight, const FVector& ShoulderTarget, const FVector& HandTarget, const FVector& ElbowPole, float WristFlex,
		float FingerCurl, UAnimSequenceBase* Grip, float InGripTime)
	{
		LeftArmWeight = Weight; LeftShoulderTarget = ShoulderTarget; LeftHandTarget = HandTarget; LeftElbowPole = ElbowPole;
		LeftWristFlex = WristFlex; LeftFingerCurl = FingerCurl; LeftGrip = Grip; LeftGripTime = InGripTime;
	}
	/** Grenade throw: degrees the left hand turns about the forearm (to show the grenade in it rather than the back of the hand). */
	void SetLeftHandRoll(float Degrees) { LeftHandRoll = Degrees; }

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
	float TriggerWeight = 0.f;
	FVector TriggerTarget = FVector::ZeroVector;
	float LeftArmWeight = 0.f;
	FVector LeftHandTarget = FVector::ZeroVector;
	FVector LeftElbowPole = FVector::ZeroVector;
	float LeftWristFlex = 0.f;
	FVector LeftShoulderTarget = FVector::ZeroVector;
	float LeftFingerCurl = 0.f;
	float LeftHandRoll = 0.f;
	UPROPERTY(Transient) TObjectPtr<UAnimSequenceBase> LeftGrip;
	float LeftGripTime = 0.f;
	float SupportWeight = 0.f;
	FVector SupportTarget = FVector::ZeroVector;
	FVector SupportFingers = FVector::ForwardVector;
	FVector SupportPalmNormal = FVector::UpVector;
	float SupportCurl = 0.f;
	float LeftCurls[ShieldbreakGrip::NumJoints] = {};
	float RightCurls[ShieldbreakGrip::NumJoints] = {};
	bool bLeftCurls = false;
	float RightCurlWeight = 0.f;
};
