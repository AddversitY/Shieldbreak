#include "Visuals/ShieldbreakBodyAnimator.h"

#include "Shieldbreak.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/ShieldbreakCharacter.h"
#include "Visuals/ShieldbreakArt.h"
#include "Visuals/ShieldbreakBodyAnimInstance.h"
#include "Weapons/ShieldbreakWeapon.h"

UShieldbreakBodyAnimator::UShieldbreakBodyAnimator()
{
	PrimaryComponentTick.bCanEverTick = true;
}

USkeletalMeshComponent* UShieldbreakBodyAnimator::GetBodyMesh() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character ? Character->GetMesh() : nullptr;
}

void UShieldbreakBodyAnimator::BeginPlay()
{
	Super::BeginPlay();

	USkeletalMeshComponent* Body = GetBodyMesh();
	USkeletalMesh* Manny = ShieldbreakArt::Load<USkeletalMesh>(ShieldbreakArt::MannyMesh);
	if (!Body || !Manny)
	{
		SetComponentTickEnabled(false);
		return;
	}

	Body->SetSkeletalMeshAsset(Manny);
	// Our native anim instance cross-fades between the sequences ChoosePose picks and bends the spine to the aim.
	Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Body->SetAnimInstanceClass(UShieldbreakBodyAnimInstance::StaticClass());
	// The local player never sees their own body, but it still casts their shadow.
	Body->SetOwnerNoSee(true);
	Body->bCastHiddenShadow = true;
	bHasBodyArt = true;
	AttachArmor(Body, false);

	// On clients the equipped weapon can replicate before the mesh exists; re-attach it to the hand now.
	if (const AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner()))
	{
		if (AShieldbreakWeapon* Weapon = OwnerCharacter->GetCurrentWeapon())
		{
			Weapon->OnEquipped();
		}
	}

	// Animation is cosmetic, so a dedicated server doesn't need to tick it.
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
	}
}

void UShieldbreakBodyAnimator::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USkeletalMeshComponent* Body = GetBodyMesh();
	const UWorld* World = GetWorld();
	if (!bHasBodyArt || bRagdoll || !Body || !World)
	{
		return;
	}
	UpdateFirstPersonArms();

	float PlayRate = 1.f;
	const EShieldbreakBodyPose Pose = ChoosePose(PlayRate);
	const float Now = World->GetTimeSeconds();

	UShieldbreakBodyAnimInstance* AnimInstance = Cast<UShieldbreakBodyAnimInstance>(Body->GetAnimInstance());
	if (Pose != CurrentPose && Now - LastPoseChangeTime >= MinPoseTime)
	{
		if (UAnimSequenceBase* Animation = GetPoseAnimation(Pose))
		{
			const bool bLoop = Pose != EShieldbreakBodyPose::Apex;
			if (AnimInstance)
			{
				AnimInstance->PlayPose(Animation, bLoop);
			}
			else
			{
				// No anim instance (shouldn't happen): fall back to single-node playback, no blending.
				Body->PlayAnimation(Animation, bLoop);
			}
			CurrentPose = Pose;
			LastPoseChangeTime = Now;
		}
	}
	if (!AnimInstance)
	{
		Body->SetPlayRate(PlayRate);
		return;
	}
	AnimInstance->SetPosePlayRate(PlayRate);

	// Aim pitch: the replicated view pitch for remote players and bots. The local player never sees their own body,
	// but their shadow and other players' view of them use the same path.
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		const float Pitch = FRotator::NormalizeAxis(Pawn->GetBaseAimRotation().Pitch);
		const FVector RightInComponent = Body->GetComponentTransform().InverseTransformVectorNoScale(Pawn->GetActorRightVector());
		AnimInstance->SetAim(Pitch, RightInComponent);
	}
}

float UShieldbreakBodyAnimator::GetAimPitch() const
{
	const USkeletalMeshComponent* Body = GetBodyMesh();
	const UShieldbreakBodyAnimInstance* AnimInstance = Body ? Cast<UShieldbreakBodyAnimInstance>(Body->GetAnimInstance()) : nullptr;
	return AnimInstance ? AnimInstance->GetAimPitch() : 0.f;
}

bool UShieldbreakBodyAnimator::IsBlendedAnimation() const
{
	const USkeletalMeshComponent* Body = GetBodyMesh();
	const UShieldbreakBodyAnimInstance* AnimInstance = Body ? Cast<UShieldbreakBodyAnimInstance>(Body->GetAnimInstance()) : nullptr;
	return AnimInstance && AnimInstance->GetPoseAnimation() != nullptr;
}

EShieldbreakBodyPose UShieldbreakBodyAnimator::ChoosePose(float& OutPlayRate) const
{
	OutPlayRate = 1.f;

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return EShieldbreakBodyPose::Idle;
	}

	const FVector Velocity = Character->GetVelocity();
	if (Movement->IsFalling())
	{
		if (Velocity.Z > 150.f)
		{
			return EShieldbreakBodyPose::Rising;
		}
		return Velocity.Z > -150.f ? EShieldbreakBodyPose::Apex : EShieldbreakBodyPose::Falling;
	}

	const float Speed = static_cast<float>(Velocity.Size2D());
	const bool bCrouched = Character->bIsCrouched;
	if (Speed < 25.f)
	{
		return bCrouched ? EShieldbreakBodyPose::CrouchIdle : EShieldbreakBodyPose::Idle;
	}

	// Direction relative to facing, bucketed into the four authored directions.
	const FVector Local = Character->GetActorRotation().UnrotateVector(Velocity);
	const bool bForwardAxis = FMath::Abs(Local.X) >= FMath::Abs(Local.Y);
	const int32 Direction = bForwardAxis ? (Local.X >= 0.f ? 0 : 1) : (Local.Y >= 0.f ? 3 : 2); // 0 fwd, 1 bwd, 2 left, 3 right

	if (bCrouched)
	{
		OutPlayRate = FMath::Clamp(Speed / CrouchAnimSpeed, 0.5f, 2.f);
		static const EShieldbreakBodyPose CrouchPoses[] = { EShieldbreakBodyPose::CrouchFwd, EShieldbreakBodyPose::CrouchBwd, EShieldbreakBodyPose::CrouchLeft, EShieldbreakBodyPose::CrouchRight };
		return CrouchPoses[Direction];
	}
	if (Speed < JogThreshold)
	{
		OutPlayRate = FMath::Clamp(Speed / WalkAnimSpeed, 0.5f, 2.f);
		static const EShieldbreakBodyPose WalkPoses[] = { EShieldbreakBodyPose::WalkFwd, EShieldbreakBodyPose::WalkBwd, EShieldbreakBodyPose::WalkLeft, EShieldbreakBodyPose::WalkRight };
		return WalkPoses[Direction];
	}

	OutPlayRate = FMath::Clamp(Speed / JogAnimSpeed, 0.6f, 2.f);
	static const EShieldbreakBodyPose JogPoses[] = { EShieldbreakBodyPose::JogFwd, EShieldbreakBodyPose::JogBwd, EShieldbreakBodyPose::JogLeft, EShieldbreakBodyPose::JogRight };
	return JogPoses[Direction];
}

UAnimSequenceBase* UShieldbreakBodyAnimator::GetPoseAnimation(EShieldbreakBodyPose Pose) const
{
	const TCHAR* Name = nullptr;
	switch (Pose)
	{
	case EShieldbreakBodyPose::Idle: Name = TEXT("Idle_Hipfire"); break;
	case EShieldbreakBodyPose::WalkFwd: Name = TEXT("Walk_Fwd"); break;
	case EShieldbreakBodyPose::WalkBwd: Name = TEXT("Walk_Bwd"); break;
	case EShieldbreakBodyPose::WalkLeft: Name = TEXT("Walk_Left"); break;
	case EShieldbreakBodyPose::WalkRight: Name = TEXT("Walk_Right"); break;
	case EShieldbreakBodyPose::JogFwd: Name = TEXT("Jog_Fwd"); break;
	case EShieldbreakBodyPose::JogBwd: Name = TEXT("Jog_Bwd"); break;
	case EShieldbreakBodyPose::JogLeft: Name = TEXT("Jog_Left"); break;
	case EShieldbreakBodyPose::JogRight: Name = TEXT("Jog_Right"); break;
	case EShieldbreakBodyPose::CrouchIdle: Name = TEXT("Crouch_Idle"); break;
	case EShieldbreakBodyPose::CrouchFwd: Name = TEXT("Crouch_Walk_Fwd"); break;
	case EShieldbreakBodyPose::CrouchBwd: Name = TEXT("Crouch_Walk_Bwd"); break;
	case EShieldbreakBodyPose::CrouchLeft: Name = TEXT("Crouch_Walk_Left"); break;
	case EShieldbreakBodyPose::CrouchRight: Name = TEXT("Crouch_Walk_Right"); break;
	case EShieldbreakBodyPose::Rising: Name = TEXT("Jump_Start_Loop"); break;
	case EShieldbreakBodyPose::Apex: Name = TEXT("Jump_Apex"); break;
	case EShieldbreakBodyPose::Falling: Name = TEXT("Jump_Fall_Loop"); break;
	default: return nullptr;
	}

	// e.g. /Game/.../Rifle/MM_Rifle_Jog_Fwd.MM_Rifle_Jog_Fwd
	const FString Asset = FString(TEXT("MM_Rifle_")) + Name;
	const FString Path = FString(ShieldbreakArt::RifleLocomotionPrefix) + Name + TEXT(".") + Asset;
	return ShieldbreakArt::Load<UAnimSequenceBase>(*Path);
}

void UShieldbreakBodyAnimator::StartRagdoll(const FVector& Impulse)
{
	USkeletalMeshComponent* Body = GetBodyMesh();
	if (!bHasBodyArt || bRagdoll || !Body)
	{
		return;
	}
	bRagdoll = true;

	if (FirstPersonArms)
	{
		FirstPersonArms->SetHiddenInGame(true);
		for (UStaticMeshComponent* Piece : FirstPersonArmor)
		{
			Piece->SetHiddenInGame(true);
		}
	}
	Body->SetOwnerNoSee(false);
	for (UStaticMeshComponent* Piece : ArmorPieces)
	{
		Piece->SetOwnerNoSee(false);
	}
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	Body->bBlendPhysics = true;
	Body->AddImpulse(Impulse, NAME_None, true);
}

namespace ShieldbreakBodyAnimatorPrivate
{
	/** Bones that carry an armor piece; the mesh is SM_SB_Armor_<bone> in /Game/Shieldbreak/Art/Armor. */
	const TCHAR* const ArmorBones[] = {
		TEXT("head"), TEXT("spine_05"), TEXT("spine_03"), TEXT("pelvis"),
		TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r"),
		TEXT("thigh_l"), TEXT("thigh_r"), TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"),
	};
	/** For performance A/B runs: sb.Armor 0 spawns soldiers without armor, sb.ArmorShadows 0 keeps it from casting. */
	TAutoConsoleVariable<int32> CVarArmor(TEXT("sb.Armor"), 1, TEXT("1: soldiers wear the armor set (applies to new spawns)."));
	TAutoConsoleVariable<int32> CVarArmorShadows(TEXT("sb.ArmorShadows"), 1, TEXT("1: armor pieces cast shadows (new spawns)."));

	/** Plates are a little darker than the HUD's team color so they don't wash out in sunlight. */
	constexpr float PlateBrightness = 0.55f;
	constexpr float GlowBrightness = 1.6f;

	/** The bone's reference-pose transform in component (mesh) space. */
	FTransform RefPoseComponentSpace(const FReferenceSkeleton& RefSkeleton, int32 BoneIndex)
	{
		FTransform Result = FTransform::Identity;
		while (BoneIndex != INDEX_NONE)
		{
			Result = Result * RefSkeleton.GetRefBonePose()[BoneIndex];
			BoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
		}
		return Result;
	}
}

void UShieldbreakBodyAnimator::AttachArmor(USkeletalMeshComponent* Body, bool bFirstPerson)
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	TArray<TObjectPtr<UStaticMeshComponent>>& Pieces = bFirstPerson ? FirstPersonArmor : ArmorPieces;
	const USkeletalMesh* Asset = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || Pieces.Num() > 0 || CVarArmor.GetValueOnGameThread() == 0
		|| (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer))
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
	for (const TCHAR* Bone : ArmorBones)
	{
		if (bFirstPerson && !FCString::Strstr(Bone, TEXT("arm_")))
		{
			continue; // the arms only wear the upper/lower arm pieces
		}
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
		const FString Name = FString(TEXT("SM_SB_Armor_")) + Bone;
		const FString Path = FString(TEXT("/Game/Shieldbreak/Art/Armor/")) + Name + TEXT(".") + Name;
		UStaticMesh* Mesh = BoneIndex != INDEX_NONE ? ShieldbreakArt::Load<UStaticMesh>(*Path) : nullptr;
		if (!Mesh)
		{
			continue;
		}

		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(GetOwner(), *(FString(bFirstPerson ? TEXT("FPArmor_") : TEXT("Armor_")) + Bone));
		Piece->SetStaticMesh(Mesh);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetGenerateOverlapEvents(false);
		Piece->SetCanEverAffectNavigation(false);
		if (bFirstPerson)
		{
			Piece->SetOnlyOwnerSee(true);
			Piece->SetCastShadow(false);
			Piece->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		}
		else
		{
			Piece->SetOwnerNoSee(true);
			Piece->bCastHiddenShadow = true;
			Piece->SetCastShadow(CVarArmorShadows.GetValueOnGameThread() != 0);
		}
		Piece->SetupAttachment(Body, Bone);
		// The piece is modeled where it sits on the reference pose, so undo the bone's reference transform.
		Piece->SetRelativeTransform(RefPoseComponentSpace(RefSkeleton, BoneIndex).Inverse());
		Piece->RegisterComponent();
		Pieces.Add(Piece);

		const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
		for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
		{
			const FString SlotName = Slots[Slot].MaterialSlotName.ToString();
			const bool bGlow = SlotName.StartsWith(TEXT("M_TeamGlow"));
			if (bGlow || SlotName == TEXT("M_Team") || SlotName.StartsWith(TEXT("M_Team_")))
			{
				if (UMaterialInstanceDynamic* MID = Piece->CreateDynamicMaterialInstance(Slot))
				{
					(bGlow ? ArmorGlowMIDs : ArmorTeamMIDs).Add(MID);
				}
			}
		}
	}
	if (Pieces.Num() > 0)
	{
		UE_LOG(LogShieldbreak, Log, TEXT("%s: %d %sarmor pieces"), *GetOwner()->GetName(), Pieces.Num(), bFirstPerson ? TEXT("first-person ") : TEXT(""));
	}
}

void UShieldbreakBodyAnimator::SetArmorColor(const FLinearColor& TeamColor)
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	for (UMaterialInstanceDynamic* MID : ArmorTeamMIDs)
	{
		MID->SetVectorParameterValue(TEXT("BaseColor"), TeamColor * PlateBrightness);
	}
	for (UMaterialInstanceDynamic* MID : ArmorGlowMIDs)
	{
		MID->SetVectorParameterValue(TEXT("BaseColor"), TeamColor);
		MID->SetVectorParameterValue(TEXT("EmissiveColor"), TeamColor * GlowBrightness);
	}
}

void UShieldbreakBodyAnimator::SetArmorHidden(bool bHidden)
{
	for (UStaticMeshComponent* Piece : ArmorPieces)
	{
		Piece->SetHiddenInGame(bHidden);
	}
}

// ---- Actions and first-person arms ----

namespace ShieldbreakBodyAnimatorPrivate
{
	TAutoConsoleVariable<int32> CVarFirstPersonArms(TEXT("sb.FPArms"), 1, TEXT("1: the local player sees first-person arms holding the gun; 0: the old floating viewmodel."));
	// Where the gun hand sits in camera space (cm forward, right, down) and where the barrel points (degrees; yaw
	// negative = in toward the crosshair). Live-tunable; the arms re-place themselves when these change.
	TAutoConsoleVariable<float> CVarGripForward(TEXT("sb.FPGripForward"), 34.f, TEXT("First-person gun hand: cm in front of the eye."));
	TAutoConsoleVariable<float> CVarThrowDip(TEXT("sb.FPThrowDip"), -35.f, TEXT("First-person grenade throw: degrees the arm rig pitches during the throw (negative tips it up so the left arm's swing crosses the screen; tuned on the PC)."));
	TAutoConsoleVariable<float> CVarGripRight(TEXT("sb.FPGripRight"), 13.f, TEXT("First-person gun hand: cm right of the eye."));
	TAutoConsoleVariable<float> CVarGripDown(TEXT("sb.FPGripDown"), 15.f, TEXT("First-person gun hand: cm below the eye."));
	TAutoConsoleVariable<float> CVarAimPitch(TEXT("sb.FPAimPitch"), 1.5f, TEXT("First-person barrel pitch (degrees, up positive)."));
	TAutoConsoleVariable<float> CVarAimYaw(TEXT("sb.FPAimYaw"), -3.f, TEXT("First-person barrel yaw (degrees, negative toward the crosshair)."));

	const TCHAR* const ActionsFolder = TEXT("/Game/Characters/Heroes/Mannequin/Animations/Actions/");
	const TCHAR* const PistolIdle = TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Pistol/MM_Pistol_Idle_Hipfire.MM_Pistol_Idle_Hipfire");
	/** Bones hidden on the arms: the head (we look out of it) and the legs. */
	const TCHAR* const FirstPersonHiddenBones[] = { TEXT("neck_01"), TEXT("thigh_l"), TEXT("thigh_r") };

	/** How each action plays: start a little in (skipping the wind-up) and faster than authored, Halo-snappy. */
	struct FActionTiming { float StartTime; float PlayRate; };
	FActionTiming GetActionTiming(EShieldbreakAction Action)
	{
		switch (Action)
		{
		case EShieldbreakAction::GrenadeThrow: return { 0.1f, 0.9f };
		case EShieldbreakAction::Melee: return { 0.f, 1.5f };
		case EShieldbreakAction::Equip: return { 0.f, 1.4f };
		default: return { 0.f, 1.f };
		}
	}
}

bool UShieldbreakBodyAnimator::UsesPistolAnimations() const
{
	const AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner());
	const AShieldbreakWeapon* Weapon = OwnerCharacter ? OwnerCharacter->GetCurrentWeapon() : nullptr;
	return Weapon && Weapon->GetStats().bPistolAnimations;
}

UAnimSequenceBase* UShieldbreakBodyAnimator::GetActionAnimation(EShieldbreakAction Action) const
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	const TCHAR* Name = nullptr;
	switch (Action)
	{
	case EShieldbreakAction::GrenadeThrow: Name = TEXT("GrenadeToss"); break;
	case EShieldbreakAction::Melee: Name = TEXT("Melee"); break;
	case EShieldbreakAction::Reload: Name = TEXT("Reload"); break;
	case EShieldbreakAction::Equip: Name = TEXT("Equip"); break;
	}
	// Pistol version first when holding the sidearm, else (or if it doesn't exist) the rifle one. Lyra has no pistol
	// grenade toss, so that one always uses the rifle's (and doesn't log a missing asset on every throw).
	const bool bPistol = UsesPistolAnimations() && Action != EShieldbreakAction::GrenadeThrow;
	for (const TCHAR* Set : { bPistol ? TEXT("Pistol") : TEXT("Rifle"), TEXT("Rifle") })
	{
		const FString Asset = FString::Printf(TEXT("MM_%s_%s"), Set, Name);
		const FString Path = FString(ActionsFolder) + Asset + TEXT(".") + Asset;
		if (UAnimSequenceBase* Animation = ShieldbreakArt::Load<UAnimSequenceBase>(*Path))
		{
			return Animation;
		}
	}
	return nullptr;
}

UAnimSequenceBase* UShieldbreakBodyAnimator::GetFirstPersonIdle() const
{
	if (UsesPistolAnimations())
	{
		if (UAnimSequenceBase* Pistol = ShieldbreakArt::Load<UAnimSequenceBase>(ShieldbreakBodyAnimatorPrivate::PistolIdle))
		{
			return Pistol;
		}
	}
	return GetPoseAnimation(EShieldbreakBodyPose::Idle);
}

void UShieldbreakBodyAnimator::PlayAction(EShieldbreakAction Action, float Duration)
{
	if (!bHasBodyArt || bRagdoll)
	{
		return;
	}
	UAnimSequenceBase* Animation = GetActionAnimation(Action);
	if (!Animation)
	{
		return;
	}
	ShieldbreakBodyAnimatorPrivate::FActionTiming Timing = ShieldbreakBodyAnimatorPrivate::GetActionTiming(Action);
	Timing.StartTime = FMath::Min(Timing.StartTime, Animation->GetPlayLength() * 0.5f);
	if (Duration > 0.f)
	{
		Timing.PlayRate = (Animation->GetPlayLength() - Timing.StartTime) / Duration;
	}
	if (Action == EShieldbreakAction::GrenadeThrow && GetWorld())
	{
		FirstPersonThrowDuration = (Animation->GetPlayLength() - Timing.StartTime) / FMath::Max(0.05f, Timing.PlayRate);
		FirstPersonThrowEnd = GetWorld()->GetTimeSeconds() + FirstPersonThrowDuration;
	}
	if (Action == EShieldbreakAction::Equip && GetWorld())
	{
		FirstPersonEquipEnd = GetWorld()->GetTimeSeconds() + (Animation->GetPlayLength() - Timing.StartTime) / FMath::Max(0.05f, Timing.PlayRate);
		// Drop the arms at once, before the equip pose reaches the screen.
		SwapLower = 1.f;
		if (FirstPersonArms && CalibratedIdle)
		{
			ApplyFirstPersonArmsTransform(AppliedThrowDip);
		}
	}
	for (USkeletalMeshComponent* Mesh : { GetBodyMesh(), FirstPersonArms.Get() })
	{
		if (UShieldbreakBodyAnimInstance* Instance = Mesh ? Cast<UShieldbreakBodyAnimInstance>(Mesh->GetAnimInstance()) : nullptr)
		{
			Instance->PlayAction(Animation, Timing.StartTime, Timing.PlayRate);
		}
	}
}

void UShieldbreakBodyAnimator::ApplyFirstPersonArmsTransform(float Dip)
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	const float Pitch = CVarThrowDip.GetValueOnGameThread() * Dip;
	// Eased so the rise at the end of a swap starts quickly and settles softly.
	const float Lower = FMath::InterpEaseOut(0.f, 1.f, SwapLower, 2.f) * 45.f;
	FirstPersonArms->SetRelativeTransform(CalibratedArmsTransform * FTransform(FRotator(-Pitch, 0.f, Pitch * 0.4f))
		* FTransform(FVector(0.f, 0.f, -Lower)));
	AppliedThrowDip = Dip;
	AppliedSwapLower = SwapLower;
}

UAnimSequenceBase* UShieldbreakBodyAnimator::GetPlayingAction() const
{
	const USkeletalMeshComponent* Body = GetBodyMesh();
	const UShieldbreakBodyAnimInstance* Instance = Body ? Cast<UShieldbreakBodyAnimInstance>(Body->GetAnimInstance()) : nullptr;
	return Instance ? Instance->GetActionAnimation() : nullptr;
}

USkeletalMeshComponent* UShieldbreakBodyAnimator::GetFirstPersonArms() const
{
	return FirstPersonArms;
}

bool UShieldbreakBodyAnimator::HasFirstPersonArms() const
{
	return FirstPersonArms && FirstPersonArms->IsVisible() && !bRagdoll;
}

void UShieldbreakBodyAnimator::SetFirstPersonArmsVisible(bool bVisible)
{
	// Not propagated: the gun in the hand is a child too, and must keep its own visibility.
	FirstPersonArms->SetVisibility(bVisible);
	for (UStaticMeshComponent* Piece : FirstPersonArmor)
	{
		Piece->SetVisibility(bVisible);
	}
}

void UShieldbreakBodyAnimator::UpdateFirstPersonArms()
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner());
	USkeletalMeshComponent* Body = GetBodyMesh();
	const UWorld* World = GetWorld();
	if (!OwnerCharacter || !Body || !World)
	{
		return;
	}

	const bool bWanted = OwnerCharacter->IsLocallyControlled() && OwnerCharacter->IsPlayerControlled()
		&& OwnerCharacter->GetFirstPersonCamera() && CVarFirstPersonArms.GetValueOnGameThread() != 0;
	if (!bWanted)
	{
		if (FirstPersonArms && FirstPersonArms->IsVisible())
		{
			SetFirstPersonArmsVisible(false);
		}
		return;
	}

	if (!FirstPersonArms)
	{
		FirstPersonArms = NewObject<USkeletalMeshComponent>(OwnerCharacter, TEXT("FirstPersonArms"));
		FirstPersonArms->SetSkeletalMeshAsset(Body->GetSkeletalMeshAsset());
		FirstPersonArms->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		FirstPersonArms->SetAnimInstanceClass(UShieldbreakBodyAnimInstance::StaticClass());
		FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FirstPersonArms->SetGenerateOverlapEvents(false);
		FirstPersonArms->SetCanEverAffectNavigation(false);
		FirstPersonArms->SetOnlyOwnerSee(true);
		FirstPersonArms->SetCastShadow(false);
		// Drawn with a fixed field of view and pulled toward the eye (the camera's first-person settings), so the
		// arms keep their size when the player changes FOV or zooms, and don't poke through walls.
		FirstPersonArms->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		FirstPersonArms->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		FirstPersonArms->SetupAttachment(OwnerCharacter->GetFirstPersonCamera());
		FirstPersonArms->RegisterComponent();
		for (const TCHAR* Bone : FirstPersonHiddenBones)
		{
			FirstPersonArms->HideBoneByName(Bone, EPhysBodyOp::PBO_None);
		}
		AttachArmor(FirstPersonArms, true);
		// Hidden (the gun stays a camera viewmodel) until the arms have been placed.
		SetFirstPersonArmsVisible(false);
		CalibratedIdle = nullptr;
		UE_LOG(LogShieldbreak, Log, TEXT("%s: first-person arms on the camera."), *OwnerCharacter->GetName());
	}
	if (CalibratedIdle && !FirstPersonArms->IsVisible())
	{
		SetFirstPersonArmsVisible(true);
	}

	// Share the body's (team-tinted) materials, which ApplyPlayerColor may swap for dynamic instances at any time.
	for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
	{
		UMaterialInterface* Material = Body->GetMaterial(Slot);
		if (FirstPersonArms->GetMaterial(Slot) != Material)
		{
			FirstPersonArms->SetMaterial(Slot, Material);
		}
	}

	UShieldbreakBodyAnimInstance* Instance = Cast<UShieldbreakBodyAnimInstance>(FirstPersonArms->GetAnimInstance());
	UAnimSequenceBase* Idle = GetFirstPersonIdle();
	if (!Instance || !Idle)
	{
		return;
	}
	if (Instance->GetPoseAnimation() != Idle)
	{
		Instance->PlayPose(Idle, true);
		FirstPersonIdleSince = World->GetTimeSeconds();
	}

	// Place the arms once the idle has settled (its cross-fade done) and no action is moving the hands.
	const FVector3f Grip(CVarGripForward.GetValueOnGameThread(), CVarGripRight.GetValueOnGameThread(), CVarGripDown.GetValueOnGameThread());
	const FVector2f Aim(CVarAimPitch.GetValueOnGameThread(), CVarAimYaw.GetValueOnGameThread());
	const bool bSettled = World->GetTimeSeconds() - FirstPersonIdleSince > 0.3f && !Instance->GetActionAnimation();
	if (bSettled && (CalibratedIdle != Idle || CalibratedGrip != Grip || CalibratedAim != Aim))
	{
		CalibratedIdle = Idle;
		CalibratedGrip = Grip;
		CalibratedAim = Aim;
		CalibrateFirstPersonArms();
	}

	// Grenade throw: Lyra's toss swings the left arm up over the head, above a first-person view. Pitch the whole rig
	// down around the eye for the throw so that swing comes through the top of the screen and the gun dips away.
	float Dip = 0.f;
	const float Now = World->GetTimeSeconds();
	if (CalibratedIdle && Now < FirstPersonThrowEnd && FirstPersonThrowDuration > 0.f)
	{
		const float T = 1.f - (FirstPersonThrowEnd - Now) / FirstPersonThrowDuration;
		// Quick in (first 15%), held while the arm swings, then out over the last 40%.
		Dip = FMath::SmoothStep(0.f, 1.f, T < 0.15f ? T / 0.15f : T < 0.6f ? 1.f : 1.f - (T - 0.6f) / 0.4f);
	}
	const bool bSwapping = Now < FirstPersonEquipEnd || (CalibratedIdle && CalibratedIdle != Idle);
	SwapLower = bSwapping ? 1.f : FMath::Max(0.f, SwapLower - World->GetDeltaSeconds() / 0.25f);
	if (CalibratedIdle && (Dip > 0.f || AppliedThrowDip > 0.f || SwapLower > 0.f || AppliedSwapLower > 0.f))
	{
		ApplyFirstPersonArmsTransform(Dip);
	}
}

void UShieldbreakBodyAnimator::CalibrateFirstPersonArms()
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	const FName Socket = FirstPersonArms->DoesSocketExist(TEXT("weapon_r")) ? FName(TEXT("weapon_r")) : FName(TEXT("hand_r"));
	const FTransform SocketComponent = FirstPersonArms->GetSocketTransform(Socket, RTS_Component);
	// Guns attach to this socket unrotated, and our (and Lyra's) gun meshes point their barrel along +Y.
	const FVector Barrel = SocketComponent.TransformVectorNoScale(FVector(0.0, 1.0, 0.0)).GetSafeNormal();

	// The mannequin faces +Y; turn it to face the camera's +X, then correct so the barrel points where we want.
	const FQuat Facing = FRotator(0.f, -90.f, 0.f).Quaternion();
	const FVector Desired = FRotator(CalibratedAim.X, CalibratedAim.Y, 0.f).Vector();
	const FQuat Rotation = FQuat::FindBetweenNormals(Facing.RotateVector(Barrel), Desired) * Facing;
	const FVector GripCamera(CalibratedGrip.X, CalibratedGrip.Y, -CalibratedGrip.Z);
	const FVector Location = GripCamera - Rotation.RotateVector(SocketComponent.GetLocation());
	CalibratedArmsTransform = FTransform(Rotation, Location);
	FirstPersonArms->SetRelativeTransform(CalibratedArmsTransform);
	UE_LOG(LogShieldbreak, Log, TEXT("First-person arms placed for %s: offset %s, rotation %s."),
		*GetNameSafe(CalibratedIdle), *Location.ToCompactString(), *Rotation.Rotator().ToCompactString());
}
