#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "ShieldbreakGameInstance.generated.h"

class UNetDriver;
class UPendingNetGame;

/** Local connection feedback survives the engine's disconnect travel and controller replacement. */
UCLASS()
class SHIELDBREAK_API UShieldbreakGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	/** Called after the menu has validated a direct host address. */
	void BeginJoin(const FString& Address);
	const FString& GetLastJoinAddress() const { return LastJoinAddress; }
	void ClearConnectionError();
	FString ConsumeConnectionError();

private:
	void HandlePendingConnectionCreated(UPendingNetGame* PendingGame);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandleNetworkFailure(UWorld* FailedWorld, UNetDriver* FailedDriver,
		ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString);
	void StoreConnectionError(const FString& Message);

	FString LastJoinAddress;
	FString PendingConnectionError;
	TWeakObjectPtr<UNetDriver> PendingJoinDriver;
	bool bJoinPending = false;
};
