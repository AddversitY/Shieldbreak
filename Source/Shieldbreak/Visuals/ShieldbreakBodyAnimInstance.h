// Native anim instance for the mannequin, so the body can blend without an Animation Blueprint asset.
// UShieldbreakBodyAnimator still picks the locomotion sequence; this instance cross-fades from the previous one
// (no more pops on direction changes) and bends the spine to the aim pitch, so bots and remote players look and
// point their gun where they shoot.

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
	};

	static void Advance(FTrack& Track, float DeltaSeconds);
	static void Sample(const FTrack& Track, FPoseContext& Output);
	void ApplyAimPitch(FPoseContext& Output) const;

	FTrack Current;
	FTrack Previous;
	/** Weight of Current; Previous fades out as it reaches 1. */
	float BlendAlpha = 1.f;
	float BlendTime = 0.18f;
	int32 SeenRequest = -1;
	float AimPitch = 0.f;
	FVector AimAxis = FVector::RightVector;
};

UCLASS(Transient, NotBlueprintable)
class SHIELDBREAK_API UShieldbreakBodyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Switch to Animation, cross-fading from what plays now. Calling it again with the same animation does nothing. */
	void PlayPose(UAnimSequenceBase* Animation, bool bLoop);
	void SetPosePlayRate(float InPlayRate) { PlayRate = InPlayRate; }
	/** Aim pitch in degrees (up is positive) and the pitch axis (the character's right) in mesh component space. */
	void SetAim(float InPitch, const FVector& InAxisComponentSpace) { AimPitch = InPitch; AimAxis = InAxisComponentSpace; }

	UAnimSequenceBase* GetPoseAnimation() const { return RequestedAnimation; }
	float GetAimPitch() const { return AimPitch; }

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
	bool bRequestedLoop = true;
	int32 RequestCount = 0;
	float PlayRate = 1.f;
	float AimPitch = 0.f;
	FVector AimAxis = FVector::RightVector;
};
