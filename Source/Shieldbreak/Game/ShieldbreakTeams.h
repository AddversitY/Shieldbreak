// Team helpers shared by the game mode, damage filtering, bots and the HUD. A team lives on
// AShieldbreakPlayerState; anything owned or instigated by a player (pawn, controller, weapon, grenade) resolves to it.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UObject;

namespace ShieldbreakTeams
{
	inline constexpr uint8 NoTeam = 255;
	inline constexpr uint8 Red = 0;
	inline constexpr uint8 Blue = 1;
	inline constexpr int32 NumTeams = 2;

	/** Team of a pawn, controller, player state, or anything owned/instigated by one. NoTeam in free-for-all. */
	SHIELDBREAK_API uint8 GetTeam(const AActor* Actor);

	/** Both resolve to the same real team (always false in free-for-all). */
	SHIELDBREAK_API bool AreTeammates(const AActor* A, const AActor* B);

	SHIELDBREAK_API bool IsTeamGame(const UObject* WorldContext);

	SHIELDBREAK_API FLinearColor GetTeamColor(uint8 Team);
	SHIELDBREAK_API FString GetTeamName(uint8 Team);
}
