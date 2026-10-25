#include "Player/ShieldbreakPlayerController.h"

#include "Audio/ShieldbreakSFX.h"
#include "Player/ShieldbreakSettings.h"
#include "Shieldbreak.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/ShieldbreakGameMode.h"
#include "GameFramework/GameUserSettings.h"
#include "Player/ShieldbreakInputSet.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UI/SShieldbreakMenus.h"
#include "Widgets/SWeakWidget.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

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
		ShieldbreakSettings::ApplyAudio();
		AutoDetectScalability();
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

	UShieldbreakInputSet* Inputs = UShieldbreakInputSet::Get();
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInput->BindAction(Inputs->Scoreboard, ETriggerEvent::Started, this, &AShieldbreakPlayerController::OnScoreboardStarted);
		EnhancedInput->BindAction(Inputs->Scoreboard, ETriggerEvent::Completed, this, &AShieldbreakPlayerController::OnScoreboardCompleted);
		EnhancedInput->BindAction(Inputs->ToggleHelp, ETriggerEvent::Started, this, &AShieldbreakPlayerController::OnToggleHelp);
		EnhancedInput->BindAction(Inputs->Pause, ETriggerEvent::Started, this, &AShieldbreakPlayerController::OnPause);
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

void AShieldbreakPlayerController::OnPause()
{
	// While open the menu has focus and handles Esc itself; this only opens it.
	OpenPauseMenu();
}

void AShieldbreakPlayerController::OpenPauseMenu()
{
	if (PauseMenu.IsValid() || !IsLocalController() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	PauseMenu = SNew(SShieldbreakPauseMenu).Owner(this);
	PauseMenuContainer = SNew(SWeakWidget).PossiblyNullContent(PauseMenu);
	GEngine->GameViewport->AddViewportWidgetContent(PauseMenuContainer.ToSharedRef(), 20);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(PauseMenu);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
	// Drop held inputs (fire, move) so the character doesn't keep shooting or running behind the menu.
	FlushPressedKeys();
}

void AShieldbreakPlayerController::ClosePauseMenu()
{
	if (!PauseMenu.IsValid())
	{
		return;
	}
	if (GEngine && GEngine->GameViewport && PauseMenuContainer.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(PauseMenuContainer.ToSharedRef());
	}
	PauseMenu.Reset();
	PauseMenuContainer.Reset();
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
}

void AShieldbreakPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClosePauseMenu();
	Super::EndPlay(EndPlayReason);
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

void AShieldbreakPlayerController::ServerSetBots_Implementation(int32 Count)
{
	if (AShieldbreakGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AShieldbreakGameMode>() : nullptr)
	{
		GameMode->SetBotCount(Count);
	}
}

void AShieldbreakPlayerController::ServerSetBotSkill_Implementation(int32 Skill)
{
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
	ShieldbreakSettings::AutoDetectGraphics();
}

void AShieldbreakPlayerController::SB_Map(const FString& Map, const FString& Mode)
{
	ServerChangeMap(Map, Mode);
}

void AShieldbreakPlayerController::ServerChangeMap_Implementation(const FString& Map, const FString& Mode)
{
	if (AShieldbreakGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AShieldbreakGameMode>() : nullptr)
	{
		GameMode->ChangeMap(Map, Mode);
	}
}
