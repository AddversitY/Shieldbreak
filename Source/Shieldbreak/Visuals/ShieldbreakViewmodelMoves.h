// Hand-keyed first-person moves: the reload, the draw after a weapon swap and the melee swing, as seen by the local
// player. Epic's free motion capture (Lyra, the UE templates) is all third-person, and played on the camera rig it
// swung shoulder plates across the screen and the hands away from the gun. These are original first-person moves
// authored as keyframes in camera space instead, the way shooters animate their viewmodels: the gun turns about the
// gun hand, and the left hand (its palm, since the support-hand IK places the palm) travels between points on the gun (the forend, the magazine, the charging handle) and
// points out of view, carrying the magazine with it. The third-person body keeps Lyra's clips for everyone else.
//
// Keys are eased with Hermite splines (tangents from the neighbouring keys), so a move flows through its poses
// instead of stopping at each one. sb.FPMoves 0 turns the system off (back to the Lyra clips on the arms).

#pragma once

#include "CoreMinimal.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;

enum class EShieldbreakViewmodelMove : uint8
{
	None,
	Reload,
	/** The new gun coming up after a swap. */
	Draw,
	Melee
};

/** Which set of keys a gun uses. */
enum class EShieldbreakViewmodelStyle : uint8
{
	/** Battle rifle: tilt, magazine out and in, palm the base, rack the charging handle. */
	Rifle,
	/** Sniper: the same with a heavier, slower hold and no charging handle. */
	Marksman,
	/** Sidearm: the empty magazine drops out, the left hand brings a fresh one up into the grip. */
	Pistol,
	/** Rocket launcher: the muzzle comes up and the left hand loads the breech. */
	Launcher
};

/** What the moves want this frame. Offset and Rotation are camera space (cm forward/right/up; degrees pitch, yaw,
 *  roll) and turn the arms about the gun hand. */
struct FShieldbreakViewmodelPose
{
	FVector Offset = FVector::ZeroVector;
	FVector Rotation = FVector::ZeroVector;
	/** 0-1: how much the left hand follows HandWorld instead of the gun's SupportHand socket. */
	float HandWeight = 0.f;
	FVector HandWorld = FVector::ZeroVector;
	/** The left arm should be drawn (the sidearm hides it outside its reload). */
	bool bWantsLeftArm = false;

	bool IsIdentity() const { return Offset.IsNearlyZero(0.01) && Rotation.IsNearlyZero(0.01) && HandWeight <= 0.f; }
};

/** What a move needs from the scene each frame. */
struct FShieldbreakViewmodelContext
{
	/** The first-person camera's world transform. */
	FTransform Camera = FTransform::Identity;
	/** The held gun's mesh (sockets: SupportHand, Magazine), or null. */
	const UStaticMeshComponent* Gun = nullptr;
	/** The gun's separate magazine (attached to Gun's Magazine socket), or null when the gun has none. */
	UStaticMeshComponent* Magazine = nullptr;
	/** The first-person arms, for where the left hand (hand_l) actually is. */
	const USkeletalMeshComponent* Arms = nullptr;
	/** The forend hold's palm point in gun space (the Support anchor), when the gun has one. */
	FVector SupportPalmLocal = FVector::ZeroVector;
	bool bHasSupportPalm = false;
	/** Still reloading (gameplay); a reload move that the game cancelled eases out. */
	bool bReloading = true;
};

class SHIELDBREAK_API FShieldbreakViewmodelMoves
{
public:
	static bool IsEnabled();

	/** Starts Move (replacing any move; the old pose eases into the new one). Duration <= 0 uses the move's own. */
	void Start(EShieldbreakViewmodelMove Move, EShieldbreakViewmodelStyle Style, float Duration, double Now,
		UStaticMeshComponent* Magazine);
	/** Eases the current move out (swap, death, a cancelled reload) and puts the magazine back. */
	void Stop(double Now, UStaticMeshComponent* Magazine);

	/** Steps the move and writes this frame's pose; also moves the magazine. */
	void Update(double Now, const FShieldbreakViewmodelContext& Context, FShieldbreakViewmodelPose& OutPose);
	/** Re-evaluate world anchors after the current gun pose has been applied. */
	void RefreshHandTarget(double Now, const FShieldbreakViewmodelContext& Context, FShieldbreakViewmodelPose& OutPose) const;
	/** Carry from the evaluated palm, after this frame's arm IK and attached gun have updated. */
	void UpdateMagazineAfterPose(double Now, const FShieldbreakViewmodelContext& Context);

	EShieldbreakViewmodelMove GetMove() const { return Move; }
	bool IsPlaying() const { return Move != EShieldbreakViewmodelMove::None; }
	/** 0-1 through the current move (or -1 when none). */
	float GetProgress(double Now) const;
	/** True while the magazine is out of the gun (in the hand, falling, or out of view). */
	bool IsMagazineOut() const { return MagazineState != EMagazineState::Seated; }

	/** Self-test: the current move's gun pose at normalized time T, without stepping anything. */
	FShieldbreakViewmodelPose SampleGunPose(EShieldbreakViewmodelMove InMove, EShieldbreakViewmodelStyle InStyle, float T) const;

	/** Puts a magazine back in its socket with no offset and makes it visible. */
	static void SeatMagazine(UStaticMeshComponent* Magazine);

private:
	enum class EMagazineState : uint8 { Seated, InHand, Falling, Hidden };

	EShieldbreakViewmodelMove Move = EShieldbreakViewmodelMove::None;
	EShieldbreakViewmodelStyle Style = EShieldbreakViewmodelStyle::Rifle;
	double StartTime = 0.0;
	float Duration = 1.f;
	/** The pose when the move started (or stopped), faded out over BlendTime so moves never pop. */
	FShieldbreakViewmodelPose FromPose;
	double BlendStart = -100.0;
	float BlendTime = 0.12f;
	/** Last written pose. */
	FShieldbreakViewmodelPose LastPose;
	bool bStopping = false;

	EMagazineState MagazineState = EMagazineState::Seated;
	/** Falling magazine (sidearm): where it left the gun, in camera space, and when. */
	FTransform FallStart = FTransform::Identity;
	double FallTime = 0.0;
	/** Hand-to-magazine offset (hand_l space) taken when the hand closed on it. */
	FTransform HandToMagazine = FTransform::Identity;
	bool bHasHandToMagazine = false;
	bool bCaptureGripAfterPose = false;
};
