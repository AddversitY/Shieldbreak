#include "Player/ShieldbreakSettings.h"

#include "Audio/ShieldbreakMusic.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "ShieldbreakSettings"

namespace ShieldbreakSettingsPrivate
{
	const TCHAR* const Section = TEXT("Shieldbreak.Settings");

	struct FValues
	{
		float Sensitivity = 1.f;
		float FieldOfView = ShieldbreakSettings::DefaultFieldOfView;
		bool bInvertLook = false;
		bool bShowTips = true;
		float MusicVolume = 0.6f;
		float MasterVolume = 1.f;
	};

	/** Loaded from the ini on first use, then kept in memory (look input reads it every frame). */
	FValues& Values()
	{
		static FValues Cached;
		static bool bLoaded = false;
		if (!bLoaded && GConfig)
		{
			bLoaded = true;
			GConfig->GetFloat(Section, TEXT("Sensitivity"), Cached.Sensitivity, GGameUserSettingsIni);
			GConfig->GetFloat(Section, TEXT("FieldOfView"), Cached.FieldOfView, GGameUserSettingsIni);
			GConfig->GetBool(Section, TEXT("InvertLook"), Cached.bInvertLook, GGameUserSettingsIni);
			GConfig->GetBool(Section, TEXT("ShowTips"), Cached.bShowTips, GGameUserSettingsIni);
			GConfig->GetFloat(Section, TEXT("MusicVolume"), Cached.MusicVolume, GGameUserSettingsIni);
			Cached.MusicVolume = FMath::Clamp(Cached.MusicVolume, 0.f, 1.f);
			GConfig->GetFloat(Section, TEXT("MasterVolume"), Cached.MasterVolume, GGameUserSettingsIni);
			Cached.MasterVolume = FMath::Clamp(Cached.MasterVolume, 0.f, 1.f);
			Cached.Sensitivity = FMath::Clamp(Cached.Sensitivity, ShieldbreakSettings::MinSensitivity, ShieldbreakSettings::MaxSensitivity);
			Cached.FieldOfView = FMath::Clamp(Cached.FieldOfView, ShieldbreakSettings::MinFieldOfView, ShieldbreakSettings::MaxFieldOfView);
		}
		return Cached;
	}

	void Save()
	{
		if (!GConfig)
		{
			return;
		}
		const FValues& V = Values();
		GConfig->SetFloat(Section, TEXT("Sensitivity"), V.Sensitivity, GGameUserSettingsIni);
		GConfig->SetFloat(Section, TEXT("FieldOfView"), V.FieldOfView, GGameUserSettingsIni);
		GConfig->SetBool(Section, TEXT("InvertLook"), V.bInvertLook, GGameUserSettingsIni);
		GConfig->SetBool(Section, TEXT("ShowTips"), V.bShowTips, GGameUserSettingsIni);
		GConfig->SetFloat(Section, TEXT("MusicVolume"), V.MusicVolume, GGameUserSettingsIni);
		GConfig->SetFloat(Section, TEXT("MasterVolume"), V.MasterVolume, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

float ShieldbreakSettings::GetSensitivity()
{
	return ShieldbreakSettingsPrivate::Values().Sensitivity;
}

void ShieldbreakSettings::SetSensitivity(float Value)
{
	ShieldbreakSettingsPrivate::Values().Sensitivity = FMath::Clamp(Value, MinSensitivity, MaxSensitivity);
	ShieldbreakSettingsPrivate::Save();
}

float ShieldbreakSettings::GetFieldOfView()
{
	return ShieldbreakSettingsPrivate::Values().FieldOfView;
}

void ShieldbreakSettings::SetFieldOfView(float Value)
{
	ShieldbreakSettingsPrivate::Values().FieldOfView = FMath::Clamp(Value, MinFieldOfView, MaxFieldOfView);
	ShieldbreakSettingsPrivate::Save();
}

bool ShieldbreakSettings::GetInvertLook()
{
	return ShieldbreakSettingsPrivate::Values().bInvertLook;
}

void ShieldbreakSettings::SetInvertLook(bool bInvert)
{
	ShieldbreakSettingsPrivate::Values().bInvertLook = bInvert;
	ShieldbreakSettingsPrivate::Save();
}

bool ShieldbreakSettings::GetShowTips()
{
	return ShieldbreakSettingsPrivate::Values().bShowTips;
}

void ShieldbreakSettings::SetShowTips(bool bShow)
{
	ShieldbreakSettingsPrivate::Values().bShowTips = bShow;
	ShieldbreakSettingsPrivate::Save();
}

float ShieldbreakSettings::GetMusicVolume()
{
	return ShieldbreakSettingsPrivate::Values().MusicVolume;
}

void ShieldbreakSettings::SetMusicVolume(float Volume)
{
	ShieldbreakSettingsPrivate::Values().MusicVolume = FMath::Clamp(Volume, 0.f, 1.f);
	ShieldbreakSettingsPrivate::Save();
	ShieldbreakMusic::RefreshVolume();
}

int32 ShieldbreakSettings::GetGraphicsQuality()
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return Settings ? Settings->GetOverallScalabilityLevel() : -1;
}

void ShieldbreakSettings::SetGraphicsQuality(int32 Level)
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	Settings->SetOverallScalabilityLevel(FMath::Clamp(Level, 0, 3));
	Settings->ApplySettings(false);
}

void ShieldbreakSettings::AutoDetectGraphics()
{
	// In the editor this would also change the editor viewport's settings.
	if (GIsEditor || !GEngine)
	{
		return;
	}
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (!Settings || Settings->GetLastGPUBenchmarkResult() >= 0.f)
	{
		return; // Already benchmarked on this machine (or the player picked settings).
	}

	// Lumen and virtual shadow maps at Epic settings run at a few FPS on integrated GPUs.
	Settings->RunHardwareBenchmark();
	Settings->ApplyHardwareBenchmarkResults();
	if (Settings->GetOverallScalabilityLevel() < 0)
	{
		const int32 Levels[] =
		{
			Settings->GetViewDistanceQuality(), Settings->GetAntiAliasingQuality(), Settings->GetShadowQuality(),
			Settings->GetGlobalIlluminationQuality(), Settings->GetReflectionQuality(), Settings->GetPostProcessingQuality(),
			Settings->GetTextureQuality(), Settings->GetVisualEffectQuality(), Settings->GetFoliageQuality(), Settings->GetShadingQuality(),
		};
		int32 Sum = 0;
		for (const int32 Level : Levels)
		{
			Sum += FMath::Clamp(Level, 0, 3);
		}
		Settings->SetOverallScalabilityLevel(Sum / static_cast<int32>(UE_ARRAY_COUNT(Levels)));
	}
	// The resolution slider handles performance separately; start sharp.
	Settings->SetResolutionScaleValueEx(MaxResolutionScale);
	Settings->ApplySettings(false);
	UE_LOG(LogTemp, Log, TEXT("Shieldbreak: graphics auto-detected: overall level %d (CPU %.0f, GPU %.0f)."),
		Settings->GetOverallScalabilityLevel(), Settings->GetLastCPUBenchmarkResult(), Settings->GetLastGPUBenchmarkResult());
}

FText ShieldbreakSettings::GetGraphicsQualityName(int32 Level)
{
	switch (Level)
	{
	case 0: return LOCTEXT("QualityLow", "Low");
	case 1: return LOCTEXT("QualityMedium", "Medium");
	case 2: return LOCTEXT("QualityHigh", "High");
	case 3: return LOCTEXT("QualityEpic", "Epic");
	default: return LOCTEXT("QualityCustom", "Custom");
	}
}

float ShieldbreakSettings::GetResolutionScale()
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return MaxResolutionScale;
	}
	float Normalized = 1.f, Value = MaxResolutionScale, Min = 0.f, Max = 0.f;
	Settings->GetResolutionScaleInformationEx(Normalized, Value, Min, Max);
	// An unset scale reads as 0 (seen on a packaged build's first launch); that renders at full resolution.
	return Value < MinResolutionScale ? MaxResolutionScale : FMath::Min(Value, MaxResolutionScale);
}

void ShieldbreakSettings::SetResolutionScale(float Percent)
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	Settings->SetResolutionScaleValueEx(FMath::Clamp(Percent, MinResolutionScale, MaxResolutionScale));
	Settings->ApplySettings(false);
}

int32 ShieldbreakSettings::GetWindowMode()
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return 0;
	}
	switch (Settings->GetFullscreenMode())
	{
	case EWindowMode::Fullscreen: return 0;
	case EWindowMode::WindowedFullscreen: return 1;
	default: return 2;
	}
}

void ShieldbreakSettings::SetWindowMode(int32 Mode)
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings || GIsEditor)
	{
		return;
	}
	Mode = (Mode % 3 + 3) % 3;
	const FIntPoint Desktop = Settings->GetDesktopResolution();
	if (Mode == 2)
	{
		// A window that fits on screen with its title bar.
		const FIntPoint Current = Settings->GetScreenResolution();
		if (Current.X >= Desktop.X || Current.Y >= Desktop.Y)
		{
			Settings->SetScreenResolution(Desktop.X > 1920 ? FIntPoint(1920, 1080) : FIntPoint(1280, 720));
		}
	}
	else
	{
		Settings->SetScreenResolution(Desktop);
	}
	Settings->SetFullscreenMode(Mode == 0 ? EWindowMode::Fullscreen : Mode == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
	Settings->ApplySettings(false);
}

FText ShieldbreakSettings::GetWindowModeName(int32 Mode)
{
	switch (Mode)
	{
	case 0: return LOCTEXT("Fullscreen", "Fullscreen");
	case 1: return LOCTEXT("Borderless", "Borderless");
	default: return LOCTEXT("Windowed", "Windowed");
	}
}

bool ShieldbreakSettings::GetVSync()
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return Settings && Settings->IsVSyncEnabled();
}

void ShieldbreakSettings::SetVSync(bool bEnabled)
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetVSyncEnabled(bEnabled);
		Settings->ApplySettings(false);
	}
}

float ShieldbreakSettings::GetFrameRateLimit()
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return Settings ? Settings->GetFrameRateLimit() : 0.f;
}

void ShieldbreakSettings::SetFrameRateLimit(float Fps)
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetFrameRateLimit(FMath::Max(0.f, Fps));
		Settings->ApplySettings(false);
	}
}

float ShieldbreakSettings::GetMasterVolume()
{
	return ShieldbreakSettingsPrivate::Values().MasterVolume;
}

void ShieldbreakSettings::SetMasterVolume(float Volume)
{
	ShieldbreakSettingsPrivate::Values().MasterVolume = FMath::Clamp(Volume, 0.f, 1.f);
	ShieldbreakSettingsPrivate::Save();
	ApplyAudio();
}

void ShieldbreakSettings::ApplyAudio()
{
	if (FAudioDeviceHandle Device = GEngine ? GEngine->GetMainAudioDevice() : FAudioDeviceHandle())
	{
		Device->SetTransientPrimaryVolume(GetMasterVolume());
	}
}

#undef LOCTEXT_NAMESPACE
