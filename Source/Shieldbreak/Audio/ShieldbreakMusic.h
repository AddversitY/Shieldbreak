// Music: the original menu loop (Art/audio/make_music.py, imported to /Game/Shieldbreak/Art/Audio) in the main menu
// and, quieter, on the match-over screen. Volume is ShieldbreakSettings::GetMusicVolume().

#pragma once

#include "CoreMinimal.h"

namespace ShieldbreakMusic
{
	/** Starts the menu loop (fading in) unless it is already playing in this world. Silent if the asset is missing. */
	SHIELDBREAK_API void PlayMenuMusic(const UObject* WorldContext, float Volume = 1.f, float FadeIn = 1.5f);
	SHIELDBREAK_API void Stop(float FadeOut = 1.f);
	/** Re-applies the music volume setting to whatever is playing. */
	SHIELDBREAK_API void RefreshVolume();
	SHIELDBREAK_API bool IsPlaying();
}
