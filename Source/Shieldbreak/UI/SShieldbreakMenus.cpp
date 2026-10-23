#include "UI/SShieldbreakMenus.h"

#include "Game/ShieldbreakGameMode.h"
#include "Game/ShieldbreakMaps.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
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
				ShieldbreakMenuStyle::MakeButton(LOCTEXT("Quit", "QUIT"), [this]()
				{
					if (APlayerController* PC = Owner.Get())
					{
						UKismetSystemLibrary::QuitGame(PC, PC, EQuitPreference::Quit, false);
					}
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 24.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Hints", "In a match: F1 controls, Tab scoreboard, Esc menu"))
				.Font(Font(12))
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

#undef LOCTEXT_NAMESPACE
