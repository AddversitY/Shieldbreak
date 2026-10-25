#include "UI/ShieldbreakMenu.h"

#include "Audio/ShieldbreakMusic.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Game/ShieldbreakArena.h"
#include "Game/ShieldbreakMaps.h"
#include "Player/ShieldbreakSettings.h"
#include "Shieldbreak.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UnrealClient.h"
#include "UI/SShieldbreakMenus.h"
#include "Widgets/SWeakWidget.h"

// ---- Launch logic ----

bool ShieldbreakMenu::ShouldShowMainMenu(const FString& Options, const TCHAR* CommandLine)
{
	if (UGameplayStatics::HasOption(Options, TEXT("Menu")))
	{
		return true;
	}
	if (IsRunningDedicatedServer())
	{
		return false;
	}
	// Any match option means a script or the menu itself asked for a match.
	for (const TCHAR* Key : { TEXT("Map"), TEXT("Mode"), TEXT("Bots"), TEXT("listen") })
	{
		if (UGameplayStatics::HasOption(Options, Key))
		{
			return false;
		}
	}
	// Self-tests and play-test screenshots expect to land in a match.
	FString Unused;
	if (FParse::Param(CommandLine, TEXT("SBSelfTest")) || FParse::Param(CommandLine, TEXT("SBNoMenu"))
		|| FParse::Value(CommandLine, TEXT("SBAutoShot="), Unused))
	{
		return false;
	}
	return true;
}

FString ShieldbreakMenu::BuildMatchOptions(FName MapId, const FString& Mode, int32 Bots, int32 Skill, bool bListen)
{
	FString Options = FString::Printf(TEXT("Map=%s?Mode=%s?Bots=%d?Skill=%d"), *MapId.ToString(), *Mode, Bots, Skill);
	if (bListen)
	{
		Options += TEXT("?listen");
	}
	return Options;
}

void ShieldbreakMenu::ReturnToMainMenu(const UObject* WorldContext)
{
	UGameplayStatics::OpenLevel(WorldContext, FName(TEXT("/Engine/Maps/Entry")), true, TEXT("Menu"));
}

const TArray<FString>& ShieldbreakMenu::GetModes()
{
	static const TArray<FString> Modes = { TEXT("Slayer"), TEXT("CTF"), TEXT("KOTH"), TEXT("Oddball") };
	return Modes;
}

// ---- Game instance ----

TSubclassOf<AGameModeBase> UShieldbreakGameInstance::OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass, const FString& MapName, const FString& Options, const FString& Portal) const
{
	if (ShieldbreakMenu::ShouldShowMainMenu(Options, FCommandLine::Get()))
	{
		return AShieldbreakMenuGameMode::StaticClass();
	}
	return Super::OverrideGameModeClass(GameModeClass, MapName, Options, Portal);
}

// ---- Menu game mode and controller ----

AShieldbreakMenuGameMode::AShieldbreakMenuGameMode()
{
	PlayerControllerClass = AShieldbreakMenuController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = AHUD::StaticClass();
	bStartPlayersAsSpectators = true;
}

AShieldbreakMenuController::AShieldbreakMenuController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AShieldbreakMenuController::ShowBackdrop(FName MapId)
{
	if (MapId == BackdropMapId || !IsLocalController())
	{
		return;
	}
	BackdropMapId = MapId;
	// The first map goes up at once; changes wait until the player stops flicking through the list.
	const float Delay = bBackdropBuilt ? 0.45f : 0.01f;
	GetWorldTimerManager().SetTimer(BackdropTimerHandle, this, &AShieldbreakMenuController::BuildBackdrop, Delay, false);
}

void AShieldbreakMenuController::BuildBackdrop()
{
	UWorld* World = GetWorld();
	if (!World || BackdropMapId.IsNone() || FParse::Param(FCommandLine::Get(), TEXT("SBMenuNoBackdrop")))
	{
		return;
	}
	const AShieldbreakArena* Arena = AShieldbreakArena::EnsureLocal(World, BackdropMapId);
	if (!Arena)
	{
		return;
	}
	const FShieldbreakMapLayout& Layout = Arena->GetLayout();
	const float Reach = static_cast<float>(FMath::Max(Layout.HalfExtent.X, Layout.HalfExtent.Y));
	// Outside the walls and high enough to look down into the whole map (the first version sat behind the wall).
	OrbitCenter = FVector(0.f, 0.f, Layout.FloorZ + 200.f);
	OrbitRadius = Reach * 1.55f;
	OrbitHeight = FMath::Max(2500.f, Reach * 0.9f);

	if (!BackdropCamera)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		BackdropCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
		if (BackdropCamera)
		{
			BackdropCamera->GetCameraComponent()->SetFieldOfView(60.f);
			BackdropCamera->GetCameraComponent()->bConstrainAspectRatio = false;
		}
	}
	if (BackdropCamera)
	{
		SetViewTarget(BackdropCamera);
	}
	bBackdropBuilt = true;
	Tick(0.f);
}

void AShieldbreakMenuController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!BackdropCamera)
	{
		return;
	}
	// A slow circle, looking past the middle of the map with the sky above the far side.
	OrbitYaw = FMath::Fmod(OrbitYaw + DeltaSeconds * 2.5f, 360.f);
	const float Radians = FMath::DegreesToRadians(OrbitYaw);
	const FVector Eye = OrbitCenter + FVector(FMath::Cos(Radians) * OrbitRadius, FMath::Sin(Radians) * OrbitRadius, OrbitHeight);
	// Aim a little to the right of the centre so the map sits in the open part of the screen, not behind the panel.
	const FVector Right = FVector::CrossProduct(FVector::UpVector, (OrbitCenter - Eye).GetSafeNormal2D());
	const FVector Target = OrbitCenter - Right * OrbitRadius * 0.18f;
	BackdropCamera->SetActorLocationAndRotation(Eye, (Target - Eye).Rotation());
}

void AShieldbreakMenuController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	MenuWidget = SNew(SShieldbreakMainMenu).Owner(this);
	ViewportWidget = SNew(SWeakWidget).PossiblyNullContent(MenuWidget);
	GEngine->GameViewport->AddViewportWidgetContent(ViewportWidget.ToSharedRef(), 10);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
	UE_LOG(LogShieldbreak, Log, TEXT("Main menu open."));
	ShieldbreakSettings::ApplyAudio();
	ShieldbreakMusic::PlayMenuMusic(this);
	// First launch: pick graphics levels before the player sees the settings rows.
	ShieldbreakSettings::AutoDetectGraphics();

	float ShotDelay = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("SBMenuShot="), ShotDelay) && ShotDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(MenuShotTimerHandle, this, &AShieldbreakMenuController::TakeMenuShot, ShotDelay, false);
	}
}

void AShieldbreakMenuController::TakeMenuShot()
{
	const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("AutoShot") / TEXT("menu.png");
	FScreenshotRequest::RequestScreenshot(File, true, false);
	UE_LOG(LogShieldbreak, Log, TEXT("Menu screenshot: %s"), *File);
	if (FParse::Param(FCommandLine::Get(), TEXT("SBAutoQuit")))
	{
		// Give the screenshot a couple of frames to be written.
		FTimerHandle QuitHandle;
		GetWorldTimerManager().SetTimer(QuitHandle, [this]()
		{
			UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
		}, 2.f, false);
	}
}

void AShieldbreakMenuController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ViewportWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ViewportWidget.ToSharedRef());
	}
	ViewportWidget.Reset();
	MenuWidget.Reset();
	ShieldbreakMusic::Stop(0.5f);
	Super::EndPlay(EndPlayReason);
}
