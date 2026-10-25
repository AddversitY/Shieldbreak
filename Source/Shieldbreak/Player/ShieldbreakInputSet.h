// Enhanced Input actions and the mapping context, built at runtime in C++ so the project
// needs no input assets. Use UShieldbreakInputSet::Get().
// Keyboard and mouse keys are rebindable (Controls menu); overrides live in GameUserSettings.ini,
// section [Shieldbreak.Keys]. Controller buttons are fixed.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "ShieldbreakInputSet.generated.h"

class UInputAction;
class UInputMappingContext;

/** One rebindable keyboard/mouse control. Movement directions share the Move action. */
struct FShieldbreakKeyBinding
{
	FName Id;
	FText Label;
	UInputAction* Action = nullptr;
	/** 0 = not movement; 1 forward, 2 back, 3 right, 4 left. */
	uint8 MoveDirection = 0;
	FKey DefaultKey;
	/** Extra default keys (scroll wheel for swap, C for crouch). Dropped if another control takes them. */
	TArray<FKey> ExtraKeys;
	TArray<FKey> GamepadKeys;
	/** The current key: DefaultKey unless the player rebound it. */
	FKey Key;
};

UCLASS()
class SHIELDBREAK_API UShieldbreakInputSet : public UObject
{
	GENERATED_BODY()

public:
	/** Lazily creates (and roots) the shared instance. */
	static UShieldbreakInputSet* Get();

	UPROPERTY() TObjectPtr<UInputAction> Move;
	UPROPERTY() TObjectPtr<UInputAction> Look;
	/** Right stick look, separate from the mouse so controller players get aim assist. */
	UPROPERTY() TObjectPtr<UInputAction> LookGamepad;
	UPROPERTY() TObjectPtr<UInputAction> Jump;
	UPROPERTY() TObjectPtr<UInputAction> Fire;
	UPROPERTY() TObjectPtr<UInputAction> Zoom;
	UPROPERTY() TObjectPtr<UInputAction> Equipment;
	UPROPERTY() TObjectPtr<UInputAction> Melee;
	UPROPERTY() TObjectPtr<UInputAction> Grenade;
	UPROPERTY() TObjectPtr<UInputAction> ReloadVent;
	UPROPERTY() TObjectPtr<UInputAction> SwapWeapon;
	UPROPERTY() TObjectPtr<UInputAction> Interact;
	UPROPERTY() TObjectPtr<UInputAction> Sprint;
	UPROPERTY() TObjectPtr<UInputAction> CrouchSlide;
	UPROPERTY() TObjectPtr<UInputAction> Scoreboard;
	UPROPERTY() TObjectPtr<UInputAction> ToggleHelp;
	UPROPERTY() TObjectPtr<UInputAction> Pause;

	UPROPERTY() TObjectPtr<UInputMappingContext> Context;

	const TArray<FShieldbreakKeyBinding>& GetBindings() const { return Bindings; }
	/** The current key of a control by Id (e.g. "Melee"), or an invalid key. */
	FKey GetKey(FName Id) const;
	/** Short display name of a control's key, e.g. "F" or "LMB"-style names from the engine. */
	FString GetKeyName(FName Id) const;
	/** Keyboard and mouse buttons only; Escape stays the menu key. */
	static bool CanBind(const FKey& Key);
	/** Binds a control to Key. A control already on Key swaps to this control's old key. Saves and applies. */
	void Rebind(int32 Index, const FKey& Key);
	void ResetToDefaults();
	/** Sets every control's key at once (one per binding, in GetBindings order), e.g. to restore saved keys. */
	void SetAllKeys(const TArray<FKey>& Keys);

private:
	void Build();
	void AddBinding(FName Id, const FText& Label, UInputAction* Action, const TArray<FKey>& KeyboardKeys, const TArray<FKey>& PadKeys, uint8 MoveDirection = 0);
	void LoadOverrides();
	void SaveOverrides() const;
	/** Rebuilds every mapping in Context from Bindings and asks local players to pick the change up. */
	void ApplyMappings();

	TArray<FShieldbreakKeyBinding> Bindings;
	UInputAction* MakeAction(const TCHAR* Name, bool bAxis2D);
};
