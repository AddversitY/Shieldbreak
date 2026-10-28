// Launch rules shared by the game mode and the self-test. The front end itself is SShieldbreakMainMenu.

#pragma once

#include "CoreMinimal.h"

namespace ShieldbreakMenu
{
	/** True when a launch with these URL options and command line should open the main menu instead of a match. */
	SHIELDBREAK_API bool ShouldShowMainMenu(const FString& Options, const TCHAR* CommandLine);

	/** URL options for a match, e.g. "Map=Keel?Mode=CTF?Bots=7?Skill=1" (plus "?listen" when hosting). */
	SHIELDBREAK_API FString BuildMatchOptions(FName MapId, const FString& Mode, int32 Bots, int32 Skill, bool bListen);

	/** Back to the main menu (disconnects from or closes the current match). */
	SHIELDBREAK_API void ReturnToMainMenu(const UObject* WorldContext);

	/** The four modes, in menu order, as the ?Mode= values the game mode accepts. */
	SHIELDBREAK_API const TArray<FString>& GetModes();
}
