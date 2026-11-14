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
	/** The trigger finger, hand to tip. The CCD bends 01 and 02; 03 (the last phalanx) follows 02. */
	const TCHAR* const TriggerHand = TEXT("hand_r");
	const TCHAR* const TriggerChain[] = { TEXT("index_01_r"), TEXT("index_02_r"), TEXT("index_03_r") };
	/** cm from index_03_r's joint to the middle of the finger pad. */
	constexpr float TriggerPadLength = 1.6f;

	FCompactPoseBoneIndex FindCompactBone(const FCompactPose& Pose, const TCHAR* Name)
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(FName(Name));
		return MeshIndex != INDEX_NONE ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex)) : FCompactPoseBoneIndex(INDEX_NONE);
	}

	/** Bone's transform relative to Ancestor (child * parent order), or false when Ancestor isn't above it. */
	bool TransformRelativeTo(const FCompactPose& Pose, FCompactPoseBoneIndex Bone, FCompactPoseBoneIndex Ancestor, FTransform& Out)
	{
		Out = FTransform::Identity;
		for (FCompactPoseBoneIndex Index = Bone; Index.IsValid(); Index = Pose.GetParentBoneIndex(Index))
		{
			if (Index == Ancestor)
			{
				return true;
			}
			Out = Out * Pose[Index];
		}
		return false;
	}
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
	TriggerWeight = FMath::Clamp(Instance->TriggerWeight, 0.f, 1.f);
	TriggerTarget = Instance->TriggerTarget;
	TriggerForward = Instance->TriggerForward;
	LeftArmWeight = FMath::Clamp(Instance->LeftArmWeight, 0.f, 1.f);
	LeftHandTarget = Instance->LeftHandTarget;
	LeftElbowPole = Instance->LeftElbowPole;
	LeftWristFlex = Instance->LeftWristFlex;
	LeftShoulderTarget = Instance->LeftShoulderTarget;
	LeftFingerCurl = Instance->LeftFingerCurl;
	LeftHandRoll = Instance->LeftHandRoll;
	GripAnimation = Instance->LeftGrip;
	GripTime = Instance->LeftGripTime;
	SupportWeight = FMath::Clamp(Instance->SupportWeight, 0.f, 1.f);
	SupportTarget = Instance->SupportTarget;
	SupportFingers = Instance->SupportFingers;
	SupportPalmNormal = Instance->SupportPalmNormal;
	SupportCurl = Instance->SupportCurl;
	bLeftCurls = Instance->bLeftCurls;
	FMemory::Memcpy(LeftCurls, Instance->LeftCurls, sizeof(LeftCurls));
	RightCurlWeight = FMath::Clamp(Instance->RightCurlWeight, 0.f, 1.f);
	FMemory::Memcpy(RightCurls, Instance->RightCurls, sizeof(RightCurls));
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
		LockRoot(Output);
		return;
	}
	Track.Animation->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(Track.Time), false));
	LockRoot(Output);
}

void FShieldbreakBodyAnimProxy::LockRoot(FPoseContext& Output)
{
	// Lyra's locomotion clips can carry root motion (the root bone travels through each cycle). Nothing here extracts
	// it, because the capsule moves the character, so any left in made the body slide ahead of its capsule and snap
	// back on every loop and cross-fade: characters skipped as they moved. Pin the root to the ref pose instead.
	const FCompactPoseBoneIndex Root(0);
	if (Output.Pose.IsValidIndex(Root))
	{
		Output.Pose[Root] = Output.Pose.GetRefPose(Root);
	}
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
	ApplyTriggerFinger(Output);
	ApplySupportHand(Output);
	ApplyLeftArm(Output);
	ApplyFingerCurls(Output, false, RightCurls, RightCurlWeight);
	return true;
}

void FShieldbreakBodyAnimProxy::ApplyLeftArm(FPoseContext& Output) const
{
	using namespace ShieldbreakBodyAnimPrivate;
	if (LeftArmWeight <= 0.01f)
	{
		return;
	}
	FCompactPose& Pose = Output.Pose;
	const FCompactPoseBoneIndex Upper = FindCompactBone(Pose, TEXT("upperarm_l"));
	const FCompactPoseBoneIndex Lower = FindCompactBone(Pose, TEXT("lowerarm_l"));
	const FCompactPoseBoneIndex Hand = FindCompactBone(Pose, TEXT("hand_l"));
	if (!Upper.IsValid() || !Lower.IsValid() || !Hand.IsValid() || Pose.GetParentBoneIndex(Lower) != Upper
		|| Pose.GetParentBoneIndex(Hand) != Lower)
	{
		return;
	}

	// Fingers: the grip pose's local rotations for everything below hand_l (compact poses list parents first).
	if (GripAnimation)
	{
		FPoseContext Grip(Output);
		FTrack Track;
		Track.Animation = GripAnimation;
		Track.Time = FMath::Clamp(GripTime, 0.f, GripAnimation->GetPlayLength());
		Track.bLoop = false;
		Sample(Track, Grip);
		TArray<bool, TInlineAllocator<128>> UnderHand;
		UnderHand.SetNumZeroed(Pose.GetNumBones());
		for (FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
		{
			const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone);
			const bool bUnder = Parent.IsValid() && (Parent == Hand || UnderHand[Parent.GetInt()]);
			UnderHand[Bone.GetInt()] = bUnder;
			if (bUnder)
			{
				Pose[Bone].SetRotation(FQuat::Slerp(Pose[Bone].GetRotation(), Grip.Pose[Bone].GetRotation(), LeftArmWeight).GetNormalized());
			}
		}
	}

	// Component-space transforms of the chain (child * parent order, as in TransformRelativeTo).
	auto ComponentSpace = [&Pose](FCompactPoseBoneIndex Bone)
	{
		FTransform Out = FTransform::Identity;
		for (FCompactPoseBoneIndex Index = Bone; Index.IsValid(); Index = Pose.GetParentBoneIndex(Index))
		{
			Out = Out * Pose[Index];
		}
		return Out;
	};
	const FCompactPoseBoneIndex Shoulder = Pose.GetParentBoneIndex(Upper);

	// Move the shoulder (the clavicle, so the upper arm keeps its length and the skin between them stays together)
	// to where the throw starts from. The rig holds the gun with the body far right of the eye; the stretch toward
	// the spine lands behind the near plane.
	if (Shoulder.IsValid() && !LeftShoulderTarget.IsZero())
	{
		const FCompactPoseBoneIndex Spine = Pose.GetParentBoneIndex(Shoulder);
		const FTransform SpineCS = Spine.IsValid() ? ComponentSpace(Spine) : FTransform::Identity;
		const FTransform ClavicleCS = Pose[Shoulder] * SpineCS;
		const FVector UpperNow = (Pose[Upper] * ClavicleCS).GetLocation();
		const FVector Moved = ClavicleCS.GetLocation() + (LeftShoulderTarget - UpperNow) * LeftArmWeight;
		Pose[Shoulder].SetTranslation(SpineCS.InverseTransformPosition(Moved));
	}
	const FTransform ParentCS = Shoulder.IsValid() ? ComponentSpace(Shoulder) : FTransform::Identity;
	const FTransform UpperCS = Pose[Upper] * ParentCS;
	const FTransform LowerCS = Pose[Lower] * UpperCS;
	const FTransform HandCS = Pose[Hand] * LowerCS;

	// Two-bone IK by the law of cosines, the elbow bending toward the pole.
	const FVector A = UpperCS.GetLocation(), J = LowerCS.GetLocation(), E = HandCS.GetLocation();
	const float L1 = static_cast<float>(FVector::Dist(A, J)), L2 = static_cast<float>(FVector::Dist(J, E));
	FVector Dir = LeftHandTarget - A;
	const float Reach = static_cast<float>(Dir.Size());
	if (L1 < 1.f || L2 < 1.f || Reach < 1.f)
	{
		return;
	}
	Dir /= Reach;
	const float D = FMath::Clamp(Reach, FMath::Abs(L1 - L2) + 1.f, (L1 + L2) * 0.995f);
	FVector Bend = (LeftElbowPole - A) - Dir * FVector::DotProduct(LeftElbowPole - A, Dir);
	if (!Bend.Normalize())
	{
		Bend = (J - A) - Dir * FVector::DotProduct(J - A, Dir);
		if (!Bend.Normalize())
		{
			return;
		}
	}
	const float CosA = FMath::Clamp((L1 * L1 + D * D - L2 * L2) / (2.f * L1 * D), -1.f, 1.f);
	const FVector NewJ = A + Dir * (L1 * CosA) + Bend * (L1 * FMath::Sqrt(1.f - CosA * CosA));
	const FVector NewE = A + Dir * D;

	const FQuat UpperDelta = FQuat::FindBetweenVectors(J - A, NewJ - A);
	const FQuat NewUpper = (UpperDelta * UpperCS.GetRotation()).GetNormalized();
	const FVector MovedHand = NewJ + UpperDelta.RotateVector(E - J);
	const FQuat LowerDelta = FQuat::FindBetweenVectors(MovedHand - NewJ, NewE - NewJ);
	const FQuat NewLower = (LowerDelta * UpperDelta * LowerCS.GetRotation()).GetNormalized();
	FQuat NewHand = (LowerDelta * UpperDelta * HandCS.GetRotation()).GetNormalized();
	// Wrist: cocked back on the wind-up, snapped through on the release, about the elbow's hinge axis.
	const FVector Hinge = FVector::CrossProduct(NewJ - A, NewE - NewJ).GetSafeNormal();
	if (!Hinge.IsNearlyZero() && !FMath::IsNearlyZero(LeftWristFlex))
	{
		NewHand = (FQuat(Hinge, FMath::DegreesToRadians(LeftWristFlex)) * NewHand).GetNormalized();
	}
	const FVector Forearm = (NewE - NewJ).GetSafeNormal();
	if (!Forearm.IsNearlyZero() && !FMath::IsNearlyZero(LeftHandRoll))
	{
		NewHand = (FQuat(Forearm, FMath::DegreesToRadians(LeftHandRoll)) * NewHand).GetNormalized();
	}

	const float W = LeftArmWeight;
	auto Blend = [W](FTransform& Local, const FQuat& Target)
	{
		Local.SetRotation(FQuat::Slerp(Local.GetRotation(), Target, W).GetNormalized());
	};
	Blend(Pose[Upper], ParentCS.GetRotation().Inverse() * NewUpper);
	Blend(Pose[Lower], NewUpper.Inverse() * NewLower);
	Blend(Pose[Hand], NewLower.Inverse() * NewHand);

	// Fingers round the grenade: each joint turns its bone toward the palm side, about the axis across the finger. The
	// palm normal comes from the knuckles (index to pinky, crossed with the hand's length), so no bone axes are assumed.
	if (LeftFingerCurl <= 0.01f)
	{
		return;
	}
	const FCompactPoseBoneIndex Middle1 = FindCompactBone(Pose, TEXT("middle_01_l"));
	const FCompactPoseBoneIndex Index1 = FindCompactBone(Pose, TEXT("index_01_l"));
	const FCompactPoseBoneIndex Pinky1 = FindCompactBone(Pose, TEXT("pinky_01_l"));
	if (!Middle1.IsValid() || !Index1.IsValid() || !Pinky1.IsValid())
	{
		return;
	}
	const FVector HandAt = ComponentSpace(Hand).GetLocation();
	// (Pinky to index) x (hand to knuckles) points out of the palm; the G4 shots, with the operands the other way
	// round, bent the fingers backwards into an open, spread hand.
	const FVector Palm = FVector::CrossProduct(ComponentSpace(Middle1).GetLocation() - HandAt,
		ComponentSpace(Index1).GetLocation() - ComponentSpace(Pinky1).GetLocation()).GetSafeNormal();
	if (Palm.IsNearlyZero())
	{
		return;
	}
	static const TCHAR* const Fingers[][3] = {
		{ TEXT("index_01_l"), TEXT("index_02_l"), TEXT("index_03_l") },
		{ TEXT("middle_01_l"), TEXT("middle_02_l"), TEXT("middle_03_l") },
		{ TEXT("ring_01_l"), TEXT("ring_02_l"), TEXT("ring_03_l") },
		{ TEXT("pinky_01_l"), TEXT("pinky_02_l"), TEXT("pinky_03_l") },
		{ TEXT("thumb_01_l"), TEXT("thumb_02_l"), TEXT("thumb_03_l") },
	};
	// Degrees per joint: a loose fist round a 7 cm ball (the thumb wraps less).
	// G5: the first values left the hand half open with the thumb out.
	static const float Curl[][3] = { { 72.f, 85.f, 55.f }, { 75.f, 88.f, 55.f }, { 78.f, 88.f, 55.f }, { 80.f, 85.f, 55.f }, { 38.f, 48.f, 35.f } };
	for (int32 Finger = 0; Finger < 5; ++Finger)
	{
		for (int32 Joint = 0; Joint < 2; ++Joint)
		{
			const FCompactPoseBoneIndex Bone = FindCompactBone(Pose, Fingers[Finger][Joint]);
			const FCompactPoseBoneIndex Child = FindCompactBone(Pose, Fingers[Finger][Joint + 1]);
			if (!Bone.IsValid() || !Child.IsValid() || Pose.GetParentBoneIndex(Child) != Bone)
			{
				break;
			}
			const FTransform BoneCS = ComponentSpace(Bone);
			const FVector Along = (ComponentSpace(Child).GetLocation() - BoneCS.GetLocation()).GetSafeNormal();
			const FVector Axis = FVector::CrossProduct(Along, Palm).GetSafeNormal();
			if (Axis.IsNearlyZero())
			{
				continue;
			}
			const FQuat Turn(Axis, FMath::DegreesToRadians(Curl[Finger][Joint] * LeftFingerCurl * W));
			const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone);
			const FQuat ParentRotation = Parent.IsValid() ? ComponentSpace(Parent).GetRotation() : FQuat::Identity;
			Pose[Bone].SetRotation((ParentRotation.Inverse() * Turn * BoneCS.GetRotation()).GetNormalized());
		}
		// The last joint follows its parent's bend.
		const FCompactPoseBoneIndex Last = FindCompactBone(Pose, Fingers[Finger][2]);
		const FCompactPoseBoneIndex LastParent = FindCompactBone(Pose, Fingers[Finger][1]);
		if (Last.IsValid() && LastParent.IsValid() && Pose.GetParentBoneIndex(Last) == LastParent)
		{
			const FTransform LastCS = ComponentSpace(Last);
			const FVector Along = (LastCS.GetLocation() - ComponentSpace(LastParent).GetLocation()).GetSafeNormal();
			const FVector Axis = FVector::CrossProduct(Along, Palm).GetSafeNormal();
			if (!Axis.IsNearlyZero())
			{
				const FQuat Turn(Axis, FMath::DegreesToRadians(Curl[Finger][2] * LeftFingerCurl * W));
				Pose[Last].SetRotation((ComponentSpace(LastParent).GetRotation().Inverse() * Turn * LastCS.GetRotation()).GetNormalized());
			}
		}
	}
}

void FShieldbreakBodyAnimProxy::ApplySupportHand(FPoseContext& Output) const
{
	using namespace ShieldbreakBodyAnimPrivate;
	if (SupportWeight <= 0.01f)
	{
		return;
	}
	FCompactPose& Pose = Output.Pose;
	const FCompactPoseBoneIndex Upper = FindCompactBone(Pose, TEXT("upperarm_l"));
	const FCompactPoseBoneIndex Lower = FindCompactBone(Pose, TEXT("lowerarm_l"));
	const FCompactPoseBoneIndex Hand = FindCompactBone(Pose, TEXT("hand_l"));
	if (!Upper.IsValid() || !Lower.IsValid() || !Hand.IsValid() || Pose.GetParentBoneIndex(Lower) != Upper || Pose.GetParentBoneIndex(Hand) != Lower)
	{
		return;
	}
	const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Upper);
	const FCompactPoseBoneIndex None(INDEX_NONE);
	auto ComponentSpace = [&Pose, None](FCompactPoseBoneIndex Bone)
	{
		FTransform Out;
		TransformRelativeTo(Pose, Bone, None, Out);
		return Out;
	};
	const FTransform ParentCS = ComponentSpace(Parent);
	const FTransform UpperCS = ComponentSpace(Upper);
	const FTransform LowerCS = ComponentSpace(Lower);
	const FTransform HandCS = ComponentSpace(Hand);

	// 1) Turn the hand: palm up under the forend, fingers across it (SupportFingers / SupportPalmNormal). The hand's
	// current frame comes from its own joints (wrist, middle knuckle, thumb base), so no bone-axis convention is
	// assumed. Left hand: the palm faces -(fingers x thumb).
	FQuat Turn = FQuat::Identity;
	FVector PalmOffset = FVector::ZeroVector; // wrist -> palm centre, in component space, after the turn
	const FCompactPoseBoneIndex Middle = FindCompactBone(Pose, TEXT("middle_01_l"));
	const FCompactPoseBoneIndex Thumb = FindCompactBone(Pose, TEXT("thumb_01_l"));
	if (Middle.IsValid() && Thumb.IsValid())
	{
		const FVector Wrist = HandCS.GetLocation();
		const FVector Knuckle = ComponentSpace(Middle).GetLocation();
		const FVector F = (Knuckle - Wrist).GetSafeNormal();
		FVector T = ComponentSpace(Thumb).GetLocation() - Wrist;
		T = (T - F * FVector::DotProduct(T, F)).GetSafeNormal();
		const FVector N = -FVector::CrossProduct(F, T).GetSafeNormal();
		if (!F.IsNearlyZero() && !N.IsNearlyZero() && !SupportFingers.IsNearlyZero() && !SupportPalmNormal.IsNearlyZero())
		{
			const FQuat Now = FRotationMatrix::MakeFromXZ(F, N).ToQuat();
			const FQuat Want = FRotationMatrix::MakeFromXZ(SupportFingers, SupportPalmNormal).ToQuat();
			Turn = Want * Now.Inverse();
		}
		PalmOffset = Turn.RotateVector((Knuckle - Wrist) * 0.5f);
	}
	const FVector WristTarget = SupportTarget - PalmOffset;

	// 2) Two-bone IK of the arm so the wrist lands where that palm needs it.
	const FVector A = UpperCS.GetLocation(), B = LowerCS.GetLocation(), C = HandCS.GetLocation();
	const float L1 = static_cast<float>(FVector::Dist(A, B)), L2 = static_cast<float>(FVector::Dist(B, C));
	const FVector ToTarget = WristTarget - A;
	const float Reach = static_cast<float>(ToTarget.Size());
	if (L1 < 1.f || L2 < 1.f || Reach < 1.f)
	{
		return;
	}
	const FVector Dir = ToTarget / Reach;
	const float D = FMath::Clamp(Reach, FMath::Abs(L1 - L2) + 0.5f, L1 + L2 - 0.1f);
	// Bend in the plane the animation's elbow already makes, so the elbow keeps pointing down and out.
	FVector Bend = (B - A) - Dir * FVector::DotProduct(B - A, Dir);
	if (!Bend.Normalize())
	{
		Bend = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	}
	const float CosA = FMath::Clamp((L1 * L1 + D * D - L2 * L2) / (2.f * L1 * D), -1.f, 1.f);
	const FVector NewB = A + (Dir * CosA + Bend * FMath::Sqrt(FMath::Max(0.f, 1.f - CosA * CosA))) * L1;
	const FVector NewC = A + Dir * D;

	const FQuat TurnUpper = FQuat::FindBetweenNormals((B - A).GetSafeNormal(), (NewB - A).GetSafeNormal());
	FTransform NewUpperCS = UpperCS;
	NewUpperCS.SetRotation((TurnUpper * UpperCS.GetRotation()).GetNormalized());
	const FVector LowerDirAfter = TurnUpper.RotateVector((C - B).GetSafeNormal());
	const FQuat TurnLower = FQuat::FindBetweenNormals(LowerDirAfter, (NewC - NewB).GetSafeNormal());
	FTransform NewLowerCS = LowerCS;
	NewLowerCS.SetRotation((TurnLower * TurnUpper * LowerCS.GetRotation()).GetNormalized());
	NewLowerCS.SetLocation(NewB);
	FTransform NewHandCS = HandCS;
	NewHandCS.SetRotation((Turn * HandCS.GetRotation()).GetNormalized());
	NewHandCS.SetLocation(NewC);

	const FTransform UpperLocal = NewUpperCS.GetRelativeTransform(ParentCS);
	const FTransform LowerLocal = NewLowerCS.GetRelativeTransform(NewUpperCS);
	const FTransform HandLocal = NewHandCS.GetRelativeTransform(NewLowerCS);
	auto Blend = [this](FTransform& Local, const FTransform& Target)
	{
		Local.SetRotation(FQuat::Slerp(Local.GetRotation(), Target.GetRotation(), SupportWeight).GetNormalized());
		Local.SetTranslation(FMath::Lerp(Local.GetTranslation(), Target.GetTranslation(), static_cast<double>(SupportWeight)));
	};
	Blend(Pose[Upper], UpperLocal);
	Blend(Pose[Lower], LowerLocal);
	Blend(Pose[Hand], HandLocal);

	// 3) Close the fingers around the forend: the contact-solved curls when the game thread has them, else the nominal
	// wrap scaled by SupportCurl.
	float Curls[ShieldbreakGrip::NumJoints];
	for (int32 I = 0; I < ShieldbreakGrip::NumJoints; ++I)
	{
		Curls[I] = bLeftCurls ? LeftCurls[I] : ShieldbreakGrip::LeftNominal[I] * SupportCurl;
	}
	ApplyFingerCurls(Output, true, Curls, SupportWeight);
}

void FShieldbreakBodyAnimProxy::ApplyFingerCurls(FPoseContext& Output, bool bLeft, const float* Curls, float Weight) const
{
	using namespace ShieldbreakBodyAnimPrivate;
	if (Weight <= 0.01f)
	{
		return;
	}
	FCompactPose& Pose = Output.Pose;
	const FCompactPoseBoneIndex None(INDEX_NONE);
	auto ComponentSpace = [&Pose, None](FCompactPoseBoneIndex Bone)
	{
		FTransform Out;
		TransformRelativeTo(Pose, Bone, None, Out);
		return Out;
	};
	const TCHAR* const* Bones = bLeft ? ShieldbreakGrip::LeftBones : ShieldbreakGrip::RightBones;
	const FCompactPoseBoneIndex Hand = FindCompactBone(Pose, bLeft ? TEXT("hand_l") : TEXT("hand_r"));
	const FCompactPoseBoneIndex Middle = FindCompactBone(Pose, Bones[3]);
	const FCompactPoseBoneIndex Thumb = FindCompactBone(Pose, Bones[12]);
	if (!Hand.IsValid() || !Middle.IsValid() || !Thumb.IsValid())
	{
		return;
	}
	// The palm normal from the hand's own joints: -(fingers x thumb) for a left hand, +(fingers x thumb) for a right.
	const FVector Wrist = ComponentSpace(Hand).GetLocation();
	const FVector F = (ComponentSpace(Middle).GetLocation() - Wrist).GetSafeNormal();
	FVector T = ComponentSpace(Thumb).GetLocation() - Wrist;
	T = (T - F * FVector::DotProduct(T, F)).GetSafeNormal();
	const FVector PalmNormal = (bLeft ? -1.f : 1.f) * FVector::CrossProduct(F, T).GetSafeNormal();
	if (PalmNormal.IsNearlyZero())
	{
		return;
	}
	for (int32 Finger = 0; Finger < 5; ++Finger)
	{
		if (!bLeft && Finger == 0)
		{
			continue; // the trigger finger
		}
		for (int32 J = 0; J < 3; ++J)
		{
			const float Degrees = Curls[Finger * 3 + J];
			const FCompactPoseBoneIndex Bone = FindCompactBone(Pose, Bones[Finger * 3 + J]);
			if (!Bone.IsValid())
			{
				break;
			}
			if (FMath::Abs(Degrees) < 0.05f)
			{
				continue;
			}
			const FTransform BoneCS = ComponentSpace(Bone);
			const FTransform BoneParentCS = ComponentSpace(Pose.GetParentBoneIndex(Bone));
			// The joint's direction: toward the next joint, or for the tip, from the previous one.
			FVector Along;
			if (J < 2)
			{
				const FCompactPoseBoneIndex Next = FindCompactBone(Pose, Bones[Finger * 3 + J + 1]);
				Along = Next.IsValid() ? (ComponentSpace(Next).GetLocation() - BoneCS.GetLocation()) : FVector::ZeroVector;
			}
			else
			{
				Along = BoneCS.GetLocation() - BoneParentCS.GetLocation();
			}
			const FVector Axis = FVector::CrossProduct(Along.GetSafeNormal(), PalmNormal).GetSafeNormal();
			if (Axis.IsNearlyZero())
			{
				continue;
			}
			FTransform NewBoneCS = BoneCS;
			NewBoneCS.SetRotation((FQuat(Axis, FMath::DegreesToRadians(Degrees * Weight)) * BoneCS.GetRotation()).GetNormalized());
			Pose[Bone].SetRotation(NewBoneCS.GetRelativeTransform(BoneParentCS).GetRotation());
		}
	}
}

void FShieldbreakBodyAnimProxy::ApplyTriggerFinger(FPoseContext& Output) const
{
	using namespace ShieldbreakBodyAnimPrivate;
	if (TriggerWeight <= 0.01f)
	{
		return;
	}
	FCompactPose& Pose = Output.Pose;
	const FCompactPoseBoneIndex Hand = FindCompactBone(Pose, TriggerHand);
	const FCompactPoseBoneIndex Chain[] = { FindCompactBone(Pose, TriggerChain[0]), FindCompactBone(Pose, TriggerChain[1]),
		FindCompactBone(Pose, TriggerChain[2]) };
	for (const FCompactPoseBoneIndex& Bone : Chain)
	{
		if (!Bone.IsValid())
		{
			return;
		}
	}
	if (!Hand.IsValid())
	{
		return;
	}

	const FTransform Original[2] = { Pose[Chain[0]], Pose[Chain[1]] };
	auto Pad = [&](FTransform* OutJoints) -> FVector
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			TransformRelativeTo(Pose, Chain[Index], Hand, OutJoints[Index]);
		}
		const FVector Last = OutJoints[2].GetLocation();
		return Last + (Last - OutJoints[1].GetLocation()).GetSafeNormal() * TriggerPadLength;
	};

	// A distance-only CCD solve can fold the middle joint backwards through the
	// grip while its pad still reaches the trigger. Pick the forward bend of the
	// two-link finger explicitly, preserving its lengths and the guard opening.
	FTransform Joints[3];
	const FVector Tip = Pad(Joints);
	const FVector A = Joints[0].GetLocation(), B = Joints[1].GetLocation(), C = Joints[2].GetLocation();
	const double L1 = FVector::Distance(A, B), L2 = FVector::Distance(B, Tip);
	const FVector ToTarget = TriggerTarget - A;
	const double Distance = ToTarget.Size();
	if (L1 < 0.1 || L2 < 0.1 || Distance < 0.1) { return; }
	const FVector Dir = ToTarget / Distance;
	FVector Bend = TriggerForward - Dir * FVector::DotProduct(TriggerForward, Dir);
	if (!Bend.Normalize())
	{
		Bend = ((B - A) - Dir * FVector::DotProduct(B - A, Dir)).GetSafeNormal();
	}
	if (Bend.IsNearlyZero()) { return; }
	const double D = FMath::Clamp(Distance, FMath::Abs(L1 - L2) + 0.01, L1 + L2 - 0.01);
	const double CosA = FMath::Clamp((L1 * L1 + D * D - L2 * L2) / (2.0 * L1 * D), -1.0, 1.0);
	const FVector NewB = A + L1 * (Dir * CosA + Bend * FMath::Sqrt(FMath::Max(0.0, 1.0 - CosA * CosA)));
	const FVector NewTip = A + Dir * D;
	const FQuat TurnRoot = FQuat::FindBetweenNormals((B - A).GetSafeNormal(), (NewB - A).GetSafeNormal());
	const FQuat TurnMiddle = FQuat::FindBetweenNormals(TurnRoot.RotateVector((C - B).GetSafeNormal()), (NewTip - NewB).GetSafeNormal());
	FTransform NewRoot = Joints[0], NewMiddle = Joints[1], Parent;
	NewRoot.SetRotation((TurnRoot * Joints[0].GetRotation()).GetNormalized());
	NewMiddle.SetLocation(NewB);
	NewMiddle.SetRotation((TurnMiddle * TurnRoot * Joints[1].GetRotation()).GetNormalized());
	TransformRelativeTo(Pose, Pose.GetParentBoneIndex(Chain[0]), Hand, Parent);
	Pose[Chain[0]].SetRotation(NewRoot.GetRelativeTransform(Parent).GetRotation());
	Pose[Chain[1]].SetRotation(NewMiddle.GetRelativeTransform(NewRoot).GetRotation());
	for (int32 Joint = 0; Joint < 2; ++Joint)
	{
		Pose[Chain[Joint]].SetRotation(FQuat::Slerp(Original[Joint].GetRotation(), Pose[Chain[Joint]].GetRotation(), TriggerWeight).GetNormalized());
	}
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
