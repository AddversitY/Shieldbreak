#include "Visuals/ShieldbreakBodyAnimator.h"

#include "Shieldbreak.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/ShieldbreakCharacter.h"
#include "Player/ShieldbreakMovementComponent.h"
#include "Visuals/ShieldbreakArt.h"
#include "Visuals/ShieldbreakBodyAnimInstance.h"
#include "Weapons/ShieldbreakWeapon.h"

UShieldbreakBodyAnimator::UShieldbreakBodyAnimator()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
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
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		AddTickPrerequisiteComponent(Character->GetCharacterMovement());
	}
	Body->AddTickPrerequisiteComponent(this);

	Body->SetSkeletalMeshAsset(Manny);
	// Our native anim instance cross-fades between the sequences ChoosePose picks and bends the spine to the aim.
	Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Body->SetAnimInstanceClass(UShieldbreakBodyAnimInstance::StaticClass());
	// The local player never sees their own body, but it still casts their shadow.
	Body->SetOwnerNoSee(true);
	Body->bCastHiddenShadow = true;
	bHasBodyArt = true;
	// Load the small shared animation set during spawn/map loading, before the
	// first strafe, turn or action would otherwise synchronously load its clip.
	static bool bAnimationsWarmed = false;
	if (!bAnimationsWarmed && GetNetMode() != NM_DedicatedServer)
	{
		bAnimationsWarmed = true;
		for (int32 Index = static_cast<int32>(EShieldbreakBodyPose::Idle); Index <= static_cast<int32>(EShieldbreakBodyPose::Falling); ++Index)
		{
			GetPoseAnimation(static_cast<EShieldbreakBodyPose>(Index));
		}
		for (const TCHAR* Name : { TEXT("MM_Rifle_Reload"), TEXT("MM_Rifle_Melee"), TEXT("MM_Rifle_GrenadeToss"),
			TEXT("MM_Rifle_Equip"), TEXT("MM_Pistol_Reload"), TEXT("MM_Pistol_Melee"), TEXT("MM_Pistol_Equip") })
		{
			const FString Path = FString(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Actions/")) + Name + TEXT(".") + Name;
			ShieldbreakArt::Load<UAnimSequenceBase>(*Path);
		}
	}
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
	if (Pose == CurrentPose || Now - LastPoseChangeTime >= MinPoseTime)
	{
		if (UAnimSequenceBase* Animation = GetPoseAnimation(Pose))
		{
			const bool bLoop = Pose != EShieldbreakBodyPose::Apex;
			if (AnimInstance)
			{
				const bool bDirectional = Pose == EShieldbreakBodyPose::WalkFwd || Pose == EShieldbreakBodyPose::JogFwd || Pose == EShieldbreakBodyPose::CrouchFwd;
				if (bDirectional)
				{
					const FVector Local = GetOwner()->GetActorRotation().UnrotateVector(GetOwner()->GetVelocity());
					const int32 Base = static_cast<int32>(Pose);
					AnimInstance->PlayDirectionalPose(Animation, GetPoseAnimation(static_cast<EShieldbreakBodyPose>(Base + 1)),
						GetPoseAnimation(static_cast<EShieldbreakBodyPose>(Base + 2)), GetPoseAnimation(static_cast<EShieldbreakBodyPose>(Base + 3)), FVector2D(Local.X, Local.Y));
				}
				else { AnimInstance->PlayPose(Animation, bLoop); }
			}
			else if (Pose != CurrentPose)
			{
				// No anim instance (shouldn't happen): fall back to single-node playback, no blending.
				Body->PlayAnimation(Animation, bLoop);
			}
			if (Pose != CurrentPose) { CurrentPose = Pose; LastPoseChangeTime = Now; }
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

	if (bCrouched)
	{
		OutPlayRate = FMath::Clamp(Speed / CrouchAnimSpeed, 0.5f, 2.f);
		return EShieldbreakBodyPose::CrouchFwd;
	}
	if (Speed < JogThreshold)
	{
		OutPlayRate = FMath::Clamp(Speed / WalkAnimSpeed, 0.5f, 2.f);
		return EShieldbreakBodyPose::WalkFwd;
	}

	OutPlayRate = FMath::Clamp(Speed / JogAnimSpeed, 0.6f, 2.f);
	return EShieldbreakBodyPose::JogFwd;
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
		if (bFirstPerson && FCString::Strcmp(Bone, TEXT("lowerarm_r")) != 0)
		{
			// The right forearm plate only: upper-arm plates sweep through the eye in full-body action clips, and the
			// left arm is hidden in first person (a hidden bone does not hide what is attached to it).
			continue;
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
		// The layered plates sit a centimetre or two over the hull; with their distance fields the sky light's
		// distance-field occlusion shaded the whole suit near black (desktop bot shots, 2026-11-04), like the map kit
		// pieces in 2026-10-30. Armor doesn't need to occlude anything.
		Piece->bAffectDistanceFieldLighting = false;
		if (bFirstPerson)
		{
			Piece->SetOnlyOwnerSee(true);
			Piece->SetCastShadow(false);
			Piece->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
			Piece->LightingChannels.bChannel1 = true; // the first-person fill light (UpdateFirstPersonArms)
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
	// The first-person arms and gun get very little light from the scene, so against Spire's bright white walls they
	// showed as black silhouettes (playtest 2026-11-03). A small unshadowed fill light on the camera that lights only them (lighting
	// channel 1) brings back their colours. Candelas; 0 turns it off. Read when the arms are made. 45 cd burned the
	// matte glove white (after5); the gun read black because its normals were inverted, not for lack of light.
	/** A/B for the black first-person gun: the local player's hidden body and armor cast their shadow from the eye,
	 *  so overhead lights may shade the gun held in front of the chest. 0 = the local body casts no hidden shadow. */
	TAutoConsoleVariable<int32> CVarFPSelfShadow(TEXT("sb.FPSelfShadow"), 1, TEXT("1: the local player's hidden body casts a shadow (new spawns)."));
	/** Bounce fill from below and in front for the gun's undersides (task 11: the BR read near black under the rail,
	 *  lit only from the ceiling). Candelas; 0 = off. Lights only the first-person arms and gun (channel 1). */
	TAutoConsoleVariable<float> CVarFirstPersonBounce(TEXT("sb.FPBounce"), 1.f, TEXT("Candelas of a fill from below for the first-person gun (0 = off, new spawns)."));
	TAutoConsoleVariable<float> CVarFirstPersonLight(TEXT("sb.FPLight"), 0.f, TEXT("Candelas of the fill light that lights only the first-person arms and gun (0 = off)."));
	TAutoConsoleVariable<int32> CVarFirstPersonArms(TEXT("sb.FPArms"), 1, TEXT("1: the local player sees first-person arms holding the gun; 0: the old floating viewmodel."));
	// Where the gun hand sits in camera space (cm forward, right, down) and where the barrel points (degrees; yaw
	// negative = in toward the crosshair). Live-tunable; the arms re-place themselves when these change.
	TAutoConsoleVariable<float> CVarGripForward(TEXT("sb.FPGripForward"), 46.f, TEXT("First-person gun hand: cm in front of the eye."));
	TAutoConsoleVariable<float> CVarThrowDip(TEXT("sb.FPThrowDip"), -35.f, TEXT("First-person grenade throw: degrees the arm rig pitches during the throw (negative tips it up so the left arm's swing crosses the screen; tuned on the PC)."));
	TAutoConsoleVariable<float> CVarGripRight(TEXT("sb.FPGripRight"), 14.f, TEXT("First-person gun hand: cm right of the eye."));
	TAutoConsoleVariable<float> CVarGripDown(TEXT("sb.FPGripDown"), 13.5f, TEXT("First-person gun hand: cm below the eye."));
	TAutoConsoleVariable<float> CVarAimPitch(TEXT("sb.FPAimPitch"), 1.5f, TEXT("First-person barrel pitch (degrees, up positive)."));
	TAutoConsoleVariable<float> CVarAimYaw(TEXT("sb.FPAimYaw"), -3.f, TEXT("First-person barrel yaw (degrees, negative toward the crosshair)."));

	const TCHAR* const ActionsFolder = TEXT("/Game/Characters/Heroes/Mannequin/Animations/Actions/");
	const TCHAR* const PistolIdle = TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Pistol/MM_Pistol_Idle_Hipfire.MM_Pistol_Idle_Hipfire");
	/** Bones hidden on the arms: the head (we look out of it), the legs, and the left arm: the pistol-idle pose holds
	 * the left hand out on its own at the lower left of the view, which read as a stray shape (playtest 2026-11-03), so
	 * the view shows only the gun and the hand holding it. */
	const TCHAR* const FirstPersonHiddenBones[] = { TEXT("neck_01"), TEXT("thigh_l"), TEXT("thigh_r") };
	/** The left arm: hidden by default (see above), shown on rifles, whose left hand holds the front of the gun
	 *  (Vincent, 2026-11-06, with Halo Infinite references: the support hand is the one you see). */
	const TCHAR* const SupportArmBone = TEXT("upperarm_l");
	/** On rifles the trigger arm (and its forearm plate) is hidden instead: the shooter sees the left hand on the front
	 *  of the gun and no trigger hand (Vincent 2026-11-06: "maybe one finger"). Task (18) showed the right forearm and
	 *  its red plate filling the lower right once the rifle was angled toward the centre. */
	const TCHAR* const TriggerArmBone = TEXT("upperarm_r");
	TAutoConsoleVariable<int32> CVarRifleHideTriggerArm(TEXT("sb.FPRifleHideTriggerArm"), 1, TEXT("1: first-person rifles hide the right (trigger) arm."));
	// Rifle framing (guns with a SupportHand socket): the standard shooter layout. The gun enters from the lower right
	// with its rear and the trigger hand below the screen edge, the optic sits just under the crosshair, and the left
	// hand on the front of the gun shows at the lower centre-right. Gun origin (top of the grip) in camera space.
	TAutoConsoleVariable<float> CVarRifleGripForward(TEXT("sb.FPRifleGripForward"), 32.f, TEXT("First-person rifles: gun origin cm in front of the eye."));
	TAutoConsoleVariable<float> CVarRifleGripRight(TEXT("sb.FPRifleGripRight"), 15.f, TEXT("First-person rifles: gun origin cm right of the eye."));
	TAutoConsoleVariable<float> CVarRifleGripDown(TEXT("sb.FPRifleGripDown"), 18.f, TEXT("First-person rifles: gun origin cm below the eye."));
	TAutoConsoleVariable<float> CVarRifleAimPitch(TEXT("sb.FPRifleAimPitch"), 14.f, TEXT("First-person rifles: barrel pitch (degrees, up positive)."));
	TAutoConsoleVariable<float> CVarRifleAimYaw(TEXT("sb.FPRifleAimYaw"), -18.f, TEXT("First-person rifles: barrel yaw (degrees, negative toward the crosshair)."));
	// Where the left palm presses: offset from the gun's SupportHand socket (gun right / forward / up, cm). Task (15)
	// showed the wrist on the socket with the hand's animated rotation: a loose hand hanging below and left of the
	// forend. The hand is now turned palm-up under the forend, fingers wrapping across it, and its wrist placed so the
	// palm sits here.
	TAutoConsoleVariable<float> CVarPalmRight(TEXT("sb.FPPalmRight"), -1.0f, TEXT("First-person rifles: left palm offset right of SupportHand (cm)."));
	TAutoConsoleVariable<float> CVarPalmForward(TEXT("sb.FPPalmForward"), 4.0f, TEXT("First-person rifles: left palm offset forward of SupportHand (cm)."));
	TAutoConsoleVariable<float> CVarPalmUp(TEXT("sb.FPPalmUp"), 0.5f, TEXT("First-person rifles: left palm offset above SupportHand (cm)."));
	TAutoConsoleVariable<float> CVarPalmWrap(TEXT("sb.FPPalmWrap"), 0.55f, TEXT("First-person rifles: how far the left fingers point across the forend (0 = along the barrel, 1 = straight across)."));
	TAutoConsoleVariable<float> CVarPalmTilt(TEXT("sb.FPPalmTilt"), 0.6f, TEXT("First-person rifles: how far the left palm turns toward the gun's side (0 = straight up)."));
	TAutoConsoleVariable<float> CVarFingerCurl(TEXT("sb.FPFingerCurl"), 0.8f, TEXT("First-person rifles: left finger curl around the forend (0 = open, 1 = gripping)."));
	TAutoConsoleVariable<int32> CVarSupportHand(TEXT("sb.FPSupportHand"), 1, TEXT("1: on rifles the first-person left hand holds the gun's SupportHand socket."));

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
	// The procedural motion pivots around the gun hand, so sway and kick turn the gun rather than swing the whole rig.
	const FVector Grip(CalibratedGrip.X, CalibratedGrip.Y, -CalibratedGrip.Z);
	const FTransform Motion = FTransform(-Grip) * FirstPersonMotion * FTransform(Grip);
	FirstPersonArms->SetRelativeTransform(CalibratedArmsTransform * FTransform(FRotator(-Pitch, 0.f, Pitch * 0.4f))
		* Motion * FTransform(FVector(0.f, 0.f, -Lower)));
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

void UShieldbreakBodyAnimator::ClearScopeHide()
{
	if (APlayerController* PC = ScopeHidePC.Get())
	{
		if (AActor* Old = ScopeHiddenWeapon.Get())
		{
			PC->HiddenActors.Remove(Old);
		}
		PC->HiddenPrimitiveComponents.Remove(TWeakObjectPtr<UPrimitiveComponent>(FirstPersonArms.Get()));
		for (UStaticMeshComponent* Piece : FirstPersonArmor)
		{
			PC->HiddenPrimitiveComponents.Remove(TWeakObjectPtr<UPrimitiveComponent>(Piece));
		}
	}
	ScopeHidePC = nullptr;
	ScopeHiddenWeapon = nullptr;
	bScopeHidden = false;
}

void UShieldbreakBodyAnimator::UpdateScopeHide(AShieldbreakCharacter* OwnerCharacter)
{
	// Looking through a scope overlay: the gun and arms would sit in front of the lens, so this viewer stops
	// rendering them (their own visibility, animation and the third-person body are untouched).
	APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	AShieldbreakWeapon* Weapon = OwnerCharacter->GetCurrentWeapon();
	const bool bHide = PC && OwnerCharacter->IsAlive() && OwnerCharacter->GetScopeBlend() > 0.5f && Weapon
		&& Weapon->GetStats().Scope != EShieldbreakScope::None;
	if (bHide == bScopeHidden && ScopeHiddenWeapon.Get() == (bHide ? Weapon : nullptr) && ScopeHidePC.Get() == (bHide ? PC : nullptr))
	{
		return;
	}
	ClearScopeHide();
	if (bHide)
	{
		bScopeHidden = true;
		ScopeHidePC = PC;
		ScopeHiddenWeapon = Weapon;
		PC->HiddenActors.AddUnique(Weapon);
		PC->HiddenPrimitiveComponents.AddUnique(TWeakObjectPtr<UPrimitiveComponent>(FirstPersonArms.Get()));
		for (UStaticMeshComponent* Piece : FirstPersonArmor)
		{
			PC->HiddenPrimitiveComponents.AddUnique(TWeakObjectPtr<UPrimitiveComponent>(Piece));
		}
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
		ClearScopeHide();
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
		FirstPersonArms->LightingChannels.bChannel1 = true;
		const float FillCandelas = CVarFirstPersonLight.GetValueOnGameThread();
		if (FillCandelas > 0.f)
		{
			// Above, behind and to the left of the eye, so the gun and hands get a soft key light from the upper left.
			// It used to sit 30 cm from the eye: by the inverse square the parts nearest the camera (the scope's
			// eyepiece, the hand) got several times the light of the gun's body and burned out white (desktop shots
			// 2026-11-04). About a metre away the whole gun is lit evenly; the intensity is raised to match.
			UPointLightComponent* Fill = NewObject<UPointLightComponent>(OwnerCharacter, TEXT("FirstPersonFill"));
			Fill->SetupAttachment(OwnerCharacter->GetFirstPersonCamera());
			Fill->SetRelativeLocation(FVector(-20.f, -40.f, 50.f));
			Fill->SetMobility(EComponentMobility::Movable);
			Fill->SetIntensityUnits(ELightUnits::Candelas);
			Fill->SetIntensity(FillCandelas);
			Fill->SetAttenuationRadius(250.f);
			Fill->SetSourceRadius(20.f);
			Fill->SetLightColor(FLinearColor(1.f, 0.97f, 0.93f));
			Fill->SetCastShadows(false);
			Fill->SetAffectTranslucentLighting(false);
			Fill->SetVolumetricScatteringIntensity(0.f);
			Fill->LightingChannels.bChannel0 = false;
			Fill->LightingChannels.bChannel1 = true;
			Fill->bAffectsWorld = true;
			Fill->RegisterComponent();
		}
		const float BounceCandelas = CVarFirstPersonBounce.GetValueOnGameThread();
		if (BounceCandelas > 0.f)
		{
			// Ahead, right and well below the eye, like light coming back off the floor; about a metre from the gun so
			// the hand and the barrel get the same amount. Warm-neutral, soft and shadowless.
			UPointLightComponent* Bounce = NewObject<UPointLightComponent>(OwnerCharacter, TEXT("FirstPersonBounce"));
			Bounce->SetupAttachment(OwnerCharacter->GetFirstPersonCamera());
			Bounce->SetRelativeLocation(FVector(90.f, 40.f, -90.f));
			Bounce->SetMobility(EComponentMobility::Movable);
			Bounce->SetIntensityUnits(ELightUnits::Candelas);
			Bounce->SetIntensity(BounceCandelas);
			Bounce->SetAttenuationRadius(250.f);
			Bounce->SetSourceRadius(40.f);
			Bounce->SetLightColor(FLinearColor(0.95f, 0.95f, 1.f));
			Bounce->SetCastShadows(false);
			Bounce->SetAffectTranslucentLighting(false);
			Bounce->SetVolumetricScatteringIntensity(0.f);
			Bounce->SetSpecularScale(0.3f);
			Bounce->LightingChannels.bChannel0 = false;
			Bounce->LightingChannels.bChannel1 = true;
			Bounce->RegisterComponent();
		}
		FirstPersonArms->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		FirstPersonArms->SetupAttachment(OwnerCharacter->GetFirstPersonCamera());
		FirstPersonArms->RegisterComponent();
		FirstPersonArms->AddTickPrerequisiteComponent(this);
		for (const TCHAR* Bone : FirstPersonHiddenBones)
		{
			FirstPersonArms->HideBoneByName(Bone, EPhysBodyOp::PBO_None);
		}
		FirstPersonArms->HideBoneByName(SupportArmBone, EPhysBodyOp::PBO_None);
		bSupportArmShown = false;
		AttachArmor(FirstPersonArms, true);
		if (CVarFPSelfShadow.GetValueOnGameThread() == 0)
		{
			Body->bCastHiddenShadow = false;
			Body->MarkRenderStateDirty();
			for (UStaticMeshComponent* Piece : ArmorPieces)
			{
				Piece->bCastHiddenShadow = false;
				Piece->MarkRenderStateDirty();
			}
		}
		// Hidden (the gun stays a camera viewmodel) until the arms have been placed.
		SetFirstPersonArmsVisible(false);
		CalibratedIdle = nullptr;
		UE_LOG(LogShieldbreak, Log, TEXT("%s: first-person arms on the camera."), *OwnerCharacter->GetName());
	}
	if (CalibratedIdle && !FirstPersonArms->IsVisible())
	{
		SetFirstPersonArmsVisible(true);
	}
	UpdateScopeHide(OwnerCharacter);

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
	const bool bRifle = WantsFirstPersonSupportArm();
	const FVector3f Grip = bRifle
		? FVector3f(CVarRifleGripForward.GetValueOnGameThread(), CVarRifleGripRight.GetValueOnGameThread(), CVarRifleGripDown.GetValueOnGameThread())
		: FVector3f(CVarGripForward.GetValueOnGameThread(), CVarGripRight.GetValueOnGameThread(), CVarGripDown.GetValueOnGameThread());
	const FVector2f Aim = bRifle
		? FVector2f(CVarRifleAimPitch.GetValueOnGameThread(), CVarRifleAimYaw.GetValueOnGameThread())
		: FVector2f(CVarAimPitch.GetValueOnGameThread(), CVarAimYaw.GetValueOnGameThread());
	// Show the left arm only while it has a gun to hold (the grenade throw shows it for its own swing, then restores
	// this state through WantsFirstPersonSupportArm).
	if (bRifle != bSupportArmShown)
	{
		bSupportArmShown = bRifle;
		if (bRifle)
		{
			FirstPersonArms->UnHideBoneByName(SupportArmBone);
		}
		else
		{
			FirstPersonArms->HideBoneByName(SupportArmBone, EPhysBodyOp::PBO_None);
		}
	}
	const bool bHideTrigger = bRifle && CVarRifleHideTriggerArm.GetValueOnGameThread() != 0;
	if (bHideTrigger != bTriggerArmHidden)
	{
		bTriggerArmHidden = bHideTrigger;
		if (bHideTrigger)
		{
			FirstPersonArms->HideBoneByName(TriggerArmBone, EPhysBodyOp::PBO_None);
		}
		else
		{
			FirstPersonArms->UnHideBoneByName(TriggerArmBone);
		}
		// A hidden bone does not hide what is attached to it: the forearm plate goes too.
		for (UStaticMeshComponent* Piece : FirstPersonArmor)
		{
			const FString Socket = Piece->GetAttachSocketName().ToString();
			if (Socket.EndsWith(TEXT("_r")))
			{
				Piece->SetHiddenInGame(bHideTrigger);
			}
		}
	}
	const bool bSettled = World->GetTimeSeconds() - FirstPersonIdleSince > 0.3f && !Instance->GetActionAnimation();
	const AShieldbreakWeapon* HeldWeapon = OwnerCharacter->GetCurrentWeapon();
	const float Lift = HeldWeapon ? HeldWeapon->GetFirstPersonLift() : 0.f;
	if (bSettled && (CalibratedIdle != Idle || CalibratedGrip != Grip || CalibratedAim != Aim || !FMath::IsNearlyEqual(CalibratedLift, Lift, 0.05f)))
	{
		CalibratedIdle = Idle;
		CalibratedLift = Lift;
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
	if (CalibratedIdle)
	{
		UpdateFirstPersonMotion(World->GetDeltaSeconds());
		ApplyFirstPersonArmsTransform(Dip);
	}
	UpdateTriggerFinger(Instance, CalibratedIdle && !bSwapping && Dip <= 0.f && !Instance->GetActionAnimation(), World->GetDeltaSeconds());
	UpdateSupportHand(Instance, CalibratedIdle && !bSwapping && Dip <= 0.f && !Instance->GetActionAnimation(), World->GetDeltaSeconds());
}

UStaticMeshComponent* UShieldbreakBodyAnimator::GetSupportMesh(bool bRequireAttached) const
{
	const AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner());
	const AShieldbreakWeapon* Weapon = OwnerCharacter ? OwnerCharacter->GetCurrentWeapon() : nullptr;
	if (!Weapon || !Weapon->GetRootComponent()
		|| (bRequireAttached && (!FirstPersonArms || Weapon->GetRootComponent()->GetAttachParent() != FirstPersonArms)))
	{
		return nullptr;
	}
	TInlineComponentArray<UStaticMeshComponent*> Meshes(Weapon);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh->DoesSocketExist(TEXT("SupportHand")))
		{
			return Mesh;
		}
	}
	return nullptr;
}

bool UShieldbreakBodyAnimator::WantsFirstPersonSupportArm() const
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	return CVarSupportHand.GetValueOnGameThread() != 0 && GetSupportMesh(false) != nullptr;
}

void UShieldbreakBodyAnimator::UpdateSupportHand(UShieldbreakBodyAnimInstance* Instance, bool bHolding, float DeltaSeconds)
{
	UStaticMeshComponent* Gun = bHolding && WantsFirstPersonSupportArm() ? GetSupportMesh(true) : nullptr;
	SupportHandWeight = FMath::FInterpConstantTo(SupportHandWeight, Gun ? 1.f : 0.f, DeltaSeconds, 1.f / 0.15f);
	if (!Gun && SupportHandWeight <= 0.f)
	{
		Instance->SetSupportHand(0.f, FVector::ZeroVector, FVector::ForwardVector, FVector::UpVector, 0.f);
		return;
	}
	if (Gun)
	{
		using namespace ShieldbreakBodyAnimatorPrivate;
		// Gun axes from its sockets, in world space: forward from the trigger to the muzzle, up from the mesh, right
		// from those two (Unreal is left-handed: Up x Forward points right).
		const FVector Socket = Gun->GetSocketLocation(TEXT("SupportHand"));
		FVector Forward = Gun->DoesSocketExist(TEXT("Muzzle")) && Gun->DoesSocketExist(TEXT("Trigger"))
			? (Gun->GetSocketLocation(TEXT("Muzzle")) - Gun->GetSocketLocation(TEXT("Trigger"))).GetSafeNormal() : Gun->GetForwardVector();
		FVector Up = Gun->GetUpVector();
		Up = (Up - Forward * FVector::DotProduct(Up, Forward)).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
		const FVector Palm = Socket + Right * CVarPalmRight.GetValueOnGameThread() + Forward * CVarPalmForward.GetValueOnGameThread()
			+ Up * CVarPalmUp.GetValueOnGameThread();
		// Palm up and turned toward the gun; fingers across the underside toward the far side, a little forward.
		const float Wrap = FMath::Clamp(CVarPalmWrap.GetValueOnGameThread(), 0.f, 1.f);
		const FVector PalmNormal = (Up + Right * CVarPalmTilt.GetValueOnGameThread()).GetSafeNormal();
		FVector Fingers = Right * Wrap + Forward * FMath::Sqrt(FMath::Max(0.f, 1.f - Wrap * Wrap));
		Fingers = (Fingers - PalmNormal * FVector::DotProduct(Fingers, PalmNormal)).GetSafeNormal();
		const FTransform& ToArms = FirstPersonArms->GetComponentTransform();
		Instance->SetSupportHand(SupportHandWeight, ToArms.InverseTransformPosition(Palm),
			ToArms.InverseTransformVectorNoScale(Fingers), ToArms.InverseTransformVectorNoScale(PalmNormal),
			FMath::Clamp(CVarFingerCurl.GetValueOnGameThread(), 0.f, 1.5f));
		SupportPalmWorld = Palm;
	}
	else
	{
		Instance->SetSupportHand(SupportHandWeight, Instance->GetSupportTarget(), Instance->GetSupportFingers(), Instance->GetSupportPalmNormal(),
			Instance->GetSupportCurl());
	}
}

float UShieldbreakBodyAnimator::GetSupportHandError() const
{
	const UStaticMeshComponent* Gun = GetSupportMesh(true);
	if (!Gun || !FirstPersonArms || FirstPersonArms->GetBoneIndex(TEXT("hand_l")) == INDEX_NONE)
	{
		return -1.f;
	}
	// Error at the palm (the point the hand is placed by): the middle of the knuckles' base and the wrist.
	if (FirstPersonArms->GetBoneIndex(TEXT("middle_01_l")) == INDEX_NONE)
	{
		return static_cast<float>(FVector::Dist(FirstPersonArms->GetBoneLocation(TEXT("hand_l")), SupportPalmWorld));
	}
	const FVector PalmNow = (FirstPersonArms->GetBoneLocation(TEXT("hand_l")) + FirstPersonArms->GetBoneLocation(TEXT("middle_01_l"))) * 0.5;
	return static_cast<float>(FVector::Dist(PalmNow, SupportPalmWorld));
}

UStaticMeshComponent* UShieldbreakBodyAnimator::GetTriggerMesh() const
{
	const AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner());
	const AShieldbreakWeapon* Weapon = OwnerCharacter ? OwnerCharacter->GetCurrentWeapon() : nullptr;
	if (!Weapon || !FirstPersonArms || !Weapon->GetRootComponent() || Weapon->GetRootComponent()->GetAttachParent() != FirstPersonArms)
	{
		return nullptr;
	}
	TInlineComponentArray<UStaticMeshComponent*> Meshes(Weapon);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh->DoesSocketExist(TEXT("Trigger")))
		{
			return Mesh;
		}
	}
	return nullptr;
}

void UShieldbreakBodyAnimator::UpdateTriggerFinger(UShieldbreakBodyAnimInstance* Instance, bool bHolding, float DeltaSeconds)
{
	// The animation's index finger points straight along the gun (Lyra's trigger discipline), and on our wider
	// receivers it stuck out through the side (playtest 2026-11-05). Curl it onto the trigger instead; ease out while
	// an action or a swap moves the hand.
	UStaticMeshComponent* Gun = bHolding ? GetTriggerMesh() : nullptr;
	TriggerFingerWeight = FMath::FInterpConstantTo(TriggerFingerWeight, Gun ? 1.f : 0.f, DeltaSeconds, 1.f / 0.12f);
	if (!Gun && TriggerFingerWeight <= 0.f)
	{
		Instance->SetTriggerFinger(0.f, FVector::ZeroVector);
		return;
	}
	if (Gun)
	{
		const FTransform Hand = FirstPersonArms->GetSocketTransform(TEXT("hand_r"), RTS_World);
		Instance->SetTriggerFinger(TriggerFingerWeight, Hand.InverseTransformPosition(Gun->GetSocketLocation(TEXT("Trigger"))));
	}
	else
	{
		Instance->SetTriggerFinger(TriggerFingerWeight, Instance->GetTriggerTarget());
	}
}

float UShieldbreakBodyAnimator::GetTriggerFingerError() const
{
	const UStaticMeshComponent* Gun = GetTriggerMesh();
	if (!Gun || !FirstPersonArms || FirstPersonArms->GetBoneIndex(TEXT("index_03_r")) == INDEX_NONE)
	{
		return -1.f;
	}
	const FVector Middle = FirstPersonArms->GetBoneLocation(TEXT("index_02_r"));
	const FVector Last = FirstPersonArms->GetBoneLocation(TEXT("index_03_r"));
	// Same pad point as the anim instance (1.6 cm past the last joint), in world space (the arms are unscaled).
	const FVector Pad = Last + (Last - Middle).GetSafeNormal() * 1.6f;
	return static_cast<float>(FVector::Dist(Pad, Gun->GetSocketLocation(TEXT("Trigger"))));
}

FString UShieldbreakBodyAnimator::DescribeTriggerFinger() const
{
	const UStaticMeshComponent* Gun = GetTriggerMesh();
	if (!Gun || !FirstPersonArms || FirstPersonArms->GetBoneIndex(TEXT("index_03_r")) == INDEX_NONE)
	{
		return TEXT("no trigger");
	}
	const FTransform Hand = FirstPersonArms->GetSocketTransform(TEXT("hand_r"), RTS_World);
	const FVector Middle = FirstPersonArms->GetBoneLocation(TEXT("index_02_r"));
	const FVector Last = FirstPersonArms->GetBoneLocation(TEXT("index_03_r"));
	const FVector Pad = Last + (Last - Middle).GetSafeNormal() * 1.6f;
	auto InHand = [&Hand](const FVector& World) { const FVector P = Hand.InverseTransformPosition(World); return FString::Printf(TEXT("(%.1f,%.1f,%.1f)"), P.X, P.Y, P.Z); };
	// The trigger socket in the gun's own mesh space too, so the art can be moved by exactly the miss.
	const FVector PadInGun = Gun->GetComponentTransform().InverseTransformPosition(Pad);
	const FVector TriggerInGun = Gun->GetComponentTransform().InverseTransformPosition(Gun->GetSocketLocation(TEXT("Trigger")));
	auto InGun = [Gun](const FVector& World) { const FVector P = Gun->GetComponentTransform().InverseTransformPosition(World); return FString::Printf(TEXT("(%.1f,%.1f,%.1f)"), P.X, P.Y, P.Z); };
	return FString::Printf(TEXT("hand space: trigger %s pad %s index_01 %s; gun space: trigger (%.1f,%.1f,%.1f) pad (%.1f,%.1f,%.1f) index_01 %s hand_r %s middle_01 %s pinky_01 %s thumb_03 %s"),
		*InHand(Gun->GetSocketLocation(TEXT("Trigger"))), *InHand(Pad), *InHand(FirstPersonArms->GetBoneLocation(TEXT("index_01_r"))),
		TriggerInGun.X, TriggerInGun.Y, TriggerInGun.Z, PadInGun.X, PadInGun.Y, PadInGun.Z,
		*InGun(FirstPersonArms->GetBoneLocation(TEXT("index_01_r"))), *InGun(Hand.GetLocation()), *InGun(FirstPersonArms->GetBoneLocation(TEXT("middle_01_r"))),
		*InGun(FirstPersonArms->GetBoneLocation(TEXT("pinky_01_r"))), *InGun(FirstPersonArms->GetBoneLocation(TEXT("thumb_03_r"))));
}

namespace ShieldbreakBodyAnimatorPrivate
{
	TAutoConsoleVariable<float> CVarFPMotion(TEXT("sb.FPMotion"), 1.f, TEXT("First-person gun motion (bob, sway, kick, landing): 0 off, 1 normal."));

	/** Damped spring toward Target, sub-stepped so it behaves the same at 15 and 240 FPS. */
	void StepSpring(FVector& Value, FVector& Velocity, const FVector& Target, float Frequency, float Damping, float DeltaSeconds)
	{
		const float Omega = 2.f * UE_PI * Frequency;
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds * 240.f), 1, 16);
		const float Step = DeltaSeconds / Steps;
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			Velocity += (-(Omega * Omega) * (Value - Target) - 2.f * Damping * Omega * Velocity) * Step;
			Value += Velocity * Step;
		}
	}

	/** Moves Value toward Target with a time constant of 1 / Rate seconds. */
	float Follow(float Value, float Target, float Rate, float DeltaSeconds)
	{
		return FMath::Lerp(Value, Target, 1.f - FMath::Exp(-Rate * DeltaSeconds));
	}
}

void UShieldbreakBodyAnimator::UpdateFirstPersonMotion(float DeltaSeconds)
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	const AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner());
	const UShieldbreakMovementComponent* Movement = OwnerCharacter ? OwnerCharacter->GetShieldbreakMovement() : nullptr;
	const AController* Controller = OwnerCharacter ? OwnerCharacter->GetController() : nullptr;
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 0.05f);
	if (!Movement || !Controller || Dt <= 0.f)
	{
		return;
	}

	// Look sway: the gun trails the view a little when turning, then settles.
	const FRotator View = Controller->GetControlRotation();
	float PitchSpeed = 0.f, YawSpeed = 0.f; // degrees per second
	if (bHasLastViewRotation)
	{
		const FRotator Delta = (View - LastViewRotation).GetNormalized();
		PitchSpeed = static_cast<float>(Delta.Pitch) / Dt;
		YawSpeed = static_cast<float>(Delta.Yaw) / Dt;
	}
	LastViewRotation = View;
	bHasLastViewRotation = true;

	const FVector Velocity = OwnerCharacter->GetVelocity();
	const FVector Local = OwnerCharacter->GetActorRotation().UnrotateVector(Velocity);
	const float WalkSpeed = FMath::Max(1.f, Movement->MaxWalkSpeed);
	const bool bGrounded = Movement->IsMovingOnGround() && !Movement->IsSliding();
	const bool bFalling = Movement->IsFalling();

	FVector TargetOffset = FVector::ZeroVector;
	FVector TargetRotation = FVector::ZeroVector;
	TargetRotation.X = FMath::Clamp(-PitchSpeed * 0.006f, -3.f, 3.f);
	TargetRotation.Y = FMath::Clamp(-YawSpeed * 0.006f, -4.f, 4.f);
	TargetRotation.Z = FMath::Clamp(-YawSpeed * 0.004f, -3.f, 3.f);
	TargetOffset.Y = FMath::Clamp(-YawSpeed * 0.0025f, -1.2f, 1.2f);
	TargetOffset.Z = FMath::Clamp(-PitchSpeed * 0.0025f, -1.f, 1.f);

	// Strafing leans the gun into the move; running forward pulls it in slightly.
	const float Strafe = FMath::Clamp(static_cast<float>(Local.Y) / WalkSpeed, -1.f, 1.f);
	const float Forward = FMath::Clamp(static_cast<float>(Local.X) / WalkSpeed, -1.f, 1.f);
	TargetRotation.Z += Strafe * 2.5f;
	TargetOffset.Y -= Strafe * 0.4f;
	TargetOffset.X -= Forward * 0.5f;

	// Sprint: gun down and turned in.
	SprintBlend = Follow(SprintBlend, Movement->IsSprinting() ? 1.f : 0.f, 10.f, Dt);
	TargetRotation += FVector(-10.f, -12.f, 6.f) * SprintBlend;
	TargetOffset += FVector(-2.f, -2.f, -3.f) * SprintBlend;

	// In the air the gun lags the jump: it sits low on the way up and floats up on the way down.
	if (bFalling)
	{
		TargetOffset.Z += FMath::Clamp(static_cast<float>(-Velocity.Z) * 0.003f, -2.5f, 2.5f);
		LastFallSpeed = FMath::Max(0.f, static_cast<float>(-Velocity.Z));
	}
	else if (bWasFalling)
	{
		// Landing: a dip that scales with the fall.
		MotionOffsetVelocity.Z -= FMath::Clamp(LastFallSpeed * 0.06f, 0.f, 90.f);
		MotionRotationVelocity.X -= FMath::Clamp(LastFallSpeed * 0.02f, 0.f, 30.f);
		LastFallSpeed = 0.f;
	}
	bWasFalling = bFalling;

	StepSpring(MotionOffset, MotionOffsetVelocity, TargetOffset, 9.f, 0.75f, Dt);
	StepSpring(MotionRotation, MotionRotationVelocity, TargetRotation, 9.f, 0.75f, Dt);

	// Walk bob, one dip per footstep, eased in and out with speed.
	const float Speed = static_cast<float>(Velocity.Size2D());
	BobAmount = Follow(BobAmount, bGrounded ? FMath::Clamp(Speed / WalkSpeed, 0.f, 1.6f) : 0.f, 8.f, Dt);
	if (bGrounded && Speed > 10.f)
	{
		BobPhase = FMath::Fmod(BobPhase + Dt * 2.f * UE_PI * Speed / 520.f, 2.f * UE_PI);
	}
	const float BobScale = BobAmount * (OwnerCharacter->bIsCrouched ? 0.6f : 1.f);
	const FVector Bob(0.f, 0.5f * FMath::Sin(BobPhase), -0.6f * 0.5f * (1.f - FMath::Cos(2.f * BobPhase)));
	const float BobRoll = 0.6f * FMath::Sin(BobPhase);

	// Scoped aim keeps the gun steady; sb.FPMotion scales everything.
	MotionScale = Follow(MotionScale, OwnerCharacter->IsZoomed() ? 0.3f : 1.f, 12.f, Dt);
	const float Scale = MotionScale * FMath::Max(0.f, CVarFPMotion.GetValueOnGameThread());
	const FVector Offset = (MotionOffset + Bob * BobScale) * Scale;
	const FVector Rotation = (MotionRotation + FVector(0.f, 0.f, BobRoll * BobScale)) * Scale;
	FirstPersonMotion = FTransform(FRotator(Rotation.X, Rotation.Y, Rotation.Z), Offset);
}

void UShieldbreakBodyAnimator::AddFirstPersonKick(float Strength)
{
	// Even the light rifle should read on screen; heavy guns kick up to about three times as hard.
	const float Kick = FMath::Clamp(0.6f + Strength * 0.5f, 0.6f, 2.f);
	MotionOffsetVelocity += FVector(-140.f, 0.f, 20.f) * Kick;
	MotionRotationVelocity += FVector(160.f, FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-20.f, 20.f)) * Kick;
}

void UShieldbreakBodyAnimator::AddBodyKick(float Strength)
{
	if (UShieldbreakBodyAnimInstance* Instance = bHasBodyArt && !bRagdoll && GetBodyMesh()
		? Cast<UShieldbreakBodyAnimInstance>(GetBodyMesh()->GetAnimInstance()) : nullptr)
	{
		Instance->AddFireKick(FMath::Clamp(0.6f + Strength * 0.5f, 0.6f, 2.f));
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
	// The gun sits CalibratedLift above the socket (AShieldbreakWeapon::GetFirstPersonLift): lower the arms by as much,
	// so the gun keeps its place on screen and the hand shows below it.
	const FVector LiftedSocket = SocketComponent.GetLocation() + SocketComponent.TransformVectorNoScale(FVector(0.0, 0.0, CalibratedLift));
	const FVector Location = GripCamera - Rotation.RotateVector(LiftedSocket);
	CalibratedArmsTransform = FTransform(Rotation, Location);
	FirstPersonArms->SetRelativeTransform(CalibratedArmsTransform);
	UE_LOG(LogShieldbreak, Log, TEXT("First-person arms placed for %s: offset %s, rotation %s."),
		*GetNameSafe(CalibratedIdle), *Location.ToCompactString(), *Rotation.Rotator().ToCompactString());
	// Where the hand's joints sit in the gun's own mesh space (X across, Y along the barrel, Z up; the gun attaches to
	// this socket unrotated), so the grips and receivers can be fitted around the real hand pose.
	FString Joints;
	for (const TCHAR* Bone : { TEXT("hand_r"), TEXT("thumb_01_r"), TEXT("thumb_02_r"), TEXT("thumb_03_r"), TEXT("index_01_r"),
		TEXT("index_02_r"), TEXT("index_03_r"), TEXT("middle_01_r"), TEXT("middle_02_r"), TEXT("middle_03_r"), TEXT("ring_01_r"),
		TEXT("ring_03_r"), TEXT("pinky_01_r"), TEXT("pinky_03_r") })
	{
		if (FirstPersonArms->GetBoneIndex(Bone) != INDEX_NONE)
		{
			const FVector Local = SocketComponent.InverseTransformPosition(
				FirstPersonArms->GetBoneTransform(FirstPersonArms->GetBoneIndex(Bone), FTransform::Identity).GetLocation());
			Joints += FString::Printf(TEXT(" %s=(%.1f,%.1f,%.1f)"), Bone, Local.X, Local.Y, Local.Z);
		}
	}
	UE_LOG(LogShieldbreak, Log, TEXT("First-person hand joints in grip space for %s:%s"), *GetNameSafe(CalibratedIdle), *Joints);
}
