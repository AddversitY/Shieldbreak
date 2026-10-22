// Shield and life sounds for one character, driven by its UShieldHealthComponent events. The recharge whine,
// vent results and the shields-down beep are heard only by the player who owns the character.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShieldbreakCharacterAudio.generated.h"

class AController;
class UAudioComponent;
class UShieldHealthComponent;

UCLASS(ClassGroup = (Shieldbreak), meta = (BlueprintSpawnableComponent))
class SHIELDBREAK_API UShieldbreakCharacterAudio : public UActorComponent
{
	GENERATED_BODY()

public:
	UShieldbreakCharacterAudio();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Seconds between beeps while the local player's shield is empty. */
	UPROPERTY(EditAnywhere, Category = "Audio")
	float LowShieldBeepInterval = 0.55f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool IsLocalPlayer() const;
	void StopRechargeWhine();

	UFUNCTION() void HandleRechargeStarted();
	UFUNCTION() void HandleShieldChanged(float NewValue, float MaxValue);
	UFUNCTION() void HandleShieldBroken();
	UFUNCTION() void HandleBreakPulse();
	UFUNCTION() void HandleVentResult(bool bSuccess);
	UFUNCTION() void HandleDeath(AController* Killer);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RechargeWhine;

	TWeakObjectPtr<UShieldHealthComponent> ObservedShield;
	float LastShieldValue = -1.f;
	float NextBeepTime = 0.f;
};
