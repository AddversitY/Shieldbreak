// Human player controller: adds the runtime input context, owns local UI state (scoreboard,
// help), receives hit/damage/respawn feedback from the server, and exposes bot console commands.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/EngineBaseTypes.h"
#include "ShieldbreakPlayerController.generated.h"

UCLASS()
class SHIELDBREAK_API AShieldbreakPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShieldbreakPlayerController();
	void ShowMainMenu();
	void CloseMainMenu();
	/** Join a validated direct host address while retaining it for a failed-connection retry. */
	void JoinMatch(const FString& Address);

	// ---- Local UI state ----
	bool IsScoreboardVisible() const { return bScoreboardVisible; }
	bool IsHelpVisible() const;

	// ---- Feedback (server -> owning client) ----
	UFUNCTION(Client, Unreliable) void ClientHitConfirmed(bool bKill, bool bHeadshot);
	/** A medal for this player (EShieldbreakMedal); queued for the HUD. */
	UFUNCTION(Client, Reliable) void ClientMedal(uint8 Medal);
	UFUNCTION(Client, Unreliable) void ClientDamagedFrom(FVector_NetQuantize SourceLocation);
	UFUNCTION(Client, Reliable) void ClientSetRespawnTime(float ServerTimeOfRespawn);

	float GetLastHitConfirmTime() const { return LastHitConfirmTime; }
	bool WasLastHitKill() const { return bLastHitKill; }
	bool WasLastHitHeadshot() const { return bLastHitHeadshot; }

	struct FMedalPopup
	{
		uint8 Medal = 0;
		float Time = 0.f;
	};
	/** Medals from the last few seconds, oldest first. */
	const TArray<FMedalPopup>& GetMedalPopups() const { return MedalPopups; }
	FVector GetLastDamageSource() const { return LastDamageSource; }
	float GetLastDamagedTime() const { return LastDamagedTime; }
	/** Server time of the pending respawn, or -1. */
	float GetRespawnServerTime() const { return RespawnServerTime; }

	// ---- Console commands ----
	UFUNCTION(Exec) void SB_Bots(int32 Count);
	UFUNCTION(Exec) void SB_Skill(int32 Skill);
	/** Console: "SB_Map Keel" or "SB_Map Bastion KOTH". Maps: Keel, Bastion, Sanctum, Creekbed, Spire, Crucible. Modes: Slayer, CTF, KOTH, Oddball. */
	UFUNCTION(Exec) void SB_Map(const FString& Map, const FString& Mode);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	friend struct FShieldbreakConnectionTest;
	TSharedPtr<class SShieldbreakMainMenu> MainMenu;
	/** Esc in a match opens the same menu as the front end (Resume, Leave, Settings, Controls). */
	void OnEscape();
	UFUNCTION(Server, Reliable) void ServerSetBots(int32 Count);
	UFUNCTION(Server, Reliable) void ServerSetBotSkill(int32 Skill);
	UFUNCTION(Server, Reliable) void ServerChangeMap(const FString& Map, const FString& Mode);

	bool AuthorizeMatchControl(const TCHAR* CommandName);
	void AddInputContext();
	void OnScoreboardStarted();
	void OnScoreboardCompleted();
	void OnToggleHelp();
	void OnPause();

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
	TArray<FMedalPopup> MedalPopups;
	FVector LastDamageSource = FVector::ZeroVector;
	float LastDamagedTime = -100.f;
	float RespawnServerTime = -1.f;
};
