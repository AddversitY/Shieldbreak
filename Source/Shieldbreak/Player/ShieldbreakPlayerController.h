// Human player controller: adds the runtime input context, owns local UI state (scoreboard,
// help), receives hit/damage/respawn feedback from the server, and exposes bot console commands.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "ShieldbreakPlayerController.generated.h"

UCLASS()
class SHIELDBREAK_API AShieldbreakPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShieldbreakPlayerController();

	// ---- Local UI state ----
	bool IsScoreboardVisible() const { return bScoreboardVisible; }
	bool IsHelpVisible() const;

	// ---- Feedback (server -> owning client) ----
	UFUNCTION(Client, Unreliable) void ClientHitConfirmed(bool bKill, bool bHeadshot);
	UFUNCTION(Client, Unreliable) void ClientDamagedFrom(FVector_NetQuantize SourceLocation);
	UFUNCTION(Client, Reliable) void ClientSetRespawnTime(float ServerTimeOfRespawn);

	float GetLastHitConfirmTime() const { return LastHitConfirmTime; }
	bool WasLastHitKill() const { return bLastHitKill; }
	bool WasLastHitHeadshot() const { return bLastHitHeadshot; }
	FVector GetLastDamageSource() const { return LastDamageSource; }
	float GetLastDamagedTime() const { return LastDamagedTime; }
	/** Server time of the pending respawn, or -1. */
	float GetRespawnServerTime() const { return RespawnServerTime; }

	// ---- Console commands ----
	UFUNCTION(Exec) void SB_Bots(int32 Count);
	UFUNCTION(Exec) void SB_Skill(int32 Skill);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	UFUNCTION(Server, Reliable) void ServerSetBots(int32 Count);
	UFUNCTION(Server, Reliable) void ServerSetBotSkill(int32 Skill);

	void AddInputContext();
	void OnScoreboardStarted();
	void OnScoreboardCompleted();
	void OnToggleHelp();

	/** Play-test helper: -SBAutoShot=<seconds> saves screenshots to Saved/Screenshots/AutoShot; -SBAutoQuit exits after. */
	void SetupAutoShots();
	/** First standalone run on a machine: benchmark and apply scalability settings. */
	void AutoDetectScalability();
	void TakeAutoShot();

	int32 AutoShotIndex = 0;
	int32 AutoShotCount = 0;
	float AutoShotInterval = 4.f;
	FTimerHandle AutoShotTimerHandle;

	bool bScoreboardVisible = false;
	/** -1 = automatic (shown for the first seconds of play). */
	int32 HelpOverride = -1;
	float HelpAutoHideTime = 12.f;

	float LastHitConfirmTime = -100.f;
	bool bLastHitKill = false;
	bool bLastHitHeadshot = false;
	FVector LastDamageSource = FVector::ZeroVector;
	float LastDamagedTime = -100.f;
	float RespawnServerTime = -1.f;
};
