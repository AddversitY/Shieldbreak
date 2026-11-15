#include "UI/ShieldbreakHUD.h"

#include "Game/ShieldbreakTeams.h"
#include "Game/ShieldbreakObjectives.h"

#include "Shieldbreak.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/ShieldbreakGameState.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShieldbreakCharacter.h"
#include "Player/ShieldbreakInputSet.h"
#include "Player/ShieldbreakSettings.h"
#include "Audio/ShieldbreakMusic.h"
#include "Game/ShieldbreakMedals.h"
#include "Misc/CommandLine.h"
#include "Player/ShieldbreakMovementComponent.h"
#include "Player/ShieldbreakPlayerController.h"
#include "Player/ShieldbreakPlayerState.h"
#include "Shield/ShieldHealthComponent.h"
#include "Shield/ShieldTuningData.h"
#include "Weapons/ShieldbreakWeapon.h"

namespace ShieldbreakHUDPrivate
{
	const FLinearColor ShieldBlue(0.25f, 0.65f, 1.f, 0.95f);
	const FLinearColor ShieldBright(0.6f, 0.9f, 1.f, 1.f);
	const FLinearColor Overshield(1.f, 0.78f, 0.15f, 1.f);
	const FLinearColor Danger(1.f, 0.15f, 0.1f, 1.f);
	const FLinearColor Panel(0.f, 0.f, 0.f, 0.45f);
	const FLinearColor White(1.f, 1.f, 1.f, 1.f);
	const FLinearColor Dim(1.f, 1.f, 1.f, 0.55f);
	const FLinearColor Gold(1.f, 0.8f, 0.2f, 1.f);
	const FLinearColor Green(0.3f, 1.f, 0.4f, 1.f);
}

// ---- Primitives ----

void AShieldbreakHUD::Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float TextScale, bool bCenter, bool bRight)
{
	if (!Canvas || !Font)
	{
		return;
	}
	float W = 0.f;
	float H = 0.f;
	Canvas->TextSize(Font, Str, W, H, TextScale, TextScale);
	if (bCenter)
	{
		X -= W * 0.5f;
	}
	else if (bRight)
	{
		X -= W;
	}

	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Str), Font, Color);
	Item.Scale = FVector2D(TextScale, TextScale);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	Canvas->DrawItem(Item);
}

void AShieldbreakHUD::Box(float X, float Y, float W, float H, const FLinearColor& Color)
{
	if (Canvas && W > 0.f && H > 0.f)
	{
		FCanvasTileItem Tile(FVector2D(X, Y), FVector2D(W, H), Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}
}

void AShieldbreakHUD::Line(float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness)
{
	if (Canvas)
	{
		FCanvasLineItem Item(FVector2D(X0, Y0), FVector2D(X1, Y1));
		Item.SetColor(Color);
		Item.LineThickness = Thickness;
		Canvas->DrawItem(Item);
	}
}

void AShieldbreakHUD::Circle(float CX, float CY, float Radius, const FLinearColor& Color, float Thickness, int32 Segments)
{
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float A0 = 2.f * PI * Index / Segments;
		const float A1 = 2.f * PI * (Index + 1) / Segments;
		Line(CX + Radius * FMath::Cos(A0), CY + Radius * FMath::Sin(A0), CX + Radius * FMath::Cos(A1), CY + Radius * FMath::Sin(A1), Color, Thickness);
	}
}

void AShieldbreakHUD::Arc(float CX, float CY, float Radius, float CenterAngleDeg, float SpanDeg, const FLinearColor& Color, float Thickness)
{
	constexpr int32 Segments = 10;
	const float Start = FMath::DegreesToRadians(CenterAngleDeg - SpanDeg * 0.5f);
	const float Step = FMath::DegreesToRadians(SpanDeg) / Segments;
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float A0 = Start + Step * Index;
		const float A1 = A0 + Step;
		Line(CX + Radius * FMath::Cos(A0), CY + Radius * FMath::Sin(A0), CX + Radius * FMath::Cos(A1), CY + Radius * FMath::Sin(A1), Color, Thickness);
	}
}

float AShieldbreakHUD::LocalTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

float AShieldbreakHUD::ServerTime() const
{
	const UWorld* World = GetWorld();
	if (const AGameStateBase* GS = World ? World->GetGameState() : nullptr)
	{
		return static_cast<float>(GS->GetServerWorldTimeSeconds());
	}
	return LocalTime();
}

void AShieldbreakHUD::BindShieldEvents(UShieldHealthComponent* Shield)
{
	if (Shield == BoundShield.Get())
	{
		return;
	}
	if (UShieldHealthComponent* Old = BoundShield.Get())
	{
		Old->OnVentResult.RemoveDynamic(this, &AShieldbreakHUD::HandleVentResult);
	}
	BoundShield = Shield;
	if (Shield)
	{
		Shield->OnVentResult.AddUniqueDynamic(this, &AShieldbreakHUD::HandleVentResult);
	}
}

void AShieldbreakHUD::HandleVentResult(bool bSuccess)
{
	VentResultTime = LocalTime();
	bVentSucceeded = bSuccess;
}

// ---- Main ----

void AShieldbreakHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}
	S = Canvas->ClipY / 1080.f;

	AShieldbreakPlayerController* PC = Cast<AShieldbreakPlayerController>(PlayerOwner);
	AShieldbreakCharacter* Character = Cast<AShieldbreakCharacter>(PlayerOwner ? PlayerOwner->GetPawn() : nullptr);
	AShieldbreakGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AShieldbreakGameState>() : nullptr;

	BindShieldEvents(Character ? Character->GetShieldHealth() : nullptr);

	if (Character && Character->IsAlive())
	{
		// The crosshair first: while scoped it draws the scope housing, and the rest of the HUD sits on top of it.
		DrawCrosshair(Character, PC);
		DrawShieldAndHealth(Character);
		DrawDamageDirection(Character, PC);
		DrawWeaponInfo(Character);
		DrawMomentum(Character);
		DrawMotionTracker(Character);
		DrawTeammateNames(Character);
		DrawEquipmentStatus(Character);
		DrawObjectives(Character);
	}

	if (GameState)
	{
		DrawKillFeed(GameState);
		DrawTopScores(GameState);
	}

	DrawRespawn(PC, GameState);
	DrawMedals(PC);

	if (GameState && GameState->IsMatchOver())
	{
		DrawMatchOver(GameState);
	}
	if (PC && PC->IsScoreboardVisible() && GameState)
	{
		DrawScoreboard(GameState);
	}
	// The controls panel would sit over the reticle while scoped (task 15 shots), so it waits until zoom ends.
	if (PC && PC->IsHelpVisible() && !(Character && Character->GetScopeBlend() > 0.f))
	{
		DrawHelp();
	}
	else
	{
		DrawTips(Character);
	}
}

// ---- Shield / health / mastery prompts ----

void AShieldbreakHUD::DrawShieldAndHealth(AShieldbreakCharacter* Character)
{
	using namespace ShieldbreakHUDPrivate;

	const UShieldHealthComponent* Shield = Character->GetShieldHealth();
	if (!Shield)
	{
		return;
	}
	const UShieldTuningData* Tuning = Shield->GetTuningData();
	const float Now = LocalTime();

	const float BarW = 520.f * S;
	const float BarH = 18.f * S;
	const float BarX = (Canvas->ClipX - BarW) * 0.5f;
	const float BarY = 40.f * S;

	Box(BarX - 3.f * S, BarY - 3.f * S, BarW + 6.f * S, BarH + 6.f * S, Panel);

	const float Fraction = Shield->GetMaxShield() > 0.f ? Shield->GetShield() / Shield->GetMaxShield() : 0.f;
	const FLinearColor Fill = Shield->IsRecharging() ? ShieldBright : ShieldBlue;

	// Ten segments with small gaps.
	constexpr int32 Segments = 10;
	const float Gap = 3.f * S;
	const float SegW = (BarW - Gap * (Segments - 1)) / Segments;
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float SegStart = static_cast<float>(Index) / Segments;
		const float SegFill = FMath::Clamp((Fraction - SegStart) * Segments, 0.f, 1.f);
		const float X = BarX + Index * (SegW + Gap);
		Box(X, BarY, SegW, BarH, FLinearColor(0.1f, 0.15f, 0.2f, 0.6f));
		Box(X, BarY, SegW * SegFill, BarH, Fill);
	}

	// Your own shield just popped: a quick blue flash over the view (one translucent tile, fading in 0.3 s).
	const float PopAge = Now - Shield->GetLastShieldBreakLocalTime();
	if (Shield->GetLastShieldBreakLocalTime() >= 0.f && PopAge >= 0.f && PopAge < 0.3f)
	{
		Box(0.f, 0.f, Canvas->ClipX, Canvas->ClipY, FLinearColor(0.35f, 0.75f, 1.f, 0.22f * FMath::Square(1.f - PopAge / 0.3f)));
	}

	// Empty: flash red.
	if (Shield->GetShield() <= 0.f && FMath::Fmod(Now, 0.5f) < 0.25f)
	{
		Box(BarX, BarY, BarW, BarH, FLinearColor(1.f, 0.1f, 0.05f, 0.55f));
	}

	// Overshield: gold strip over the top half.
	if (Shield->GetOvershield() > 0.f && Shield->GetMaxOvershield() > 0.f)
	{
		Box(BarX, BarY, BarW * FMath::Clamp(Shield->GetOvershield() / Shield->GetMaxOvershield(), 0.f, 1.f), BarH * 0.45f, Overshield);
	}

	// Overcharge Vent sweep.
	const float Marker = Shield->GetVentMarkerPosition();
	if (Marker >= 0.f && Tuning)
	{
		const float SweetX = BarX + BarW * Tuning->VentSweetSpotStart;
		const float SweetW = BarW * Tuning->VentSweetSpotWidth;
		Box(SweetX, BarY - 6.f * S, SweetW, BarH + 12.f * S, FLinearColor(0.3f, 1.f, 0.4f, 0.35f));
		const float MarkerX = BarX + BarW * Marker;
		Box(MarkerX - 2.f * S, BarY - 9.f * S, 4.f * S, BarH + 18.f * S, White);
		Text(FString::Printf(TEXT("[%s] VENT"), *UShieldbreakInputSet::Get()->GetKeyName(TEXT("ReloadVent"))), SweetX + SweetW * 0.5f, BarY + BarH + 10.f * S, Green, GEngine->GetSmallFont(), S, true);
	}

	if (Now - VentResultTime < 1.f)
	{
		Text(bVentSucceeded ? TEXT("VENT!") : TEXT("VENT FAILED"), Canvas->ClipX * 0.5f, BarY + BarH + 28.f * S,
			bVentSucceeded ? Gold : Danger, GEngine->GetMediumFont(), S, true);
	}

	// Health pips, Halo style: only shown once the shield is down or health is hurt.
	if (Shield->GetShield() <= 0.f || Shield->GetHealth() < Shield->GetMaxHealth())
	{
		constexpr int32 Pips = 8;
		const float HealthFraction = Shield->GetMaxHealth() > 0.f ? Shield->GetHealth() / Shield->GetMaxHealth() : 0.f;
		const float PipW = 28.f * S;
		const float PipGap = 6.f * S;
		const float PipsW = Pips * PipW + (Pips - 1) * PipGap;
		const float PipY = BarY + BarH + 52.f * S;
		const FLinearColor PipColor = HealthFraction < 0.35f ? Danger : FLinearColor(0.85f, 0.85f, 0.85f, 0.9f);
		for (int32 Index = 0; Index < Pips; ++Index)
		{
			const float X = (Canvas->ClipX - PipsW) * 0.5f + Index * (PipW + PipGap);
			const bool bFull = HealthFraction > (static_cast<float>(Index) + 0.01f) / Pips;
			Box(X, PipY, PipW, 7.f * S, bFull ? PipColor : FLinearColor(0.2f, 0.2f, 0.2f, 0.6f));
		}
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;

	// Aegis raised: frontal arc plus remaining strength / uptime.
	if (Character->IsAegisRaised())
	{
		const FLinearColor AegisColor(0.3f, 0.8f, 1.f, 0.9f);
		Arc(CX, CY, 70.f * S, -90.f, 120.f, AegisColor, 4.f * S);
		Arc(CX, CY, 78.f * S, -90.f, 120.f * Character->GetAegisHealthFraction(), FLinearColor(AegisColor.R, AegisColor.G, AegisColor.B, 0.6f), 3.f * S);
		Arc(CX, CY, 84.f * S, -90.f, 120.f * Character->GetAegisUptimeFraction(), FLinearColor(1.f, 1.f, 1.f, 0.4f), 2.f * S);
		Text(TEXT("AEGIS"), CX, CY - 100.f * S, AegisColor, GEngine->GetSmallFont(), S, true);
	}

	// Power Jump charge: a bar under the crosshair.
	if (const UShieldbreakMovementComponent* Movement = Character->GetShieldbreakMovement(); Movement && Movement->IsChargingPowerJump())
	{
		const float Charge = FMath::Clamp(Movement->GetPowerJumpCharge(), 0.f, 1.f);
		Box(CX - 40.f * S, CY + 40.f * S, 80.f * S, 6.f * S, Panel);
		Box(CX - 40.f * S, CY + 40.f * S, 80.f * S * Charge, 6.f * S, Charge >= 1.f ? Gold : ShieldBright);
	}

	// Charge weapon: a ring that closes as the charge builds; gold when it's a full-damage shot.
	if (const AShieldbreakWeapon* ChargeWeapon = Character->GetCurrentWeapon(); ChargeWeapon && ChargeWeapon->IsCharging())
	{
		const float Charge = ChargeWeapon->GetChargeFraction();
		const FLinearColor ChargeColor = Charge >= 1.f ? Gold : FLinearColor(0.3f, 0.95f, 1.f, 0.9f);
		Arc(CX, CY, 30.f * S, 0.f, 360.f * Charge, ChargeColor, 3.f * S);
	}

	// Break Pulse prompt right after the shield pops (teaches the timing).
	const float BreakTime = Shield->GetLastShieldBreakLocalTime();
	const float Window = Tuning ? Tuning->BreakPulseWindow : 0.15f;
	if (BreakTime >= 0.f && Now - BreakTime <= Window + 0.25f)
	{
		const float Alpha = 1.f - (Now - BreakTime) / (Window + 0.25f);
		Text(FString::Printf(TEXT("PULSE [%s]"), *UShieldbreakInputSet::Get()->GetKeyName(TEXT("Melee"))), CX, CY + 60.f * S, FLinearColor(0.4f, 0.75f, 1.f, Alpha), GEngine->GetLargeFont(), S, true);
	}
}

// ---- Crosshair ----

void AShieldbreakHUD::DrawCrosshair(AShieldbreakCharacter* Character, AShieldbreakPlayerController* PC)
{
	using namespace ShieldbreakHUDPrivate;

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const AShieldbreakWeapon* Weapon = Character->GetCurrentWeapon();
	const float Now = LocalTime();

	// Red reticle when an enemy is under the crosshair within range.
	bool bOnEnemy = false;
	AimDistance = 0.f;
	if (Weapon && GetWorld())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		PlayerOwner->GetPlayerViewPoint(ViewLocation, ViewRotation);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakReticle), true, Character);
		if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * Weapon->GetStats().Range, ECC_Weapon, Params))
		{
			AimDistance = Hit.Distance;
			const AShieldbreakCharacter* HitCharacter = Cast<AShieldbreakCharacter>(Hit.GetActor());
			bOnEnemy = HitCharacter && HitCharacter != Character && HitCharacter->IsAlive()
				&& !ShieldbreakTeams::AreTeammates(HitCharacter, Character);
		}
	}
	const FLinearColor ReticleColor = bOnEnemy ? Danger : FLinearColor(1.f, 1.f, 1.f, 0.85f);

	// Spread ring sized from the true cone at the current FOV.
	float RingRadius = 14.f * S;
	if (Weapon)
	{
		const UCameraComponent* Camera = Character->GetFirstPersonCamera();
		const float FOV = Camera ? Camera->FieldOfView : 78.f;
		const float Spread = Weapon->GetCurrentSpreadDegrees();
		RingRadius = FMath::Max(6.f * S, FMath::Tan(FMath::DegreesToRadians(Spread)) / FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f)) * Canvas->ClipX * 0.5f);
		RingRadius = FMath::Min(RingRadius, 160.f * S);
	}
	if (!DrawScope(Character, bOnEnemy))
	{
		Box(CX - 2.f * S, CY - 2.f * S, 4.f * S, 4.f * S, ReticleColor);
		Circle(CX, CY, RingRadius + 6.f * S, ReticleColor, 1.5f * S, 28);
	}

	// Hit marker: an X that pops out a little bigger and settles as it fades. White on health, shield blue on a
	// shielded hit, red on a kill. Popping a shield adds a ring bursting outward, so you know the head is open.
	if (PC)
	{
		constexpr float MarkerLife = 0.28f;
		const float Age = Now - PC->GetLastHitConfirmTime();
		if (Age >= 0.f && Age < MarkerLife)
		{
			const bool bKill = PC->WasLastHitKill();
			const bool bBreak = PC->WasLastHitShieldBreak();
			const float T = Age / MarkerLife;
			const float Pop = 1.f + 0.35f * FMath::Square(1.f - FMath::Min(1.f, Age / 0.08f));
			const float Size = (bKill ? 1.45f : bBreak ? 1.25f : 1.f) * Pop;
			const float Inner = 7.f * S * Size;
			const float Outer = 15.f * S * Size;
			FLinearColor MarkColor = bKill ? Danger : (PC->WasLastHitShield() ? ShieldBright : White);
			MarkColor.A *= 1.f - T * T;
			const float Thick = (bKill ? 3.f : bBreak ? 2.5f : 2.f) * S;
			for (const FVector2D Dir : { FVector2D(1.f, 1.f), FVector2D(-1.f, 1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, -1.f) })
			{
				const FVector2D N = Dir.GetSafeNormal();
				Line(CX + N.X * Inner, CY + N.Y * Inner, CX + N.X * Outer, CY + N.Y * Outer, MarkColor, Thick);
			}
		}
		constexpr float BurstLife = 0.35f;
		const float BurstAge = Now - PC->GetLastShieldBreakConfirmTime();
		if (BurstAge >= 0.f && BurstAge < BurstLife)
		{
			const float T = BurstAge / BurstLife;
			const float Ease = 1.f - FMath::Square(1.f - T);
			const FLinearColor BurstColor(ShieldBright.R, ShieldBright.G, ShieldBright.B, 0.9f * (1.f - T));
			Circle(CX, CY, FMath::Lerp(14.f, 42.f, Ease) * S, BurstColor, FMath::Lerp(3.f, 1.f, T) * S, 32);
		}
	}
	if (PC && PC->WasLastHitHeadshot() && PC->WasLastHitKill() && Now - PC->GetLastHitConfirmTime() < 1.f)
	{
		Text(TEXT("HEADSHOT"), CX, CY - 70.f * S, Gold, GEngine->GetMediumFont(), S, true);
	}
}

// ---- Scopes ----
// Two original optics, one per scoped weapon, so zooming in reads as looking through a different instrument rather
// than a narrower view (Vincent 2026-11-06). The Battle Rifle's Marksman optic is a cool cyan instrument with a
// chevron reticle and holdover marks; the Sniper's LongRange optic is a full-height lens with a warm fine cross, mil
// ladder, heading tape and a second zoom level. The gun and arms hide while either is up (UpdateScopeHide).

UTexture2D* AShieldbreakHUD::GetLensMaskTexture()
{
	if (LensMask)
	{
		return LensMask;
	}
	constexpr int32 Size = 256;
	LensMask = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (!LensMask)
	{
		return nullptr;
	}
	LensMask->SRGB = false;
	LensMask->Filter = TF_Bilinear;
	FTexture2DMipMap& Mip = LensMask->GetPlatformData()->Mips[0];
	FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const float DX = (X + 0.5f) / Size * 2.f - 1.f;
			const float DY = (Y + 0.5f) / Size * 2.f - 1.f;
			const float D = FMath::Sqrt(DX * DX + DY * DY);
			// Clear centre, a gentle vignette from 70% out, then a soft but quick edge into solid housing.
			const float Vignette = 0.55f * FMath::SmoothStep(0.7f, 0.985f, D);
			const float Edge = FMath::SmoothStep(0.975f, 1.f, D);
			const float A = FMath::Clamp(FMath::Max(Vignette, Edge), 0.f, 1.f);
			Pixels[Y * Size + X] = FColor(255, 255, 255, static_cast<uint8>(A * 255.f + 0.5f));
		}
	}
	Mip.BulkData.Unlock();
	LensMask->UpdateResource();
	return LensMask;
}

void AShieldbreakHUD::DrawLensMask(float CX, float CY, float R, const FLinearColor& Color)
{
	// Solid housing everywhere outside the lens square, then the soft round mask inside it.
	Box(0.f, 0.f, Canvas->ClipX, FMath::Max(0.f, CY - R), Color);
	Box(0.f, CY + R, Canvas->ClipX, FMath::Max(0.f, Canvas->ClipY - (CY + R)), Color);
	Box(0.f, CY - R, FMath::Max(0.f, CX - R), 2.f * R, Color);
	Box(CX + R, CY - R, FMath::Max(0.f, Canvas->ClipX - (CX + R)), 2.f * R, Color);
	if (UTexture2D* Mask = GetLensMaskTexture())
	{
		FCanvasTileItem Tile(FVector2D(CX - R, CY - R), Mask->GetResource(), FVector2D(2.f * R, 2.f * R), Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}
}

bool AShieldbreakHUD::DrawScope(AShieldbreakCharacter* Character, bool bOnEnemy)
{
	const AShieldbreakWeapon* Weapon = Character->GetCurrentWeapon();
	const float Blend = Character->GetScopeBlend();
	if (!Weapon || Weapon->GetStats().Scope == EShieldbreakScope::None || Blend <= 0.01f)
	{
		return false;
	}
	const float Alpha = FMath::SmoothStep(0.f, 1.f, Blend);
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	// The lens opens out from slightly smaller as the scope comes up.
	const float Open = FMath::Lerp(0.9f, 1.f, Alpha);
	const float RangeMeters = AimDistance / 100.f;
	if (Weapon->GetStats().Scope == EShieldbreakScope::LongRange)
	{
		DrawLongRangeScope(Character, CX, CY, Canvas->ClipY * 0.475f * Open, Alpha, bOnEnemy, RangeMeters);
	}
	else
	{
		DrawMarksmanScope(Character, CX, CY, Canvas->ClipY * 0.4f * Open, Alpha, bOnEnemy, RangeMeters);
	}
	return Blend > 0.5f;
}

namespace ShieldbreakHUDPrivate
{
	FLinearColor Faded(const FLinearColor& C, float Alpha) { return FLinearColor(C.R, C.G, C.B, C.A * Alpha); }

	float Magnification(const AShieldbreakCharacter* Character)
	{
		const UCameraComponent* Camera = Character->GetFirstPersonCamera();
		const float FOV = Camera ? Camera->FieldOfView : 78.f;
		return FMath::Tan(FMath::DegreesToRadians(ShieldbreakSettings::GetFieldOfView() * 0.5f)) / FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f));
	}
}

void AShieldbreakHUD::DrawMarksmanScope(AShieldbreakCharacter* Character, float CX, float CY, float R, float Alpha, bool bOnEnemy, float RangeMeters)
{
	using namespace ShieldbreakHUDPrivate;
	const FLinearColor Housing(0.008f, 0.01f, 0.012f, Alpha);
	const FLinearColor Cyan = Faded(FLinearColor(0.45f, 0.95f, 1.f, 0.9f), Alpha);
	const FLinearColor CyanDim = Faded(FLinearColor(0.45f, 0.95f, 1.f, 0.35f), Alpha);
	const FLinearColor Reticle = bOnEnemy ? Faded(Danger, Alpha) : Faded(FLinearColor(0.8f, 1.f, 1.f, 0.95f), Alpha);
	UFont* Small = GEngine->GetSmallFont();

	DrawLensMask(CX, CY, R, Housing);
	// Faint cool tint over the glass.
	Box(CX - R * 0.7f, CY - R * 0.7f, R * 1.4f, R * 1.4f, Faded(FLinearColor(0.2f, 0.6f, 0.7f, 0.035f), Alpha));
	// Housing: a dark metal ring with a bright inner lip, and an instrument scale every 5 degrees.
	Circle(CX, CY, R + 5.f * S, Faded(FLinearColor(0.16f, 0.19f, 0.22f, 1.f), Alpha), 9.f * S, 96);
	Circle(CX, CY, R - 1.f * S, CyanDim, 1.5f * S, 96);
	for (int32 I = 0; I < 72; ++I)
	{
		const float A = FMath::DegreesToRadians(I * 5.f);
		const float Len = (I % 6 == 0 ? 14.f : 6.f) * S;
		const FVector2D D(FMath::Cos(A), FMath::Sin(A));
		Line(CX + D.X * (R - 4.f * S), CY + D.Y * (R - 4.f * S), CX + D.X * (R - 4.f * S - Len), CY + D.Y * (R - 4.f * S - Len),
			I % 6 == 0 ? Cyan : CyanDim, (I % 6 == 0 ? 2.f : 1.f) * S);
	}

	// Reticle: left, right and lower stems that thicken toward the rim (no top stem, so the target stays clear),
	// a small diamond at the centre and three holdover bars down the lower stem.
	const float Gap = 22.f * S;
	const float Mid = R * 0.55f;
	const float Outer = R - 22.f * S;
	for (const FVector2D D : { FVector2D(-1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(0.f, 1.f) })
	{
		Line(CX + D.X * Gap, CY + D.Y * Gap, CX + D.X * Mid, CY + D.Y * Mid, Reticle, 2.5f * S);
		Line(CX + D.X * Mid, CY + D.Y * Mid, CX + D.X * Outer, CY + D.Y * Outer, Reticle, 5.f * S);
	}
	const float Dm = 6.f * S;
	Line(CX, CY - Dm, CX + Dm, CY, Reticle, 2.f * S);
	Line(CX + Dm, CY, CX, CY + Dm, Reticle, 2.f * S);
	Line(CX, CY + Dm, CX - Dm, CY, Reticle, 2.f * S);
	Line(CX - Dm, CY, CX, CY - Dm, Reticle, 2.f * S);
	for (int32 I = 1; I <= 3; ++I)
	{
		const float Y = CY + Gap + I * R * 0.09f;
		const float W = (16.f - I * 3.f) * S;
		Line(CX - W, Y, CX + W, Y, Reticle, 1.5f * S);
	}
	// Chevron brackets either side of the centre, opening outward.
	for (const float Side : { -1.f, 1.f })
	{
		const float X = CX + Side * (Gap + 30.f * S);
		Line(X, CY - 10.f * S, X + Side * 10.f * S, CY, Cyan, 2.f * S);
		Line(X + Side * 10.f * S, CY, X, CY + 10.f * S, Cyan, 2.f * S);
	}

	// Readouts inside the rim: magnification and range up top right, ammo as an arc on the left.
	const float TextX = CX + R * 0.5f;
	Text(FString::Printf(TEXT("%.1fx"), Magnification(Character)), TextX, CY - R * 0.62f, Cyan, Small, 1.4f * S, true);
	Text(RangeMeters > 0.f ? FString::Printf(TEXT("RNG %d m"), FMath::RoundToInt(RangeMeters)) : FString(TEXT("RNG ---")),
		TextX, CY - R * 0.62f + 22.f * S, CyanDim.CopyWithNewOpacity(0.8f * Alpha), Small, 1.1f * S, true);
	if (const AShieldbreakWeapon* Weapon = Character->GetCurrentWeapon())
	{
		const int32 Max = FMath::Clamp(Weapon->GetStats().MagazineSize, 1, 40);
		const int32 Left = FMath::Clamp(Weapon->GetAmmoInMagazine(), 0, Max);
		const float Span = 70.f;
		const float Step = Span / Max;
		for (int32 I = 0; I < Max; ++I)
		{
			const float Angle = 180.f - Span * 0.5f + Step * (I + 0.5f);
			Arc(CX, CY, R - 30.f * S, Angle, Step * 0.6f, I < Left ? Cyan : CyanDim.CopyWithNewOpacity(0.15f * Alpha), 6.f * S);
		}
	}
}

void AShieldbreakHUD::DrawLongRangeScope(AShieldbreakCharacter* Character, float CX, float CY, float R, float Alpha, bool bOnEnemy, float RangeMeters)
{
	using namespace ShieldbreakHUDPrivate;
	const FLinearColor Housing(0.005f, 0.005f, 0.006f, Alpha);
	const FLinearColor Warm = Faded(FLinearColor(1.f, 0.82f, 0.55f, 0.92f), Alpha);
	const FLinearColor WarmDim = Faded(FLinearColor(1.f, 0.82f, 0.55f, 0.4f), Alpha);
	const FLinearColor Reticle = bOnEnemy ? Faded(Danger, Alpha) : Faded(FLinearColor(0.05f, 0.05f, 0.05f, 0.95f), Alpha);
	const FLinearColor ReticleLight = bOnEnemy ? Faded(Danger, Alpha) : Warm;
	UFont* Small = GEngine->GetSmallFont();
	const int32 Level = Character->GetZoomLevel();

	DrawLensMask(CX, CY, R, Housing);
	Circle(CX, CY, R + 6.f * S, Faded(FLinearColor(0.1f, 0.1f, 0.11f, 1.f), Alpha), 12.f * S, 128);
	Circle(CX, CY, R - 2.f * S, WarmDim.CopyWithNewOpacity(0.25f * Alpha), 1.f * S, 128);

	// Fine cross through the whole lens, with heavy posts in from the rim on all four sides (dark, so it reads on
	// bright sky; outlined warm so it reads in shadow) and a lit centre dot.
	const float Post = R * 0.62f;
	for (const FVector2D D : { FVector2D(-1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(0.f, -1.f), FVector2D(0.f, 1.f) })
	{
		Line(CX + D.X * 3.f * S, CY + D.Y * 3.f * S, CX + D.X * Post, CY + D.Y * Post, Reticle, 2.f * S);
		Line(CX + D.X * Post, CY + D.Y * Post, CX + D.X * R, CY + D.Y * R, ReticleLight, 9.f * S);
		Line(CX + D.X * Post, CY + D.Y * Post, CX + D.X * R, CY + D.Y * R, Reticle, 6.f * S);
	}
	Box(CX - 2.f * S, CY - 2.f * S, 4.f * S, 4.f * S, ReticleLight);
	// Mil ladder: ticks along the horizontal and the lower vertical, longer every fifth.
	const float Mil = R * 0.05f;
	for (int32 I = 1; I <= 11; ++I)
	{
		const float Len = (I % 5 == 0 ? 9.f : 4.f) * S;
		for (const float Side : { -1.f, 1.f })
		{
			Line(CX + Side * I * Mil, CY - Len, CX + Side * I * Mil, CY + Len, Reticle, 2.f * S);
		}
		Line(CX - Len, CY + I * Mil, CX + Len, CY + I * Mil, Reticle, 2.f * S);
	}
	// The second zoom level adds a rangefinder box around the centre.
	if (Level >= 2)
	{
		const float B = R * 0.16f;
		const float L = B * 0.35f;
		for (const FVector2D C : { FVector2D(-1.f, -1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, 1.f), FVector2D(1.f, 1.f) })
		{
			Line(CX + C.X * B, CY + C.Y * B, CX + C.X * (B - L), CY + C.Y * B, Warm, 2.f * S);
			Line(CX + C.X * B, CY + C.Y * B, CX + C.X * B, CY + C.Y * (B - L), Warm, 2.f * S);
		}
	}

	// Heading tape across the top of the lens: a tick every 5 degrees of yaw, numbered every 15.
	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerOwner->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const float Yaw = FRotator::ClampAxis(ViewRotation.Yaw);
	const float TapeY = CY - R * 0.8f;
	const float PxPerDeg = R * 0.6f / 30.f;
	for (int32 Deg = FMath::FloorToInt(Yaw / 5.f) * 5 - 30; Deg <= Yaw + 30.f; Deg += 5)
	{
		const float X = CX + (Deg - Yaw) * PxPerDeg;
		const bool bMajor = ((Deg % 15) + 15) % 15 == 0;
		Line(X, TapeY, X, TapeY + (bMajor ? 10.f : 5.f) * S, WarmDim, 1.5f * S);
		if (bMajor)
		{
			Text(FString::Printf(TEXT("%03d"), ((Deg % 360) + 360) % 360), X, TapeY - 18.f * S, WarmDim, Small, 1.f * S, true);
		}
	}
	Line(CX, TapeY + 12.f * S, CX - 5.f * S, TapeY + 20.f * S, Warm, 2.f * S);
	Line(CX, TapeY + 12.f * S, CX + 5.f * S, TapeY + 20.f * S, Warm, 2.f * S);

	// Lower readouts: range on the left, magnification and the zoom-level pips on the right.
	const float RowY = CY + R * 0.7f;
	Text(RangeMeters > 0.f ? FString::Printf(TEXT("%d m"), FMath::RoundToInt(RangeMeters)) : FString(TEXT("--- m")),
		CX - R * 0.42f, RowY, Warm, Small, 1.4f * S, true);
	Text(FString::Printf(TEXT("%.1fx"), Magnification(Character)), CX + R * 0.42f, RowY, Warm, Small, 1.4f * S, true);
	for (int32 I = 0; I < 2; ++I)
	{
		const float PX = CX + R * 0.42f - 10.f * S + I * 20.f * S;
		Box(PX - 6.f * S, RowY + 26.f * S, 12.f * S, 4.f * S, I < Level ? Warm : WarmDim.CopyWithNewOpacity(0.2f * Alpha));
	}
	if (const AShieldbreakWeapon* Weapon = Character->GetCurrentWeapon())
	{
		Text(FString::Printf(TEXT("%d / %d"), Weapon->GetAmmoInMagazine(), Weapon->GetReserveAmmo()), CX, CY + R * 0.82f, WarmDim,
			Small, 1.2f * S, true);
	}
}

void AShieldbreakHUD::DrawDamageDirection(AShieldbreakCharacter* Character, AShieldbreakPlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	const float Age = LocalTime() - PC->GetLastDamagedTime();
	if (Age > 1.f)
	{
		return;
	}

	const FVector ToSource = PC->GetLastDamageSource() - Character->GetActorLocation();
	const float SourceYaw = static_cast<float>(ToSource.Rotation().Yaw);
	const float ViewYaw = static_cast<float>(PC->GetControlRotation().Yaw);
	const float Relative = FRotator::NormalizeAxis(SourceYaw - ViewYaw);
	// Screen angle: 0 = up (forward), clockwise positive.
	const float ScreenAngle = Relative - 90.f;
	Arc(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f, 150.f * S, ScreenAngle, 40.f, FLinearColor(1.f, 0.15f, 0.1f, 1.f - Age), 6.f * S);
}

// ---- Weapon / momentum ----

void AShieldbreakHUD::DrawWeaponInfo(AShieldbreakCharacter* Character)
{
	using namespace ShieldbreakHUDPrivate;

	const float Right = Canvas->ClipX - 40.f * S;
	const float Bottom = Canvas->ClipY - 40.f * S;
	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetSmallFont();

	Box(Right - 300.f * S, Bottom - 150.f * S, 310.f * S, 160.f * S, Panel);

	if (const AShieldbreakWeapon* Weapon = Character->GetCurrentWeapon())
	{
		const FShieldbreakWeaponStats& Stats = Weapon->GetStats();
		Text(Stats.DisplayName.ToString().ToUpper(), Right, Bottom - 140.f * S, White, GEngine->GetMediumFont(), S, false, true);

		if (Stats.bUsesHeat)
		{
			const float BarW = 220.f * S;
			const float Heat = FMath::Clamp(Weapon->GetHeat(), 0.f, 1.f);
			const FLinearColor HeatColor = Weapon->IsOverheated() ? Danger : FLinearColor::LerpUsingHSV(FLinearColor(0.2f, 0.9f, 1.f), FLinearColor(1.f, 0.4f, 0.1f), Heat);
			Box(Right - BarW, Bottom - 95.f * S, BarW, 16.f * S, FLinearColor(0.1f, 0.1f, 0.1f, 0.7f));
			Box(Right - BarW, Bottom - 95.f * S, BarW * Heat, 16.f * S, HeatColor);
			if (Weapon->IsOverheated())
			{
				Text(TEXT("OVERHEATED"), Right, Bottom - 72.f * S, Danger, Small, S, false, true);
			}
		}
		else
		{
			const FLinearColor AmmoColor = Weapon->GetAmmoInMagazine() == 0 ? Danger : White;
			Text(FString::Printf(TEXT("%d / %d"), Weapon->GetAmmoInMagazine(), Weapon->GetReserveAmmo()), Right, Bottom - 105.f * S, AmmoColor, Big, S * 1.2f, false, true);
		}

		const float Reload = Weapon->GetReloadProgress();
		if (Reload >= 0.f)
		{
			const float BarW = 220.f * S;
			Box(Right - BarW, Bottom - 60.f * S, BarW, 6.f * S, FLinearColor(0.1f, 0.1f, 0.1f, 0.7f));
			Box(Right - BarW, Bottom - 60.f * S, BarW * Reload, 6.f * S, Gold);
			Text(TEXT("RELOADING"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f + 40.f * S, Gold, Small, S, true);
		}
		else if (!Stats.bUsesHeat && Weapon->GetAmmoInMagazine() == 0)
		{
			Text(Weapon->GetReserveAmmo() > 0 ? TEXT("RELOAD [R]") : TEXT("NO AMMO"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f + 40.f * S, Danger, Small, S, true);
		}
	}

	if (const AShieldbreakWeapon* Holstered = Character->GetHolsteredWeapon())
	{
		Text(FString::Printf(TEXT("[%s] %s"), *UShieldbreakInputSet::Get()->GetKeyName(TEXT("SwapWeapon")), *Holstered->GetStats().DisplayName.ToString()), Right, Bottom - 40.f * S, Dim, Small, S, false, true);
	}

	Text(FString::Printf(TEXT("GRENADES  %d"), Character->GetGrenadeCount()), Right - 290.f * S, Bottom - 40.f * S, Gold, Small, S);
}

void AShieldbreakHUD::DrawMomentum(AShieldbreakCharacter* Character)
{
	const float Speed = static_cast<float>(Character->GetVelocity().Size2D());
	if (Speed <= 650.f)
	{
		return;
	}
	const UShieldbreakMovementComponent* Movement = Character->GetShieldbreakMovement();
	const bool bCarry = Movement && Movement->IsMomentumCarrying();
	const bool bSlide = Movement && Movement->IsSliding();

	const float X = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY - 110.f * S;
	Text(FString::Printf(TEXT("%d u/s"), FMath::RoundToInt(Speed)), X, Y, FLinearColor(1.f, 1.f, 1.f, 0.8f), GEngine->GetMediumFont(), S, true);
	if (bCarry || bSlide)
	{
		Text(bCarry ? TEXT("CARRY") : TEXT("SLIDE"), X, Y + 26.f * S, ShieldbreakHUDPrivate::Gold, GEngine->GetSmallFont(), S, true);
	}
}

void AShieldbreakHUD::DrawObjectiveMarker(const FVector& WorldLocation, const FString& Label, const FLinearColor& Color)
{
	if (!PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
	const bool bInFront = FVector::DotProduct(WorldLocation - CameraLocation, CameraForward) > 0.0;
	FVector Screen = Project(WorldLocation);

	// Off-screen or behind: pin the marker to the screen edge in the right direction.
	const float Margin = 40.f * S;
	float X = static_cast<float>(Screen.X);
	float Y = static_cast<float>(Screen.Y);
	if (!bInFront)
	{
		X = Canvas->ClipX - X;
		Y = Canvas->ClipY - Margin;
	}
	X = FMath::Clamp(X, Margin, Canvas->ClipX - Margin);
	Y = FMath::Clamp(Y, Margin, Canvas->ClipY - Margin);

	const float Size = 9.f * S;
	Line(X, Y - Size, X + Size, Y, Color, 2.f * S);
	Line(X + Size, Y, X, Y + Size, Color, 2.f * S);
	Line(X, Y + Size, X - Size, Y, Color, 2.f * S);
	Line(X - Size, Y, X, Y - Size, Color, 2.f * S);

	const int32 Meters = FMath::RoundToInt(static_cast<float>(FVector::Dist(CameraLocation, WorldLocation)) / 100.f);
	Text(FString::Printf(TEXT("%s  %dm"), *Label, Meters), X, Y + 12.f * S, Color, GEngine->GetSmallFont(), S * 0.85f, true);
}

void AShieldbreakHUD::DrawObjectives(AShieldbreakCharacter* Character)
{
	const uint8 MyTeam = ShieldbreakTeams::GetTeam(Character);
	const AShieldbreakCarriable* Mine = Character->GetCarriedObjective();

	for (TActorIterator<AShieldbreakCarriable> It(GetWorld()); It; ++It)
	{
		const AShieldbreakCarriable* Objective = *It;
		if (Objective == Mine)
		{
			continue;
		}
		const AShieldbreakCharacter* Carrier = Objective->GetCarrier();
		const bool bFriendlyCarrier = Carrier && ShieldbreakTeams::AreTeammates(Carrier, Character);
		const FVector Where = Objective->GetActorLocation() + FVector(0.f, 0.f, Carrier ? 120.f : 100.f);

		if (Objective->GetKind() == EShieldbreakCarriableKind::Ball)
		{
			const FString Label = !Carrier ? TEXT("BALL") : (bFriendlyCarrier ? TEXT("PROTECT") : TEXT("KILL CARRIER"));
			DrawObjectiveMarker(Where, Label, FLinearColor(1.f, 0.9f, 0.3f));
			continue;
		}

		const bool bOurFlag = Objective->GetTeam() == MyTeam;
		const FLinearColor FlagColor = ShieldbreakTeams::GetTeamColor(Objective->GetTeam()) * 1.5f;
		FString Label;
		if (bOurFlag)
		{
			Label = Objective->IsAtHome() ? TEXT("DEFEND") : (Carrier ? TEXT("FLAG STOLEN") : TEXT("RETURN"));
		}
		else
		{
			Label = Objective->IsAtHome() ? TEXT("CAPTURE") : (Carrier ? (bFriendlyCarrier ? TEXT("ESCORT") : TEXT("FLAG")) : TEXT("TAKE"));
		}
		DrawObjectiveMarker(Where, Label, FlagColor);

		// Carrying the enemy flag: point home.
		if (Mine && bOurFlag)
		{
			DrawObjectiveMarker(Objective->GetHomeLocation() + FVector(0.f, 0.f, 140.f), TEXT("SCORE"), ShieldbreakHUDPrivate::Gold);
		}
	}

	for (TActorIterator<AShieldbreakHill> It(GetWorld()); It; ++It)
	{
		const uint8 Control = It->GetControllingTeam();
		FLinearColor HillColor(0.95f, 0.95f, 0.95f);
		FString Label = TEXT("HILL");
		if (Control == AShieldbreakHill::ContestedTeam)
		{
			HillColor = FLinearColor(0.75f, 0.4f, 1.f);
			Label = TEXT("CONTESTED");
		}
		else if (Control < ShieldbreakTeams::NumTeams)
		{
			HillColor = ShieldbreakTeams::GetTeamColor(Control) * 1.5f;
			Label = Control == MyTeam ? TEXT("HOLD") : TEXT("TAKE HILL");
		}
		DrawObjectiveMarker(It->GetActorLocation(), Label, HillColor);
	}

	if (Mine)
	{
		const bool bFlag = Mine->GetKind() == EShieldbreakCarriableKind::Flag;
		Text(bFlag ? TEXT("YOU HAVE THE FLAG  -  SIDEARM ONLY") : TEXT("YOU HAVE THE BALL  -  MELEE ONLY"),
			Canvas->ClipX * 0.5f, Canvas->ClipY - 160.f * S, ShieldbreakHUDPrivate::Gold, GEngine->GetMediumFont(), S, true);
	}
}

void AShieldbreakHUD::DrawEquipmentStatus(AShieldbreakCharacter* Character)
{
	// Bottom-left, above the motion tracker: carried equipment and active power-ups.
	float Y = Canvas->ClipY - 280.f * S;
	const float X = 40.f * S;
	UFont* Font = GEngine->GetSmallFont();
	if (Character->GetEquipment() == EShieldbreakEquipment::Aegis)
	{
		Text(FString::Printf(TEXT("AEGIS  %d%%  [Alt]"), FMath::RoundToInt(100.f * FMath::Min(Character->GetAegisHealthFraction(), Character->GetAegisUptimeFraction()))),
			X, Y, FLinearColor(0.3f, 0.8f, 1.f, 1.f), Font, S);
		Y -= 22.f * S;
	}
	const float Camo = Character->GetCamoTimeRemaining();
	if (Camo > 0.f)
	{
		Text(FString::Printf(TEXT("CAMO  %ds"), FMath::CeilToInt(Camo)), X, Y, FLinearColor(0.7f, 0.9f, 0.7f, 1.f), Font, S);
	}
}

void AShieldbreakHUD::DrawTeammateNames(AShieldbreakCharacter* Character)
{
	if (!PlayerOwner || !ShieldbreakTeams::IsTeamGame(this))
	{
		return;
	}

	if (!PlayerOwner->PlayerCameraManager)
	{
		return;
	}
	const FVector CameraLocation = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
	UFont* Font = GEngine->GetSmallFont();
	for (TActorIterator<AShieldbreakCharacter> It(GetWorld()); It; ++It)
	{
		const AShieldbreakCharacter* Other = *It;
		if (Other == Character || !Other->IsAlive() || !ShieldbreakTeams::AreTeammates(Other, Character))
		{
			continue;
		}

		const FVector HeadTop = Other->GetActorLocation() + FVector(0.f, 0.f, 120.f);
		const float Distance = static_cast<float>(FVector::Dist(CameraLocation, HeadTop));
		if (Distance > 6000.f)
		{
			continue;
		}

		if (FVector::DotProduct(HeadTop - CameraLocation, CameraForward) <= 0.0)
		{
			continue; // behind the camera
		}
		const FVector Screen = Project(HeadTop);

		const APlayerState* OtherState = Other->GetPlayerState();
		const FString Name = OtherState ? OtherState->GetPlayerName() : FString();
		const float Alpha = FMath::GetMappedRangeValueClamped(FVector2f(3000.f, 6000.f), FVector2f(1.f, 0.35f), Distance);
		const FLinearColor TeamColor = ShieldbreakTeams::GetTeamColor(ShieldbreakTeams::GetTeam(Other)) * 1.5f;
		Text(Name, static_cast<float>(Screen.X), static_cast<float>(Screen.Y), FLinearColor(TeamColor.R, TeamColor.G, TeamColor.B, Alpha), Font, S * 0.9f, true);
	}
}

void AShieldbreakHUD::DrawMotionTracker(AShieldbreakCharacter* Character)
{
	using namespace ShieldbreakHUDPrivate;

	const float Radius = 95.f * S;
	const float CX = 40.f * S + Radius;
	const float CY = Canvas->ClipY - 40.f * S - Radius;
	const FLinearColor Ring(0.35f, 0.75f, 1.f, 0.6f);

	// Translucent disc from thick concentric rings, then the frame, half-range ring and center cross.
	for (float R = 6.f * S; R < Radius; R += 10.f * S)
	{
		Circle(CX, CY, R, FLinearColor(0.f, 0.02f, 0.05f, 0.35f), 11.f * S, 40);
	}
	Circle(CX, CY, Radius, Ring, 2.f * S, 48);
	Circle(CX, CY, Radius * 0.5f, FLinearColor(Ring.R, Ring.G, Ring.B, 0.25f), 1.f * S, 32);
	Line(CX - Radius, CY, CX + Radius, CY, FLinearColor(Ring.R, Ring.G, Ring.B, 0.15f));
	Line(CX, CY - Radius, CX, CY + Radius, FLinearColor(Ring.R, Ring.G, Ring.B, 0.15f));

	// Outgoing pulse ring, like the classic tracker sweep.
	const float Pulse = FMath::Fmod(LocalTime(), 1.5f) / 1.5f;
	Circle(CX, CY, Radius * Pulse, FLinearColor(Ring.R, Ring.G, Ring.B, 0.35f * (1.f - Pulse)), 2.f * S, 40);

	// Player marker: small chevron pointing up (forward).
	Line(CX - 6.f * S, CY + 6.f * S, CX, CY - 7.f * S, ShieldBright, 2.f * S);
	Line(CX + 6.f * S, CY + 6.f * S, CX, CY - 7.f * S, ShieldBright, 2.f * S);

	const float ViewYaw = PlayerOwner ? PlayerOwner->GetControlRotation().Yaw : Character->GetActorRotation().Yaw;
	const FRotator ToView(0.f, -ViewYaw, 0.f);
	const FVector MyLocation = Character->GetActorLocation();

	for (TActorIterator<AShieldbreakCharacter> It(GetWorld()); It; ++It)
	{
		const AShieldbreakCharacter* Other = *It;
		if (Other == Character || !Other->IsAlive())
		{
			continue;
		}

		const FVector Delta = Other->GetActorLocation() - MyLocation;
		if (Delta.Size2D() > MotionTrackerRange)
		{
			continue;
		}

		// Halo rule: allies always show; enemies only when running or jumping (crouch-walking hides them).
		const bool bAlly = ShieldbreakTeams::AreTeammates(Other, Character);
		const FVector OtherVelocity = Other->GetVelocity();
		if (!bAlly && OtherVelocity.Size2D() < MotionTrackerSpeedThreshold && FMath::Abs(OtherVelocity.Z) < 150.f)
		{
			continue;
		}

		// View space: X forward maps to up on the tracker, Y right maps to right.
		const FVector Local = ToView.RotateVector(FVector(Delta.X, Delta.Y, 0.f));
		const float PX = CX + static_cast<float>(Local.Y) / MotionTrackerRange * Radius;
		const float PY = CY - static_cast<float>(Local.X) / MotionTrackerRange * Radius;

		// Enemies well above or below draw smaller so verticality reads at a glance.
		const float Size = (FMath::Abs(Delta.Z) > 250.f ? 6.f : 9.f) * S;
		Box(PX - Size * 0.5f, PY - Size * 0.5f, Size, Size, bAlly ? Gold : Danger);
	}

	Text(TEXT("25m"), CX + Radius - 4.f * S, CY + Radius - 14.f * S, Dim, GEngine->GetSmallFont(), S * 0.8f, false, true);
}

// ---- Match info ----

void AShieldbreakHUD::DrawKillFeed(AShieldbreakGameState* GameState)
{
	const float Now = ServerTime();
	float Y = 30.f * S;
	UFont* Font = GEngine->GetSmallFont();

	for (const FShieldbreakKillFeedEntry& Entry : GameState->GetKillFeed())
	{
		if (Now - Entry.ServerTime > 6.f)
		{
			continue;
		}
		const float X = 30.f * S;
		const bool bSuicide = Entry.KillerName == Entry.VictimName;
		if (Entry.bEvent)
		{
			// Objective event: "<name> <action>", or just the action for system events.
			float W = 0.f;
			float H = 0.f;
			if (!Entry.KillerName.IsEmpty())
			{
				Canvas->TextSize(Font, Entry.KillerName + TEXT(" "), W, H, S, S);
				Text(Entry.KillerName, X, Y, Entry.KillerColor * 1.4f, Font, S);
			}
			Text(Entry.VictimName, X + W, Y, Entry.KillerName.IsEmpty() ? Entry.KillerColor * 1.4f : ShieldbreakHUDPrivate::White, Font, S);
		}
		else if (bSuicide)
		{
			Text(FString::Printf(TEXT("%s  died"), *Entry.VictimName), X, Y, Entry.VictimColor * 1.4f, Font, S);
		}
		else
		{
			float W = 0.f;
			float H = 0.f;
			Canvas->TextSize(Font, Entry.KillerName, W, H, S, S);
			Text(Entry.KillerName, X, Y, Entry.KillerColor * 1.4f, Font, S);
			Text(TEXT("  >  "), X + W, Y, ShieldbreakHUDPrivate::Dim, Font, S);
			float ArrowW = 0.f;
			Canvas->TextSize(Font, TEXT("  >  "), ArrowW, H, S, S);
			Text(Entry.VictimName, X + W + ArrowW, Y, Entry.VictimColor * 1.4f, Font, S);
		}
		Y += 22.f * S;
	}
}

void AShieldbreakHUD::DrawTopScores(AShieldbreakGameState* GameState)
{
	const TArray<APlayerState*> Sorted = GameState->GetSortedPlayers();
	const APlayerState* Mine = PlayerOwner ? PlayerOwner->PlayerState.Get() : nullptr;
	UFont* Font = GEngine->GetSmallFont();
	const float Right = Canvas->ClipX - 30.f * S;
	float Y = 30.f * S;

	if (GameState->IsTeamGame())
	{
		const TCHAR* ModeName = TEXT("TEAM SLAYER");
		switch (GameState->GetGameType())
		{
		case EShieldbreakGameType::CaptureTheFlag: ModeName = TEXT("CAPTURE THE FLAG"); break;
		case EShieldbreakGameType::KingOfTheHill: ModeName = TEXT("KING OF THE HILL"); break;
		case EShieldbreakGameType::Oddball: ModeName = TEXT("ODDBALL"); break;
		default: break;
		}
		Text(FString::Printf(TEXT("%s  -  %s  -  FIRST TO %d"), *GameState->GetMapId().ToString().ToUpper(), ModeName, GameState->GetScoreLimit()), Right, Y, ShieldbreakHUDPrivate::Dim, Font, S, false, true);
		Y += 26.f * S;

		// Your team first, Halo-style.
		const uint8 MyTeam = ShieldbreakTeams::GetTeam(Mine);
		const uint8 First = MyTeam == ShieldbreakTeams::Blue ? ShieldbreakTeams::Blue : ShieldbreakTeams::Red;
		const uint8 Second = First == ShieldbreakTeams::Red ? ShieldbreakTeams::Blue : ShieldbreakTeams::Red;
		for (const uint8 Team : { First, Second })
		{
			const float BarWidth = 220.f * S;
			const float Fill = FMath::Clamp(static_cast<float>(GameState->GetTeamScore(Team)) / FMath::Max(1, GameState->GetScoreLimit()), 0.f, 1.f);
			const FLinearColor TeamColor = ShieldbreakTeams::GetTeamColor(Team);
			Box(Right - BarWidth, Y, BarWidth, 22.f * S, ShieldbreakHUDPrivate::Panel);
			Box(Right - BarWidth, Y, BarWidth * Fill, 22.f * S, FLinearColor(TeamColor.R, TeamColor.G, TeamColor.B, 0.75f));
			const FString Label = FString::Printf(TEXT("%s%s   %d"), Team == MyTeam ? TEXT("> ") : TEXT(""), *ShieldbreakTeams::GetTeamName(Team).ToUpper(), GameState->GetTeamScore(Team));
			Text(Label, Right - 6.f * S, Y + 3.f * S, ShieldbreakHUDPrivate::White, Font, S, false, true);
			Y += 28.f * S;
		}
		return;
	}

	Text(FString::Printf(TEXT("SLAYER  -  FIRST TO %d"), GameState->GetScoreLimit()), Right, Y, ShieldbreakHUDPrivate::Dim, Font, S, false, true);
	Y += 24.f * S;

	int32 MyRank = INDEX_NONE;
	for (int32 Index = 0; Index < Sorted.Num(); ++Index)
	{
		const AShieldbreakPlayerState* Entry = Cast<AShieldbreakPlayerState>(Sorted[Index]);
		if (!Entry)
		{
			continue;
		}
		if (Entry == Mine)
		{
			MyRank = Index;
		}
		if (Index < 4)
		{
			const bool bMe = Entry == Mine;
			const FString RowText = FString::Printf(TEXT("%d. %s   %d"), Index + 1, *Entry->GetPlayerName(), Entry->GetKills());
			Text(RowText, Right, Y, bMe ? ShieldbreakHUDPrivate::Gold : Entry->GetPlayerColor() * 1.4f, Font, S, false, true);
			Y += 20.f * S;
		}
	}

	if (MyRank >= 4)
	{
		if (const AShieldbreakPlayerState* MyState = Cast<AShieldbreakPlayerState>(Mine))
		{
			Text(FString::Printf(TEXT("%d. %s   %d"), MyRank + 1, *MyState->GetPlayerName(), MyState->GetKills()), Right, Y + 4.f * S, ShieldbreakHUDPrivate::Gold, Font, S, false, true);
		}
	}
}

void AShieldbreakHUD::DrawScoreboard(AShieldbreakGameState* GameState)
{
	using namespace ShieldbreakHUDPrivate;

	const TArray<APlayerState*> Sorted = GameState->GetSortedPlayers();
	const APlayerState* Mine = PlayerOwner ? PlayerOwner->PlayerState.Get() : nullptr;
	UFont* Font = GEngine->GetMediumFont();

	const float W = 720.f * S;
	const float RowH = 30.f * S;
	const float H = (Sorted.Num() + 2) * RowH + 20.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y0 = (Canvas->ClipY - H) * 0.5f;

	Box(X, Y0, W, H, FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Text(TEXT("PLAYER"), X + 60.f * S, Y0 + 10.f * S, Dim, Font, S);
	Text(TEXT("MEDALS"), X + W - 330.f * S, Y0 + 10.f * S, Dim, Font, S, false, true);
	Text(TEXT("KILLS"), X + W - 200.f * S, Y0 + 10.f * S, Dim, Font, S, false, true);
	Text(TEXT("DEATHS"), X + W - 70.f * S, Y0 + 10.f * S, Dim, Font, S, false, true);

	float Y = Y0 + 10.f * S + RowH * 1.3f;
	for (int32 Index = 0; Index < Sorted.Num(); ++Index)
	{
		const AShieldbreakPlayerState* Entry = Cast<AShieldbreakPlayerState>(Sorted[Index]);
		if (!Entry)
		{
			continue;
		}
		if (Entry == Mine)
		{
			Box(X + 10.f * S, Y - 3.f * S, W - 20.f * S, RowH, FLinearColor(1.f, 0.8f, 0.2f, 0.15f));
		}
		Box(X + 24.f * S, Y + 4.f * S, 18.f * S, 18.f * S, Entry->GetPlayerColor());
		Text(FString::Printf(TEXT("%d  %s"), Index + 1, *Entry->GetPlayerName()), X + 60.f * S, Y, White, Font, S);
		Text(FString::FromInt(Entry->GetMedalCount()), X + W - 330.f * S, Y, Dim, Font, S, false, true);
		Text(FString::FromInt(Entry->GetKills()), X + W - 200.f * S, Y, White, Font, S, false, true);
		Text(FString::FromInt(Entry->GetDeaths()), X + W - 70.f * S, Y, White, Font, S, false, true);
		Y += RowH;
	}
}

void AShieldbreakHUD::DrawMatchOver(AShieldbreakGameState* GameState)
{
	// The menu theme, a little quieter, over the results (starts once per match world).
	ShieldbreakMusic::PlayMenuMusic(this, 0.7f, 3.f);

	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.28f;
	Box(0.f, Y - 20.f * S, Canvas->ClipX, 130.f * S, FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Text(FString::Printf(TEXT("%s WINS"), *GameState->GetWinnerName().ToUpper()), CX, Y, ShieldbreakHUDPrivate::Gold, GEngine->GetLargeFont(), S * 2.f, true);
	Text(FString::Printf(TEXT("Next match in %d"), FMath::CeilToInt(GameState->GetMatchRestartCountdown())), CX, Y + 70.f * S,
		ShieldbreakHUDPrivate::White, GEngine->GetMediumFont(), S, true);
}

void AShieldbreakHUD::DrawRespawn(AShieldbreakPlayerController* PC, AShieldbreakGameState* GameState)
{
	if (!PC || PC->GetRespawnServerTime() < 0.f)
	{
		return;
	}
	const APawn* Pawn = PC->GetPawn();
	const AShieldbreakCharacter* Character = Cast<AShieldbreakCharacter>(Pawn);
	if (Character && Character->IsAlive())
	{
		return;
	}
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(PC->GetRespawnServerTime() - ServerTime()));
	Text(FString::Printf(TEXT("RESPAWN IN %d"), Seconds), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.62f,
		ShieldbreakHUDPrivate::White, GEngine->GetLargeFont(), S * 1.4f, true);
}

void AShieldbreakHUD::DrawMedals(const AShieldbreakPlayerController* PC)
{
	using namespace ShieldbreakHUDPrivate;
	const UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}
	constexpr float Lifetime = 3.f;
	const float Now = World->GetTimeSeconds();
	const TArray<AShieldbreakPlayerController::FMedalPopup>& Popups = PC->GetMedalPopups();
	float Y = Canvas->ClipY * 0.66f;
	for (int32 Index = Popups.Num() - 1; Index >= 0; --Index)
	{
		const float Age = Now - Popups[Index].Time;
		if (Age < 0.f || Age > Lifetime)
		{
			continue;
		}
		const EShieldbreakMedal Medal = static_cast<EShieldbreakMedal>(Popups[Index].Medal);
		const bool bBig = ShieldbreakMedals::IsBig(Medal);
		// Pop in (overshoot), hold, fade out.
		const float Pop = Age < 0.15f ? FMath::Lerp(1.6f, 1.f, Age / 0.15f) : 1.f;
		const float Alpha = FMath::Clamp((Lifetime - Age) / 0.5f, 0.f, 1.f);
		const FLinearColor Color = (bBig ? Gold : White).CopyWithNewOpacity(Alpha);
		Text(ShieldbreakMedals::GetName(Medal), Canvas->ClipX * 0.5f, Y, Color, GEngine->GetLargeFont(), S * (bBig ? 1.3f : 0.95f) * Pop, true);
		Y -= (bBig ? 44.f : 34.f) * S;
	}
}

void AShieldbreakHUD::DrawTips(const AShieldbreakCharacter* Character)
{
	using namespace ShieldbreakHUDPrivate;

	// Once per launch; tests and automated runs skip them so screenshots stay clean.
	static bool bShownThisLaunch = false;
	static const bool bAutomated = FParse::Param(FCommandLine::Get(), TEXT("SBSelfTest")) || FParse::Param(FCommandLine::Get(), TEXT("SBNoMenu"))
		|| FParse::Param(FCommandLine::Get(), TEXT("SBShots")) || FParse::Param(FCommandLine::Get(), TEXT("SBAutoShot"));
	const UWorld* World = GetWorld();
	if (bAutomated || !World || !ShieldbreakSettings::GetShowTips())
	{
		return;
	}
	if (TipsStartTime < 0.f)
	{
		if (bShownThisLaunch || !Character || !Character->IsAlive())
		{
			return;
		}
		bShownThisLaunch = true;
		// A short beat after the first spawn so the player gets their bearings.
		TipsStartTime = World->GetTimeSeconds() + 3.f;
	}

	const UShieldbreakInputSet* Inputs = UShieldbreakInputSet::Get();
	auto K = [Inputs](const TCHAR* Id) { return Inputs->GetKeyName(Id); };
	const TArray<FString> Tips =
	{
		FString::Printf(TEXT("Your shield soaks damage and recharges when you stop getting hit. Break it, then finish with a headshot or melee [%s]."), *K(TEXT("Melee"))),
		FString::Printf(TEXT("[%s] throws a grenade. They bounce: bank them off walls and around corners."), *K(TEXT("Grenade"))),
		FString::Printf(TEXT("Shield recharging? Tap [%s] in the green window to vent for an instant refill. Miss and it restarts."), *K(TEXT("ReloadVent"))),
		FString::Printf(TEXT("Sprint [%s], then crouch [%s] to slide. Jump out of a slide to keep the speed."), *K(TEXT("Sprint")), *K(TEXT("CrouchSlide"))),
		FString::Printf(TEXT("[%s] picks up weapons on the map. [%s] swaps between your two guns."), *K(TEXT("Interact")), *K(TEXT("SwapWeapon"))),
		FString::Printf(TEXT("Power weapons respawn on a timer: control them to win. [%s] lists every control; tips can be turned off in the Esc menu."), *K(TEXT("ToggleHelp"))),
	};

	constexpr float TipSeconds = 7.f;
	const float Elapsed = World->GetTimeSeconds() - TipsStartTime;
	const int32 Index = FMath::FloorToInt(Elapsed / TipSeconds);
	if (Elapsed < 0.f || !Tips.IsValidIndex(Index))
	{
		return;
	}
	const float Local = Elapsed - Index * TipSeconds;
	const float Alpha = FMath::Clamp(FMath::Min(Local / 0.4f, (TipSeconds - Local) / 0.6f), 0.f, 1.f);

	UFont* Font = GEngine->GetSmallFont();
	const float W = 1000.f * S;
	const float H = 58.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = Canvas->ClipY * 0.17f;
	Box(X, Y, W, H, FLinearColor(0.f, 0.f, 0.f, 0.55f * Alpha));
	Text(FString::Printf(TEXT("TIP %d/%d"), Index + 1, Tips.Num()), Canvas->ClipX * 0.5f, Y + 6.f * S, Gold.CopyWithNewOpacity(Alpha), Font, S, true);
	Text(Tips[Index], Canvas->ClipX * 0.5f, Y + 28.f * S, White.CopyWithNewOpacity(Alpha), Font, S, true);
}

void AShieldbreakHUD::DrawHelp()
{
	using namespace ShieldbreakHUDPrivate;

	const UShieldbreakInputSet* Inputs = UShieldbreakInputSet::Get();
	auto K = [Inputs](const TCHAR* Id) { return Inputs->GetKeyName(Id); };
	const FString Move = K(TEXT("MoveForward")) + K(TEXT("MoveLeft")) + K(TEXT("MoveBack")) + K(TEXT("MoveRight"));

	const TArray<FString> Lines =
	{
		FString::Printf(TEXT("SHIELDBREAK  -  CONTROLS   (%s to toggle, rebind in Esc > Controls)"), *K(TEXT("ToggleHelp"))),
		TEXT(""),
		FString::Printf(TEXT("%s move    Mouse look    %s jump    %s sprint    %s crouch"),
			*Move, *K(TEXT("Jump")), *K(TEXT("Sprint")), *K(TEXT("CrouchSlide"))),
		FString::Printf(TEXT("%s fire    %s zoom    %s reload / vent    %s swap    %s pick up"),
			*K(TEXT("Fire")), *K(TEXT("Zoom")), *K(TEXT("ReloadVent")), *K(TEXT("SwapWeapon")), *K(TEXT("Interact"))),
		FString::Printf(TEXT("%s melee    %s grenade    %s use equipment (Aegis)    %s scores    Esc menu"),
			*K(TEXT("Melee")), *K(TEXT("Grenade")), *K(TEXT("Equipment")), *K(TEXT("Scoreboard"))),
		TEXT("Console (~):  SB_Map <Keel|Bastion|Sanctum|Creekbed|Spire> <Slayer|CTF|KOTH|Oddball>    SB_Bots <n>    SB_Skill <0-3>"),
		TEXT(""),
		TEXT("MASTERY"),
		FString::Printf(TEXT("Aegis Shield (map item, hold %s): frontal barrier that stops bullets, not explosives or melee. No firing while up."), *K(TEXT("Equipment"))),
		TEXT("Rail Lance (map weapon): hold fire to charge, release at full charge (gold ring) for a one-shot kill."),
		FString::Printf(TEXT("Overcharge Vent (%s while recharging): tap in the green window for an instant refill + overshield. Miss = restart."), *K(TEXT("ReloadVent"))),
		FString::Printf(TEXT("Break Pulse (%s the instant your shield pops): shockwave that knocks back and strips nearby shields."), *K(TEXT("Melee"))),
		TEXT("Momentum Carry (sprint > crouch to slide > jump): keep slide speed in the air. Downhill slides go faster."),
		FString::Printf(TEXT("Power Jump: crouch, hold %s, release: up to 1.5x height. Slow while charging."), *K(TEXT("Jump"))),
		TEXT("Motion Tracker (bottom left): enemies within 25 m show when they move fast. Crouch-walk to stay hidden."),
		TEXT(""),
		TEXT("Console: SB_Bots N    SB_Skill 0-3"),
	};

	UFont* Font = GEngine->GetSmallFont();
	const float W = 900.f * S;
	const float LineH = 22.f * S;
	const float H = Lines.Num() * LineH + 30.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y0 = Canvas->ClipY - H - 190.f * S;

	Box(X, Y0, W, H, FLinearColor(0.f, 0.f, 0.f, 0.6f));
	float Y = Y0 + 15.f * S;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		const bool bHeader = Index == 0 || Lines[Index] == TEXT("MASTERY");
		Text(Lines[Index], X + 20.f * S, Y, bHeader ? Gold : White, Font, S);
		Y += LineH;
	}
}
