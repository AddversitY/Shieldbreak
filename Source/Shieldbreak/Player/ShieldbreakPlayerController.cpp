#include "Player/ShieldbreakPlayerController.h"

#include "Audio/ShieldbreakSFX.h"
#include "Shieldbreak.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/ShieldbreakGameMode.h"
#include "Game/ShieldbreakGameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "Player/ShieldbreakInputSet.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UI/SShieldbreakMainMenu.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWeakWidget.h"
#include "InputCoreTypes.h"
#include "Engine/Engine.h"
#include "Test/ShieldbreakNetworkTest.h"
#include "Test/ShieldbreakConnectionTest.h"

void AShieldbreakPlayerController::ShowMainMenu()
{
	if (!IsLocalController() || !GEngine || !GEngine->GameViewport) { return; }
	if (!MainMenu.IsValid())
	{
		const AShieldbreakGameMode* Rules = GetWorld()->GetAuthGameMode<AShieldbreakGameMode>();
		const bool bInMatch = !Rules || !Rules->IsFrontEnd();
		SAssignNew(MainMenu, SShieldbreakMainMenu).Controller(this).InMatch(bInMatch);
		GEngine->GameViewport->AddViewportWidgetContent(MainMenu.ToSharedRef(), 100);
		FInputModeUIOnly MenuInput;
		MenuInput.SetWidgetToFocus(MainMenu).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(MenuInput);
		SetShowMouseCursor(true);
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
	}
	if (UShieldbreakGameInstance* Instance = Cast<UShieldbreakGameInstance>(GetGameInstance()))
	{
		const FString Message = Instance->ConsumeConnectionError();
		if (!Message.IsEmpty()) { MainMenu->ShowConnectionError(Message, Instance->GetLastJoinAddress()); }
	}
}

void AShieldbreakPlayerController::CloseMainMenu()
{
	if (!MainMenu.IsValid()) { return; }
	if (GEngine && GEngine->GameViewport) { GEngine->GameViewport->RemoveViewportWidgetContent(MainMenu.ToSharedRef()); }
	MainMenu.Reset();
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
}

void AShieldbreakPlayerController::OnEscape()
{
	if (MainMenu.IsValid()) { CloseMainMenu(); }
	else { ShowMainMenu(); }
}

void AShieldbreakPlayerController::JoinMatch(const FString& Address)
{
	if (!IsLocalController()) { return; }
	if (UShieldbreakGameInstance* Instance = Cast<UShieldbreakGameInstance>(GetGameInstance()))
	{
		Instance->BeginJoin(Address);
	}
	ClientTravel(Address, TRAVEL_Absolute);
}

void AShieldbreakPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseMainMenu();
	Super::EndPlay(EndPlayReason);
}

AShieldbreakPlayerController::AShieldbreakPlayerController()
{
}

void AShieldbreakPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		AddInputContext();
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		if (const UWorld* World = GetWorld())
		{
			HelpAutoHideTime = World->GetTimeSeconds() + 12.f;
		}
		SetupAutoShots();
		FShieldbreakNetworkTest::Start(this);
		AutoDetectScalability();
		if (const AShieldbreakGameMode* Rules = GetWorld()->GetAuthGameMode<AShieldbreakGameMode>())
		{
			if (Rules->IsFrontEnd()) { ShowMainMenu(); }
		}
		FShieldbreakConnectionTest::Start(this);
	}
}

void AShieldbreakPlayerController::AddInputContext()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		UShieldbreakInputSet* Inputs = UShieldbreakInputSet::Get();
		if (Inputs && Inputs->Context && !Subsystem->HasMappingContext(Inputs->Context))
		{
			Subsystem->AddMappingContext(Inputs->Context, 0);
		}
	}
}

void AShieldbreakPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AShieldbreakPlayerController::OnEscape);

	UShieldbreakInputSet* Inputs = UShieldbreakInputSet::Get();
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInput->BindAction(Inputs->Scoreboard, ETriggerEvent::Started, this, &AShieldbreakPlayerController::OnScoreboardStarted);
		EnhancedInput->BindAction(Inputs->Scoreboard, ETriggerEvent::Completed, this, &AShieldbreakPlayerController::OnScoreboardCompleted);
		EnhancedInput->BindAction(Inputs->ToggleHelp, ETriggerEvent::Started, this, &AShieldbreakPlayerController::OnToggleHelp);
	}
	else
	{
		UE_LOG(LogShieldbreak, Error, TEXT("InputComponent is not an EnhancedInputComponent; check Config/DefaultInput.ini."));
	}
}

void AShieldbreakPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	RespawnServerTime = -1.f;
}

void AShieldbreakPlayerController::OnUnPossess()
{
	APawn* PreviousPawn = GetPawn();
	Super::OnUnPossess();

	// Keep watching the body until the respawn.
	if (IsValid(PreviousPawn))
	{
		SetViewTarget(PreviousPawn);
	}
}

bool AShieldbreakPlayerController::IsHelpVisible() const
{
	if (HelpOverride >= 0)
	{
		return HelpOverride == 1;
	}
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < HelpAutoHideTime;
}

void AShieldbreakPlayerController::OnScoreboardStarted()
{
	bScoreboardVisible = true;
}

void AShieldbreakPlayerController::OnScoreboardCompleted()
{
	bScoreboardVisible = false;
}

void AShieldbreakPlayerController::OnToggleHelp()
{
	HelpOverride = IsHelpVisible() ? 0 : 1;
}

// ---- Feedback ----

void AShieldbreakPlayerController::ClientHitConfirmed_Implementation(bool bKill, bool bHeadshot)
{
	if (const UWorld* World = GetWorld())
	{
		LastHitConfirmTime = World->GetTimeSeconds();
	}
	bLastHitKill = bKill;
	bLastHitHeadshot = bHeadshot;
	FShieldbreakSFX::Play2D(this, bKill ? EShieldbreakSound::KillMarker : EShieldbreakSound::HitMarker);
}

void AShieldbreakPlayerController::ClientDamagedFrom_Implementation(FVector_NetQuantize SourceLocation)
{
	if (const UWorld* World = GetWorld())
	{
		LastDamagedTime = World->GetTimeSeconds();
	}
	LastDamageSource = SourceLocation;
}

void AShieldbreakPlayerController::ClientSetRespawnTime_Implementation(float ServerTimeOfRespawn)
{
	RespawnServerTime = ServerTimeOfRespawn;
}

// ---- Console ----

void AShieldbreakPlayerController::SB_Bots(int32 Count)
{
	ServerSetBots(Count);
}

void AShieldbreakPlayerController::SB_Skill(int32 Skill)
{
	ServerSetBotSkill(Skill);
}

bool AShieldbreakPlayerController::AuthorizeMatchControl(const TCHAR* CommandName)
{
	// Evaluate locality on the authority, never on the owning client that sent the RPC.
	if (HasAuthority() && IsLocalController()) { return true; }
	UE_LOG(LogShieldbreak, Warning, TEXT("Rejected remote match control: %s"), CommandName);
	ClientMessage(TEXT("Only the host can change match settings."));
	return false;
}

void AShieldbreakPlayerController::ServerSetBots_Implementation(int32 Count)
{
	if (!AuthorizeMatchControl(TEXT("SB_Bots"))) { return; }
	if (AShieldbreakGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AShieldbreakGameMode>() : nullptr)
	{
		GameMode->SetBotCount(Count);
	}
}

void AShieldbreakPlayerController::ServerSetBotSkill_Implementation(int32 Skill)
{
	if (!AuthorizeMatchControl(TEXT("SB_Skill"))) { return; }
	if (AShieldbreakGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AShieldbreakGameMode>() : nullptr)
	{
		GameMode->SetBotSkill(Skill);
	}
}

// ---- Play-test screenshots ----

void AShieldbreakPlayerController::SetupAutoShots()
{
	float FirstDelay = 0.f;
	if (!FParse::Value(FCommandLine::Get(), TEXT("SBAutoShot="), FirstDelay) || FirstDelay <= 0.f)
	{
		return;
	}
	FParse::Value(FCommandLine::Get(), TEXT("SBAutoShotCount="), AutoShotCount);
	FParse::Value(FCommandLine::Get(), TEXT("SBAutoShotInterval="), AutoShotInterval);
	AutoShotCount = FMath::Clamp(AutoShotCount, 1, 30);
	AutoShotInterval = FMath::Max(0.5f, AutoShotInterval);

	GetWorldTimerManager().SetTimer(AutoShotTimerHandle, this, &AShieldbreakPlayerController::TakeAutoShot, AutoShotInterval, true, FirstDelay);
	UE_LOG(LogShieldbreak, Log, TEXT("AutoShot: %d shots, first at %.1fs, every %.1fs."), AutoShotCount, FirstDelay, AutoShotInterval);
}

void AShieldbreakPlayerController::TakeAutoShot()
{
	if (AutoShotIndex >= AutoShotCount)
	{
		GetWorldTimerManager().ClearTimer(AutoShotTimerHandle);
		if (FParse::Param(FCommandLine::Get(), TEXT("SBAutoQuit")))
		{
			UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
		}
		return;
	}

	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("AutoShot");
	IFileManager::Get().MakeDirectory(*Dir, true);
	// -SBAutoShotPrefix=name keeps two instances (listen server + client) from overwriting each other.
	FString Prefix = TEXT("shot");
	FParse::Value(FCommandLine::Get(), TEXT("SBAutoShotPrefix="), Prefix);
	const FString File = Dir / FString::Printf(TEXT("%s_%02d.png"), *Prefix, AutoShotIndex);
	FScreenshotRequest::RequestScreenshot(File, true, false);
	UE_LOG(LogShieldbreak, Log, TEXT("AutoShot: requested %s"), *File);
	++AutoShotIndex;
}

// ---- First-run graphics settings ----

void AShieldbreakPlayerController::AutoDetectScalability()
{
	// Standalone game only: in the editor this would also change the editor viewport's settings.
	if (GIsEditor || !GEngine)
	{
		return;
	}

	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (!Settings || Settings->GetLastGPUBenchmarkResult() >= 0.f)
	{
		return; // Already benchmarked on this machine (or the player picked settings).
	}

	// Lumen and virtual shadow maps at Epic settings run at a few FPS on integrated GPUs;
	// let Unreal's hardware benchmark pick the scalability level once, then remember it.
	Settings->RunHardwareBenchmark();
	Settings->ApplyHardwareBenchmarkResults();
	UE_LOG(LogShieldbreak, Log, TEXT("Scalability auto-detected: overall level %d (CPU %.0f, GPU %.0f)."),
		Settings->GetOverallScalabilityLevel(), Settings->GetLastCPUBenchmarkResult(), Settings->GetLastGPUBenchmarkResult());
}

void AShieldbreakPlayerController::SB_Map(const FString& Map, const FString& Mode)
{
	ServerChangeMap(Map, Mode);
}

void AShieldbreakPlayerController::ServerChangeMap_Implementation(const FString& Map, const FString& Mode)
{
	if (!AuthorizeMatchControl(TEXT("SB_Map"))) { return; }
	if (AShieldbreakGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AShieldbreakGameMode>() : nullptr)
	{
		GameMode->ChangeMap(Map, Mode);
	}
}
