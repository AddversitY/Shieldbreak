// -SBImpactShots: screenshots of bullet hits caught mid-effect (wall sparks and dust, bullet holes, a shield flare, bare
// armor sparks, the muzzle flash), then a 20 s firing frame-rate figure. Run it twice, once with
// -ExecCmds="sb.ImpactFX 0", for the before/after cost of the effects under fire.

#include "Test/ShieldbreakSelfTest.h"

#include "Shieldbreak.h"
#include "Engine/World.h"
#include "Game/ShieldbreakArena.h"
#include "Game/ShieldbreakGameMode.h"
#include "Game/ShieldbreakTeams.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Player/ShieldbreakCharacter.h"
#include "Shield/ShieldHealthComponent.h"
#include "Visuals/ShieldbreakImpactFX.h"
#include "Weapons/ShieldbreakDamageTypes.h"
#include "Weapons/ShieldbreakWeapon.h"

void AShieldbreakSelfTest::BuildImpactShotSteps()
{
	ModeTag = TEXT("ImpactShots");
	auto Player = [this]() -> AShieldbreakCharacter*
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		return PC ? Cast<AShieldbreakCharacter>(PC->GetPawn()) : nullptr;
	};
	auto Aim = [this, Player](const FVector& Target)
	{
		AShieldbreakCharacter* P = Player();
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController(); P && PC)
		{
			PC->SetControlRotation((Target - P->GetAimOrigin()).Rotation());
		}
	};
	auto LogFX = [this](const TCHAR* Where)
	{
		const UShieldbreakImpactFXSubsystem* FX = UShieldbreakImpactFXSubsystem::Get(this);
		Lines.Add(FString::Printf(TEXT("INFO  %s: impacts %d (metal %d, dirt %d, armor %d, shield %d), decals %d, muzzle flashes %d, live sparks %d, live cards %d, materials %d"),
			Where, FX ? FX->GetImpactCount() : -1, FX ? FX->GetSurfaceCount(EShieldbreakImpactSurface::Metal) : -1,
			FX ? FX->GetSurfaceCount(EShieldbreakImpactSurface::Dirt) : -1, FX ? FX->GetSurfaceCount(EShieldbreakImpactSurface::Armor) : -1,
			FX ? FX->GetSurfaceCount(EShieldbreakImpactSurface::Shield) : -1, FX ? FX->GetDecalsPlaced() : -1, FX ? FX->GetMuzzleFlashCount() : -1,
			FX ? FX->GetLiveSparkCount() : -1, FX ? FX->GetLiveCardCount() : -1, FX && FX->HasMaterials()));
	};
	// Tap the trigger (bursts and semi-autos need a fresh press) every Interval seconds until Stop.
	auto Trigger = [Player](float Elapsed, float Interval, float Stop)
	{
		AShieldbreakCharacter* P = Player();
		if (!P) { return; }
		const float Phase = FMath::Fmod(Elapsed, Interval);
		if (Elapsed < Stop && Phase < Interval * 0.5f) { P->StartFire(); }
		else { P->StopFire(); }
	};
	// Screenshots at fixed times after Begin, named <Name>_<index>; Mark holds the next index.
	auto Shot = [this](const TCHAR* Name, TFunction<void()> Begin, TFunction<void(float)> During, TArray<float> ShotTimes, float Length)
	{
		Steps.Add({ Name, [this, Begin]() { Mark = 0.f; if (Begin) Begin(); },
			[this, Name, During, ShotTimes, Length](float Elapsed)
			{
				if (During) { During(Elapsed); }
				const int32 Next = FMath::Max(0, static_cast<int32>(Mark));
				if (ShotTimes.IsValidIndex(Next) && Elapsed >= ShotTimes[Next])
				{
					TakeShot(FString::Printf(TEXT("%s_%d"), Name, Next), true);
					Mark = static_cast<float>(Next + 1);
				}
				return Elapsed >= Length;
			}, Length + 5.f });
	};

	Steps.Add({ TEXT("No bots; possess a soldier at the first spawn"),
		[this]()
		{
			if (AShieldbreakGameMode* GameMode = GetWorld()->GetAuthGameMode<AShieldbreakGameMode>())
			{
				GameMode->SetBotCount(0);
			}
			const AShieldbreakArena* Arena = AShieldbreakArena::Find(GetWorld());
			const FTransform Where = Arena && Arena->GetSpawnTransforms().Num() > 0 ? Arena->GetSpawnTransforms()[0] : FTransform(FVector(0.f, 0.f, 200.f));
			A = PossessLocalDummy(Where);
		},
		[](float Elapsed) { return Elapsed > 9.f; }, 12.f }); // also waits out the controls overlay

	// The nearest wall 5-15 m away, looking around the spawn: impacts are judged up close, as in a firefight.
	Steps.Add({ TEXT("Face a wall"),
		[this, Player, Aim]()
		{
			Value0 = 0.f;
			AShieldbreakCharacter* P = Player();
			if (!P) { return; }
			const FVector Eye = P->GetAimOrigin();
			float BestScore = TNumericLimits<float>::Max();
			FVector Best = Eye + P->GetActorForwardVector() * 800.f;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakImpactShots), false, P);
			for (int32 Step = 0; Step < 24; ++Step)
			{
				const FVector Dir = FRotator(-4.f, P->GetActorRotation().Yaw + Step * 15.f, 0.f).Vector();
				FHitResult Hit;
				if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Dir * 2000.f, ECC_Visibility, Params) && Hit.Distance > 250.f
					&& FMath::Abs(Hit.ImpactNormal.Z) < 0.4f)
				{
					const float Score = FMath::Abs(Hit.Distance - 420.f);
					if (Score < BestScore) { BestScore = Score; Best = Hit.ImpactPoint; }
				}
			}
			Aim(Best);
			Lines.Add(FString::Printf(TEXT("INFO  wall target %s (%.0f cm off ideal)"), *Best.ToString(), BestScore));
		},
		[](float Elapsed) { return Elapsed > 1.f; }, 3.f });

	// Mid-burst: the muzzle flash and the first sparks, then the dust and fading glow.
	Shot(TEXT("fx_0_wall_burst"), nullptr, [Trigger](float Elapsed) { Trigger(Elapsed, 0.6f, 0.3f); },
		{ 0.02f, 0.05f, 0.09f, 0.14f, 0.25f, 0.45f }, 1.2f);
	// Several bursts later: bullet holes, lingering dust.
	Shot(TEXT("fx_1_wall_holes"), nullptr, [Trigger](float Elapsed) { Trigger(Elapsed, 0.6f, 3.f); }, { 1.55f, 3.6f }, 4.f);

	// A shielded enemy 7 m ahead, then the same enemy with the shield gone.
	Steps.Add({ TEXT("Enemy target"),
		[this, Player, Aim]()
		{
			AShieldbreakCharacter* P = Player();
			if (!P) { return; }
			const FVector Forward = FRotator(0.f, P->GetActorRotation().Yaw, 0.f).Vector();
			B = SpawnTeamDummy(P->GetActorLocation() + Forward * 700.f, P->GetActorRotation().Yaw + 160.f, ShieldbreakTeams::Blue);
			if (B) { Aim(B->GetActorLocation() + FVector(0.f, 0.f, 25.f)); }
			if (AShieldbreakWeapon* Weapon = P->GetCurrentWeapon()) { Weapon->AddReserveAmmo(200); }
		},
		[](float Elapsed) { return Elapsed > 1.f; }, 3.f });
	Shot(TEXT("fx_2_shield_hit"), nullptr, [Trigger](float Elapsed) { Trigger(Elapsed, 0.6f, 0.3f); }, { 0.03f, 0.07f, 0.12f, 0.2f }, 0.8f);
	Shot(TEXT("fx_3_armor_hit"),
		[this]()
		{
			if (B && B->GetShieldHealth())
			{
				// Shield off: the next rounds land on bare armor.
				Damage(B, B->GetShieldHealth()->GetShield(), UDamageType_Ballistic::StaticClass());
			}
		},
		[Trigger](float Elapsed) { if (Elapsed > 0.4f) Trigger(Elapsed - 0.4f, 0.6f, 0.3f); }, { 0.43f, 0.47f, 0.52f, 0.6f }, 1.2f);
	Steps.Add({ TEXT("Log effect counts"), [this, LogFX]() { LogFX(TEXT("after captures")); if (B) { B->Destroy(); B = nullptr; } },
		nullptr, 2.f });

	// Firing frame rate: 20 s of bursts into the wall from the same spot (holes, sparks, dust and lights all live).
	Steps.Add({ TEXT("Face the wall again"),
		[this, Player]()
		{
			if (AShieldbreakCharacter* P = Player())
			{
				if (AShieldbreakWeapon* Weapon = P->GetCurrentWeapon()) { Weapon->AddReserveAmmo(400); }
			}
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				PC->SetControlRotation(FRotator(-4.f, PC->GetControlRotation().Yaw - 20.f, 0.f));
			}
		},
		[](float Elapsed) { return Elapsed > 1.f; }, 3.f });
	Steps.Add({ TEXT("Firing FPS (20 s)"),
		[this]()
		{
			Value0 = static_cast<float>(GFrameCounter);
			Value1 = static_cast<float>(FPlatformTime::Seconds());
		},
		[this, Trigger, LogFX, Player](float Elapsed)
		{
			Trigger(Elapsed, 0.5f, 20.f);
			if (Elapsed < 20.f) { return false; }
			if (AShieldbreakCharacter* P = Player()) { P->StopFire(); }
			const double Seconds = FPlatformTime::Seconds() - Value1;
			const IConsoleVariable* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("sb.ImpactFX"));
			Lines.Add(FString::Printf(TEXT("INFO  firing FPS %.1f over %.1f s (sb.ImpactFX %d)"), (GFrameCounter - Value0) / Seconds, Seconds,
				Enabled ? Enabled->GetInt() : -1));
			LogFX(TEXT("after firing"));
			return true;
		}, 25.f });
}
