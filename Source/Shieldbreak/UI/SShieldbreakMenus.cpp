#include "UI/SShieldbreakMenus.h"

#include "Game/ShieldbreakGameMode.h"
#include "Game/ShieldbreakMaps.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Player/ShieldbreakInputSet.h"
#include "Player/ShieldbreakPlayerController.h"
#include "Player/ShieldbreakSettings.h"
#include "Styling/CoreStyle.h"
#include "UI/ShieldbreakMenu.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "ShieldbreakMenus"

namespace ShieldbreakMenuPrivate
{
	const TCHAR* const ConfigSection = TEXT("Shieldbreak.Menu");
	const FLinearColor Backdrop(0.015f, 0.02f, 0.03f, 1.f);
	const FLinearColor Dim(0.f, 0.f, 0.f, 0.65f);
	const FLinearColor Accent(0.35f, 0.8f, 1.f, 1.f);
	const FLinearColor Muted(0.6f, 0.65f, 0.7f, 1.f);

	FSlateFontInfo Font(int32 Size, const ANSICHAR* Weight = "Regular")
	{
		return FCoreStyle::GetDefaultFontStyle(Weight, Size);
	}

	/** ProjectVersion from DefaultGame.ini, e.g. "0.2.0". */
	FString GameVersion()
	{
		FString Version;
		GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
		Version.RemoveFromEnd(TEXT(".0"));
		return Version.IsEmpty() ? FString(TEXT("dev")) : TEXT("v") + Version;
	}

	const TCHAR* SkillName(int32 Skill)
	{
		static const TCHAR* const Names[] = { TEXT("Easy"), TEXT("Normal"), TEXT("Heroic"), TEXT("Legendary") };
		return Names[FMath::Clamp(Skill, 0, static_cast<int32>(UE_ARRAY_COUNT(Names)) - 1)];
	}

	/** The modes each map was laid out for (docs/DESIGN.md, section 6). */
	FString BestModes(FName MapId)
	{
		static const TMap<FName, FString> Table =
		{
			{ TEXT("Keel"), TEXT("CTF, Slayer") },
			{ TEXT("Bastion"), TEXT("KOTH, Oddball, Slayer") },
			{ TEXT("Sanctum"), TEXT("CTF, Slayer") },
			{ TEXT("Creekbed"), TEXT("Slayer, CTF") },
			{ TEXT("Spire"), TEXT("CTF, KOTH") },
			{ TEXT("Crucible"), TEXT("test map") },
		};
		const FString* Found = Table.Find(MapId);
		return Found ? *Found : FString();
	}

	TSharedRef<SWidget> Backdrop_Image(const FLinearColor& Color)
	{
		return SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(Color);
	}
}

TSharedRef<SWidget> ShieldbreakMenuStyle::MakeButton(const FText& Label, TFunction<void()> OnClick, float Width)
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
			.OnClicked_Lambda([OnClick]()
			{
				OnClick();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Label).Font(Font(20, "Bold"))
			]
		];
}

TSharedRef<SWidget> ShieldbreakMenuStyle::MakeSettingsRows()
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
			MakeChooser(LOCTEXT("Graphics", "Graphics"),
				[]() { return ShieldbreakSettings::GetGraphicsQualityName(ShieldbreakSettings::GetGraphicsQuality()); },
				[](int32 Delta)
				{
					// From "Custom" (-1), the first step lands on Low or Epic.
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
					// Snap to steps of 10 (the graphics presets can leave values like 71 or 87).
					const float Current = FMath::RoundToFloat(ShieldbreakSettings::GetResolutionScale() / 10.f) * 10.f;
					ShieldbreakSettings::SetResolutionScale(Current + 10.f * Delta);
				})
		];
}

// ---- Main menu ----

void SShieldbreakMainMenu::Construct(const FArguments& InArgs)
{
	using namespace ShieldbreakMenuPrivate;
	Owner = InArgs._Owner;
	Maps = ShieldbreakMaps::GetMapIds();
	LoadChoices();

	const TArray<FString>& Modes = ShieldbreakMenu::GetModes();

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			Backdrop_Image(Backdrop)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("Title", "SHIELDBREAK")).Font(Font(64, "Bold")).ColorAndOpacity(FLinearColor::White)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 28.f)
			[
				SNew(STextBlock).Text(LOCTEXT("Subtitle", "Arena shooter. Shields up.")).Font(Font(16)).ColorAndOpacity(Accent)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				ShieldbreakMenuStyle::MakeChooser(LOCTEXT("Map", "Map"),
					[this]() { return Maps.IsValidIndex(MapIndex) ? FText::FromName(Maps[MapIndex]) : FText::GetEmpty(); },
					[this](int32 Delta) { MapIndex = Maps.Num() > 0 ? (MapIndex + Delta + Maps.Num()) % Maps.Num() : 0; })
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock).Text(this, &SShieldbreakMainMenu::GetMapDetails).Font(Font(13)).ColorAndOpacity(Muted)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				ShieldbreakMenuStyle::MakeChooser(LOCTEXT("Mode", "Mode"),
					[this, &Modes]() { return FText::FromString(Modes[ModeIndex]); },
					[this, &Modes](int32 Delta) { ModeIndex = (ModeIndex + Delta + Modes.Num()) % Modes.Num(); })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				ShieldbreakMenuStyle::MakeChooser(LOCTEXT("Bots", "Bots"),
					[this]() { return FText::AsNumber(Bots); },
					[this](int32 Delta) { Bots = FMath::Clamp(Bots + Delta, 0, AShieldbreakGameMode::GetMaxBots()); })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				ShieldbreakMenuStyle::MakeChooser(LOCTEXT("Skill", "Bot skill"),
					[this]() { return FText::FromString(SkillName(Skill)); },
					[this](int32 Delta) { Skill = FMath::Clamp(Skill + Delta, 0, AShieldbreakGameMode::GetMaxBotSkill()); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
			[
				ShieldbreakMenuStyle::MakeSettingsRows()
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 22.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					ShieldbreakMenuStyle::MakeButton(LOCTEXT("Play", "PLAY"), [this]() { StartMatch(false); })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					ShieldbreakMenuStyle::MakeButton(LOCTEXT("Host", "HOST LAN GAME"), [this]() { StartMatch(true); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f)
				[
					SNew(SBox)
					.WidthOverride(320.f)
					[
						SAssignNew(AddressBox, SEditableTextBox)
						.Text(FText::FromString(Address))
						.HintText(LOCTEXT("AddressHint", "Host IP address"))
						.Font(Font(18))
						.OnTextChanged_Lambda([this](const FText& NewText) { Address = NewText.ToString().TrimStartAndEnd(); })
						.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type CommitType)
						{
							if (CommitType == ETextCommit::OnEnter)
							{
								Join();
							}
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					ShieldbreakMenuStyle::MakeButton(LOCTEXT("Join", "JOIN"), [this]() { Join(); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					ShieldbreakMenuStyle::MakeButton(LOCTEXT("Controls", "CONTROLS"), [this]() { SShieldbreakControlsMenu::Open(AsShared()); })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					ShieldbreakMenuStyle::MakeButton(LOCTEXT("Quit", "QUIT"), [this]()
					{
						if (APlayerController* PC = Owner.Get())
						{
							UKismetSystemLibrary::QuitGame(PC, PC, EQuitPreference::Quit, false);
						}
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 24.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Hints", "In a match: F1 controls, Tab scoreboard, Esc menu"))
				.Font(Font(12))
				.ColorAndOpacity(Muted)
			]
			// Credits: the Unreal Engine EULA asks for the engine notice in a game's credits, and the Lyra content
			// (Fab Standard License, UE-only) is Epic's. See docs/ASSET_CREDITS.md and docs/RELEASE.md.
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 18.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("Credits", "Shieldbreak {0}  -  Original maps, weapons and armor by the Shieldbreak team\nMannequin, animations, effects and sounds from Lyra Starter Game by Epic Games\nUnreal Engine, Copyright 1998-2026, Epic Games, Inc. All rights reserved."),
					FText::FromString(ShieldbreakMenuPrivate::GameVersion())))
				.Font(Font(10))
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(Muted)
			]
		]
	];
}

TSharedRef<SWidget> ShieldbreakMenuStyle::MakeChooser(const FText& Label, TFunction<FText()> Value, TFunction<void(int32)> Step)
{
	using namespace ShieldbreakMenuPrivate;
	auto Arrow = [Step](const FText& Glyph, int32 Delta) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(14.f, 4.f))
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
			SNew(SBox).WidthOverride(150.f)
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
			SNew(SBox).WidthOverride(260.f).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text_Lambda([Value]() { return Value(); }).Font(Font(24, "Bold")).ColorAndOpacity(FLinearColor::White)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f)
		[
			Arrow(LOCTEXT("Next", ">"), 1)
		];
}

FText SShieldbreakMainMenu::GetMapDetails() const
{
	if (!Maps.IsValidIndex(MapIndex))
	{
		return FText::GetEmpty();
	}
	const FString Best = ShieldbreakMenuPrivate::BestModes(Maps[MapIndex]);
	return Best.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("BestFor", "Best for {0}"), FText::FromString(Best));
}

FReply SShieldbreakMainMenu::StartMatch(bool bListen)
{
	APlayerController* PC = Owner.Get();
	if (!PC || !Maps.IsValidIndex(MapIndex))
	{
		return FReply::Handled();
	}
	SaveChoices();
	const FString Options = ShieldbreakMenu::BuildMatchOptions(Maps[MapIndex], ShieldbreakMenu::GetModes()[ModeIndex], Bots, Skill, bListen);
	UGameplayStatics::OpenLevel(PC, FName(TEXT("/Engine/Maps/Entry")), true, Options);
	return FReply::Handled();
}

FReply SShieldbreakMainMenu::Join()
{
	APlayerController* PC = Owner.Get();
	if (!PC || Address.IsEmpty())
	{
		return FReply::Handled();
	}
	SaveChoices();
	PC->ClientTravel(Address, TRAVEL_Absolute);
	return FReply::Handled();
}

void SShieldbreakMainMenu::LoadChoices()
{
	using namespace ShieldbreakMenuPrivate;
	if (!GConfig)
	{
		return;
	}
	FString MapName, ModeName;
	if (GConfig->GetString(ConfigSection, TEXT("Map"), MapName, GGameUserSettingsIni))
	{
		MapIndex = FMath::Max(0, Maps.IndexOfByKey(FName(*MapName)));
	}
	if (GConfig->GetString(ConfigSection, TEXT("Mode"), ModeName, GGameUserSettingsIni))
	{
		ModeIndex = FMath::Max(0, ShieldbreakMenu::GetModes().IndexOfByKey(ModeName));
	}
	GConfig->GetInt(ConfigSection, TEXT("Bots"), Bots, GGameUserSettingsIni);
	GConfig->GetInt(ConfigSection, TEXT("Skill"), Skill, GGameUserSettingsIni);
	GConfig->GetString(ConfigSection, TEXT("Address"), Address, GGameUserSettingsIni);
	Bots = FMath::Clamp(Bots, 0, AShieldbreakGameMode::GetMaxBots());
	Skill = FMath::Clamp(Skill, 0, AShieldbreakGameMode::GetMaxBotSkill());
}

void SShieldbreakMainMenu::SaveChoices() const
{
	using namespace ShieldbreakMenuPrivate;
	if (!GConfig || !Maps.IsValidIndex(MapIndex))
	{
		return;
	}
	GConfig->SetString(ConfigSection, TEXT("Map"), *Maps[MapIndex].ToString(), GGameUserSettingsIni);
	GConfig->SetString(ConfigSection, TEXT("Mode"), *ShieldbreakMenu::GetModes()[ModeIndex], GGameUserSettingsIni);
	GConfig->SetInt(ConfigSection, TEXT("Bots"), Bots, GGameUserSettingsIni);
	GConfig->SetInt(ConfigSection, TEXT("Skill"), Skill, GGameUserSettingsIni);
	GConfig->SetString(ConfigSection, TEXT("Address"), *Address, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

// ---- Pause menu ----

void SShieldbreakPauseMenu::Construct(const FArguments& InArgs)
{
	using namespace ShieldbreakMenuPrivate;
	Owner = InArgs._Owner;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			Backdrop_Image(Dim)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 16.f)
			[
				SNew(STextBlock).Text(LOCTEXT("Paused", "MENU")).Font(Font(40, "Bold")).ColorAndOpacity(FLinearColor::White)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 12.f)
			[
				ShieldbreakMenuStyle::MakeSettingsRows()
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				ShieldbreakMenuStyle::MakeButton(LOCTEXT("Resume", "RESUME"), [this]()
				{
					if (AShieldbreakPlayerController* PC = Owner.Get())
					{
						PC->ClosePauseMenu();
					}
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				ShieldbreakMenuStyle::MakeButton(LOCTEXT("PauseControls", "CONTROLS"), [this]() { SShieldbreakControlsMenu::Open(AsShared()); })
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				ShieldbreakMenuStyle::MakeButton(LOCTEXT("Leave", "LEAVE MATCH"), [this]()
				{
					if (AShieldbreakPlayerController* PC = Owner.Get())
					{
						ShieldbreakMenu::ReturnToMainMenu(PC);
					}
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				ShieldbreakMenuStyle::MakeButton(LOCTEXT("QuitGame", "QUIT GAME"), [this]()
				{
					if (AShieldbreakPlayerController* PC = Owner.Get())
					{
						UKismetSystemLibrary::QuitGame(PC, PC, EQuitPreference::Quit, false);
					}
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 12.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PauseHint", "The match keeps running. Esc to resume."))
				.Font(Font(12))
				.ColorAndOpacity(Muted)
			]
		]
	];
}

FReply SShieldbreakPauseMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		if (AShieldbreakPlayerController* PC = Owner.Get())
		{
			PC->ClosePauseMenu();
		}
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

// ---- Controls (key rebinding) ----

void SShieldbreakControlsMenu::Open(const TSharedPtr<SWidget>& ReturnFocus)
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}
	TSharedRef<SShieldbreakControlsMenu> Menu = SNew(SShieldbreakControlsMenu);
	Menu->ReturnFocus = ReturnFocus;
	GEngine->GameViewport->AddViewportWidgetContent(Menu, 30);
	FSlateApplication::Get().SetAllUserFocus(Menu, EFocusCause::SetDirectly);
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
			Backdrop_Image(FLinearColor(0.f, 0.f, 0.f, 0.85f))
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
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		Capturing = INDEX_NONE;
		return FReply::Handled();
	}
	if (Key.IsGamepadKey())
	{
		// Controller buttons are fixed; keep waiting for a keyboard key.
		return FReply::Handled();
	}
	return Capture(Key);
}

FReply SShieldbreakControlsMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
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
	// Swallow clicks on the backdrop so the menu underneath doesn't get them.
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
	const TSharedRef<SWidget> Self = AsShared();
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Self);
	}
	if (TSharedPtr<SWidget> Focus = ReturnFocus.Pin())
	{
		FSlateApplication::Get().SetAllUserFocus(Focus, EFocusCause::SetDirectly);
	}
}

#undef LOCTEXT_NAMESPACE
