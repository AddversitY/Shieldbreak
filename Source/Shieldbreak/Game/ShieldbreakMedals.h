// Medals: small awards for multi-kills, sprees and stylish kills, shown on the killer's HUD. The game mode awards
// them (server); AShieldbreakPlayerController::ClientMedal shows them; AShieldbreakPlayerState counts them.

#pragma once

#include "CoreMinimal.h"

enum class EShieldbreakMedal : uint8
{
	FirstBlood,
	DoubleKill,
	TripleKill,
	QuadKill,
	Rampage,
	KillingSpree,
	Unstoppable,
	Untouchable,
	Headshot,
	Knockout,
	Blast,
	Revenge,
	SpreeBreaker,
	Count
};

namespace ShieldbreakMedals
{
	inline const TCHAR* GetName(EShieldbreakMedal Medal)
	{
		static const TCHAR* const Names[] =
		{
			TEXT("FIRST BLOOD"), TEXT("DOUBLE KILL"), TEXT("TRIPLE KILL"), TEXT("QUAD KILL"), TEXT("RAMPAGE"),
			TEXT("KILLING SPREE"), TEXT("UNSTOPPABLE"), TEXT("UNTOUCHABLE"), TEXT("HEADSHOT"), TEXT("KNOCKOUT"),
			TEXT("BLAST"), TEXT("REVENGE"), TEXT("SPREE BREAKER"),
		};
		static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(EShieldbreakMedal::Count), "one name per medal");
		const int32 Index = static_cast<int32>(Medal);
		return Index >= 0 && Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("");
	}

	/** Multi-kills and sprees get the bigger popup and chime. */
	inline bool IsBig(EShieldbreakMedal Medal)
	{
		return Medal >= EShieldbreakMedal::DoubleKill && Medal <= EShieldbreakMedal::Untouchable;
	}
}
