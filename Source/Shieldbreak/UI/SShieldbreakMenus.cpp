#include "UI/SShieldbreakMenus.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Player/ShieldbreakInputSet.h"
#include "Player/ShieldbreakSettings.h"
#include "Sound/SlateSound.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ShieldbreakMenus"

namespace ShieldbreakMenuPrivate
{
	const FLinearColor Accent(0.35f, 0.8f, 1.f, 1.f);
	const FLinearColor Muted(0.6f, 0.65f, 0.7f, 1.f);

	FSlateFontInfo Font(int32 Size, const ANSICHAR* Weight = "Regular")
	{
		return FCoreStyle::GetDefaultFontStyle(Weight, Size);
	}

	/** Lyra's menu click and hover sounds, loaded once and rooted; silent if the assets are missing. */
	FSlateSound MenuSound(bool bClick)
	{
		static USoundBase* Click = nullptr;
		static USoundBase* Hover = nullptr;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			Click = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Sounds/UI/ButtonClick.ButtonClick"));
			Hover = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Sounds/UI/ButtonHover.ButtonHover"));
			for (USoundBase* Sound : { Click, Hover })
			{
				if (Sound)
				{
					Sound->AddToRoot();
				}
			}
		}
		USoundBase* Sound = bClick ? Click : Hover;
		FSlateSound SlateSound;
		if (Sound)
		{
			SlateSound.SetResourceObject(Sound);
		}
		return SlateSound;
	}

	/** Settings and Controls sit above the main menu (Z 100). */
	void OpenOverlay(const TSharedRef<SWidget>& Overlay)
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->AddViewportWidgetContent(Overlay, 200);
			FSlateApplication::Get().SetAllUserFocus(Overlay, EFocusCause::SetDirectly);
		}
	}

	void CloseOverlay(const TSharedRef<SWidget>& Overlay, const TWeakPtr<SWidget>& ReturnFocus)
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(Overlay);
		}
		if (TSharedPtr<SWidget> Focus = ReturnFocus.Pin())
		{
			FSlateApplication::Get().SetAllUserFocus(Focus, EFocusCause::SetDirectly);
		}
	}

	TSharedRef<SWidget> BackdropImage(const FLinearColor& Color)
	{
		return SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(Color);
	}
}

TSharedRef<SWidget> ShieldbreakMenuStyle::MakeButton(const FText& Label, TFunction<void()> OnClick, float Width, int32 FontSize)
{
	using namespace ShieldbreakMenuPrivate;
	return SNew(SBox)
		.WidthOverride(Width)
		.Padding(FMargin(6.f))
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(12.f, 10.f))
			.PressedSoundOverride(MenuSound(true))
			.HoveredSoundOverride(MenuSound(false))
			.OnClicked_Lambda([OnClick]()
			{
				OnClick();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Label).Font(Font(FontSize, "Bold"))
			]
		];
}

TSharedRef<SWidget> ShieldbreakMenuStyle::MakeChooser(const FText& Label, TFunction<FText()> Value, TFunction<void(int32)> Step, float LabelWidth, float ValueWidth)
{
	using namespace ShieldbreakMenuPrivate;
	auto Arrow = [Step](const FText& Glyph, int32 Delta) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(14.f, 4.f))
			.PressedSoundOverride(MenuSound(true))
			.OnClicked_Lambda([Step, Delta]()
			{
				Step(Delta);
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Glyph).Font(Font(20, "Bold"))
			];
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(LabelWidth)
			[
				SNew(STextBlock).Text(Label).Font(Font(20)).ColorAndOpacity(Muted)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f)
		[
			Arrow(LOCTEXT("Prev", "<"), -1)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(ValueWidth).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text_Lambda([Value]() { return Value(); }).Font(Font(24, "Bold")).ColorAndOpacity(FLinearColor::White)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f)
		[
			Arrow(LOCTEXT("Next", ">"), 1)
		];
}

TSharedRef<SWidget> ShieldbreakMenuStyle::MakeGameplayRows()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChooser(LOCTEXT("Sensitivity", "Sensitivity"),
				[]()
				{
					static const FNumberFormattingOptions OneDecimal = FNumberFormattingOptions().SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
					return FText::AsNumber(ShieldbreakSettings::GetSensitivity(), &OneDecimal);
				},
				[](int32 Delta) { ShieldbreakSettings::SetSensitivity(FMath::RoundToFloat((ShieldbreakSettings::GetSensitivity() + 0.1f * Delta) * 10.f) / 10.f); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChooser(LOCTEXT("FieldOfView", "Field of view"),
				[]() { return FText::AsNumber(FMath::RoundToInt(ShieldbreakSettings::GetFieldOfView())); },
				[](int32 Delta) { ShieldbreakSettings::SetFieldOfView(ShieldbreakSettings::GetFieldOfView() + 5.f * Delta); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChooser(LOCTEXT("InvertLook", "Invert look"),
				[]() { return ShieldbreakSettings::GetInvertLook() ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off"); },
				[](int32) { ShieldbreakSettings::SetInvertLook(!ShieldbreakSettings::GetInvertLook()); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChooser(LOCTEXT("Tips", "New-player tips"),
				[]() { return ShieldbreakSettings::GetShowTips() ? LOCTEXT("TipsOn", "On") : LOCTEXT("TipsOff", "Off"); },
				[](int32) { ShieldbreakSettings::SetShowTips(!ShieldbreakSettings::GetShowTips()); })
		];
}

TSharedRef<SWidget> ShieldbreakMenuStyle::MakeVideoAudioRows()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChooser(LOCTEXT("Graphics", "Graphics"),
				[]() { return ShieldbreakSettings::GetGraphicsQualityName(ShieldbreakSettings::GetGraphicsQuality()); },
				[](int32 Delta)
				{
					const int32 Current = ShieldbreakSettings::GetGraphicsQuality();
					const int32 Next = Current < 0 ? (Delta > 0 ? 0 : 3) : (Current + Delta + 4) % 4;
					ShieldbreakSettings::SetGraphicsQuality(Next);
				})
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChooser(LOCTEXT("ResolutionScale", "Resolution"),
				[]() { return FText::Format(LOCTEXT("Percent", "{0}%"), FText::AsNumber(FMath::RoundToInt(ShieldbreakSettings::GetResolutionScale()))); },
				[](int32 Delta)
				{
					const float Current = FMath::RoundToFloat(ShieldbreakSettings::GetResolutionScale() / 10.f) * 10.f;
					ShieldbreakSettings::SetResolutionScale(Current + 10.f * Delta);
				})
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChooser(LOCTEXT("Display", "Display"),
				[]() { return ShieldbreakSettings::GetWindowModeName(ShieldbreakSettings::GetWindowMode()); },
				[](int32 Delta) { ShieldbreakSettings::SetWindowMode(ShieldbreakSettings::GetWindowMode() + Delta); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChooser(LOCTEXT("VSync", "VSync"),
				[]() { return ShieldbreakSettings::GetVSync() ? LOCTEXT("VSyncOn", "On") : LOCTEXT("VSyncOff", "Off"); },
				[](int32) { ShieldbreakSettings::SetVSync(!ShieldbreakSettings::GetVSync()); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChooser(LOCTEXT("FrameLimit", "Frame limit"),
				[]()
				{
					const int32 Fps = FMath::RoundToInt(ShieldbreakSettings::GetFrameRateLimit());
					return Fps <= 0 ? LOCTEXT("Unlimited", "None") : FText::AsNumber(Fps);
				},
				[](int32 Delta)
				{
					static const float Steps[] = { 30.f, 60.f, 120.f, 144.f, 0.f };
					constexpr int32 Count = UE_ARRAY_COUNT(Steps);
					const float Current = ShieldbreakSettings::GetFrameRateLimit();
					int32 Index = Count - 1;
					for (int32 Step = 0; Step < Count; ++Step)
					{
						if (FMath::IsNearlyEqual(Steps[Step], Current, 0.5f))
						{
							Index = Step;
						}
					}
					ShieldbreakSettings::SetFrameRateLimit(Steps[(Index + Delta + Count) % Count]);
				})
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChooser(LOCTEXT("MasterVolume", "Master volume"),
				[]() { return FText::AsPercent(ShieldbreakSettings::GetMasterVolume()); },
				[](int32 Delta) { ShieldbreakSettings::SetMasterVolume(FMath::RoundToFloat((ShieldbreakSettings::GetMasterVolume() + 0.1f * Delta) * 10.f) / 10.f); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeChooser(LOCTEXT("Music", "Music volume"),
				[]() { return FText::AsPercent(ShieldbreakSettings::GetMusicVolume()); },
				[](int32 Delta) { ShieldbreakSettings::SetMusicVolume(FMath::RoundToFloat((ShieldbreakSettings::GetMusicVolume() + 0.1f * Delta) * 10.f) / 10.f); })
		];
}

void SShieldbreakControlsMenu::Open(const TSharedPtr<SWidget>& ReturnFocus)
{
	TSharedRef<SShieldbreakControlsMenu> Menu = SNew(SShieldbreakControlsMenu);
	Menu->ReturnFocus = ReturnFocus;
	ShieldbreakMenuPrivate::OpenOverlay(Menu);
}

void SShieldbreakControlsMenu::Construct(const FArguments& InArgs)
{
	using namespace ShieldbreakMenuPrivate;

	const int32 Count = UShieldbreakInputSet::Get()->GetBindings().Num();
	const int32 Half = (Count + 1) / 2;
	TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
	TSharedRef<SVerticalBox> Right = SNew(SVerticalBox);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		(Index < Half ? Left : Right)->AddSlot().AutoHeight()[ MakeRow(Index) ];
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			BackdropImage(FLinearColor(0.f, 0.f, 0.f, 0.85f))
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(STextBlock).Text(LOCTEXT("ControlsTitle", "CONTROLS")).Font(Font(36, "Bold")).ColorAndOpacity(FLinearColor::White)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 14.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ControlsHint", "Click a control, then press the new key or mouse button. Esc cancels. Controller buttons are fixed."))
				.Font(Font(12))
				.ColorAndOpacity(Muted)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)[ Left ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)[ Right ]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 16.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					ShieldbreakMenuStyle::MakeButton(LOCTEXT("ResetKeys", "RESET DEFAULTS"), [this]()
					{
						Capturing = INDEX_NONE;
						UShieldbreakInputSet::Get()->ResetToDefaults();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					ShieldbreakMenuStyle::MakeButton(LOCTEXT("ControlsBack", "BACK"), [this]() { Close(); })
				]
			]
		]
	];
}

TSharedRef<SWidget> SShieldbreakControlsMenu::MakeRow(int32 Index)
{
	using namespace ShieldbreakMenuPrivate;
	const FShieldbreakKeyBinding& Binding = UShieldbreakInputSet::Get()->GetBindings()[Index];

	return SNew(SBox)
		.WidthOverride(400.f)
		.Padding(FMargin(0.f, 2.f))
		[
			SNew(SButton)
			.ContentPadding(FMargin(12.f, 5.f))
			.PressedSoundOverride(MenuSound(true))
			.HoveredSoundOverride(MenuSound(false))
			.OnClicked_Lambda([this, Index]()
			{
				Capturing = Index;
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Binding.Label).Font(Font(15))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(Font(15, "Bold"))
					.Text_Lambda([this, Index]()
					{
						if (Capturing == Index)
						{
							return LOCTEXT("PressKey", "Press a key...");
						}
						return FText::FromString(UShieldbreakInputSet::Get()->GetKeyName(UShieldbreakInputSet::Get()->GetBindings()[Index].Id));
					})
					.ColorAndOpacity_Lambda([this, Index]() { return FSlateColor(Capturing == Index ? FLinearColor(1.f, 0.8f, 0.3f) : Accent); })
				]
			]
		];
}

FReply SShieldbreakControlsMenu::Capture(const FKey& Key)
{
	const int32 Index = Capturing;
	Capturing = INDEX_NONE;
	if (UShieldbreakInputSet::CanBind(Key))
	{
		UShieldbreakInputSet::Get()->Rebind(Index, Key);
	}
	return FReply::Handled();
}

FReply SShieldbreakControlsMenu::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (Capturing == INDEX_NONE)
	{
		return FReply::Unhandled();
	}
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		Capturing = INDEX_NONE;
		return FReply::Handled();
	}
	if (Key.IsGamepadKey())
	{
		return FReply::Handled();
	}
	return Capture(Key);
}

FReply SShieldbreakControlsMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		Close();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SShieldbreakControlsMenu::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return Capturing == INDEX_NONE ? FReply::Unhandled() : Capture(MouseEvent.GetEffectingButton());
}

FReply SShieldbreakControlsMenu::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

FReply SShieldbreakControlsMenu::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (Capturing == INDEX_NONE || MouseEvent.GetWheelDelta() == 0.f)
	{
		return FReply::Unhandled();
	}
	return Capture(MouseEvent.GetWheelDelta() > 0.f ? EKeys::MouseScrollUp : EKeys::MouseScrollDown);
}

void SShieldbreakControlsMenu::Close()
{
	ShieldbreakMenuPrivate::CloseOverlay(AsShared(), ReturnFocus);
}

void SShieldbreakSettingsMenu::Open(const TSharedPtr<SWidget>& ReturnFocus)
{
	TSharedRef<SShieldbreakSettingsMenu> Menu = SNew(SShieldbreakSettingsMenu);
	Menu->ReturnFocus = ReturnFocus;
	ShieldbreakMenuPrivate::OpenOverlay(Menu);
}

void SShieldbreakSettingsMenu::Construct(const FArguments& InArgs)
{
	using namespace ShieldbreakMenuPrivate;

	auto Column = [](const FText& Heading, const TSharedRef<SWidget>& Rows) -> TSharedRef<SWidget>
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock).Text(Heading).Font(Font(16, "Bold")).ColorAndOpacity(Accent)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Rows
			];
	};

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			BackdropImage(FLinearColor(0.f, 0.f, 0.f, 0.85f))
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 16.f)
			[
				SNew(STextBlock).Text(LOCTEXT("SettingsTitle", "SETTINGS")).Font(Font(36, "Bold")).ColorAndOpacity(FLinearColor::White)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(16.f, 0.f)
				[
					Column(LOCTEXT("GameplayHeading", "GAMEPLAY"), ShieldbreakMenuStyle::MakeGameplayRows())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(16.f, 0.f)
				[
					Column(LOCTEXT("VideoAudioHeading", "VIDEO AND AUDIO"), ShieldbreakMenuStyle::MakeVideoAudioRows())
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 20.f, 0.f, 0.f)
			[
				ShieldbreakMenuStyle::MakeButton(LOCTEXT("SettingsBack", "BACK"), [this]() { Close(); })
			]
		]
	];
}

FReply SShieldbreakSettingsMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		Close();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SShieldbreakSettingsMenu::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

void SShieldbreakSettingsMenu::Close()
{
	ShieldbreakMenuPrivate::CloseOverlay(AsShared(), ReturnFocus);
}

#undef LOCTEXT_NAMESPACE
