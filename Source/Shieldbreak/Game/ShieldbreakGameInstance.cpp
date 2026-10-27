#include "Game/ShieldbreakGameInstance.h"

#include "Shieldbreak.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/NetworkDelegates.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "Game/ShieldbreakGameMode.h"
#include "Player/ShieldbreakPlayerController.h"
#include "UObject/UObjectGlobals.h"

void UShieldbreakGameInstance::Init()
{
	Super::Init();
	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UShieldbreakGameInstance::HandleNetworkFailure);
		GEngine->OnTravelFailure().AddUObject(this, &UShieldbreakGameInstance::HandleTravelFailure);
	}
	FNetDelegates::OnPendingNetGameConnectionCreated.AddUObject(this, &UShieldbreakGameInstance::HandlePendingConnectionCreated);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UShieldbreakGameInstance::HandlePostLoadMap);
}

void UShieldbreakGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	FNetDelegates::OnPendingNetGameConnectionCreated.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	Super::Shutdown();
}

void UShieldbreakGameInstance::BeginJoin(const FString& Address)
{
	LastJoinAddress = Address.TrimStartAndEnd();
	ClearConnectionError();
	PendingJoinDriver.Reset();
	bJoinPending = true;
}

void UShieldbreakGameInstance::ClearConnectionError()
{
	PendingConnectionError.Reset();
}

FString UShieldbreakGameInstance::ConsumeConnectionError()
{
	FString Message = MoveTemp(PendingConnectionError);
	PendingConnectionError.Reset();
	return Message;
}

void UShieldbreakGameInstance::HandlePendingConnectionCreated(UPendingNetGame* PendingGame)
{
	if (!GEngine || !PendingGame || !PendingGame->NetDriver) { return; }
	const FWorldContext* Context = GEngine->GetWorldContextFromPendingNetGame(PendingGame);
	if (!Context || Context->OwningGameInstance != this) { return; }

	// This also covers direct startup URLs and console joins. Cache ownership before
	// the engine's failure listener can cancel the pending game (whose World may be null).
	BeginJoin(FString::Printf(TEXT("%s:%d"), *PendingGame->URL.Host, PendingGame->URL.Port));
	PendingJoinDriver = PendingGame->NetDriver;
}

void UShieldbreakGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != this) { return; }
	const AShieldbreakGameMode* Rules = LoadedWorld->GetAuthGameMode<AShieldbreakGameMode>();
	if (LoadedWorld->GetNetMode() == NM_Client || (Rules && !Rules->IsFrontEnd()))
	{
		// A successful join, host or practice launch must not replay an earlier error.
		ClearConnectionError();
		PendingJoinDriver.Reset();
		bJoinPending = false;
	}
}

void UShieldbreakGameInstance::StoreConnectionError(const FString& Message)
{
	// Never display raw server/driver text: it can contain paths or untrusted content.
	PendingConnectionError = Message;
	bJoinPending = false;
	PendingJoinDriver.Reset();
	UE_LOG(LogShieldbreak, Warning, TEXT("Connection recovery: %s"), *Message);
}

void UShieldbreakGameInstance::HandleNetworkFailure(UWorld* FailedWorld, UNetDriver* FailedDriver,
	ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (FailedDriver && FailedDriver->NetDriverName != NAME_GameNetDriver
		&& FailedDriver->NetDriverName != NAME_PendingNetDriver) { return; }
	const bool bOwnWorld = FailedWorld && FailedWorld->GetGameInstance() == this;
	if (FailedWorld && !bOwnWorld) { return; }
	const bool bOwnPendingDriver = FailedDriver && PendingJoinDriver.HasSameIndexAndSerialNumber(TWeakObjectPtr<UNetDriver>(FailedDriver));
	const UWorld* DriverWorld = FailedDriver ? FailedDriver->GetWorld() : nullptr;
	const bool bOwnClientDriver = FailedDriver && DriverWorld && DriverWorld->GetGameInstance() == this
		&& FailedDriver->GetNetMode() == NM_Client;
	const bool bOwnFailedClientWorld = bOwnWorld && FailedDriver && FailedDriver->GetNetMode() == NM_Client;
	const bool bOwnDriverCreationFailure = !FailedDriver && bJoinPending && (!FailedWorld || bOwnWorld);
	if (!bOwnPendingDriver && !bOwnClientDriver && !bOwnFailedClientWorld && !bOwnDriverCreationFailure) { return; }

	const bool bWasJoining = bJoinPending || bOwnPendingDriver;
	FString Message;
	switch (FailureType)
	{
	case ENetworkFailure::ConnectionLost:
		Message = TEXT("Connection to the host was lost. You can reconnect or join another match.");
		break;
	case ENetworkFailure::ConnectionTimeout:
		Message = bWasJoining ? TEXT("Connection timed out. Check the host address and try again.")
			: TEXT("Connection to the host timed out. You can reconnect or join another match.");
		break;
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
		Message = TEXT("The host uses a different game version. Update both games and try again.");
		break;
	default:
		Message = bWasJoining ? TEXT("Could not join the match. The host may be full or unavailable. Try again.")
			: TEXT("The connection to the match ended. You can reconnect or join another match.");
		break;
	}
	StoreConnectionError(Message);

	// Normally the engine schedules its default-map recovery. A driver-creation
	// failure has no driver to disconnect and can leave the existing front end alive.
	if (!FailedDriver)
	{
		const UWorld* CurrentWorld = GetWorld();
		const AShieldbreakGameMode* Rules = CurrentWorld ? CurrentWorld->GetAuthGameMode<AShieldbreakGameMode>() : nullptr;
		if (Rules && Rules->IsFrontEnd())
		{
			if (AShieldbreakPlayerController* LocalPC = Cast<AShieldbreakPlayerController>(GetFirstLocalPlayerController()))
			{
				LocalPC->ShowMainMenu();
			}
		}
	}
}

void UShieldbreakGameInstance::HandleTravelFailure(UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (!FailedWorld || FailedWorld->GetGameInstance() != this) { return; }
	// Listen hosts must not receive client-connection recovery feedback.
	if (!bJoinPending && FailedWorld->GetNetMode() != NM_Client) { return; }
	StoreConnectionError(TEXT("Could not load the match. Check that you and the host have the same game version, then try again."));
}
