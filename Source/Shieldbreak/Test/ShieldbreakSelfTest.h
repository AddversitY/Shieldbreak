// Scripted gameplay self-test. Launch the game with -SBSelfTest (see Tools/SelfTest.bat): bots are
// disabled, the human player stays a spectator, and this actor spawns test characters on open floor,
// drives the core and mastery mechanics, and writes PASS/FAIL lines to Saved/Logs/SelfTest.txt.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShieldbreakSelfTest.generated.h"

class AAIController;
enum class EShieldbreakGameType : uint8;
class AShieldbreakCharacter;

UCLASS(NotPlaceable, Transient)
class SHIELDBREAK_API AShieldbreakSelfTest : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakSelfTest();

	virtual void Tick(float DeltaSeconds) override;

	static bool IsRequested();

protected:
	virtual void BeginPlay() override;

private:
	struct FStep
	{
		FString Name;
		TFunction<void()> Begin;
		/** Called every tick with seconds since Begin; return true when the step is finished. */
		TFunction<bool(float)> Update;
		float Timeout = 10.f;
	};

	void BuildSteps();
	/** Objective-mode checks (CTF / KOTH / Oddball), used instead of the mechanics suite in those modes. */
	void BuildModeSteps(EShieldbreakGameType Mode);
	void Check(const FString& What, bool bPassed, const FString& Detail = FString());
	void Finish();

	AShieldbreakCharacter* SpawnDummy(const FVector& Location, float Yaw, bool bWithController);
	void ClearDummies();
	/** A dummy with a frozen bot controller, so it has a PlayerState and a team. */
	AShieldbreakCharacter* SpawnTeamDummy(const FVector& Location, float Yaw, uint8 Team);
	/** Kills a team dummy and destroys its controller so the game mode can't respawn it. */
	void KillTeamDummy(AShieldbreakCharacter* Dummy);
	void Damage(AShieldbreakCharacter* Target, float Amount, TSubclassOf<class UDamageType> Type, AActor* Causer = nullptr);
	float Now() const;

	TArray<FStep> Steps;
	int32 StepIndex = -1;
	float StepStartTime = 0.f;
	bool bFinished = false;

	TArray<FString> Lines;
	int32 Passed = 0;
	int32 Failed = 0;

	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> Spawned;
	UPROPERTY(Transient) TObjectPtr<AShieldbreakCharacter> A;
	UPROPERTY(Transient) TObjectPtr<AShieldbreakCharacter> B;

	// Chase camera for the bot-match screenshots.
	UPROPERTY(Transient) TObjectPtr<class ACameraActor> ChaseCamera;
	TWeakObjectPtr<AShieldbreakCharacter> ChaseSubject;

	// A non-character actor a step watches (a rocket, a power-up).
	TWeakObjectPtr<AActor> Probe;
	float ShotDue = -1.f;
	/** "Bot" in Slayer on Crucible, else the map and/or mode name: prefixes screenshots and names the report. */
	FString ModeTag = TEXT("Bot");

	/** Map tour (any map but Crucible): every gravity lift lands where the layout says. */
	void BuildMapSteps(const class AShieldbreakArena* Arena);
	/** -SBShots: fixed-camera map views + FPS; -SBFPShots: first-person idle/throw/melee/reload. -SBShotTag=<name> names the files. */
	void BuildShotSteps();
	void TakeShot(const FString& Name, bool bShowUI);
	/** Spawns a soldier and gives it to the local player controller (the human has no pawn during a self-test). */
	AShieldbreakCharacter* PossessLocalDummy(const FTransform& Where);

	// Bot-match stuck detection.
	struct FBotSample
	{
		TWeakObjectPtr<APawn> Pawn;
		FVector Location = FVector::ZeroVector;
		int32 StillSamples = 0;
	};
	TMap<TWeakObjectPtr<AController>, FBotSample> BotSamples;
	float NextBotSample = 0.f;
	TArray<FString> StuckReports;
	int32 ShotIndex = 0;

	// Scratch state shared between a step's Begin and Update.
	float Mark = -1.f;
	float Value0 = 0.f;
	float Value1 = 0.f;
	int32 Phase = 0;
};
