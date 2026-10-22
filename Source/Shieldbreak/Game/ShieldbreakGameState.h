// Replicated Free-For-All Slayer state: kill feed, score limit and the match-over banner.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Game/ShieldbreakObjectives.h"
#include "ShieldbreakGameState.generated.h"

class APlayerState;

USTRUCT(BlueprintType)
struct SHIELDBREAK_API FShieldbreakKillFeedEntry
{
	GENERATED_BODY()

	/** For suicides this is the victim's own name. */
	UPROPERTY(BlueprintReadOnly, Category = "Shieldbreak")
	FString KillerName;

	UPROPERTY(BlueprintReadOnly, Category = "Shieldbreak")
	FString VictimName;

	UPROPERTY(BlueprintReadOnly, Category = "Shieldbreak")
	FLinearColor KillerColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Shieldbreak")
	FLinearColor VictimColor = FLinearColor::White;

	/** Objective event ("<Killer> <Victim>", e.g. "Player 1 captured the flag!") rather than a kill. */
	UPROPERTY(BlueprintReadOnly, Category = "Shieldbreak")
	bool bEvent = false;

	/** AGameStateBase::GetServerWorldTimeSeconds() when the kill happened. */
	UPROPERTY(BlueprintReadOnly, Category = "Shieldbreak")
	float ServerTime = 0.f;
};

UCLASS()
class SHIELDBREAK_API AShieldbreakGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AShieldbreakGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Server only ----

	/** Appends a kill (stamped with the current server time), keeping only the newest 6. */
	void AddKillFeedEntry(const FString& KillerName, const FString& VictimName, const FLinearColor& KillerColor, const FLinearColor& VictimColor);

	/** Appends a prebuilt entry. A ServerTime <= 0 is replaced with the current server time. */
	void AddKillFeedEntry(const FShieldbreakKillFeedEntry& Entry);

	void SetMatchOver(const FString& Winner, float RestartAt);

	/** Clears the kill feed and match-over state and resets every player's stats. */
	void ResetMatch();

	void SetScoreLimit(int32 NewScoreLimit);
	void SetTeamGame(bool bNewTeamGame);
	void SetGameType(EShieldbreakGameType NewGameType);
	/** Server: which map layout every machine builds. */
	void SetMapId(FName NewMapId);
	/** Adds (or with a negative delta, removes) team points. */
	void AddTeamScore(uint8 Team, int32 Delta);

	// ---- Queries ----

	/** Oldest first, newest last. */
	const TArray<FShieldbreakKillFeedEntry>& GetKillFeed() const { return KillFeed; }

	UFUNCTION(BlueprintPure, Category = "Shieldbreak") int32 GetScoreLimit() const { return ScoreLimit; }
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") bool IsMatchOver() const { return bMatchOver; }
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") FString GetWinnerName() const { return WinnerName; }

	/** Server time at which the post-match reset happens, or -1. */
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") float GetMatchRestartServerTime() const { return MatchRestartServerTime; }

	/** Seconds left on the match-over banner (0 when the match is running). */
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") float GetMatchRestartCountdown() const;

	UFUNCTION(BlueprintPure, Category = "Shieldbreak") bool IsTeamGame() const { return bTeamGame; }
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") EShieldbreakGameType GetGameType() const { return GameType; }
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") FName GetMapId() const { return MapId; }
	UFUNCTION(BlueprintPure, Category = "Shieldbreak") int32 GetTeamScore(uint8 Team) const { return TeamScores.IsValidIndex(Team) ? TeamScores[Team] : 0; }

	/** Non-spectator players by kills (desc), then deaths (asc), then join order. */
	TArray<APlayerState*> GetSortedPlayers() const;

	// Public for convenient read access; only the server writes them, via the functions above.

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Shieldbreak")
	TArray<FShieldbreakKillFeedEntry> KillFeed;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Shieldbreak")
	int32 ScoreLimit = 25;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Shieldbreak")
	bool bMatchOver = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Shieldbreak")
	FString WinnerName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Shieldbreak")
	float MatchRestartServerTime = -1.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Shieldbreak")
	bool bTeamGame = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Shieldbreak")
	EShieldbreakGameType GameType = EShieldbreakGameType::Slayer;

	UPROPERTY(ReplicatedUsing = OnRep_MapId, BlueprintReadOnly, Category = "Shieldbreak")
	FName MapId;

	/** Indexed by ShieldbreakTeams::Red / Blue. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Shieldbreak")
	TArray<int32> TeamScores;

protected:
	virtual void BeginPlay() override;

	UFUNCTION() void OnRep_MapId();

private:
	float GetServerTimeNow() const;
};
