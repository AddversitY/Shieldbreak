// Canvas-only HUD: shield bar with the vent sweep, health pips, mastery prompts, crosshair and
// hit markers, damage direction, weapon/ammo, momentum readout, kill feed, scores and overlays.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ShieldbreakHUD.generated.h"

class AShieldbreakCharacter;
class AShieldbreakGameState;
class AShieldbreakPlayerController;
class UFont;
class UShieldHealthComponent;

UCLASS()
class SHIELDBREAK_API AShieldbreakHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Motion tracker radius in world units (25 m). */
	UPROPERTY(EditAnywhere, Category = "Motion Tracker")
	float MotionTrackerRange = 2500.f;

	/** Enemies moving slower than this (crouch-walking or standing still) stay off the tracker. */
	UPROPERTY(EditAnywhere, Category = "Motion Tracker")
	float MotionTrackerSpeedThreshold = 300.f;

private:
	// Sections.
	void DrawShieldAndHealth(AShieldbreakCharacter* Character);
	void DrawCrosshair(AShieldbreakCharacter* Character, AShieldbreakPlayerController* PC);
	void DrawDamageDirection(AShieldbreakCharacter* Character, AShieldbreakPlayerController* PC);
	void DrawWeaponInfo(AShieldbreakCharacter* Character);
	void DrawMomentum(AShieldbreakCharacter* Character);
	void DrawMotionTracker(AShieldbreakCharacter* Character);
	void DrawKillFeed(AShieldbreakGameState* GameState);
	void DrawTopScores(AShieldbreakGameState* GameState);
	void DrawScoreboard(AShieldbreakGameState* GameState);
	void DrawMatchOver(AShieldbreakGameState* GameState);
	void DrawRespawn(AShieldbreakPlayerController* PC, AShieldbreakGameState* GameState);
	void DrawHelp();

	// Primitives (all coordinates in pixels).
	void Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float TextScale = 1.f, bool bCenter = false, bool bRight = false);
	void Box(float X, float Y, float W, float H, const FLinearColor& Color);
	void Line(float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness = 1.f);
	void Circle(float CX, float CY, float Radius, const FLinearColor& Color, float Thickness = 1.f, int32 Segments = 32);
	void Arc(float CX, float CY, float Radius, float CenterAngleDeg, float SpanDeg, const FLinearColor& Color, float Thickness);

	float LocalTime() const;
	float ServerTime() const;
	void BindShieldEvents(UShieldHealthComponent* Shield);

	UFUNCTION() void HandleVentResult(bool bSuccess);

	TWeakObjectPtr<UShieldHealthComponent> BoundShield;
	float VentResultTime = -100.f;
	bool bVentSucceeded = false;

	/** Pixel scale relative to 1080p. */
	float S = 1.f;
};
