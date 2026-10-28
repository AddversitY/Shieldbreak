// Per-player Slayer stats and the player's tint color. Server-written, replicated to everyone.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ShieldbreakPlayerState.generated.h"

UCLASS()
class SHIELDBREAK_API AShieldbreakPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AShieldbreakPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	// ---- Server only ----

	/** +1 kill; Score mirrors Kills. */
	void AddKill();

	/** Suicide penalty: -1 kill, never below zero. */
	void RemoveKill();

	void AddDeath();
	void ResetStats();

	/** Server: medal bookkeeping. Kills within MultiKillWindow of each other chain; a death ends chain and spree. */
	static constexpr float MultiKillWindow = 4.5f;
	int32 GetChainKills() const { return ChainKills; }
	int32 GetSpreeKills() const { return SpreeKills; }
	/** Counts a kill at ServerTime toward the chain and spree. */
	void NoteKillForMedals(float ServerTime);
	AShieldbreakPlayerState* GetLastKilledBy() const { return LastKilledBy.Get(); }
	void SetLastKilledBy(AShieldbreakPlayerState* Killer) { LastKilledBy = Killer; }
	/** Medals earned this match (replicated for the scoreboard). */
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") int32 GetMedalCount() const { return MedalCount; }
	void AddMedal() { ++MedalCount; }

	/** Assigned by AShieldbreakGameMode on join. PaletteIndex lets the GameMode keep colors distinct. */
	void SetPlayerColor(const FLinearColor& NewColor, int32 PaletteIndex = INDEX_NONE);

	// ---- Queries ----

	UFUNCTION(BlueprintPure, Category = "Shieldbreak") int32 GetKills() const { return Kills; }
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") int32 GetDeaths() const { return Deaths; }
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") FLinearColor GetPlayerColor() const { return PlayerColor; }

	/** Palette slot of PlayerColor, or INDEX_NONE before the GameMode assigned one. Server only. */
	int32 GetColorIndex() const { return ColorIndex; }

	/** ShieldbreakTeams::NoTeam (255) in free-for-all. */
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") uint8 GetTeamId() const { return TeamId; }
	/** Server. */
	void SetTeamId(uint8 NewTeamId);

private:
	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Deaths = 0;

	UPROPERTY(Replicated)
	uint8 TeamId = 255;

	UPROPERTY(Replicated)
	FLinearColor PlayerColor = FLinearColor(0.6f, 0.6f, 0.6f, 1.f);

	int32 ColorIndex = INDEX_NONE;

	UPROPERTY(Replicated)
	int32 MedalCount = 0;

	float LastKillServerTime = -100.f;
	int32 ChainKills = 0;
	int32 SpreeKills = 0;
	TWeakObjectPtr<AShieldbreakPlayerState> LastKilledBy;
};
