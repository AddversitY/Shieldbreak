#include "Player/ShieldbreakAimAssistSettings.h"

#include "HAL/IConsoleManager.h"

namespace ShieldbreakAimAssistPrivate
{
	// Live overrides of the config (Project Settings > Game > Shieldbreak Aim Assist). -1 = use the config value.
	TAutoConsoleVariable<float> CVarControllerStrength(TEXT("sb.AimAssist.ControllerStrength"), -1.f, TEXT("Controller aim assist strength (0 = off, 1 = default). -1: config."));
	TAutoConsoleVariable<float> CVarControllerFriction(TEXT("sb.AimAssist.ControllerFriction"), -1.f, TEXT("Controller look speed multiplier on an enemy. -1: config."));
	TAutoConsoleVariable<float> CVarControllerPull(TEXT("sb.AimAssist.ControllerPull"), -1.f, TEXT("Controller magnetism, degrees per second. -1: config."));
	TAutoConsoleVariable<float> CVarControllerBullet(TEXT("sb.AimAssist.ControllerBullet"), -1.f, TEXT("Controller bullet magnetism scale (0..1). -1: config."));
	TAutoConsoleVariable<float> CVarMouseStrength(TEXT("sb.AimAssist.MouseStrength"), -1.f, TEXT("Mouse aim assist strength (0 = off, 1 = default). -1: config."));
	TAutoConsoleVariable<float> CVarMouseFriction(TEXT("sb.AimAssist.MouseFriction"), -1.f, TEXT("Mouse look speed multiplier on an enemy. -1: config."));
	TAutoConsoleVariable<float> CVarMousePull(TEXT("sb.AimAssist.MousePull"), -1.f, TEXT("Mouse magnetism, degrees per second. -1: config."));
	TAutoConsoleVariable<float> CVarMouseBullet(TEXT("sb.AimAssist.MouseBullet"), -1.f, TEXT("Mouse bullet magnetism scale (0..1). -1: config."));

	void Override(float& Value, const TAutoConsoleVariable<float>& CVar)
	{
		const float Set = CVar.GetValueOnGameThread();
		if (Set >= 0.f)
		{
			Value = Set;
		}
	}
}

UShieldbreakAimAssistSettings::UShieldbreakAimAssistSettings()
{
	// Defaults (docs/AIM_ASSIST.md). The mouse gets the same effects, lighter: a gentle drag, a slow follow and about
	// two thirds of each weapon's bullet magnetism.
	Controller.Strength = 1.f;
	Controller.Friction = 0.45f;
	Controller.PullDegreesPerSecond = 9.f;
	Controller.BulletMagnetism = 1.f;
	Mouse.Strength = 1.f;
	Mouse.Friction = 0.75f;
	Mouse.PullDegreesPerSecond = 5.f;
	Mouse.BulletMagnetism = 0.6f;
}

FShieldbreakAimAssistProfile UShieldbreakAimAssistSettings::GetProfile(bool bController)
{
	using namespace ShieldbreakAimAssistPrivate;
	FShieldbreakAimAssistProfile Profile = bController ? Get().Controller : Get().Mouse;
	Override(Profile.Strength, bController ? CVarControllerStrength : CVarMouseStrength);
	Override(Profile.Friction, bController ? CVarControllerFriction : CVarMouseFriction);
	Override(Profile.PullDegreesPerSecond, bController ? CVarControllerPull : CVarMousePull);
	Override(Profile.BulletMagnetism, bController ? CVarControllerBullet : CVarMouseBullet);
	Profile.Strength = FMath::Clamp(Profile.Strength, 0.f, 1.5f);
	Profile.Friction = FMath::Clamp(Profile.Friction, 0.f, 1.f);
	Profile.PullDegreesPerSecond = FMath::Max(Profile.PullDegreesPerSecond, 0.f);
	Profile.BulletMagnetism = FMath::Clamp(Profile.BulletMagnetism, 0.f, 1.f);
	return Profile;
}

float UShieldbreakAimAssistSettings::RangeFactor(float Distance) const
{
	if (Distance <= FullStrengthRange || MaxRange <= FullStrengthRange)
	{
		return 1.f;
	}
	const float T = FMath::Clamp((Distance - FullStrengthRange) / (MaxRange - FullStrengthRange), 0.f, 1.f);
	return FMath::Lerp(1.f, FarStrength, T);
}
