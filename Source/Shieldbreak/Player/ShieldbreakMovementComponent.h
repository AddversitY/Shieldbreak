// Halo-style character movement plus sprint, crouch-slide and the Momentum Carry mechanic.
// Sprint is network-predicted through a custom saved move (FLAG_Custom_0).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShieldbreakMovementComponent.generated.h"

enum ECustomMovementMode_Shieldbreak
{
	CMOVE_None = 0,
	CMOVE_Slide = 1,
};

UCLASS()
class SHIELDBREAK_API UShieldbreakMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UShieldbreakMovementComponent();

	// ---- Tuning ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shieldbreak|Sprint")
	float SprintSpeed = 880.f;

	/** Minimum 2D speed to start a slide from a sprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shieldbreak|Slide")
	float SlideMinEntrySpeed = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shieldbreak|Slide")
	float SlideEntryBoost = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shieldbreak|Slide")
	float SlideMaxSpeed = 1150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shieldbreak|Slide")
	float SlideExitSpeed = 380.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shieldbreak|Slide")
	float SlideFriction = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shieldbreak|Slide")
	float SlideBraking = 350.f;

	/** Lateral steering acceleration while sliding (uu/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shieldbreak|Slide")
	float SlideSteerAcceleration = 700.f;

	// ---- Queries ----

	bool IsSliding() const;
	bool IsSprinting() const;
	bool IsMomentumCarrying() const { return bMomentumCarry; }

	/** Called by the owning character from input or AI. */
	void SetSprinting(bool bSprint);

	// ---- UCharacterMovementComponent ----

	virtual float GetMaxSpeed() const override;
	virtual bool IsMovingOnGround() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	/** Replicated-by-saved-move intent. */
	bool Safe_bWantsToSprint = false;

protected:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	void EnterSlide();
	void ExitSlide(EMovementMode NewMode);
	void PhysSlide(float deltaTime, int32 Iterations);
	bool FindSlideSurface(FHitResult& OutHit) const;

	bool bMomentumCarry = false;
};
