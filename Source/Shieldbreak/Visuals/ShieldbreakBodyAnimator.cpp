#include "Visuals/ShieldbreakBodyAnimator.h"

#include "Shieldbreak.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Visuals/ShieldbreakArt.h"

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
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	Body->bBlendPhysics = true;
	Body->AddImpulse(Impulse, NAME_None, true);
}
