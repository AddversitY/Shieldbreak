#include "Shield/ShieldHealthComponent.h"

#include "Shield/ShieldTuningData.h"
#include "Shield/ShieldbreakDamageType.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

UShieldHealthComponent::UShieldHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UShieldHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UShieldHealthComponent, Shield);
	DOREPLIFETIME(UShieldHealthComponent, ShieldBreakCount);
	DOREPLIFETIME(UShieldHealthComponent, Overshield);
	DOREPLIFETIME(UShieldHealthComponent, Health);
	DOREPLIFETIME(UShieldHealthComponent, bShaping);
	DOREPLIFETIME(UShieldHealthComponent, RechargeStartServerTime);
	DOREPLIFETIME(UShieldHealthComponent, bVentUsedThisRecharge);
	DOREPLIFETIME(UShieldHealthComponent, bDead);
}

void UShieldHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	const bool bAuthority = Owner && Owner->HasAuthority();

	// Recharge, regen and decay are simulated on the server only; clients just read replicated values.
	SetComponentTickEnabled(bAuthority);

	if (bAuthority)
	{
		SetShieldInternal(GetMaxShield());
		SetHealthInternal(GetMaxHealth());
		Owner->OnTakeAnyDamage.AddDynamic(this, &UShieldHealthComponent::HandleTakeAnyDamage);
	}
}

void UShieldHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDead)
	{
		return;
	}

	const UShieldTuningData& T = GetTuning();
	const float Now = GetServerTime();

	if (Overshield > 0.f && T.OvershieldDecayPerSecond > 0.f)
	{
		SetOvershieldInternal(Overshield - T.OvershieldDecayPerSecond * DeltaTime);
	}

	if (Shield < T.MaxShield)
	{
		if (Now >= NextRechargeServerTime)
		{
			if (RechargeStartServerTime < 0.f)
			{
				RechargeStartServerTime = Now;
				bVentUsedThisRecharge = false;
				PendingRechargePenalty = 0.f;
				OnRechargeStarted.Broadcast();
			}

			SetShieldInternal(Shield + T.MaxShield / T.RechargeDuration * DeltaTime);
			if (Shield >= T.MaxShield)
			{
				RechargeStartServerTime = -1.f;
			}
		}
	}
	else if (T.bRegenHealthWhenShieldFull && Health < T.MaxHealth)
	{
		SetHealthInternal(Health + T.HealthRegenPerSecond * DeltaTime);
	}
}

// ---- Inputs ----

void UShieldHealthComponent::SetShaping(bool bNewShaping)
{
	if (bNewShaping != bShaping)
	{
		ServerSetShaping(bNewShaping);
	}
}

void UShieldHealthComponent::TryVent()
{
	ServerTryVent(GetServerTime());
}

void UShieldHealthComponent::TryBreakPulse()
{
	ServerTryBreakPulse(GetServerTime());
}

void UShieldHealthComponent::ServerSetShaping_Implementation(bool bNewShaping)
{
	bShaping = bNewShaping && !bDead;
}

void UShieldHealthComponent::ServerTryVent_Implementation(float ClientServerTime)
{
	if (bDead || RechargeStartServerTime < 0.f || bVentUsedThisRecharge)
	{
		return;
	}

	const UShieldTuningData& T = GetTuning();
	const float PressTime = ResolveInputTime(ClientServerTime);
	const float MarkerPosition = (PressTime - RechargeStartServerTime) / T.VentSweepDuration;
	const bool bSuccess = MarkerPosition >= T.VentSweetSpotStart
		&& MarkerPosition <= T.VentSweetSpotStart + T.VentSweetSpotWidth;

	bVentUsedThisRecharge = true;

	if (bSuccess)
	{
		SetShieldInternal(T.MaxShield);
		SetOvershieldInternal(Overshield + T.VentOvershieldGain);
		RechargeStartServerTime = -1.f;
	}
	else
	{
		// Botched vent: the recharge restarts from an empty shield (no delay, but no second attempt).
		SetShieldInternal(0.f);
		RechargeStartServerTime = GetServerTime();
	}

	ClientVentResult(bSuccess);
}

void UShieldHealthComponent::ServerTryBreakPulse_Implementation(float ClientServerTime)
{
	if (bDead)
	{
		return;
	}

	// Buffer the press so a melee slightly *before* the shield pops still counts.
	LastBreakPulsePressServerTime = ResolveInputTime(ClientServerTime);

	if (ShieldBrokenServerTime >= 0.f
		&& FMath::Abs(LastBreakPulsePressServerTime - ShieldBrokenServerTime) <= GetTuning().BreakPulseWindow)
	{
		DoBreakPulse();
	}
}

void UShieldHealthComponent::ClientVentResult_Implementation(bool bSuccess)
{
	OnVentResult.Broadcast(bSuccess);
}

void UShieldHealthComponent::MulticastBreakPulse_Implementation()
{
	OnBreakPulse.Broadcast();
}

// ---- Damage ----

void UShieldHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	ApplyShieldbreakDamage(Damage, DamageType, InstigatedBy, DamageCauser);
}

float UShieldHealthComponent::ApplyShieldbreakDamage(float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bDead || Damage <= 0.f)
	{
		return 0.f;
	}

	const UShieldTuningData& T = GetTuning();
	const UShieldbreakDamageType* SBType = Cast<UShieldbreakDamageType>(DamageType);

	float ShieldMultiplier = SBType ? SBType->ShieldDamageMultiplier : 1.f;
	if (!SBType || !SBType->bIgnoresShieldShaping)
	{
		ShieldMultiplier *= GetShapingMultiplier(DamageCauser, InstigatedBy);
	}

	const float Leftover = AbsorbWithShield(Damage, ShieldMultiplier);
	const float HealthDamage = Leftover * (SBType ? SBType->HealthDamageMultiplier : 1.f);

	float ExtraDelay = SBType ? SBType->RechargeDelayExtension : 0.f;
	if (bShaping)
	{
		ExtraDelay += T.ShapingRechargeDelayBonus;
	}
	ResetRechargeDelay(ExtraDelay);

	if (HealthDamage > 0.f)
	{
		SetHealthInternal(Health - HealthDamage);
		if (Health <= 0.f)
		{
			Die(InstigatedBy);
		}
	}

	return Damage;
}

void UShieldHealthComponent::ApplyShieldOnlyDamage(float Damage, AController* InstigatedBy)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bDead || Damage <= 0.f)
	{
		return;
	}

	AbsorbWithShield(Damage, 1.f);
	ResetRechargeDelay(0.f);
}

float UShieldHealthComponent::AbsorbWithShield(float RawDamage, float ShieldMultiplier)
{
	if (ShieldMultiplier <= 0.f)
	{
		return RawDamage;
	}

	float ShieldDamage = RawDamage * ShieldMultiplier;

	const float FromOvershield = FMath::Min(Overshield, ShieldDamage);
	SetOvershieldInternal(Overshield - FromOvershield);
	ShieldDamage -= FromOvershield;

	const bool bHadShield = Shield > 0.f;
	const float FromShield = FMath::Min(Shield, ShieldDamage);
	SetShieldInternal(Shield - FromShield);
	ShieldDamage -= FromShield;

	if (bHadShield && Shield <= 0.f)
	{
		ShieldBrokenServerTime = GetServerTime();
		++ShieldBreakCount;
		OnShieldBroken.Broadcast();

		if (LastBreakPulsePressServerTime >= 0.f
			&& ShieldBrokenServerTime - LastBreakPulsePressServerTime <= GetTuning().BreakPulseWindow)
		{
			DoBreakPulse();
		}
	}

	// Convert what got through back into unmultiplied damage for the health layer.
	return ShieldDamage / ShieldMultiplier;
}

float UShieldHealthComponent::GetShapingMultiplier(const AActor* DamageCauser, const AController* InstigatedBy) const
{
	const AActor* Owner = GetOwner();
	if (!bShaping || !Owner)
	{
		return 1.f;
	}

	const AActor* Source = DamageCauser ? DamageCauser : (InstigatedBy ? InstigatedBy->GetPawn() : nullptr);
	if (!Source)
	{
		return 1.f;
	}

	const UShieldTuningData& T = GetTuning();
	const FVector ToSource = (Source->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
	if (ToSource.IsNearlyZero())
	{
		return T.ShapingFrontMultiplier;
	}

	// Use aim yaw rather than actor rotation so shaping follows where the player is looking.
	const APawn* OwnerPawn = Cast<APawn>(Owner);
	const float FacingYaw = OwnerPawn ? OwnerPawn->GetBaseAimRotation().Yaw : Owner->GetActorRotation().Yaw;
	const FVector Facing = FRotator(0.f, FacingYaw, 0.f).Vector();

	const float CosHalfArc = FMath::Cos(FMath::DegreesToRadians(T.ShapingFrontArcDegrees * 0.5f));
	return FVector::DotProduct(Facing, ToSource) >= CosHalfArc ? T.ShapingFrontMultiplier : T.ShapingRearMultiplier;
}

void UShieldHealthComponent::ResetRechargeDelay(float ExtraDelay)
{
	NextRechargeServerTime = GetServerTime() + GetTuning().RechargeDelay + ExtraDelay + PendingRechargePenalty;
	RechargeStartServerTime = -1.f;
}

void UShieldHealthComponent::DoBreakPulse()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	ShieldBrokenServerTime = -1.f;
	LastBreakPulsePressServerTime = -1.f;

	const UShieldTuningData& T = GetTuning();
	const FVector Origin = Owner->GetActorLocation();
	AController* InstigatorController = Owner->GetInstigatorController();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakBreakPulse), false, Owner);
	World->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(T.BreakPulseRadius), Params);

	TSet<AActor*> AlreadyHit;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Other = Overlap.GetActor();
		if (!Other || Other == Owner || AlreadyHit.Contains(Other))
		{
			continue;
		}
		AlreadyHit.Add(Other);

		if (UShieldHealthComponent* OtherShield = Other->FindComponentByClass<UShieldHealthComponent>())
		{
			OtherShield->ApplyShieldOnlyDamage(T.BreakPulseShieldDamage, InstigatorController);
		}

		if (ACharacter* OtherCharacter = Cast<ACharacter>(Other))
		{
			const FVector Away = (Other->GetActorLocation() - Origin).GetSafeNormal2D();
			OtherCharacter->LaunchCharacter(Away * T.BreakPulseImpulse + FVector::UpVector * T.BreakPulseUpImpulse, true, true);
		}
	}

	PendingRechargePenalty += T.BreakPulseRechargePenalty;
	NextRechargeServerTime += T.BreakPulseRechargePenalty;

	MulticastBreakPulse();
}

void UShieldHealthComponent::Die(AController* Killer)
{
	if (bDead)
	{
		return;
	}

	bDead = true;
	bShaping = false;
	RechargeStartServerTime = -1.f;
	OnDeath.Broadcast(Killer);
}

// ---- State helpers ----

void UShieldHealthComponent::SetShieldInternal(float NewShield)
{
	NewShield = FMath::Clamp(NewShield, 0.f, GetMaxShield());
	if (NewShield < KINDA_SMALL_NUMBER)
	{
		NewShield = 0.f;
	}

	if (NewShield != Shield)
	{
		Shield = NewShield;
		OnShieldChanged.Broadcast(Shield, GetMaxShield());
	}
}

void UShieldHealthComponent::SetOvershieldInternal(float NewOvershield)
{
	NewOvershield = FMath::Clamp(NewOvershield, 0.f, GetTuning().MaxOvershield);
	if (NewOvershield != Overshield)
	{
		Overshield = NewOvershield;
		OnOvershieldChanged.Broadcast(Overshield, GetTuning().MaxOvershield);
	}
}

void UShieldHealthComponent::SetHealthInternal(float NewHealth)
{
	NewHealth = FMath::Clamp(NewHealth, 0.f, GetMaxHealth());
	if (NewHealth != Health)
	{
		Health = NewHealth;
		OnHealthChanged.Broadcast(Health, GetMaxHealth());
	}
}

float UShieldHealthComponent::ResolveInputTime(float ClientServerTime) const
{
	// Trust the client's timestamp only within a small window, so lag can't be abused to rewind time.
	const float Now = GetServerTime();
	return FMath::Clamp(ClientServerTime, Now - GetTuning().MaxInputLagCompensation, Now);
}

float UShieldHealthComponent::GetVentMarkerPosition() const
{
	if (bDead || RechargeStartServerTime < 0.f || bVentUsedThisRecharge)
	{
		return -1.f;
	}

	return FMath::Clamp((GetServerTime() - RechargeStartServerTime) / GetTuning().VentSweepDuration, 0.f, 1.f);
}

float UShieldHealthComponent::GetMaxShield() const
{
	return GetTuning().MaxShield;
}

float UShieldHealthComponent::GetMaxHealth() const
{
	return GetTuning().MaxHealth;
}

const UShieldTuningData& UShieldHealthComponent::GetTuning() const
{
	return Tuning ? *Tuning : *GetDefault<UShieldTuningData>();
}

float UShieldHealthComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return static_cast<float>(GameState->GetServerWorldTimeSeconds());
	}
	return World->GetTimeSeconds();
}

// ---- Replication notifies ----

void UShieldHealthComponent::OnRep_Shield()
{
	OnShieldChanged.Broadcast(Shield, GetMaxShield());
}

void UShieldHealthComponent::OnRep_ShieldBreakCount()
{
	OnShieldBroken.Broadcast();
}

void UShieldHealthComponent::OnRep_Overshield()
{
	OnOvershieldChanged.Broadcast(Overshield, GetTuning().MaxOvershield);
}

void UShieldHealthComponent::OnRep_Health()
{
	OnHealthChanged.Broadcast(Health, GetMaxHealth());
}

void UShieldHealthComponent::OnRep_RechargeStart(float OldRechargeStart)
{
	if (OldRechargeStart < 0.f && RechargeStartServerTime >= 0.f)
	{
		OnRechargeStarted.Broadcast();
	}
}

void UShieldHealthComponent::OnRep_Dead()
{
	if (bDead)
	{
		OnDeath.Broadcast(nullptr);
	}
}
