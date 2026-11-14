#include "Visuals/ShieldbreakTrooperAnimInstance.h"
#include "Retargeter/IKRetargeter.h"

void FShieldbreakTrooperAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	UShieldbreakTrooperAnimInstance* Trooper = Cast<UShieldbreakTrooperAnimInstance>(InAnimInstance);
	if (Trooper && !Trooper->Retargeter)
	{
		Trooper->Retargeter = LoadObject<UIKRetargeter>(nullptr, TEXT("/Game/Characters/Trooper/RTG_Trooper.RTG_Trooper"));
	}
	RetargetNode.IKRetargeterAsset = Trooper ? Trooper->Retargeter : nullptr;
	// The trooper is attached to the mannequin body, which animates (and ragdolls) first.
	RetargetNode.RetargetFrom = ERetargetSourceMode::ParentSkeletalMeshComponent;
	RetargetNode.bSuppressWarnings = true;
	FAnimInstanceProxy::Initialize(InAnimInstance);
}
