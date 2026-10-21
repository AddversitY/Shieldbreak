#include "Game/ShieldbreakGameMode.h"

#include "Shieldbreak.h"
#include "AI/ShieldbreakBotController.h"
#include "Game/ShieldbreakTeams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/ShieldbreakArena.h"
#include "Game/ShieldbreakGameState.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ShieldbreakCharacter.h"
#include "Player/ShieldbreakPlayerController.h"
#include "Player/ShieldbreakPlayerState.h"
#include "Test/ShieldbreakSelfTest.h"
#include "UI/ShieldbreakHUD.h"

// Named (not anonymous) so unity builds cannot collide with other files' helpers.
namespace ShieldbreakGameModePrivate
{
	static const TCHAR* const BotNames[] =
	{
		TEXT("Kestrel"), TEXT("Vanta"), TEXT("Ion"), TEXT("Sable"),
		TEXT("Talon"), TEXT("Ember"), TEXT("Rook"), TEXT("Nyx"),
		TEXT("Cobalt"), TEXT("Vesper"), TEXT("Juno"), TEXT("Quill"),
		TEXT("Argent"), TEXT("Lark"), TEXT("Brine"), TEXT("Zephyr")
	};

	constexpr int32 MaxBots = static_cast<int32>(UE_ARRAY_COUNT(BotNames));
	constexpr int32 MaxBotSkill = 3;
	constexpr int32 SpawnChoicePool = 3;
	constexpr float InitialBotDelay = 0.5f;
	constexpr float StrandedCheckInterval = 1.f;
	constexpr float DefaultCapsuleHalfHeight = 92.f;
	constexpr float FloorTraceUp = 100.f;
	constexpr float FloorTraceDown = 500.f;
	constexpr float FallbackTraceRange = 5000.f;

	struct FSpawnCandidate
	{
		int32 Index = 0;
		double Rating = 0.0;
	};
}

AShieldbreakGameMode::AShieldbreakGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	DefaultPawnClass = AShieldbreakCharacter::StaticClass();
	PlayerControllerClass = AShieldbreakPlayerController::StaticClass();
	PlayerStateClass = AShieldbreakPlayerState::StaticClass();
	GameStateClass = AShieldbreakGameState::StaticClass();
	HUDClass = AShieldbreakHUD::StaticClass();
	BotControllerClass = AShieldbreakBotController::StaticClass();

	PlayerColors =
	{
		FLinearColor(0.80f, 0.06f, 0.06f), // Red
		FLinearColor(0.06f, 0.24f, 0.90f), // Blue
		FLinearColor(0.12f, 0.55f, 0.10f), // Green
		FLinearColor(0.95f, 0.42f, 0.04f), // Orange
		FLinearColor(0.45f, 0.10f, 0.80f), // Violet
		FLinearColor(0.90f, 0.75f, 0.08f), // Yellow
		FLinearColor(0.04f, 0.75f, 0.85f), // Cyan
		FLinearColor(0.95f, 0.35f, 0.65f), // Pink
		FLinearColor(0.55f, 0.90f, 0.15f), // Lime
		FLinearColor(0.42f, 0.47f, 0.55f), // Steel
		FLinearColor(0.40f, 0.22f, 0.08f), // Brown
		FLinearColor(0.92f, 0.92f, 0.92f)  // White
	};
}

// ---- Game flow ----

void AShieldbreakGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	using namespace ShieldbreakGameModePrivate;
	NumBots = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Bots"), NumBots), 0, MaxBots);
	BotSkill = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Skill"), BotSkill), 0, MaxBotSkill);
	ScoreLimit = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("ScoreLimit"), ScoreLimit));
	bTeamGame = UGameplayStatics::GetIntOption(Options, TEXT("Teams"), bTeamGame ? 1 : 0) != 0;
	bFriendlyFire = UGameplayStatics::GetIntOption(Options, TEXT("FriendlyFire"), bFriendlyFire ? 1 : 0) != 0;
	if (bTeamGame)
	{
		TeamScoreLimit = FMath::Max(1, UGameplayStatics::GetIntOption(Options, TEXT("ScoreLimit"), TeamScoreLimit));
	}

	// Local players log in before BeginPlay, so the spawn points must exist this early.
	EnsureArena();
}

void AShieldbreakGameMode::InitGameState()
{
	Super::InitGameState();

	if (AShieldbreakGameState* SBGameState = GetGameState<AShieldbreakGameState>())
	{
		SBGameState->SetTeamGame(bTeamGame);
		SBGameState->SetScoreLimit(bTeamGame ? TeamScoreLimit : ScoreLimit);
	}
}

void AShieldbreakGameMode::StartPlay()
{
	EnsureArena();

	Super::StartPlay();

	if (AShieldbreakSelfTest::IsRequested())
	{
		// Scripted test run: no bots until the test asks for them, humans stay spectators.
		NumBots = 0;
		FActorSpawnParameters SelfTestParams;
		SelfTestParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GetWorld()->SpawnActor<AShieldbreakSelfTest>(AShieldbreakSelfTest::StaticClass(), FTransform::Identity, SelfTestParams);
	}

	GetWorldTimerManager().SetTimer(InitialBotsTimerHandle, this, &AShieldbreakGameMode::SpawnInitialBots,
		ShieldbreakGameModePrivate::InitialBotDelay, false);
}

void AShieldbreakGameMode::PostLogin(APlayerController* NewPlayer)
{
	// Name and tint before Super, which spawns the pawn (and the pawn reads the tint on possession).
	if (NewPlayer)
	{
		APlayerState* NewState = NewPlayer->GetPlayerState<APlayerState>();
		if (bUseGenericPlayerNames && NewState)
		{
			ChangeName(NewPlayer, MakeHumanName(NewState), false);
		}
		EnsurePlayerColor(NewPlayer);
	}

	Super::PostLogin(NewPlayer);
}

void AShieldbreakGameMode::Logout(AController* Exiting)
{
	RemovePendingRespawn(Exiting);
	Bots.RemoveAll([Exiting](const TObjectPtr<AShieldbreakBotController>& Bot)
	{
		return !IsValid(Bot.Get()) || Bot.Get() == Exiting;
	});

	Super::Logout(Exiting);
}

void AShieldbreakGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// During the banner nobody spawns; the reset brings everyone back at once.
	if (bMatchOver)
	{
		return;
	}

	ProcessPendingRespawns();

	StrandedCheckCountdown -= DeltaSeconds;
	if (StrandedCheckCountdown <= 0.f)
	{
		StrandedCheckCountdown = ShieldbreakGameModePrivate::StrandedCheckInterval;
		if (bBotsStarted)
		{
			ReconcileBots();
		}
		RescueStrandedControllers();
	}
}

AShieldbreakArena* AShieldbreakGameMode::EnsureArena()
{
	if (IsValid(CachedArena.Get()))
	{
		return CachedArena.Get();
	}

	UWorld* const CurrentWorld = GetWorld();
	if (!CurrentWorld)
	{
		return nullptr;
	}

	for (TActorIterator<AShieldbreakArena> It(CurrentWorld); It; ++It)
	{
		if (IsValid(*It))
		{
			CachedArena = *It;
			return CachedArena.Get();
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	CachedArena = CurrentWorld->SpawnActor<AShieldbreakArena>(AShieldbreakArena::StaticClass(), FTransform::Identity, SpawnParams);

	if (!CachedArena)
	{
		UE_LOG(LogShieldbreak, Error, TEXT("Failed to spawn AShieldbreakArena."));
	}
	return CachedArena.Get();
}

// ---- Spawning ----

void AShieldbreakGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!IsValid(NewPlayer) || NewPlayer->IsPendingKillPending())
	{
		return;
	}

	// Nobody spawns during the match-over banner; PerformMatchReset clears bMatchOver first.
	if (bMatchOver)
	{
		return;
	}

	// During a self-test run the human only watches.
	if (AShieldbreakSelfTest::IsRequested() && Cast<APlayerController>(NewPlayer))
	{
		return;
	}

	EnsureArena();
	EnsurePlayerColor(NewPlayer);
	RemovePendingRespawn(NewPlayer);

	// RestartPlayerAtTransform reuses an existing pawn, so never hand it a corpse.
	if (const AShieldbreakCharacter* ExistingCharacter = Cast<AShieldbreakCharacter>(NewPlayer->GetPawn()))
	{
		if (!ExistingCharacter->IsAlive())
		{
			NewPlayer->UnPossess();
		}
	}

	RestartPlayerAtTransform(NewPlayer, ChooseSpawnTransform(NewPlayer));
}

FTransform AShieldbreakGameMode::ChooseSpawnTransform(AController* ForController)
{
	using namespace ShieldbreakGameModePrivate;

	UWorld* const CurrentWorld = GetWorld();
	const AShieldbreakArena* Arena = EnsureArena();
	if (!CurrentWorld || !Arena || Arena->GetSpawnTransforms().Num() == 0)
	{
		// FindPlayerStart falls back to the WorldSettings (origin) when the map has no PlayerStarts.
		if (const APlayerStart* Start = Cast<APlayerStart>(FindPlayerStart(ForController, FString())))
		{
			return FTransform(FRotator(0.0, Start->GetActorRotation().Yaw, 0.0), Start->GetActorLocation());
		}

		UE_LOG(LogShieldbreak, Warning, TEXT("No arena spawn points; spawning on the topmost surface at the origin."));
		return FTransform(FRotator::ZeroRotator, SnapToFloor(FVector::ZeroVector, ForController, FallbackTraceRange, FallbackTraceRange));
	}

	const TArray<FTransform>& Spawns = Arena->GetSpawnTransforms();

	// Every other living character is an enemy, except teammates in Team Slayer.
	const uint8 SpawnTeam = ShieldbreakTeams::GetTeam(ForController);
	TArray<FVector> EnemyLocations;
	for (TActorIterator<AShieldbreakCharacter> It(CurrentWorld); It; ++It)
	{
		const AShieldbreakCharacter* Other = *It;
		if (IsValid(Other) && Other->IsAlive() && Other->GetController() != ForController
			&& !ShieldbreakTeams::AreTeammates(Other, ForController))
		{
			EnemyLocations.Add(Other->GetActorLocation());
		}
	}

	TArray<FSpawnCandidate> Candidates;
	Candidates.Reserve(Spawns.Num());
	for (int32 SpawnIndex = 0; SpawnIndex < Spawns.Num(); ++SpawnIndex)
	{
		FSpawnCandidate Candidate;
		Candidate.Index = SpawnIndex;

		if (EnemyLocations.Num() == 0)
		{
			Candidate.Rating = FMath::FRand();
		}
		else
		{
			const FVector SpawnLocation = Spawns[SpawnIndex].GetLocation();
			Candidate.Rating = TNumericLimits<double>::Max();
			for (const FVector& EnemyLocation : EnemyLocations)
			{
				Candidate.Rating = FMath::Min(Candidate.Rating, FVector::DistSquared(SpawnLocation, EnemyLocation));
			}
		}

		// Teams start from their own half of the (X-symmetric) arena: red at -X, blue at +X.
		if (SpawnTeam != ShieldbreakTeams::NoTeam)
		{
			const bool bOwnHalf = (SpawnTeam == ShieldbreakTeams::Red) == (Spawns[SpawnIndex].GetLocation().X < 0.0);
			if (bOwnHalf)
			{
				Candidate.Rating *= 2.0;
			}
		}

		Candidates.Add(Candidate);
	}

	Candidates.Sort([](const FSpawnCandidate& A, const FSpawnCandidate& B)
	{
		return A.Rating > B.Rating;
	});

	const int32 PoolSize = FMath::Min(SpawnChoicePool, Candidates.Num());
	const FTransform& Picked = Spawns[Candidates[FMath::RandRange(0, PoolSize - 1)].Index];

	return FTransform(FRotator(0.0, Picked.Rotator().Yaw, 0.0),
		SnapToFloor(Picked.GetLocation(), ForController, FloorTraceUp, FloorTraceDown));
}

FVector AShieldbreakGameMode::SnapToFloor(const FVector& SpawnLocation, AController* ForController, float TraceUp, float TraceDown)
{
	UWorld* const CurrentWorld = GetWorld();
	if (!CurrentWorld)
	{
		return SpawnLocation;
	}

	float HalfHeight = ShieldbreakGameModePrivate::DefaultCapsuleHalfHeight;
	if (UClass* SpawnPawnClass = GetDefaultPawnClassForController(ForController))
	{
		if (const ACharacter* CharacterDefaults = Cast<ACharacter>(SpawnPawnClass->GetDefaultObject()))
		{
			if (const UCapsuleComponent* Capsule = CharacterDefaults->GetCapsuleComponent())
			{
				HalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
			}
		}
	}

	// Works whether the arena's spawn points sit at the feet or at capsule center.
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShieldbreakSpawnFloor), false);
	const FVector TraceStart = SpawnLocation + FVector(0.f, 0.f, TraceUp);
	const FVector TraceEnd = SpawnLocation - FVector(0.f, 0.f, TraceDown);
	if (CurrentWorld->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		return FVector(SpawnLocation.X, SpawnLocation.Y, Hit.ImpactPoint.Z + HalfHeight + 5.0);
	}
	return SpawnLocation;
}

// ---- Kills ----

void AShieldbreakGameMode::NotifyKilled(AController* Killer, AController* Victim, AShieldbreakCharacter* VictimPawn)
{
	AController* VictimController = Victim ? Victim : (VictimPawn ? VictimPawn->GetController() : nullptr);

	// Nothing scores during the banner; the reset respawns everyone.
	if (bMatchOver)
	{
		if (AShieldbreakPlayerController* VictimPC = Cast<AShieldbreakPlayerController>(VictimController))
		{
			VictimPC->ClientSetRespawnTime(MatchRestartServerTime);
		}
		return;
	}

	AShieldbreakPlayerState* VictimState = VictimController ? VictimController->GetPlayerState<AShieldbreakPlayerState>() : nullptr;
	if (!VictimState && VictimPawn)
	{
		VictimState = VictimPawn->GetPlayerState<AShieldbreakPlayerState>();
	}
	AShieldbreakPlayerState* KillerState = Killer ? Killer->GetPlayerState<AShieldbreakPlayerState>() : nullptr;

	const bool bSuicide = !Killer || Killer == VictimController || (KillerState && KillerState == VictimState);

	if (VictimState)
	{
		VictimState->AddDeath();
		if (bSuicide)
		{
			VictimState->RemoveKill();
		}
	}
	if (!bSuicide && KillerState)
	{
		KillerState->AddKill();
	}

	const FString VictimName = VictimState ? VictimState->GetPlayerName() : FString(TEXT("Unknown"));
	const FLinearColor VictimColor = VictimState ? VictimState->GetPlayerColor() : FLinearColor::White;
	FString KillerName = VictimName;
	FLinearColor KillerColor = VictimColor;
	if (!bSuicide)
	{
		KillerName = KillerState ? KillerState->GetPlayerName() : FString(TEXT("Unknown"));
		KillerColor = KillerState ? KillerState->GetPlayerColor() : FLinearColor::White;
	}

	if (AShieldbreakGameState* SBGameState = GetGameState<AShieldbreakGameState>())
	{
		SBGameState->AddKillFeedEntry(KillerName, VictimName, KillerColor, VictimColor);
	}

	UE_LOG(LogShieldbreak, Log, TEXT("%s killed %s%s"), *KillerName, *VictimName, bSuicide ? TEXT(" (suicide)") : TEXT(""));

	if (VictimController)
	{
		ScheduleRespawn(VictimController, RespawnDelay);
	}

	if (bTeamGame)
	{
		AShieldbreakGameState* SBGameState = GetGameState<AShieldbreakGameState>();
		const uint8 VictimTeam = VictimState ? VictimState->GetTeamId() : ShieldbreakTeams::NoTeam;
		const uint8 KillerTeam = KillerState ? KillerState->GetTeamId() : ShieldbreakTeams::NoTeam;
		if (SBGameState)
		{
			if (bSuicide)
			{
				SBGameState->AddTeamScore(VictimTeam, -1);
			}
			else if (KillerState && KillerTeam == VictimTeam)
			{
				// Betrayal: undo the kill AddKill() just credited and take one more, and the team loses a point.
				KillerState->RemoveKill();
				KillerState->RemoveKill();
				SBGameState->AddTeamScore(KillerTeam, -1);
			}
			else
			{
				SBGameState->AddTeamScore(KillerTeam, 1);
				if (SBGameState->GetTeamScore(KillerTeam) >= TeamScoreLimit)
				{
					FinishMatch(ShieldbreakTeams::GetTeamName(KillerTeam));
				}
			}
		}
	}
	else if (!bSuicide && KillerState && KillerState->GetKills() >= ScoreLimit)
	{
		FinishMatch(KillerState->GetPlayerName());
	}
}

// ---- Bots ----

void AShieldbreakGameMode::SetBotCount(int32 NewCount)
{
	NumBots = FMath::Clamp(NewCount, 0, ShieldbreakGameModePrivate::MaxBots);

	GetWorldTimerManager().ClearTimer(InitialBotsTimerHandle);
	bBotsStarted = true;
	ReconcileBots();

	UE_LOG(LogShieldbreak, Log, TEXT("Bot count set to %d."), NumBots);
}

void AShieldbreakGameMode::SetBotSkill(int32 NewSkill)
{
	BotSkill = FMath::Clamp(NewSkill, 0, ShieldbreakGameModePrivate::MaxBotSkill);

	for (AShieldbreakBotController* Bot : Bots)
	{
		if (IsValid(Bot))
		{
			Bot->SetSkill(BotSkill);
		}
	}

	UE_LOG(LogShieldbreak, Log, TEXT("Bot skill set to %d."), BotSkill);
}

void AShieldbreakGameMode::SpawnInitialBots()
{
	bBotsStarted = true;
	ReconcileBots();
}

void AShieldbreakGameMode::ReconcileBots()
{
	Bots.RemoveAll([](const TObjectPtr<AShieldbreakBotController>& Bot)
	{
		return !IsValid(Bot.Get()) || Bot->IsPendingKillPending();
	});

	while (Bots.Num() < NumBots && AddBot())
	{
	}

	while (Bots.Num() > NumBots)
	{
		RemoveLastBot();
	}
}

bool AShieldbreakGameMode::AddBot()
{
	UWorld* const CurrentWorld = GetWorld();
	UClass* const ClassToSpawn = BotControllerClass ? BotControllerClass.Get() : AShieldbreakBotController::StaticClass();
	if (!CurrentWorld || !ClassToSpawn)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	// The bot's PlayerState is created during spawn (bWantsPlayerState, PostInitializeComponents).
	AShieldbreakBotController* Bot = CurrentWorld->SpawnActor<AShieldbreakBotController>(ClassToSpawn, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!Bot)
	{
		UE_LOG(LogShieldbreak, Warning, TEXT("Failed to spawn a bot controller."));
		return false;
	}

	Bots.Add(Bot);

	if (APlayerState* BotState = Bot->GetPlayerState<APlayerState>())
	{
		BotState->SetPlayerName(MakeBotName());
	}
	else
	{
		UE_LOG(LogShieldbreak, Warning, TEXT("Bot %s has no PlayerState (bWantsPlayerState?)."), *Bot->GetName());
	}

	EnsurePlayerColor(Bot);
	Bot->SetSkill(BotSkill);
	RestartPlayer(Bot);
	return true;
}

void AShieldbreakGameMode::RemoveLastBot()
{
	if (Bots.Num() == 0)
	{
		return;
	}

	AShieldbreakBotController* Bot = Bots.Last().Get();
	Bots.RemoveAt(Bots.Num() - 1);
	if (!IsValid(Bot))
	{
		return;
	}

	RemovePendingRespawn(Bot);
	if (APawn* BotPawn = Bot->GetPawn())
	{
		BotPawn->Destroy();
	}
	Bot->Destroy();
}

FString AShieldbreakGameMode::MakeBotName() const
{
	TSet<FString> Taken;
	if (GameState)
	{
		for (const APlayerState* Other : GameState->PlayerArray)
		{
			if (Other)
			{
				Taken.Add(Other->GetPlayerName());
			}
		}
	}

	for (const TCHAR* BaseName : ShieldbreakGameModePrivate::BotNames)
	{
		FString Candidate = FString::Printf(TEXT("Bot %s"), BaseName);
		if (!Taken.Contains(Candidate))
		{
			return Candidate;
		}
	}
	return FString::Printf(TEXT("Bot %d"), Taken.Num() + 1);
}

// ---- Players ----

FString AShieldbreakGameMode::MakeHumanName(const APlayerState* ForState) const
{
	TSet<FString> Taken;
	if (GameState)
	{
		for (const APlayerState* Other : GameState->PlayerArray)
		{
			if (Other && Other != ForState)
			{
				Taken.Add(Other->GetPlayerName());
			}
		}
	}

	for (int32 Number = 1; Number <= 64; ++Number)
	{
		FString Candidate = FString::Printf(TEXT("Player %d"), Number);
		if (!Taken.Contains(Candidate))
		{
			return Candidate;
		}
	}
	return FString(TEXT("Player"));
}

void AShieldbreakGameMode::EnsurePlayerColor(AController* ForController)
{
	if (bTeamGame)
	{
		AssignTeam(ForController);
		return;
	}

	AShieldbreakPlayerState* ForState = ForController ? ForController->GetPlayerState<AShieldbreakPlayerState>() : nullptr;
	if (!ForState || ForState->GetColorIndex() != INDEX_NONE || PlayerColors.Num() == 0)
	{
		return;
	}

	TArray<bool> Used;
	Used.Init(false, PlayerColors.Num());
	int32 OtherCount = 0;

	if (GameState)
	{
		for (const APlayerState* Other : GameState->PlayerArray)
		{
			const AShieldbreakPlayerState* OtherState = Cast<AShieldbreakPlayerState>(Other);
			if (!OtherState || OtherState == ForState)
			{
				continue;
			}

			++OtherCount;
			if (Used.IsValidIndex(OtherState->GetColorIndex()))
			{
				Used[OtherState->GetColorIndex()] = true;
			}
		}
	}

	int32 Slot = Used.Find(false);
	if (Slot == INDEX_NONE)
	{
		Slot = OtherCount % PlayerColors.Num();
	}
	ForState->SetPlayerColor(PlayerColors[Slot], Slot);
}

void AShieldbreakGameMode::AssignTeam(AController* ForController)
{
	AShieldbreakPlayerState* ForState = ForController ? ForController->GetPlayerState<AShieldbreakPlayerState>() : nullptr;
	if (!ForState)
	{
		return;
	}

	uint8 Team = ForState->GetTeamId();
	if (Team == ShieldbreakTeams::NoTeam)
	{
		int32 Counts[ShieldbreakTeams::NumTeams] = { 0, 0 };
		if (GameState)
		{
			for (const APlayerState* Other : GameState->PlayerArray)
			{
				const AShieldbreakPlayerState* OtherState = Cast<AShieldbreakPlayerState>(Other);
				if (OtherState && OtherState != ForState && OtherState->GetTeamId() < ShieldbreakTeams::NumTeams)
				{
					++Counts[OtherState->GetTeamId()];
				}
			}
		}
		Team = Counts[ShieldbreakTeams::Blue] < Counts[ShieldbreakTeams::Red] ? ShieldbreakTeams::Blue : ShieldbreakTeams::Red;
		ForState->SetTeamId(Team);
	}
	ForState->SetPlayerColor(ShieldbreakTeams::GetTeamColor(Team), Team);
}

bool AShieldbreakGameMode::CanParticipate(AController* Candidate)
{
	if (!IsValid(Candidate) || Candidate->IsPendingKillPending())
	{
		return false;
	}

	const APlayerState* CandidateState = Candidate->GetPlayerState<APlayerState>();
	if (!CandidateState || CandidateState->IsOnlyASpectator())
	{
		return false;
	}

	if (APlayerController* CandidatePC = Cast<APlayerController>(Candidate))
	{
		return PlayerCanRestart(CandidatePC);
	}
	return Cast<AShieldbreakBotController>(Candidate) != nullptr;
}

// ---- Respawning ----

void AShieldbreakGameMode::ScheduleRespawn(AController* Who, float Delay)
{
	if (!IsValid(Who))
	{
		return;
	}

	RemovePendingRespawn(Who);

	FShieldbreakPendingRespawn& Pending = PendingRespawns.AddDefaulted_GetRef();
	Pending.Who = Who;
	Pending.RespawnServerTime = GetServerTimeSeconds() + FMath::Max(0.f, Delay);

	if (AShieldbreakPlayerController* WhoPC = Cast<AShieldbreakPlayerController>(Who))
	{
		WhoPC->ClientSetRespawnTime(Pending.RespawnServerTime);
	}
}

void AShieldbreakGameMode::RemovePendingRespawn(const AController* Who)
{
	PendingRespawns.RemoveAll([Who](const FShieldbreakPendingRespawn& Pending)
	{
		return Pending.Who.Get() == Who;
	});
}

bool AShieldbreakGameMode::IsRespawnPending(const AController* Who) const
{
	return PendingRespawns.ContainsByPredicate([Who](const FShieldbreakPendingRespawn& Pending)
	{
		return Pending.Who.Get() == Who;
	});
}

void AShieldbreakGameMode::ProcessPendingRespawns()
{
	if (PendingRespawns.Num() == 0)
	{
		return;
	}

	const float Now = GetServerTimeSeconds();
	TArray<AController*> Due;

	for (int32 PendingIndex = PendingRespawns.Num() - 1; PendingIndex >= 0; --PendingIndex)
	{
		AController* Who = PendingRespawns[PendingIndex].Who.Get();
		if (!IsValid(Who))
		{
			PendingRespawns.RemoveAt(PendingIndex);
		}
		else if (Now >= PendingRespawns[PendingIndex].RespawnServerTime)
		{
			Due.Add(Who);
			PendingRespawns.RemoveAt(PendingIndex);
		}
	}

	// RestartPlayer edits PendingRespawns, so spawn only after the scan.
	for (AController* Who : Due)
	{
		RespawnNow(Who);
	}
}

void AShieldbreakGameMode::RespawnNow(AController* Who)
{
	if (!CanParticipate(Who))
	{
		return;
	}

	RestartPlayer(Who);

	// A failed spawn keeps the old countdown; RescueStrandedControllers will queue a retry.
	if (Who->GetPawn())
	{
		if (AShieldbreakPlayerController* WhoPC = Cast<AShieldbreakPlayerController>(Who))
		{
			WhoPC->ClientSetRespawnTime(-1.f);
		}
	}
}

void AShieldbreakGameMode::RescueStrandedControllers()
{
	UWorld* const CurrentWorld = GetWorld();
	if (!CurrentWorld)
	{
		return;
	}

	// Catches pawns destroyed without dying (e.g. fell out of the world) and failed spawns.
	TArray<AController*> Stranded;
	for (FConstControllerIterator It = CurrentWorld->GetControllerIterator(); It; ++It)
	{
		AController* Candidate = It->Get();
		if (!Candidate || IsRespawnPending(Candidate))
		{
			continue;
		}

		const AShieldbreakCharacter* CandidateCharacter = Cast<AShieldbreakCharacter>(Candidate->GetPawn());
		const bool bNeedsPawn = !Candidate->GetPawn() || (CandidateCharacter && !CandidateCharacter->IsAlive());
		if (bNeedsPawn && CanParticipate(Candidate))
		{
			Stranded.Add(Candidate);
		}
	}

	for (AController* Candidate : Stranded)
	{
		ScheduleRespawn(Candidate, RespawnDelay);
	}
}

// ---- Match flow ----

void AShieldbreakGameMode::FinishMatch(const FString& Winner)
{
	if (bMatchOver)
	{
		return;
	}

	bMatchOver = true;
	MatchRestartServerTime = GetServerTimeSeconds() + MatchOverDuration;

	if (AShieldbreakGameState* SBGameState = GetGameState<AShieldbreakGameState>())
	{
		SBGameState->SetMatchOver(Winner, MatchRestartServerTime);
	}

	// Anyone waiting to respawn now comes back with the reset instead.
	for (const FShieldbreakPendingRespawn& Pending : PendingRespawns)
	{
		if (AShieldbreakPlayerController* PendingPC = Cast<AShieldbreakPlayerController>(Pending.Who.Get()))
		{
			PendingPC->ClientSetRespawnTime(MatchRestartServerTime);
		}
	}
	PendingRespawns.Reset();

	// Freeze damage for the banner so the final score stands. Fresh pawns after the reset take damage again.
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		It->SetCanBeDamaged(false);
	}

	GetWorldTimerManager().SetTimer(MatchResetTimerHandle, this, &AShieldbreakGameMode::PerformMatchReset,
		FMath::Max(MatchOverDuration, 0.01f), false);

	UE_LOG(LogShieldbreak, Log, TEXT("Match over: %s wins. Restarting in %.0f s."), *Winner, MatchOverDuration);
}

void AShieldbreakGameMode::PerformMatchReset()
{
	UWorld* const CurrentWorld = GetWorld();
	if (!CurrentWorld)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(MatchResetTimerHandle);
	bMatchOver = false;
	MatchRestartServerTime = -1.f;
	PendingRespawns.Reset();
	StrandedCheckCountdown = ShieldbreakGameModePrivate::StrandedCheckInterval;

	// Clears the kill feed and banner and zeroes every PlayerState's stats.
	if (AShieldbreakGameState* SBGameState = GetGameState<AShieldbreakGameState>())
	{
		SBGameState->ResetMatch();
	}

	TArray<AShieldbreakCharacter*> OldPawns;
	for (TActorIterator<AShieldbreakCharacter> It(CurrentWorld); It; ++It)
	{
		OldPawns.Add(*It);
	}
	for (AShieldbreakCharacter* OldPawn : OldPawns)
	{
		if (IsValid(OldPawn))
		{
			OldPawn->Destroy();
		}
	}

	TArray<AController*> Participants;
	for (FConstControllerIterator It = CurrentWorld->GetControllerIterator(); It; ++It)
	{
		AController* Candidate = It->Get();
		if (CanParticipate(Candidate))
		{
			Participants.Add(Candidate);
		}
	}
	for (AController* Participant : Participants)
	{
		RespawnNow(Participant);
	}

	UE_LOG(LogShieldbreak, Log, TEXT("Match reset: %d participants respawned."), Participants.Num());
}

float AShieldbreakGameMode::GetServerTimeSeconds() const
{
	if (GameState)
	{
		return static_cast<float>(GameState->GetServerWorldTimeSeconds());
	}

	const UWorld* CurrentWorld = GetWorld();
	return CurrentWorld ? static_cast<float>(CurrentWorld->GetTimeSeconds()) : 0.f;
}
