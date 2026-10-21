#include "Player/ShieldbreakPlayerController.h"

#include "Shieldbreak.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/ShieldbreakGameMode.h"
#include "Player/ShieldbreakInputSet.h"

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
	APawn* OldPawn = GetPawn();
	Super::OnUnPossess();

	// Keep watching the body until the respawn.
	if (IsValid(OldPawn))
	{
		SetViewTarget(OldPawn);
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
