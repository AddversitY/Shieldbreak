// Enhanced Input actions and the mapping context, built at runtime in C++ so the project
// needs no input assets. Use UShieldbreakInputSet::Get().

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ShieldbreakInputSet.generated.h"

class UInputAction;
class UInputMappingContext;

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

private:
	void Build();
	UInputAction* MakeAction(const TCHAR* Name, bool bAxis2D);
};
