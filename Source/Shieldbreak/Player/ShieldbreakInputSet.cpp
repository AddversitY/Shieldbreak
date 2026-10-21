#include "Player/ShieldbreakInputSet.h"

#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "UObject/Package.h"

namespace ShieldbreakInputSetPrivate
{
	static UShieldbreakInputSet* GInstance = nullptr;

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
	Jump = MakeAction(TEXT("IA_Jump"), false);
	Fire = MakeAction(TEXT("IA_Fire"), false);
	Zoom = MakeAction(TEXT("IA_Zoom"), false);
	Shape = MakeAction(TEXT("IA_Shape"), false);
	Melee = MakeAction(TEXT("IA_Melee"), false);
	Grenade = MakeAction(TEXT("IA_Grenade"), false);
	ReloadVent = MakeAction(TEXT("IA_ReloadVent"), false);
	SwapWeapon = MakeAction(TEXT("IA_SwapWeapon"), false);
	Interact = MakeAction(TEXT("IA_Interact"), false);
	Sprint = MakeAction(TEXT("IA_Sprint"), false);
	CrouchSlide = MakeAction(TEXT("IA_CrouchSlide"), false);
	Scoreboard = MakeAction(TEXT("IA_Scoreboard"), false);
	ToggleHelp = MakeAction(TEXT("IA_ToggleHelp"), false);

	Context = NewObject<UInputMappingContext>(this, TEXT("IMC_Shieldbreak"));

	// ---- Move: WASD -> (X = right, Y = forward) ----
	{
		// W: key value lands in X; swizzle to Y.
		FEnhancedActionKeyMapping& W = Context->MapKey(Move, EKeys::W);
		AddModifier<UInputModifierSwizzleAxis>(W, Context)->Order = EInputAxisSwizzle::YXZ;

		FEnhancedActionKeyMapping& S = Context->MapKey(Move, EKeys::S);
		AddModifier<UInputModifierSwizzleAxis>(S, Context)->Order = EInputAxisSwizzle::YXZ;
		AddModifier<UInputModifierNegate>(S, Context);

		Context->MapKey(Move, EKeys::D);

		FEnhancedActionKeyMapping& A = Context->MapKey(Move, EKeys::A);
		AddModifier<UInputModifierNegate>(A, Context);

		FEnhancedActionKeyMapping& Stick = Context->MapKey(Move, EKeys::Gamepad_Left2D);
		AddModifier<UInputModifierDeadZone>(Stick, Context);
	}

	// ---- Look: mouse (Y up positive) and right stick ----
	{
		Context->MapKey(Look, EKeys::Mouse2D);

		FEnhancedActionKeyMapping& Stick = Context->MapKey(Look, EKeys::Gamepad_Right2D);
		AddModifier<UInputModifierDeadZone>(Stick, Context);
		UInputModifierScalar* Scalar = AddModifier<UInputModifierScalar>(Stick, Context);
		// ~300 x 200 deg/s at full deflection after the PlayerController's legacy 2.5x input scale.
		Scalar->Scalar = FVector(120.0, 80.0, 1.0);
		AddModifier<UInputModifierScaleByDeltaTime>(Stick, Context);
	}

	auto MapPair = [this](UInputAction* Action, const TArray<FKey>& Keys)
	{
		for (const FKey& Key : Keys)
		{
			Context->MapKey(Action, Key);
		}
	};

	MapPair(Jump, { EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom });
	MapPair(Fire, { EKeys::LeftMouseButton, EKeys::Gamepad_RightTriggerAxis });
	MapPair(Zoom, { EKeys::RightMouseButton, EKeys::Gamepad_RightThumbstick });
	MapPair(Shape, { EKeys::LeftAlt, EKeys::ThumbMouseButton, EKeys::Gamepad_LeftTriggerAxis });
	MapPair(Melee, { EKeys::F, EKeys::Gamepad_RightShoulder });
	MapPair(Grenade, { EKeys::G, EKeys::Gamepad_LeftShoulder });
	MapPair(ReloadVent, { EKeys::R, EKeys::Gamepad_FaceButton_Left });
	MapPair(SwapWeapon, { EKeys::Q, EKeys::MouseScrollUp, EKeys::MouseScrollDown, EKeys::Gamepad_FaceButton_Top });
	MapPair(Interact, { EKeys::E, EKeys::Gamepad_DPad_Up });
	MapPair(Sprint, { EKeys::LeftShift, EKeys::Gamepad_LeftThumbstick });
	MapPair(CrouchSlide, { EKeys::LeftControl, EKeys::C, EKeys::Gamepad_FaceButton_Right });
	MapPair(Scoreboard, { EKeys::Tab, EKeys::Gamepad_Special_Left });
	MapPair(ToggleHelp, { EKeys::F1, EKeys::Gamepad_Special_Right });
}
