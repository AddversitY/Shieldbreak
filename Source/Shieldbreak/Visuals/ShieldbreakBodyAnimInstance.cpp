#include "Visuals/ShieldbreakBodyAnimInstance.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BonePose.h"

namespace ShieldbreakBodyAnimPrivate
{
	/** The spine bones that share the aim pitch, root to tip; the head, arms and the gun in the hand follow. */
	const TCHAR* const AimBones[] = { TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05") };
	constexpr float MaxAimPitch = 70.f;
}

// ---- Anim instance (game thread) ----

void UShieldbreakBodyAnimInstance::PlayPose(UAnimSequenceBase* Animation, bool bLoop)
{
	if (!Animation || Animation == RequestedAnimation)
	{
		return;
	}
	FadingAnimation = RequestedAnimation;
	RequestedAnimation = Animation;
	bRequestedLoop = bLoop;
	++RequestCount;
}

// ---- Proxy (worker thread after PreUpdate) ----

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
		Current.Animation = Instance->RequestedAnimation;
		Current.bLoop = Instance->bRequestedLoop;
		Current.Time = 0.f;
		BlendAlpha = bFirst ? 1.f : 0.f;
	}
	Current.PlayRate = Instance->PlayRate;
	AimPitch = FMath::Clamp(Instance->AimPitch, -ShieldbreakBodyAnimPrivate::MaxAimPitch, ShieldbreakBodyAnimPrivate::MaxAimPitch);
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

	Advance(Current, DeltaSeconds);
	if (BlendAlpha < 1.f)
	{
		Advance(Previous, DeltaSeconds);
		BlendAlpha = BlendTime > 0.f ? FMath::Min(1.f, BlendAlpha + DeltaSeconds / BlendTime) : 1.f;
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
	Track.Animation->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(Track.Time), false));
}

bool FShieldbreakBodyAnimProxy::Evaluate(FPoseContext& Output)
{
	if (BlendAlpha >= 1.f || !Previous.Animation)
	{
		Sample(Current, Output);
	}
	else
	{
		FPoseContext CurrentPose(Output);
		FPoseContext FadingPose(Output);
		Sample(Current, CurrentPose);
		Sample(Previous, FadingPose);
		const FAnimationPoseData CurrentData(CurrentPose);
		const FAnimationPoseData FadingData(FadingPose);
		FAnimationPoseData OutputData(Output);
		// Ease in/out so the fade doesn't start or stop abruptly.
		FAnimationRuntime::BlendTwoPosesTogether(CurrentData, FadingData, FMath::SmoothStep(0.f, 1.f, BlendAlpha), OutputData);
		Output.Pose.NormalizeRotations();
	}

	ApplyAimPitch(Output);
	return true;
}

void FShieldbreakBodyAnimProxy::ApplyAimPitch(FPoseContext& Output) const
{
	using namespace ShieldbreakBodyAnimPrivate;
	if (FMath::IsNearlyZero(AimPitch, 0.5f) || AimAxis.IsNearlyZero())
	{
		return;
	}

	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	// Pitch up is a rotation about -right (UE's FRotator convention); each spine bone takes an equal share.
	const FQuat Delta(AimAxis, -FMath::DegreesToRadians(AimPitch / UE_ARRAY_COUNT(AimBones)));

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
