// Per-player view and aim settings (mouse/stick sensitivity, field of view, invert look). Stored in
// GameUserSettings.ini [Shieldbreak.Settings]; graphics quality is the engine's own scalability level. Edited from the main menu and the Esc menu.

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
	/** New-player tips at the start of the first match of each launch (on by default). */
	SHIELDBREAK_API bool GetShowTips();
	SHIELDBREAK_API void SetShowTips(bool bShow);
	/** Music volume, 0 to 1 (default 0.6). */
	SHIELDBREAK_API float GetMusicVolume();
	SHIELDBREAK_API void SetMusicVolume(float Volume);

	/** Overall engine scalability, 0 (Low) to 3 (Epic); -1 when individual groups were set by hand. Stored by
	 *  UGameUserSettings (GameUserSettings.ini [ScalabilityGroups]), applied at once. */
	SHIELDBREAK_API int32 GetGraphicsQuality();
	SHIELDBREAK_API void SetGraphicsQuality(int32 Level);
	SHIELDBREAK_API FText GetGraphicsQualityName(int32 Level);

	/** 3D render resolution in percent of the window (UI stays sharp). The laptop is GPU bound: 50% nearly doubles FPS. */
	constexpr float MinResolutionScale = 50.f;
	constexpr float MaxResolutionScale = 100.f;
	SHIELDBREAK_API float GetResolutionScale();
	SHIELDBREAK_API void SetResolutionScale(float Percent);
}
