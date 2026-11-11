#include "Audio/ShieldbreakCharacterAudio.h"

#include "Audio/ShieldbreakSFX.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/ShieldbreakMovementComponent.h"
#include "Shield/ShieldHealthComponent.h"

UShieldbreakCharacterAudio::UShieldbreakCharacterAudio()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Every frame: footsteps are timed from distance travelled and must land on time.
	PrimaryComponentTick.TickInterval = 0.f;
}

void UShieldbreakCharacterAudio::BeginPlay()
{
	Super::BeginPlay();

	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}

	// Load the imported combat banks once, before the first shot (cheap after the first character).
	FShieldbreakSFX::Preload();

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

	TickFootsteps(DeltaTime);

	// Danger cue for the owning player: one alarm beep, repeated fast while the shield is low or gone. It stops as soon
	// as the shield starts recharging (the recharge whine takes over).
	const UShieldHealthComponent* ShieldComponent = ObservedShield.Get();
	const UWorld* World = GetWorld();
	if (!ShieldComponent || !World || ShieldComponent->IsDead() || !IsLocalPlayer())
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float Fraction = ShieldComponent->GetShield() / FMath::Max(ShieldComponent->GetMaxShield(), 1.f);
	const bool bRecharging = RechargeWhine && RechargeWhine->IsPlaying();
	if (Fraction < LowShieldFraction && !bRecharging && Now >= NextBeepTime)
	{
		NextBeepTime = Now + LowShieldBeepInterval;
		FShieldbreakSFX::Play2D(this, EShieldbreakSound::ShieldLowBeep, 0.55f);
	}
}

bool UShieldbreakCharacterAudio::IsLocalPlayer() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled() && Pawn->IsPlayerControlled();
}

void UShieldbreakCharacterAudio::PlayMovementSound(EShieldbreakSound Sound, float Volume)
{
	if (IsLocalPlayer())
	{
		FShieldbreakSFX::Play2D(this, Sound, Volume * 0.45f);
	}
	else if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		const float HalfHeight = Character->GetSimpleCollisionHalfHeight();
		FShieldbreakSFX::PlayAt(this, Sound, Character->GetActorLocation() - FVector(0.f, 0.f, HalfHeight), Volume);
	}
}

void UShieldbreakCharacterAudio::TickFootsteps(float DeltaTime)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const UShieldHealthComponent* ShieldComponent = ObservedShield.Get();
	if (!Movement || (ShieldComponent && ShieldComponent->IsDead()))
	{
		return;
	}

	if (Movement->IsFalling())
	{
		bWasFalling = true;
		FallSpeed = FMath::Max(FallSpeed, -Movement->Velocity.Z);
		StrideDistance = 0.f;
		return;
	}

	if (bWasFalling)
	{
		bWasFalling = false;
		// Landing weight scales with how fast you came down; tiny hops just get a step.
		if (FallSpeed > 350.f)
		{
			PlayMovementSound(EShieldbreakSound::Land, FMath::GetMappedRangeValueClamped(FVector2D(350.f, 1400.f), FVector2D(0.45f, 1.f), FallSpeed));
		}
		else if (FallSpeed > 100.f)
		{
			PlayMovementSound(EShieldbreakSound::Footstep, 0.6f);
		}
		FallSpeed = 0.f;
		StrideDistance = 0.f;
		return;
	}
	FallSpeed = 0.f;

	const UShieldbreakMovementComponent* ShieldbreakMovement = Cast<UShieldbreakMovementComponent>(Movement);
	const float Speed = Movement->Velocity.Size2D();
	if (!Movement->IsMovingOnGround() || Speed < 120.f || (ShieldbreakMovement && ShieldbreakMovement->IsSliding()))
	{
		// Standing still resets the cadence so the first step after moving lands quickly.
		StrideDistance = FMath::Min(StrideDistance, StrideWalk * 0.6f);
		return;
	}

	StrideDistance += Speed * DeltaTime;
	const float Stride = FMath::GetMappedRangeValueClamped(FVector2D(300.f, 1000.f), FVector2D(StrideWalk, StrideSprint), Speed);
	if (StrideDistance >= Stride)
	{
		StrideDistance -= Stride;
		const float Volume = Character->bIsCrouched ? 0.3f : FMath::GetMappedRangeValueClamped(FVector2D(300.f, 1000.f), FVector2D(0.55f, 0.9f), Speed);
		PlayMovementSound(EShieldbreakSound::Footstep, Volume);
	}
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
