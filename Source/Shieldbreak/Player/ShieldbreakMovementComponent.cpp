#include "Player/ShieldbreakMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

// ---- Network prediction: carry the sprint intent in the saved move ----

class FSavedMove_Shieldbreak : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	uint8 Saved_bWantsToSprint : 1;

	FSavedMove_Shieldbreak()
		: Saved_bWantsToSprint(0)
	{
	}

	virtual void Clear() override
	{
		Super::Clear();
		Saved_bWantsToSprint = 0;
	}

	virtual uint8 GetCompressedFlags() const override
	{
		uint8 Result = Super::GetCompressedFlags();
		if (Saved_bWantsToSprint)
		{
			Result |= FLAG_Custom_0;
		}
		return Result;
	}

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
	{
		const FSavedMove_Shieldbreak* Other = static_cast<const FSavedMove_Shieldbreak*>(NewMove.Get());
		if (Other && Saved_bWantsToSprint != Other->Saved_bWantsToSprint)
		{
			return false;
		}
		return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
	}

	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
		if (const UShieldbreakMovementComponent* Movement = C ? Cast<UShieldbreakMovementComponent>(C->GetCharacterMovement()) : nullptr)
		{
			Saved_bWantsToSprint = Movement->Safe_bWantsToSprint;
		}
	}

	virtual void PrepMoveFor(ACharacter* C) override
	{
		Super::PrepMoveFor(C);
		if (UShieldbreakMovementComponent* Movement = C ? Cast<UShieldbreakMovementComponent>(C->GetCharacterMovement()) : nullptr)
		{
			Movement->Safe_bWantsToSprint = Saved_bWantsToSprint;
		}
	}
};

class FNetworkPredictionData_Client_Shieldbreak : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	explicit FNetworkPredictionData_Client_Shieldbreak(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_Shieldbreak());
	}
};

// ---- Component ----

UShieldbreakMovementComponent::UShieldbreakMovementComponent()
{
	MaxWalkSpeed = 600.f;
	MaxWalkSpeedCrouched = 260.f;
	// Snappier than Halo 2: ~2.6 m apex but only ~1.15 s of airtime (h = v^2 / 2g, t = 2v / g with g = 980 * scale).
	JumpZVelocity = 900.f;
	GravityScale = 1.6f;
	AirControl = 0.45f;
	MaxAcceleration = 4096.f;
	BrakingDecelerationWalking = 2400.f;
	GroundFriction = 8.f;
	BrakingDecelerationFalling = 0.f;
	FallingLateralFriction = 0.f;
	SetCrouchedHalfHeight(58.f);
	bCanWalkOffLedgesWhenCrouching = true;
	NavAgentProps.bCanCrouch = true;
	NavAgentProps.bCanJump = true;
}

FNetworkPredictionData_Client* UShieldbreakMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UShieldbreakMovementComponent* MutableThis = const_cast<UShieldbreakMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Shieldbreak(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}
	return ClientPredictionData;
}

void UShieldbreakMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	Safe_bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

void UShieldbreakMovementComponent::SetSprinting(bool bSprint)
{
	Safe_bWantsToSprint = bSprint;
}

bool UShieldbreakMovementComponent::IsSliding() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_Slide;
}

bool UShieldbreakMovementComponent::IsSprinting() const
{
	if (!Safe_bWantsToSprint || MovementMode != MOVE_Walking || IsCrouching() || !UpdatedComponent)
	{
		return false;
	}

	// Sprint only counts while heading roughly forward.
	FVector Direction = Velocity.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = GetCurrentAcceleration().GetSafeNormal2D();
	}
	const FVector Forward = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	return FVector::DotProduct(Direction, Forward) > 0.5f;
}

float UShieldbreakMovementComponent::GetMaxSpeed() const
{
	if (IsSliding())
	{
		return SlideMaxSpeed;
	}
	if (IsSprinting())
	{
		return SprintSpeed;
	}
	return Super::GetMaxSpeed();
}

bool UShieldbreakMovementComponent::IsMovingOnGround() const
{
	return Super::IsMovingOnGround() || IsSliding();
}

bool UShieldbreakMovementComponent::CanAttemptJump() const
{
	// The engine refuses jumps while bWantsToCrouch is set, which made jumping out of a slide
	// (Momentum Carry) and Halo-style crouch-jumps impossible.
	return IsJumpAllowed() && (IsMovingOnGround() || IsFalling());
}

void UShieldbreakMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (CharacterOwner)
	{
		// Crouched entry is allowed so landing from a slide-jump with crouch held chains straight into another slide.
		if (MovementMode == MOVE_Walking && !IsSliding()
			&& bWantsToCrouch && Safe_bWantsToSprint && Velocity.Size2D() >= SlideMinEntrySpeed)
		{
			EnterSlide();
		}
		else if (IsSliding() && !bWantsToCrouch)
		{
			ExitSlide(MOVE_Walking);
		}
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UShieldbreakMovementComponent::EnterSlide()
{
	const FVector Direction = Velocity.GetSafeNormal2D();
	FVector NewVelocity = Velocity + Direction * SlideEntryBoost;
	if (NewVelocity.Size2D() > SlideMaxSpeed)
	{
		const float Z = NewVelocity.Z;
		NewVelocity = NewVelocity.GetSafeNormal2D() * SlideMaxSpeed;
		NewVelocity.Z = Z;
	}
	Velocity = NewVelocity;
	SetMovementMode(MOVE_Custom, CMOVE_Slide);
}

void UShieldbreakMovementComponent::ExitSlide(EMovementMode NewMode)
{
	SetMovementMode(NewMode);
}

void UShieldbreakMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);

	if (CustomMovementMode == CMOVE_Slide)
	{
		PhysSlide(deltaTime, Iterations);
	}
}

bool UShieldbreakMovementComponent::FindSlideSurface(FHitResult& OutHit) const
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 58.f;
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start - FVector(0.f, 0.f, HalfHeight + 30.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakSlideSurface), false, CharacterOwner);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
	return bHit && OutHit.ImpactNormal.Z >= GetWalkableFloorZ();
}

void UShieldbreakMovementComponent::PhysSlide(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	FHitResult Surface;
	if (!FindSlideSurface(Surface))
	{
		ExitSlide(MOVE_Falling);
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	const FVector Normal = Surface.ImpactNormal;

	// Gravity pulls along the slope: downhill slides speed up, uphill slides bleed speed.
	Velocity += FVector::VectorPlaneProject(FVector(0.f, 0.f, GetGravityZ()), Normal) * deltaTime;

	// Lateral steering only.
	const FVector Dir2D = Velocity.GetSafeNormal2D();
	if (!Dir2D.IsNearlyZero())
	{
		const FVector Right(-Dir2D.Y, Dir2D.X, 0.f);
		const float Steer = FVector::DotProduct(GetCurrentAcceleration().GetSafeNormal2D(), Right);
		Velocity += Right * Steer * SlideSteerAcceleration * deltaTime;
	}

	// Low friction plus constant braking.
	const float Speed = Velocity.Size();
	if (Speed > KINDA_SMALL_NUMBER)
	{
		const float Decel = (SlideFriction * Speed + SlideBraking) * deltaTime;
		const float NewSpeed = FMath::Clamp(Speed - Decel, 0.f, SlideMaxSpeed);
		Velocity *= NewSpeed / Speed;
	}

	Velocity = FVector::VectorPlaneProject(Velocity, Normal);

	// Move, gluing to the floor with a small downward bias.
	Iterations++;
	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FQuat Rotation = UpdatedComponent->GetComponentQuat();
	const FVector Delta = (Velocity + FVector(0.f, 0.f, -120.f)) * deltaTime;

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, Rotation, true, Hit);
	if (Hit.Time < 1.f)
	{
		HandleImpact(Hit, deltaTime, Delta);
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}

	if (!bJustTeleported)
	{
		const FVector Moved = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
		// Keep the planar result but never let the glue bias accumulate as vertical speed.
		Velocity = FVector(Moved.X, Moved.Y, FMath::Max(Moved.Z, Velocity.Z));
	}

	FHitResult NewSurface;
	if (!FindSlideSurface(NewSurface))
	{
		ExitSlide(MOVE_Falling);
	}
	else if (Velocity.Size2D() < SlideExitSpeed)
	{
		ExitSlide(MOVE_Walking);
	}
}

void UShieldbreakMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	// Super clears this for every non-walking mode; without it the crouch that follows slide entry shrinks the
	// capsule about its center, lifting the feet off the floor so the slide immediately falls.
	if (IsSliding())
	{
		bCrouchMaintainsBaseLocation = true;
	}

	const bool bWasSliding = PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_Slide;
	if (bWasSliding && MovementMode == MOVE_Falling)
	{
		bMomentumCarry = true;
	}
	else if (MovementMode == MOVE_Walking || MovementMode == MOVE_None || IsSliding())
	{
		bMomentumCarry = false;
	}
}
