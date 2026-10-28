#include "Player/ShieldbreakInputSet.h"

#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "EnhancedInputSubsystems.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "ShieldbreakInput"

namespace ShieldbreakInputSetPrivate
{
	static UShieldbreakInputSet* GInstance = nullptr;
	const TCHAR* const KeysSection = TEXT("Shieldbreak.Keys");

	template <typename T>
	T* AddModifier(FEnhancedActionKeyMapping& Mapping, UObject* Outer)
	{
		T* Modifier = NewObject<T>(Outer);
		Mapping.Modifiers.Add(Modifier);
		return Modifier;
	}
}

UShieldbreakInputSet* UShieldbreakInputSet::Get()
{
	using namespace ShieldbreakInputSetPrivate;

	if (!GInstance)
	{
		GInstance = NewObject<UShieldbreakInputSet>(GetTransientPackage());
		GInstance->AddToRoot();
		GInstance->Build();
	}
	return GInstance;
}

UInputAction* UShieldbreakInputSet::MakeAction(const TCHAR* Name, bool bAxis2D)
{
	UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
	Action->ValueType = bAxis2D ? EInputActionValueType::Axis2D : EInputActionValueType::Boolean;
	return Action;
}

void UShieldbreakInputSet::Build()
{
	using namespace ShieldbreakInputSetPrivate;

	Move = MakeAction(TEXT("IA_Move"), true);
	Look = MakeAction(TEXT("IA_Look"), true);
	LookGamepad = MakeAction(TEXT("IA_LookGamepad"), true);
	Jump = MakeAction(TEXT("IA_Jump"), false);
	Fire = MakeAction(TEXT("IA_Fire"), false);
	Zoom = MakeAction(TEXT("IA_Zoom"), false);
	Equipment = MakeAction(TEXT("IA_Equipment"), false);
	Melee = MakeAction(TEXT("IA_Melee"), false);
	Grenade = MakeAction(TEXT("IA_Grenade"), false);
	ReloadVent = MakeAction(TEXT("IA_ReloadVent"), false);
	SwapWeapon = MakeAction(TEXT("IA_SwapWeapon"), false);
	Interact = MakeAction(TEXT("IA_Interact"), false);
	Sprint = MakeAction(TEXT("IA_Sprint"), false);
	CrouchSlide = MakeAction(TEXT("IA_CrouchSlide"), false);
	Scoreboard = MakeAction(TEXT("IA_Scoreboard"), false);
	ToggleHelp = MakeAction(TEXT("IA_ToggleHelp"), false);
	Pause = MakeAction(TEXT("IA_Pause"), false);

	Context = NewObject<UInputMappingContext>(this, TEXT("IMC_Shieldbreak"));

	AddBinding(TEXT("MoveForward"), LOCTEXT("MoveForward", "Move forward"), Move, { EKeys::W }, {}, 1);
	AddBinding(TEXT("MoveBack"), LOCTEXT("MoveBack", "Move back"), Move, { EKeys::S }, {}, 2);
	AddBinding(TEXT("MoveLeft"), LOCTEXT("MoveLeft", "Move left"), Move, { EKeys::A }, {}, 4);
	AddBinding(TEXT("MoveRight"), LOCTEXT("MoveRight", "Move right"), Move, { EKeys::D }, {}, 3);
	AddBinding(TEXT("Jump"), LOCTEXT("Jump", "Jump"), Jump, { EKeys::SpaceBar }, { EKeys::Gamepad_FaceButton_Bottom });
	AddBinding(TEXT("Sprint"), LOCTEXT("Sprint", "Sprint"), Sprint, { EKeys::LeftShift }, { EKeys::Gamepad_LeftThumbstick });
	AddBinding(TEXT("CrouchSlide"), LOCTEXT("CrouchSlide", "Crouch / slide"), CrouchSlide, { EKeys::LeftControl, EKeys::C }, { EKeys::Gamepad_FaceButton_Right });
	AddBinding(TEXT("Fire"), LOCTEXT("Fire", "Fire"), Fire, { EKeys::LeftMouseButton }, { EKeys::Gamepad_RightTriggerAxis });
	AddBinding(TEXT("Zoom"), LOCTEXT("Zoom", "Zoom"), Zoom, { EKeys::RightMouseButton }, { EKeys::Gamepad_RightThumbstick });
	AddBinding(TEXT("ReloadVent"), LOCTEXT("ReloadVent", "Reload / vent"), ReloadVent, { EKeys::R }, { EKeys::Gamepad_FaceButton_Left });
	AddBinding(TEXT("SwapWeapon"), LOCTEXT("SwapWeapon", "Swap weapon"), SwapWeapon, { EKeys::Q, EKeys::MouseScrollUp, EKeys::MouseScrollDown }, { EKeys::Gamepad_FaceButton_Top });
	AddBinding(TEXT("Melee"), LOCTEXT("Melee", "Melee"), Melee, { EKeys::F }, { EKeys::Gamepad_RightShoulder });
	AddBinding(TEXT("Grenade"), LOCTEXT("Grenade", "Grenade"), Grenade, { EKeys::G }, { EKeys::Gamepad_LeftShoulder });
	AddBinding(TEXT("Equipment"), LOCTEXT("Equipment", "Use equipment"), Equipment, { EKeys::LeftAlt, EKeys::ThumbMouseButton }, { EKeys::Gamepad_LeftTriggerAxis });
	AddBinding(TEXT("Interact"), LOCTEXT("Interact", "Pick up / interact"), Interact, { EKeys::E }, { EKeys::Gamepad_DPad_Up });
	AddBinding(TEXT("Scoreboard"), LOCTEXT("Scoreboard", "Scoreboard"), Scoreboard, { EKeys::Tab }, { EKeys::Gamepad_Special_Left });
	AddBinding(TEXT("ToggleHelp"), LOCTEXT("ToggleHelp", "Controls help"), ToggleHelp, { EKeys::F1 }, { EKeys::Gamepad_DPad_Down });

	LoadOverrides();
	ApplyMappings();
}

void UShieldbreakInputSet::AddBinding(FName Id, const FText& Label, UInputAction* Action, const TArray<FKey>& KeyboardKeys, const TArray<FKey>& PadKeys, uint8 MoveDirection)
{
	FShieldbreakKeyBinding& Binding = Bindings.AddDefaulted_GetRef();
	Binding.Id = Id;
	Binding.Label = Label;
	Binding.Action = Action;
	Binding.MoveDirection = MoveDirection;
	Binding.DefaultKey = KeyboardKeys[0];
	Binding.ExtraKeys = TArray<FKey>(KeyboardKeys.GetData() + 1, KeyboardKeys.Num() - 1);
	Binding.GamepadKeys = PadKeys;
	Binding.Key = Binding.DefaultKey;
}

void UShieldbreakInputSet::ApplyMappings()
{
	using namespace ShieldbreakInputSetPrivate;

	Context->UnmapAll();

	TSet<FKey> Taken;
	for (const FShieldbreakKeyBinding& Binding : Bindings)
	{
		Taken.Add(Binding.Key);
	}

	for (const FShieldbreakKeyBinding& Binding : Bindings)
	{
		TArray<FKey> Keys = { Binding.Key };
		for (const FKey& Extra : Binding.ExtraKeys)
		{
			if (!Taken.Contains(Extra))
			{
				Keys.Add(Extra);
			}
		}
		Keys.Append(Binding.GamepadKeys);

		for (const FKey& Key : Keys)
		{
			FEnhancedActionKeyMapping& Mapping = Context->MapKey(Binding.Action, Key);
			// Move is (X = right, Y = forward); a key's value lands in X.
			if (Binding.MoveDirection == 1 || Binding.MoveDirection == 2)
			{
				AddModifier<UInputModifierSwizzleAxis>(Mapping, Context)->Order = EInputAxisSwizzle::YXZ;
			}
			if (Binding.MoveDirection == 2 || Binding.MoveDirection == 4)
			{
				AddModifier<UInputModifierNegate>(Mapping, Context);
			}
		}
	}

	FEnhancedActionKeyMapping& MoveStick = Context->MapKey(Move, EKeys::Gamepad_Left2D);
	AddModifier<UInputModifierDeadZone>(MoveStick, Context);

	// ---- Look: mouse (Y up positive) and right stick ----
	Context->MapKey(Look, EKeys::Mouse2D);

	FEnhancedActionKeyMapping& LookStick = Context->MapKey(LookGamepad, EKeys::Gamepad_Right2D);
	AddModifier<UInputModifierDeadZone>(LookStick, Context);
	UInputModifierScalar* Scalar = AddModifier<UInputModifierScalar>(LookStick, Context);
	// ~300 x 200 deg/s at full deflection after the PlayerController's legacy 2.5x input scale.
	Scalar->Scalar = FVector(120.0, 80.0, 1.0);
	AddModifier<UInputModifierScaleByDeltaTime>(LookStick, Context);

	Context->MapKey(Pause, EKeys::Gamepad_Special_Right);

	for (TObjectIterator<UEnhancedInputLocalPlayerSubsystem> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject) && It->GetLocalPlayer() && It->HasMappingContext(Context))
		{
			It->RequestRebuildControlMappings();
		}
	}
}

FKey UShieldbreakInputSet::GetKey(FName Id) const
{
	const FShieldbreakKeyBinding* Binding = Bindings.FindByPredicate([Id](const FShieldbreakKeyBinding& B) { return B.Id == Id; });
	return Binding ? Binding->Key : EKeys::Invalid;
}

FString UShieldbreakInputSet::GetKeyName(FName Id) const
{
	const FKey Key = GetKey(Id);
	return Key.IsValid() ? Key.GetDisplayName(false).ToString() : FString(TEXT("?"));
}

bool UShieldbreakInputSet::CanBind(const FKey& Key)
{
	return Key.IsValid() && Key != EKeys::Escape && !Key.IsGamepadKey() && !Key.IsTouch()
		&& !Key.IsAxis1D() && !Key.IsAxis2D() && !Key.IsAxis3D();
}

void UShieldbreakInputSet::Rebind(int32 Index, const FKey& Key)
{
	if (!Bindings.IsValidIndex(Index) || !CanBind(Key))
	{
		return;
	}
	const FKey OldKey = Bindings[Index].Key;
	for (FShieldbreakKeyBinding& Other : Bindings)
	{
		if (&Other != &Bindings[Index] && Other.Key == Key)
		{
			Other.Key = OldKey;
		}
	}
	Bindings[Index].Key = Key;
	SaveOverrides();
	ApplyMappings();
}

void UShieldbreakInputSet::ResetToDefaults()
{
	for (FShieldbreakKeyBinding& Binding : Bindings)
	{
		Binding.Key = Binding.DefaultKey;
	}
	SaveOverrides();
	ApplyMappings();
}

void UShieldbreakInputSet::SetAllKeys(const TArray<FKey>& Keys)
{
	for (int32 Index = 0; Index < Bindings.Num() && Index < Keys.Num(); ++Index)
	{
		if (CanBind(Keys[Index]))
		{
			Bindings[Index].Key = Keys[Index];
		}
	}
	SaveOverrides();
	ApplyMappings();
}

void UShieldbreakInputSet::LoadOverrides()
{
	using namespace ShieldbreakInputSetPrivate;
	if (!GConfig)
	{
		return;
	}
	for (FShieldbreakKeyBinding& Binding : Bindings)
	{
		FString Name;
		if (GConfig->GetString(KeysSection, *Binding.Id.ToString(), Name, GGameUserSettingsIni))
		{
			const FKey Key(*Name);
			if (CanBind(Key))
			{
				Binding.Key = Key;
			}
		}
	}
}

void UShieldbreakInputSet::SaveOverrides() const
{
	using namespace ShieldbreakInputSetPrivate;
	if (!GConfig)
	{
		return;
	}
	GConfig->EmptySection(KeysSection, GGameUserSettingsIni);
	for (const FShieldbreakKeyBinding& Binding : Bindings)
	{
		if (Binding.Key != Binding.DefaultKey)
		{
			GConfig->SetString(KeysSection, *Binding.Id.ToString(), *Binding.Key.ToString(), GGameUserSettingsIni);
		}
	}
	GConfig->Flush(false, GGameUserSettingsIni);
}

#undef LOCTEXT_NAMESPACE
