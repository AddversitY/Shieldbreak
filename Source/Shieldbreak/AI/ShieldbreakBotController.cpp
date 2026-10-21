#include "AI/ShieldbreakBotController.h"

#include "Game/ShieldbreakTeams.h"

#include "Shieldbreak.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/ShieldbreakArena.h"
#include "Player/ShieldbreakCharacter.h"
#include "Player/ShieldbreakMovementComponent.h"
#include "Shield/ShieldHealthComponent.h"
#include "Shield/ShieldTuningData.h"
#include "Weapons/ShieldbreakWeapon.h"
#include "Weapons/ShieldbreakWeaponClasses.h"
#include "Weapons/ShieldbreakWeaponPickup.h"

namespace ShieldbreakBotPrivate
{
	constexpr float PerceiveInterval = 0.15f;
	constexpr float SightRange = 9000.f;
	constexpr float HalfFOV = 70.f;
	constexpr float MemorySeconds = 8.f;
	constexpr float NodeReachRadius = 110.f;
	constexpr float RepathInterval = 1.0f;
}

AShieldbreakBotController::AShieldbreakBotController()
{
	bWantsPlayerState = true;
	PrimaryActorTick.bCanEverTick = true;
	bSetControlRotationFromPawnOrientation = false;
}

void AShieldbreakBotController::SetSkill(int32 NewSkill)
{
	Skill = FMath::Clamp(NewSkill, 0, 3);
}

AShieldbreakBotController::FSkillProfile AShieldbreakBotController::GetProfile() const
{
	static const FSkillProfile Profiles[4] =
	{
		{ 0.60f, 9.0f, 180.f, 1.6f },
		{ 0.40f, 5.0f, 280.f, 1.1f },
		{ 0.25f, 2.5f, 420.f, 0.7f },
		{ 0.15f, 1.2f, 600.f, 0.45f },
	};
	return Profiles[FMath::Clamp(Skill, 0, 3)];
}

AShieldbreakCharacter* AShieldbreakBotController::GetBot() const
{
	return Cast<AShieldbreakCharacter>(GetPawn());
}

AShieldbreakArena* AShieldbreakBotController::GetArena()
{
	if (!CachedArena)
	{
		CachedArena = AShieldbreakArena::Find(GetWorld());
	}
	return CachedArena;
}

float AShieldbreakBotController::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

void AShieldbreakBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ResetState();

	if (InPawn)
	{
		InPawn->OnTakeAnyDamage.AddUniqueDynamic(this, &AShieldbreakBotController::HandlePawnDamaged);
		SetControlRotation(InPawn->GetActorRotation());
	}
}

void AShieldbreakBotController::OnUnPossess()
{
	if (APawn* PreviousPawn = GetPawn())
	{
		PreviousPawn->OnTakeAnyDamage.RemoveDynamic(this, &AShieldbreakBotController::HandlePawnDamaged);
	}
	Super::OnUnPossess();
	ResetState();
}

void AShieldbreakBotController::ResetState()
{
	Target.Reset();
	bTargetVisible = false;
	LastSeenTime = -100.f;
	Path.Reset();
	PathIndex = 0;
	bHasRoamGoal = false;
	StuckTimer = 0.f;
	bSliding = false;
	bFiring = false;
	VentPressAt = -1.f;
	bVentDecided = false;
	TrackingTime = 0.f;
	NextGrenadeTime = Now() + 3.f;
}

void AShieldbreakBotController::HandlePawnDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	// A teammate's stray grenade shouldn't turn bots on each other.
	if (InstigatedBy && InstigatedBy != this && !ShieldbreakTeams::AreTeammates(InstigatedBy, this))
	{
		if (AShieldbreakCharacter* Attacker = Cast<AShieldbreakCharacter>(InstigatedBy->GetPawn()))
		{
			LastAttacker = Attacker;
			LastAttackedTime = Now();
		}
	}
}

// ---- Main loop ----

void AShieldbreakBotController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	if (bUpdatePawn)
	{
		if (APawn* MyPawn = GetPawn())
		{
			MyPawn->FaceRotation(GetControlRotation(), DeltaTime);
		}
	}
}

void AShieldbreakBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AShieldbreakCharacter* Bot = GetBot();
	if (!Bot || !Bot->IsAlive())
	{
		return;
	}

	PerceiveTimer -= DeltaSeconds;
	if (PerceiveTimer <= 0.f)
	{
		PerceiveTimer = ShieldbreakBotPrivate::PerceiveInterval;
		Perceive();
	}

	UpdateMovement(DeltaSeconds);
	UpdateAim(DeltaSeconds);
	UpdateCombat(DeltaSeconds);
	UpdateMastery();
}

// ---- Perception ----

bool AShieldbreakBotController::CanSee(const AShieldbreakCharacter* Other, FVector& OutAimPoint) const
{
	const AShieldbreakCharacter* Bot = GetBot();
	const UWorld* World = GetWorld();
	if (!Bot || !Other || !World)
	{
		return false;
	}

	const FVector Eye = Bot->GetAimOrigin();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakBotSight), false, Bot);
	Params.AddIgnoredActor(Other);

	const FVector Head = Other->GetHeadMesh() ? Other->GetHeadMesh()->GetComponentLocation() : Other->GetActorLocation() + FVector(0.f, 0.f, 70.f);
	const FVector Body = Other->GetActorLocation() + FVector(0.f, 0.f, 20.f);

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Eye, Head, ECC_Visibility, Params))
	{
		OutAimPoint = Head;
		return true;
	}
	if (!World->LineTraceSingleByChannel(Hit, Eye, Body, ECC_Visibility, Params))
	{
		OutAimPoint = Body;
		return true;
	}
	return false;
}

void AShieldbreakBotController::Perceive()
{
	using namespace ShieldbreakBotPrivate;

	AShieldbreakCharacter* Bot = GetBot();
	UWorld* World = GetWorld();
	if (!Bot || !World)
	{
		return;
	}

	const FVector Eye = Bot->GetAimOrigin();
	const FVector Facing = FRotator(0.f, GetControlRotation().Yaw, 0.f).Vector();
	const float CosHalfFOV = FMath::Cos(FMath::DegreesToRadians(HalfFOV));
	const bool bRecentlyHit = Now() - LastAttackedTime < 2.f;

	AShieldbreakCharacter* Best = nullptr;
	FVector BestAim = FVector::ZeroVector;
	float BestScore = TNumericLimits<float>::Max();
	VisibleEnemyCount = 0;

	for (TActorIterator<AShieldbreakCharacter> It(World); It; ++It)
	{
		AShieldbreakCharacter* Other = *It;
		if (!IsValid(Other) || Other == Bot || !Other->IsAlive() || ShieldbreakTeams::AreTeammates(Other, Bot))
		{
			continue;
		}

		// Active Camo: a still camouflaged player is only spotted up close; a moving one at shorter range.
		if (Other->IsCamouflaged())
		{
			const float CamoRange = Other->GetVelocity().Size2D() < 60.f ? 450.f : 1800.f;
			if (FVector::DistSquared(Other->GetActorLocation(), Bot->GetActorLocation()) > FMath::Square(CamoRange))
			{
				continue;
			}
		}

		const FVector ToOther = Other->GetActorLocation() - Eye;
		const float Distance = static_cast<float>(ToOther.Size());
		if (Distance > SightRange)
		{
			continue;
		}

		const bool bIsAttacker = bRecentlyHit && LastAttacker.Get() == Other;
		const bool bInFOV = FVector::DotProduct(ToOther.GetSafeNormal2D(), Facing) >= CosHalfFOV || Distance < 400.f;
		if (!bInFOV && !bIsAttacker && Other != Target.Get())
		{
			continue;
		}

		FVector AimPoint;
		if (!CanSee(Other, AimPoint))
		{
			continue;
		}
		++VisibleEnemyCount;

		// Prefer the current target and attackers; otherwise the nearest.
		float Score = Distance;
		if (Other == Target.Get())
		{
			Score *= 0.6f;
		}
		if (bIsAttacker)
		{
			Score *= 0.5f;
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Other;
			BestAim = AimPoint;
		}
	}

	if (Best)
	{
		if (Best != Target.Get())
		{
			Target = Best;
			TargetAcquiredTime = Now();
			TrackingTime = 0.f;
		}
		bTargetVisible = true;
		TargetAimPoint = BestAim;
		LastKnownLocation = Best->GetActorLocation();
		LastSeenTime = Now();
	}
	else
	{
		bTargetVisible = false;
		if (Now() - LastSeenTime > MemorySeconds || (Target.IsValid() && !Target->IsAlive()))
		{
			Target.Reset();
		}
	}
}

// ---- Movement ----

void AShieldbreakBotController::MoveToward(const FVector& Location, float Scale)
{
	if (APawn* Bot = GetPawn())
	{
		const FVector Direction = (Location - Bot->GetActorLocation()).GetSafeNormal2D();
		Bot->AddMovementInput(Direction, Scale);
	}
}

void AShieldbreakBotController::PickRoamGoal()
{
	AShieldbreakArena* Arena = GetArena();
	AShieldbreakCharacter* Bot = GetBot();
	if (!Arena || !Bot)
	{
		return;
	}

	// Sometimes go for an available pickup we don't carry.
	if (FMath::FRand() < 0.35f)
	{
		for (TActorIterator<AShieldbreakWeaponPickup> It(GetWorld()); It; ++It)
		{
			const AShieldbreakWeaponPickup* Pickup = *It;
			if (!Pickup || !Pickup->IsAvailable())
			{
				continue;
			}
			const bool bHave = (Bot->GetCurrentWeapon() && Bot->GetCurrentWeapon()->GetClass() == Pickup->GetWeaponClass())
				|| (Bot->GetHolsteredWeapon() && Bot->GetHolsteredWeapon()->GetClass() == Pickup->GetWeaponClass());
			if (!bHave)
			{
				RoamGoal = Pickup->GetActorLocation();
				bHasRoamGoal = true;
				return;
			}
		}
	}

	const TArray<FVector>& Nodes = Arena->GetWaypoints();
	if (Nodes.Num() > 0)
	{
		RoamGoal = Nodes[FMath::RandRange(0, Nodes.Num() - 1)];
		bHasRoamGoal = true;
	}
}

void AShieldbreakBotController::FollowPathTo(const FVector& Goal, float DeltaSeconds)
{
	using namespace ShieldbreakBotPrivate;

	AShieldbreakCharacter* Bot = GetBot();
	AShieldbreakArena* Arena = GetArena();
	if (!Bot || !Arena)
	{
		return;
	}

	const FVector BotLocation = Bot->GetActorLocation();

	RepathTimer -= DeltaSeconds;
	if (Path.Num() == 0 || RepathTimer <= 0.f || FVector::DistSquared(Goal, PathGoal) > FMath::Square(400.f))
	{
		RepathTimer = RepathInterval;
		PathGoal = Goal;
		Path.Reset();
		PathIndex = 0;

		const int32 From = Arena->FindNearestWaypoint(BotLocation, true);
		const int32 To = Arena->FindNearestWaypoint(Goal, false);
		if (!Arena->FindPath(From, To, Path))
		{
			Path.Reset();
		}
		// Skip the first node if we're already past it toward the second.
		if (Path.Num() > 1)
		{
			const FVector First = Arena->GetWaypoints()[Path[0]];
			const FVector Second = Arena->GetWaypoints()[Path[1]];
			if (FVector::Dist2D(BotLocation, Second) < FVector::Dist2D(First, Second))
			{
				PathIndex = 1;
			}
		}
	}

	FVector Next = Goal;
	FVector AfterNext = Goal;
	if (Path.IsValidIndex(PathIndex))
	{
		const TArray<FVector>& Nodes = Arena->GetWaypoints();
		Next = Nodes[Path[PathIndex]];
		AfterNext = Path.IsValidIndex(PathIndex + 1) ? Nodes[Path[PathIndex + 1]] : Goal;
		if (FVector::Dist2D(BotLocation, Next) < NodeReachRadius && FMath::Abs(BotLocation.Z - Next.Z) < 160.f)
		{
			++PathIndex;
			Next = AfterNext;
		}
	}

	MoveToward(Next);

	// Sprint on long legs; Heroic+ slide down long descents for Momentum Carry.
	UShieldbreakMovementComponent* Movement = Bot->GetShieldbreakMovement();
	const float LegLength = static_cast<float>(FVector::Dist2D(BotLocation, Next));
	const bool bLongLeg = LegLength > 500.f;
	if (!bTargetVisible)
	{
		Bot->SetSprinting(bLongLeg);
	}

	if (Movement && Skill >= 2)
	{
		const bool bDownhill = Next.Z < BotLocation.Z - 120.f && LegLength > 600.f;
		if (!bSliding && bDownhill && Movement->IsSprinting() && Bot->GetVelocity().Size2D() > 650.f)
		{
			Bot->StartCrouchOrSlide();
			bSliding = true;
			SlideStartTime = Now();
		}
		else if (bSliding && (!Movement->IsSliding() && Now() - SlideStartTime > 0.3f))
		{
			// Slide over: hop out to carry the speed.
			Bot->StopCrouchOrSlide();
			if (Movement->IsMovingOnGround() && Bot->GetVelocity().Size2D() > 500.f)
			{
				Bot->Jump();
			}
			bSliding = false;
		}
	}

	// Stuck handling: jump, then give up on the goal.
	if (FVector::DistSquared2D(BotLocation, LastProgressLocation) > FMath::Square(60.f))
	{
		LastProgressLocation = BotLocation;
		StuckTimer = 0.f;
	}
	else
	{
		StuckTimer += DeltaSeconds;
		if (StuckTimer > 1.0f && Movement && Movement->IsMovingOnGround())
		{
			Bot->Jump();
		}
		if (StuckTimer > 2.5f)
		{
			StuckTimer = 0.f;
			bHasRoamGoal = false;
			Path.Reset();
		}
	}
}

void AShieldbreakBotController::UpdateMovement(float DeltaSeconds)
{
	AShieldbreakCharacter* Bot = GetBot();
	if (!Bot)
	{
		return;
	}
	Bot->StopJumping();

	AShieldbreakCharacter* Enemy = Target.Get();
	const FVector BotLocation = Bot->GetActorLocation();

	if (Enemy && bTargetVisible)
	{
		Bot->SetSprinting(false);
		if (bSliding)
		{
			Bot->StopCrouchOrSlide();
			bSliding = false;
		}

		const float Distance = static_cast<float>(FVector::Dist(BotLocation, Enemy->GetActorLocation()));
		const FVector ToEnemy = (Enemy->GetActorLocation() - BotLocation).GetSafeNormal2D();
		const FVector Side(-ToEnemy.Y, ToEnemy.X, 0.f);

		StrafeTimer -= DeltaSeconds;
		if (StrafeTimer <= 0.f)
		{
			StrafeTimer = FMath::FRandRange(0.4f, 1.4f);
			StrafeDir = FMath::RandBool() ? 1.f : -1.f;

			if (Skill >= 1 && FMath::FRand() < 0.15f + 0.05f * Skill)
			{
				Bot->Jump();
			}
			else if (Skill >= 1 && FMath::FRand() < 0.1f)
			{
				CrouchUntil = Now() + FMath::FRandRange(0.3f, 0.8f);
			}
		}

		if (Now() < CrouchUntil)
		{
			Bot->StartCrouchOrSlide();
		}
		else if (Bot->bIsCrouched && !bSliding)
		{
			Bot->StopCrouchOrSlide();
		}

		// Keep to a comfortable range for the weapon in hand.
		const AShieldbreakWeapon* Weapon = Bot->GetCurrentWeapon();
		const float Preferred = (Weapon && Weapon->GetStats().Range < 5000.f) ? 500.f : 1400.f;
		float Approach = 0.f;
		if (Distance > Preferred * 1.4f)
		{
			Approach = 1.f;
		}
		else if (Distance < Preferred * 0.5f && Distance > 250.f)
		{
			Approach = -0.6f;
		}
		else if (Distance <= 250.f)
		{
			Approach = 1.f; // close the gap for a melee
		}

		const FVector Desired = (Side * StrafeDir + ToEnemy * Approach).GetSafeNormal2D();
		Bot->AddMovementInput(Desired, 1.f);
		return;
	}

	if (Bot->bIsCrouched && !bSliding)
	{
		Bot->StopCrouchOrSlide();
	}

	if (Enemy && Now() - LastSeenTime < ShieldbreakBotPrivate::MemorySeconds)
	{
		FollowPathTo(LastKnownLocation, DeltaSeconds);
		if (FVector::Dist2D(BotLocation, LastKnownLocation) < 200.f)
		{
			// Got there and nothing: forget it.
			Target.Reset();
		}
		return;
	}

	if (!bHasRoamGoal || FVector::Dist2D(BotLocation, RoamGoal) < 150.f)
	{
		PickRoamGoal();
	}
	if (bHasRoamGoal)
	{
		FollowPathTo(RoamGoal, DeltaSeconds);
	}
}

// ---- Aim ----

void AShieldbreakBotController::UpdateAim(float DeltaSeconds)
{
	AShieldbreakCharacter* Bot = GetBot();
	if (!Bot)
	{
		return;
	}

	const FSkillProfile Profile = GetProfile();
	FRotator Desired = GetControlRotation();

	AShieldbreakCharacter* Enemy = Target.Get();
	if (Enemy && bTargetVisible)
	{
		TrackingTime += DeltaSeconds;

		// Track the head while it's visible (it moves between perception ticks).
		FVector AimPoint = TargetAimPoint;
		if (Enemy->GetHeadMesh() && FVector::DistSquared(TargetAimPoint, Enemy->GetHeadMesh()->GetComponentLocation()) < FMath::Square(300.f))
		{
			const bool bAimingHead = FVector::DistSquared(TargetAimPoint, Enemy->GetActorLocation()) > FMath::Square(45.f);
			AimPoint = bAimingHead ? Enemy->GetHeadMesh()->GetComponentLocation() : Enemy->GetActorLocation() + FVector(0.f, 0.f, 20.f);
		}

		// Error shrinks as the bot settles onto the target.
		AimNoiseTimer -= DeltaSeconds;
		if (AimNoiseTimer <= 0.f)
		{
			AimNoiseTimer = FMath::FRandRange(0.35f, 0.8f);
			const float Magnitude = Profile.AimError * FMath::Max(0.3f, FMath::Exp(-TrackingTime / 1.5f));
			AimNoise = FRotator(FMath::FRandRange(-Magnitude, Magnitude) * 0.6f, FMath::FRandRange(-Magnitude, Magnitude), 0.f);
		}

		Desired = (AimPoint - Bot->GetAimOrigin()).Rotation() + AimNoise;
	}
	else
	{
		AimNoise = FRotator::ZeroRotator;
		TrackingTime = 0.f;

		const FVector Velocity = Bot->GetVelocity();
		if (Velocity.SizeSquared2D() > FMath::Square(100.f))
		{
			Desired = FRotator(0.f, Velocity.Rotation().Yaw, 0.f);
		}
		else if (Enemy)
		{
			Desired = (LastKnownLocation - Bot->GetAimOrigin()).Rotation();
		}
	}

	const FRotator Current = GetControlRotation();
	FRotator NewRotation = FMath::RInterpConstantTo(Current, Desired, DeltaSeconds, Profile.TurnSpeed);
	NewRotation.Pitch = FMath::ClampAngle(NewRotation.Pitch, -80.f, 80.f);
	NewRotation.Roll = 0.f;
	SetControlRotation(NewRotation);
}

// ---- Combat ----

float AShieldbreakBotController::ScoreWeapon(const AShieldbreakWeapon* Weapon, float TargetDistance) const
{
	if (!Weapon)
	{
		return -1000.f;
	}
	const FShieldbreakWeaponStats& Stats = Weapon->GetStats();
	float Score = 0.f;

	if (!Weapon->HasAmmo() && Weapon->GetReserveAmmo() <= 0 && !Stats.bUsesHeat)
	{
		return -500.f;
	}
	if (TargetDistance > Stats.Range * 0.85f)
	{
		Score -= 200.f;
	}

	if (Stats.FireMode == EShieldbreakFireMode::Charge)
	{
		// Rail Lance: one-shot kill at any range a bot can track.
		Score += TargetDistance > 400.f ? 85.f : 30.f;
	}
	else if (Stats.ProjectileClass)
	{
		// Rockets: devastating at mid range, suicidal up close.
		Score += TargetDistance < 350.f ? -100.f : (TargetDistance < 2500.f ? 90.f : 20.f);
	}
	else if (Stats.bHeadshotIgnoresShields)
	{
		// Sniper: the long-range weapon of choice, still serviceable no-scope.
		Score += TargetDistance > 1200.f ? 95.f : 25.f;
	}
	else if (Stats.bPrecision)
	{
		Score += TargetDistance > 900.f ? 60.f : 10.f;
	}
	else if (Stats.bUsesHeat)
	{
		Score += TargetDistance < 1800.f ? 45.f : 0.f;
		// Noob combo (Heroic+): plasma to strip, precision to finish.
		const AShieldbreakCharacter* Enemy = Target.Get();
		const UShieldHealthComponent* EnemyShield = Enemy ? Enemy->GetShieldHealth() : nullptr;
		if (Skill >= 2 && EnemyShield && EnemyShield->GetShield() <= 0.f)
		{
			Score -= 60.f;
		}
		if (Weapon->IsOverheated())
		{
			Score -= 100.f;
		}
	}
	else
	{
		Score += TargetDistance < 900.f ? 55.f : 0.f;
	}
	return Score;
}

void AShieldbreakBotController::ChooseWeapon(float TargetDistance)
{
	AShieldbreakCharacter* Bot = GetBot();
	if (!Bot || Now() < NextWeaponSwapTime)
	{
		return;
	}

	const AShieldbreakWeapon* Holstered = Bot->GetHolsteredWeapon();
	if (Holstered && ScoreWeapon(Holstered, TargetDistance) > ScoreWeapon(Bot->GetCurrentWeapon(), TargetDistance) + 15.f)
	{
		Bot->SwapWeapon();
		NextWeaponSwapTime = Now() + 1.5f;
	}
}

void AShieldbreakBotController::UpdateCombat(float DeltaSeconds)
{
	AShieldbreakCharacter* Bot = GetBot();
	if (!Bot)
	{
		return;
	}

	AShieldbreakCharacter* Enemy = Target.Get();
	AShieldbreakWeapon* Weapon = Bot->GetCurrentWeapon();
	const FSkillProfile Profile = GetProfile();

	bool bWantFire = false;
	if (Enemy && bTargetVisible && Weapon)
	{
		const float Distance = static_cast<float>(FVector::Dist(Bot->GetActorLocation(), Enemy->GetActorLocation()));
		ChooseWeapon(Distance);
		Weapon = Bot->GetCurrentWeapon();

		// Melee up close.
		if (Distance < 220.f)
		{
			Bot->Melee();
		}

		const bool bReacted = Now() - TargetAcquiredTime >= Profile.Reaction;
		const FVector ToAim = (TargetAimPoint - Bot->GetAimOrigin()).GetSafeNormal();
		const float AngleOff = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToAim, GetControlRotation().Vector()), -1.f, 1.f)));
		const float Allowed = (Weapon ? Weapon->GetCurrentSpreadDegrees() : 1.f) + Profile.FireMargin + 1200.f / FMath::Max(Distance, 300.f);
		bWantFire = bReacted && AngleOff <= Allowed && Weapon && Distance <= Weapon->GetStats().Range;
	}
	else if (!Enemy && Weapon && !Weapon->IsReloading() && !Weapon->GetStats().bUsesHeat
		&& Weapon->GetAmmoInMagazine() < Weapon->GetStats().MagazineSize / 2 && Weapon->CanReload())
	{
		// Top off between fights.
		Bot->ReloadOrVent();
	}

	// Charge weapons: hold until fully charged, release only on target; drop the charge if the target is gone.
	if (Weapon && Weapon->GetStats().FireMode == EShieldbreakFireMode::Charge)
	{
		if (!Weapon->IsCharging())
		{
			if (bWantFire || (Enemy && bTargetVisible))
			{
				Bot->StartFire();
			}
		}
		else if (Weapon->GetChargeFraction() >= 1.f && bWantFire)
		{
			Bot->StopFire();
		}
		else if (!Enemy)
		{
			Weapon->CancelCharge();
		}
		bFiring = false;
	}
	else if (bWantFire && !bFiring)
	{
		Bot->StartFire();
		bFiring = true;
	}
	else if (!bWantFire && bFiring)
	{
		Bot->StopFire();
		bFiring = false;
	}
	else if (bWantFire && Weapon && !Weapon->IsFiring())
	{
		// Semi-auto / burst re-press, or weapon swapped mid-fight.
		Bot->StartFire();
	}

	// Grenade at a last-known position behind cover.
	if (Enemy && !bTargetVisible && Bot->GetGrenadeCount() > 0 && Now() >= NextGrenadeTime && Now() - LastSeenTime < 3.f)
	{
		const float Distance = static_cast<float>(FVector::Dist(Bot->GetActorLocation(), LastKnownLocation));
		if (Distance > 500.f && Distance < 1800.f && FMath::FRand() < 0.5f)
		{
			const FRotator Toss = (LastKnownLocation - Bot->GetAimOrigin()).Rotation() + FRotator(10.f + Distance / 200.f, 0.f, 0.f);
			SetControlRotation(Toss);
			Bot->ThrowGrenade();
		}
		NextGrenadeTime = Now() + FMath::FRandRange(5.f, 9.f);
	}
	else if (Enemy && bTargetVisible && Bot->GetGrenadeCount() > 0 && Now() >= NextGrenadeTime && Skill >= 1)
	{
		const float Distance = static_cast<float>(FVector::Dist(Bot->GetActorLocation(), Enemy->GetActorLocation()));
		if (Distance > 600.f && Distance < 1500.f && FMath::FRand() < 0.25f)
		{
			Bot->ThrowGrenade();
		}
		NextGrenadeTime = Now() + FMath::FRandRange(4.f, 8.f);
	}
}

// ---- Mastery mechanics ----

void AShieldbreakBotController::UpdateMastery()
{
	AShieldbreakCharacter* Bot = GetBot();
	UShieldHealthComponent* Shield = Bot ? Bot->GetShieldHealth() : nullptr;
	if (!Bot || !Shield)
	{
		return;
	}

	const UShieldTuningData* Tuning = Shield->GetTuningData();
	AShieldbreakCharacter* Enemy = Target.Get();

	// Overcharge Vent: aim for the sweet spot with skill-scaled timing error (never on Easy).
	const float Marker = Shield->GetVentMarkerPosition();
	if (Marker < 0.f)
	{
		bVentDecided = false;
		VentPressAt = -1.f;
	}
	else if (!bVentDecided && Tuning)
	{
		bVentDecided = true;
		static const float TryChance[4] = { 0.f, 0.3f, 0.75f, 0.95f };
		static const float TimingError[4] = { 0.f, 0.12f, 0.04f, 0.02f };
		if (FMath::FRand() < TryChance[FMath::Clamp(Skill, 0, 3)])
		{
			const float Center = Tuning->VentSweetSpotStart + Tuning->VentSweetSpotWidth * 0.5f;
			VentPressAt = Center + FMath::FRandRange(-1.f, 1.f) * TimingError[FMath::Clamp(Skill, 0, 3)];
		}
	}
	if (VentPressAt >= 0.f && Marker >= VentPressAt)
	{
		Shield->TryVent();
		VentPressAt = -1.f;
	}

	// Break Pulse (Legendary): melee the instant our shield pops with an enemy close.
	const float BreakTime = Shield->GetLastShieldBreakLocalTime();
	if (Skill >= 3 && BreakTime >= 0.f && BreakTime != LastSeenShieldBreak)
	{
		LastSeenShieldBreak = BreakTime;
		if (Enemy && FVector::Dist(Enemy->GetActorLocation(), Bot->GetActorLocation()) < 350.f)
		{
			Bot->Melee();
		}
	}
}
