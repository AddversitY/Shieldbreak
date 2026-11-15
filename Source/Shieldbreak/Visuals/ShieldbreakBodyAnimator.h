// Puts the imported mannequin on the owning ACharacter's mesh and drives it without an Animation Blueprint asset:
// it picks a locomotion sequence from the movement state and hands it to UShieldbreakBodyAnimInstance, which
// cross-fades between sequences and bends the spine to the aim pitch. Death switches the
// mesh to a ragdoll. If the art is missing, nothing changes and the greybox body stays.
// It also dresses the mannequin in Shieldbreak's armor: one static mesh per bone (SM_SB_Armor_<bone>, modeled in the
// mannequin's mesh space by Art/blender/build_armor.py), attached so it follows the animation and the ragdoll.
// For the local player it also builds the first-person arms: a second mannequin on the camera with the head and legs
// hidden, wearing the arm armor and holding the gun, so throws, melee, reloads and weapon swaps show on screen. The
// same one-shot actions play over the third-person body's upper half for everyone else.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Visuals/ShieldbreakViewmodelMoves.h"
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

/** One-shot upper-body actions (Lyra's Actions animations). */
enum class EShieldbreakAction : uint8
{
	GrenadeThrow,
	Melee,
	Reload,
	Equip
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

	/** Plays Action on the body and (for the local player) the first-person arms. Duration > 0 stretches it to fit. */
	void PlayAction(EShieldbreakAction Action, float Duration = 0.f);
	/** The action animation playing on the body now, or null. */
	UAnimSequenceBase* GetPlayingAction() const;
	/** The local player's first-person arms (null for everyone else, or before the art loads). */
	USkeletalMeshComponent* GetFirstPersonArms() const;
	/** True when the arms are on screen and the equipped gun should go in their hand. */
	bool HasFirstPersonArms() const;
	/** First person: seconds into the current grenade throw, or -1 when not throwing. */
	float GetFirstPersonThrowTime() const;
	/** The grenade shown in the first-person (or third-person) hand, when visible; null otherwise. */
	UStaticMeshComponent* GetVisibleHeldGrenade(bool bFirstPerson) const;
	/** First person: cm between the left hand and where the throw wants it (last evaluated pose), or -1. */
	float GetLeftHandError() const { return LastLeftHandError; }
	/** First person: cm between the index finger's pad and the trigger's front contact point (last evaluated pose), or -1
	 *  when there are no arms or the gun has no trigger socket. */
	float GetTriggerFingerError() const;
	/** Trigger socket, finger pad and index_01_r in hand_r space (cm), for fitting the trigger art. */
	FString DescribeTriggerFinger() const;
	/** True when the held gun has a "SupportHand" socket (the rifles): first person then shows the left arm, reaching
	 *  the hand onto that socket, and frames the gun so the trigger hand sits below the screen. */
	bool WantsFirstPersonSupportArm() const;
	/** Distance (cm) from hand_l to the support socket, or -1 without one (self-test). */
	float GetSupportHandError() const;
	/** Hand-through-gun check: how many finger and palm segments of the visible hands cross the held gun's surface
	 *  (first-person arms or the third-person body). -1 when no gun is held or its surface can't be tested. */
	int32 MeasureHandClipping(bool bFirstPerson, FString* OutDetail = nullptr, bool bIncludeMagazine = true) const;
	/** Trigger-hand hold check: cm between the held gun's pistol-grip centre and where the right palm holds it (palm
	 *  centre + palm normal x (grip half width + palm gap)). -1 when it can't be measured; 100 when the grip is on the
	 *  back of the hand. */
	float MeasureTriggerHandHold(bool bFirstPerson, FString* OutDetail = nullptr) const;
	/** Socket-based trigger-hand fit: measures the gun's pistol grip (traced below its Trigger socket) and the
	 *  hand's palm (hand_r and the knuckles), and moves the gun in weapon_r so the grip sits in the palm with the
	 *  knuckles clear of the receiver. Measured once per hand mesh and gun mesh, then cached. */
	void FitTriggerHand(USkeletalMeshComponent* Mesh);
	/** First person: the hand-keyed reload / draw / melee (ShieldbreakViewmodelMoves) and this frame's pose. */
	const FShieldbreakViewmodelMoves& GetViewmodelMoves() const { return Moves; }
	const FShieldbreakViewmodelPose& GetViewmodelPose() const { return MovePose; }
	/** Which keys the held gun's moves use. */
	EShieldbreakViewmodelStyle GetViewmodelStyle() const;

	/** Local player's view: a shot kicks the first-person arms back and up, then they spring home. Strength is the
	 *  weapon's recoil (degrees), so heavier guns kick harder. */
	void AddFirstPersonKick(float Strength);
	/** Everyone else's view: a shot rocks the third-person upper body back for a moment. */
	void AddBodyKick(float Strength);
	/** The procedural first-person motion (walk bob, look sway, strafe lean, sprint pose, jump and landing, shot
	 *  kick) applied around the gun hand on top of the arms' placement. Identity when still. */
	const FTransform& GetFirstPersonMotion() const { return FirstPersonMotion; }

	/** Every machine, on death: ragdoll the body with an initial velocity change. */
	void StartRagdoll(const FVector& Impulse);

	/** True when at least one armor piece is on the body (the mannequin then wears a dark undersuit). */
	bool HasArmor() const { return ArmorPieces.Num() > 0 || SkinnedSuit != nullptr; }
	int32 GetArmorPieceCount() const { return ArmorPieces.Num(); }
	/** True when the third-person armor is the skinned suit (SK_SB_Soldier) rather than rigid pieces. */
	bool HasSkinnedSuit() const { return SkinnedSuit != nullptr; }
	/** Paints the armor's team plates and glow strips. */
	void SetArmorColor(const FLinearColor& TeamColor);
	/** Team-tinted armor material instances (slots named M_Team*), for the soldier shots' log. */
	int32 GetArmorTeamMaterialCount() const { return ArmorTeamMIDs.Num(); }
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
	/** Third person: every armor piece, hidden from the owner. First person: the arm pieces, seen only by the owner. */
	void AttachArmor(USkeletalMeshComponent* Mesh, bool bFirstPerson);
	void UpdateFirstPersonArms();
	void SetFirstPersonArmsVisible(bool bVisible);
	void UpdateScopeHide(class AShieldbreakCharacter* OwnerCharacter);
	void ClearScopeHide();
	/** Places the arms on the camera so the gun hand sits at the viewmodel spot with the barrel toward the crosshair. */
	void CalibrateFirstPersonArms();
	bool UsesPistolAnimations() const;
	UAnimSequenceBase* GetFirstPersonIdle() const;
	UAnimSequenceBase* GetActionAnimation(EShieldbreakAction Action) const;
	EShieldbreakBodyPose ChoosePose(float& OutPlayRate) const;
	UAnimSequenceBase* GetPoseAnimation(EShieldbreakBodyPose Pose) const;

	EShieldbreakBodyPose CurrentPose = EShieldbreakBodyPose::None;
	float LastPoseChangeTime = -100.f;
	bool bHasBodyArt = false;
	bool bRagdoll = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ArmorPieces;
	/** Third-person suit skinned to the mannequin skeleton, following the body by leader pose (sb.SkinnedSuit). */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> SkinnedSuit;
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> FirstPersonArmor;
	/** The left arm's plates in first person, shown only while it throws a grenade. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> FirstPersonLeftArmor;
	/** The idle the arms were last placed for, and the settings used (re-placed when either changes). */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> CalibratedIdle;
	FVector3f CalibratedGrip = FVector3f::ZeroVector;
	FVector2f CalibratedAim = FVector2f::ZeroVector;
	FVector CalibratedHandOffset = FVector::ZeroVector;
	float FirstPersonIdleSince = -1.f;
	FTransform CalibratedArmsTransform = FTransform::Identity;
	/** Grenade throw, first person: world time it started (the arm and gun follow UpdateFirstPersonThrow's timeline),
	 *  and the gun's current dip (0-1). */
	float FirstPersonThrowStart = -100.f;
	float AppliedThrowDip = 0.f;
	bool bFirstPersonLeftArmShown = false;
	bool bFirstPersonThrowArm = false;
	bool bFirstPersonThrowKicked = false;
	/** Grenade throw, third person: world time the toss started (the grenade shows in the hand until release). */
	float BodyThrowStart = -100.f;
	bool bBodyTossReleased = true;
	/** The grenade in the throwing hand until it is let go: first person (owner only) and third person (everyone else). */
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstPersonHeldGrenade;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> BodyHeldGrenade;
	/** First person: drives the left arm (IK), the held grenade and the gun's dip for the throw; returns the dip. */
	float UpdateFirstPersonThrow(class UShieldbreakBodyAnimInstance* Instance, float Now);
	/** Shows Grenade in Mesh's left palm (placed from the hand's own joints), or hides it. Creates it on first use. */
	void UpdateHeldGrenade(USkeletalMeshComponent* Mesh, TObjectPtr<UStaticMeshComponent>& Grenade, bool bVisible, bool bFirstPerson);
	/** Weapon swap: the arms drop out of view while the equip animation plays and until they are re-placed for the
	 *  new gun's idle (the equip pose and the old placement put a shoulder plate across the screen), then rise. */
	float FirstPersonEquipEnd = -1.f;
	float SwapLower = 0.f;
	float AppliedSwapLower = 0.f;
	FVector LastLeftHandTarget = FVector::ZeroVector;
	float LastLeftHandError = -1.f;
	/** How far the index finger is curled onto the trigger (eases out for actions and swaps). */
	float TriggerFingerWeight = 0.f;
	/** Finds the equipped gun's "Trigger" socket and hands the arms' anim instance the finger target. */
	void UpdateTriggerFinger(class UShieldbreakBodyAnimInstance* Instance, bool bHolding, float DeltaSeconds);
	/** The equipped gun's mesh with a "Trigger" socket, or null. */
	class UStaticMeshComponent* GetTriggerMesh() const;
	/** How far the left hand has reached onto the support socket (eases out for actions, swaps and throws). */
	float SupportHandWeight = 0.f;
	/** First-person moves: the current one, its pose this frame, and a draw waiting for the arms to be re-placed. */
	FShieldbreakViewmodelMoves Moves;
	FShieldbreakViewmodelPose MovePose;
	bool bDrawPending = false;
	/** Steps the moves (call after the arms have their transform for this frame, before the support hand). */
	void UpdateViewmodelMoves(bool bSwapping);
	class UStaticMeshComponent* GetHeldMagazine() const;
	/** The forend hold: palm point, fingers direction and palm normal (world) for a gun with a SupportHand socket. */
	void ComputeSupportPalm(class UStaticMeshComponent* Gun, const USkeletalMeshComponent* HandMesh, FVector& OutPalm, FVector& OutFingers, FVector& OutPalmNormal) const;
	struct FSupportPalmCache
	{
		TWeakObjectPtr<UStaticMeshComponent> Gun;
		FVector Palm = FVector::ZeroVector;
		FVector Fingers = FVector::ForwardVector;
		FVector Normal = FVector::UpVector;
		float UpdateAt = -1.f;
	};
	mutable FSupportPalmCache FirstPersonPalmCache;
	mutable FSupportPalmCache ThirdPersonPalmCache;
	/** Whether upperarm_l is currently shown on the arms (only with a support socket; the throw shows it too). */
	bool bSupportArmShown = false;
	void UpdateSupportHand(class UShieldbreakBodyAnimInstance* Instance, bool bHolding, float DeltaSeconds);
	void UpdateHeldMagazineAfterPose();
	class UStaticMeshComponent* GetSupportMesh(bool bRequireAttached) const;
	void ApplyFirstPersonArmsTransform(float Dip);
	/** Steps the first-person motion springs and writes FirstPersonMotion. */
	void UpdateFirstPersonMotion(float DeltaSeconds);
	/** Spring state, camera space: offset in cm (X forward, Y right, Z up), rotation in degrees (pitch, yaw, roll). */
	FVector MotionOffset = FVector::ZeroVector;
	FVector MotionOffsetVelocity = FVector::ZeroVector;
	FVector MotionRotation = FVector::ZeroVector;
	FVector MotionRotationVelocity = FVector::ZeroVector;
	float BobPhase = 0.f;
	float BobAmount = 0.f;
	float SprintBlend = 0.f;
	float MotionScale = 1.f;
	FRotator LastViewRotation = FRotator::ZeroRotator;
	bool bHasLastViewRotation = false;
	bool bWasFalling = false;
	float LastFallSpeed = 0.f;
	FTransform FirstPersonMotion = FTransform::Identity;
	/** Dynamic instances of the armor's team-colored slots (plates and glow strips). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ArmorTeamMIDs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ArmorGlowMIDs;
	bool bScopeHidden = false;
	bool bTriggerArmHidden = false;
	TWeakObjectPtr<AActor> ScopeHiddenWeapon;
	TWeakObjectPtr<class APlayerController> ScopeHidePC;
	/** Last frame's left-palm target (world), for GetSupportHandError. */
	FVector SupportPalmWorld = FVector::ZeroVector;
	/** Contact-solved grip curls (ShieldbreakGrip order, degrees): first-person arms and third-person body. */
	float FPLeftGrip[15] = {};
	float FPReloadGrip[15] = {};
	float LastReloadGripTarget[15] = {};
	bool bFPReloadGripInit = false;
	float FPRightGrip[15] = {};
	float TPLeftGrip[15] = {};
	float TPRightGrip[15] = {};
	bool bFPLeftGripInit = false;
	bool bTPLeftGripInit = false;
	float FPRightGripWeight = 0.f;
	float TPRightGripWeight = 0.f;
	float TPSupportWeight = 0.f;
	/** Third-person grip solves run ten times a second (staggered per body), not every frame: they cost traces. */
	float TPGripTimer = -1.f;
	void UpdateThirdPersonGrip(class UShieldbreakBodyAnimInstance* Instance, USkeletalMeshComponent* Body, float DeltaSeconds);
	void UpdateFirstPersonRightGrip(class UShieldbreakBodyAnimInstance* Instance, bool bHolding, float DeltaSeconds);
};
