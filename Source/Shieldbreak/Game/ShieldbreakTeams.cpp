#include "Game/ShieldbreakTeams.h"

#include "Engine/World.h"
#include "Game/ShieldbreakGameState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Player/ShieldbreakPlayerState.h"

namespace ShieldbreakTeams
{
	uint8 GetTeam(const AActor* Actor)
	{
		// Walk owner / instigator links (weapon -> character, grenade -> thrower) a few steps.
		for (int32 Depth = 0; Actor && Depth < 4; ++Depth)
		{
			const AShieldbreakPlayerState* State = Cast<AShieldbreakPlayerState>(Actor);
			if (!State)
			{
				if (const APawn* Pawn = Cast<APawn>(Actor))
				{
					State = Pawn->GetPlayerState<AShieldbreakPlayerState>();
				}
				else if (const AController* Controller = Cast<AController>(Actor))
				{
					State = Controller->GetPlayerState<AShieldbreakPlayerState>();
				}
			}
			if (State)
			{
				return State->GetTeamId();
			}
			Actor = Actor->GetInstigator() ? Actor->GetInstigator() : Actor->GetOwner();
		}
		return NoTeam;
	}

	bool AreTeammates(const AActor* A, const AActor* B)
	{
		const uint8 TeamA = GetTeam(A);
		return TeamA != NoTeam && TeamA == GetTeam(B);
	}

	bool IsTeamGame(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const AShieldbreakGameState* GameState = World ? World->GetGameState<AShieldbreakGameState>() : nullptr;
		return GameState && GameState->IsTeamGame();
	}

	FLinearColor GetTeamColor(uint8 Team)
	{
		switch (Team)
		{
		case Red: return FLinearColor(0.85f, 0.12f, 0.08f);
		case Blue: return FLinearColor(0.1f, 0.35f, 0.95f);
		default: return FLinearColor(0.6f, 0.6f, 0.6f);
		}
	}

	FString GetTeamName(uint8 Team)
	{
		switch (Team)
		{
		case Red: return TEXT("Red Team");
		case Blue: return TEXT("Blue Team");
		default: return TEXT("No Team");
		}
	}
}
