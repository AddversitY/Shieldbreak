#include "Visuals/ShieldbreakBodyAnimator.h"

#include "Shieldbreak.h"
#include "Animation/AnimSequenceBase.h"
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
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// The local player never sees their own body, but it still casts their shadow.
	Body->SetOwnerNoSee(true);
	Body->bCastHiddenShadow = true;
	bHasBodyArt = true;
	AttachArmor(Body);

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

	float PlayRate = 1.f;
	const EShieldbreakBodyPose Pose = ChoosePose(PlayRate);
	const float Now = World->GetTimeSeconds();

	if (Pose != CurrentPose && Now - LastPoseChangeTime >= MinPoseTime)
	{
		if (UAnimSequenceBase* Animation = GetPoseAnimation(Pose))
		{
			const bool bLoop = Pose != EShieldbreakBodyPose::Apex;
			Body->PlayAnimation(Animation, bLoop);
			CurrentPose = Pose;
			LastPoseChangeTime = Now;
		}
	}
	Body->SetPlayRate(PlayRate);
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

void UShieldbreakBodyAnimator::AttachArmor(USkeletalMeshComponent* Body)
{
	using namespace ShieldbreakBodyAnimatorPrivate;
	const USkeletalMesh* Asset = Body ? Body->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || ArmorPieces.Num() > 0 || CVarArmor.GetValueOnGameThread() == 0
		|| (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer))
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
	for (const TCHAR* Bone : ArmorBones)
	{
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
		const FString Name = FString(TEXT("SM_SB_Armor_")) + Bone;
		const FString Path = FString(TEXT("/Game/Shieldbreak/Art/Armor/")) + Name + TEXT(".") + Name;
		UStaticMesh* Mesh = BoneIndex != INDEX_NONE ? ShieldbreakArt::Load<UStaticMesh>(*Path) : nullptr;
		if (!Mesh)
		{
			continue;
		}

		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(GetOwner(), *(FString(TEXT("Armor_")) + Bone));
		Piece->SetStaticMesh(Mesh);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetGenerateOverlapEvents(false);
		Piece->SetCanEverAffectNavigation(false);
		Piece->SetOwnerNoSee(true);
		Piece->bCastHiddenShadow = true;
		Piece->SetCastShadow(CVarArmorShadows.GetValueOnGameThread() != 0);
		Piece->SetupAttachment(Body, Bone);
		// The piece is modeled where it sits on the reference pose, so undo the bone's reference transform.
		Piece->SetRelativeTransform(RefPoseComponentSpace(RefSkeleton, BoneIndex).Inverse());
		Piece->RegisterComponent();
		ArmorPieces.Add(Piece);

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
	if (ArmorPieces.Num() > 0)
	{
		UE_LOG(LogShieldbreak, Log, TEXT("%s: %d armor pieces"), *GetOwner()->GetName(), ArmorPieces.Num());
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
