#include "Game/ShieldbreakGameState.h"

#include "Algo/Sort.h"
#include "Game/ShieldbreakArena.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Player/ShieldbreakPlayerState.h"

namespace ShieldbreakGameStatePrivate
{
	constexpr int32 MaxKillFeedEntries = 6;
}

AShieldbreakGameState::AShieldbreakGameState()
{
}

void AShieldbreakGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShieldbreakGameState, KillFeed);
	DOREPLIFETIME(AShieldbreakGameState, ScoreLimit);
	DOREPLIFETIME(AShieldbreakGameState, bMatchOver);
	DOREPLIFETIME(AShieldbreakGameState, WinnerName);
	DOREPLIFETIME(AShieldbreakGameState, MatchRestartServerTime);
}

void AShieldbreakGameState::BeginPlay()
{
	Super::BeginPlay();

	// The arena is not replicated; every machine builds the same deterministic copy locally.
	AShieldbreakArena::EnsureLocal(GetWorld());
}

void AShieldbreakGameState::AddKillFeedEntry(const FString& KillerName, const FString& VictimName, const FLinearColor& KillerColor, const FLinearColor& VictimColor)
{
	FShieldbreakKillFeedEntry Entry;
	Entry.KillerName = KillerName;
	Entry.VictimName = VictimName;
	Entry.KillerColor = KillerColor;
	Entry.VictimColor = VictimColor;
	Entry.ServerTime = GetServerTimeNow();
	AddKillFeedEntry(Entry);
}

void AShieldbreakGameState::AddKillFeedEntry(const FShieldbreakKillFeedEntry& Entry)
{
	if (!HasAuthority())
	{
		return;
	}

	FShieldbreakKillFeedEntry& Added = KillFeed.Add_GetRef(Entry);
	if (Added.ServerTime <= 0.f)
	{
		Added.ServerTime = GetServerTimeNow();
	}

	while (KillFeed.Num() > ShieldbreakGameStatePrivate::MaxKillFeedEntries)
	{
		KillFeed.RemoveAt(0);
	}

	ForceNetUpdate();
}

void AShieldbreakGameState::SetMatchOver(const FString& Winner, float RestartAt)
{
	if (!HasAuthority())
	{
		return;
	}

	bMatchOver = true;
	WinnerName = Winner;
	MatchRestartServerTime = RestartAt;
	ForceNetUpdate();
}

void AShieldbreakGameState::ResetMatch()
{
	if (!HasAuthority())
	{
		return;
	}

	KillFeed.Reset();
	bMatchOver = false;
	WinnerName.Reset();
	MatchRestartServerTime = -1.f;

	for (APlayerState* Entry : PlayerArray)
	{
		if (AShieldbreakPlayerState* SBPlayerState = Cast<AShieldbreakPlayerState>(Entry))
		{
			SBPlayerState->ResetStats();
		}
	}

	ForceNetUpdate();
}

void AShieldbreakGameState::SetScoreLimit(int32 NewScoreLimit)
{
	if (!HasAuthority())
	{
		return;
	}

	ScoreLimit = FMath::Max(1, NewScoreLimit);
	ForceNetUpdate();
}

float AShieldbreakGameState::GetMatchRestartCountdown() const
{
	if (!bMatchOver || MatchRestartServerTime < 0.f)
	{
		return 0.f;
	}

	return FMath::Max(0.f, MatchRestartServerTime - GetServerTimeNow());
}

TArray<APlayerState*> AShieldbreakGameState::GetSortedPlayers() const
{
	TArray<APlayerState*> Sorted;
	Sorted.Reserve(PlayerArray.Num());

	for (APlayerState* Entry : PlayerArray)
	{
		if (IsValid(Entry) && !Entry->IsOnlyASpectator())
		{
			Sorted.Add(Entry);
		}
	}

	Algo::Sort(Sorted, [](const APlayerState* A, const APlayerState* B)
	{
		const AShieldbreakPlayerState* SBA = Cast<AShieldbreakPlayerState>(A);
		const AShieldbreakPlayerState* SBB = Cast<AShieldbreakPlayerState>(B);

		const int32 KillsA = SBA ? SBA->GetKills() : FMath::RoundToInt(A->GetScore());
		const int32 KillsB = SBB ? SBB->GetKills() : FMath::RoundToInt(B->GetScore());
		if (KillsA != KillsB)
		{
			return KillsA > KillsB;
		}

		const int32 DeathsA = SBA ? SBA->GetDeaths() : 0;
		const int32 DeathsB = SBB ? SBB->GetDeaths() : 0;
		if (DeathsA != DeathsB)
		{
			return DeathsA < DeathsB;
		}

		return A->GetPlayerId() < B->GetPlayerId();
	});

	return Sorted;
}

float AShieldbreakGameState::GetServerTimeNow() const
{
	return static_cast<float>(GetServerWorldTimeSeconds());
}
