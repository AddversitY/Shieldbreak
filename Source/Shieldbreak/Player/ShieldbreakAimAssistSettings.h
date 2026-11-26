#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ShieldbreakAimAssistSettings.generated.h"

/** Aim assist for one input type (controller or mouse). See docs/AIM_ASSIST.md. */
USTRUCT()
struct SHIELDBREAK_API FShieldbreakAimAssistProfile
{
	GENERATED_BODY()

	/** Overall multiplier on this input's assist: how deep the slowdown goes, how fast the view follows and how far shots
	 *  bend. 0 turns aim assist off for this input; 1 is the tuned default. Clamped to 0..1.5. */
	UPROPERTY(Config, EditAnywhere, Category = "Aim Assist") float Strength = 1.f;
	/** Reticle friction: look speed multiplier with the crosshair dead on an enemy (1 = no slowdown). */
	UPROPERTY(Config, EditAnywhere, Category = "Aim Assist") float Friction = 0.45f;
	/** Magnetism: how fast the view follows an enemy across the crosshair, in degrees per second at full strength. Only
	 *  while the player is aiming or moving; it never moves an idle player's view. */
	UPROPERTY(Config, EditAnywhere, Category = "Aim Assist") float PullDegreesPerSecond = 9.f;
	/** Bullet magnetism: scale on each weapon's MagnetismDegrees (0..1; the server's shot check allows the weapon's full
	 *  cone, so it can't go higher). */
	UPROPERTY(Config, EditAnywhere, Category = "Aim Assist") float BulletMagnetism = 1.f;
};

/**
 * Shieldbreak aim assist tuning (Project Settings > Game > Shieldbreak Aim Assist, stored in DefaultGame.ini).
 * Every profile value can also be overridden live with a console variable (sb.AimAssist.ControllerStrength,
 * sb.AimAssist.MouseFriction, ...; -1 = use this config). sb.AimAssist 0 turns aim assist off entirely.
 * Human players only: bots never get any of it.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Shieldbreak Aim Assist"))
class SHIELDBREAK_API UShieldbreakAimAssistSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UShieldbreakAimAssistSettings();

	/** Right stick. Classic console assist: a firm slowdown, a gentle follow and the weapons' full bullet magnetism. */
	UPROPERTY(Config, EditAnywhere, Category = "Profiles") FShieldbreakAimAssistProfile Controller;
	/** Mouse: the same three effects, lighter, so flicks still feel 1:1. */
	UPROPERTY(Config, EditAnywhere, Category = "Profiles") FShieldbreakAimAssistProfile Mouse;

	/** Half-width of the assist cone at the target (cm): about a body's width either side of the chest. */
	UPROPERTY(Config, EditAnywhere, Category = "Targeting") float ConeRadius = 90.f;
	/** No friction or magnetism beyond this range (or the weapon's own range, if shorter). */
	UPROPERTY(Config, EditAnywhere, Category = "Targeting") float MaxRange = 6000.f;
	/** Full strength out to this range, then fading linearly to FarStrength at MaxRange. */
	UPROPERTY(Config, EditAnywhere, Category = "Targeting") float FullStrengthRange = 1500.f;
	UPROPERTY(Config, EditAnywhere, Category = "Targeting") float FarStrength = 0.35f;
	/** Seconds after the last stick input that a player still counts as on a controller. */
	UPROPERTY(Config, EditAnywhere, Category = "Targeting") float GamepadMemorySeconds = 2.f;
	/** Magnetism follows only within this many seconds of look input (or while moving). */
	UPROPERTY(Config, EditAnywhere, Category = "Targeting") float ActiveInputSeconds = 0.15f;

	static const UShieldbreakAimAssistSettings& Get() { return *GetDefault<UShieldbreakAimAssistSettings>(); }
	/** The values in effect for one input type: the config above with any sb.AimAssist.* console overrides applied. */
	static FShieldbreakAimAssistProfile GetProfile(bool bController);
	/** 0..1 range factor: 1 out to FullStrengthRange, FarStrength at MaxRange. */
	float RangeFactor(float Distance) const;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
