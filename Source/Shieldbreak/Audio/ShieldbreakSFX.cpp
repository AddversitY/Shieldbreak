#include "Audio/ShieldbreakSFX.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
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
			FSynth S(0.45f, 16);
			S.AddNoise(0.7f, 9000.f, 1200.f, 0.f, 0.07f);
			S.AddTone(0.7f, 1400.f, 180.f, 0.25f, 0.001f, 0.12f, 0.f, 0.25f);
			S.AddBuzz(0.2f, 90.f, 60.f, 0.f, 0.15f);
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

	USoundAttenuation* GetAttenuation()
	{
		static USoundAttenuation* Attenuation = nullptr;
		if (!Attenuation)
		{
			Attenuation = NewObject<USoundAttenuation>(GetTransientPackage());
			Attenuation->AddToRoot();
			FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
			Settings.bAttenuate = true;
			Settings.bSpatialize = true;
			Settings.AttenuationShape = EAttenuationShape::Sphere;
			Settings.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
			Settings.FalloffDistance = 7000.f;
			Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		}
		return Attenuation;
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

UAudioComponent* FShieldbreakSFX::Play2D(const UObject* WorldContext, EShieldbreakSound Sound, float Volume, float Pitch)
{
	using namespace ShieldbreakSFXPrivate;

	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!CanPlay(World))
	{
		return nullptr;
	}

	USoundWaveProcedural* Wave = MakeWave(Sound);
	UAudioComponent* Component = Wave ? UGameplayStatics::SpawnSound2D(World, Wave, Volume, Pitch) : nullptr;
	StopAfter(World, Component, GetDuration(Sound) / FMath::Max(Pitch, 0.1f));
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

	USoundWaveProcedural* Wave = MakeWave(Sound);
	UAudioComponent* Component = Wave
		? UGameplayStatics::SpawnSoundAtLocation(World, Wave, Location, FRotator::ZeroRotator, Volume, Pitch, 0.f, GetAttenuation())
		: nullptr;
	StopAfter(World, Component, GetDuration(Sound) / FMath::Max(Pitch, 0.1f));
	return Component;
}

float FShieldbreakSFX::GetDuration(EShieldbreakSound Sound)
{
	return static_cast<float>(ShieldbreakSFXPrivate::GetPCM(Sound).Num()) / ShieldbreakSFXPrivate::SampleRate;
}
