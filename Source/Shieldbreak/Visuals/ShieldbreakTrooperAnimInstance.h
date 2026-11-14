// Native anim instance for the trooper character: retargets the mannequin body's animated pose onto the trooper's own
// skeleton every frame (IK Retargeter, "Retarget Pose From Mesh" from the parent component), so it keeps its own
// proportions instead of being squeezed onto the mannequin's bone positions by leader pose.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "ShieldbreakTrooperAnimInstance.generated.h"

class UIKRetargeter;

/** Proxy with a single root node: the retarget from the parent (mannequin) mesh component. */
USTRUCT()
struct FShieldbreakTrooperAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FShieldbreakTrooperAnimInstanceProxy() = default;
	explicit FShieldbreakTrooperAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual FAnimNode_Base* GetCustomRootNode() override { return &RetargetNode; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(&RetargetNode); }

	FAnimNode_RetargetPoseFromMesh RetargetNode;
};

UCLASS(Transient, NotBlueprintable)
class SHIELDBREAK_API UShieldbreakTrooperAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Mannequin -> trooper retargeter. Set before the instance initializes. */
	UPROPERTY(Transient)
	TObjectPtr<UIKRetargeter> Retargeter;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FShieldbreakTrooperAnimInstanceProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FShieldbreakTrooperAnimInstanceProxy*>(InProxy); }
};
