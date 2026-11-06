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
		/* Rifle */    { FVector(3.f, 1.5f, 6.f), FVector(5.f, 3.f, 0.f), FVector(0.f, 13.3f, -3.6f), FVector(-3.2f, 2.0f, -8.0f), FVector(-2.5f, 24.f, -7.5f) },
		/* Marksman */ { FVector(3.f, 1.5f, 5.5f), FVector(4.f, 3.f, 0.f), FVector(0.f, 14.5f, -3.6f), FVector(-3.2f, 2.0f, -6.5f), FVector(-2.5f, 30.f, -6.6f) },
		/* Pistol */   { FVector(2.f, 0.f, 5.f), FVector(6.f, 4.f, -6.f), FVector(0.f, -2.2f, -2.4f), FVector(-3.0f, -1.8f, -10.f), FVector(-3.f, -2.f, -12.f) },
		/* Launcher */ { FVector(0.f, -2.f, 4.f), FVector(0.f, 0.f, 0.f), FVector(0.f, -22.f, 6.5f), FVector(-8.f, 0.f, -2.f), FVector(-2.f, 20.f, -7.f) },
	};

	// ---- Reloads ----

	// Battle rifle, 2.0 s. Cant the gun right so the magazine well turns toward the left hand, strip the magazine
	// (the gun dips with the pull and springs back), take it down out of view, bring it back up and seat it with a
	// shove, palm the base, then roll the gun back to show its left side and rack the charging handle.
	const FGunKey RifleReloadGun[] = {
		{ 0.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
		{ 0.10f, FVector(-1.5f, -2.0f, 1.0f), FVector(5.f, -6.f, 22.f) },
		{ 0.20f, FVector(-2.0f, -2.5f, 1.6f), FVector(7.f, -7.f, 30.f) },
		{ 0.25f, FVector(-2.0f, -2.4f, 0.6f), FVector(5.f, -7.f, 31.f) },
		{ 0.31f, FVector(-1.8f, -2.4f, 2.3f), FVector(9.f, -7.f, 33.f) },
		{ 0.46f, FVector(-2.2f, -2.6f, 1.7f), FVector(7.f, -6.f, 30.f) },
		{ 0.60f, FVector(-2.2f, -2.8f, 1.4f), FVector(6.f, -7.f, 31.f) },
		{ 0.665f, FVector(-2.0f, -2.8f, 2.8f), FVector(10.f, -7.f, 33.f) },
		{ 0.71f, FVector(-2.0f, -2.8f, 1.7f), FVector(8.f, -7.f, 31.f) },
		{ 0.75f, FVector(-2.0f, -2.8f, 2.4f), FVector(9.f, -7.f, 32.f) },
		{ 0.81f, FVector(-2.2f, -1.6f, 1.2f), FVector(4.f, 2.f, 16.f) },
		{ 0.87f, FVector(-3.6f, -1.6f, 0.9f), FVector(6.f, 2.f, 20.f) },
		{ 0.93f, FVector(-0.8f, -0.4f, 0.5f), FVector(1.5f, 0.f, 5.f) },
		{ 1.00f, FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f) },
	};
	const FHandKey RifleReloadHand[] = {
		{ 0.00f, EAnchor::Support, FVector(0.f, 0.f, 0.f) },
		{ 0.08f, EAnchor::Support, FVector(-0.5f, -3.f, -2.f) },
		{ 0.18f, EAnchor::Magazine, FVector(0.f, 0.f, -0.8f) },
		{ 0.21f, EAnchor::Magazine, FVector(0.f, 0.f, 0.f) },
		{ 0.27f, EAnchor::Magazine, FVector(0.f, 0.5f, -6.f) },
		{ 0.37f, EAnchor::Camera, FVector(22.f, -6.f, -40.f) },
		{ 0.48f, EAnchor::Camera, FVector(20.f, -4.f, -42.f) },
		{ 0.58f, EAnchor::Magazine, FVector(0.f, 0.5f, -9.f) },
		{ 0.665f, EAnchor::Magazine, FVector(0.f, 0.f, 0.f) },
		{ 0.70f, EAnchor::Magazine, FVector(-0.5f, 0.f, -5.f) },
		{ 0.73f, EAnchor::Magazine, FVector(0.f, 0.f, -9.5f) },
		{ 0.755f, EAnchor::Magazine, FVector(0.f, 0.f, -7.5f) },
		{ 0.81f, EAnchor::Gun, FVector(-5.5f, 4.f, 3.5f) },
		{ 0.87f, EAnchor::Gun, FVector(-5.5f, -3.f, 3.5f) },
		{ 0.94f, EAnchor::Support, FVector(-0.5f, -1.f, -1.f) },
		{ 1.00f, EAnchor::Support, FVector(0.f, 0.f, 0.f) },
	};
	const FMagKey RifleReloadMag[] = { { 0.21f, EMagEvent::Grab }, { 0.665f, EMagEvent::Seat } };

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
		// Up into view over the first 15%, held, back down over the last 15%.
		const float Envelope = FMath::SmoothStep(0.f, 0.15f, T) * (1.f - FMath::SmoothStep(0.85f, 1.f, T))
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

		// Magazine events (in order; a frame may pass several).
		for (const FMagKey& Key : Data.Magazine)
		{
			if (T < Key.T || !Context.Magazine || !Context.Gun)
			{
				continue;
			}
			switch (Key.Event)
			{
			case EMagEvent::Grab:
				if (MagazineState == EMagazineState::Seated && !bHasHandToMagazine)
				{
					HandToMagazine = PredictHandToMagazine(Style, Context);
					bHasHandToMagazine = true;
					MagazineState = EMagazineState::InHand;
				}
				break;
			case EMagEvent::Drop:
				if (MagazineState == EMagazineState::Seated)
				{
					FallStart = Context.Magazine->GetComponentTransform().GetRelativeTransform(Context.Camera);
					FallTime = Now;
					MagazineState = EMagazineState::Falling;
				}
				break;
			case EMagEvent::Fresh:
				if (MagazineState == EMagazineState::Falling || MagazineState == EMagazineState::Hidden)
				{
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
		if (MagazineState == EMagazineState::InHand && Context.Arms && Context.Arms->GetBoneIndex(TEXT("hand_l")) != INDEX_NONE)
		{
			const FTransform Hand = PalmFrame(Context.Arms);
			Magazine->SetWorldTransform(HandToMagazine * Hand);
			Magazine->SetVisibility(true);
		}
		else if (MagazineState == EMagazineState::Falling)
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
