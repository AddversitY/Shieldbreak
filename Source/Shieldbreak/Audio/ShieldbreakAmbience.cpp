#include "Audio/ShieldbreakAmbience.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"

namespace ShieldbreakAmbiencePrivate
{
	constexpr int32 Rate = 24000;
	using FPCM = TArray<int16>;

	/** One-pole low-pass, the building block of the wind and the water. */
	struct FLowPass
	{
		float State = 0.f;
		float Step(float In, float CutoffHz)
		{
			const float Coefficient = 1.f - FMath::Exp(-2.f * UE_PI * FMath::Max(CutoffHz, 1.f) / Rate);
			State += Coefficient * (In - State);
			return State;
		}
	};

	/** Renders Seconds of sound plus a short tail, then crossfades the tail into the start so the loop has no seam. */
	TSharedRef<FPCM> MakeLoop(float Seconds, float Peak, TFunctionRef<void(TArray<float>&)> Render)
	{
		const int32 Length = FMath::RoundToInt(Seconds * Rate);
		const int32 Fade = Rate / 2;
		TArray<float> Raw;
		Raw.SetNumZeroed(Length + Fade);
		Render(Raw);
		for (int32 Index = 0; Index < Fade; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / Fade;
			Raw[Index] = Raw[Index] * Alpha + Raw[Length + Index] * (1.f - Alpha);
		}
		Raw.SetNum(Length);

		float Max = KINDA_SMALL_NUMBER;
		for (const float Sample : Raw)
		{
			Max = FMath::Max(Max, FMath::Abs(Sample));
		}
		TSharedRef<FPCM> Out = MakeShared<FPCM>();
		Out->SetNumUninitialized(Length);
		for (int32 Index = 0; Index < Length; ++Index)
		{
			(*Out)[Index] = static_cast<int16>(FMath::Clamp(Raw[Index] / Max * Peak, -1.f, 1.f) * 32767.f);
		}
		return Out;
	}

	/** Wind: noise through a low-pass whose cutoff and level follow slow gusts, plus a faint whistle band. */
	TSharedRef<FPCM> Wind(int32 Seed, float Strength)
	{
		return MakeLoop(12.f, 0.8f, [Seed, Strength](TArray<float>& Out)
		{
			FRandomStream Noise(Seed);
			FLowPass Body, BandHigh, BandLow;
			const float PhaseA = Noise.FRandRange(0.f, 6.f), PhaseB = Noise.FRandRange(0.f, 6.f);
			for (int32 Index = 0; Index < Out.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / Rate;
				const float Gust = 0.5f + 0.32f * FMath::Sin(2.f * UE_PI * Time / 6.f + PhaseA) + 0.18f * FMath::Sin(2.f * UE_PI * Time / 2.3f + PhaseB);
				const float White = Noise.FRandRange(-1.f, 1.f);
				const float Low = Body.Step(White, 180.f + 650.f * Gust * Strength);
				const float Whistle = BandHigh.Step(White, 950.f) - BandLow.Step(White, 700.f);
				Out[Index] = (Low * (0.45f + 0.55f * Gust) + Whistle * 0.35f * Gust * Gust * Strength);
			}
		});
	}

	/** An enclosed ship or tower: a low mains-like hum and air handling. Whole-number frequencies over 8 s loop cleanly. */
	TSharedRef<FPCM> Hum(int32 Seed)
	{
		return MakeLoop(8.f, 0.6f, [Seed](TArray<float>& Out)
		{
			FRandomStream Noise(Seed);
			FLowPass Air;
			for (int32 Index = 0; Index < Out.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / Rate;
				const float Wobble = 0.85f + 0.15f * FMath::Sin(2.f * UE_PI * 0.25f * Time);
				const float Tone = FMath::Sin(2.f * UE_PI * 55.f * Time) + 0.45f * FMath::Sin(2.f * UE_PI * 110.f * Time)
					+ 0.15f * FMath::Sin(2.f * UE_PI * 165.f * Time);
				Out[Index] = 0.5f * Tone * Wobble + 1.6f * Air.Step(Noise.FRandRange(-1.f, 1.f), 260.f);
			}
		});
	}

	/** A hull vent fan: a soft motor whine and air pushed past six blades (a 3 Hz thrum). Whole-number rates over 4 s. */
	TSharedRef<FPCM> VentFan(int32 Seed)
	{
		return MakeLoop(4.f, 0.7f, [Seed](TArray<float>& Out)
		{
			FRandomStream Noise(Seed);
			FLowPass Air, AirLow;
			for (int32 Index = 0; Index < Out.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / Rate;
				const float Blades = 0.65f + 0.35f * FMath::Sin(2.f * UE_PI * 3.f * Time);
				const float White = Noise.FRandRange(-1.f, 1.f);
				const float Rush = Air.Step(White, 900.f) - 0.6f * AirLow.Step(White, 150.f);
				const float Whine = 0.12f * FMath::Sin(2.f * UE_PI * 180.f * Time) + 0.05f * FMath::Sin(2.f * UE_PI * 360.f * Time);
				Out[Index] = 1.4f * Rush * Blades + Whine;
			}
		});
	}

	/** The reactor ring: a deep two-tone drone that swells every 4 s with the pylons' light pulse. */
	TSharedRef<FPCM> ReactorDrone(int32 Seed)
	{
		return MakeLoop(8.f, 0.7f, [Seed](TArray<float>& Out)
		{
			FRandomStream Noise(Seed);
			FLowPass Rumble;
			for (int32 Index = 0; Index < Out.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / Rate;
				const float Swell = 0.7f + 0.3f * FMath::Sin(2.f * UE_PI * 0.25f * Time);
				const float Tone = FMath::Sin(2.f * UE_PI * 41.f * Time) + 0.6f * FMath::Sin(2.f * UE_PI * 82.f * Time)
					+ 0.2f * FMath::Sin(2.f * UE_PI * 123.f * Time) + 0.08f * FMath::Sin(2.f * UE_PI * 492.f * Time);
				Out[Index] = (0.55f * Tone + 1.2f * Rumble.Step(Noise.FRandRange(-1.f, 1.f), 120.f)) * Swell;
			}
		});
	}

	/** A shallow creek: a rushing bed of filtered noise and lots of tiny bubbles (fast rising, quickly decaying tones). */
	TSharedRef<FPCM> Creek(int32 Seed)
	{
		return MakeLoop(7.f, 0.75f, [Seed](TArray<float>& Out)
		{
			FRandomStream Noise(Seed);
			FLowPass Rush, RushLow;
			for (int32 Index = 0; Index < Out.Num(); ++Index)
			{
				const float White = Noise.FRandRange(-1.f, 1.f);
				Out[Index] = 0.5f * (Rush.Step(White, 2200.f) - RushLow.Step(White, 350.f));
			}
			const int32 Bubbles = FMath::RoundToInt(70.f * Out.Num() / Rate);
			for (int32 Bubble = 0; Bubble < Bubbles; ++Bubble)
			{
				const int32 Start = Noise.RandRange(0, Out.Num() - 1);
				const float Hz = Noise.FRandRange(450.f, 1700.f);
				const float Decay = Noise.FRandRange(0.008f, 0.03f);
				const float Gain = Noise.FRandRange(0.05f, 0.22f);
				const int32 Span = FMath::Min(FMath::RoundToInt(Decay * 5.f * Rate), Out.Num() - Start);
				float Phase = 0.f;
				for (int32 Offset = 0; Offset < Span; ++Offset)
				{
					const float Local = static_cast<float>(Offset) / Rate;
					Phase += 2.f * UE_PI * Hz * (1.f + 6.f * Local) / Rate;
					Out[Start + Offset] += Gain * FMath::Sin(Phase) * FMath::Exp(-Local / Decay);
				}
			}
		});
	}

	/** A short bird call: a few quick rising whistles with a little vibrato. Not looped. */
	FPCM BirdCall(int32 Seed)
	{
		FRandomStream Noise(Seed);
		const int32 Notes = Noise.RandRange(2, 5);
		const float Base = Noise.FRandRange(2300.f, 4200.f);
		const float NoteLength = Noise.FRandRange(0.06f, 0.13f);
		const float Gap = Noise.FRandRange(0.04f, 0.09f);
		TArray<float> Raw;
		Raw.SetNumZeroed(FMath::RoundToInt((Notes * (NoteLength + Gap) + 0.05f) * Rate));
		for (int32 Note = 0; Note < Notes; ++Note)
		{
			const int32 Start = FMath::RoundToInt(Note * (NoteLength + Gap) * Rate);
			const float From = Base * Noise.FRandRange(0.85f, 1.1f), To = From * Noise.FRandRange(1.1f, 1.45f);
			float Phase = 0.f;
			const int32 Span = FMath::Min(FMath::RoundToInt(NoteLength * Rate), Raw.Num() - Start);
			for (int32 Offset = 0; Offset < Span; ++Offset)
			{
				const float Alpha = static_cast<float>(Offset) / Span;
				const float Hz = FMath::Lerp(From, To, Alpha) * (1.f + 0.03f * FMath::Sin(2.f * UE_PI * 28.f * Offset / Rate));
				Phase += 2.f * UE_PI * Hz / Rate;
				Raw[Start + Offset] += FMath::Sin(Phase) * FMath::Sin(UE_PI * Alpha);
			}
		}
		FPCM Out;
		Out.SetNumUninitialized(Raw.Num());
		for (int32 Index = 0; Index < Raw.Num(); ++Index)
		{
			Out[Index] = static_cast<int16>(FMath::Clamp(Raw[Index] * 0.7f, -1.f, 1.f) * 32767.f);
		}
		return Out;
	}

	/** A procedural wave that queues the same loop again every time it runs dry, so it plays until stopped. */
	USoundWaveProcedural* LoopingWave(const TSharedRef<FPCM>& PCM)
	{
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
		Wave->SetSampleRate(Rate);
		Wave->NumChannels = 1;
		Wave->Duration = INDEFINITELY_LOOPING_DURATION;
		Wave->SoundGroup = SOUNDGROUP_Default;
		Wave->bLooping = false;
		const uint8* Bytes = reinterpret_cast<const uint8*>(PCM->GetData());
		const int32 Size = PCM->Num() * sizeof(int16);
		Wave->QueueAudio(Bytes, Size);
		Wave->QueueAudio(Bytes, Size);
		Wave->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([PCM](USoundWaveProcedural* Dry, int32)
		{
			Dry->QueueAudio(reinterpret_cast<const uint8*>(PCM->GetData()), PCM->Num() * sizeof(int16));
		});
		return Wave;
	}

	USoundAttenuation* Falloff(float Inner, float Distance)
	{
		USoundAttenuation* Attenuation = NewObject<USoundAttenuation>(GetTransientPackage());
		FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
		Settings.bAttenuate = true;
		Settings.bSpatialize = true;
		Settings.AttenuationShape = EAttenuationShape::Sphere;
		Settings.AttenuationShapeExtents = FVector(Inner, 0.f, 0.f);
		Settings.FalloffDistance = Distance;
		Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		return Attenuation;
	}

	UAudioComponent* AddLoop(AActor* Arena, const TSharedRef<FPCM>& PCM, float Volume, const FVector* WorldLocation, USoundAttenuation* Attenuation)
	{
		UAudioComponent* Component = NewObject<UAudioComponent>(Arena);
		Component->bAutoActivate = false;
		Component->bAutoDestroy = false;
		Component->SetupAttachment(Arena->GetRootComponent());
		Component->RegisterComponent();
		if (WorldLocation)
		{
			Component->SetWorldLocation(*WorldLocation);
			Component->AttenuationSettings = Attenuation;
			Component->bAllowSpatialization = true;
		}
		else
		{
			Component->bAllowSpatialization = false;
		}
		Component->SetSound(LoopingWave(PCM));
		Component->SetVolumeMultiplier(Volume);
		Component->FadeIn(2.f, Volume);
		return Component;
	}

	/** Every few seconds, a bird calls from somewhere above the canyon rim. Stops by itself once the arena is gone. */
	void ScheduleBirds(TWeakObjectPtr<AActor> WeakArena, FRandomStream Random, float Volume)
	{
		AActor* Arena = WeakArena.Get();
		UWorld* World = Arena ? Arena->GetWorld() : nullptr;
		if (!World)
		{
			return;
		}
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakArena, Random, Volume]() mutable
		{
			AActor* Alive = WeakArena.Get();
			UWorld* Where = Alive ? Alive->GetWorld() : nullptr;
			if (!Where)
			{
				return;
			}
			static TArray<FPCM> Calls;
			if (Calls.Num() == 0)
			{
				for (int32 Variant = 0; Variant < 6; ++Variant)
				{
					Calls.Add(BirdCall(9100 + Variant));
				}
			}
			static USoundAttenuation* Attenuation = nullptr;
			if (!Attenuation)
			{
				Attenuation = Falloff(800.f, 9000.f);
				Attenuation->AddToRoot();
			}
			const FPCM& Call = Calls[Random.RandRange(0, Calls.Num() - 1)];
			USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
			Wave->SetSampleRate(Rate);
			Wave->NumChannels = 1;
			Wave->Duration = static_cast<float>(Call.Num()) / Rate;
			Wave->bLooping = false;
			Wave->QueueAudio(reinterpret_cast<const uint8*>(Call.GetData()), Call.Num() * sizeof(int16));
			const float Angle = Random.FRandRange(0.f, 2.f * UE_PI);
			const float Radius = Random.FRandRange(3000.f, 5000.f);
			const FVector Location = Alive->GetActorLocation() + FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), Random.FRandRange(900.f, 1800.f));
			if (UAudioComponent* Played = UGameplayStatics::SpawnSoundAtLocation(Where, Wave, Location, FRotator::ZeroRotator,
				Volume * Random.FRandRange(0.6f, 1.f), Random.FRandRange(0.9f, 1.12f), 0.f, Attenuation))
			{
				// Procedural sources idle on silence once their queue drains, so stop them explicitly.
				TWeakObjectPtr<UAudioComponent> WeakPlayed(Played);
				FTimerHandle StopHandle;
				Where->GetTimerManager().SetTimer(StopHandle, FTimerDelegate::CreateLambda([WeakPlayed]()
				{
					if (UAudioComponent* Still = WeakPlayed.Get())
					{
						Still->Stop();
					}
				}), Wave->Duration + 0.2f, false);
			}
			ScheduleBirds(WeakArena, Random, Volume);
		}), Random.FRandRange(4.f, 13.f), false);
	}
}

void ShieldbreakAmbience::Start(AActor* Arena, FName MapId, bool bIndoor, const TArray<FBox>& WaterSheets)
{
	using namespace ShieldbreakAmbiencePrivate;

	UWorld* World = Arena ? Arena->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer || !World->bAllowAudioPlayback
		|| FParse::Param(FCommandLine::Get(), TEXT("nosound")) || FParse::Param(FCommandLine::Get(), TEXT("SBNoAmbience")))
	{
		return;
	}

	static const FName BastionId(TEXT("Bastion")), CreekbedId(TEXT("Creekbed")), SanctumId(TEXT("Sanctum"));
	const uint32 Seed = GetTypeHash(MapId.ToString());
	int32 Sources = 0;
	if (bIndoor)
	{
		AddLoop(Arena, Hum(static_cast<int32>(Seed)), 0.22f, nullptr, nullptr);
		++Sources;
		if (MapId == FName(TEXT("Keel")))
		{
			// Placed sources that match Keel's set pieces (ShieldbreakArena::BuildMapArt): the six Galleries hull fans
			// and the reactor pylon ring round the Camo. Arena-local positions.
			const FTransform& ToWorld = Arena->GetActorTransform();
			USoundAttenuation* FanFalloff = Falloff(150.f, 1600.f);
			const TSharedRef<FPCM> Fan = VentFan(static_cast<int32>(Seed) ^ 0xFA4);
			for (const double X : { -3400.0, -2600.0, -1000.0, 1000.0, 2600.0, 3400.0 })
			{
				const FVector Location = ToWorld.TransformPosition(FVector(X, -3540.0, 850.0));
				AddLoop(Arena, Fan, 0.35f, &Location, FanFalloff);
				++Sources;
			}
			const FVector Reactor = ToWorld.TransformPosition(FVector(0.0, -2800.0, 300.0));
			AddLoop(Arena, ReactorDrone(static_cast<int32>(Seed) ^ 0x2EAC), 0.45f, &Reactor, Falloff(400.f, 3200.f));
			++Sources;
		}
	}
	else
	{
		// Bastion's snowfield gets a real wind; the green maps a breeze under the water and the birds.
		const bool bSnow = MapId == BastionId;
		AddLoop(Arena, Wind(static_cast<int32>(Seed), bSnow ? 1.f : 0.45f), bSnow ? 0.32f : 0.14f, nullptr, nullptr);
		++Sources;

		if (WaterSheets.Num() > 0)
		{
			USoundAttenuation* Attenuation = Falloff(350.f, 2600.f);
			const TSharedRef<FPCM> Water = Creek(static_cast<int32>(Seed) ^ 0x51);
			const FTransform& ToWorld = Arena->GetActorTransform();
			for (const FBox& Sheet : WaterSheets)
			{
				// A source every 15 m or so along the sheet's long side, just above the surface.
				const FVector Size = Sheet.GetSize();
				const bool bAlongY = Size.Y >= Size.X;
				const double Long = bAlongY ? Size.Y : Size.X;
				const int32 Count = FMath::Clamp(FMath::RoundToInt(Long / 1500.0), 1, 6);
				for (int32 Spot = 0; Spot < Count; ++Spot)
				{
					const double Along = (Spot + 0.5) / Count;
					FVector Local = Sheet.GetCenter();
					Local.Z = Sheet.Max.Z + 40.0;
					if (bAlongY)
					{
						Local.Y = FMath::Lerp(Sheet.Min.Y, Sheet.Max.Y, Along);
					}
					else
					{
						Local.X = FMath::Lerp(Sheet.Min.X, Sheet.Max.X, Along);
					}
					const FVector Location = ToWorld.TransformPosition(Local);
					AddLoop(Arena, Water, 0.55f, &Location, Attenuation);
					++Sources;
				}
			}
		}

		if (MapId == CreekbedId || MapId == SanctumId)
		{
			ScheduleBirds(Arena, FRandomStream(static_cast<int32>(Seed) ^ 0xB1D), MapId == CreekbedId ? 0.45f : 0.3f);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Ambience for %s: %d looping sources%s."), *MapId.ToString(), Sources,
		(MapId == CreekbedId || MapId == SanctumId) ? TEXT(" and birds") : TEXT(""));
}
