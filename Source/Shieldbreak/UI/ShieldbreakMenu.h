// Front end: the main menu (pick map, mode and bots; play, host or join) and the in-game Esc menu. The menu runs
// in its own game mode on the same empty Entry map; UShieldbreakGameInstance picks it when the game starts with no
// match options, so test scripts and server/client launches go straight into a match as before.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "ShieldbreakMenu.generated.h"

class SShieldbreakMainMenu;
class SWidget;

namespace ShieldbreakMenu
{
	/** True when a launch with these URL options and command line should open the main menu instead of a match. */
	SHIELDBREAK_API bool ShouldShowMainMenu(const FString& Options, const TCHAR* CommandLine);

	/** URL options for a match, e.g. "Map=Keel?Mode=CTF?Bots=7?Skill=1" (plus "?listen" when hosting). */
	SHIELDBREAK_API FString BuildMatchOptions(FName MapId, const FString& Mode, int32 Bots, int32 Skill, bool bListen);

	/** Back to the main menu (disconnects from or closes the current match). */
	SHIELDBREAK_API void ReturnToMainMenu(const UObject* WorldContext);

	/** The four modes, in menu order, as the ?Mode= values the game mode accepts. */
	SHIELDBREAK_API const TArray<FString>& GetModes();
}

UCLASS()
class SHIELDBREAK_API UShieldbreakGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<AGameModeBase> OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass, const FString& MapName, const FString& Options, const FString& Portal) const override;
};

/** No arena, no pawns: just the menu. */
UCLASS()
class SHIELDBREAK_API AShieldbreakMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShieldbreakMenuGameMode();
};

UCLASS()
class SHIELDBREAK_API AShieldbreakMenuController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TSharedPtr<SWidget> MenuWidget;
	TSharedPtr<SWidget> ViewportWidget;
};
