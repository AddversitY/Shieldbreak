#include "Test/ShieldbreakNetworkTest.h"

#include "Shieldbreak.h"
#include "Engine/World.h"
#include "Game/ShieldbreakGameMode.h"
#include "Game/ShieldbreakGameState.h"
#include "Game/ShieldbreakTeams.h"
#include "GameFramework/GameSession.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Player/ShieldbreakPlayerController.h"
#include "Player/ShieldbreakPlayerState.h"
#include "TimerManager.h"

void FShieldbreakNetworkTest::Start(AShieldbreakPlayerController* TestController)
{
	if (!TestController || !TestController->IsLocalController()
		|| !FParse::Param(FCommandLine::Get(), TEXT("SBNetworkTest"))) { return; }
	UWorld* TestWorld = TestController->GetWorld();
	if (!TestWorld) { return; }
	const TWeakObjectPtr<AShieldbreakPlayerController> WeakController(TestController);
	FTimerHandle ProbeTimer;
	if (TestWorld->GetNetMode() == NM_Client)
	{
		TestWorld->GetTimerManager().SetTimer(ProbeTimer, FTimerDelegate::CreateLambda([WeakController]()
		{
			if (AShieldbreakPlayerController* Probe = WeakController.Get())
			{
				UE_LOG(LogShieldbreak, Log, TEXT("NetTest: sending remote match-control probes."));
				Probe->SB_Bots(0);
				Probe->SB_Skill(3);
				Probe->SB_Map(TEXT("Spire"), TEXT("CTF"));
			}
		}), 3.f, false);
		return;
	}
	// Positive control: the actual listen host can change settings, but 16 requested bots
	// still cannot exceed the eight-player roster. The client must not change these values.
	TestWorld->GetTimerManager().SetTimer(ProbeTimer, FTimerDelegate::CreateLambda([WeakController]()
	{
		if (AShieldbreakPlayerController* Probe = WeakController.Get())
		{
			Probe->SB_Bots(16);
			Probe->SB_Skill(2);
		}
	}), 1.f, false);
	const TWeakObjectPtr<UWorld> WeakWorld(TestWorld);
	const TSharedRef<FTimerHandle> SnapshotTimer = MakeShared<FTimerHandle>();
	TestWorld->GetTimerManager().SetTimer(*SnapshotTimer, FTimerDelegate::CreateLambda([WeakWorld, SnapshotTimer, Samples = 0]() mutable
	{
		UWorld* SnapshotWorld = WeakWorld.Get();
		if (!SnapshotWorld) { return; }
		const AShieldbreakGameState* State = SnapshotWorld->GetGameState<AShieldbreakGameState>();
		const AShieldbreakGameMode* Rules = SnapshotWorld->GetAuthGameMode<AShieldbreakGameMode>();
		if (State && Rules)
		{
			int32 Humans = 0, Bots = 0, Red = 0, Blue = 0;
			for (const APlayerState* Entry : State->PlayerArray)
			{
				const AShieldbreakPlayerState* Player = Cast<AShieldbreakPlayerState>(Entry);
				if (!IsValid(Player) || Player->IsActorBeingDestroyed() || Player->IsOnlyASpectator()) { continue; }
				if (Player->IsABot()) { ++Bots; } else { ++Humans; }
				if (Player->GetTeamId() == ShieldbreakTeams::Red) { ++Red; }
				if (Player->GetTeamId() == ShieldbreakTeams::Blue) { ++Blue; }
			}
			UE_LOG(LogShieldbreak, Log, TEXT("NetTest roster humans=%d bots=%d red=%d blue=%d total=%d limit=%d requested=%d skill=%d map=%s mode=%d"),
				Humans, Bots, Red, Blue, Humans + Bots, Rules->GameSession ? Rules->GameSession->MaxPlayers : 0,
				Rules->GetBotCount(), Rules->GetBotSkill(), *Rules->GetMapId().ToString(), static_cast<int32>(Rules->GetGameType()));
		}
		if (++Samples >= 120) { SnapshotWorld->GetTimerManager().ClearTimer(*SnapshotTimer); }
	}), 2.f, true);
}
