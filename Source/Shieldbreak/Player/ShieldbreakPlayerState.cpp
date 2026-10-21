#include "Player/ShieldbreakPlayerState.h"

#include "Net/UnrealNetwork.h"

AShieldbreakPlayerState::AShieldbreakPlayerState()
{
}

void AShieldbreakPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShieldbreakPlayerState, Kills);
	DOREPLIFETIME(AShieldbreakPlayerState, Deaths);
	DOREPLIFETIME(AShieldbreakPlayerState, PlayerColor);
	DOREPLIFETIME(AShieldbreakPlayerState, TeamId);
}

void AShieldbreakPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	if (AShieldbreakPlayerState* Destination = Cast<AShieldbreakPlayerState>(NewPlayerState))
	{
		Destination->Kills = Kills;
		Destination->Deaths = Deaths;
		Destination->PlayerColor = PlayerColor;
		Destination->ColorIndex = ColorIndex;
	}
}

// PlayerStates replicate at a low default rate, so every change forces an update to keep the
// scoreboard and kill feed in sync.

void AShieldbreakPlayerState::AddKill()
{
	if (!HasAuthority())
	{
		return;
	}

	++Kills;
	SetScore(static_cast<float>(Kills));
	ForceNetUpdate();
}

void AShieldbreakPlayerState::RemoveKill()
{
	if (!HasAuthority())
	{
		return;
	}

	Kills = FMath::Max(0, Kills - 1);
	SetScore(static_cast<float>(Kills));
	ForceNetUpdate();
}

void AShieldbreakPlayerState::AddDeath()
{
	if (!HasAuthority())
	{
		return;
	}

	++Deaths;
	ForceNetUpdate();
}

void AShieldbreakPlayerState::ResetStats()
{
	if (!HasAuthority())
	{
		return;
	}

	Kills = 0;
	Deaths = 0;
	SetScore(0.f);
	ForceNetUpdate();
}

void AShieldbreakPlayerState::SetPlayerColor(const FLinearColor& NewColor, int32 PaletteIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	PlayerColor = NewColor;
	ColorIndex = PaletteIndex;
	ForceNetUpdate();
}

void AShieldbreakPlayerState::SetTeamId(uint8 NewTeamId)
{
	if (HasAuthority() && TeamId != NewTeamId)
	{
		TeamId = NewTeamId;
		ForceNetUpdate();
	}
}
