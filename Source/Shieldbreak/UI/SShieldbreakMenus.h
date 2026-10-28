// Settings and key-rebinding screens. They open over SShieldbreakMainMenu.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Key rebinding (UShieldbreakInputSet): click a control, press a key. Opens over the main menu. */
class SShieldbreakControlsMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShieldbreakControlsMenu) {}
	SLATE_END_ARGS()

	/** Adds the menu to the game viewport above the other menus; focus returns to ReturnFocus on close. */
	static void Open(const TSharedPtr<SWidget>& ReturnFocus);

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	TSharedRef<SWidget> MakeRow(int32 Index);
	FReply Capture(const FKey& Key);
	void Close();

	/** The control waiting for a key, or INDEX_NONE. */
	int32 Capturing = INDEX_NONE;
	TWeakPtr<SWidget> ReturnFocus;
};

/** Gameplay, video and audio settings in two columns. Opens over the main menu. */
class SShieldbreakSettingsMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShieldbreakSettingsMenu) {}
	SLATE_END_ARGS()

	static void Open(const TSharedPtr<SWidget>& ReturnFocus);

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	void Close();

	TWeakPtr<SWidget> ReturnFocus;
};

namespace ShieldbreakMenuStyle
{
	/** Big menu button with a fixed width. */
	TSharedRef<SWidget> MakeButton(const FText& Label, TFunction<void()> OnClick, float Width = 320.f, int32 FontSize = 20);
	/** "Label   <  value  >" row; Step gets -1 or +1. */
	TSharedRef<SWidget> MakeChooser(const FText& Label, TFunction<FText()> Value, TFunction<void(int32)> Step, float LabelWidth = 200.f, float ValueWidth = 190.f);
	/** Sensitivity, field of view, invert look and tips (ShieldbreakSettings). */
	TSharedRef<SWidget> MakeGameplayRows();
	/** Graphics, resolution, display mode, VSync, frame limit, master and music volume. */
	TSharedRef<SWidget> MakeVideoAudioRows();
}
