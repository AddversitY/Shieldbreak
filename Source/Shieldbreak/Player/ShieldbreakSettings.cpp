#include "Player/ShieldbreakSettings.h"

#include "Audio/ShieldbreakMusic.h"
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
	return Value;
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

#undef LOCTEXT_NAMESPACE
