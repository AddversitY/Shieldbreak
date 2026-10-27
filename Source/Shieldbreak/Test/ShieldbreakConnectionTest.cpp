#include "Test/ShieldbreakConnectionTest.h"

#include "Shieldbreak.h"
#include "Engine/World.h"
#include "Game/ShieldbreakGameInstance.h"
#include "Game/ShieldbreakGameMode.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Player/ShieldbreakPlayerController.h"
#include "TimerManager.h"
#include "UI/SShieldbreakMainMenu.h"

namespace ShieldbreakConnectionTestPrivate
{
	// A world change creates a new controller, but the process must issue each attempt once.
	int32 Stage = 0;
	FString FailedAddress;

	void Fail(const TCHAR* Reason)
	{
		UE_LOG(LogShieldbreak, Error, TEXT("ConnectionTest FAIL: %s"), Reason);
		FPlatformMisc::RequestExitWithStatus(false, 1);
	}
}

void FShieldbreakConnectionTest::Start(AShieldbreakPlayerController* TestController)
{
	FString Scenario;
	if (!TestController || !TestController->IsLocalController()
		|| !FParse::Value(FCommandLine::Get(), TEXT("SBConnectionTest="), Scenario)
		|| (Scenario != TEXT("HostLoss") && Scenario != TEXT("FailedJoin") && Scenario != TEXT("Retry"))) { return; }
	UWorld* TestWorld = TestController->GetWorld();
	if (!TestWorld) { return; }
	const TWeakObjectPtr<AShieldbreakPlayerController> WeakController(TestController);
	FTimerHandle ProbeTimer;
	TestWorld->GetTimerManager().SetTimer(ProbeTimer, FTimerDelegate::CreateLambda([WeakController, Scenario]()
	{
		using namespace ShieldbreakConnectionTestPrivate;
		AShieldbreakPlayerController* Probe = WeakController.Get();
		if (!Probe || !Probe->GetWorld()) { return; }
		UWorld* World = Probe->GetWorld();
		UShieldbreakGameInstance* Instance = World->GetGameInstance<UShieldbreakGameInstance>();
		if (!Instance) { Fail(TEXT("Shieldbreak game instance is missing.")); return; }
		const AShieldbreakGameMode* Rules = World->GetAuthGameMode<AShieldbreakGameMode>();
		const bool bFrontEnd = Rules && Rules->IsFrontEnd();
		if (Scenario == TEXT("HostLoss"))
		{
			if (Stage == 0 && World->GetNetMode() == NM_Client)
			{
				Stage = 1;
				UE_LOG(LogShieldbreak, Log, TEXT("ConnectionTest: HostLoss joined."));
			}
			else if (Stage == 1 && bFrontEnd)
			{
				if (!Probe->MainMenu.IsValid() || !Probe->MainMenu->IsConnectionRetryVisible())
				{ Fail(TEXT("Host loss did not restore the front-end retry screen.")); return; }
				Stage = 2;
				UE_LOG(LogShieldbreak, Log, TEXT("ConnectionTest PASS: HostLoss recovered frontend retry=1."));
				FPlatformMisc::RequestExit(false);
			}
			return;
		}
		if (Stage == 0 && bFrontEnd)
		{
			FailedAddress = TEXT("127.0.0.1:1");
			FParse::Value(FCommandLine::Get(), TEXT("SBFailedAddress="), FailedAddress);
			Stage = 1;
			UE_LOG(LogShieldbreak, Log, TEXT("ConnectionTest: attempting unavailable address %s."), *FailedAddress);
			Probe->JoinMatch(FailedAddress);
		}
		else if (Stage == 1 && bFrontEnd)
		{
			if (!Probe->MainMenu.IsValid() || !Probe->MainMenu->IsConnectionRetryVisible())
			{ Fail(TEXT("Failed join did not restore the front-end retry screen.")); return; }
			if (Instance->GetLastJoinAddress() != FailedAddress || Probe->MainMenu->GetJoinAddress() != FailedAddress)
			{ Fail(TEXT("Failed join address was not retained in the instance and menu.")); return; }
			Stage = 2;
			UE_LOG(LogShieldbreak, Log, TEXT("ConnectionTest PASS: FailedJoin recovered frontend retry=1 retained=%s."), *FailedAddress);
			if (Scenario == TEXT("FailedJoin")) { FPlatformMisc::RequestExit(false); return; }
			FString RetryAddress;
			if (!FParse::Value(FCommandLine::Get(), TEXT("SBRetryAddress="), RetryAddress) || RetryAddress.IsEmpty())
			{ Fail(TEXT("Retry scenario requires SBRetryAddress.")); return; }
			Stage = 3;
			UE_LOG(LogShieldbreak, Log, TEXT("ConnectionTest: retrying address %s."), *RetryAddress);
			Probe->JoinMatch(RetryAddress);
		}
		else if (Stage == 3 && World->GetNetMode() == NM_Client)
		{
			if (Probe->MainMenu.IsValid() && Probe->MainMenu->IsConnectionRetryVisible())
			{ Fail(TEXT("Retry connected with a stale connection-error screen.")); return; }
			if (!Instance->ConsumeConnectionError().IsEmpty())
			{ Fail(TEXT("Retry connected with a stale game-instance error.")); return; }
			Stage = 4;
			UE_LOG(LogShieldbreak, Log, TEXT("ConnectionTest PASS: Retry joined match with no stale error screen."));
			FPlatformMisc::RequestExit(false);
		}
		else if (Stage == 3 && bFrontEnd)
		{
			Fail(TEXT("Retry fell back to the front end instead of joining its host."));
		}
	}), 1.f, false);
}
