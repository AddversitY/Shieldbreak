#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"

class AShieldbreakPlayerController;
class SVerticalBox;
class SEditableTextBox;
class STextBlock;

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
	void Join();
	void Settings();
	void StartMatch(bool bListen);
	void Heading(const FString& Label);
	TSharedRef<STextBlock> Action(const FText& Label, TFunction<void()> Callback);
	void Note(const FText& Label);
	FText MapLabel() const;
	FText ModeLabel() const;
	TWeakObjectPtr<AShieldbreakPlayerController> Controller;
	TSharedPtr<SVerticalBox> Actions;
	TSharedPtr<SEditableTextBox> AddressBox;
	FButtonStyle ButtonStyle;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> MapText;
	TSharedPtr<STextBlock> ModeText;
	TSharedPtr<STextBlock> GraphicsText;
	FText QualityLabel() const;
	int32 MapIndex = 0;
	int32 ModeIndex = 0;
	int32 Page = 0;
	bool bInMatch = false;
};
