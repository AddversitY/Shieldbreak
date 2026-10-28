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
#include "Player/ShieldbreakCharacter.h"
#include "Shield/ShieldHealthComponent.h"
#include "Weapons/ShieldbreakDamageTypes.h"
#include "TimerManager.h"

void FShieldbreakNetworkTest::Start(AShieldbreakPlayerController* TestController)
{
	if (!TestController || !TestController->IsLocalController()) { return; }
	UWorld* TestWorld = TestController->GetWorld();
	if (!TestWorld) { return; }
	const TWeakObjectPtr<AShieldbreakPlayerController> WeakController(TestController);
	if (FParse::Param(FCommandLine::Get(), TEXT("SBShieldFeedbackTest")))
	{
		if (TestWorld->GetNetMode() == NM_Client)
		{
			UE_LOG(LogShieldbreak, Log, TEXT("ShieldFeedback: client ready."));
			return;
		}
		const TSharedRef<FTimerHandle> ImpactTimer = MakeShared<FTimerHandle>();
		TestWorld->GetTimerManager().SetTimer(*ImpactTimer, FTimerDelegate::CreateLambda([WeakController, ImpactTimer, Stage = -4]() mutable
		{
			AShieldbreakPlayerController* Host = WeakController.Get();
			UWorld* World = Host ? Host->GetWorld() : nullptr;
			const auto* State = World ? World->GetGameState<AShieldbreakGameState>() : nullptr;
			auto* Pawn = Host ? Cast<AShieldbreakCharacter>(Host->GetPawn()) : nullptr;
			if (!State || State->PlayerArray.Num() < 2 || !Pawn || !Pawn->GetShieldHealth()) { return; }
			UShieldHealthComponent* Shield = Pawn->GetShieldHealth();
			// Wait two seconds after the human joins, so the cosmetic receiver exists.
			if (Stage == 0 || Stage == 1 || Stage == 2 || Stage == 3 || Stage == 8)
			{
				const float Damage = Stage < 2 ? 20.f : Stage == 2 ? 40.f : 1.f;
				Shield->ApplyShieldbreakDamage(Damage, GetDefault<UDamageType_Ballistic>(), nullptr, Pawn);
				Pawn->ForceNetUpdate();
			}
			if (Stage == 4) { Shield->GrantOvershield(10.f); }
			if (++Stage > 8)
			{
				UE_LOG(LogShieldbreak, Log, TEXT("ShieldFeedback: probe completed (four impacts, one break; no health-only/decay impact)."));
				World->GetTimerManager().ClearTimer(*ImpactTimer);
			}
		}), 0.5f, true);
		return;
	}
	if (!FParse::Param(FCommandLine::Get(), TEXT("SBNetworkTest"))) { return; }
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
