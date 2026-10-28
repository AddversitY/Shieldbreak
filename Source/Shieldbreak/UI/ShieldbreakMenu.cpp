#include "UI/ShieldbreakMenu.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool ShieldbreakMenu::ShouldShowMainMenu(const FString& Options, const TCHAR* CommandLine)
{
	// Dedicated servers never show a menu, even if the URL also says ?Menu.
	if (IsRunningDedicatedServer())
	{
		return false;
	}
	// ?Menu is how Leave Match and connection recovery return to the front end.
	// It wins over -SBSelfTest in this helper so the self-test can assert that case;
	// the real self-test launch does not pass ?Menu, so it still enters a match.
	if (UGameplayStatics::HasOption(Options, TEXT("Menu")))
	{
		return true;
	}
	// Map, mode or listen means a script or the menu itself asked for a match.
	// ?Bots= alone does not; that stays on the front end.
	for (const TCHAR* Key : { TEXT("Map"), TEXT("Mode"), TEXT("listen") })
	{
		if (UGameplayStatics::HasOption(Options, Key))
		{
			return false;
		}
	}
	// Self-tests and play-test screenshots expect to land in a match.
	FString Unused;
	if (FParse::Param(CommandLine, TEXT("SBSelfTest")) || FParse::Param(CommandLine, TEXT("SBNoMenu"))
		|| FParse::Value(CommandLine, TEXT("SBAutoShot="), Unused))
	{
		return false;
	}
	return true;
}

FString ShieldbreakMenu::BuildMatchOptions(FName MapId, const FString& Mode, int32 Bots, int32 Skill, bool bListen)
{
	FString Options = FString::Printf(TEXT("Map=%s?Mode=%s?Bots=%d?Skill=%d"), *MapId.ToString(), *Mode, Bots, Skill);
	if (bListen)
	{
		Options += TEXT("?listen");
	}
	return Options;
}

void ShieldbreakMenu::ReturnToMainMenu(const UObject* WorldContext)
{
	UGameplayStatics::OpenLevel(WorldContext, FName(TEXT("/Engine/Maps/Entry")), true, TEXT("Menu=1"));
}

const TArray<FString>& ShieldbreakMenu::GetModes()
{
	static const TArray<FString> Modes = { TEXT("Slayer"), TEXT("CTF"), TEXT("KOTH"), TEXT("Oddball") };
	return Modes;
}
