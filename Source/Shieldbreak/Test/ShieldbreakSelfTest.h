// Scripted gameplay self-test. Launch the game with -SBSelfTest (see Tools/SelfTest.bat): bots are
// disabled, the human player stays a spectator, and this actor spawns test characters on open floor,
// drives the core and mastery mechanics, and writes PASS/FAIL lines to Saved/Logs/SelfTest.txt.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShieldbreakSelfTest.generated.h"

class AAIController;
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
	void Check(const FString& What, bool bPassed, const FString& Detail = FString());
	void Finish();

	AShieldbreakCharacter* SpawnDummy(const FVector& Location, float Yaw, bool bWithController);
	void ClearDummies();
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
	float ShotDue = -1.f;
	int32 ShotIndex = 0;

	// Scratch state shared between a step's Begin and Update.
	float Mark = -1.f;
	float Value0 = 0.f;
	float Value1 = 0.f;
	int32 Phase = 0;
};
