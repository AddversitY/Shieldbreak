#include "Player/ShieldbreakSettings.h"

#include "Misc/ConfigCacheIni.h"

namespace ShieldbreakSettingsPrivate
{
	const TCHAR* const Section = TEXT("Shieldbreak.Settings");

	struct FValues
	{
		float Sensitivity = 1.f;
		float FieldOfView = ShieldbreakSettings::DefaultFieldOfView;
		bool bInvertLook = false;
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
