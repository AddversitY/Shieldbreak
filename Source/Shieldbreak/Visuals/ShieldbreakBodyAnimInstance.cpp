#include "Visuals/ShieldbreakBodyAnimInstance.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "Engine/World.h"

namespace ShieldbreakBodyAnimPrivate
{
	/** The spine bones that share the aim pitch, root to tip; the head, arms and the gun in the hand follow. */
	const TCHAR* const AimBones[] = { TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05") };
	constexpr float MaxAimPitch = 70.f;
	/** Actions replace this bone and everything below it (arms, head, the gun hand); legs and pelvis keep moving. */
	const TCHAR* const ActionRootBone = TEXT("spine_01");
}

// ---- Anim instance (game thread) ----

void UShieldbreakBodyAnimInstance::PlayPose(UAnimSequenceBase* Animation, bool bLoop)
{
	if (!Animation || (Animation == RequestedAnimation && DirectionalAnimations.IsEmpty()))
	{
		return;
	}
	FadingAnimation = RequestedAnimation;
	FadingDirectionalAnimations = DirectionalAnimations;
	DirectionalAnimations.Reset();
	RequestedAnimation = Animation;
	bRequestedLoop = bLoop;
	++RequestCount;
}

void UShieldbreakBodyAnimInstance::PlayDirectionalPose(UAnimSequenceBase* Forward, UAnimSequenceBase* Backward,
	UAnimSequenceBase* Left, UAnimSequenceBase* Right, const FVector2D& Direction)
{
	if (!Forward || !Backward || !Left || !Right) { PlayPose(Forward, true); return; }
	const double Total = FMath::Abs(Direction.X) + FMath::Abs(Direction.Y);
	if (Total > UE_SMALL_NUMBER)
	{
		DirectionWeights = FVector4(FMath::Max(0.0, Direction.X) / Total, FMath::Max(0.0, -Direction.X) / Total,
			FMath::Max(0.0, -Direction.Y) / Total, FMath::Max(0.0, Direction.Y) / Total);
	}
	if (RequestedAnimation == Forward && DirectionalAnimations.Num() == 4) { return; }
	FadingAnimation = RequestedAnimation;
	FadingDirectionalAnimations = DirectionalAnimations;
	RequestedAnimation = Forward;
	DirectionalAnimations = { Forward, Backward, Left, Right };
	bRequestedLoop = true;
	++RequestCount;
}

void UShieldbreakBodyAnimInstance::PlayAction(UAnimSequenceBase* Animation, float StartTime, float InPlayRate)
{
	if (!Animation)
	{
		return;
	}
	FadingActionAnimation = ActionAnimation;
	ActionAnimation = Animation;
	ActionStartTime = FMath::Max(0.f, StartTime);
	ActionPlayRate = FMath::Max(0.05f, InPlayRate);
	const UWorld* World = GetWorld();
	ActionEndWorldTime = (World ? World->GetTimeSeconds() : 0.0) + (Animation->GetPlayLength() - ActionStartTime) / ActionPlayRate;
	++ActionRequestCount;
}

UAnimSequenceBase* UShieldbreakBodyAnimInstance::GetActionAnimation() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < ActionEndWorldTime ? ActionAnimation.Get() : nullptr;
}

// ---- Proxy (worker thread after PreUpdate) ----

float FShieldbreakBodyAnimProxy::GetCyclePhase() const
{
	return Current.Animation && Current.Animation->GetPlayLength() > 0.f ? Current.Time / Current.Animation->GetPlayLength() : -1.f;
}

void FShieldbreakBodyAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const UShieldbreakBodyAnimInstance* Instance = CastChecked<UShieldbreakBodyAnimInstance>(InAnimInstance);
	if (Instance->RequestCount != SeenRequest)
	{
		SeenRequest = Instance->RequestCount;
		// The first pose snaps in; later ones fade from the current one.
		const bool bFirst = Current.Animation == nullptr;
		Previous = Current;
		const float Phase = Current.bLoop && Current.Animation && Current.Animation->GetPlayLength() > 0.f
			? Current.Time / Current.Animation->GetPlayLength() : 0.f;
		Current.Animation = Instance->RequestedAnimation;
		Current.bLoop = Instance->bRequestedLoop;
		Current.bDirectional = Instance->DirectionalAnimations.Num() == 4;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Current.Directions[Index] = Current.bDirectional ? Instance->DirectionalAnimations[Index].Get() : nullptr;
		}
		// Preserve the feet's cycle across walk/jog/crouch transitions; turns never restart it.
		Current.Time = Previous.bDirectional && Current.bDirectional && Current.Animation
			? Phase * Current.Animation->GetPlayLength() : 0.f;
		if (!Previous.bDirectional) { Current.Weights = Instance->DirectionWeights; }
		// Interrupting a cross-fade (stop, start, jump in quick succession) would drop the half-faded pose at once.
		bFadeFromSnapshot = !bFirst && BlendAlpha < 1.f && LastBasePose.Num() > 0;
		BlendAlpha = bFirst ? 1.f : 0.f;
	}
	TargetWeights = Instance->DirectionWeights;
	TargetPlayRate = Instance->PlayRate;
	if (Instance->ActionRequestCount != SeenActionRequest)
	{
		SeenActionRequest = Instance->ActionRequestCount;
		PreviousAction = Action;
		ActionTransition = Action.Animation && ActionWeight > 0.f ? 0.f : 1.f;
		Action.Animation = Instance->ActionAnimation;
		Action.Time = Instance->ActionStartTime;
		Action.PlayRate = Instance->ActionPlayRate;
		Action.bLoop = false;
		// Keep the layer's weight, but cross-fade the old and new actions instead of cutting.
	}
	// Shot kicks arrive as a running total; each one is an impulse on the kick spring.
	KickVelocity += (Instance->KickTotal - SeenKickTotal) * 250.f;
	SeenKickTotal = Instance->KickTotal;
	TargetAimPitch = FMath::Clamp(Instance->AimPitch, -ShieldbreakBodyAnimPrivate::MaxAimPitch, ShieldbreakBodyAnimPrivate::MaxAimPitch);
	AimAxis = Instance->AimAxis.GetSafeNormal();
}

void FShieldbreakBodyAnimProxy::Advance(FTrack& Track, float DeltaSeconds)
{
	if (!Track.Animation)
	{
		return;
	}
	const float Length = Track.Animation->GetPlayLength();
	Track.Time += DeltaSeconds * Track.PlayRate;
	if (Length <= KINDA_SMALL_NUMBER)
	{
		Track.Time = 0.f;
	}
	else if (Track.bLoop)
	{
		Track.Time = FMath::Fmod(Track.Time, Length);
		if (Track.Time < 0.f)
		{
			Track.Time += Length;
		}
	}
	else
	{
		Track.Time = FMath::Clamp(Track.Time, 0.f, Length);
	}
}

void FShieldbreakBodyAnimProxy::Update(float DeltaSeconds)
{
	FAnimInstanceProxy::Update(DeltaSeconds);
	const float Follow = 1.f - FMath::Exp(-12.f * FMath::Max(0.f, DeltaSeconds));
	Current.Weights = FMath::Lerp(Current.Weights, TargetWeights, Follow);
	Current.PlayRate = FMath::Lerp(Current.PlayRate, TargetPlayRate, Follow);
	AimPitch = FMath::Lerp(AimPitch, TargetAimPitch, Follow);
	{
		// Kick spring (8 Hz, lightly damped), sub-stepped for low frame rates.
		constexpr float Omega = 2.f * UE_PI * 8.f;
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds * 240.f), 1, 16);
		const float Step = FMath::Max(0.f, DeltaSeconds) / Steps;
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			KickVelocity += (-(Omega * Omega) * KickPitch - 2.f * 0.6f * Omega * KickVelocity) * Step;
			KickPitch += KickVelocity * Step;
		}
	}

	Advance(Current, DeltaSeconds);
	if (BlendAlpha < 1.f)
	{
		Advance(Previous, DeltaSeconds);
		BlendAlpha = BlendTime > 0.f ? FMath::Min(1.f, BlendAlpha + DeltaSeconds / BlendTime) : 1.f;
	}

	if (Action.Animation)
	{
		if (ActionTransition < 1.f)
		{
			Advance(PreviousAction, DeltaSeconds);
			ActionTransition = FMath::Min(1.f, ActionTransition + DeltaSeconds / 0.1f);
		}
		Advance(Action, DeltaSeconds);
		const float Remaining = (Action.Animation->GetPlayLength() - Action.Time) / FMath::Max(0.05f, Action.PlayRate);
		if (Remaining <= 0.f)
		{
			Action.Animation = nullptr;
			ActionWeight = 0.f;
		}
		else if (Remaining < ActionBlendOut)
		{
			ActionWeight = FMath::Min(ActionWeight, Remaining / ActionBlendOut);
		}
		else
		{
			ActionWeight = FMath::Min(1.f, ActionWeight + DeltaSeconds / ActionBlendIn);
		}
	}
}

void FShieldbreakBodyAnimProxy::Sample(const FTrack& Track, FPoseContext& Output)
{
	if (!Track.Animation)
	{
		Output.ResetToRefPose();
		return;
	}
	FAnimationPoseData PoseData(Output);
	if (Track.bDirectional)
	{
		const float Phase = Track.Animation->GetPlayLength() > 0.f ? Track.Time / Track.Animation->GetPlayLength() : 0.f;
		float TotalWeight = 0.f;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const float Weight = static_cast<float>(Track.Weights[Index]);
			const UAnimSequenceBase* Clip = Track.Directions[Index];
			if (!Clip || Weight < 0.001f) { continue; }
			if (TotalWeight == 0.f)
			{
				Clip->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(Phase * Clip->GetPlayLength()), false));
			}
			else
			{
				FPoseContext Next(Output);
				FAnimationPoseData NextData(Next);
				Clip->GetAnimationPose(NextData, FAnimExtractContext(static_cast<double>(Phase * Clip->GetPlayLength()), false));
				FAnimationRuntime::BlendTwoPosesTogetherInPlace(PoseData, NextData, TotalWeight / (TotalWeight + Weight));
			}
			TotalWeight += Weight;
		}
		if (TotalWeight == 0.f) { Output.ResetToRefPose(); }
		Output.Pose.NormalizeRotations();
		return;
	}
	Track.Animation->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(Track.Time), false));
}

bool FShieldbreakBodyAnimProxy::Evaluate(FPoseContext& Output)
{
	const bool bSnapshotFits = bFadeFromSnapshot && LastBasePose.Num() == Output.Pose.GetNumBones();
	if (BlendAlpha >= 1.f || (!Previous.Animation && !bSnapshotFits))
	{
		Sample(Current, Output);
	}
	else
	{
		FPoseContext CurrentPose(Output);
		FPoseContext FadingPose(Output);
		Sample(Current, CurrentPose);
		if (bSnapshotFits)
		{
			for (FCompactPoseBoneIndex Bone : FadingPose.Pose.ForEachBoneIndex())
			{
				FadingPose.Pose[Bone] = LastBasePose[Bone.GetInt()];
			}
			FadingPose.Curve = CurrentPose.Curve;
		}
		else
		{
			Sample(Previous, FadingPose);
		}
		const FAnimationPoseData CurrentData(CurrentPose);
		const FAnimationPoseData FadingData(FadingPose);
		FAnimationPoseData OutputData(Output);
		// Ease in/out so the fade doesn't start or stop abruptly.
		FAnimationRuntime::BlendTwoPosesTogether(CurrentData, FadingData, FMath::SmoothStep(0.f, 1.f, BlendAlpha), OutputData);
		Output.Pose.NormalizeRotations();
	}

	LastBasePose.SetNum(Output.Pose.GetNumBones(), EAllowShrinking::No);
	for (FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
	{
		LastBasePose[Bone.GetInt()] = Output.Pose[Bone];
	}

	ApplyAction(Output);
	ApplyAimPitch(Output);
	return true;
}

void FShieldbreakBodyAnimProxy::ApplyAction(FPoseContext& Output) const
{
	if (!Action.Animation || ActionWeight <= 0.f)
	{
		return;
	}

	FPoseContext ActionPose(Output);
	Sample(Action, ActionPose);
	if (PreviousAction.Animation && ActionTransition < 1.f)
	{
		FPoseContext OldAction(Output);
		Sample(PreviousAction, OldAction);
		FAnimationPoseData NewData(ActionPose), OldData(OldAction);
		FAnimationRuntime::BlendTwoPosesTogetherInPlace(NewData, OldData, FMath::SmoothStep(0.f, 1.f, ActionTransition));
	}

	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	const int32 RootMeshIndex = Bones.GetPoseBoneIndexForBoneName(FName(ShieldbreakBodyAnimPrivate::ActionRootBone));
	const FCompactPoseBoneIndex Root = RootMeshIndex != INDEX_NONE ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(RootMeshIndex)) : FCompactPoseBoneIndex(INDEX_NONE);
	const float Weight = FMath::SmoothStep(0.f, 1.f, ActionWeight);

	// Compact pose bones are ordered parents first, so one pass marks the root's whole branch.
	TArray<bool, TInlineAllocator<128>> InBranch;
	InBranch.SetNumZeroed(Pose.GetNumBones());
	for (FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
	{
		const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone);
		const bool bLayered = !Root.IsValid() || Bone == Root || (Parent.IsValid() && InBranch[Parent.GetInt()]);
		InBranch[Bone.GetInt()] = bLayered;
		if (bLayered)
		{
			Pose[Bone].BlendWith(ActionPose.Pose[Bone], Weight);
		}
	}
	Pose.NormalizeRotations();
}

void FShieldbreakBodyAnimProxy::ApplyAimPitch(FPoseContext& Output) const
{
	using namespace ShieldbreakBodyAnimPrivate;
	const float TotalPitch = AimPitch + KickPitch;
	if (FMath::IsNearlyZero(TotalPitch, 0.5f) || AimAxis.IsNearlyZero())
	{
		return;
	}

	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	// Pitch up is a rotation about -right (UE's FRotator convention); each spine bone takes an equal share.
	const FQuat Delta(AimAxis, -FMath::DegreesToRadians(TotalPitch / UE_ARRAY_COUNT(AimBones)));

	for (const TCHAR* BoneName : AimBones)
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(FName(BoneName));
		if (MeshIndex == INDEX_NONE)
		{
			continue;
		}
		const FCompactPoseBoneIndex Bone = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		if (!Bone.IsValid())
		{
			continue;
		}

		// Component-space rotation of the parent, from the (already bent) local pose.
		FQuat ParentRotation = FQuat::Identity;
		for (FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone); Parent.IsValid(); Parent = Pose.GetParentBoneIndex(Parent))
		{
			ParentRotation = Pose[Parent].GetRotation() * ParentRotation;
		}

		// Rotate the bone by Delta in component space: Local' = Parent^-1 * Delta * Parent * Local.
		FTransform& Local = Pose[Bone];
		Local.SetRotation((ParentRotation.Inverse() * Delta * ParentRotation * Local.GetRotation()).GetNormalized());
	}
}
