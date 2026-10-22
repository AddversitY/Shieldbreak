// Slate widgets for the front end, built in code so the project needs no UMG assets.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/ShieldbreakPlayerController.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;

/** Map, mode, bots and skill choosers; Play, Host LAN game, Join and Quit. Remembers the last choices. */
class SShieldbreakMainMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShieldbreakMainMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	TSharedRef<SWidget> MakeChooser(const FText& Label, TFunction<FText()> Value, TFunction<void(int32)> Step);
	FText GetMapDetails() const;
	FReply StartMatch(bool bListen);
	FReply Join();
	void LoadChoices();
	void SaveChoices() const;

	TWeakObjectPtr<APlayerController> Owner;
	TArray<FName> Maps;
	int32 MapIndex = 0;
	int32 ModeIndex = 0;
	int32 Bots = 7;
	int32 Skill = 1;
	FString Address = TEXT("127.0.0.1");
	TSharedPtr<SEditableTextBox> AddressBox;
};

/** Esc in a match: Resume, Leave match, Quit. Esc again resumes. */
class SShieldbreakPauseMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShieldbreakPauseMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AShieldbreakPlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TWeakObjectPtr<AShieldbreakPlayerController> Owner;
};

namespace ShieldbreakMenuStyle
{
	/** Big menu button with a fixed width, shared by both menus. */
	TSharedRef<SWidget> MakeButton(const FText& Label, TFunction<void()> OnClick, float Width = 320.f);
}
