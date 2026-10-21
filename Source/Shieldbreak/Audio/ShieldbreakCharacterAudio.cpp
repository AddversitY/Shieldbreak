#include "Audio/ShieldbreakCharacterAudio.h"

#include "Audio/ShieldbreakSFX.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Shield/ShieldHealthComponent.h"

UShieldbreakCharacterAudio::UShieldbreakCharacterAudio()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
}

void UShieldbreakCharacterAudio::BeginPlay()
{
	Super::BeginPlay();

	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}

	UShieldHealthComponent* ShieldComponent = GetOwner() ? GetOwner()->FindComponentByClass<UShieldHealthComponent>() : nullptr;
	if (!ShieldComponent)
	{
		return;
	}

	ObservedShield = ShieldComponent;
	LastShieldValue = ShieldComponent->GetShield();
	ShieldComponent->OnRechargeStarted.AddUniqueDynamic(this, &UShieldbreakCharacterAudio::HandleRechargeStarted);
	ShieldComponent->OnShieldChanged.AddUniqueDynamic(this, &UShieldbreakCharacterAudio::HandleShieldChanged);
	ShieldComponent->OnShieldBroken.AddUniqueDynamic(this, &UShieldbreakCharacterAudio::HandleShieldBroken);
	ShieldComponent->OnBreakPulse.AddUniqueDynamic(this, &UShieldbreakCharacterAudio::HandleBreakPulse);
	ShieldComponent->OnVentResult.AddUniqueDynamic(this, &UShieldbreakCharacterAudio::HandleVentResult);
	ShieldComponent->OnDeath.AddUniqueDynamic(this, &UShieldbreakCharacterAudio::HandleDeath);
}

void UShieldbreakCharacterAudio::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopRechargeWhine();

	if (UShieldHealthComponent* ShieldComponent = ObservedShield.Get())
	{
		ShieldComponent->OnRechargeStarted.RemoveAll(this);
		ShieldComponent->OnShieldChanged.RemoveAll(this);
		ShieldComponent->OnShieldBroken.RemoveAll(this);
		ShieldComponent->OnBreakPulse.RemoveAll(this);
		ShieldComponent->OnVentResult.RemoveAll(this);
		ShieldComponent->OnDeath.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UShieldbreakCharacterAudio::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Halo-style warning: keep beeping while your shield is gone.
	const UShieldHealthComponent* ShieldComponent = ObservedShield.Get();
	const UWorld* World = GetWorld();
	if (!ShieldComponent || !World || ShieldComponent->IsDead() || ShieldComponent->GetShield() > 0.f || !IsLocalPlayer())
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now >= NextBeepTime)
	{
		NextBeepTime = Now + LowShieldBeepInterval;
		FShieldbreakSFX::Play2D(this, EShieldbreakSound::ShieldLowBeep, 0.6f);
	}
}

bool UShieldbreakCharacterAudio::IsLocalPlayer() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled() && Pawn->IsPlayerControlled();
}

void UShieldbreakCharacterAudio::StopRechargeWhine()
{
	if (RechargeWhine)
	{
		RechargeWhine->Stop();
		RechargeWhine = nullptr;
	}
}

void UShieldbreakCharacterAudio::HandleRechargeStarted()
{
	if (IsLocalPlayer())
	{
		StopRechargeWhine();
		RechargeWhine = FShieldbreakSFX::Play2D(this, EShieldbreakSound::ShieldRecharge, 0.7f);
	}
}

void UShieldbreakCharacterAudio::HandleShieldChanged(float NewValue, float MaxValue)
{
	// Any hit interrupts the recharge; reaching full ends it.
	if (NewValue < LastShieldValue || NewValue >= MaxValue)
	{
		StopRechargeWhine();
	}
	LastShieldValue = NewValue;
}

void UShieldbreakCharacterAudio::HandleShieldBroken()
{
	if (IsLocalPlayer())
	{
		FShieldbreakSFX::Play2D(this, EShieldbreakSound::ShieldPop, 0.9f);
		// The first warning beep follows the pop rather than overlapping it.
		if (const UWorld* World = GetWorld())
		{
			NextBeepTime = World->GetTimeSeconds() + 0.35f;
		}
	}
	else if (GetOwner())
	{
		FShieldbreakSFX::PlayAt(this, EShieldbreakSound::ShieldPop, GetOwner()->GetActorLocation(), 0.8f);
	}
}

void UShieldbreakCharacterAudio::HandleBreakPulse()
{
	if (GetOwner())
	{
		FShieldbreakSFX::PlayAt(this, EShieldbreakSound::BreakPulse, GetOwner()->GetActorLocation());
	}
}

void UShieldbreakCharacterAudio::HandleVentResult(bool bSuccess)
{
	if (IsLocalPlayer())
	{
		FShieldbreakSFX::Play2D(this, bSuccess ? EShieldbreakSound::VentSuccess : EShieldbreakSound::VentFail, 0.8f);
	}
}

void UShieldbreakCharacterAudio::HandleDeath(AController* Killer)
{
	StopRechargeWhine();
	if (GetOwner())
	{
		FShieldbreakSFX::PlayAt(this, EShieldbreakSound::Death, GetOwner()->GetActorLocation(), 0.8f);
	}
}
