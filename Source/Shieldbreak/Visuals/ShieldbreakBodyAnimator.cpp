#include "Visuals/ShieldbreakBodyAnimator.h"

#include "Shieldbreak.h"
#include "Audio/ShieldbreakSFX.h"
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
#include "Weapons/ShieldbreakGrenade.h"
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
	// Everyone else sees the grenade in the body's left hand from the toss's start until the hand lets it go.
	const float SinceToss = World->GetTimeSeconds() - BodyThrowStart;
	if (BodyHeldGrenade || (SinceToss >= 0.f && SinceToss < AShieldbreakGrenade::GetReleaseDelay()))
	{
		UpdateHeldGrenade(Body, BodyHeldGrenade, SinceToss >= 0.f && SinceToss < AShieldbreakGrenade::GetReleaseDelay(), false);
	}
	// The throw's whoosh at the hand for everyone else (the local player hears it from the first-person arm).
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!bBodyTossReleased && SinceToss >= AShieldbreakGrenade::GetReleaseDelay() && SinceToss < 1.f)
	{
		bBodyTossReleased = true;
		if (OwnerPawn && !(OwnerPawn->IsLocallyControlled() && OwnerPawn->IsPlayerControlled()))
		{
			FShieldbreakSFX::PlayAt(this, EShieldbreakSound::GrenadeThrow, Body->GetBoneLocation(TEXT("hand_l")));
		}
	}

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
	UpdateThirdPersonGrip(AnimInstance, Body, DeltaTime);
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
	if (GetWorld())
	{
		Moves.Stop(GetWorld()->GetTimeSeconds(), GetHeldMagazine());
	}
	MovePose = FShieldbreakViewmodelPose();
	bDrawPending = false;

	if (FirstPersonArms)
	{
		FirstPersonArms->SetHiddenInGame(true);
		for (UStaticMeshComponent* Piece : FirstPersonArmor)
		{
			Piece->SetHiddenInGame(true);
		}
		for (UStaticMeshComponent* Piece : FirstPersonLeftArmor)
		{
			Piece->SetHiddenInGame(true);
		}
		if (FirstPersonHeldGrenade)
		{
			FirstPersonHeldGrenade->SetHiddenInGame(true);
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
		const bool bLeftArm = FCString::Strcmp(Bone, TEXT("upperarm_l")) == 0 || FCString::Strcmp(Bone, TEXT("lowerarm_l")) == 0;
		if (bFirstPerson && FCString::Strcmp(Bone, TEXT("lowerarm_r")) != 0 && !bLeftArm)
		{
			// The right forearm plate, plus the left arm's plates for the grenade throw (shown only while it throws: the
			// left arm is hidden otherwise, and a hidden bone does not hide what is attached to it). The right upper-arm
			// plate sweeps through the eye in full-body action clips.
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
		if (bFirstPerson && bLeftArm)
		{
			Piece->SetVisibility(false);
			FirstPersonLeftArmor.Add(Piece);
		}
		else
		{
			Pieces.Add(Piece);
		}

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
	TAutoConsoleVariable<float> CVarThrowDip(TEXT("sb.FPThrowDip"), 1.f, TEXT("First-person grenade throw: how far the gun dips down and right out of the throwing arm's way (1 = 20 deg and 9 cm)."));
	TAutoConsoleVariable<float> CVarThrowReach(TEXT("sb.FPThrowReach"), 1.f, TEXT("First-person grenade throw: scales how far forward the left hand's path goes (camera space)."));
	TAutoConsoleVariable<float> CVarThrowWrist(TEXT("sb.FPThrowWrist"), 1.f, TEXT("First-person grenade throw: wrist cock/snap amount; -1 flips its direction."));
	TAutoConsoleVariable<float> CVarGripTime(TEXT("sb.FPThrowGripTime"), -1.f, TEXT("Seconds into Lyra's toss whose left-finger pose the throw starts from (negative: the idle's fingers; the curl closes them either way)."));
	TAutoConsoleVariable<float> CVarThrowFreeze(TEXT("sb.FPThrowFreeze"), -1.f, TEXT("Captures: >= 0 holds the first-person throw pose at this many seconds into the throw (the grenade still leaves on time)."));
	TAutoConsoleVariable<float> CVarHandRoll(TEXT("sb.FPThrowHandRoll"), -70.f, TEXT("First-person grenade throw: degrees the left hand turns about the forearm so the grenade shows (G6: at 0 only the back of the hand did; G8: -70 shows it in the cocked pose, +70 did not)."));
	TAutoConsoleVariable<float> CVarThrowCurl(TEXT("sb.FPThrowCurl"), 1.f, TEXT("First-person grenade throw: how far the left fingers close round the grenade (0 = open)."));
	TAutoConsoleVariable<float> CVarShoulderX(TEXT("sb.FPThrowShoulderX"), -4.f, TEXT("First-person grenade throw: the throwing shoulder, cm in front of the eye (camera space)."));
	TAutoConsoleVariable<float> CVarShoulderY(TEXT("sb.FPThrowShoulderY"), -20.f, TEXT("First-person grenade throw: the throwing shoulder, cm right of the eye (negative = left)."));
	TAutoConsoleVariable<float> CVarShoulderZ(TEXT("sb.FPThrowShoulderZ"), -18.f, TEXT("First-person grenade throw: the throwing shoulder, cm above the eye (negative = below)."));
	TAutoConsoleVariable<float> CVarHeldGrenadePalm(TEXT("sb.HeldGrenadePalm"), 3.5f, TEXT("cm the held grenade sits off the palm (negative puts it on the back of the hand)."));
	TAutoConsoleVariable<float> CVarTossStart(TEXT("sb.TPThrowStart"), 0.1f, TEXT("Third-person toss: seconds of Lyra's clip skipped at the start."));
	TAutoConsoleVariable<float> CVarTossRelease(TEXT("sb.TPThrowRelease"), 0.45f, TEXT("Third-person toss: seconds into Lyra's clip where the hand lets go (the play rate puts it at sb.GrenadeReleaseDelay)."));
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
	TAutoConsoleVariable<float> CVarRifleGripDown(TEXT("sb.FPRifleGripDown"), 19.f, TEXT("First-person rifles: gun origin cm below the eye."));
	TAutoConsoleVariable<float> CVarRifleAimPitch(TEXT("sb.FPRifleAimPitch"), 14.f, TEXT("First-person rifles: barrel pitch (degrees, up positive)."));
	TAutoConsoleVariable<float> CVarRifleAimYaw(TEXT("sb.FPRifleAimYaw"), -16.f, TEXT("First-person rifles: barrel yaw (degrees, negative toward the crosshair)."));
	// Where the left palm presses: offset from the gun's SupportHand socket (gun right / forward / up, cm). Task (15)
	// showed the wrist on the socket with the hand's animated rotation: a loose hand hanging below and left of the
	// forend. The hand is now turned palm-up under the forend, fingers wrapping across it, and its wrist placed so the
	// palm sits here.
	TAutoConsoleVariable<float> CVarPalmRight(TEXT("sb.FPPalmRight"), -2.5f, TEXT("First-person rifles: left palm offset right of SupportHand (cm)."));
	TAutoConsoleVariable<float> CVarPalmForward(TEXT("sb.FPPalmForward"), 6.0f, TEXT("First-person rifles: left palm offset forward of SupportHand (cm)."));
	TAutoConsoleVariable<float> CVarPalmUp(TEXT("sb.FPPalmUp"), -1.5f, TEXT("First-person rifles: left palm offset above SupportHand (cm)."));
	TAutoConsoleVariable<float> CVarPalmWrap(TEXT("sb.FPPalmWrap"), 0.4f, TEXT("First-person rifles: how far the left fingers point across the forend (0 = along the barrel, 1 = straight across)."));
	TAutoConsoleVariable<float> CVarPalmTilt(TEXT("sb.FPPalmTilt"), 0.6f, TEXT("First-person rifles: how far the left palm turns toward the gun's side (0 = straight up)."));
	TAutoConsoleVariable<float> CVarFingerCurl(TEXT("sb.FPFingerCurl"), 0.8f, TEXT("First-person rifles: left finger curl around the forend (0 = open, 1 = gripping)."));
	// Grip contact (Vincent 2026-11-06: fingers kept sinking into the guns). Every frame each finger joint traces the
	// held gun's real surface: a joint whose finger crosses it opens, one that can close further without crossing
	// closes, so fingers settle on the surface whatever the gun or framing.
	TAutoConsoleVariable<int32> CVarGripContact(TEXT("sb.GripContact"), 1, TEXT("1: fingers curl until they touch the held gun's surface (traced each frame)."));
	TAutoConsoleVariable<float> CVarGripSkin(TEXT("sb.GripSkin"), 0.55f, TEXT("Finger radius (cm) the grip contact keeps off the gun's surface."));
	TAutoConsoleVariable<float> CVarGripRange(TEXT("sb.GripRange"), 2500.f, TEXT("Third-person bodies further than this (cm) from the camera skip the grip solve."));
	// Grip contact helpers (defined with the grip code below).
	void GripParts(const AShieldbreakWeapon* Weapon, TArray<UPrimitiveComponent*, TInlineAllocator<2>>& Out);
	bool SegmentHits(const TArray<UPrimitiveComponent*, TInlineAllocator<2>>& Parts, const FVector& A, const FVector& B, float Radius);
	int32 SolveGripContact(const USkeletalMeshComponent* Mesh, const TArray<UPrimitiveComponent*, TInlineAllocator<2>>& Parts, bool bLeft,
		float* Curls, float MinCurl, const float* MaxCurls, float DeltaSeconds);
	const float* LeftMaxCurls();
	const float* RightMaxCurls();
	TAutoConsoleVariable<int32> CVarSupportHand(TEXT("sb.FPSupportHand"), 1, TEXT("1: on rifles the first-person left hand holds the gun's SupportHand socket."));

	/** How each action plays: start a little in (skipping the wind-up) and faster than authored, Halo-snappy. */
	struct FActionTiming { float StartTime; float PlayRate; };
	FActionTiming GetActionTiming(EShieldbreakAction Action)
	{
		switch (Action)
		{
		case EShieldbreakAction::GrenadeThrow:
		{
			// Timed so the hand comes through when the server lets the grenade go, within a believable speed.
			const float Start = CVarTossStart.GetValueOnGameThread();
			const float Release = FMath::Max(Start + 0.05f, CVarTossRelease.GetValueOnGameThread());
			const float Delay = FMath::Max(0.05f, AShieldbreakGrenade::GetReleaseDelay());
			return { Start, FMath::Clamp((Release - Start) / Delay, 0.8f, 2.2f) };
		}
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
	const bool bThrow = Action == EShieldbreakAction::GrenadeThrow && GetWorld();
	if (bThrow)
	{
		// The first-person arms throw procedurally (UpdateFirstPersonThrow); the body plays Lyra's toss for others.
		FirstPersonThrowStart = GetWorld()->GetTimeSeconds();
		BodyThrowStart = FirstPersonThrowStart;
		bFirstPersonThrowKicked = false;
		bBodyTossReleased = false;
		LastLeftHandTarget = FVector::ZeroVector;
	}
	// First person: reload, draw and melee are hand-keyed moves on the arms (ShieldbreakViewmodelMoves); the Lyra
	// clip still plays on the third-person body below for everyone else.
	const bool bFirstPersonMove = FirstPersonArms && GetWorld() && FShieldbreakViewmodelMoves::IsEnabled()
		&& Action != EShieldbreakAction::GrenadeThrow;
	if (bFirstPersonMove)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		switch (Action)
		{
		case EShieldbreakAction::Reload:
			Moves.Start(EShieldbreakViewmodelMove::Reload, GetViewmodelStyle(), Duration, Now, GetHeldMagazine());
			break;
		case EShieldbreakAction::Melee:
			Moves.Start(EShieldbreakViewmodelMove::Melee, GetViewmodelStyle(), 0.f, Now, GetHeldMagazine());
			break;
		case EShieldbreakAction::Equip:
			// The new gun comes up once the arms are placed for it (UpdateViewmodelMoves).
			Moves.Stop(Now, nullptr);
			bDrawPending = true;
			break;
		default:
			break;
		}
	}
	if (Action == EShieldbreakAction::Equip && GetWorld() && !bFirstPersonMove)
	{
		FirstPersonEquipEnd = GetWorld()->GetTimeSeconds() + (Animation->GetPlayLength() - Timing.StartTime) / FMath::Max(0.05f, Timing.PlayRate);
		// Drop the arms at once, before the equip pose reaches the screen.
		SwapLower = 1.f;
		if (FirstPersonArms && CalibratedIdle)
		{
			ApplyFirstPersonArmsTransform(AppliedThrowDip);
		}
	}
	for (USkeletalMeshComponent* Mesh : { GetBodyMesh(), bFirstPersonMove ? nullptr : FirstPersonArms.Get() })
	{
		if (bThrow && Mesh == FirstPersonArms.Get())
		{
			continue;
		}
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
	// The hand-keyed move turns the arms about the gun hand too, under the procedural motion.
	const FTransform MoveDelta(FRotator(MovePose.Rotation.X, MovePose.Rotation.Y, MovePose.Rotation.Z), MovePose.Offset);
	const FTransform Move = FTransform(-Grip) * MoveDelta * FTransform(Grip);
	FirstPersonArms->SetRelativeTransform(CalibratedArmsTransform * Move * FTransform(FRotator(-Pitch, 0.f, Pitch * 0.4f))
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
		FirstPersonArms->RegisterOnBoneTransformsFinalizedDelegate(
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &UShieldbreakBodyAnimator::UpdateHeldMagazineAfterPose));
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
	// A move can bring the left arm in for a gun that otherwise hides it (the sidearm's reload); it shows once the
	// arm has reached its first, out-of-view, key.
	const bool bShowArm = bRifle || (MovePose.bWantsLeftArm && SupportHandWeight > 0.95f);
	if (bShowArm != bSupportArmShown)
	{
		bSupportArmShown = bShowArm;
		if (bShowArm)
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
	const bool bSettled = World->GetTimeSeconds() - FirstPersonIdleSince > 0.3f && !Instance->GetActionAnimation() && GetFirstPersonThrowTime() < 0.f;
	const AShieldbreakWeapon* HeldWeapon = OwnerCharacter->GetCurrentWeapon();
	const FVector HandOffset = HeldWeapon ? HeldWeapon->GetHandGripOffset() : FVector::ZeroVector;
	if (bSettled && (CalibratedIdle != Idle || CalibratedGrip != Grip || CalibratedAim != Aim || !CalibratedHandOffset.Equals(HandOffset, 0.05f)))
	{
		CalibratedIdle = Idle;
		CalibratedHandOffset = HandOffset;
		CalibratedGrip = Grip;
		CalibratedAim = Aim;
		CalibrateFirstPersonArms();
	}

	const float Now = World->GetTimeSeconds();
	const float Dip = CalibratedIdle ? UpdateFirstPersonThrow(Instance, Now) : 0.f;
	const bool bSwapping = Now < FirstPersonEquipEnd || (CalibratedIdle && CalibratedIdle != Idle);
	SwapLower = bSwapping ? 1.f : FMath::Max(0.f, SwapLower - World->GetDeltaSeconds() / 0.25f);
	if (CalibratedIdle)
	{
		UpdateFirstPersonMotion(World->GetDeltaSeconds());
		// Placed first with last frame's move pose, so the move's hand keys read the gun where it is now; then again
		// with this frame's pose (only while a move plays).
		ApplyFirstPersonArmsTransform(Dip);
		UpdateViewmodelMoves(bSwapping);
	}
	UpdateTriggerFinger(Instance, CalibratedIdle && !bSwapping && Dip <= 0.f && !Instance->GetActionAnimation(), World->GetDeltaSeconds());
	UpdateSupportHand(Instance, CalibratedIdle && !bSwapping && Dip <= 0.f && !Instance->GetActionAnimation(), World->GetDeltaSeconds());
	UpdateFirstPersonRightGrip(Instance, CalibratedIdle && !bSwapping && Dip <= 0.f && !Instance->GetActionAnimation(), World->GetDeltaSeconds());
}

UStaticMeshComponent* UShieldbreakBodyAnimator::GetHeldMagazine() const
{
	const AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner());
	const AShieldbreakWeapon* Weapon = OwnerCharacter ? OwnerCharacter->GetCurrentWeapon() : nullptr;
	return Weapon ? Weapon->GetFirstPersonMagazine() : nullptr;
}

EShieldbreakViewmodelStyle UShieldbreakBodyAnimator::GetViewmodelStyle() const
{
	const AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner());
	const AShieldbreakWeapon* Weapon = OwnerCharacter ? OwnerCharacter->GetCurrentWeapon() : nullptr;
	if (!Weapon)
	{
		return EShieldbreakViewmodelStyle::Rifle;
	}
	const FShieldbreakWeaponStats& Stats = Weapon->GetStats();
	if (Stats.bPistolAnimations)
	{
		return EShieldbreakViewmodelStyle::Pistol;
	}
	if (Stats.Scope == EShieldbreakScope::LongRange)
	{
		return EShieldbreakViewmodelStyle::Marksman;
	}
	if (Stats.Scope == EShieldbreakScope::Marksman || (Weapon->GetWeaponMesh() && Weapon->GetWeaponMesh()->DoesSocketExist(TEXT("Magazine"))))
	{
		return EShieldbreakViewmodelStyle::Rifle;
	}
	return EShieldbreakViewmodelStyle::Launcher;
}

void UShieldbreakBodyAnimator::UpdateViewmodelMoves(bool bSwapping)
{
	AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner());
	const UWorld* World = GetWorld();
	if (!OwnerCharacter || !World || !FirstPersonArms)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	AShieldbreakWeapon* Weapon = OwnerCharacter->GetCurrentWeapon();
	UStaticMeshComponent* Magazine = Weapon ? Weapon->GetFirstPersonMagazine() : nullptr;
	if (!FShieldbreakViewmodelMoves::IsEnabled())
	{
		Moves.Stop(Now, Magazine);
		bDrawPending = false;
	}
	// The draw waits until the arms are placed for the new gun (a pistol/rifle change re-places them), then comes up
	// from below the screen in place of the old drop-and-rise.
	if (bDrawPending && !bSwapping)
	{
		bDrawPending = false;
		Moves.Start(EShieldbreakViewmodelMove::Draw, GetViewmodelStyle(), 0.f, Now, Magazine);
		SwapLower = 0.f;
	}
	FShieldbreakViewmodelContext Context;
	Context.Camera = OwnerCharacter->GetFirstPersonCamera()->GetComponentTransform();
	Context.Gun = Weapon && Weapon->GetRootComponent() && Weapon->GetRootComponent()->GetAttachParent() == FirstPersonArms
		? Weapon->GetWeaponMesh() : nullptr;
	Context.Magazine = Context.Gun ? Magazine : nullptr;
	if (UStaticMeshComponent* SupportGun = Context.Gun && Context.Gun->DoesSocketExist(TEXT("SupportHand")) ? Weapon->GetWeaponMesh() : nullptr)
	{
		// The hand is placed by its palm now: the move's forend key is the same palm point the idle hold uses.
		FVector Palm, Fingers, Normal;
		ComputeSupportPalm(SupportGun, FirstPersonArms, Palm, Fingers, Normal);
		Context.SupportPalmLocal = SupportGun->GetComponentTransform().InverseTransformPosition(Palm);
		Context.bHasSupportPalm = true;
	}
	Context.Arms = FirstPersonArms;
	const float ReloadProgress = Weapon ? Weapon->GetReloadProgress() : -1.f;
	Context.bReloading = Weapon && (Weapon->IsReloading() || (ReloadProgress >= 0.f && ReloadProgress < 1.f));
	Moves.Update(Now, Context, MovePose);
	// Hand anchors must see this frame's gun placement, not the previous move pose.
	ApplyFirstPersonArmsTransform(AppliedThrowDip);
	Moves.RefreshHandTarget(Now, Context, MovePose);
}

void UShieldbreakBodyAnimator::UpdateHeldMagazineAfterPose()
{
	const AShieldbreakCharacter* Character = Cast<AShieldbreakCharacter>(GetOwner());
	const AShieldbreakWeapon* Weapon = Character ? Character->GetCurrentWeapon() : nullptr;
	if (!Weapon || !Weapon->GetRootComponent() || !FirstPersonArms || Weapon->GetRootComponent()->GetAttachParent() != FirstPersonArms) { return; }
	// This callback precedes Unreal's attached-child update. Bring the gun to its
	// new socket first, so the later child update cannot move the carried magazine again.
	Weapon->GetRootComponent()->UpdateComponentToWorld();
	FShieldbreakViewmodelContext Context;
	Context.Camera = Character->GetFirstPersonCamera()->GetComponentTransform();
	Context.Gun = Weapon->GetWeaponMesh();
	Context.Magazine = Weapon->GetFirstPersonMagazine();
	Context.Arms = FirstPersonArms;
	Moves.UpdateMagazineAfterPose(GetWorld()->GetTimeSeconds(), Context);
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

void UShieldbreakBodyAnimator::ComputeSupportPalm(UStaticMeshComponent* Gun, const USkeletalMeshComponent* HandMesh, FVector& OutPalm, FVector& OutFingers, FVector& OutPalmNormal) const
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	FSupportPalmCache& Cache = HandMesh == FirstPersonArms ? FirstPersonPalmCache : ThirdPersonPalmCache;
	const FTransform& GunTransform = Gun->GetComponentTransform();
	const float Now = GetWorld()->GetTimeSeconds();
	if (Cache.Gun == Gun && Now < Cache.UpdateAt)
	{
		OutPalm = GunTransform.TransformPosition(Cache.Palm);
		OutFingers = GunTransform.TransformVectorNoScale(Cache.Fingers);
		OutPalmNormal = GunTransform.TransformVectorNoScale(Cache.Normal);
		return;
	}
	// Gun axes from its sockets, in world space: forward from the trigger to the muzzle, up from the mesh, right
	// from those two (Unreal is left-handed: Up x Forward points right).
	const FVector Socket = Gun->GetSocketLocation(TEXT("SupportHand"));
	const FVector Forward = Gun->DoesSocketExist(TEXT("Muzzle")) && Gun->DoesSocketExist(TEXT("Trigger"))
		? (Gun->GetSocketLocation(TEXT("Muzzle")) - Gun->GetSocketLocation(TEXT("Trigger"))).GetSafeNormal() : Gun->GetForwardVector();
	FVector Up = Gun->GetUpVector();
	Up = (Up - Forward * FVector::DotProduct(Up, Forward)).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
	OutPalm = Socket + Right * CVarPalmRight.GetValueOnGameThread() + Forward * CVarPalmForward.GetValueOnGameThread()
		+ Up * CVarPalmUp.GetValueOnGameThread();
	// Palm up and turned toward the gun; fingers across the underside toward the far side, a little forward.
	const float Wrap = FMath::Clamp(CVarPalmWrap.GetValueOnGameThread(), 0.f, 1.f);
	OutPalmNormal = (Up + Right * CVarPalmTilt.GetValueOnGameThread()).GetSafeNormal();
	FVector Fingers = Right * Wrap + Forward * FMath::Sqrt(FMath::Max(0.f, 1.f - Wrap * Wrap));
	OutFingers = (Fingers - OutPalmNormal * FVector::DotProduct(Fingers, OutPalmNormal)).GetSafeNormal();
	// Solve a reachable palm before curling. IK clamps an unreachable wrist along
	// its reach sphere, which otherwise raises the actual palm back into the gun.
	const FVector Shoulder = HandMesh->GetBoneLocation(TEXT("upperarm_l"));
	const FVector Elbow = HandMesh->GetBoneLocation(TEXT("lowerarm_l"));
	const FVector Wrist = HandMesh->GetBoneLocation(TEXT("hand_l"));
	const FVector Knuckle = HandMesh->GetBoneLocation(TEXT("middle_01_l"));
	const FVector Thumb = HandMesh->GetBoneLocation(TEXT("thumb_01_l"));
	const FVector HandForward = (Knuckle - Wrist).GetSafeNormal();
	const FVector HandNormal = -FVector::CrossProduct(HandForward, Thumb - Wrist).GetSafeNormal();
	const FQuat Turn = FRotationMatrix::MakeFromXZ(OutFingers, OutPalmNormal).ToQuat()
		* FRotationMatrix::MakeFromXZ(HandForward, HandNormal).ToQuat().Inverse();
	const FVector PalmOffset = Turn.RotateVector((Knuckle - Wrist) * 0.5f);
	const float Reach = FVector::Distance(Shoulder, Elbow) + FVector::Distance(Elbow, Wrist) - 1.f;
	const FVector AuthoredPalm = OutPalm;
	FVector BestPalm = OutPalm;
	int32 BestBlocked = 6;
	TArray<UPrimitiveComponent*, TInlineAllocator<2>> Parts;
	GripParts(Cast<AShieldbreakWeapon>(Gun->GetOwner()), Parts);
	// A palm spans more than its centre: the wrist can overlap a magazine even
	// with the centre under clear forend. Try the nearest outside hold first.
	for (int32 Trial = 0; Trial < 5; ++Trial)
	{
		OutPalm = AuthoredPalm - Right * (Trial * 1.5f);
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			FHitResult Underside;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(ShieldbreakSupportPalm), true);
			if (Gun->LineTraceComponent(Underside, OutPalm - Up * 40.f, OutPalm + Up * 20.f, Query))
			{
				const float Gap = FVector::DotProduct(OutPalm - Underside.ImpactPoint, Up);
				OutPalm -= Up * FMath::Max(0.f, Gap + 3.5f);
			}
			const FVector ToWrist = OutPalm - PalmOffset - Shoulder;
			if (Reach <= 1.f || ToWrist.SizeSquared() <= FMath::Square(Reach)) { break; }
			const double Along = FVector::DotProduct(ToWrist, Forward);
			const double AcrossSquared = ToWrist.SizeSquared() - FMath::Square(Along);
			if (AcrossSquared >= FMath::Square(Reach)) { break; }
			const double Back = Along - FMath::Sqrt(FMath::Square(Reach) - AcrossSquared);
			if (Back <= 0.0) { break; }
			OutPalm -= Forward * (Back + 0.25);
		}
		const FVector FittedWrist = OutPalm - PalmOffset;
		int32 Blocked = 0;
		for (int32 Finger = 0; Finger < 5; ++Finger)
		{
			const FVector Base = HandMesh->GetBoneLocation(FName(ShieldbreakGrip::LeftBones[Finger * 3]));
			Blocked += SegmentHits(Parts, FittedWrist, FittedWrist + Turn.RotateVector(Base - Wrist), 0.55f) ? 1 : 0;
		}
		if (Blocked < BestBlocked) { BestBlocked = Blocked; BestPalm = OutPalm; }
		if (Blocked == 0) { break; }
	}
	OutPalm = BestPalm;
	// Cache in gun space, so the target still follows its rendered pose every
	// frame. Refit at 20 Hz for the owner and the existing 10 Hz body-grip rate.
	Cache.Gun = Gun;
	Cache.Palm = GunTransform.InverseTransformPosition(OutPalm);
	Cache.Fingers = GunTransform.InverseTransformVectorNoScale(OutFingers);
	Cache.Normal = GunTransform.InverseTransformVectorNoScale(OutPalmNormal);
	Cache.UpdateAt = Now + (HandMesh == FirstPersonArms ? 0.05f : 0.1f);
}

void UShieldbreakBodyAnimator::UpdateSupportHand(UShieldbreakBodyAnimInstance* Instance, bool bHolding, float DeltaSeconds)
{
	UStaticMeshComponent* Gun = bHolding && WantsFirstPersonSupportArm() ? GetSupportMesh(true) : nullptr;
	// A first-person move (reload) can lead the left hand elsewhere: blended over the socket by its own weight,
	// and still eased in and out here.
	const bool bMoveHand = bHolding && MovePose.HandWeight > 0.f;
	SupportHandWeight = FMath::FInterpConstantTo(SupportHandWeight, Gun || bMoveHand ? 1.f : 0.f, DeltaSeconds, 1.f / 0.15f);
	if (!Gun && !bMoveHand && SupportHandWeight <= 0.f)
	{
		Instance->SetSupportHand(0.f, FVector::ZeroVector, FVector::ForwardVector, FVector::UpVector, 0.f);
		return;
	}
	if (Gun || bMoveHand)
	{
		using namespace ShieldbreakBodyAnimatorPrivate;
		FVector Palm, Fingers, PalmNormal;
		float Curl = FMath::Clamp(CVarFingerCurl.GetValueOnGameThread(), 0.f, 1.5f);
		if (Gun)
		{
			ComputeSupportPalm(Gun, FirstPersonArms, Palm, Fingers, PalmNormal);
			SupportPalmWorld = Palm;
		}
		else
		{
			// No forend (the sidearm's reload): palm up, fingers ahead, from the camera.
			const FTransform Camera = Cast<AShieldbreakCharacter>(GetOwner())->GetFirstPersonCamera()->GetComponentTransform();
			Palm = MovePose.HandWorld;
			Fingers = Camera.GetUnitAxis(EAxis::X);
			PalmNormal = Camera.GetUnitAxis(EAxis::Z);
		}
		if (bMoveHand)
		{
			// A first-person move (reload) leads the palm elsewhere, blended over the forend hold by its own weight;
			// the hand opens a little to carry the magazine.
			const float MoveWeight = FMath::Clamp(MovePose.HandWeight, 0.f, 1.f);
			Palm = FMath::Lerp(Palm, MovePose.HandWorld, MoveWeight);
			Curl = FMath::Lerp(Curl, FMath::Min(Curl, 0.5f), MoveWeight);
			if (MovePose.HandPoseWeight > 0.f)
			{
				const FQuat IdleFrame = FRotationMatrix::MakeFromXZ(Fingers, PalmNormal).ToQuat();
				const FQuat Frame = FQuat::Slerp(IdleFrame, MovePose.HandOrientation, MoveWeight * MovePose.HandPoseWeight).GetNormalized();
				Fingers = Frame.GetAxisX();
				PalmNormal = Frame.GetAxisZ();
			}
		}
		const FTransform& ToArms = FirstPersonArms->GetComponentTransform();
		Instance->SetSupportHand(SupportHandWeight, ToArms.InverseTransformPosition(Palm),
			ToArms.InverseTransformVectorNoScale(Fingers), ToArms.InverseTransformVectorNoScale(PalmNormal), Curl);
		// Fingers settle on the gun's real surface (see CVarGripContact), unless a move has taken the hand away.
		const AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner());
		if (bMoveHand && MovePose.HandPoseWeight > 0.f)
		{
			// Preserve surface corrections while authored finger shapes close/release.
			for (int32 I = 0; I < 15; ++I)
			{
				const float Target = FMath::Lerp(ShieldbreakGrip::LeftNominal[I] * Curl,
					MovePose.HandCurls[I], MovePose.HandPoseWeight);
				FPReloadGrip[I] = bFPReloadGripInit ? FPReloadGrip[I] + Target - LastReloadGripTarget[I] : Target;
				FPReloadGrip[I] = FMath::Clamp(FPReloadGrip[I], -15.f, Target);
				LastReloadGripTarget[I] = Target;
			}
			bFPReloadGripInit = true;
			if (UStaticMeshComponent* Magazine = GetHeldMagazine(); Magazine && Magazine->IsVisible() && !Moves.IsMagazineFalling())
			{
				TArray<UPrimitiveComponent*, TInlineAllocator<2>> Parts;
				if (const AShieldbreakWeapon* HeldWeapon = OwnerCharacter ? OwnerCharacter->GetCurrentWeapon() : nullptr)
				{
					Parts.Add(HeldWeapon->GetWeaponMesh());
				}
				Parts.Add(Magazine);
				SolveGripContact(FirstPersonArms, Parts, true, FPReloadGrip, -15.f, LastReloadGripTarget, DeltaSeconds);
			}
			Instance->SetLeftFingerCurls(FPReloadGrip);
			bFPLeftGripInit = false;
		}
		else if (Gun && CVarGripContact.GetValueOnGameThread() != 0 && MovePose.HandWeight <= 0.5f && OwnerCharacter)
		{
			if (!bFPLeftGripInit)
			{
				FMemory::Memzero(FPLeftGrip, sizeof(FPLeftGrip)); // close onto the surface from the open pose
				bFPLeftGripInit = true;
			}
			TArray<UPrimitiveComponent*, TInlineAllocator<2>> Parts;
			GripParts(OwnerCharacter->GetCurrentWeapon(), Parts);
			SolveGripContact(FirstPersonArms, Parts, true, FPLeftGrip, -15.f, LeftMaxCurls(), DeltaSeconds);
			Instance->SetLeftFingerCurls(FPLeftGrip);
		}
		else
		{
			Instance->SetLeftFingerCurls(nullptr);
			bFPLeftGripInit = false;
		}
		if (!bMoveHand || MovePose.HandPoseWeight <= 0.f) { bFPReloadGripInit = false; }
	}
	else
	{
		Instance->SetSupportHand(SupportHandWeight, Instance->GetSupportTarget(), Instance->GetSupportFingers(), Instance->GetSupportPalmNormal(),
			Instance->GetSupportCurl());
	}
}

// ---- Grip contact ----

namespace ShieldbreakBodyAnimatorPrivate
{
	/** The parts of a gun the hands can touch: the receiver mesh and, when shown, the magazine. */
	void GripParts(const AShieldbreakWeapon* Weapon, TArray<UPrimitiveComponent*, TInlineAllocator<2>>& Out)
	{
		Out.Reset();
		if (!Weapon)
		{
			return;
		}
		for (UStaticMeshComponent* Part : { Weapon->GetWeaponMesh(), Weapon->GetMagazineMesh() })
		{
			if (Part && Part->GetStaticMesh() && Part->IsVisible() && Part->IsCollisionEnabled())
			{
				Out.Add(Part);
			}
		}
	}

	/** Does the segment cross a part's surface (Radius 0: the centre line; else a sphere of that radius)? Traced both
	 *  ways, against the complex (render) triangles. */
	bool SegmentHits(const TArray<UPrimitiveComponent*, TInlineAllocator<2>>& Parts, const FVector& A, const FVector& B, float Radius)
	{
		if (FVector::DistSquared(A, B) < 0.0001)
		{
			return false;
		}
		static const FName Tag(TEXT("SBGrip"));
		FCollisionQueryParams Params(Tag, true);
		for (UPrimitiveComponent* Part : Parts)
		{
			FHitResult Hit;
			if (Radius > 0.f)
			{
				const FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
				if (Part->SweepComponent(Hit, A, B, FQuat::Identity, Sphere, true) || Part->SweepComponent(Hit, B, A, FQuat::Identity, Sphere, true))
				{
					return true;
				}
			}
			else if (Part->LineTraceComponent(Hit, A, B, Params) || Part->LineTraceComponent(Hit, B, A, Params))
			{
				return true;
			}
		}
		return false;
	}

	/** World joints of one finger (_01, _02, _03, tip). */
	bool FingerPoints(const USkeletalMeshComponent* Mesh, const TCHAR* const* Bones, int32 Finger, FVector (&Out)[4])
	{
		for (int32 J = 0; J < 3; ++J)
		{
			const FName Bone(Bones[Finger * 3 + J]);
			if (Mesh->GetBoneIndex(Bone) == INDEX_NONE)
			{
				return false;
			}
			Out[J] = Mesh->GetBoneLocation(Bone);
		}
		// The last joint can rotate without moving either _02 or _03. Extending their
		// positions ignores that curl, so the contact servo never sees its distal fix.
		const USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
		if (!Asset) { return false; }
		const int32 TipBone = Mesh->GetBoneIndex(FName(Bones[Finger * 3 + 2]));
		const FTransform& RefLocal = Asset->GetRefSkeleton().GetRefBonePose()[TipBone];
		const FVector TipAxis = RefLocal.InverseTransformVectorNoScale(RefLocal.GetTranslation()).GetSafeNormal();
		Out[3] = Out[2] + Mesh->GetBoneTransform(TipBone).TransformVectorNoScale(TipAxis).GetSafeNormal()
			* ShieldbreakGrip::TipLength[Finger];
		return true;
	}

	/** Palm normal from the hand's joints, as the anim proxy computes it. */
	FVector PalmNormal(const USkeletalMeshComponent* Mesh, bool bLeft)
	{
		const TCHAR* const* Bones = bLeft ? ShieldbreakGrip::LeftBones : ShieldbreakGrip::RightBones;
		const FName Hand(bLeft ? TEXT("hand_l") : TEXT("hand_r"));
		if (Mesh->GetBoneIndex(Hand) == INDEX_NONE || Mesh->GetBoneIndex(FName(Bones[3])) == INDEX_NONE || Mesh->GetBoneIndex(FName(Bones[12])) == INDEX_NONE)
		{
			return FVector::ZeroVector;
		}
		const FVector Wrist = Mesh->GetBoneLocation(Hand);
		const FVector F = (Mesh->GetBoneLocation(FName(Bones[3])) - Wrist).GetSafeNormal();
		FVector T = Mesh->GetBoneLocation(FName(Bones[12])) - Wrist;
		T = (T - F * FVector::DotProduct(T, F)).GetSafeNormal();
		return (bLeft ? -1.f : 1.f) * FVector::CrossProduct(F, T).GetSafeNormal();
	}

	/** One servo step of every grip joint of one hand (see CVarGripContact). Curls are degrees toward the palm,
	 *  kept within [MinCurl, MaxCurls[i]]. Returns how many fingers were crossing the gun this frame. */
	int32 SolveGripContact(const USkeletalMeshComponent* Mesh, const TArray<UPrimitiveComponent*, TInlineAllocator<2>>& Parts, bool bLeft,
		float* Curls, float MinCurl, const float* MaxCurls, float DeltaSeconds)
	{
		const FVector Normal = PalmNormal(Mesh, bLeft);
		if (Normal.IsNearlyZero() || Parts.Num() == 0)
		{
			return 0;
		}
		const float Skin = FMath::Max(0.f, CVarGripSkin.GetValueOnGameThread());
		const float Step = FMath::Clamp(DeltaSeconds * 300.f, 1.f, 8.f);
		const TCHAR* const* Bones = bLeft ? ShieldbreakGrip::LeftBones : ShieldbreakGrip::RightBones;
		int32 Crossing = 0;
		for (int32 Finger = 0; Finger < 5; ++Finger)
		{
			FVector P[4];
			if ((!bLeft && Finger == 0) || !FingerPoints(Mesh, Bones, Finger, P))
			{
				continue;
			}
			bool bFingerCrossing = false;
			for (int32 J = 0; J < 3; ++J)
			{
				float& Curl = Curls[Finger * 3 + J];
				bool bNow = false;
				for (int32 K = J; K < 3 && !bNow; ++K)
				{
					bNow = SegmentHits(Parts, P[K], P[K + 1], Skin);
				}
				if (bNow)
				{
					bool bPenetrating = false;
					for (int32 K = J; K < 3 && !bPenetrating; ++K)
					{
						bPenetrating = SegmentHits(Parts, P[K], P[K + 1], 0.f);
					}
					// A skin-radius touch is a resting contact. Only a crossing needs
					// the escape search; repeatedly searching at rest wastes frame time.
					if (!bPenetrating) { continue; }
					bFingerCrossing = true;
					// Opening is not always the escape direction (a wrapped thumb or
					// middle finger can open back through the receiver). Look both ways
					// for a less obstructed bend, then approach it at the servo speed.
					const FVector Along = J == 2 ? P[2] - P[1] : P[J + 1] - P[J];
					const FVector Axis = FVector::CrossProduct(Along.GetSafeNormal(), Normal).GetSafeNormal();
					int32 BestScore = 0;
					// Escape centre-line crossings with cheap triangle rays. The normal
					// closing prediction still uses skin-radius sweeps for resting contact.
					for (int32 K = J; K < 3; ++K) { BestScore += SegmentHits(Parts, P[K], P[K + 1], 0.f) ? 1 : 0; }
					float BestCurl = Curl;
					const float MaxCurl = MaxCurls[Finger * 3 + J];
					const int32 Samples = FMath::CeilToInt(FMath::Max(Curl - MinCurl, MaxCurl - Curl) / 8.f);
					for (int32 Sample = 1; Sample <= Samples && BestScore > 0 && !Axis.IsNearlyZero(); ++Sample)
					{
						for (const float Sign : { -1.f, 1.f })
						{
							const float Candidate = FMath::Clamp(Curl + Sign * Sample * 8.f, MinCurl, MaxCurl);
							const FQuat Turn(Axis, FMath::DegreesToRadians(Candidate - Curl));
							FVector Q[4] = { P[0], P[1], P[2], P[3] };
							for (int32 K = J + 1; K < 4; ++K) { Q[K] = P[J] + Turn.RotateVector(P[K] - P[J]); }
							int32 Score = 0;
							for (int32 K = J; K < 3; ++K) { Score += SegmentHits(Parts, Q[K], Q[K + 1], 0.f) ? 1 : 0; }
							if (Score < BestScore) { BestScore = Score; BestCurl = Candidate; }
							if (BestScore == 0) { break; }
						}
					}
					const float Change = FMath::Clamp(BestCurl - Curl, -Step * 1.5f, Step * 1.5f);
					Curl += Change;
					const FQuat Turn(Axis, FMath::DegreesToRadians(Change));
					for (int32 K = J + 1; K < 4; ++K) { P[K] = P[J] + Turn.RotateVector(P[K] - P[J]); }
					continue;
				}
				if (Curl >= MaxCurls[Finger * 3 + J])
				{
					continue;
				}
				// Would one more step toward the palm still be clear?
				const FVector Along = J == 2 ? P[2] - P[1] : P[J + 1] - P[J];
				const FVector Axis = FVector::CrossProduct(Along.GetSafeNormal(), Normal).GetSafeNormal();
				if (Axis.IsNearlyZero())
				{
					continue;
				}
				const FQuat Turn(Axis, FMath::DegreesToRadians(Step));
				FVector Q[4] = { P[0], P[1], P[2], P[3] };
				for (int32 K = J + 1; K < 4; ++K)
				{
					Q[K] = P[J] + Turn.RotateVector(P[K] - P[J]);
				}
				bool bBlocked = false;
				for (int32 K = J; K < 3 && !bBlocked; ++K)
				{
					bBlocked = SegmentHits(Parts, Q[K], Q[K + 1], Skin);
				}
				if (!bBlocked)
				{
					Curl = FMath::Min(MaxCurls[Finger * 3 + J], Curl + Step);
					for (int32 K = J + 1; K < 4; ++K) { P[K] = Q[K]; }
				}
			}
			Crossing += bFingerCrossing ? 1 : 0;
		}
		return Crossing;
	}

	/** Max curls: the left hand may close up to 1.8x its nominal wrap; the right fingers up to 40 degrees past the pose. */
	const float* LeftMaxCurls()
	{
		static float Max[ShieldbreakGrip::NumJoints];
		static bool bInit = false;
		if (!bInit)
		{
			for (int32 I = 0; I < ShieldbreakGrip::NumJoints; ++I) { Max[I] = FMath::Max(20.f, ShieldbreakGrip::LeftNominal[I] * 1.8f); }
			bInit = true;
		}
		return Max;
	}
	FVector TriggerContact(UStaticMeshComponent* Gun)
	{
		const FVector Socket = Gun->GetSocketLocation(TEXT("Trigger"));
		const FVector Forward = Gun->GetComponentTransform().GetUnitAxis(EAxis::Y);
		FHitResult Blade;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(ShieldbreakTriggerContact), true);
		// Stay within the guard's opening: the socket is behind the blade, so
		// targeting it directly drives the index finger through the solid trigger.
		if (Gun->LineTraceComponent(Blade, Socket + Forward * 2.f, Socket, Query))
		{
			return Blade.ImpactPoint + Forward * 0.65f;
		}
		return Socket;
	}

	const float* RightMaxCurls()
	{
		static float Max[ShieldbreakGrip::NumJoints];
		static bool bInit = false;
		if (!bInit)
		{
			for (int32 I = 0; I < ShieldbreakGrip::NumJoints; ++I) { Max[I] = 40.f; }
			bInit = true;
		}
		return Max;
	}
}

int32 UShieldbreakBodyAnimator::MeasureHandClipping(bool bFirstPerson, FString* OutDetail, bool bIncludeMagazine) const
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	const AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner());
	const AShieldbreakWeapon* Weapon = OwnerCharacter ? OwnerCharacter->GetCurrentWeapon() : nullptr;
	const USkeletalMeshComponent* Mesh = bFirstPerson ? FirstPersonArms.Get() : GetBodyMesh();
	TArray<UPrimitiveComponent*, TInlineAllocator<2>> Parts;
	GripParts(Weapon, Parts);
	if (!bIncludeMagazine && Parts.Num() > 1)
	{
		Parts.SetNum(1); // the receiver only (a reload's hand carries the magazine)
	}
	if (!Mesh || Parts.Num() == 0 || !Weapon->GetRootComponent() || Weapon->GetRootComponent()->GetAttachParent() != Mesh)
	{
		return -1;
	}
	// Probe: a line straight through the receiver must hit, or the surface isn't testable (no collision data).
	const FTransform GunXf = Parts[0]->GetComponentTransform();
	const FBoxSphereBounds LocalBounds = Parts[0]->CalcBounds(FTransform::Identity);
	const FVector Centre = GunXf.TransformPosition(LocalBounds.Origin);
	const FVector Across = GunXf.TransformVectorNoScale(FVector(1.0, 0.0, 0.0)) * (LocalBounds.BoxExtent.X + 5.0);
	TArray<UPrimitiveComponent*, TInlineAllocator<2>> Receiver;
	Receiver.Add(Parts[0]);
	if (!SegmentHits(Receiver, Centre - Across, Centre + Across, 0.f) && !SegmentHits(Receiver, Centre - GunXf.TransformVectorNoScale(FVector::UpVector) * 30.0,
		Centre + GunXf.TransformVectorNoScale(FVector::UpVector) * 30.0, 0.f))
	{
		if (OutDetail) { *OutDetail = FString::Printf(TEXT("%s has no testable surface"), *GetNameSafe(Cast<UStaticMeshComponent>(Parts[0]) ? Cast<UStaticMeshComponent>(Parts[0])->GetStaticMesh() : nullptr)); }
		return -1;
	}
	int32 Count = 0;
	FString Detail;
	for (const bool bLeft : { true, false })
	{
		const FName Shoulder(bLeft ? TEXT("upperarm_l") : TEXT("upperarm_r"));
		if (Mesh->GetBoneIndex(Shoulder) == INDEX_NONE || (bFirstPerson && !bLeft && bTriggerArmHidden)
			|| (bFirstPerson && bLeft && !WantsFirstPersonSupportArm()))
		{
			continue; // that arm isn't drawn
		}
		const TCHAR* const* Bones = bLeft ? ShieldbreakGrip::LeftBones : ShieldbreakGrip::RightBones;
		const FName Hand(bLeft ? TEXT("hand_l") : TEXT("hand_r"));
		const FVector Wrist = Mesh->GetBoneLocation(Hand);
		for (int32 Finger = 0; Finger < 5; ++Finger)
		{
			FVector P[4];
			if (!FingerPoints(Mesh, Bones, Finger, P))
			{
				continue;
			}
			// Palm (wrist to knuckle) and the finger's three segments.
			const FVector Chain[5] = { Wrist, P[0], P[1], P[2], P[3] };
			for (int32 K = 0; K < 4; ++K)
			{
				if (SegmentHits(Parts, Chain[K], Chain[K + 1], 0.f))
				{
					++Count;
					Detail += FString::Printf(TEXT(" %s%s"), K == 0 ? TEXT("palm>") : TEXT(""), Bones[Finger * 3 + FMath::Max(0, K - 1)]);
					if (FParse::Param(FCommandLine::Get(), TEXT("SBGripDebug")))
					{
						FCollisionQueryParams Query(SCENE_QUERY_STAT(ShieldbreakGripDebug), true);
						Query.bReturnFaceIndex = true;
						for (UPrimitiveComponent* Part : Parts)
						{
							FHitResult Hit;
							if (Part->LineTraceComponent(Hit, Chain[K], Chain[K + 1], Query)
								|| Part->LineTraceComponent(Hit, Chain[K + 1], Chain[K], Query))
							{
								UE_LOG(LogShieldbreak, Log, TEXT("GripDebug %s %s %s%s: A %s B %s hit %s part %s face %d"),
									*GetNameSafe(Weapon->GetClass()), bFirstPerson ? TEXT("FP") : TEXT("TP"), K == 0 ? TEXT("palm>") : TEXT(""),
									Bones[Finger * 3 + FMath::Max(0, K - 1)],
									*GunXf.InverseTransformPosition(Chain[K]).ToCompactString(), *GunXf.InverseTransformPosition(Chain[K + 1]).ToCompactString(),
									*GunXf.InverseTransformPosition(Hit.ImpactPoint).ToCompactString(), *GetNameSafe(Part), Hit.FaceIndex);
							}
						}
					}
				}
			}
		}
	}
	if (OutDetail) { *OutDetail = Detail.IsEmpty() ? TEXT("none") : Detail.TrimStart(); }
	return Count;
}

void UShieldbreakBodyAnimator::UpdateFirstPersonRightGrip(UShieldbreakBodyAnimInstance* Instance, bool bHolding, float DeltaSeconds)
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	const AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner());
	const AShieldbreakWeapon* Weapon = OwnerCharacter ? OwnerCharacter->GetCurrentWeapon() : nullptr;
	TArray<UPrimitiveComponent*, TInlineAllocator<2>> Parts;
	const bool bActive = bHolding && CVarGripContact.GetValueOnGameThread() != 0 && !bTriggerArmHidden && Weapon && Weapon->GetRootComponent()
		&& Weapon->GetRootComponent()->GetAttachParent() == FirstPersonArms;
	if (bActive)
	{
		GripParts(Weapon, Parts);
		SolveGripContact(FirstPersonArms, Parts, false, FPRightGrip, -25.f, RightMaxCurls(), DeltaSeconds);
	}
	FPRightGripWeight = FMath::FInterpConstantTo(FPRightGripWeight, bActive ? 1.f : 0.f, DeltaSeconds, 1.f / 0.15f);
	if (FPRightGripWeight <= 0.f)
	{
		FMemory::Memzero(FPRightGrip, sizeof(FPRightGrip));
	}
	Instance->SetRightFingerCurls(FPRightGrip, FPRightGripWeight);
}

void UShieldbreakBodyAnimator::UpdateThirdPersonGrip(UShieldbreakBodyAnimInstance* Instance, USkeletalMeshComponent* Body, float DeltaSeconds)
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	// Other players' and bots' bodies: the same forend hold for the left hand and the same contact solve for both
	// hands' fingers, while no action (reload, throw, melee) is moving the arms. Skipped off screen and far away.
	const AShieldbreakCharacter* OwnerCharacter = Cast<AShieldbreakCharacter>(GetOwner());
	AShieldbreakWeapon* Weapon = OwnerCharacter ? OwnerCharacter->GetCurrentWeapon() : nullptr;
	const UWorld* World = GetWorld();
	const APlayerController* Viewer = World ? World->GetFirstPlayerController() : nullptr;
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation;
	if (Viewer)
	{
		Viewer->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	const bool bNear = Viewer && FVector::DistSquared(ViewLocation, Body->GetComponentLocation()) < FMath::Square(CVarGripRange.GetValueOnGameThread());
	const bool bHeld = Weapon && Weapon->GetRootComponent() && Weapon->GetRootComponent()->GetAttachParent() == Body;
	const bool bActive = bHeld && bNear && Body->WasRecentlyRendered(0.25f) && !Instance->GetActionAnimation() && CVarGripContact.GetValueOnGameThread() != 0;
	UStaticMeshComponent* Gun = bActive && Weapon->GetWeaponMesh()->DoesSocketExist(TEXT("SupportHand")) && CVarSupportHand.GetValueOnGameThread() != 0
		? Weapon->GetWeaponMesh() : nullptr;

	TPSupportWeight = FMath::FInterpConstantTo(TPSupportWeight, Gun ? 1.f : 0.f, DeltaSeconds, 1.f / 0.2f);
	TArray<UPrimitiveComponent*, TInlineAllocator<2>> Parts;
	if (TPGripTimer < 0.f)
	{
		TPGripTimer = FMath::FRand() * 0.1f; // stagger bodies across frames
	}
	TPGripTimer += DeltaSeconds;
	const bool bSolveNow = TPGripTimer >= 0.1f;
	const float SolveDelta = bSolveNow ? TPGripTimer : 0.f;
	if (bSolveNow)
	{
		TPGripTimer = 0.f;
	}
	if (bActive && bSolveNow)
	{
		GripParts(Weapon, Parts);
	}
	if (Gun)
	{
		FVector Palm, Fingers, Normal;
		ComputeSupportPalm(Gun, Body, Palm, Fingers, Normal);
		if (!bTPLeftGripInit)
		{
			FMemory::Memzero(TPLeftGrip, sizeof(TPLeftGrip));
			bTPLeftGripInit = true;
		}
		if (Parts.Num() > 0)
		{
			SolveGripContact(Body, Parts, true, TPLeftGrip, -15.f, LeftMaxCurls(), SolveDelta);
		}
		const FTransform& ToBody = Body->GetComponentTransform();
		Instance->SetSupportHand(TPSupportWeight, ToBody.InverseTransformPosition(Palm), ToBody.InverseTransformVectorNoScale(Fingers),
			ToBody.InverseTransformVectorNoScale(Normal), CVarFingerCurl.GetValueOnGameThread());
		Instance->SetLeftFingerCurls(TPLeftGrip);
	}
	else if (TPSupportWeight > 0.f)
	{
		Instance->SetSupportHand(TPSupportWeight, Instance->GetSupportTarget(), Instance->GetSupportFingers(), Instance->GetSupportPalmNormal(),
			Instance->GetSupportCurl());
	}
	else
	{
		Instance->SetSupportHand(0.f, FVector::ZeroVector, FVector::ForwardVector, FVector::UpVector, 0.f);
		Instance->SetLeftFingerCurls(nullptr);
		bTPLeftGripInit = false;
	}

	if (bActive && Parts.Num() > 0)
	{
		SolveGripContact(Body, Parts, false, TPRightGrip, -25.f, RightMaxCurls(), SolveDelta);
	}
	TPRightGripWeight = FMath::FInterpConstantTo(TPRightGripWeight, bActive ? 1.f : 0.f, DeltaSeconds, 1.f / 0.2f);
	if (TPRightGripWeight <= 0.f)
	{
		FMemory::Memzero(TPRightGrip, sizeof(TPRightGrip));
	}
	if (bActive && Weapon->GetWeaponMesh()->DoesSocketExist(TEXT("Trigger")))
	{
		const FTransform Hand = Body->GetSocketTransform(TEXT("hand_r"), RTS_World);
		Instance->SetTriggerFinger(TPRightGripWeight, Hand.InverseTransformPosition(TriggerContact(Weapon->GetWeaponMesh())),
			Hand.InverseTransformVectorNoScale(Weapon->GetWeaponMesh()->GetComponentTransform().GetUnitAxis(EAxis::Y)));
	}
	else
	{
		Instance->SetTriggerFinger(TPRightGripWeight, Instance->GetTriggerTarget());
	}
	Instance->SetRightFingerCurls(TPRightGrip, TPRightGripWeight);
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
		Instance->SetTriggerFinger(TriggerFingerWeight, Hand.InverseTransformPosition(ShieldbreakBodyAnimatorPrivate::TriggerContact(Gun)),
			Hand.InverseTransformVectorNoScale(Gun->GetComponentTransform().GetUnitAxis(EAxis::Y)));
	}
	else
	{
		Instance->SetTriggerFinger(TriggerFingerWeight, Instance->GetTriggerTarget());
	}
}

float UShieldbreakBodyAnimator::GetTriggerFingerError() const
{
	UStaticMeshComponent* Gun = GetTriggerMesh();
	if (!Gun || !FirstPersonArms || FirstPersonArms->GetBoneIndex(TEXT("index_03_r")) == INDEX_NONE)
	{
		return -1.f;
	}
	const FVector Middle = FirstPersonArms->GetBoneLocation(TEXT("index_02_r"));
	const FVector Last = FirstPersonArms->GetBoneLocation(TEXT("index_03_r"));
	// Same pad point as the anim instance (1.6 cm past the last joint), in world space (the arms are unscaled).
	const FVector Pad = Last + (Last - Middle).GetSafeNormal() * 1.6f;
	return static_cast<float>(FVector::Dist(Pad, ShieldbreakBodyAnimatorPrivate::TriggerContact(Gun)));
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
	TAutoConsoleVariable<float> CVarSprintPitch(TEXT("sb.FPSprintPitch"), -14.f, TEXT("Sprint carry: rifle muzzle pitch (degrees, down negative)."));
	TAutoConsoleVariable<float> CVarSprintYaw(TEXT("sb.FPSprintYaw"), -24.f, TEXT("Sprint carry: rifle muzzle yaw (degrees, negative = in across the body)."));
	TAutoConsoleVariable<float> CVarSprintRoll(TEXT("sb.FPSprintRoll"), -26.f, TEXT("Sprint carry: rifle cant (degrees, negative = top toward the screen centre)."));
	TAutoConsoleVariable<float> CVarSprintPistolPitch(TEXT("sb.FPSprintPistolPitch"), -24.f, TEXT("Sprint carry: sidearm muzzle pitch (degrees)."));
	TAutoConsoleVariable<float> CVarSprintPump(TEXT("sb.FPSprintPump"), 1.f, TEXT("Sprint arm-swing pump on the gun (0 off)."));

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
	// Sprint carry: the gun swings down and in across the body, canted with its top toward the screen centre, the
	// sidearm held low and pointed down. The stride pump on top is added with the walk bob below.
	const bool bPistolCarry = GetViewmodelStyle() == EShieldbreakViewmodelStyle::Pistol;
	TargetRotation += (bPistolCarry ? FVector(CVarSprintPistolPitch.GetValueOnGameThread(), -10.f, -12.f)
		: FVector(CVarSprintPitch.GetValueOnGameThread(), CVarSprintYaw.GetValueOnGameThread(), CVarSprintRoll.GetValueOnGameThread())) * SprintBlend;
	TargetOffset += (bPistolCarry ? FVector(-3.f, -2.f, -6.f) : FVector(-4.f, -3.f, -5.f)) * SprintBlend;

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
	FVector Bob(0.f, 0.5f * FMath::Sin(BobPhase), -0.6f * 0.5f * (1.f - FMath::Cos(2.f * BobPhase)));
	float BobRoll = 0.6f * FMath::Sin(BobPhase);
	// Sprinting: a bigger, arm-swing pump (one side-to-side swing per stride, a dip on each footfall).
	const float Pump = SprintBlend * FMath::Max(0.f, CVarSprintPump.GetValueOnGameThread());
	Bob += FVector(0.4f * FMath::Cos(2.f * BobPhase), 1.3f * FMath::Sin(BobPhase), -1.6f * 0.5f * (1.f - FMath::Cos(2.f * BobPhase))) * Pump;
	BobRoll += 3.f * FMath::Sin(BobPhase) * Pump;
	const FVector SprintSwing = FVector(1.2f * FMath::Sin(2.f * BobPhase), 2.f * FMath::Sin(BobPhase), 0.f) * Pump;

	// Scoped aim keeps the gun steady; sb.FPMotion scales everything.
	MotionScale = Follow(MotionScale, OwnerCharacter->IsZoomed() ? 0.3f : 1.f, 12.f, Dt);
	const float Scale = MotionScale * FMath::Max(0.f, CVarFPMotion.GetValueOnGameThread());
	const FVector Offset = (MotionOffset + Bob * BobScale) * Scale;
	const FVector Rotation = (MotionRotation + FVector(0.f, 0.f, BobRoll * BobScale) + SprintSwing * BobScale) * Scale;
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
	// Compensate for the whole fitting offset, keeping the gun's framing stable
	// while the hand moves to a grip that clears the receiver.
	const FVector LiftedSocket = SocketComponent.GetLocation() + SocketComponent.TransformVectorNoScale(CalibratedHandOffset);
	const FVector Location = GripCamera - Rotation.RotateVector(LiftedSocket);
	CalibratedArmsTransform = FTransform(Rotation, Location);
	FirstPersonArms->SetRelativeTransform(CalibratedArmsTransform);
	UE_LOG(LogShieldbreak, Log, TEXT("First-person arms placed for %s: offset %s, rotation %s."),
		*GetNameSafe(CalibratedIdle), *Location.ToCompactString(), *Rotation.Rotator().ToCompactString());
	// Where the throwing arm starts from in camera space (the grenade throw's hand path is laid out around it).
	if (FirstPersonArms->GetBoneIndex(TEXT("upperarm_l")) != INDEX_NONE)
	{
		const FVector Shoulder = CalibratedArmsTransform.TransformPosition(
			FirstPersonArms->GetBoneTransform(FirstPersonArms->GetBoneIndex(TEXT("upperarm_l")), FTransform::Identity).GetLocation());
		UE_LOG(LogShieldbreak, Log, TEXT("First-person left shoulder in camera space: %s"), *Shoulder.ToCompactString());
	}
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

// ---- Grenade throw ----

namespace ShieldbreakBodyAnimatorPrivate
{
	/** One key of the first-person throw: seconds (as a fraction of the release delay R before release, or seconds
	 *  after it), where the left hand is in camera space (cm: forward, right, up), where the elbow points (camera
	 *  space, relative to the hand) and how far the wrist is flexed (degrees, + = snapped forward). */
	struct FThrowKey { float Time; FVector Hand; FVector Elbow; float Wrist; };

	/** The throw, after Halo's: the left hand comes up from below the view holding the grenade, cocks beside the
	 *  head, snaps forward past the crosshair (release), follows through down and across, and drops out of view while
	 *  the gun comes back up. R is the release delay; times past it are absolute offsets after release. */
	void GetThrowKeys(float R, TArray<FThrowKey, TInlineAllocator<6>>& Keys)
	{
		Keys.Reset();
		// Around a shoulder at sb.FPThrowShoulder (about 20 cm left of and below the eye, a little behind it).
		Keys.Add({ 0.f, FVector(10.f, -30.f, -52.f), FVector(-12.f, -22.f, -18.f), -10.f });
		// Cocked inside the view's left third (63 degrees off centre was out of frame, G6; 22 cm put the camera inside the hand, G7).
		Keys.Add({ R * 0.55f, FVector(36.f, -19.f, -4.f), FVector(-14.f, -20.f, 4.f), -40.f });
		Keys.Add({ R, FVector(40.f, -9.f, -7.f), FVector(-22.f, -16.f, -18.f), 15.f });
		Keys.Add({ R + 0.1f, FVector(34.f, 1.f, -25.f), FVector(-20.f, -12.f, -22.f), 35.f });
		Keys.Add({ R + 0.24f, FVector(14.f, -8.f, -58.f), FVector(-10.f, -20.f, -18.f), 20.f });
	}

	FVector CatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float T)
	{
		const float T2 = T * T, T3 = T2 * T;
		return 0.5f * ((2.f * P1) + (-P0 + P2) * T + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2 + (-P0 + 3.f * P1 - 3.f * P2 + P3) * T3);
	}

	/** Gun dip 0-1 at Time: down fast, held through the release, back up with a small overshoot. */
	float ThrowDip(float Time, float R)
	{
		const float In = 0.09f, HoldUntil = R + 0.1f, Out = 0.3f;
		if (Time < 0.f) return 0.f;
		if (Time < In) return FMath::InterpEaseOut(0.f, 1.f, Time / In, 2.f);
		if (Time < HoldUntil) return 1.f;
		const float U = (Time - HoldUntil) / Out;
		if (U >= 1.f) return 0.f;
		return FMath::SmoothStep(1.f, 0.f, U) - 0.12f * FMath::Sin(UE_PI * U) * U;
	}
}

float UShieldbreakBodyAnimator::GetFirstPersonThrowTime() const
{
	const UWorld* World = GetWorld();
	const float T = World ? World->GetTimeSeconds() - FirstPersonThrowStart : -1.f;
	return T >= 0.f && T < AShieldbreakGrenade::GetThrowDuration() ? T : -1.f;
}

UStaticMeshComponent* UShieldbreakBodyAnimator::GetVisibleHeldGrenade(bool bFirstPerson) const
{
	UStaticMeshComponent* Grenade = bFirstPerson ? FirstPersonHeldGrenade.Get() : BodyHeldGrenade.Get();
	return Grenade && Grenade->IsVisible() ? Grenade : nullptr;
}

float UShieldbreakBodyAnimator::UpdateFirstPersonThrow(UShieldbreakBodyAnimInstance* Instance, float Now)
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	const float R = AShieldbreakGrenade::GetReleaseDelay();
	const float Freeze = CVarThrowFreeze.GetValueOnGameThread();
	const float Elapsed = Now - FirstPersonThrowStart;
	// A capture can hold the pose (screenshots land a frame or two after their label, too late for a 0.15 s wind-up).
	const float T = Freeze >= 0.f && Elapsed >= 0.f && Elapsed < 5.f ? Freeze : Elapsed;
	TArray<FThrowKey, TInlineAllocator<6>> Keys;
	GetThrowKeys(R, Keys);
	const float ArmEnd = Keys.Last().Time;
	const bool bArm = T >= 0.f && T < ArmEnd;

	// The left arm shows while it throws and, on rifles, while it holds the gun's fore-grip (UpdateFirstPersonArms
	// owns that steady state); after a throw it goes back to what WantsFirstPersonSupportArm says.
	const bool bSupport = WantsFirstPersonSupportArm();
	const bool bShowArm = bArm || bSupport;
	if (bShowArm != bFirstPersonLeftArmShown || bArm != bFirstPersonThrowArm)
	{
		bFirstPersonLeftArmShown = bShowArm;
		bFirstPersonThrowArm = bArm;
		if (bShowArm) { FirstPersonArms->UnHideBoneByName(TEXT("upperarm_l")); }
		else { FirstPersonArms->HideBoneByName(TEXT("upperarm_l"), EPhysBodyOp::PBO_None); }
		for (UStaticMeshComponent* Piece : FirstPersonLeftArmor)
		{
			Piece->SetVisibility(bShowArm && FirstPersonArms->IsVisible());
		}
	}
	UpdateHeldGrenade(FirstPersonArms, FirstPersonHeldGrenade, bArm && T < R, true);

	// Letting go: the gun takes a small jolt as the body's weight goes into the throw.
	if (T >= R && !bFirstPersonThrowKicked && T < ArmEnd)
	{
		bFirstPersonThrowKicked = true;
		AddFirstPersonKick(0.5f);
		FShieldbreakSFX::PlayAt(this, EShieldbreakSound::GrenadeThrow, FirstPersonArms->GetBoneLocation(TEXT("hand_l")));
	}

	if (!bArm)
	{
		Instance->SetLeftArm(0.f, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, 0.f, 0.f, nullptr, 0.f);
		LastLeftHandError = -1.f;
		return ThrowDip(T, R);
	}

	int32 Index = 0;
	while (Index + 2 < Keys.Num() && T >= Keys[Index + 1].Time)
	{
		++Index;
	}
	const FThrowKey& A = Keys[Index];
	const FThrowKey& B = Keys[Index + 1];
	const FThrowKey& Before = Keys[FMath::Max(0, Index - 1)];
	const FThrowKey& After = Keys[FMath::Min(Keys.Num() - 1, Index + 2)];
	const float Alpha = FMath::Clamp((T - A.Time) / FMath::Max(0.001f, B.Time - A.Time), 0.f, 1.f);
	const float Reach = CVarThrowReach.GetValueOnGameThread();
	auto Scaled = [Reach](const FVector& V) { return FVector(V.X * Reach, V.Y, V.Z); };
	const FVector HandCamera = CatmullRom(Scaled(Before.Hand), Scaled(A.Hand), Scaled(B.Hand), Scaled(After.Hand), Alpha);
	const FVector ElbowCamera = HandCamera + FMath::Lerp(A.Elbow, B.Elbow, FMath::SmoothStep(0.f, 1.f, Alpha));
	const float Wrist = FMath::Lerp(A.Wrist, B.Wrist, FMath::SmoothStep(0.f, 1.f, Alpha)) * CVarThrowWrist.GetValueOnGameThread();

	// Camera space to the arms' component space (the arms hang off the camera with their current relative transform).
	const FTransform ArmsToCamera = FirstPersonArms->GetRelativeTransform();
	const FVector HandTarget = ArmsToCamera.InverseTransformPosition(HandCamera);
	const FVector ElbowTarget = ArmsToCamera.InverseTransformPosition(ElbowCamera);
	// Eased in over the first frames so the arm doesn't pop from its hidden idle pose.
	const float Weight = FMath::Min(1.f, T / 0.03f) * FMath::Min(1.f, (ArmEnd - T) / 0.04f);
	const float GripTime = CVarGripTime.GetValueOnGameThread();
	UAnimSequenceBase* Grip = GripTime >= 0.f ? GetActionAnimation(EShieldbreakAction::GrenadeThrow) : nullptr;
	const FVector Shoulder = ArmsToCamera.InverseTransformPosition(FVector(CVarShoulderX.GetValueOnGameThread(),
		CVarShoulderY.GetValueOnGameThread(), CVarShoulderZ.GetValueOnGameThread()));
	// The roll eases out after the release, so the follow-through ends palm down.
	Instance->SetLeftHandRoll(CVarHandRoll.GetValueOnGameThread() * (T < R ? 1.f : FMath::Max(0.f, 1.f - (T - R) / 0.12f)));
	Instance->SetLeftArm(Weight, Shoulder, HandTarget, ElbowTarget, Wrist, CVarThrowCurl.GetValueOnGameThread(), Grip, GripTime);
	// How close last frame's evaluated hand got to last frame's target (reach limits show up here).
	if (!LastLeftHandTarget.IsZero())
	{
		LastLeftHandError = static_cast<float>(FVector::Dist(FirstPersonArms->GetBoneTransform(FirstPersonArms->GetBoneIndex(TEXT("hand_l")), FTransform::Identity).GetLocation(), LastLeftHandTarget));
	}
	LastLeftHandTarget = HandTarget;
	return ThrowDip(T, R);
}

void UShieldbreakBodyAnimator::UpdateHeldGrenade(USkeletalMeshComponent* Mesh, TObjectPtr<UStaticMeshComponent>& Grenade, bool bVisible, bool bFirstPerson)
{
	if (!Mesh)
	{
		return;
	}
	if (!Grenade)
	{
		if (!bVisible)
		{
			return;
		}
		UStaticMesh* Model = AShieldbreakGrenade::GetGrenadeMesh();
		if (!Model)
		{
			return;
		}
		Grenade = NewObject<UStaticMeshComponent>(GetOwner(), bFirstPerson ? TEXT("FPHeldGrenade") : TEXT("HeldGrenade"));
		Grenade->SetStaticMesh(Model);
		Grenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Grenade->SetGenerateOverlapEvents(false);
		Grenade->SetCanEverAffectNavigation(false);
		Grenade->bAffectDistanceFieldLighting = false;
		Grenade->SetMobility(EComponentMobility::Movable);
		if (bFirstPerson)
		{
			Grenade->SetOnlyOwnerSee(true);
			Grenade->SetCastShadow(false);
			Grenade->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
			Grenade->LightingChannels.bChannel1 = true; // the first-person fill light
		}
		else
		{
			Grenade->SetOwnerNoSee(true);
			Grenade->SetCastShadow(false);
		}
		Grenade->SetupAttachment(Mesh);
		Grenade->RegisterComponent();
	}
	Grenade->SetVisibility(bVisible);
	if (!bVisible)
	{
		return;
	}
	// In the palm, under the middle knuckles so the curled fingers wrap it (G8: at 0.75 they closed short of it), off the palm face. The palm normal comes
	// from the knuckles (for a left hand, index-to-pinky across the knuckles crossed with the hand's length).
	const FVector Hand = Mesh->GetBoneLocation(TEXT("hand_l"));
	const FVector Middle = Mesh->GetBoneLocation(TEXT("middle_01_l"));
	const FVector Index = Mesh->GetBoneLocation(TEXT("index_01_l"));
	const FVector Pinky = Mesh->GetBoneLocation(TEXT("pinky_01_l"));
	const FVector Along = (Middle - Hand).GetSafeNormal();
	const FVector Palm = FVector::CrossProduct(Middle - Hand, Index - Pinky).GetSafeNormal();
	const float Off = ShieldbreakBodyAnimatorPrivate::CVarHeldGrenadePalm.GetValueOnGameThread();
	Grenade->SetWorldLocationAndRotation(Hand + (Middle - Hand) * 0.95f + Palm * Off, FRotationMatrix::MakeFromZX(-Along, Palm).ToQuat());
}
