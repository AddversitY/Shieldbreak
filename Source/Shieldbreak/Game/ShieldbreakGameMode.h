// Free-For-All Slayer rules: arena bootstrap, spawning, bots, scoring, kill feed and the
// post-match reset. Exists on the server only.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "ShieldbreakGameMode.generated.h"

class AShieldbreakArena;
class AShieldbreakBotController;
class AShieldbreakCharacter;
class APlayerController;
class APlayerState;

/** A controller waiting to respawn. */
struct FShieldbreakPendingRespawn
{
	TWeakObjectPtr<AController> Who;
	float RespawnServerTime = 0.f;
};

UCLASS(Config = Game)
class SHIELDBREAK_API AShieldbreakGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShieldbreakGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Spawns the greybox arena at the origin if the world has none. Safe to call any time. */
	AShieldbreakArena* EnsureArena();

	/** Called by AShieldbreakCharacter on the server when it dies. Killer may be null or equal Victim (suicide). */
	void NotifyKilled(AController* Killer, AController* Victim, AShieldbreakCharacter* VictimPawn);

	/** Adds or removes bots live (removal destroys the bot's pawn and controller). */
	void SetBotCount(int32 NewCount);

	/** 0 Easy, 1 Normal, 2 Heroic, 3 Legendary. Applies to existing bots immediately. */
	void SetBotSkill(int32 NewSkill);

	bool IsMatchOver() const { return bMatchOver; }

	int32 GetBotCount() const { return NumBots; }
	int32 GetBotSkill() const { return BotSkill; }
	int32 GetScoreLimit() const { return ScoreLimit; }
	float GetRespawnDelay() const { return RespawnDelay; }

protected:
	/** Arena spawn farthest from living enemies, picked at random among the best few. */
	virtual FTransform ChooseSpawnTransform(AController* ForController);

	/** Bots kept in the match. URL option ?Bots=N. */
	UPROPERTY(EditDefaultsOnly, Config, BlueprintReadOnly, Category = "Shieldbreak|Bots", meta = (ClampMin = "0", ClampMax = "16"))
	int32 NumBots = 5;

	/** 0 Easy, 1 Normal, 2 Heroic, 3 Legendary. URL option ?Skill=N. */
	UPROPERTY(EditDefaultsOnly, Config, BlueprintReadOnly, Category = "Shieldbreak|Bots", meta = (ClampMin = "0", ClampMax = "3"))
	int32 BotSkill = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Bots")
	TSubclassOf<AShieldbreakBotController> BotControllerClass;

	UPROPERTY(EditDefaultsOnly, Config, BlueprintReadOnly, Category = "Shieldbreak|Match", meta = (ClampMin = "0"))
	float RespawnDelay = 3.f;

	/** Kills to win. URL option ?ScoreLimit=N. */
	UPROPERTY(EditDefaultsOnly, Config, BlueprintReadOnly, Category = "Shieldbreak|Match", meta = (ClampMin = "1"))
	int32 ScoreLimit = 25;

	/** Seconds the match-over banner shows before stats reset and everyone respawns. */
	UPROPERTY(EditDefaultsOnly, Config, BlueprintReadOnly, Category = "Shieldbreak|Match", meta = (ClampMin = "0"))
	float MatchOverDuration = 8.f;

	/** Distinct tints handed out to players and bots, first unused slot first. */
	UPROPERTY(EditDefaultsOnly, Category = "Shieldbreak|Players")
	TArray<FLinearColor> PlayerColors;

	/** Rename humans "Player N" (the default online nickname is a long machine name). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Shieldbreak|Players")
	bool bUseGenericPlayerNames = true;

private:
	// ---- Bots ----
	void SpawnInitialBots();
	void ReconcileBots();
	bool AddBot();
	void RemoveLastBot();
	FString MakeBotName() const;

	// ---- Players ----
	FString MakeHumanName(const APlayerState* ForState) const;
	void EnsurePlayerColor(AController* ForController);

	/** Players and bots with a non-spectator PlayerState that are ready to spawn. */
	bool CanParticipate(AController* Candidate);

	// ---- Respawning ----
	void ScheduleRespawn(AController* Who, float Delay);
	void RemovePendingRespawn(const AController* Who);
	bool IsRespawnPending(const AController* Who) const;
	void ProcessPendingRespawns();
	void RespawnNow(AController* Who);

	/** Safety net: anyone left without a living pawn and no respawn queued gets one. */
	void RescueStrandedControllers();

	FVector SnapToFloor(const FVector& SpawnLocation, AController* ForController);

	// ---- Match flow ----
	void FinishMatch(const FString& Winner);
	void PerformMatchReset();

	float GetServerTimeSeconds() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AShieldbreakBotController>> Bots;

	UPROPERTY(Transient)
	TObjectPtr<AShieldbreakArena> CachedArena;

	TArray<FShieldbreakPendingRespawn> PendingRespawns;

	FTimerHandle InitialBotsTimerHandle;
	FTimerHandle MatchResetTimerHandle;

	float StrandedCheckCountdown = 1.f;
	float MatchRestartServerTime = -1.f;
	bool bMatchOver = false;
	bool bBotsStarted = false;
};
