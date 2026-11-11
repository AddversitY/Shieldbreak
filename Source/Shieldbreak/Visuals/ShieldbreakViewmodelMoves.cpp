#include "Visuals/ShieldbreakViewmodelMoves.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/ArrayView.h"
#include "HAL/IConsoleManager.h"

namespace ShieldbreakViewmodelPrivate
{
	TAutoConsoleVariable<int32> CVarFPMoves(TEXT("sb.FPMoves"), 1,
		TEXT("1: hand-keyed first-person reload, draw and melee; 0: the Lyra third-person clips on the arms."));
	TAutoConsoleVariable<float> CVarFPMovesRate(TEXT("sb.FPMovesRate"), 1.f,
		TEXT("Draw and melee speed multiplier (reloads always fit the weapon's reload time)."));

	TAutoConsoleVariable<float> CVarReloadLift(TEXT("sb.FPReloadLift"), 1.f,
		TEXT("How far reloads bring the gun up and in so the magazine work is on screen (0 = keys only)."));

	/** One gun pose: camera-space offset (cm) and rotation (pitch, yaw, roll degrees) about the gun hand. */
	struct FGunKey { float T; FVector Offset; FVector Rotation; };

	/** Where the left hand (hand_l, the wrist) is at a key. */
	enum class EAnchor : uint8
	{
		/** The gun's SupportHand socket (the forend hold) + Local in gun space. */
		Support,
		/** The magazine grip: the Magazine socket + the style's grip offset + Local, gun space. */
		Magazine,
		/** The gun's origin + Local, gun space (charging handle, breech). */
		Gun,
		/** Local in camera space (out of view). */
		Camera
	};
	struct FHandKey { float T; EAnchor Anchor; FVector Local; };

	enum class EMagEvent : uint8 { Grab, Drop, Fresh, Seat };
	struct FMagKey { float T; EMagEvent Event; };
	struct FHandShapeKey { float T; float Weight; FVector Wrist; float Close; };

	struct FMoveData
	{
		TArrayView<const FGunKey> Gun;
		TArrayView<const FHandKey> Hand;
		TArrayView<const FMagKey> Magazine;
		/** Seconds when the caller gives none (draw, melee). */
		float DefaultDuration = 0.5f;
		/** The left arm is drawn for the whole move (the sidearm's arm is hidden otherwise). */
		bool bShowsLeftArm = false;
	};

	struct FStyleData
	{
		/** Reloads: the gun comes up and in toward the centre for the whole reload (on top of the keys), so the
		 *  magazine well is on screen; desktop task (M1) showed every reload happening below the frame. */
		FVector ReloadLift;
		FVector ReloadLiftRotation;
		/** Magazine socket in gun space when the mesh has none (art not reimported yet). */
		FVector MagazineSocket;
		/** The palm (the support-hand IK target) relative to the Magazine socket when the hand holds the magazine, gun space. */
		FVector MagazineGrip;
		/** SupportHand socket fallback (the sidearm has none). */
		FVector SupportSocket;
	};

	// Gun space: +Y down the barrel, +Z up, -X the gun's left side (where the left hand works).
	const FStyleData Styles[] = {
		/* Rifle */    { FVector(11.f, -5.f, 9.f), FVector(2.f, 2.f, 0.f), FVector(0.f, 13.3f, -3.6f), FVector(6.f, 1.5f, -8.0f), FVector(-2.5f, 24.f, -7.5f) },
		/* Marksman */ { FVector(3.f, 1.5f, 5.5f), FVector(4.f, 3.f, 0.f), FVector(0.f, 14.5f, -3.6f), FVector(-3.2f, 2.0f, -6.5f), FVector(-2.5f, 30.f, -6.6f) },
		/* Pistol */   { FVector(5.f, -1.f, 9.f), FVector(4.f, 4.f, -6.f), FVector(0.f, -2.2f, -2.4f), FVector(-3.0f, -1.8f, -10.f), FVector(-3.f, -2.f, -12.f) },
		/* Launcher */ { FVector(0.f, -2.f, 4.f), FVector(0.f, 0.f, 0.f), FVector(0.f, -22.f, 6.5f), FVector(-8.f, 0.f, -2.f), FVector(-2.f, 20.f, -7.f) },
	};

	// ---- Reloads ----

	// Battle rifle, 2.0 s. Reach, close, strip and release; bring a fresh magazine
	// from below, align, seat with a short impulse, release and regain the forend.
	// The gun never holds still: it eases into the cant, yanks down with the strip, tips
	// further toward the returning hand to present the well, dips into the push, kicks on
	// the seat with a damped rebound, and settles back with a small overshoot.
	const FGunKey RifleReloadGun[] = {
		{ 0.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
		{ 0.08f, FVector(-0.4f, -0.8f, 0.4f), FVector(0.5f, -1.5f, 6.f) },
		{ 0.18f, FVector(-1.2f, -1.8f, 1.0f), FVector(2.f, -3.5f, 15.f) },
		{ 0.215f, FVector(-1.3f, -2.f, 1.3f), FVector(2.5f, -4.f, 17.f) },
		{ 0.27f, FVector(-1.5f, -2.1f, 0.f), FVector(-0.5f, -4.5f, 19.5f) },
		{ 0.33f, FVector(-1.3f, -2.f, 0.9f), FVector(1.5f, -4.f, 17.5f) },
		{ 0.42f, FVector(-1.1f, -2.2f, 0.7f), FVector(1.f, -4.5f, 19.f) },
		{ 0.52f, FVector(-1.2f, -2.2f, 1.0f), FVector(2.5f, -4.5f, 20.f) },
		{ 0.61f, FVector(-1.3f, -2.1f, 1.2f), FVector(3.f, -4.f, 19.f) },
		{ 0.645f, FVector(-1.3f, -2.f, 0.9f), FVector(2.f, -4.f, 18.5f) },
		{ 0.67f, FVector(-1.2f, -2.f, 2.3f), FVector(5.f, -4.f, 18.f) },
		{ 0.70f, FVector(-1.2f, -2.f, 1.0f), FVector(1.8f, -4.f, 17.f) },
		{ 0.73f, FVector(-1.2f, -2.f, 1.4f), FVector(2.6f, -4.f, 17.3f) },
		{ 0.80f, FVector(-1.0f, -1.5f, 1.1f), FVector(1.8f, -3.f, 13.f) },
		{ 0.90f, FVector(-0.4f, -0.6f, 0.4f), FVector(0.3f, -1.f, 4.5f) },
		{ 0.96f, FVector(0.05f, 0.1f, -0.1f), FVector(-0.3f, 0.2f, -0.8f) },
		{ 1.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
	};
	const FHandKey RifleReloadHand[] = {
		{ 0.00f, EAnchor::Support, FVector(0.f, 0.f, 0.f) },
		{ 0.08f, EAnchor::Support, FVector(-0.5f, -3.f, -2.f) },
		{ 0.18f, EAnchor::Magazine, FVector(0.f, 0.f, -0.8f) },
		{ 0.21f, EAnchor::Magazine, FVector(0.f, 0.f, 0.f) },
		{ 0.28f, EAnchor::Magazine, FVector(1.f, 0.f, -16.f) },
		{ 0.32f, EAnchor::Magazine, FVector(5.f, -1.f, -18.f) },
		{ 0.38f, EAnchor::Camera, FVector(30.f, -9.f, -30.f) },
		{ 0.43f, EAnchor::Camera, FVector(30.f, -9.f, -30.f) },
		// The fresh magazine comes up nose-forward and rocks back into the well.
		{ 0.53f, EAnchor::Magazine, FVector(4.f, 2.f, -13.f) },
		{ 0.61f, EAnchor::Magazine, FVector(0.5f, 1.f, -5.f) },
		{ 0.665f, EAnchor::Magazine, FVector(0.f, 0.f, 0.f) },
		{ 0.69f, EAnchor::Magazine, FVector(0.f, 0.f, 0.f) },
		// Heel of the hand follows through on the base, then slides forward under the
		// receiver to the forend instead of opening out toward the camera.
		{ 0.72f, EAnchor::Magazine, FVector(0.5f, 0.f, -1.5f) },
		{ 0.78f, EAnchor::Magazine, FVector(1.5f, 5.f, -4.f) },
		{ 0.86f, EAnchor::Support, FVector(1.f, -3.f, -3.f) },
		{ 0.95f, EAnchor::Support, FVector(-0.3f, -0.5f, -0.5f) },
		{ 1.00f, EAnchor::Support, FVector(0.f, 0.f, 0.f) },
	};
	const FMagKey RifleReloadMag[] = { { 0.21f, EMagEvent::Grab }, { 0.32f, EMagEvent::Drop },
		{ 0.43f, EMagEvent::Fresh }, { 0.665f, EMagEvent::Seat } };

	// Weight blends from the idle wrist into a side grasp, with the thumb above
	// the magazine. Closed fingers persist through the pull and seat; after the
	// seat they relax partway rather than splaying while the hand returns.
	const FHandShapeKey RifleHandShape[] = {
		{ 0.f, 0.f, FVector::ZeroVector, 0.f }, { 0.10f, 0.f, FVector::ZeroVector, 0.f },
		{ 0.17f, 1.f, FVector::ZeroVector, 0.15f }, { 0.205f, 1.f, FVector::ZeroVector, 1.f },
		{ 0.28f, 1.f, FVector(-3.f, 2.f, -7.f), 1.f },
		{ 0.32f, 1.f, FVector(-5.f, 3.f, -12.f), 1.f },
		{ 0.36f, 1.f, FVector(-5.f, 3.f, -12.f), 0.f },
		{ 0.40f, 1.f, FVector(0.f, 0.f, 8.f), 0.f },
		{ 0.43f, 1.f, FVector(0.f, 0.f, 8.f), 1.f },
		{ 0.53f, 1.f, FVector(-3.f, 0.f, 5.f), 1.f },
		{ 0.61f, 1.f, FVector::ZeroVector, 1.f }, { 0.685f, 1.f, FVector::ZeroVector, 1.f },
		{ 0.72f, 1.f, FVector::ZeroVector, 0.75f }, { 0.78f, 0.8f, FVector::ZeroVector, 0.45f },
		{ 0.86f, 0.4f, FVector::ZeroVector, 0.35f },
		{ 0.95f, 0.f, FVector::ZeroVector, 0.2f }, { 1.f, 0.f, FVector::ZeroVector, 0.f }
	};

	// Sniper, 2.6 s: a heavier gun, so a smaller cant and slower settles, and no charging handle (the hand returns
	// to the forend after palming the magazine).
	const FGunKey MarksmanReloadGun[] = {
		{ 0.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
		{ 0.12f, FVector(-1.5f, -2.0f, 0.8f), FVector(4.f, -5.f, 18.f) },
		{ 0.22f, FVector(-2.0f, -2.4f, 1.2f), FVector(6.f, -6.f, 24.f) },
		{ 0.27f, FVector(-2.0f, -2.4f, 0.5f), FVector(4.f, -6.f, 25.f) },
		{ 0.33f, FVector(-1.8f, -2.4f, 1.8f), FVector(7.f, -6.f, 26.f) },
		{ 0.50f, FVector(-2.2f, -2.6f, 1.3f), FVector(6.f, -5.f, 24.f) },
		{ 0.63f, FVector(-2.2f, -2.8f, 1.1f), FVector(5.f, -6.f, 25.f) },
		{ 0.70f, FVector(-2.0f, -2.8f, 2.2f), FVector(8.f, -6.f, 26.f) },
		{ 0.75f, FVector(-2.0f, -2.8f, 1.3f), FVector(6.f, -6.f, 25.f) },
		{ 0.79f, FVector(-2.0f, -2.8f, 1.9f), FVector(7.f, -6.f, 25.f) },
		{ 0.90f, FVector(-0.6f, -0.6f, 0.4f), FVector(1.f, -1.f, 5.f) },
		{ 1.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
	};
	const FHandKey MarksmanReloadHand[] = {
		{ 0.00f, EAnchor::Support, FVector(0.f, 0.f, 0.f) },
		{ 0.10f, EAnchor::Support, FVector(-0.5f, -3.f, -2.f) },
		{ 0.20f, EAnchor::Magazine, FVector(0.f, 0.f, -0.8f) },
		{ 0.23f, EAnchor::Magazine, FVector(0.f, 0.f, 0.f) },
		{ 0.29f, EAnchor::Magazine, FVector(0.f, 0.5f, -6.f) },
		{ 0.40f, EAnchor::Camera, FVector(22.f, -6.f, -40.f) },
		{ 0.52f, EAnchor::Camera, FVector(20.f, -4.f, -42.f) },
		{ 0.62f, EAnchor::Magazine, FVector(0.f, 0.5f, -9.f) },
		{ 0.70f, EAnchor::Magazine, FVector(0.f, 0.f, 0.f) },
		{ 0.74f, EAnchor::Magazine, FVector(-0.5f, 0.f, -5.f) },
		{ 0.77f, EAnchor::Magazine, FVector(0.f, 0.f, -8.f) },
		{ 0.80f, EAnchor::Magazine, FVector(0.f, 0.f, -6.f) },
		{ 0.90f, EAnchor::Support, FVector(-0.5f, -1.5f, -1.5f) },
		{ 1.00f, EAnchor::Support, FVector(0.f, 0.f, 0.f) },
	};
	const FMagKey MarksmanReloadMag[] = { { 0.23f, EMagEvent::Grab }, { 0.70f, EMagEvent::Seat } };

	// Sidearm, 1.4 s. The gun flicks and the empty magazine drops free; the left hand comes up from below with a
	// fresh one, seats it with a shove, and the gun snaps back level (the slide release jolt).
	const FGunKey PistolReloadGun[] = {
		{ 0.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
		{ 0.11f, FVector(-2.0f, -3.0f, 1.5f), FVector(10.f, -10.f, 18.f) },
		{ 0.17f, FVector(-2.0f, -3.2f, 2.4f), FVector(13.f, -10.f, 23.f) },
		{ 0.30f, FVector(-2.4f, -3.8f, 1.6f), FVector(9.f, -10.f, 20.f) },
		{ 0.50f, FVector(-2.5f, -4.0f, 1.2f), FVector(8.f, -10.f, 20.f) },
		{ 0.58f, FVector(-2.5f, -4.0f, 2.7f), FVector(13.f, -10.f, 23.f) },
		{ 0.65f, FVector(-2.5f, -4.0f, 1.6f), FVector(9.f, -10.f, 20.f) },
		{ 0.78f, FVector(-1.0f, -2.0f, 0.8f), FVector(3.f, -4.f, 7.f) },
		{ 0.83f, FVector(-0.6f, -1.0f, 1.6f), FVector(7.f, -3.f, 5.f) },
		{ 1.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
	};
	const FHandKey PistolReloadHand[] = {
		{ 0.00f, EAnchor::Camera, FVector(16.f, -14.f, -44.f) },
		{ 0.14f, EAnchor::Camera, FVector(16.f, -14.f, -44.f) },
		{ 0.32f, EAnchor::Camera, FVector(20.f, -8.f, -40.f) },
		{ 0.44f, EAnchor::Magazine, FVector(0.f, -0.5f, -7.f) },
		{ 0.58f, EAnchor::Magazine, FVector(0.f, 0.f, 0.f) },
		{ 0.65f, EAnchor::Magazine, FVector(-0.5f, 0.f, -3.f) },
		{ 0.82f, EAnchor::Camera, FVector(18.f, -12.f, -42.f) },
		{ 1.00f, EAnchor::Camera, FVector(16.f, -14.f, -44.f) },
	};
	const FMagKey PistolReloadMag[] = { { 0.16f, EMagEvent::Drop }, { 0.32f, EMagEvent::Fresh }, { 0.58f, EMagEvent::Seat } };

	// Rocket launcher, 3.0 s: the muzzle comes up, the left hand drops out of view for a rocket and pushes it into
	// the breech, then returns to the foregrip.
	const FGunKey LauncherReloadGun[] = {
		{ 0.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
		{ 0.13f, FVector(-3.0f, -2.0f, 3.0f), FVector(16.f, -4.f, -8.f) },
		{ 0.50f, FVector(-3.2f, -2.2f, 2.6f), FVector(15.f, -4.f, -8.f) },
		{ 0.62f, FVector(-2.0f, -2.0f, 4.0f), FVector(19.f, -4.f, -10.f) },
		{ 0.70f, FVector(-3.0f, -2.0f, 3.0f), FVector(15.f, -4.f, -8.f) },
		{ 0.86f, FVector(-1.0f, -1.0f, 1.0f), FVector(5.f, -1.f, -2.f) },
		{ 1.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
	};
	const FHandKey LauncherReloadHand[] = {
		{ 0.00f, EAnchor::Support, FVector(0.f, 0.f, 0.f) },
		{ 0.12f, EAnchor::Support, FVector(0.f, -3.f, -3.f) },
		{ 0.28f, EAnchor::Camera, FVector(16.f, -10.f, -40.f) },
		{ 0.42f, EAnchor::Camera, FVector(14.f, -8.f, -40.f) },
		{ 0.55f, EAnchor::Magazine, FVector(0.f, -7.f, 0.f) },
		{ 0.63f, EAnchor::Magazine, FVector(0.f, 0.f, 0.f) },
		{ 0.72f, EAnchor::Magazine, FVector(-1.f, -3.f, -2.f) },
		{ 0.88f, EAnchor::Support, FVector(0.f, -2.f, -2.f) },
		{ 1.00f, EAnchor::Support, FVector(0.f, 0.f, 0.f) },
	};

	// ---- Draw after a swap: up from below the screen, muzzle low, overshoot and settle ----
	const FGunKey RifleDraw[] = {
		{ 0.00f, FVector(-6.f, 4.f, -30.f), FVector(-40.f, 16.f, 30.f) },
		{ 0.25f, FVector(-3.f, 2.f, -9.f), FVector(-14.f, 6.f, 12.f) },
		{ 0.50f, FVector(0.5f, 0.f, 0.9f), FVector(3.f, -1.f, -4.f) },
		{ 0.72f, FVector(0.f, 0.f, -0.3f), FVector(-1.f, 0.f, 1.5f) },
		{ 1.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
	};
	const FGunKey PistolDraw[] = {
		{ 0.00f, FVector(-4.f, 5.f, -26.f), FVector(-55.f, 12.f, -35.f) },
		{ 0.25f, FVector(-2.f, 2.f, -8.f), FVector(-18.f, 5.f, -10.f) },
		{ 0.50f, FVector(0.4f, 0.f, 1.0f), FVector(4.f, -1.f, 5.f) },
		{ 0.72f, FVector(0.f, 0.f, -0.3f), FVector(-1.f, 0.f, -1.5f) },
		{ 1.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
	};

	// ---- Melee: a short wind-up, then the gun chops down and in with almost no forward travel (the rig's
	// shoulder sits just beside the eye: a 16 cm thrust put it on screen in task M1, 9 cm with a roll in M3) and across to the left (the hit lands at once in
	// gameplay, so the strike peaks early), then recovery ----
	const FGunKey RifleMelee[] = {
		{ 0.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
		{ 0.10f, FVector(-3.f, 4.f, -1.f), FVector(-2.f, 6.f, 6.f) },
		{ 0.28f, FVector(3.f, -3.f, -4.f), FVector(-9.f, -8.f, 4.f) },
		{ 0.45f, FVector(2.5f, -2.5f, -3.f), FVector(-7.f, -6.f, 3.f) },
		{ 0.75f, FVector(-1.f, 1.f, -0.5f), FVector(-1.f, 2.f, 2.f) },
		{ 1.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
	};
	const FGunKey PistolMelee[] = {
		{ 0.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
		{ 0.10f, FVector(-3.f, 4.f, 1.f), FVector(4.f, 6.f, 8.f) },
		{ 0.28f, FVector(3.f, -3.f, -3.f), FVector(-10.f, -8.f, 4.f) },
		{ 0.45f, FVector(2.5f, -2.5f, -2.f), FVector(-8.f, -6.f, 3.f) },
		{ 0.75f, FVector(-1.f, 1.f, -0.4f), FVector(1.f, 3.f, 3.f) },
		{ 1.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
	};

	FMoveData GetMoveData(EShieldbreakViewmodelMove Move, EShieldbreakViewmodelStyle Style)
	{
		FMoveData Data;
		const bool bPistol = Style == EShieldbreakViewmodelStyle::Pistol;
		switch (Move)
		{
		case EShieldbreakViewmodelMove::Reload:
			switch (Style)
			{
			case EShieldbreakViewmodelStyle::Rifle: Data = { MakeArrayView(RifleReloadGun), MakeArrayView(RifleReloadHand), MakeArrayView(RifleReloadMag), 2.f, false }; break;
			case EShieldbreakViewmodelStyle::Marksman: Data = { MakeArrayView(MarksmanReloadGun), MakeArrayView(MarksmanReloadHand), MakeArrayView(MarksmanReloadMag), 2.6f, false }; break;
			case EShieldbreakViewmodelStyle::Pistol: Data = { MakeArrayView(PistolReloadGun), MakeArrayView(PistolReloadHand), MakeArrayView(PistolReloadMag), 1.4f, true }; break;
			case EShieldbreakViewmodelStyle::Launcher: Data = { MakeArrayView(LauncherReloadGun), MakeArrayView(LauncherReloadHand), TArrayView<const FMagKey>(), 3.f, false }; break;
			}
			break;
		case EShieldbreakViewmodelMove::Draw:
			Data.Gun = bPistol ? MakeArrayView(PistolDraw) : MakeArrayView(RifleDraw);
			Data.DefaultDuration = 0.5f;
			break;
		case EShieldbreakViewmodelMove::Melee:
			Data.Gun = bPistol ? MakeArrayView(PistolMelee) : MakeArrayView(RifleMelee);
			Data.DefaultDuration = 0.5f;
			break;
		default:
			break;
		}
		return Data;
	}

	/** Cubic Hermite through keys at non-uniform times; tangents from the neighbours, flat at the ends. */
	template <typename TValue, typename TKeyTime, typename TKeyValue>
	TValue Hermite(int32 Count, float T, TKeyTime KeyTime, TKeyValue KeyValue)
	{
		if (Count <= 0) { return TValue(); }
		if (T <= KeyTime(0) || Count == 1) { return KeyValue(0); }
		if (T >= KeyTime(Count - 1)) { return KeyValue(Count - 1); }
		int32 I = 0;
		while (I < Count - 2 && T > KeyTime(I + 1)) { ++I; }
		const float T0 = KeyTime(I), T1 = KeyTime(I + 1);
		const float H = FMath::Max(T1 - T0, 1e-4f);
		auto Tangent = [&](int32 K) -> TValue
		{
			if (K <= 0 || K >= Count - 1) { return TValue(0.0); }
			return (KeyValue(K + 1) - KeyValue(K - 1)) / FMath::Max(KeyTime(K + 1) - KeyTime(K - 1), 1e-4f);
		};
		const float S = (T - T0) / H;
		const float S2 = S * S, S3 = S2 * S;
		const float H00 = 2.f * S3 - 3.f * S2 + 1.f, H10 = S3 - 2.f * S2 + S, H01 = -2.f * S3 + 3.f * S2, H11 = S3 - S2;
		return KeyValue(I) * H00 + Tangent(I) * (H10 * H) + KeyValue(I + 1) * H01 + Tangent(I + 1) * (H11 * H);
	}

	FVector GunSpaceSocket(const UStaticMeshComponent* Gun, FName Socket, const FVector& Fallback)
	{
		return Gun && Gun->DoesSocketExist(Socket) ? Gun->GetSocketTransform(Socket, RTS_Component).GetLocation() : Fallback;
	}

	/** World position of a hand key this frame (the gun keeps moving, so anchors are re-evaluated every frame). */
	FVector HandKeyWorld(const FHandKey& Key, EShieldbreakViewmodelStyle Style, const FShieldbreakViewmodelContext& Context)
	{
		const FStyleData& Data = Styles[static_cast<int32>(Style)];
		if (Key.Anchor == EAnchor::Camera || !Context.Gun)
		{
			return Context.Camera.TransformPosition(Key.Anchor == EAnchor::Camera ? Key.Local : FVector(20.f, -8.f, -40.f));
		}
		FVector Local = Key.Local;
		switch (Key.Anchor)
		{
		case EAnchor::Support: Local += Context.bHasSupportPalm ? Context.SupportPalmLocal : GunSpaceSocket(Context.Gun, TEXT("SupportHand"), Data.SupportSocket); break;
		case EAnchor::Magazine: Local += GunSpaceSocket(Context.Gun, TEXT("Magazine"), Data.MagazineSocket) + Data.MagazineGrip; break;
		default: break;
		}
		return Context.Gun->GetComponentTransform().TransformPosition(Local);
	}

	FShieldbreakViewmodelPose LerpPose(const FShieldbreakViewmodelPose& A, const FShieldbreakViewmodelPose& B, float Alpha)
	{
		FShieldbreakViewmodelPose Out;
		Out.Offset = FMath::Lerp(A.Offset, B.Offset, Alpha);
		Out.Rotation = FMath::Lerp(A.Rotation, B.Rotation, Alpha);
		Out.HandWeight = FMath::Lerp(A.HandWeight, B.HandWeight, Alpha);
		Out.HandPoseWeight = FMath::Lerp(A.HandPoseWeight, B.HandPoseWeight, Alpha);
		Out.HandOrientation = FQuat::Slerp(A.HandPoseWeight > 0.f ? A.HandOrientation : B.HandOrientation,
			B.HandPoseWeight > 0.f ? B.HandOrientation : A.HandOrientation, Alpha).GetNormalized();
		for (int32 I = 0; I < 15; ++I) { Out.HandCurls[I] = FMath::Lerp(A.HandCurls[I], B.HandCurls[I], Alpha); }
		// A hand that wasn't overridden on one side starts or ends wherever the other side has it.
		const FVector HandA = A.HandWeight > 0.f ? A.HandWorld : B.HandWorld;
		const FVector HandB = B.HandWeight > 0.f ? B.HandWorld : A.HandWorld;
		Out.HandWorld = FMath::Lerp(HandA, HandB, Alpha);
		Out.bWantsLeftArm = Alpha < 0.5f ? A.bWantsLeftArm : B.bWantsLeftArm;
		return Out;
	}

	/** Where the magazine sits in hand_l space when the hand holds it at the grip key: the socket relative to the
	 *  hand placed at the grip with its current rotation. */
	/** The left hand's frame as the support-hand IK places it: at the palm (between the wrist and the middle
	 *  knuckle), turned like hand_l. */
	FTransform PalmFrame(const USkeletalMeshComponent* Arms)
	{
		const FQuat Rotation = Arms->GetBoneQuaternion(TEXT("hand_l"), EBoneSpaces::WorldSpace);
		FVector Location = Arms->GetBoneLocation(TEXT("hand_l"));
		if (Arms->GetBoneIndex(TEXT("middle_01_l")) != INDEX_NONE)
		{
			Location = (Location + Arms->GetBoneLocation(TEXT("middle_01_l"))) * 0.5;
		}
		return FTransform(Rotation, Location);
	}

	FTransform PredictHandToMagazine(EShieldbreakViewmodelStyle Style, const FShieldbreakViewmodelContext& Context)
	{
		const FStyleData& Data = Styles[static_cast<int32>(Style)];
		const FTransform GunWorld = Context.Gun->GetComponentTransform();
		const FVector SocketLocal = GunSpaceSocket(Context.Gun, TEXT("Magazine"), Data.MagazineSocket);
		const FTransform MagazineWorld(GunWorld.GetRotation(), GunWorld.TransformPosition(SocketLocal), GunWorld.GetScale3D());
		const FQuat HandRotation = Context.Arms ? Context.Arms->GetBoneQuaternion(TEXT("hand_l"), EBoneSpaces::WorldSpace) : GunWorld.GetRotation();
		const FTransform HandAtGrip(HandRotation, GunWorld.TransformPosition(SocketLocal + Data.MagazineGrip));
		return MagazineWorld.GetRelativeTransform(HandAtGrip);
	}
}

bool FShieldbreakViewmodelMoves::IsEnabled()
{
	return ShieldbreakViewmodelPrivate::CVarFPMoves.GetValueOnGameThread() != 0;
}

void FShieldbreakViewmodelMoves::SeatMagazine(UStaticMeshComponent* Magazine)
{
	if (Magazine)
	{
		Magazine->SetRelativeTransform(FTransform::Identity);
		Magazine->SetVisibility(true);
	}
}

void FShieldbreakViewmodelMoves::Start(EShieldbreakViewmodelMove InMove, EShieldbreakViewmodelStyle InStyle, float InDuration, double Now,
	UStaticMeshComponent* Magazine)
{
	using namespace ShieldbreakViewmodelPrivate;
	const FMoveData Data = GetMoveData(InMove, InStyle);
	if (Data.Gun.IsEmpty())
	{
		Stop(Now, Magazine);
		return;
	}
	SeatMagazine(Magazine);
	MagazineState = EMagazineState::Seated;
	bHasHandToMagazine = false;
	bCaptureGripAfterPose = false;
	NextMagazineEvent = 0;
	FromPose = LastPose;
	BlendStart = Now;
	// A draw starts below the screen on purpose: it must not ease in from the old gun's pose.
	BlendTime = InMove == EShieldbreakViewmodelMove::Draw ? 0.f : 0.12f;
	Move = InMove;
	Style = InStyle;
	StartTime = Now;
	const float Rate = InMove == EShieldbreakViewmodelMove::Reload ? 1.f : FMath::Max(0.25f, CVarFPMovesRate.GetValueOnGameThread());
	Duration = FMath::Max(0.1f, (InDuration > 0.f ? InDuration : Data.DefaultDuration) / Rate);
	bStopping = false;
}

void FShieldbreakViewmodelMoves::Stop(double Now, UStaticMeshComponent* Magazine)
{
	SeatMagazine(Magazine);
	MagazineState = EMagazineState::Seated;
	bHasHandToMagazine = false;
	bCaptureGripAfterPose = false;
	NextMagazineEvent = 0;
	if (Move != EShieldbreakViewmodelMove::None || !LastPose.IsIdentity())
	{
		FromPose = LastPose;
		BlendStart = Now;
		BlendTime = 0.15f;
	}
	Move = EShieldbreakViewmodelMove::None;
}

float FShieldbreakViewmodelMoves::GetProgress(double Now) const
{
	return Move == EShieldbreakViewmodelMove::None ? -1.f : FMath::Clamp(static_cast<float>((Now - StartTime) / Duration), 0.f, 1.f);
}

FShieldbreakViewmodelPose FShieldbreakViewmodelMoves::SampleGunPose(EShieldbreakViewmodelMove InMove, EShieldbreakViewmodelStyle InStyle, float T) const
{
	using namespace ShieldbreakViewmodelPrivate;
	const FMoveData Data = GetMoveData(InMove, InStyle);
	FShieldbreakViewmodelPose Pose;
	const int32 Count = Data.Gun.Num();
	Pose.Offset = Hermite<FVector>(Count, T, [&](int32 I) { return Data.Gun[I].T; }, [&](int32 I) { return Data.Gun[I].Offset; });
	Pose.Rotation = Hermite<FVector>(Count, T, [&](int32 I) { return Data.Gun[I].T; }, [&](int32 I) { return Data.Gun[I].Rotation; });
	if (InMove == EShieldbreakViewmodelMove::Reload)
	{
		// Up into view over the first 20%, held, back down over the last 20% (15% read as a snap at 2 s).
		const float Envelope = FMath::SmoothStep(0.f, 0.2f, T) * (1.f - FMath::SmoothStep(0.8f, 1.f, T))
			* FMath::Max(0.f, CVarReloadLift.GetValueOnGameThread());
		const FStyleData& Lift = Styles[static_cast<int32>(InStyle)];
		Pose.Offset += Lift.ReloadLift * Envelope;
		Pose.Rotation += Lift.ReloadLiftRotation * Envelope;
	}
	return Pose;
}

void FShieldbreakViewmodelMoves::Update(double Now, const FShieldbreakViewmodelContext& Context, FShieldbreakViewmodelPose& OutPose)
{
	using namespace ShieldbreakViewmodelPrivate;

	// A reload the game ended early (swap, melee, a server that refused it) eases out instead of finishing.
	if (Move == EShieldbreakViewmodelMove::Reload && !Context.bReloading && GetProgress(Now) < 0.97f)
	{
		Stop(Now, Context.Magazine);
	}

	FShieldbreakViewmodelPose Target;
	if (Move != EShieldbreakViewmodelMove::None)
	{
		const float T = GetProgress(Now);
		const FMoveData Data = GetMoveData(Move, Style);
		Target = SampleGunPose(Move, Style, T);

		if (!Data.Hand.IsEmpty())
		{
			const int32 Count = Data.Hand.Num();
			TArray<FVector, TInlineAllocator<20>> Points;
			for (const FHandKey& Key : Data.Hand)
			{
				Points.Add(HandKeyWorld(Key, Style, Context));
			}
			Target.HandWorld = Hermite<FVector>(Count, T, [&](int32 I) { return Data.Hand[I].T; }, [&](int32 I) { return Points[I]; });
			Target.HandWeight = 1.f;
		}
		Target.bWantsLeftArm = Data.bShowsLeftArm;

		// Consume events once. A carry -> drop -> fresh sequence must not replay
		// the old drop on every subsequent frame after a new magazine is grasped.
		while (NextMagazineEvent < Data.Magazine.Num() && T >= Data.Magazine[NextMagazineEvent].T)
		{
			const FMagKey& Key = Data.Magazine[NextMagazineEvent++];
			if (!Context.Magazine || !Context.Gun)
			{
				continue;
			}
			switch (Key.Event)
			{
			case EMagEvent::Grab:
				if (MagazineState == EMagazineState::Seated && !bHasHandToMagazine)
				{
					// Capture the actual grasp only after this frame's IK has reached the magazine.
					bCaptureGripAfterPose = true;
					bHasHandToMagazine = true;
					MagazineState = EMagazineState::InHand;
				}
				break;
			case EMagEvent::Drop:
				if (MagazineState == EMagazineState::Seated || MagazineState == EMagazineState::InHand)
				{
					FallStart = Context.Magazine->GetComponentTransform().GetRelativeTransform(Context.Camera);
					FallTime = Now;
					MagazineState = EMagazineState::Falling;
				}
				break;
			case EMagEvent::Fresh:
				if (MagazineState == EMagazineState::Falling || MagazineState == EMagazineState::Hidden)
				{
					// A fresh magazine starts at the authored grasp, not an offset
					// captured while the hand was still reaching for the old one.
					HandToMagazine = PredictHandToMagazine(Style, Context);
					bHasHandToMagazine = true;
					MagazineState = EMagazineState::InHand;
				}
				break;
			case EMagEvent::Seat:
				if (MagazineState != EMagazineState::Seated)
				{
					SeatMagazine(Context.Magazine);
					MagazineState = EMagazineState::Seated;
				}
				break;
			}
		}

		if (T >= 1.f)
		{
			SeatMagazine(Context.Magazine);
			MagazineState = EMagazineState::Seated;
			Move = EShieldbreakViewmodelMove::None;
			Target = FShieldbreakViewmodelPose();
		}
	}

	// Carry the magazine: in the left hand, or falling away below the view (camera space, so it always clears the
	// screen the same way whatever the player looks at).
	if (UStaticMeshComponent* Magazine = Context.Magazine)
	{
		if (MagazineState == EMagazineState::Falling)
		{
			const float Age = static_cast<float>(Now - FallTime);
			if (Age > 0.45f)
			{
				Magazine->SetVisibility(false);
				MagazineState = EMagazineState::Hidden;
			}
			else
			{
				FTransform Fall = FallStart;
				Fall.AddToTranslation(FVector(8.f * Age, -10.f * Age, -40.f * Age - 0.5f * 980.f * Age * Age));
				Fall.ConcatenateRotation(FQuat(FVector(0.f, 1.f, 0.f), FMath::DegreesToRadians(-220.f * Age)));
				Magazine->SetWorldTransform(Fall * Context.Camera);
			}
		}
	}

	// Ease from where the last move (or a stop) left the gun.
	const float Blend = BlendTime > 0.f ? FMath::Clamp(static_cast<float>((Now - BlendStart) / BlendTime), 0.f, 1.f) : 1.f;
	OutPose = Blend < 1.f ? LerpPose(FromPose, Target, FMath::SmoothStep(0.f, 1.f, Blend)) : Target;
	LastPose = OutPose;
}

void FShieldbreakViewmodelMoves::RefreshHandTarget(double Now, const FShieldbreakViewmodelContext& Context,
	FShieldbreakViewmodelPose& OutPose)
{
	using namespace ShieldbreakViewmodelPrivate;
	if (Move == EShieldbreakViewmodelMove::None) { return; }
	const FMoveData Data = GetMoveData(Move, Style);
	if (Data.Hand.IsEmpty()) { return; }
	const float T = GetProgress(Now);
	const FVector Target = Hermite<FVector>(Data.Hand.Num(), T, [&](int32 I) { return Data.Hand[I].T; },
		[&](int32 I) { return HandKeyWorld(Data.Hand[I], Style, Context); });
	const float Blend = BlendTime > 0.f ? FMath::SmoothStep(0.f, 1.f,
		FMath::Clamp(static_cast<float>((Now - BlendStart) / BlendTime), 0.f, 1.f)) : 1.f;
	OutPose.HandWorld = FromPose.HandWeight > 0.f ? FMath::Lerp(FromPose.HandWorld, Target, Blend) : Target;
	if (Move == EShieldbreakViewmodelMove::Reload && Style == EShieldbreakViewmodelStyle::Rifle && Context.Gun)
	{
		const int32 Count = UE_ARRAY_COUNT(RifleHandShape);
		auto Time = [](int32 I) { return RifleHandShape[I].T; };
		const float Weight = FMath::Clamp(Hermite<float>(Count, T, Time, [](int32 I) { return RifleHandShape[I].Weight; }), 0.f, 1.f);
		const float Close = FMath::Clamp(Hermite<float>(Count, T, Time, [](int32 I) { return RifleHandShape[I].Close; }), 0.f, 1.f);
		const FVector Wrist = Hermite<FVector>(Count, T, Time, [](int32 I) { return RifleHandShape[I].Wrist; });
		const FQuat SideGrip = FRotationMatrix::MakeFromXZ(FVector::YAxisVector, -FVector::XAxisVector).ToQuat();
		OutPose.HandOrientation = (Context.Gun->GetComponentQuat() * SideGrip * FRotator(Wrist.X, Wrist.Y, Wrist.Z).Quaternion()).GetNormalized();
		OutPose.HandPoseWeight = Weight * Blend;
		// A relaxed open hand (as when letting the empty magazine fall), not a splayed one.
		static constexpr float Open[15] = { 6.f, 14.f, 8.f, 6.f, 16.f, 10.f, 8.f, 18.f, 12.f, 10.f, 20.f, 14.f, 4.f, 8.f, 8.f };
		static constexpr float Closed[15] = { 28.f, 65.f, 38.f, 40.f, 78.f, 48.f, 45.f, 82.f, 50.f, 50.f, 85.f, 50.f, 8.f, 20.f, 12.f };
		for (int32 I = 0; I < 15; ++I) { OutPose.HandCurls[I] = FMath::Lerp(Open[I], Closed[I], Close); }
	}
	LastPose = OutPose; // cancellation/replacement starts from the refreshed wrist and hand target too
}

void FShieldbreakViewmodelMoves::UpdateMagazineAfterPose(double Now, const FShieldbreakViewmodelContext& Context)
{
	using namespace ShieldbreakViewmodelPrivate;
	if (MagazineState != EMagazineState::InHand || !Context.Gun || !Context.Magazine || !Context.Arms
		|| Context.Arms->GetBoneIndex(TEXT("hand_l")) == INDEX_NONE) { return; }
	const FTransform Hand = PalmFrame(Context.Arms);
	const FTransform Socket = Context.Gun->GetSocketTransform(TEXT("Magazine"), RTS_World);
	if (bCaptureGripAfterPose)
	{
		HandToMagazine = Socket.GetRelativeTransform(Hand);
		bCaptureGripAfterPose = false;
	}
	FTransform Carried = HandToMagazine * Hand;
	// Settle into the well before handing ownership back to the socket. Small IK
	// errors must not turn the seat event into a visible magazine teleport.
	for (const FMagKey& Key : GetMoveData(Move, Style).Magazine)
	{
		if (Key.Event == EMagEvent::Seat)
		{
			const float SeatBlend = FMath::SmoothStep(Key.T - 0.06f, Key.T, GetProgress(Now));
			FTransform Seated;
			Seated.Blend(Carried, Socket, SeatBlend);
			Carried = Seated;
			break;
		}
	}
	Context.Magazine->SetWorldTransform(Carried);
	Context.Magazine->SetVisibility(true);
}
