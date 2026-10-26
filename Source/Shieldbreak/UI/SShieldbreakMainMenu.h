#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"

class AShieldbreakPlayerController;
class SVerticalBox;
class SEditableTextBox;
class STextBlock;
class SBox;

/** Asset-free front end. Match choices are shown only after selecting Play. */
class SShieldbreakMainMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShieldbreakMainMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AShieldbreakPlayerController>, Controller)
		SLATE_ARGUMENT(bool, InMatch)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	static bool IsValidAddress(const FString& Address);
	void ShowConnectionError(const FString& Message);

private:
	void Home();
	void Play();
	/** 0: browse, 1: host, 2: practice. Selection is retained between screens. */
	void Multiplayer(int32 Intent);
	void Join();
	void Settings();
	void StartMatch(bool bListen);
	void Heading(const FString& Label);
	TSharedRef<STextBlock> Action(const FText& Label, TFunction<void()> Callback);
	void Note(const FText& Label);
	TWeakObjectPtr<AShieldbreakPlayerController> Controller;
	TSharedPtr<SVerticalBox> Actions;
	TSharedPtr<SEditableTextBox> AddressBox;
	FButtonStyle ButtonStyle;
	FButtonStyle BrowserButtonStyle;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SBox> ContentWidth;
	TSharedPtr<STextBlock> GraphicsText;
	FText QualityLabel() const;
	int32 MapIndex = 0;
	int32 ModeIndex = 0;
	int32 Page = 0;
	bool bInMatch = false;
};
