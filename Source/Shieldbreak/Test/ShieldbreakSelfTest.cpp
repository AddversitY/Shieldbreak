#include "Test/ShieldbreakSelfTest.h"

#include "Shieldbreak.h"
#include "AIController.h"
#include "AI/ShieldbreakBotController.h"
#include "Camera/CameraActor.h"
#include "Game/ShieldbreakTeams.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/ShieldbreakGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/ShieldbreakCharacter.h"
#include "Player/ShieldbreakMovementComponent.h"
#include "Player/ShieldbreakPlayerState.h"
#include "Shield/ShieldHealthComponent.h"
#include "Shield/ShieldTuningData.h"
#include "Weapons/ShieldbreakDamageTypes.h"
#include "Weapons/ShieldbreakGrenade.h"
#include "Weapons/ShieldbreakWeapon.h"
#include "Weapons/ShieldbreakWeaponClasses.h"
#include "Weapons/ShieldbreakWeaponPickup.h"
#include "Weapons/ShieldbreakPowerup.h"
#include "Weapons/ShieldbreakRocket.h"

namespace ShieldbreakSelfTestPrivate
{
	// Open floor on the +Y side of the arena (clear of ramps, crates and walls).
	const FVector RangeA(1200.f, 1450.f, 100.f);
	const FVector RangeB(1600.f, 1450.f, 100.f);
	// Long clear runway on the -Y side, heading +X.
	const FVector RunwayStart(-2400.f, -1450.f, 100.f);

	FString Num(float Value)
	{
		return FString::Printf(TEXT("%.2f"), Value);
	}
}

AShieldbreakSelfTest::AShieldbreakSelfTest()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
}

bool AShieldbreakSelfTest::IsRequested()
{
	return FParse::Param(FCommandLine::Get(), TEXT("SBSelfTest"));
}

float AShieldbreakSelfTest::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

void AShieldbreakSelfTest::BeginPlay()
{
	Super::BeginPlay();
	BuildSteps();
	Lines.Add(FString::Printf(TEXT("Shieldbreak self-test, %s"), *FDateTime::Now().ToString()));
	UE_LOG(LogShieldbreak, Log, TEXT("SelfTest: %d steps queued."), Steps.Num());
}

void AShieldbreakSelfTest::Check(const FString& What, bool bPassed, const FString& Detail)
{
	const FString Line = FString::Printf(TEXT("%s  %s%s%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *What,
		Detail.IsEmpty() ? TEXT("") : TEXT("  -- "), *Detail);
	Lines.Add(Line);
	(bPassed ? Passed : Failed)++;
	UE_LOG(LogShieldbreak, Log, TEXT("SelfTest: %s"), *Line);
}

AShieldbreakCharacter* AShieldbreakSelfTest::SpawnDummy(const FVector& Location, float Yaw, bool bWithController)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AShieldbreakCharacter* Dummy = World->SpawnActor<AShieldbreakCharacter>(AShieldbreakCharacter::StaticClass(), Location, FRotator(0.f, Yaw, 0.f), Params);
	if (!Dummy)
	{
		return nullptr;
	}
	Spawned.Add(Dummy);

	// Uncontrolled characters normally skip movement entirely; the tests need knockback to move them.
	if (UCharacterMovementComponent* DummyMovement = Dummy->GetCharacterMovement())
	{
		DummyMovement->bRunPhysicsWithNoController = true;
		// Normally set by ACharacter::Restart on possession; without it the mode stays MOVE_None.
		DummyMovement->SetDefaultMovementMode();
	}

	if (bWithController)
	{
		Dummy->SpawnDefaultController();
		if (AController* DummyController = Dummy->GetController())
		{
			DummyController->SetControlRotation(FRotator(0.f, Yaw, 0.f));
			Spawned.Add(DummyController);
		}
	}
	return Dummy;
}

void AShieldbreakSelfTest::ClearDummies()
{
	for (AActor* Actor : Spawned)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	Spawned.Reset();
	A = nullptr;
	B = nullptr;
	Mark = -1.f;
	Value0 = Value1 = 0.f;
	Phase = 0;
}

void AShieldbreakSelfTest::Damage(AShieldbreakCharacter* Target, float Amount, TSubclassOf<UDamageType> Type, AActor* Causer)
{
	if (Target)
	{
		UGameplayStatics::ApplyDamage(Target, Amount, nullptr, Causer ? Causer : this, Type);
	}
}

void AShieldbreakSelfTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFinished)
	{
		return;
	}

	if (StepIndex >= 0 && Steps.IsValidIndex(StepIndex))
	{
		FStep& Step = Steps[StepIndex];
		const float Elapsed = Now() - StepStartTime;
		bool bDone = false;
		if (Step.Update)
		{
			bDone = Step.Update(Elapsed);
		}
		else
		{
			bDone = true;
		}
		if (!bDone && Elapsed > Step.Timeout)
		{
			Check(Step.Name + TEXT(" finished in time"), false, FString::Printf(TEXT("timed out after %.1f s (phase %d)"), Step.Timeout, Phase));
			bDone = true;
		}
		if (!bDone)
		{
			return;
		}
	}

	++StepIndex;
	if (!Steps.IsValidIndex(StepIndex))
	{
		Finish();
		return;
	}

	Lines.Add(FString::Printf(TEXT("-- %s"), *Steps[StepIndex].Name));
	UE_LOG(LogShieldbreak, Log, TEXT("SelfTest: step %s"), *Steps[StepIndex].Name);
	StepStartTime = Now();
	if (Steps[StepIndex].Begin)
	{
		Steps[StepIndex].Begin();
	}
}

void AShieldbreakSelfTest::Finish()
{
	bFinished = true;
	ClearDummies();

	Lines.Add(FString::Printf(TEXT("RESULT %d passed, %d failed"), Passed, Failed));
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("SelfTest.txt");
	FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")) + TEXT("\n"), *Path);
	UE_LOG(LogShieldbreak, Log, TEXT("SelfTest: %d passed, %d failed. Report: %s"), Passed, Failed, *Path);

	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}

void AShieldbreakSelfTest::BuildSteps()
{
	using namespace ShieldbreakSelfTestPrivate;

	const UShieldTuningData* T = GetDefault<UShieldTuningData>();
	const float MaxShield = T->MaxShield;
	const float MaxHealth = T->MaxHealth;

	auto ShieldOf = [](const AShieldbreakCharacter* C) { return C && C->GetShieldHealth() ? C->GetShieldHealth()->GetShield() : -1.f; };
	auto HealthOf = [](const AShieldbreakCharacter* C) { return C && C->GetShieldHealth() ? C->GetShieldHealth()->GetHealth() : -1.f; };

	// ---- Settle: let the arena and the first characters finish BeginPlay. ----
	Steps.Add({ TEXT("Settle"), nullptr, [](float Elapsed) { return Elapsed > 1.5f; }, 5.f });

	// ---- Core shield layers ----
	Steps.Add({ TEXT("Shield absorbs damage"),
		[this, MaxShield, MaxHealth, ShieldOf, HealthOf]()
		{
			A = SpawnDummy(RangeA, 0.f, false);
			B = SpawnDummy(RangeB, 180.f, false);
		},
		[this, MaxShield, MaxHealth, ShieldOf, HealthOf](float Elapsed)
		{
			if (Elapsed < 0.5f) return false;
			Check(TEXT("Spawned at full shield/health"), ShieldOf(A) == MaxShield && HealthOf(A) == MaxHealth,
				FString::Printf(TEXT("shield %s health %s"), *Num(ShieldOf(A)), *Num(HealthOf(A))));
			Damage(A, 30.f, UDamageType_Ballistic::StaticClass());
			Check(TEXT("30 ballistic damage comes off the shield only"), FMath::IsNearlyEqual(ShieldOf(A), MaxShield - 30.f, 0.01f) && HealthOf(A) == MaxHealth,
				FString::Printf(TEXT("shield %s health %s"), *Num(ShieldOf(A)), *Num(HealthOf(A))));
			Damage(A, 60.f, UDamageType_Ballistic::StaticClass());
			const float ExpectedHealth = MaxHealth - (60.f - (MaxShield - 30.f));
			Check(TEXT("Overflow past a broken shield carries into health"), ShieldOf(A) == 0.f && FMath::IsNearlyEqual(HealthOf(A), ExpectedHealth, 0.01f),
				FString::Printf(TEXT("shield %s health %s (expected %s)"), *Num(ShieldOf(A)), *Num(HealthOf(A)), *Num(ExpectedHealth)));
			Mark = Now();
			return true;
		}, 5.f });

	Steps.Add({ TEXT("Recharge delay and refill"), nullptr,
		[this, T, MaxShield, MaxHealth, ShieldOf, HealthOf](float)
		{
			const float SinceHit = Now() - Mark;
			if (Phase == 0 && SinceHit >= T->RechargeDelay - 0.4f)
			{
				Check(TEXT("No recharge before the delay"), ShieldOf(A) == 0.f, FString::Printf(TEXT("shield %s at %ss"), *Num(ShieldOf(A)), *Num(SinceHit)));
				Phase = 1;
			}
			if (Phase == 1 && SinceHit >= T->RechargeDelay + T->RechargeDuration + 0.4f)
			{
				Check(TEXT("Shield refills after delay + duration"), ShieldOf(A) >= MaxShield - 0.01f, FString::Printf(TEXT("shield %s at %ss"), *Num(ShieldOf(A)), *Num(SinceHit)));
				Phase = 2;
			}
			if (Phase == 2 && SinceHit >= T->RechargeDelay + T->RechargeDuration + 0.4f + MaxHealth / FMath::Max(1.f, T->HealthRegenPerSecond) + 0.3f)
			{
				Check(TEXT("Health regenerates once the shield is full"), HealthOf(A) >= MaxHealth - 0.01f, FString::Printf(TEXT("health %s"), *Num(HealthOf(A))));
				return true;
			}
			return false;
		}, 14.f });

	// ---- Shield Bleed ----
	Steps.Add({ TEXT("Shield Bleed (plasma)"),
		[this, MaxShield, ShieldOf]()
		{
			Phase = 0;
			Damage(A, 10.f, UDamageType_Plasma::StaticClass());
			Value0 = ShieldOf(A);
			Mark = Now();
			const float Expected = MaxShield - 10.f * GetDefault<UDamageType_Plasma>()->ShieldDamageMultiplier;
			Check(TEXT("Plasma does bonus shield damage"), FMath::IsNearlyEqual(Value0, Expected, 0.01f),
				FString::Printf(TEXT("shield %s (expected %s)"), *Num(Value0), *Num(Expected)));
		},
		[this, T, ShieldOf](float)
		{
			const float Extension = GetDefault<UDamageType_Plasma>()->RechargeDelayExtension;
			const float SinceHit = Now() - Mark;
			if (Phase == 0 && SinceHit >= T->RechargeDelay + Extension * 0.5f)
			{
				Check(TEXT("Plasma extends the recharge delay"), !A->GetShieldHealth()->IsRecharging() && ShieldOf(A) == Value0,
					FString::Printf(TEXT("recharging=%d shield %s at %ss"), A->GetShieldHealth()->IsRecharging(), *Num(ShieldOf(A)), *Num(SinceHit)));
				Phase = 1;
			}
			if (Phase == 1 && SinceHit >= T->RechargeDelay + Extension + 0.5f)
			{
				Check(TEXT("...and recharge still starts afterwards"), ShieldOf(A) > Value0, FString::Printf(TEXT("shield %s"), *Num(ShieldOf(A))));
				return true;
			}
			return false;
		}, 10.f });

	// ---- Overcharge Vent ----
	Steps.Add({ TEXT("Overcharge Vent (hit)"),
		[this, MaxShield]()
		{
			ClearDummies();
			A = SpawnDummy(RangeA, 0.f, false);
			B = SpawnDummy(RangeB, 180.f, false);
		},
		[this, T, MaxShield, ShieldOf](float Elapsed)
		{
			UShieldHealthComponent* Shield = A->GetShieldHealth();
			if (Phase == 0 && Elapsed > 0.5f)
			{
				Damage(A, MaxShield, UDamageType_Ballistic::StaticClass());
				Phase = 1;
			}
			else if (Phase == 1 && Shield->IsRecharging())
			{
				Mark = Now();
				Phase = 2;
			}
			else if (Phase == 2 && Now() >= Mark + T->VentSweepDuration * (T->VentSweetSpotStart + T->VentSweetSpotWidth * 0.5f))
			{
				Value0 = Shield->GetVentMarkerPosition();
				Shield->TryVent();
				Phase = 3;
			}
			else if (Phase == 3)
			{
				Check(TEXT("Vent in the sweet spot refills the shield"), ShieldOf(A) >= MaxShield - 0.01f,
					FString::Printf(TEXT("shield %s, marker was %s"), *Num(ShieldOf(A)), *Num(Value0)));
				Check(TEXT("...and grants overshield"), Shield->GetOvershield() > T->VentOvershieldGain * 0.7f,
					FString::Printf(TEXT("overshield %s"), *Num(Shield->GetOvershield())));
				return true;
			}
			return false;
		}, 12.f });

	Steps.Add({ TEXT("Overcharge Vent (miss)"), nullptr,
		[this, MaxShield, ShieldOf](float)
		{
			UShieldHealthComponent* Shield = B->GetShieldHealth();
			if (Phase == 0)
			{
				Damage(B, MaxShield, UDamageType_Ballistic::StaticClass());
				Phase = 1;
			}
			else if (Phase == 1 && Shield->IsRecharging())
			{
				Mark = Now();
				Phase = 2;
			}
			else if (Phase == 2 && Now() >= Mark + 0.25f)
			{
				Value0 = ShieldOf(B);
				Shield->TryVent();
				// Checked in the same frame: the vent is resolved synchronously on the server.
				Value1 = ShieldOf(B);
				Phase = 3;
			}
			else if (Phase == 3)
			{
				Check(TEXT("Missed vent restarts the recharge from empty"), Value1 < 0.01f && Value0 > 0.01f,
					FString::Printf(TEXT("shield %s before, %s right after"), *Num(Value0), *Num(Value1)));
				Check(TEXT("...and only one attempt per recharge"), Shield->GetVentMarkerPosition() < 0.f,
					FString::Printf(TEXT("marker %s"), *Num(Shield->GetVentMarkerPosition())));
				return true;
			}
			return false;
		}, 12.f });

	// ---- Break Pulse ----
	Steps.Add({ TEXT("Break Pulse"),
		[this]()
		{
			ClearDummies();
			A = SpawnDummy(RangeA, 0.f, false);
			B = SpawnDummy(RangeA + FVector(250.f, 0.f, 0.f), 180.f, false);
		},
		[this, T, MaxShield, ShieldOf](float Elapsed)
		{
			if (Phase == 0 && Elapsed > 0.6f)
			{
				Value1 = B->GetActorLocation().X;
				Damage(A, MaxShield, UDamageType_Ballistic::StaticClass());
				A->GetShieldHealth()->TryBreakPulse();
				Mark = Now();
				Phase = 1;
			}
			else if (Phase == 1 && Now() - Mark > 0.3f)
			{
				Check(TEXT("Pulse strips nearby enemy shields"), FMath::IsNearlyEqual(ShieldOf(B), MaxShield - T->BreakPulseShieldDamage, 0.05f),
					FString::Printf(TEXT("enemy shield %s"), *Num(ShieldOf(B))));
				Check(TEXT("Pulse knocks the enemy back"), B->GetActorLocation().X - Value1 > 40.f,
					FString::Printf(TEXT("moved %s uu"), *Num(B->GetActorLocation().X - Value1)));
				return true;
			}
			return false;
		}, 6.f });

	Steps.Add({ TEXT("Break Pulse outside the window"),
		[this]()
		{
			ClearDummies();
			A = SpawnDummy(RangeA, 0.f, false);
			B = SpawnDummy(RangeA + FVector(250.f, 0.f, 0.f), 180.f, false);
		},
		[this, MaxShield, ShieldOf](float Elapsed)
		{
			if (Phase == 0 && Elapsed > 0.6f)
			{
				Damage(A, MaxShield, UDamageType_Ballistic::StaticClass());
				Mark = Now();
				Phase = 1;
			}
			else if (Phase == 1 && Now() - Mark > 0.6f)
			{
				A->GetShieldHealth()->TryBreakPulse();
				Mark = Now();
				Phase = 2;
			}
			else if (Phase == 2 && Now() - Mark > 0.3f)
			{
				Check(TEXT("A late melee does not pulse"), ShieldOf(B) >= MaxShield - 0.01f, FString::Printf(TEXT("enemy shield %s"), *Num(ShieldOf(B))));
				return true;
			}
			return false;
		}, 6.f });

	// ---- Hitscan and headshots (Battle Rifle) ----
	Steps.Add({ TEXT("Battle Rifle hitscan and headshot"),
		[this]()
		{
			ClearDummies();
			A = SpawnDummy(RangeA, 0.f, true);
			B = SpawnDummy(RangeA + FVector(700.f, 0.f, 0.f), 180.f, false);
		},
		[this, ShieldOf, HealthOf](float Elapsed)
		{
			AAIController* AI = Cast<AAIController>(A->GetController());
			if (!AI)
			{
				Check(TEXT("Test shooter has an AI controller"), false);
				return true;
			}
			// AAIController only pitches toward a focus *pawn* (and then at its center), so aim by hand.
			const FVector Head = B->GetHeadMesh() ? B->GetHeadMesh()->GetComponentLocation() : B->GetActorLocation() + FVector(0.f, 0.f, 72.f);
			AI->SetActorTickEnabled(false); // see AimAt below: the AI tick would zero our pitch
			AI->ClearFocus(EAIFocusPriority::Gameplay);
			AI->bSetControlRotationFromPawnOrientation = false;
			AI->SetControlRotation((Head - A->GetAimOrigin()).Rotation());

			if (Phase == 0 && Elapsed > 1.0f)
			{
				Check(TEXT("Shooter holds the Battle Rifle"), A->GetCurrentWeapon() != nullptr,
					A->GetCurrentWeapon() ? A->GetCurrentWeapon()->GetName() : FString(TEXT("none")));
				Value0 = ShieldOf(B) + HealthOf(B);
				A->StartFire();
				Mark = Now();
				Phase = 1;
			}
			else if (Phase == 1 && Now() - Mark > 0.25f)
			{
				A->StopFire();
				Phase = 2;
			}
			else if (Phase == 2 && Now() - Mark > 0.8f)
			{
				const float Lost = Value0 - (ShieldOf(B) + HealthOf(B));
				Check(TEXT("A burst into a shielded head does normal damage"), Lost >= 12.f && Lost <= 20.f && B->IsAlive(),
					FString::Printf(TEXT("lost %s, alive %d"), *Num(Lost), B->IsAlive()));
				Damage(B, ShieldOf(B), UDamageType_Ballistic::StaticClass());
				Value0 = HealthOf(B);
				A->StartFire();
				Mark = Now();
				Phase = 3;
			}
			else if (Phase == 3 && Now() - Mark > 0.25f)
			{
				A->StopFire();
				Phase = 4;
			}
			else if (Phase == 4 && Now() - Mark > 0.8f)
			{
				Check(TEXT("An unshielded headshot kills"), !B->IsAlive(), FString::Printf(TEXT("health %s -> %s"), *Num(Value0), *Num(HealthOf(B))));
				return true;
			}
			return false;
		}, 8.f });

	// ---- Movement: sprint, slide, Momentum Carry ----
	Steps.Add({ TEXT("Sprint, slide and Momentum Carry"),
		[this]()
		{
			ClearDummies();
			A = SpawnDummy(RunwayStart, 0.f, true);
		},
		[this](float Elapsed)
		{
			UShieldbreakMovementComponent* Move = A->GetShieldbreakMovement();
			if (!Move)
			{
				Check(TEXT("Character uses the Shieldbreak movement component"), false);
				return true;
			}
			const float Speed = static_cast<float>(A->GetVelocity().Size2D());
			if (Elapsed > 0.4f)
			{
				A->AddMovementInput(FVector::ForwardVector, 1.f);
			}

			if (Phase == 0 && Elapsed > 0.4f)
			{
				A->SetSprinting(true);
				Phase = 1;
			}
			else if (Phase == 1 && (Speed >= Move->SprintSpeed - 30.f || Elapsed > 3.f))
			{
				Check(TEXT("Sprint reaches sprint speed"), Speed >= Move->SprintSpeed - 30.f, FString::Printf(TEXT("%s uu/s"), *Num(Speed)));
				A->StartCrouchOrSlide();
				Mark = Now();
				Phase = 2;
			}
			else if (Phase == 2 && Now() - Mark > 0.15f)
			{
				Check(TEXT("Crouching at sprint speed starts a slide"), Move->IsSliding(), FString::Printf(TEXT("%s uu/s, mode %d"), *Num(Speed), static_cast<int32>(Move->MovementMode)));
				Value0 = Speed;
				Phase = 3;
			}
			else if (Phase == 3 && Now() - Mark > 0.45f)
			{
				Check(TEXT("The slide keeps far more speed than a crouch-walk"), Speed > Move->MaxWalkSpeedCrouched * 2.f,
					FString::Printf(TEXT("%s uu/s after 0.45 s (crouch walk %s)"), *Num(Speed), *Num(Move->MaxWalkSpeedCrouched)));
				Value1 = Speed;
				A->Jump();
				Phase = 4;
			}
			else if (Phase == 4 && Now() - Mark > 0.65f)
			{
				Check(TEXT("Jumping out of a slide sets Momentum Carry"), Move->IsMomentumCarrying() && Move->IsFalling(),
					FString::Printf(TEXT("carry=%d falling=%d"), Move->IsMomentumCarrying(), Move->IsFalling()));
				// Compare with the speed at take-off: slide decay depends on frame rate on slow machines.
				Check(TEXT("...and keeps the speed in the air"), Speed >= Value1 * 0.95f && Speed > 500.f,
					FString::Printf(TEXT("%s uu/s in the air, %s at take-off"), *Num(Speed), *Num(Value1)));
				A->StopCrouchOrSlide();
				A->SetSprinting(false);
				return true;
			}
			return false;
		}, 8.f });

	// ---- Grenade: damage and grenade jump ----
	Steps.Add({ TEXT("Grenade damage and knockback"),
		[this]()
		{
			ClearDummies();
			A = SpawnDummy(RangeA, 0.f, false);
		},
		[this, MaxShield, ShieldOf](float Elapsed)
		{
			if (Phase == 0 && Elapsed > 0.6f)
			{
				FActorSpawnParameters Params;
				Params.Instigator = A;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AShieldbreakGrenade* Grenade = GetWorld()->SpawnActor<AShieldbreakGrenade>(AShieldbreakGrenade::StaticClass(),
					A->GetActorLocation() + FVector(60.f, 0.f, -60.f), FRotator::ZeroRotator, Params);
				if (!Grenade)
				{
					Check(TEXT("Grenade spawns"), false);
					return true;
				}
				Spawned.Add(Grenade);
				// Net velocity of ~50 uu/s straight down: it settles at A's feet and the fuse runs.
				Grenade->Launch(FVector::DownVector, FVector(0.f, 0.f, 1850.f));
				Value0 = 0.f;
				Mark = Now();
				Phase = 1;
			}
			else if (Phase == 1)
			{
				Value0 = FMath::Max(Value0, static_cast<float>(A->GetVelocity().Z));
				if (Now() - Mark > 4.f)
				{
					Check(TEXT("The explosion damages nearby characters"), ShieldOf(A) < MaxShield - 1.f, FString::Printf(TEXT("shield %s"), *Num(ShieldOf(A))));
					Check(TEXT("The explosion launches them upward (grenade jump)"), Value0 > 250.f, FString::Printf(TEXT("peak up-velocity %s"), *Num(Value0)));
					return true;
				}
			}
			return false;
		}, 8.f });

	// ---- Melee and assassination ----
	Steps.Add({ TEXT("Melee and assassination"),
		[this]()
		{
			ClearDummies();
			A = SpawnDummy(RangeA, 0.f, false);
			B = SpawnDummy(RangeA + FVector(150.f, 0.f, 0.f), 180.f, false);
		},
		[this, MaxShield, MaxHealth, ShieldOf, HealthOf](float Elapsed)
		{
			if (Phase == 0 && Elapsed > 0.6f)
			{
				A->Melee();
				Mark = Now();
				Phase = 1;
			}
			else if (Phase == 1 && Now() - Mark > 0.3f)
			{
				Check(TEXT("A frontal melee hits hard but does not kill"), B->IsAlive() && ShieldOf(B) + HealthOf(B) < MaxShield + MaxHealth - 50.f,
					FString::Printf(TEXT("shield %s health %s"), *Num(ShieldOf(B)), *Num(HealthOf(B))));
				B->SetActorRotation(FRotator(0.f, 0.f, 0.f)); // turn away from A
				Phase = 2;
			}
			else if (Phase == 2 && Now() - Mark > 1.3f)
			{
				// Re-place A right behind B (the first hit may have lunged or knocked them apart).
				A->SetActorLocation(B->GetActorLocation() - FVector(150.f, 0.f, 0.f));
				A->SetActorRotation(FRotator::ZeroRotator);
				A->Melee();
				Phase = 3;
			}
			else if (Phase == 3 && Now() - Mark > 1.7f)
			{
				Check(TEXT("A melee from behind is an assassination"), !B->IsAlive(), FString::Printf(TEXT("health %s"), *Num(HealthOf(B))));
				return true;
			}
			return false;
		}, 6.f });

	// ---- Power weapons and power-ups (d1204c4) ----
	Steps.Add({ TEXT("Arena power weapon and power-up placement"),
		[this]() { ClearDummies(); },
		[this](float Elapsed)
		{
			int32 Snipers = 0, Rockets = 0, Powerups = 0;
			for (TActorIterator<AShieldbreakWeaponPickup> It(GetWorld()); It; ++It)
			{
				Snipers += It->GetWeaponClass() == AWeapon_SniperRifle::StaticClass() ? 1 : 0;
				Rockets += It->GetWeaponClass() == AWeapon_RocketLauncher::StaticClass() ? 1 : 0;
			}
			for (TActorIterator<AShieldbreakPowerup> It(GetWorld()); It; ++It)
			{
				Powerups += It->Type == EShieldbreakPowerupType::Overshield ? 1 : 0;
			}
			Check(TEXT("The arena places two Sniper Rifles"), Snipers == 2, FString::Printf(TEXT("%d"), Snipers));
			Check(TEXT("...one Rocket Launcher"), Rockets == 1, FString::Printf(TEXT("%d"), Rockets));
			Check(TEXT("...and one Overshield"), Powerups == 1, FString::Printf(TEXT("%d"), Powerups));
			return true;
		}, 3.f });

	auto AimAt = [](AShieldbreakCharacter* Shooter, const FVector& Target)
	{
		if (AAIController* AI = Cast<AAIController>(Shooter ? Shooter->GetController() : nullptr))
		{
			// AAIController::UpdateControlRotation (its Tick) zeroes pitch unless focused on a pawn, and the
			// weapon may tick in between; freeze the controller so the aim we set is the aim that fires.
			AI->SetActorTickEnabled(false);
			AI->ClearFocus(EAIFocusPriority::Gameplay);
			AI->bSetControlRotationFromPawnOrientation = false;
			AI->SetControlRotation((Target - Shooter->GetAimOrigin()).Rotation());
		}
	};
	auto HeadOf = [](const AShieldbreakCharacter* C)
	{
		return C->GetHeadMesh() ? C->GetHeadMesh()->GetComponentLocation() : C->GetActorLocation() + FVector(0.f, 0.f, 72.f);
	};

	Steps.Add({ TEXT("Sniper Rifle body shots and headshot"),
		[this]()
		{
			ClearDummies();
			A = SpawnDummy(RangeA, 0.f, true);
			B = SpawnDummy(RangeA + FVector(700.f, 0.f, 0.f), 180.f, false);
			if (A)
			{
				A->GiveWeapon(AWeapon_SniperRifle::StaticClass());
				A->GiveWeapon(AWeapon_SniperRifle::StaticClass()); // inventory full: replaces the weapon in hand
			}
		},
		[this, MaxShield, MaxHealth, ShieldOf, HealthOf, AimAt, HeadOf](float Elapsed)
		{
			if (!B || B->IsActorBeingDestroyed())
			{
				return false;
			}
			// Chest for the body shots, head for the last one.
			AimAt(A, Phase < 6 ? B->GetActorLocation() + FVector(0.f, 0.f, 10.f) : HeadOf(B));

			if (Phase == 0 && Elapsed > 1.0f)
			{
				const AShieldbreakWeapon* Weapon = A->GetCurrentWeapon();
				Check(TEXT("Shooter holds the Sniper Rifle"), Weapon && Weapon->IsA<AWeapon_SniperRifle>(), Weapon ? Weapon->GetName() : FString(TEXT("none")));
				Value0 = ShieldOf(B) + HealthOf(B);
				{
					// Diagnostics: where the shot starts, where it points, and whether the head test would fire.
					const FVector Origin = A->GetAimOrigin();
					const FVector Dir = A->GetAimRotation().Vector();
					FHitResult HeadHit;
					const bool bHead = B->GetHeadMesh() && B->GetHeadMesh()->LineTraceComponent(HeadHit, Origin, Origin + Dir * 1000.f,
						FCollisionQueryParams(SCENE_QUERY_STAT(SelfTestHead), true));
					Lines.Add(FString::Printf(TEXT("INFO  sniper shot: eye z %s, aim pitch %s, target z %s, head z %s, head scale %s, head-ray hit %d"),
						*Num(Origin.Z), *Num(A->GetAimRotation().Pitch), *Num(B->GetActorLocation().Z), *Num(HeadOf(B).Z),
						*Num(B->GetHeadMesh() ? B->GetHeadMesh()->GetComponentScale().X : -1.f), bHead));
				}
				A->StartFire();
				Mark = Now();
				Phase = 1;
			}
			else if (Phase == 1 && Now() - Mark > 0.1f)
			{
				A->StopFire();
				Phase = 2;
			}
			else if (Phase == 2 && Now() - Mark > 0.9f)
			{
				const float Lost = Value0 - (ShieldOf(B) + HealthOf(B));
				Check(TEXT("A sniper body shot does 60"), FMath::IsNearlyEqual(Lost, 60.f, 1.f) && B->IsAlive(),
					FString::Printf(TEXT("lost %s, alive %d"), *Num(Lost), B->IsAlive()));
				A->StartFire();
				Mark = Now();
				Phase = 3;
			}
			else if (Phase == 3 && Now() - Mark > 0.1f)
			{
				A->StopFire();
				Phase = 4;
			}
			else if (Phase == 4 && Now() - Mark > 0.9f)
			{
				Check(TEXT("Two sniper body shots kill from full"), !B->IsAlive(), FString::Printf(TEXT("shield %s health %s"), *Num(ShieldOf(B)), *Num(HealthOf(B))));
				B = SpawnDummy(RangeA + FVector(700.f, 250.f, 0.f), 180.f, false); // clear of the ragdoll
				Mark = Now();
				Phase = 5;
			}
			else if (Phase == 5 && Now() - Mark > 0.8f)
			{
				Check(TEXT("Fresh target has a full shield"), ShieldOf(B) >= MaxShield - 0.01f && HealthOf(B) >= MaxHealth - 0.01f);
				Phase = 6;
			}
			else if (Phase == 6)
			{
				A->StartFire();
				Mark = Now();
				Phase = 7;
			}
			else if (Phase == 7 && Now() - Mark > 0.1f)
			{
				A->StopFire();
				Phase = 8;
			}
			else if (Phase == 8 && Now() - Mark > 0.9f)
			{
				Check(TEXT("A sniper headshot kills through a full shield"), !B->IsAlive(),
					FString::Printf(TEXT("shield %s health %s"), *Num(ShieldOf(B)), *Num(HealthOf(B))));
				return true;
			}
			return false;
		}, 10.f });

	Steps.Add({ TEXT("Rocket Launcher direct hit"),
		[this]()
		{
			ClearDummies();
			A = SpawnDummy(RangeA, 0.f, true);
			B = SpawnDummy(RangeA + FVector(700.f, 0.f, 0.f), 180.f, false);
			if (A)
			{
				A->GiveWeapon(AWeapon_RocketLauncher::StaticClass());
				A->GiveWeapon(AWeapon_RocketLauncher::StaticClass());
			}
		},
		[this, ShieldOf, HealthOf, AimAt](float Elapsed)
		{
			AimAt(A, B->GetActorLocation());
			if (Phase == 0 && Elapsed > 1.0f)
			{
				const AShieldbreakWeapon* Weapon = A->GetCurrentWeapon();
				Check(TEXT("Shooter holds the Rocket Launcher"), Weapon && Weapon->IsA<AWeapon_RocketLauncher>(), Weapon ? Weapon->GetName() : FString(TEXT("none")));
				A->StartFire();
				Mark = Now();
				Phase = 1;
			}
			else if (Phase == 1 && Now() - Mark > 0.05f)
			{
				A->StopFire();
				AShieldbreakRocket* Rocket = nullptr;
				for (TActorIterator<AShieldbreakRocket> It(GetWorld()); It; ++It)
				{
					Rocket = *It;
				}
				const float Ahead = Rocket ? static_cast<float>(FVector::Dist(Rocket->GetActorLocation(), A->GetAimOrigin())) : -1.f;
				Check(TEXT("Firing spawns a rocket ahead of the shooter"), Rocket != nullptr && Ahead > 60.f,
					FString::Printf(TEXT("%s uu from the eye"), *Num(Ahead)));
				Phase = 2;
			}
			else if (Phase == 2 && Now() - Mark > 1.5f)
			{
				Check(TEXT("A direct rocket hit kills from full"), !B->IsAlive(), FString::Printf(TEXT("shield %s health %s"), *Num(ShieldOf(B)), *Num(HealthOf(B))));
				Check(TEXT("...and the shooter 700 uu away is unharmed"), A->IsAlive() && ShieldOf(A) > 60.f, FString::Printf(TEXT("shooter shield %s"), *Num(ShieldOf(A))));
				return true;
			}
			return false;
		}, 6.f });

	Steps.Add({ TEXT("Rocket jump"),
		[this]()
		{
			ClearDummies();
			A = SpawnDummy(RangeA, 0.f, true);
			if (A)
			{
				A->GiveWeapon(AWeapon_RocketLauncher::StaticClass());
				A->GiveWeapon(AWeapon_RocketLauncher::StaticClass());
			}
		},
		[this, ShieldOf, HealthOf, AimAt](float Elapsed)
		{
			if (!A || A->IsActorBeingDestroyed())
			{
				return false;
			}
			AimAt(A, A->GetActorLocation() + FVector(5.f, 0.f, -500.f)); // straight at the feet
			Value0 = FMath::Max(Value0, static_cast<float>(A->GetVelocity().Z));

			// Pass 1 with a plain shield (is it survivable?), pass 2 with an overshield for the knockback check.
			if ((Phase == 0 || Phase == 3) && Elapsed > (Phase == 0 ? 1.0f : 0.f) && (Phase == 0 || Now() - Mark > 1.0f))
			{
				if (Phase == 3)
				{
					A->GetShieldHealth()->GrantOvershield(A->GetShieldHealth()->GetTuningData()->PowerupOvershield);
				}
				Value0 = 0.f;
				A->StartFire();
				Mark = Now();
				++Phase;
			}
			else if ((Phase == 1 || Phase == 4) && Now() - Mark > 0.05f)
			{
				A->StopFire();
				FString Rockets;
				for (TActorIterator<AShieldbreakRocket> It(GetWorld()); It; ++It)
				{
					Rockets += FString::Printf(TEXT(" (%s)"), *It->GetActorLocation().ToCompactString());
				}
				Lines.Add(FString::Printf(TEXT("INFO  rocket jump pass %d: shooter at %s, aim pitch %s, ammo %d, rockets:%s"), Phase == 1 ? 1 : 2,
					*A->GetActorLocation().ToCompactString(), *Num(A->GetAimRotation().Pitch),
					A->GetCurrentWeapon() ? A->GetCurrentWeapon()->GetAmmoInMagazine() : -1, Rockets.IsEmpty() ? TEXT(" none") : *Rockets));
				++Phase;
			}
			else if (Phase == 2 && Now() - Mark > 1.2f)
			{
				// 458b8d7: own rockets do 0.4x (88 at point blank) with 0.65x knockback.
				Check(TEXT("A rocket jump from full shield is survivable"), A->IsAlive(),
					FString::Printf(TEXT("shield %s health %s"), *Num(ShieldOf(A)), *Num(HealthOf(A))));
				Check(TEXT("...leaves 20-40 health"), A->IsAlive() && HealthOf(A) >= 20.f && HealthOf(A) <= 40.f,
					FString::Printf(TEXT("health %s"), *Num(HealthOf(A))));
				Check(TEXT("...and launches you upward"), Value0 > 400.f, FString::Printf(TEXT("peak up-velocity %s"), *Num(Value0)));
				// Fresh shooter for the overshield pass.
				ClearDummies();
				A = SpawnDummy(RangeA, 0.f, true);
				if (A)
				{
					A->GiveWeapon(AWeapon_RocketLauncher::StaticClass());
					A->GiveWeapon(AWeapon_RocketLauncher::StaticClass());
				}
				Mark = Now();
				Phase = 3;
			}
			else if (Phase == 5 && Now() - Mark > 1.2f)
			{
				Check(TEXT("With an overshield, a rocket at your feet is survivable"), A->IsAlive(),
					FString::Printf(TEXT("overshield %s shield %s health %s"), *Num(A->GetShieldHealth()->GetOvershield()), *Num(ShieldOf(A)), *Num(HealthOf(A))));
				Check(TEXT("...and launches you upward (rocket jump)"), Value0 > 400.f, FString::Printf(TEXT("peak up-velocity %s"), *Num(Value0)));
				return true;
			}
			return false;
		}, 10.f });

	Steps.Add({ TEXT("Rocket times out after 6 s"),
		[this]()
		{
			ClearDummies();
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AShieldbreakRocket* Rocket = GetWorld()->SpawnActor<AShieldbreakRocket>(AShieldbreakRocket::StaticClass(),
				RangeA + FVector(0.f, 0.f, 300.f), FRotator(90.f, 0.f, 0.f), Params);
			if (Rocket)
			{
				Rocket->Launch(FVector::UpVector);
			}
			Probe = Rocket;
			Mark = Now();
		},
		[this](float Elapsed)
		{
			AActor* Rocket = Probe.Get();
			const bool bGone = !IsValid(Rocket) || Rocket->IsActorBeingDestroyed() || !Rocket->GetActorEnableCollision();
			if (Phase == 0 && Elapsed > 0.1f)
			{
				Check(TEXT("A rocket fired into the open sky spawns"), IsValid(Rocket));
				Phase = 1;
			}
			else if (Phase == 1 && (bGone || Elapsed > 7.5f))
			{
				Check(TEXT("...and explodes on its own after ~6 s"), bGone && Elapsed > 5.5f && Elapsed < 7.f, FString::Printf(TEXT("gone at %s s"), *Num(Elapsed)));
				return true;
			}
			return false;
		}, 9.f });

	Steps.Add({ TEXT("Overshield power-up"),
		[this]()
		{
			ClearDummies();
			A = SpawnDummy(RangeA, 0.f, false);
			AShieldbreakPowerup* Powerup = GetWorld()->SpawnActorDeferred<AShieldbreakPowerup>(AShieldbreakPowerup::StaticClass(),
				FTransform(RangeB), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Powerup)
			{
				Powerup->RespawnTime = 3.f; // the arena uses 90 s
				Powerup->FinishSpawning(FTransform(RangeB));
				Spawned.Add(Powerup);
			}
			Probe = Powerup;
		},
		[this, MaxShield](float Elapsed)
		{
			AShieldbreakPowerup* Powerup = Cast<AShieldbreakPowerup>(Probe.Get());
			if (!Powerup)
			{
				Check(TEXT("Test power-up spawns"), false);
				return true;
			}
			UShieldHealthComponent* Shield = A->GetShieldHealth();
			if (Phase == 0 && Elapsed > 0.6f)
			{
				A->SetActorLocation(RangeB, false, nullptr, ETeleportType::TeleportPhysics);
				Mark = Now();
				Phase = 1;
			}
			else if (Phase == 1 && Now() - Mark > 0.2f)
			{
				Value0 = Shield->GetOvershield();
				Check(TEXT("Walking into the Overshield grants 140"), Value0 >= 139.f, FString::Printf(TEXT("overshield %s"), *Num(Value0)));
				Check(TEXT("...and the orb is consumed"), !Powerup->IsAvailable());
				A->SetActorLocation(RangeA, false, nullptr, ETeleportType::TeleportPhysics);
				Mark = Now();
				Phase = 2;
			}
			else if (Phase == 2 && Now() - Mark > 2.f)
			{
				const float Decay = (Value0 - Shield->GetOvershield()) / (Now() - Mark);
				Check(TEXT("The overshield decays at ~2/s"), Decay > 1.5f && Decay < 2.5f, FString::Printf(TEXT("%s/s, now %s"), *Num(Decay), *Num(Shield->GetOvershield())));
				Damage(A, 50.f, UDamageType_Ballistic::StaticClass());
				Check(TEXT("Damage drains the overshield before the shield"), Shield->GetShield() >= MaxShield - 0.01f,
					FString::Printf(TEXT("overshield %s shield %s"), *Num(Shield->GetOvershield()), *Num(Shield->GetShield())));
				Phase = 3;
			}
			else if (Phase == 3 && Now() - Mark > 3.6f)
			{
				Check(TEXT("The orb respawns after its timer"), Powerup->IsAvailable());
				return true;
			}
			return false;
		}, 8.f });

	// ---- Teams and friendly fire ----
	Steps.Add({ TEXT("Teams and friendly fire"),
		[this]()
		{
			ClearDummies();
			// Bot controllers give the dummies PlayerStates (teams live there); their AI is frozen.
			auto MakeTeamDummy = [this](const FVector& Location, float Yaw, uint8 Team) -> AShieldbreakCharacter*
			{
				AShieldbreakCharacter* Dummy = SpawnDummy(Location, Yaw, false);
				if (!Dummy)
				{
					return nullptr;
				}
				Dummy->AIControllerClass = AShieldbreakBotController::StaticClass();
				Dummy->SpawnDefaultController();
				if (AController* DummyController = Dummy->GetController())
				{
					DummyController->SetActorTickEnabled(false);
					Spawned.Add(DummyController);
					if (AShieldbreakPlayerState* State = DummyController->GetPlayerState<AShieldbreakPlayerState>())
					{
						State->SetTeamId(Team);
					}
				}
				return Dummy;
			};
			A = MakeTeamDummy(RangeA, 0.f, ShieldbreakTeams::Red);
			B = MakeTeamDummy(RangeB, 180.f, ShieldbreakTeams::Red);
		},
		[this, MaxShield, ShieldOf](float Elapsed)
		{
			if (Elapsed < 0.6f)
			{
				return false;
			}
			if (!ShieldbreakTeams::IsTeamGame(this))
			{
				Lines.Add(TEXT("INFO  free-for-all mode: team checks skipped"));
				return true;
			}
			AController* TeamController = A->GetController();
			Check(TEXT("Team dummies resolve to the same team"), ShieldbreakTeams::AreTeammates(A, B),
				FString::Printf(TEXT("teams %d / %d"), ShieldbreakTeams::GetTeam(A), ShieldbreakTeams::GetTeam(B)));

			UGameplayStatics::ApplyDamage(B, 20.f, TeamController, A, UDamageType_Ballistic::StaticClass());
			Check(TEXT("Friendly fire is off by default"), ShieldOf(B) >= MaxShield - 0.01f, FString::Printf(TEXT("teammate shield %s"), *Num(ShieldOf(B))));

			UGameplayStatics::ApplyDamage(A, 20.f, TeamController, A, UDamageType_Explosive::StaticClass());
			Check(TEXT("Your own explosives still hurt you"), ShieldOf(A) < MaxShield - 1.f, FString::Printf(TEXT("own shield %s"), *Num(ShieldOf(A))));

			if (AShieldbreakPlayerState* StateB = B->GetController() ? B->GetController()->GetPlayerState<AShieldbreakPlayerState>() : nullptr)
			{
				StateB->SetTeamId(ShieldbreakTeams::Blue);
			}
			UGameplayStatics::ApplyDamage(B, 20.f, TeamController, A, UDamageType_Ballistic::StaticClass());
			Check(TEXT("Enemies take damage"), ShieldOf(B) < MaxShield - 1.f, FString::Printf(TEXT("enemy shield %s"), *Num(ShieldOf(B))));
			return true;
		}, 6.f });

	// ---- Bots ----
	Steps.Add({ TEXT("Bot match sanity (75 s)"),
		[this]()
		{
			ClearDummies();
			Phase = 0;
			Value0 = static_cast<float>(GFrameCounter);
			Value1 = static_cast<float>(FPlatformTime::Seconds());
			if (AShieldbreakGameMode* GameMode = GetWorld()->GetAuthGameMode<AShieldbreakGameMode>())
			{
				GameMode->SetBotCount(4);
			}
		},
		[this](float Elapsed)
		{
			// Chase-camera screenshots of a bot at 25 s, 45 s and 65 s (third-person art, animation, team colors).
			if (Phase < 3 && Elapsed > 25.f + Phase * 20.f)
			{
				APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
				AShieldbreakCharacter* Subject = nullptr;
				for (TActorIterator<AShieldbreakCharacter> It(GetWorld()); It; ++It)
				{
					if (It->IsAlive() && It->GetController() && !It->GetController()->IsPlayerController())
					{
						Subject = *It;
						if (Phase % 2 == 0 && Subject->GetVelocity().SizeSquared2D() > 10000.f)
						{
							break; // prefer a moving bot
						}
					}
				}
				if (Viewer && Subject)
				{
					if (!ChaseCamera)
					{
						FActorSpawnParameters CameraParams;
						CameraParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						ChaseCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, CameraParams);
					}
					ChaseSubject = Subject;
					Viewer->SetViewTarget(ChaseCamera);
					ShotDue = Now() + 1.5f; // let the camera settle on the subject first
				}
				++Phase;
			}
			if (ChaseCamera && ChaseSubject.IsValid())
			{
				const FVector Back = ChaseSubject->GetActorForwardVector() * -320.f + ChaseSubject->GetActorRightVector() * 90.f + FVector(0.f, 0.f, 110.f);
				const FVector CameraLocation = ChaseSubject->GetActorLocation() + Back;
				ChaseCamera->SetActorLocationAndRotation(CameraLocation, (ChaseSubject->GetActorLocation() + FVector(0.f, 0.f, 40.f) - CameraLocation).Rotation());
			}
			if (ShotDue > 0.f && Now() >= ShotDue)
			{
				const FString Dir = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("SelfTest");
				IFileManager::Get().MakeDirectory(*Dir, true);
				FScreenshotRequest::RequestScreenshot(Dir / FString::Printf(TEXT("bot_%d.png"), ShotIndex++), true, false);
				ShotDue = -1.f;
			}

			if (Elapsed < 75.f)
			{
				return false;
			}
			Lines.Add(FString::Printf(TEXT("INFO  %d third-person screenshots in Saved/Screenshots/SelfTest"), ShotIndex));
			int32 Kills = 0;
			int32 Bots = 0;
			if (const AGameStateBase* GameState = GetWorld()->GetGameState())
			{
				for (APlayerState* Entry : GameState->PlayerArray)
				{
					if (const AShieldbreakPlayerState* SBState = Cast<AShieldbreakPlayerState>(Entry))
					{
						if (Entry->IsABot())
						{
							++Bots;
							Kills += SBState->GetKills() + SBState->GetDeaths();
						}
					}
				}
			}
			const float RealSeconds = static_cast<float>(FPlatformTime::Seconds()) - Value1;
			const float Fps = (static_cast<float>(GFrameCounter) - Value0) / FMath::Max(1.f, RealSeconds);
			Lines.Add(FString::Printf(TEXT("INFO  average %.1f FPS over %.0f real seconds with 4 bots"), Fps, RealSeconds));
			Check(TEXT("4 bots joined"), Bots == 4, FString::Printf(TEXT("%d bots"), Bots));
			Check(TEXT("Bots find and fight each other"), Kills > 0, FString::Printf(TEXT("%d kills+deaths in 75 s"), Kills));
			return true;
		}, 90.f });
}
