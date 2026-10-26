#include "UI/SShieldbreakMainMenu.h"

#include "Player/ShieldbreakPlayerController.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ShieldbreakMenuPrivate
{
	const FLinearColor Ink(0.025f, 0.035f, 0.05f);
	const FLinearColor Accent(0.2f, 0.85f, 0.92f);
	const FLinearColor Muted(0.5f, 0.59f, 0.65f);
	const TCHAR* Maps[] = { TEXT("Keel"), TEXT("Bastion"), TEXT("Sanctum"), TEXT("Creekbed"), TEXT("Spire") };
	const TCHAR* Modes[] = { TEXT("Slayer"), TEXT("CTF"), TEXT("KOTH"), TEXT("Oddball") };
	const TCHAR* ModeNames[] = { TEXT("Slayer"), TEXT("Capture the Flag"), TEXT("King of the Hill"), TEXT("Oddball") };
	FText Label(const FString& Value) { return FText::FromString(Value); }
}

void SShieldbreakMainMenu::Construct(const FArguments& InArgs)
{
	using namespace ShieldbreakMenuPrivate;
	Controller = InArgs._Controller;
	bInMatch = InArgs._InMatch;
	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	FSlateBrush Normal = *White;
	Normal.TintColor = FSlateColor(FLinearColor(0.045f, 0.065f, 0.085f));
	FSlateBrush Hover = *White;
	Hover.TintColor = FSlateColor(FLinearColor(0.07f, 0.19f, 0.23f));
	FSlateBrush Press = *White;
	Press.TintColor = FSlateColor(FLinearColor(0.08f, 0.28f, 0.32f));
	ButtonStyle.SetNormal(Normal).SetHovered(Hover).SetPressed(Press)
		.SetNormalPadding(FMargin(20.f, 13.f)).SetPressedPadding(FMargin(20.f, 13.f));
	// Browser rows use only explicit content padding, avoiding double style/content insets.
	BrowserButtonStyle = ButtonStyle;
	BrowserButtonStyle.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
	ChildSlot
	[
		SNew(SBorder).BorderImage(White).BorderBackgroundColor(Ink).Padding(0)
		[
			SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
			[
				SNew(SBox).WidthOverride(1280).HeightOverride(720)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(96, 64, 96, 8)
					[SNew(STextBlock).Text(Label(TEXT("4v4  /  ARENA COMBAT"))).ColorAndOpacity(Accent)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12))]
					+ SVerticalBox::Slot().AutoHeight().Padding(92, 0, 0, 28)
					[SNew(STextBlock).Text(Label(TEXT("SHIELDBREAK"))).ColorAndOpacity(FLinearColor::White)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 54))]
					+ SVerticalBox::Slot().FillHeight(1).Padding(96, 0, 0, 0)
					.HAlign(HAlign_Left)
					[SAssignNew(ContentWidth, SBox).WidthOverride(440)[SAssignNew(Actions, SVerticalBox)]]
					+ SVerticalBox::Slot().AutoHeight().Padding(96, 12, 96, 44)
					[SNew(STextBlock).Text(Label(TEXT("PRE-ALPHA                                     ESC  /  BACK")))
						.ColorAndOpacity(Muted).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10))]
				]
			]
		]
	];
	Home();
}

void SShieldbreakMainMenu::Heading(const FString& LabelText)
{
	Actions->ClearChildren();
	StatusText.Reset(); AddressBox.Reset(); GraphicsText.Reset();
	ContentWidth->SetWidthOverride(440);
	Actions->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
	[SNew(STextBlock).Text(ShieldbreakMenuPrivate::Label(LabelText)).ColorAndOpacity(ShieldbreakMenuPrivate::Accent)
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12))];
}

TSharedRef<STextBlock> SShieldbreakMainMenu::Action(const FText& LabelText, TFunction<void()> Callback)
{
	const TSharedRef<STextBlock> ButtonText = SNew(STextBlock).Text(LabelText).ColorAndOpacity(FLinearColor::White)
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18));
	Actions->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
	[SNew(SButton).ButtonStyle(&ButtonStyle).HAlign(HAlign_Left)
		.OnClicked_Lambda([Callback]() { Callback(); return FReply::Handled(); })[ButtonText]];
	return ButtonText;
}

void SShieldbreakMainMenu::Note(const FText& LabelText)
{
	Actions->AddSlot().AutoHeight().Padding(0, 8)
	[SNew(STextBlock).Text(LabelText).AutoWrapText(true).ColorAndOpacity(ShieldbreakMenuPrivate::Muted)
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12))];
}

void SShieldbreakMainMenu::Home()
{
	using namespace ShieldbreakMenuPrivate;
	Page = 0;
	Heading(bInMatch ? TEXT("MATCH") : TEXT("MAIN MENU"));
	if (bInMatch)
	{
		Action(Label(TEXT("RESUME")), [this]() { if (Controller.IsValid()) { Controller->CloseMainMenu(); } });
		Action(Label(TEXT("LEAVE MATCH")), [this]() {
			Page = 4; Heading(TEXT("LEAVE MATCH?"));
			Note(Label(TEXT("Leaving a hosted match disconnects its players.")));
			Action(Label(TEXT("LEAVE")), [this]() { UGameplayStatics::OpenLevel(Controller.Get(), TEXT("/Engine/Maps/Entry"), true, TEXT("Menu=1")); });
			Action(Label(TEXT("BACK")), [this]() { Home(); });
		});
	}
	else
	{
		Action(Label(TEXT("PLAY")), [this]() { Play(); });
		Action(Label(TEXT("JOIN")), [this]() { Join(); });
	}
	Action(Label(TEXT("SETTINGS")), [this]() { Settings(); });
	Action(Label(TEXT("EXIT")), [this]() { UKismetSystemLibrary::QuitGame(Controller.Get(), Controller.Get(), EQuitPreference::Quit, false); });
}

void SShieldbreakMainMenu::Play()
{
	using namespace ShieldbreakMenuPrivate;
	Page = 1; Heading(TEXT("PLAY  /  4v4"));
	Action(Label(TEXT("MULTIPLAYER")), [this]() { Multiplayer(0); });
	Action(Label(TEXT("HOST MATCH")), [this]() { Multiplayer(1); });
	Action(Label(TEXT("PRACTICE")), [this]() { Multiplayer(2); });
	Action(Label(TEXT("BACK")), [this]() { Home(); });
}

void SShieldbreakMainMenu::Multiplayer(int32 Intent)
{
	using namespace ShieldbreakMenuPrivate;
	Page = 5;
	Heading(Intent == 1 ? TEXT("MULTIPLAYER  /  HOST MATCH") : Intent == 2 ? TEXT("MULTIPLAYER  /  PRACTICE") : TEXT("MULTIPLAYER"));
	ContentWidth->SetWidthOverride(1088);
	TSharedPtr<SVerticalBox> MapList;
	TSharedPtr<SVerticalBox> GameTypes;
	Actions->AddSlot().AutoHeight()
	[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 32, 0)[SAssignNew(MapList, SVerticalBox)]
		+ SHorizontalBox::Slot().FillWidth(1)[SAssignNew(GameTypes, SVerticalBox)]];
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Maps); ++Index)
	{
		MapList->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
		[SNew(SButton).ButtonStyle(&BrowserButtonStyle).ContentPadding(FMargin(16, 8)).HAlign(HAlign_Left)
			.OnClicked_Lambda([this, Index, Intent]() { MapIndex = Index; Multiplayer(Intent); return FReply::Handled(); })
			[SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(STextBlock).Text(Label(Maps[Index])).ColorAndOpacity(Index == MapIndex ? Accent : FLinearColor::White)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
				[SNew(STextBlock).Text(Label(TEXT("Slayer  /  CTF  /  KOTH  /  Oddball"))).ColorAndOpacity(Muted)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10))]]];
	}
	GameTypes->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
	[SNew(STextBlock).Text(Label(FString(Maps[MapIndex]).ToUpper())).ColorAndOpacity(FLinearColor::White)
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22))];
	GameTypes->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
	[SNew(STextBlock).Text(Label(TEXT("GAME TYPE  /  4v4"))).ColorAndOpacity(Muted)
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 11))];
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Modes); ++Index)
	{
		GameTypes->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
		[SNew(SButton).ButtonStyle(&BrowserButtonStyle).ContentPadding(FMargin(16, 10)).HAlign(HAlign_Left)
			.OnClicked_Lambda([this, Index, Intent]() { ModeIndex = Index; Multiplayer(Intent); return FReply::Handled(); })
			[SNew(STextBlock).Text(Label(ModeNames[Index])).ColorAndOpacity(Index == ModeIndex ? Accent : FLinearColor::White)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16))]];
	}
	const FString ContinueLabel = Intent == 1 ? TEXT("HOST MATCH") : Intent == 2 ? TEXT("START PRACTICE") : TEXT("DONE");
	GameTypes->AddSlot().AutoHeight().Padding(0, 12, 0, 6)
	[SNew(SButton).ButtonStyle(&ButtonStyle).HAlign(HAlign_Left)
		.OnClicked_Lambda([this, Intent]() {
			if (Intent == 0) { Play(); } else { StartMatch(Intent == 1); }
			return FReply::Handled();
		})
		[SNew(STextBlock).Text(Label(ContinueLabel)).ColorAndOpacity(Accent)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18))]];
	GameTypes->AddSlot().AutoHeight()
	[SNew(SButton).ButtonStyle(&ButtonStyle).HAlign(HAlign_Left)
		.OnClicked_Lambda([this]() { Play(); return FReply::Handled(); })
		[SNew(STextBlock).Text(Label(TEXT("BACK"))).ColorAndOpacity(FLinearColor::White)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18))]];
}

void SShieldbreakMainMenu::StartMatch(bool bListen)
{
	using namespace ShieldbreakMenuPrivate;
	if (!Controller.IsValid()) { return; }
	const FString Options = FString::Printf(TEXT("Map=%s?Mode=%s?Teams=1?Bots=%d%s"), Maps[MapIndex], Modes[ModeIndex], bListen ? 0 : 7, bListen ? TEXT("?listen") : TEXT(""));
	UGameplayStatics::OpenLevel(Controller.Get(), TEXT("/Engine/Maps/Entry"), true, Options);
}

bool SShieldbreakMainMenu::IsValidAddress(const FString& Address)
{
	// Direct host[:port] only: reject travel options, paths and command text.
	if (Address.IsEmpty() || Address.Len() > 253) { return false; }
	FString HostPart, PortPart;
	if (Address.Split(TEXT(":"), &HostPart, &PortPart))
	{
		if (!PortPart.IsNumeric() || PortPart.Len() > 5 || FCString::Atoi(*PortPart) < 1 || FCString::Atoi(*PortPart) > 65535) { return false; }
	}
	else { HostPart = Address; }
	if (HostPart.IsEmpty() || HostPart.StartsWith(TEXT(".")) || HostPart.EndsWith(TEXT("."))) { return false; }
	for (TCHAR Ch : HostPart)
	{
		if (!(FChar::IsAlnum(Ch) || Ch == TEXT('.') || Ch == TEXT('-'))) { return false; }
	}
	return true;
}

void SShieldbreakMainMenu::Join()
{
	using namespace ShieldbreakMenuPrivate;
	Page = 2; Heading(TEXT("JOIN MATCH"));
	Actions->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
	[SAssignNew(AddressBox, SEditableTextBox).HintText(Label(TEXT("Host address   /   192.168.1.10:7777")))
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16)).Padding(FMargin(14))];
	Action(Label(TEXT("CONNECT")), [this]() {
		const FString Address = AddressBox->GetText().ToString().TrimStartAndEnd();
		if (!IsValidAddress(Address)) { ShowConnectionError(TEXT("Enter a host address and optional port.")); return; }
		ShowConnectionError(TEXT("Connecting..."));
		if (Controller.IsValid()) { Controller->ClientTravel(Address, TRAVEL_Absolute); }
	});
	Action(Label(TEXT("BACK")), [this]() { Home(); });
	Actions->AddSlot().AutoHeight().Padding(0, 8)
	[SAssignNew(StatusText, STextBlock).AutoWrapText(true).ColorAndOpacity(Muted)
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12))];
	Note(Label(TEXT("Direct connection. Internet hosts require a reachable UDP port (default 7777).")));
}

void SShieldbreakMainMenu::ShowConnectionError(const FString& Message)
{
	if (StatusText.IsValid()) { StatusText->SetText(FText::FromString(Message)); }
}

FText SShieldbreakMainMenu::QualityLabel() const
{
	const UGameUserSettings* Config = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const int32 Quality = Config ? Config->GetOverallScalabilityLevel() : -1;
	const TCHAR* Names[] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC") };
	return ShieldbreakMenuPrivate::Label(FString(TEXT("GRAPHICS   /   ")) + (Quality >= 0 && Quality < 4 ? Names[Quality] : TEXT("CUSTOM")));
}

void SShieldbreakMainMenu::Settings()
{
	using namespace ShieldbreakMenuPrivate;
	Page = 3; Heading(TEXT("SETTINGS"));
	GraphicsText = Action(QualityLabel(), [this]() {
		if (UGameUserSettings* Config = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			Config->SetOverallScalabilityLevel((Config->GetOverallScalabilityLevel() + 1) % 4);
			Config->ApplySettings(false); Config->SaveSettings();
			GraphicsText->SetText(QualityLabel());
		}
	});
	Action(Label(TEXT("BACK")), [this]() { Home(); });
	Note(Label(TEXT("Select a graphics preset. Changes apply and save immediately.")));
}

FReply SShieldbreakMainMenu::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		if (Page == 5) { Play(); }
		else if (Page != 0) { Home(); }
		else if (bInMatch && Controller.IsValid()) { Controller->CloseMainMenu(); }
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(Geometry, KeyEvent);
}
