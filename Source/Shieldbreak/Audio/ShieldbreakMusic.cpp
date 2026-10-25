#include "Audio/ShieldbreakMusic.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Player/ShieldbreakSettings.h"
#include "Sound/SoundBase.h"

namespace ShieldbreakMusicPrivate
{
	const TCHAR* const MenuMusicPath = TEXT("/Game/Shieldbreak/Art/Audio/MUS_SB_Menu.MUS_SB_Menu");

	TWeakObjectPtr<UAudioComponent> GPlaying;
	float GTrackVolume = 1.f;

	USoundBase* MenuMusic()
	{
		static TWeakObjectPtr<USoundBase> Cached;
		if (!Cached.IsValid())
		{
			Cached = LoadObject<USoundBase>(nullptr, MenuMusicPath);
		}
		return Cached.Get();
	}
}

void ShieldbreakMusic::PlayMenuMusic(const UObject* WorldContext, float Volume, float FadeIn)
{
	using namespace ShieldbreakMusicPrivate;

	// Automated runs stay quiet.
	if (FParse::Param(FCommandLine::Get(), TEXT("SBSelfTest")) || FParse::Param(FCommandLine::Get(), TEXT("nosound")))
	{
		return;
	}
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (UAudioComponent* Playing = GPlaying.Get(); Playing && Playing->GetWorld() == World && Playing->IsPlaying())
	{
		GTrackVolume = Volume;
		RefreshVolume();
		return;
	}
	USoundBase* Sound = MenuMusic();
	if (!Sound)
	{
		return;
	}
	GTrackVolume = Volume;
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(WorldContext, Sound, 1.f, 1.f, 0.f, nullptr, false, true);
	if (!Component)
	{
		return;
	}
	Component->bIsUISound = true;
	Component->FadeIn(FadeIn, GTrackVolume * ShieldbreakSettings::GetMusicVolume());
	GPlaying = Component;
}

void ShieldbreakMusic::Stop(float FadeOut)
{
	if (UAudioComponent* Playing = ShieldbreakMusicPrivate::GPlaying.Get())
	{
		Playing->FadeOut(FadeOut, 0.f);
	}
	ShieldbreakMusicPrivate::GPlaying.Reset();
}

void ShieldbreakMusic::RefreshVolume()
{
	if (UAudioComponent* Playing = ShieldbreakMusicPrivate::GPlaying.Get())
	{
		Playing->SetVolumeMultiplier(ShieldbreakMusicPrivate::GTrackVolume * ShieldbreakSettings::GetMusicVolume());
	}
}

bool ShieldbreakMusic::IsPlaying()
{
	const UAudioComponent* Playing = ShieldbreakMusicPrivate::GPlaying.Get();
	return Playing && Playing->IsPlaying();
}
