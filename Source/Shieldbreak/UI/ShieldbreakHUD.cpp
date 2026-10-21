#include "UI/ShieldbreakHUD.h"

#include "Game/ShieldbreakTeams.h"

#include "Shieldbreak.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/ShieldbreakGameState.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShieldbreakCharacter.h"
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
		DrawShieldAndHealth(Character);
		DrawDamageDirection(Character, PC);
		DrawCrosshair(Character, PC);
		DrawWeaponInfo(Character);
		DrawMomentum(Character);
		DrawMotionTracker(Character);
		DrawTeammateNames(Character);
		DrawEquipmentStatus(Character);
	}

	if (GameState)
	{
		DrawKillFeed(GameState);
		DrawTopScores(GameState);
	}

	DrawRespawn(PC, GameState);

	if (GameState && GameState->IsMatchOver())
	{
		DrawMatchOver(GameState);
	}
	if (PC && PC->IsScoreboardVisible() && GameState)
	{
		DrawScoreboard(GameState);
	}
	if (PC && PC->IsHelpVisible())
	{
		DrawHelp();
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
		Text(TEXT("[R] VENT"), SweetX + SweetW * 0.5f, BarY + BarH + 10.f * S, Green, GEngine->GetSmallFont(), S, true);
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
		Text(TEXT("PULSE [F]"), CX, CY + 60.f * S, FLinearColor(0.4f, 0.75f, 1.f, Alpha), GEngine->GetLargeFont(), S, true);
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
	if (Weapon && GetWorld())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		PlayerOwner->GetPlayerViewPoint(ViewLocation, ViewRotation);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakReticle), true, Character);
		if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * Weapon->GetStats().Range, ECC_Weapon, Params))
		{
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
	Box(CX - 2.f * S, CY - 2.f * S, 4.f * S, 4.f * S, ReticleColor);
	Circle(CX, CY, RingRadius + 6.f * S, ReticleColor, 1.5f * S, 28);

	// Hit marker.
	if (PC && Now - PC->GetLastHitConfirmTime() < 0.2f)
	{
		const bool bKill = PC->WasLastHitKill();
		const float Inner = (bKill ? 10.f : 7.f) * S;
		const float Outer = (bKill ? 22.f : 15.f) * S;
		const FLinearColor MarkColor = bKill ? Danger : White;
		const float Thick = (bKill ? 3.f : 2.f) * S;
		for (const FVector2D Dir : { FVector2D(1.f, 1.f), FVector2D(-1.f, 1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, -1.f) })
		{
			const FVector2D N = Dir.GetSafeNormal();
			Line(CX + N.X * Inner, CY + N.Y * Inner, CX + N.X * Outer, CY + N.Y * Outer, MarkColor, Thick);
		}
	}
	if (PC && PC->WasLastHitHeadshot() && PC->WasLastHitKill() && Now - PC->GetLastHitConfirmTime() < 1.f)
	{
		Text(TEXT("HEADSHOT"), CX, CY - 70.f * S, Gold, GEngine->GetMediumFont(), S, true);
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
		Text(FString::Printf(TEXT("[Q] %s"), *Holstered->GetStats().DisplayName.ToString()), Right, Bottom - 40.f * S, Dim, Small, S, false, true);
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
		if (bSuicide)
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
		Text(FString::Printf(TEXT("TEAM SLAYER  -  FIRST TO %d"), GameState->GetScoreLimit()), Right, Y, ShieldbreakHUDPrivate::Dim, Font, S, false, true);
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

	const float W = 640.f * S;
	const float RowH = 30.f * S;
	const float H = (Sorted.Num() + 2) * RowH + 20.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y0 = (Canvas->ClipY - H) * 0.5f;

	Box(X, Y0, W, H, FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Text(TEXT("PLAYER"), X + 60.f * S, Y0 + 10.f * S, Dim, Font, S);
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
		Text(FString::FromInt(Entry->GetKills()), X + W - 200.f * S, Y, White, Font, S, false, true);
		Text(FString::FromInt(Entry->GetDeaths()), X + W - 70.f * S, Y, White, Font, S, false, true);
		Y += RowH;
	}
}

void AShieldbreakHUD::DrawMatchOver(AShieldbreakGameState* GameState)
{
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

void AShieldbreakHUD::DrawHelp()
{
	using namespace ShieldbreakHUDPrivate;

	static const TCHAR* const Lines[] =
	{
		TEXT("SHIELDBREAK  -  CONTROLS   (F1 to toggle)"),
		TEXT(""),
		TEXT("WASD move    Mouse look    Space jump    Shift sprint    Ctrl/C crouch"),
		TEXT("LMB fire    RMB zoom    R reload / vent    Q swap    E pick up"),
		TEXT("F melee    G grenade    Alt / Mouse4 use equipment (Aegis)    Tab scores"),
		TEXT(""),
		TEXT("MASTERY"),
		TEXT("Aegis Shield (map item, hold Alt): frontal barrier that stops bullets, not explosives or melee. No firing while up."),
		TEXT("Rail Lance (map weapon): hold fire to charge, release at full charge (gold ring) for a one-shot kill."),
		TEXT("Overcharge Vent (R while recharging): tap in the green window for an instant refill + overshield. Miss = restart."),
		TEXT("Break Pulse (F the instant your shield pops): shockwave that knocks back and strips nearby shields."),
		TEXT("Momentum Carry (sprint > crouch to slide > jump): keep slide speed in the air. Downhill slides go faster."),
		TEXT("Power Jump: crouch, hold Space, release: up to 1.5x height. Slow while charging."),
		TEXT("Motion Tracker (bottom left): enemies within 25 m show when they move fast. Crouch-walk to stay hidden."),
		TEXT(""),
		TEXT("Console: SB_Bots N    SB_Skill 0-3"),
	};

	UFont* Font = GEngine->GetSmallFont();
	const float W = 900.f * S;
	const float LineH = 22.f * S;
	const float H = UE_ARRAY_COUNT(Lines) * LineH + 30.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y0 = Canvas->ClipY - H - 190.f * S;

	Box(X, Y0, W, H, FLinearColor(0.f, 0.f, 0.f, 0.6f));
	float Y = Y0 + 15.f * S;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Lines); ++Index)
	{
		const bool bHeader = Index == 0 || FCString::Strcmp(Lines[Index], TEXT("MASTERY")) == 0;
		Text(Lines[Index], X + 20.f * S, Y, bHeader ? Gold : White, Font, S);
		Y += LineH;
	}
}
