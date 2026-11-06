#include "Audio/ShieldbreakSFX.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"

namespace ShieldbreakSFXPrivate
{
	constexpr int32 SampleRate = 24000;

	/** Float scratch buffer with the handful of building blocks the sounds are made from. */
	struct FSynth
	{
		TArray<float> Samples;
		FRandomStream Rng;

		FSynth(float Seconds, int32 Seed)
			: Rng(Seed)
		{
			Samples.SetNumZeroed(FMath::Max(1, FMath::RoundToInt(Seconds * SampleRate)));
		}

		static float TimeAt(int32 Index) { return static_cast<float>(Index) / SampleRate; }

		/** Linear attack then exponential decay. */
		static float Envelope(float Time, float Attack, float Decay)
		{
			if (Time < Attack)
			{
				return Attack > 0.f ? Time / Attack : 1.f;
			}
			return FMath::Exp(-(Time - Attack) / FMath::Max(Decay, KINDA_SMALL_NUMBER));
		}

		/** Sine whose frequency glides exponentially from StartHz to EndHz over GlideTime. */
		void AddTone(float Gain, float StartHz, float EndHz, float GlideTime, float Attack, float Decay, float StartTime = 0.f, float Harmonic = 0.f)
		{
			float Phase = 0.f;
			const int32 First = FMath::RoundToInt(StartTime * SampleRate);
			for (int32 Index = First; Index < Samples.Num(); ++Index)
			{
				const float Local = TimeAt(Index - First);
				const float Alpha = FMath::Clamp(Local / FMath::Max(GlideTime, KINDA_SMALL_NUMBER), 0.f, 1.f);
				const float Hz = StartHz * FMath::Pow(EndHz / StartHz, Alpha);
				Phase += 2.f * UE_PI * Hz / SampleRate;
				const float Wave = FMath::Sin(Phase) + Harmonic * FMath::Sin(2.f * Phase);
				Samples[Index] += Gain * Wave * Envelope(Local, Attack, Decay);
			}
		}

		/** White noise through a one-pole low-pass whose cutoff glides from StartHz to EndHz. */
		void AddNoise(float Gain, float StartHz, float EndHz, float Attack, float Decay, float StartTime = 0.f)
		{
			float Filtered = 0.f;
			const int32 First = FMath::RoundToInt(StartTime * SampleRate);
			const float Length = FMath::Max(TimeAt(Samples.Num() - First), KINDA_SMALL_NUMBER);
			for (int32 Index = First; Index < Samples.Num(); ++Index)
			{
				const float Local = TimeAt(Index - First);
				const float Cutoff = FMath::Lerp(StartHz, EndHz, Local / Length);
				const float Coefficient = 1.f - FMath::Exp(-2.f * UE_PI * Cutoff / SampleRate);
				Filtered += Coefficient * (Rng.FRandRange(-1.f, 1.f) - Filtered);
				Samples[Index] += Gain * Filtered * Envelope(Local, Attack, Decay);
			}
		}

		/** Square-ish buzz (clipped sine) gliding between two pitches. */
		void AddBuzz(float Gain, float StartHz, float EndHz, float Attack, float Decay)
		{
			float Phase = 0.f;
			const float Length = FMath::Max(TimeAt(Samples.Num()), KINDA_SMALL_NUMBER);
			for (int32 Index = 0; Index < Samples.Num(); ++Index)
			{
				const float Local = TimeAt(Index);
				Phase += 2.f * UE_PI * FMath::Lerp(StartHz, EndHz, Local / Length) / SampleRate;
				Samples[Index] += Gain * FMath::Clamp(FMath::Sin(Phase) * 3.f, -1.f, 1.f) * Envelope(Local, Attack, Decay);
			}
		}

		/** Peak-normalize and convert to 16-bit PCM with a short fade-out so nothing clicks. */
		TArray<int16> ToPCM(float Peak = 0.85f) const
		{
			float Max = KINDA_SMALL_NUMBER;
			for (const float Sample : Samples)
			{
				Max = FMath::Max(Max, FMath::Abs(Sample));
			}

			const int32 FadeSamples = FMath::Min(Samples.Num(), SampleRate / 100);
			TArray<int16> Out;
			Out.SetNumUninitialized(Samples.Num());
			for (int32 Index = 0; Index < Samples.Num(); ++Index)
			{
				const int32 FromEnd = Samples.Num() - 1 - Index;
				const float Fade = FromEnd < FadeSamples ? static_cast<float>(FromEnd) / FadeSamples : 1.f;
				Out[Index] = static_cast<int16>(FMath::Clamp(Samples[Index] / Max * Peak * Fade, -1.f, 1.f) * 32767.f);
			}
			return Out;
		}
	};

	TArray<int16> Synthesize(EShieldbreakSound Sound)
	{
		switch (Sound)
		{
		case EShieldbreakSound::BattleRifleShot:
		{
			FSynth S(0.32f, 11);
			S.AddNoise(0.9f, 5000.f, 1500.f, 0.001f, 0.03f);
			S.AddTone(0.8f, 120.f, 55.f, 0.08f, 0.002f, 0.06f);
			S.AddNoise(0.25f, 900.f, 300.f, 0.01f, 0.12f);
			return S.ToPCM();
		}
		case EShieldbreakSound::SMGShot:
		{
			FSynth S(0.14f, 12);
			S.AddNoise(0.8f, 7000.f, 2500.f, 0.0005f, 0.018f);
			S.AddTone(0.5f, 180.f, 90.f, 0.04f, 0.001f, 0.03f);
			return S.ToPCM(0.7f);
		}
		case EShieldbreakSound::PlasmaShot:
		{
			FSynth S(0.2f, 13);
			S.AddTone(0.8f, 1600.f, 320.f, 0.12f, 0.002f, 0.05f, 0.f, 0.35f);
			S.AddBuzz(0.2f, 400.f, 160.f, 0.001f, 0.04f);
			S.AddNoise(0.15f, 8000.f, 3000.f, 0.f, 0.02f);
			return S.ToPCM(0.75f);
		}
		case EShieldbreakSound::GrenadeExplosion:
		{
			FSynth S(1.4f, 14);
			S.AddNoise(1.f, 2500.f, 150.f, 0.004f, 0.32f);
			S.AddTone(0.9f, 70.f, 32.f, 0.6f, 0.008f, 0.38f);
			S.AddNoise(0.3f, 400.f, 80.f, 0.05f, 0.5f, 0.05f);
			return S.ToPCM(0.95f);
		}
		case EShieldbreakSound::ShieldRecharge:
		{
			// The rising "charging" whine: long glide, octave harmonic, slow attack.
			FSynth S(1.8f, 15);
			S.AddTone(0.6f, 240.f, 1150.f, 1.6f, 0.25f, 4.f, 0.f, 0.3f);
			S.AddTone(0.15f, 480.f, 2300.f, 1.6f, 0.4f, 2.f);
			return S.ToPCM(0.5f);
		}
		case EShieldbreakSound::ShieldPop:
		{
			// An electric crack, a falling two-tone "pew" and a low thump: unmistakable across the map.
			FSynth S(0.5f, 16);
			S.AddNoise(0.6f, 14000.f, 5000.f, 0.f, 0.018f);
			S.AddNoise(0.5f, 9000.f, 1200.f, 0.002f, 0.07f);
			S.AddTone(0.7f, 1500.f, 180.f, 0.22f, 0.001f, 0.12f, 0.f, 0.25f);
			S.AddTone(0.3f, 2250.f, 270.f, 0.22f, 0.001f, 0.08f);
			S.AddTone(0.45f, 130.f, 48.f, 0.15f, 0.002f, 0.09f);
			S.AddBuzz(0.15f, 90.f, 60.f, 0.f, 0.15f);
			return S.ToPCM();
		}
		case EShieldbreakSound::ShieldLowBeep:
		{
			FSynth S(0.09f, 17);
			S.AddTone(0.6f, 1750.f, 1750.f, 0.1f, 0.003f, 0.05f);
			return S.ToPCM(0.45f);
		}
		case EShieldbreakSound::HitMarker:
		{
			FSynth S(0.06f, 18);
			S.AddTone(0.6f, 2700.f, 2400.f, 0.04f, 0.0005f, 0.012f);
			S.AddNoise(0.2f, 9000.f, 6000.f, 0.f, 0.005f);
			return S.ToPCM(0.5f);
		}
		case EShieldbreakSound::KillMarker:
		{
			FSynth S(0.16f, 19);
			S.AddTone(0.6f, 1800.f, 1800.f, 0.05f, 0.001f, 0.03f);
			S.AddTone(0.6f, 2700.f, 2700.f, 0.05f, 0.001f, 0.04f, 0.06f);
			return S.ToPCM(0.6f);
		}
		case EShieldbreakSound::VentSuccess:
		{
			FSynth S(0.32f, 20);
			S.AddTone(0.5f, 660.f, 660.f, 0.1f, 0.002f, 0.06f);
			S.AddTone(0.5f, 990.f, 990.f, 0.1f, 0.002f, 0.06f, 0.06f);
			S.AddTone(0.6f, 1320.f, 1320.f, 0.1f, 0.002f, 0.12f, 0.12f, 0.2f);
			return S.ToPCM(0.6f);
		}
		case EShieldbreakSound::VentFail:
		{
			FSynth S(0.35f, 21);
			S.AddBuzz(0.6f, 190.f, 100.f, 0.002f, 0.18f);
			return S.ToPCM(0.55f);
		}
		case EShieldbreakSound::BreakPulse:
		{
			FSynth S(0.7f, 22);
			S.AddTone(1.f, 110.f, 38.f, 0.35f, 0.004f, 0.22f);
			S.AddNoise(0.5f, 1200.f, 200.f, 0.002f, 0.14f);
			S.AddTone(0.2f, 300.f, 1800.f, 0.3f, 0.01f, 0.12f);
			return S.ToPCM(0.95f);
		}
		case EShieldbreakSound::Melee:
		{
			FSynth S(0.2f, 23);
			S.AddTone(0.8f, 150.f, 70.f, 0.06f, 0.002f, 0.05f);
			S.AddNoise(0.5f, 1800.f, 500.f, 0.001f, 0.03f);
			return S.ToPCM(0.8f);
		}
		case EShieldbreakSound::Death:
		{
			FSynth S(0.6f, 24);
			S.AddTone(0.6f, 420.f, 80.f, 0.5f, 0.01f, 0.25f, 0.f, 0.3f);
			S.AddNoise(0.3f, 3000.f, 300.f, 0.f, 0.12f);
			return S.ToPCM(0.7f);
		}
		case EShieldbreakSound::RailCharge:
		{
			// Rising whine that tops out right at a full charge (0.8 s): the audible tell.
			FSynth S(0.85f, 25);
			S.AddTone(0.5f, 300.f, 1900.f, 0.8f, 0.05f, 6.f, 0.f, 0.4f);
			S.AddBuzz(0.08f, 60.f, 240.f, 0.05f, 6.f);
			return S.ToPCM(0.55f);
		}
		case EShieldbreakSound::RailFire:
		{
			FSynth S(0.5f, 26);
			S.AddNoise(0.8f, 12000.f, 2000.f, 0.f, 0.05f);
			S.AddTone(0.7f, 2400.f, 200.f, 0.2f, 0.001f, 0.12f, 0.f, 0.5f);
			S.AddTone(0.6f, 90.f, 40.f, 0.3f, 0.002f, 0.15f);
			return S.ToPCM(0.9f);
		}
		case EShieldbreakSound::AegisRaise:
		{
			FSynth S(0.3f, 27);
			S.AddTone(0.5f, 180.f, 520.f, 0.15f, 0.01f, 0.2f, 0.f, 0.5f);
			S.AddNoise(0.15f, 5000.f, 2000.f, 0.f, 0.1f);
			return S.ToPCM(0.6f);
		}
		case EShieldbreakSound::AegisHit:
		{
			FSynth S(0.12f, 28);
			S.AddTone(0.5f, 1300.f, 900.f, 0.08f, 0.001f, 0.04f, 0.f, 0.3f);
			S.AddNoise(0.3f, 9000.f, 4000.f, 0.f, 0.02f);
			return S.ToPCM(0.5f);
		}
		case EShieldbreakSound::CamoOn:
		{
			FSynth S(0.6f, 29);
			S.AddTone(0.4f, 900.f, 200.f, 0.5f, 0.02f, 0.3f, 0.f, 0.6f);
			S.AddNoise(0.2f, 3000.f, 600.f, 0.05f, 0.3f);
			return S.ToPCM(0.5f);
		}
		case EShieldbreakSound::ShieldHitMarker:
		{
			FSynth S(0.06f, 30);
			S.AddTone(0.5f, 2100.f, 1950.f, 0.04f, 0.0005f, 0.012f, 0.f, 0.2f);
			S.AddNoise(0.25f, 10000.f, 7000.f, 0.f, 0.005f);
			return S.ToPCM(0.75f);
		}
		case EShieldbreakSound::ShieldBreakMarker:
		{
			FSynth S(0.22f, 31);
			S.AddNoise(0.7f, 13000.f, 3000.f, 0.f, 0.025f);
			S.AddTone(0.6f, 3200.f, 900.f, 0.12f, 0.0005f, 0.06f, 0.f, 0.3f);
			S.AddTone(0.3f, 4800.f, 1350.f, 0.12f, 0.0005f, 0.04f);
			return S.ToPCM(0.8f);
		}
		case EShieldbreakSound::GrenadeThrow:
		{
			FSynth S(0.3f, 32);
			S.AddNoise(0.6f, 900.f, 2600.f, 0.08f, 0.09f);
			return S.ToPCM(0.5f);
		}
		case EShieldbreakSound::GrenadeBounce:
		{
			FSynth S(0.18f, 33);
			S.AddTone(0.6f, 1150.f, 1050.f, 0.1f, 0.0005f, 0.035f, 0.f, 0.4f);
			S.AddTone(0.4f, 2730.f, 2600.f, 0.1f, 0.0005f, 0.02f);
			S.AddNoise(0.4f, 6000.f, 2000.f, 0.f, 0.01f);
			return S.ToPCM(0.6f);
		}
		default:
			return TArray<int16>();
		}
	}

	const TArray<int16>& GetPCM(EShieldbreakSound Sound)
	{
		static TArray<int16> Cache[static_cast<int32>(EShieldbreakSound::Count)];
		const int32 Index = FMath::Clamp(static_cast<int32>(Sound), 0, static_cast<int32>(EShieldbreakSound::Count) - 1);
		if (Cache[Index].Num() == 0)
		{
			Cache[Index] = Synthesize(static_cast<EShieldbreakSound>(Index));
		}
		return Cache[Index];
	}


	// ---- Attenuation and voice limits ----

	enum class EAtten : uint8 { Default, Gun, Tail, Far, Explosion, Foley, Shield, Num };

	USoundAttenuation* GetAttenuation(EAtten Kind = EAtten::Default)
	{
		static USoundAttenuation* Cache[static_cast<int32>(EAtten::Num)] = {};
		USoundAttenuation*& Attenuation = Cache[static_cast<int32>(Kind)];
		if (!Attenuation)
		{
			// Inner radius, falloff distance, and whether distance muffles the sound (air absorption).
			struct FShape { float Inner; float Falloff; bool bMuffle; };
			static const FShape Shapes[] = {
				{ 400.f, 7000.f, false },   // Default
				{ 300.f, 6000.f, true },    // Gun: close layer, crossfades into Far by ~55 m
				{ 800.f, 9000.f, true },    // Tail
				{ 1000.f, 26000.f, true },  // Far: distant gunfire carries across the whole map
				{ 600.f, 14000.f, true },   // Explosion
				{ 150.f, 2600.f, false },   // Foley: steps, reloads, bounces
				{ 200.f, 4500.f, false },   // Shield
			};
			const FShape& Shape = Shapes[static_cast<int32>(Kind)];
			Attenuation = NewObject<USoundAttenuation>(GetTransientPackage());
			Attenuation->AddToRoot();
			FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
			Settings.bAttenuate = true;
			Settings.bSpatialize = true;
			Settings.AttenuationShape = EAttenuationShape::Sphere;
			Settings.AttenuationShapeExtents = FVector(Shape.Inner, 0.f, 0.f);
			Settings.FalloffDistance = Shape.Falloff;
			Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
			Settings.bAttenuateWithLPF = Shape.bMuffle;
			Settings.LPFRadiusMin = 1500.f;
			Settings.LPFRadiusMax = Shape.Falloff;
			Settings.LPFFrequencyAtMin = 20000.f;
			Settings.LPFFrequencyAtMax = 3500.f;
		}
		return Attenuation;
	}

	/** Two voice pools: combat (guns, explosions, shields) and foley (steps, reloads, bounces). */
	USoundConcurrency* GetConcurrency(bool bFoley)
	{
		static USoundConcurrency* Pools[2] = {};
		USoundConcurrency*& Pool = Pools[bFoley ? 1 : 0];
		if (!Pool)
		{
			Pool = NewObject<USoundConcurrency>(GetTransientPackage());
			Pool->AddToRoot();
			Pool->Concurrency.MaxCount = bFoley ? 12 : 24;
			Pool->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopFarthestThenOldest;
			Pool->Concurrency.bLimitToOwner = false;
		}
		return Pool;
	}

	// ---- Imported banks ----

	struct FBank
	{
		const TCHAR* Near2D;   // first-person / non-positional version
		int32 Near2DCount;
		const TCHAR* Near3D;   // positional (mono) version; null = same as Near2D
		int32 Near3DCount;
		const TCHAR* Far;      // distant version crossfaded in by listener distance; null = none
		int32 FarCount;
		EAtten Atten;
		float NearEnd;         // listener distance where the close layer has faded out
		float FarStart;        // listener distance where the distant layer is at full volume
		bool bFoley;
	};

	const FBank* GetBank(EShieldbreakSound Sound)
	{
		using S = EShieldbreakSound;
		switch (Sound)
		{
		case S::BattleRifleShot: { static const FBank B{ TEXT("BR_FP"), 8, TEXT("BR_TP"), 8, TEXT("BR_Dist"), 4, EAtten::Gun, 5000.f, 4200.f, false }; return &B; }
		case S::BattleRifleTail: { static const FBank B{ TEXT("BR_FPTail"), 3, TEXT("BR_TPTail"), 3, nullptr, 0, EAtten::Tail, 0.f, 0.f, false }; return &B; }
		case S::SidearmShot: { static const FBank B{ TEXT("Sidearm_FP"), 6, TEXT("Sidearm_TP"), 6, TEXT("Sidearm_Dist"), 4, EAtten::Gun, 4500.f, 3800.f, false }; return &B; }
		case S::SidearmTail: { static const FBank B{ TEXT("Sidearm_FPTail"), 3, TEXT("Sidearm_TPTail"), 3, nullptr, 0, EAtten::Tail, 0.f, 0.f, false }; return &B; }
		case S::SniperShot: { static const FBank B{ TEXT("Sniper_FP"), 5, TEXT("Sniper_TP"), 5, TEXT("Sniper_Dist"), 4, EAtten::Gun, 6000.f, 5000.f, false }; return &B; }
		case S::SniperTail: { static const FBank B{ TEXT("Sniper_FPTail"), 3, TEXT("Sniper_TPTail"), 3, nullptr, 0, EAtten::Tail, 0.f, 0.f, false }; return &B; }
		case S::SniperBolt: { static const FBank B{ TEXT("Sniper_Bolt"), 3, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::RocketShot: { static const FBank B{ TEXT("Rocket_FP"), 4, TEXT("Rocket_TP"), 4, TEXT("Rocket_Dist"), 4, EAtten::Gun, 6000.f, 5000.f, false }; return &B; }
		case S::RocketTail: { static const FBank B{ TEXT("Rocket_FPTail"), 3, TEXT("Rocket_TPTail"), 3, nullptr, 0, EAtten::Tail, 0.f, 0.f, false }; return &B; }
		case S::RailFire: { static const FBank B{ TEXT("Rail_FP"), 4, TEXT("Rail_TP"), 4, TEXT("Rail_Dist"), 4, EAtten::Gun, 6000.f, 5000.f, false }; return &B; }
		case S::RailTail: { static const FBank B{ TEXT("Rail_FPTail"), 3, TEXT("Rail_TPTail"), 3, nullptr, 0, EAtten::Tail, 0.f, 0.f, false }; return &B; }
		case S::RailCharge: { static const FBank B{ TEXT("Rail_Charge"), 1, nullptr, 0, nullptr, 0, EAtten::Shield, 0.f, 0.f, false }; return &B; }
		case S::GrenadeExplosion: { static const FBank B{ TEXT("Grenade_Explosion_FP"), 5, TEXT("Grenade_Explosion"), 5, TEXT("Grenade_Explosion_Dist"), 4, EAtten::Explosion, 7000.f, 5500.f, false }; return &B; }
		case S::GrenadeThrow: { static const FBank B{ TEXT("Grenade_Throw"), 4, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::GrenadeBounce: { static const FBank B{ TEXT("Grenade_Bounce"), 6, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::ReloadBattleRifle: { static const FBank B{ TEXT("BR_Reload"), 2, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::ReloadSidearm: { static const FBank B{ TEXT("Sidearm_Reload"), 2, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::ReloadSniper: { static const FBank B{ TEXT("Sniper_Reload"), 2, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::ReloadRocket: { static const FBank B{ TEXT("Rocket_Reload"), 2, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::DryFireLight: { static const FBank B{ TEXT("DryFire_Light"), 3, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::DryFireRifle: { static const FBank B{ TEXT("DryFire_Rifle"), 3, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::DryFireHeavy: { static const FBank B{ TEXT("DryFire_Heavy"), 3, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::DryFireEnergy: { static const FBank B{ TEXT("DryFire_Energy"), 2, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::WeaponRaise: { static const FBank B{ TEXT("Weapon_Raise"), 3, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::ShieldImpact: { static const FBank B{ TEXT("Shield_Impact"), 6, nullptr, 0, nullptr, 0, EAtten::Shield, 0.f, 0.f, false }; return &B; }
		case S::ShieldPop: { static const FBank B{ TEXT("Shield_Pop"), 3, nullptr, 0, nullptr, 0, EAtten::Shield, 0.f, 0.f, false }; return &B; }
		case S::ShieldRecharge: { static const FBank B{ TEXT("Shield_Recharge"), 1, nullptr, 0, nullptr, 0, EAtten::Shield, 0.f, 0.f, false }; return &B; }
		case S::ShieldLowWarning: { static const FBank B{ TEXT("Shield_LowWarn"), 2, nullptr, 0, nullptr, 0, EAtten::Shield, 0.f, 0.f, false }; return &B; }
		case S::ShieldLowBeep: { static const FBank B{ TEXT("Shield_Low"), 1, nullptr, 0, nullptr, 0, EAtten::Shield, 0.f, 0.f, false }; return &B; }
		case S::Footstep: { static const FBank B{ TEXT("Footstep"), 10, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		case S::Land: { static const FBank B{ TEXT("Land"), 4, nullptr, 0, nullptr, 0, EAtten::Foley, 0.f, 0.f, true }; return &B; }
		default: return nullptr;
		}
	}

	/** Procedural stand-in while a bank isn't imported (Count = silent). */
	EShieldbreakSound GetSynthFallback(EShieldbreakSound Sound)
	{
		using S = EShieldbreakSound;
		switch (Sound)
		{
		case S::SidearmShot: case S::SniperShot: case S::RocketShot: return S::BattleRifleShot;
		case S::BattleRifleTail: case S::SidearmTail: case S::SniperTail: case S::RocketTail: case S::RailTail:
		case S::SniperBolt: case S::ReloadBattleRifle: case S::ReloadSidearm: case S::ReloadSniper: case S::ReloadRocket:
		case S::DryFireLight: case S::DryFireRifle: case S::DryFireHeavy: case S::DryFireEnergy: case S::WeaponRaise:
		case S::ShieldImpact: case S::ShieldLowWarning: case S::Footstep: case S::Land:
			return S::Count;
		default: return Sound;
		}
	}

	struct FLoadedBank
	{
		TArray<USoundBase*> Variants;
		int32 Last = -1;
	};

	/** Loads SFX_<Name>_01..Count from /Game/Shieldbreak/Audio/SFX once; empty if not imported. */
	FLoadedBank& LoadBank(const TCHAR* Name, int32 Count)
	{
		static TMap<FString, FLoadedBank> Banks;
		const FString Key(Name);
		if (FLoadedBank* Found = Banks.Find(Key))
		{
			return *Found;
		}
		FLoadedBank& Bank = Banks.Add(Key);
		for (int32 Index = 1; Index <= Count; ++Index)
		{
			const FString Path = FString::Printf(TEXT("/Game/Shieldbreak/Audio/SFX/SFX_%s_%02d.SFX_%s_%02d"), Name, Index, Name, Index);
			if (USoundBase* Sound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)))
			{
				Sound->AddToRoot();
				Bank.Variants.Add(Sound);
			}
		}
		return Bank;
	}

	/** A random variation, never the one played last. */
	USoundBase* PickVariant(const TCHAR* Name, int32 Count)
	{
		if (!Name || Count <= 0)
		{
			return nullptr;
		}
		FLoadedBank& Bank = LoadBank(Name, Count);
		const int32 Num = Bank.Variants.Num();
		if (Num == 0)
		{
			return nullptr;
		}
		int32 Pick = FMath::RandRange(0, Num - 1);
		if (Num > 1 && Pick == Bank.Last)
		{
			Pick = (Pick + 1 + FMath::RandRange(0, Num - 2)) % Num;
		}
		Bank.Last = Pick;
		return Bank.Variants[Pick];
	}

	/** Small per-play jitter so repeated shots never sound copy-pasted. */
	void Jitter(float& Volume, float& Pitch)
	{
		Volume *= FMath::FRandRange(0.9f, 1.f);
		Pitch *= FMath::FRandRange(0.97f, 1.03f);
	}

	bool GetListener(const UWorld* World, FVector& OutLocation)
	{
		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		if (!Controller)
		{
			return false;
		}
		FVector Front, Right;
		Controller->GetAudioListenerPosition(OutLocation, Front, Right);
		return true;
	}

	USoundWaveProcedural* MakeWave(EShieldbreakSound Sound)
	{
		const TArray<int16>& PCM = GetPCM(Sound);
		if (PCM.Num() == 0)
		{
			return nullptr;
		}

		// A procedural wave plays its queue once, so every play gets a fresh wave sharing the cached PCM.
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
		Wave->SetSampleRate(SampleRate);
		Wave->NumChannels = 1;
		Wave->Duration = static_cast<float>(PCM.Num()) / SampleRate;
		Wave->SoundGroup = SOUNDGROUP_Default;
		Wave->bLooping = false;
		Wave->QueueAudio(reinterpret_cast<const uint8*>(PCM.GetData()), PCM.Num() * sizeof(int16));
		return Wave;
	}

	bool CanPlay(const UWorld* World)
	{
		return World && World->GetNetMode() != NM_DedicatedServer && World->bAllowAudioPlayback;
	}

	/** Procedural sources can idle on silence once their queue drains, so stop them explicitly. */
	void StopAfter(UWorld* World, UAudioComponent* Component, float Seconds)
	{
		if (!World || !Component)
		{
			return;
		}

		TWeakObjectPtr<UAudioComponent> WeakComponent(Component);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakComponent]()
		{
			if (UAudioComponent* Alive = WeakComponent.Get())
			{
				Alive->Stop();
			}
		}), Seconds + 0.05f, false);
	}
}

void FShieldbreakSFX::Preload()
{
	using namespace ShieldbreakSFXPrivate;
	for (int32 Index = 0; Index < static_cast<int32>(EShieldbreakSound::Count); ++Index)
	{
		if (const FBank* Bank = GetBank(static_cast<EShieldbreakSound>(Index)))
		{
			LoadBank(Bank->Near2D, Bank->Near2DCount);
			if (Bank->Near3D)
			{
				LoadBank(Bank->Near3D, Bank->Near3DCount);
			}
			if (Bank->Far)
			{
				LoadBank(Bank->Far, Bank->FarCount);
			}
		}
	}
}

FString FShieldbreakSFX::DescribeMissingBanks(int32& OutLoaded)
{
	using namespace ShieldbreakSFXPrivate;
	OutLoaded = 0;
	TArray<FString> Missing;
	auto CheckBank = [&](const TCHAR* Name, int32 Count)
	{
		if (!Name)
		{
			return;
		}
		const int32 Num = LoadBank(Name, Count).Variants.Num();
		OutLoaded += Num;
		if (Num < Count)
		{
			Missing.AddUnique(FString::Printf(TEXT("%s (%d/%d)"), Name, Num, Count));
		}
	};
	for (int32 Index = 0; Index < static_cast<int32>(EShieldbreakSound::Count); ++Index)
	{
		if (const FBank* Bank = GetBank(static_cast<EShieldbreakSound>(Index)))
		{
			CheckBank(Bank->Near2D, Bank->Near2DCount);
			CheckBank(Bank->Near3D, Bank->Near3DCount);
			CheckBank(Bank->Far, Bank->FarCount);
		}
	}
	return FString::Join(Missing, TEXT(", "));
}

UAudioComponent* FShieldbreakSFX::Play2D(const UObject* WorldContext, EShieldbreakSound Sound, float Volume, float Pitch)
{
	using namespace ShieldbreakSFXPrivate;

	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!CanPlay(World))
	{
		return nullptr;
	}

	if (const FBank* Bank = GetBank(Sound))
	{
		if (USoundBase* Imported = PickVariant(Bank->Near2D, Bank->Near2DCount))
		{
			Jitter(Volume, Pitch);
			return UGameplayStatics::SpawnSound2D(World, Imported, Volume, Pitch, 0.f, GetConcurrency(Bank->bFoley));
		}
	}

	const EShieldbreakSound Synth = GetSynthFallback(Sound);
	USoundWaveProcedural* Wave = Synth != EShieldbreakSound::Count ? MakeWave(Synth) : nullptr;
	UAudioComponent* Component = Wave ? UGameplayStatics::SpawnSound2D(World, Wave, Volume, Pitch) : nullptr;
	if (Component)
	{
		StopAfter(World, Component, GetDuration(Synth) / FMath::Max(Pitch, 0.1f));
	}
	return Component;
}

UAudioComponent* FShieldbreakSFX::PlayAt(const UObject* WorldContext, EShieldbreakSound Sound, const FVector& Location, float Volume, float Pitch)
{
	using namespace ShieldbreakSFXPrivate;

	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!CanPlay(World))
	{
		return nullptr;
	}

	if (const FBank* Bank = GetBank(Sound))
	{
		const TCHAR* NearName = Bank->Near3D ? Bank->Near3D : Bank->Near2D;
		const int32 NearCount = Bank->Near3D ? Bank->Near3DCount : Bank->Near2DCount;
		USoundBase* Near = PickVariant(NearName, NearCount);
		if (Near)
		{
			Jitter(Volume, Pitch);
			float NearWeight = 1.f;
			float FarWeight = 0.f;
			FVector Listener;
			if (Bank->Far && GetListener(World, Listener))
			{
				// Close layer fades out toward NearEnd; the distant layer fades in from 60% of FarStart.
				const float Distance = FVector::Dist(Listener, Location);
				NearWeight = 1.f - FMath::SmoothStep(Bank->NearEnd * 0.5f, Bank->NearEnd, Distance);
				FarWeight = FMath::SmoothStep(Bank->FarStart * 0.6f, Bank->FarStart, Distance);
			}

			UAudioComponent* Component = nullptr;
			if (NearWeight > 0.03f)
			{
				Component = UGameplayStatics::SpawnSoundAtLocation(World, Near, Location, FRotator::ZeroRotator, Volume * NearWeight, Pitch,
					0.f, GetAttenuation(Bank->Atten), GetConcurrency(Bank->bFoley));
			}
			if (FarWeight > 0.03f)
			{
				if (USoundBase* FarSound = PickVariant(Bank->Far, Bank->FarCount))
				{
					UAudioComponent* FarComponent = UGameplayStatics::SpawnSoundAtLocation(World, FarSound, Location, FRotator::ZeroRotator,
						Volume * FarWeight, Pitch, 0.f, GetAttenuation(EAtten::Far), GetConcurrency(false));
					Component = Component ? Component : FarComponent;
				}
			}
			return Component;
		}
	}

	const EShieldbreakSound Synth = GetSynthFallback(Sound);
	USoundWaveProcedural* Wave = Synth != EShieldbreakSound::Count ? MakeWave(Synth) : nullptr;
	UAudioComponent* Component = Wave
		? UGameplayStatics::SpawnSoundAtLocation(World, Wave, Location, FRotator::ZeroRotator, Volume, Pitch, 0.f, GetAttenuation())
		: nullptr;
	if (Component)
	{
		StopAfter(World, Component, GetDuration(Synth) / FMath::Max(Pitch, 0.1f));
	}
	return Component;
}

UAudioComponent* FShieldbreakSFX::PlayAttached(USceneComponent* AttachTo, EShieldbreakSound Sound, float Volume, float Pitch)
{
	using namespace ShieldbreakSFXPrivate;

	UWorld* World = AttachTo ? AttachTo->GetWorld() : nullptr;
	if (!CanPlay(World))
	{
		return nullptr;
	}
	const FBank* Bank = GetBank(Sound);
	USoundBase* Imported = Bank ? PickVariant(Bank->Near3D ? Bank->Near3D : Bank->Near2D, Bank->Near3D ? Bank->Near3DCount : Bank->Near2DCount) : nullptr;
	if (!Imported)
	{
		return PlayAt(AttachTo, Sound, AttachTo->GetComponentLocation(), Volume, Pitch);
	}
	Jitter(Volume, Pitch);
	return UGameplayStatics::SpawnSoundAttached(Imported, AttachTo, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
		true, Volume, Pitch, 0.f, GetAttenuation(Bank->Atten), GetConcurrency(Bank->bFoley));
}

void FShieldbreakSFX::PlayShot(const UObject* WorldContext, const FShieldbreakWeaponSounds& Sounds, const FVector& Muzzle, bool bLocalView)
{
	using namespace ShieldbreakSFXPrivate;

	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!CanPlay(World))
	{
		return;
	}

	if (bLocalView)
	{
		Play2D(WorldContext, Sounds.Shot, 0.8f);
		if (Sounds.Tail != EShieldbreakSound::Count)
		{
			Play2D(WorldContext, Sounds.Tail, 0.45f);
		}
	}
	else
	{
		PlayAt(WorldContext, Sounds.Shot, Muzzle, 1.f);
		if (Sounds.Tail != EShieldbreakSound::Count)
		{
			PlayAt(WorldContext, Sounds.Tail, Muzzle, 0.6f);
		}
	}

	if (Sounds.Cycle != EShieldbreakSound::Count)
	{
		TWeakObjectPtr<const UObject> WeakContext(WorldContext);
		const EShieldbreakSound Cycle = Sounds.Cycle;
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakContext, Cycle, Muzzle, bLocalView]()
		{
			if (const UObject* Context = WeakContext.Get())
			{
				if (bLocalView)
				{
					FShieldbreakSFX::Play2D(Context, Cycle, 0.55f);
				}
				else
				{
					FShieldbreakSFX::PlayAt(Context, Cycle, Muzzle, 0.7f);
				}
			}
		}), Sounds.CycleDelay, false);
	}
}

float FShieldbreakSFX::GetDuration(EShieldbreakSound Sound)
{
	return static_cast<float>(ShieldbreakSFXPrivate::GetPCM(Sound).Num()) / ShieldbreakSFXPrivate::SampleRate;
}
