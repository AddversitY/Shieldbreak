// Puts the imported mannequin on the owning ACharacter's mesh and drives it without an Animation Blueprint asset:
// it picks a locomotion sequence from the movement state and hands it to UShieldbreakBodyAnimInstance, which
// cross-fades between sequences and bends the spine to the aim pitch. Death switches the
// mesh to a ragdoll. If the art is missing, nothing changes and the greybox body stays.
// It also dresses the mannequin in Shieldbreak's armor: one static mesh per bone (SM_SB_Armor_<bone>, modeled in the
// mannequin's mesh space by Art/blender/build_armor.py), attached so it follows the animation and the ragdoll.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShieldbreakBodyAnimator.generated.h"

class UAnimSequenceBase;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;
class UStaticMeshComponent;

enum class EShieldbreakBodyPose : uint8
{
	None,
	Idle,
	WalkFwd, WalkBwd, WalkLeft, WalkRight,
	JogFwd, JogBwd, JogLeft, JogRight,
	CrouchIdle, CrouchFwd, CrouchBwd, CrouchLeft, CrouchRight,
	Rising, Apex, Falling
};

UCLASS(ClassGroup = (Shieldbreak), meta = (BlueprintSpawnableComponent))
class SHIELDBREAK_API UShieldbreakBodyAnimator : public UActorComponent
{
	GENERATED_BODY()

public:
	UShieldbreakBodyAnimator();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True once the mannequin mesh is on the character (the owner then hides its greybox). */
	bool HasBodyArt() const { return bHasBodyArt; }

	/** True when the body plays through UShieldbreakBodyAnimInstance (cross-fades and aim pitch) and has a pose. */
	bool IsBlendedAnimation() const;
	/** The aim pitch the body last bent to (degrees, up positive). */
	float GetAimPitch() const;

	/** Every machine, on death: ragdoll the body with an initial velocity change. */
	void StartRagdoll(const FVector& Impulse);

	/** True when at least one armor piece is on the body (the mannequin then wears a dark undersuit). */
	bool HasArmor() const { return ArmorPieces.Num() > 0; }
	int32 GetArmorPieceCount() const { return ArmorPieces.Num(); }
	/** Paints the armor's team plates and glow strips. */
	void SetArmorColor(const FLinearColor& TeamColor);
	/** Active Camo hides the armor along with the body. */
	void SetArmorHidden(bool bHidden);

	/** Below this 2D speed the body plays walk cycles; above it, jogs. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float JogThreshold = 380.f;

	/** Speeds the sequences were authored at; play rate scales with actual speed / these. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float WalkAnimSpeed = 220.f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float JogAnimSpeed = 480.f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	float CrouchAnimSpeed = 200.f;

	/** Minimum time between pose switches, so direction changes don't flicker. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float MinPoseTime = 0.12f;

protected:
	virtual void BeginPlay() override;

private:
	USkeletalMeshComponent* GetBodyMesh() const;
	void AttachArmor(USkeletalMeshComponent* Body);
	EShieldbreakBodyPose ChoosePose(float& OutPlayRate) const;
	UAnimSequenceBase* GetPoseAnimation(EShieldbreakBodyPose Pose) const;

	EShieldbreakBodyPose CurrentPose = EShieldbreakBodyPose::None;
	float LastPoseChangeTime = -100.f;
	bool bHasBodyArt = false;
	bool bRagdoll = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ArmorPieces;
	/** Dynamic instances of the armor's team-colored slots (plates and glow strips). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ArmorTeamMIDs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ArmorGlowMIDs;
};
