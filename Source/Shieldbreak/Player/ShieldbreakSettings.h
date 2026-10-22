// Per-player view and aim settings (mouse/stick sensitivity, field of view, invert look). Stored in
// GameUserSettings.ini [Shieldbreak.Settings]; edited from the main menu and the Esc menu.

#pragma once

#include "CoreMinimal.h"

namespace ShieldbreakSettings
{
	constexpr float MinSensitivity = 0.2f;
	constexpr float MaxSensitivity = 3.f;
	constexpr float MinFieldOfView = 70.f;
	constexpr float MaxFieldOfView = 110.f;
	constexpr float DefaultFieldOfView = 78.f;

	/** Multiplier on look input; 1 is the tuned default. */
	SHIELDBREAK_API float GetSensitivity();
	SHIELDBREAK_API void SetSensitivity(float Value);
	/** Horizontal field of view (hip fire; scopes keep their own zoom). */
	SHIELDBREAK_API float GetFieldOfView();
	SHIELDBREAK_API void SetFieldOfView(float Value);
	SHIELDBREAK_API bool GetInvertLook();
	SHIELDBREAK_API void SetInvertLook(bool bInvert);
}
