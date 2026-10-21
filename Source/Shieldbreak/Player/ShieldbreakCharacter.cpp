#include "Player/ShieldbreakCharacter.h"

#include "Audio/ShieldbreakCharacterAudio.h"
#include "Audio/ShieldbreakSFX.h"
#include "Visuals/ShieldbreakBodyAnimator.h"
#include "Game/ShieldbreakTeams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Shieldbreak.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/ShieldbreakGameMode.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/ShieldbreakInputSet.h"
#include "Player/ShieldbreakMovementComponent.h"
#include "Player/ShieldbreakPlayerController.h"
#include "Player/ShieldbreakPlayerState.h"
#include "Shield/ShieldHealthComponent.h"
#include "Shield/ShieldTuningData.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/ShieldbreakDamageTypes.h"
#include "Weapons/ShieldbreakGrenade.h"
#include "Weapons/ShieldbreakTracer.h"
#include "Weapons/ShieldbreakWeapon.h"
#include "Weapons/ShieldbreakWeaponClasses.h"
#include "Weapons/ShieldbreakWeaponPickup.h"

namespace ShieldbreakCharacterPrivate
{
	const FLinearColor VisorColor(1.f, 0.65f, 0.08f);
	const FLinearColor ShapeColor(0.15f, 0.45f, 1.f);
	constexpr float LungeSpeed = 1500.f;
	constexpr float MeleeConeDot = 0.7f;
	constexpr float ZoomInterpSpeed = 14.f;
}

AShieldbreakCharacter::AShieldbreakCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UShieldbreakMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(40.f, 92.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	BaseEyeHeight = 64.f;
	CrouchedEyeHeight = 64.f;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(DefaultFOV);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	UStaticMesh* CubeAsset = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* SphereAsset = SphereFinder.Succeeded() ? SphereFinder.Object : nullptr;
	UStaticMesh* CylinderAsset = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
	UMaterialInterface* BaseMaterial = MaterialFinder.Succeeded() ? MaterialFinder.Object : nullptr;

	auto SetupMesh = [&](UStaticMeshComponent* Component, UStaticMesh* Asset)
	{
		Component->SetupAttachment(GetCapsuleComponent());
		if (Asset)
		{
			Component->SetStaticMesh(Asset);
		}
		if (BaseMaterial)
		{
			Component->SetMaterial(0, BaseMaterial);
		}
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
	};

	// Body: cylinder 80 wide, 150 tall, feet at the capsule bottom.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	SetupMesh(BodyMesh, CylinderAsset);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -17.f));
	BodyMesh->SetRelativeScale3D(FVector(0.75f, 0.75f, 1.5f));
	BodyMesh->SetOwnerNoSee(true);

	// Head: the headshot hitbox. Only weapon traces see it.
	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	SetupMesh(HeadMesh, SphereAsset);
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 72.f));
	HeadMesh->SetRelativeScale3D(FVector(0.42f));
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadMesh->SetCollisionObjectType(ECC_WorldDynamic);
	HeadMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	HeadMesh->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);
	HeadMesh->SetOwnerNoSee(true);

	VisorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisorMesh"));
	SetupMesh(VisorMesh, CubeAsset);
	VisorMesh->SetRelativeLocation(FVector(16.f, 0.f, 74.f));
	VisorMesh->SetRelativeScale3D(FVector(0.08f, 0.3f, 0.12f));
	VisorMesh->SetOwnerNoSee(true);

	ShapeShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShapeShieldMesh"));
	SetupMesh(ShapeShieldMesh, CubeAsset);
	ShapeShieldMesh->SetRelativeLocation(FVector(48.f, 0.f, 10.f));
	ShapeShieldMesh->SetRelativeScale3D(FVector(0.04f, 1.1f, 1.5f));
	ShapeShieldMesh->SetOwnerNoSee(true);
	ShapeShieldMesh->SetVisibility(false);
	ShapeShieldMesh->SetCastShadow(false);

	ShieldHealth = CreateDefaultSubobject<UShieldHealthComponent>(TEXT("ShieldHealth"));
	CreateDefaultSubobject<UShieldbreakCharacterAudio>(TEXT("ShieldAudio"));
	BodyAnimator = CreateDefaultSubobject<UShieldbreakBodyAnimator>(TEXT("BodyAnimator"));

	// Mannequin placement (the mesh asset itself is assigned at runtime by BodyAnimator): feet on the
	// capsule bottom, facing +X. Set here so ACharacter caches it as the crouch base offset.
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -92.f), FRotator(0.f, -90.f, 0.f));
	// Bullets must reach the head hitbox sphere, not stop on the visual mesh.
	GetMesh()->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);

	PrimaryWeaponClass = AWeapon_BattleRifle::StaticClass();
	SecondaryWeaponClass = AWeapon_SMG::StaticClass();
	GrenadeClass = AShieldbreakGrenade::StaticClass();
}

void AShieldbreakCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShieldbreakCharacter, Inventory);
	DOREPLIFETIME(AShieldbreakCharacter, CurrentWeapon);
	DOREPLIFETIME_CONDITION(AShieldbreakCharacter, GrenadeCount, COND_OwnerOnly);
}

void AShieldbreakCharacter::BeginPlay()
{
	Super::BeginPlay();

	// With the mannequin in place the greybox pieces are hidden; HeadMesh stays as the (invisible) headshot hitbox.
	if (BodyAnimator && BodyAnimator->HasBodyArt())
	{
		BodyMesh->SetVisibility(false);
		VisorMesh->SetVisibility(false);
		HeadMesh->SetVisibility(false);

		// A replicated weapon can be equipped before the body art existed; move it to the hand now.
		if (CurrentWeapon)
		{
			CurrentWeapon->OnEquipped();
		}
	}

	TintMesh(VisorMesh, ShieldbreakCharacterPrivate::VisorColor, VisorMID);
	TintMesh(ShapeShieldMesh, ShieldbreakCharacterPrivate::ShapeColor, ShapeMID);
	// The color may have been applied before the mannequin existed; force a re-apply now.
	AppliedColor = FLinearColor(-1.f, -1.f, -1.f, -1.f);
	ApplyPlayerColor();

	if (ShieldHealth)
	{
		ShieldHealth->OnDeath.AddDynamic(this, &AShieldbreakCharacter::HandleDeath);
		ShieldHealth->OnShieldChanged.AddDynamic(this, &AShieldbreakCharacter::HandleShieldChanged);
		ShieldHealth->OnBreakPulse.AddDynamic(this, &AShieldbreakCharacter::HandleBreakPulse);
	}

	if (HasAuthority())
	{
		GrenadeCount = FMath::Clamp(StartingGrenades, 0, MaxGrenades);
		SpawnDefaultInventory();
	}
}

void AShieldbreakCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		DestroyInventory();
	}
	Super::EndPlay(EndPlayReason);
}

void AShieldbreakCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ApplyPlayerColor();
}

void AShieldbreakCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Viewmodel vs third-person attachment depends on who controls us, which can arrive after the weapon.
	if (CurrentWeapon)
	{
		CurrentWeapon->OnEquipped();
	}
}

bool AShieldbreakCharacter::ShouldTakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) const
{
	if (!Super::ShouldTakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser))
	{
		return false;
	}

	// Friendly fire off: teammates can't hurt each other, but your own grenades still hurt you.
	const AActor* Source = EventInstigator ? static_cast<const AActor*>(EventInstigator) : DamageCauser;
	if (Source && EventInstigator != GetController() && ShieldbreakTeams::AreTeammates(Source, this))
	{
		const AShieldbreakGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AShieldbreakGameMode>() : nullptr;
		return GameMode && GameMode->IsFriendlyFireEnabled();
	}
	return true;
}

void AShieldbreakCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyPlayerColor();
}

UShieldbreakMovementComponent* AShieldbreakCharacter::GetShieldbreakMovement() const
{
	return Cast<UShieldbreakMovementComponent>(GetCharacterMovement());
}

// ---- Inventory ----

void AShieldbreakCharacter::SpawnDefaultInventory()
{
	if (PrimaryWeaponClass)
	{
		GiveWeapon(PrimaryWeaponClass);
	}
	if (SecondaryWeaponClass)
	{
		GiveWeapon(SecondaryWeaponClass);
	}
}

void AShieldbreakCharacter::DestroyInventory()
{
	for (AShieldbreakWeapon* Weapon : Inventory)
	{
		if (IsValid(Weapon))
		{
			Weapon->Destroy();
		}
	}
	Inventory.Reset();
	CurrentWeapon = nullptr;
}

void AShieldbreakCharacter::GiveWeapon(TSubclassOf<AShieldbreakWeapon> WeaponClass)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !WeaponClass)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AShieldbreakWeapon* NewWeapon = World->SpawnActor<AShieldbreakWeapon>(WeaponClass, GetActorLocation(), GetActorRotation(), Params);
	if (!NewWeapon)
	{
		UE_LOG(LogShieldbreak, Warning, TEXT("Failed to spawn weapon %s."), *GetNameSafe(WeaponClass.Get()));
		return;
	}
	NewWeapon->SetOwningCharacter(this);

	Inventory.RemoveAll([](const TObjectPtr<AShieldbreakWeapon>& Entry) { return !IsValid(Entry.Get()); });

	if (Inventory.Num() < 2)
	{
		Inventory.Add(NewWeapon);
		if (!CurrentWeapon)
		{
			EquipWeapon(NewWeapon);
		}
		else
		{
			NewWeapon->OnUnequipped();
		}
		return;
	}

	// Full: the new weapon replaces the one in hand.
	const int32 Slot = Inventory.IndexOfByKey(CurrentWeapon);
	AShieldbreakWeapon* Replaced = CurrentWeapon;
	Inventory[Slot == INDEX_NONE ? 0 : Slot] = NewWeapon;
	CurrentWeapon = nullptr;
	EquipWeapon(NewWeapon);
	if (IsValid(Replaced))
	{
		Replaced->Destroy();
	}
}

void AShieldbreakCharacter::EquipWeapon(AShieldbreakWeapon* Weapon)
{
	if (!HasAuthority() || !Weapon || Weapon == CurrentWeapon)
	{
		return;
	}

	CurrentWeapon = Weapon;
	RefreshEquippedVisuals();
	SetZoomed(false);
}

void AShieldbreakCharacter::OnRep_CurrentWeapon()
{
	RefreshEquippedVisuals();
	SetZoomed(false);
}

void AShieldbreakCharacter::RefreshEquippedVisuals()
{
	AShieldbreakWeapon* Previous = VisualEquippedWeapon.Get();
	if (Previous && Previous != CurrentWeapon)
	{
		Previous->OnUnequipped();
	}

	// Hide anything else we carry that this machine may have shown.
	for (AShieldbreakWeapon* Carried : Inventory)
	{
		if (IsValid(Carried) && Carried != CurrentWeapon)
		{
			Carried->SetActorHiddenInGame(true);
		}
	}

	if (CurrentWeapon)
	{
		CurrentWeapon->OnEquipped();
	}
	VisualEquippedWeapon = CurrentWeapon;
}

AShieldbreakWeapon* AShieldbreakCharacter::GetHolsteredWeapon() const
{
	for (AShieldbreakWeapon* Carried : Inventory)
	{
		if (IsValid(Carried) && Carried != CurrentWeapon)
		{
			return Carried;
		}
	}
	return nullptr;
}

void AShieldbreakCharacter::AddGrenades(int32 Count)
{
	if (HasAuthority())
	{
		GrenadeCount = FMath::Clamp(GrenadeCount + Count, 0, MaxGrenades);
	}
}

// ---- Queries ----

bool AShieldbreakCharacter::IsAlive() const
{
	return ShieldHealth && !ShieldHealth->IsDead();
}

FVector AShieldbreakCharacter::GetAimOrigin() const
{
	return GetPawnViewLocation();
}

FRotator AShieldbreakCharacter::GetAimRotation() const
{
	if (IsLocallyControlled() && Controller)
	{
		return Controller->GetControlRotation();
	}
	return GetBaseAimRotation();
}

float AShieldbreakCharacter::GetSpreadMultiplier() const
{
	float Multiplier = 1.f;
	const UShieldbreakMovementComponent* Movement = GetShieldbreakMovement();
	if (Movement)
	{
		if (Movement->IsMomentumCarrying())
		{
			Multiplier *= 1.75f;
		}
		else if (Movement->IsFalling())
		{
			Multiplier *= 1.3f;
		}
	}
	if (bIsCrouched)
	{
		Multiplier *= 0.75f;
	}
	if (bZoomed)
	{
		Multiplier *= 0.5f;
	}
	return Multiplier;
}

FLinearColor AShieldbreakCharacter::GetPlayerColor() const
{
	if (const AShieldbreakPlayerState* SBState = GetPlayerState<AShieldbreakPlayerState>())
	{
		return SBState->GetPlayerColor();
	}
	return FLinearColor(0.6f, 0.6f, 0.6f);
}

// ---- Visuals ----

void AShieldbreakCharacter::TintMesh(UStaticMeshComponent* MeshComponent, const FLinearColor& Color, TObjectPtr<UMaterialInstanceDynamic>& OutMID)
{
	if (!MeshComponent)
	{
		return;
	}
	if (!OutMID)
	{
		OutMID = MeshComponent->CreateDynamicMaterialInstance(0);
	}
	if (OutMID)
	{
		OutMID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void AShieldbreakCharacter::ApplyPlayerColor()
{
	const FLinearColor Color = GetPlayerColor();
	if (Color.Equals(AppliedColor))
	{
		return;
	}
	AppliedColor = Color;
	TintMesh(BodyMesh, Color, BodyMID);
	TintMesh(HeadMesh, Color * 0.8f, HeadMID);

	// Mannequin (M_Mannequin, both slots): TeamColor is the armor base, EmissiveColor* the glowing strips.
	if (USkeletalMeshComponent* Body = GetMesh(); Body && Body->GetSkeletalMeshAsset())
	{
		static const FName GlowParameters[] = { TEXT("EmissiveColor"), TEXT("EmissiveColor2"), TEXT("EmissiveColor3") };
		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* MID = Body->CreateDynamicMaterialInstance(Slot))
			{
				MID->SetVectorParameterValue(TEXT("TeamColor"), Color);
				for (const FName& Parameter : GlowParameters)
				{
					MID->SetVectorParameterValue(Parameter, Color * 4.f);
				}
			}
		}
	}
}

void AShieldbreakCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The PlayerState color can replicate after the pawn; cheap to re-check.
	ApplyPlayerColor();

	const bool bLocalPlayerView = Controller && Controller->IsLocalPlayerController();
	if (!bLocalPlayerView && FirstPersonCamera)
	{
		// Remote characters and bots: pitch the camera (and the weapon attached to it) to the aim.
		FRotator Relative = FirstPersonCamera->GetRelativeRotation();
		Relative.Pitch = FRotator::NormalizeAxis(GetBaseAimRotation().Pitch);
		Relative.Yaw = 0.f;
		Relative.Roll = 0.f;
		FirstPersonCamera->SetRelativeRotation(Relative);
	}

	if (ShapeShieldMesh)
	{
		ShapeShieldMesh->SetVisibility(IsAlive() && ShieldHealth && ShieldHealth->IsShaping());
	}

	if (bLocalPlayerView && FirstPersonCamera)
	{
		const float ZoomFOV = (bZoomed && CurrentWeapon) ? CurrentWeapon->GetStats().ZoomFOV : 0.f;
		const float TargetFOV = ZoomFOV > 0.f ? ZoomFOV : DefaultFOV;
		FirstPersonCamera->SetFieldOfView(FMath::FInterpTo(FirstPersonCamera->FieldOfView, TargetFOV, DeltaSeconds, ShieldbreakCharacterPrivate::ZoomInterpSpeed));
	}
}

// ---- Input ----

void AShieldbreakCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	UShieldbreakInputSet* Inputs = UShieldbreakInputSet::Get();
	if (!EnhancedInput || !Inputs)
	{
		UE_LOG(LogShieldbreak, Error, TEXT("Character input setup failed: no EnhancedInputComponent."));
		return;
	}

	EnhancedInput->BindAction(Inputs->Move, ETriggerEvent::Triggered, this, &AShieldbreakCharacter::Input_Move);
	EnhancedInput->BindAction(Inputs->Look, ETriggerEvent::Triggered, this, &AShieldbreakCharacter::Input_Look);
	EnhancedInput->BindAction(Inputs->Jump, ETriggerEvent::Started, this, &AShieldbreakCharacter::Input_JumpStarted);
	EnhancedInput->BindAction(Inputs->Jump, ETriggerEvent::Completed, this, &AShieldbreakCharacter::Input_JumpCompleted);
	EnhancedInput->BindAction(Inputs->Fire, ETriggerEvent::Started, this, &AShieldbreakCharacter::Input_FireStarted);
	EnhancedInput->BindAction(Inputs->Fire, ETriggerEvent::Completed, this, &AShieldbreakCharacter::Input_FireCompleted);
	EnhancedInput->BindAction(Inputs->Zoom, ETriggerEvent::Started, this, &AShieldbreakCharacter::Input_Zoom);
	EnhancedInput->BindAction(Inputs->Shape, ETriggerEvent::Started, this, &AShieldbreakCharacter::Input_ShapeStarted);
	EnhancedInput->BindAction(Inputs->Shape, ETriggerEvent::Completed, this, &AShieldbreakCharacter::Input_ShapeCompleted);
	EnhancedInput->BindAction(Inputs->Melee, ETriggerEvent::Started, this, &AShieldbreakCharacter::Input_Melee);
	EnhancedInput->BindAction(Inputs->Grenade, ETriggerEvent::Started, this, &AShieldbreakCharacter::Input_Grenade);
	EnhancedInput->BindAction(Inputs->ReloadVent, ETriggerEvent::Started, this, &AShieldbreakCharacter::Input_ReloadVent);
	EnhancedInput->BindAction(Inputs->SwapWeapon, ETriggerEvent::Started, this, &AShieldbreakCharacter::Input_Swap);
	EnhancedInput->BindAction(Inputs->Interact, ETriggerEvent::Started, this, &AShieldbreakCharacter::Input_Interact);
	EnhancedInput->BindAction(Inputs->Sprint, ETriggerEvent::Started, this, &AShieldbreakCharacter::Input_SprintStarted);
	EnhancedInput->BindAction(Inputs->Sprint, ETriggerEvent::Completed, this, &AShieldbreakCharacter::Input_SprintCompleted);
	EnhancedInput->BindAction(Inputs->CrouchSlide, ETriggerEvent::Started, this, &AShieldbreakCharacter::Input_CrouchStarted);
	EnhancedInput->BindAction(Inputs->CrouchSlide, ETriggerEvent::Completed, this, &AShieldbreakCharacter::Input_CrouchCompleted);
}

void AShieldbreakCharacter::Input_Move(const FInputActionValue& Value)
{
	if (!IsAlive() || !Controller)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Axis.X);
}

void AShieldbreakCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	// Zoomed aim is slower so the scope stays usable.
	const float Scale = bZoomed ? 0.55f : 1.f;
	AddControllerYawInput(Axis.X * Scale);
	AddControllerPitchInput(-Axis.Y * Scale);
}

void AShieldbreakCharacter::Input_JumpStarted() { Jump(); }
void AShieldbreakCharacter::Input_JumpCompleted() { StopJumping(); }
void AShieldbreakCharacter::Input_FireStarted() { StartFire(); }
void AShieldbreakCharacter::Input_FireCompleted() { StopFire(); }
void AShieldbreakCharacter::Input_Zoom() { ToggleZoom(); }
void AShieldbreakCharacter::Input_ShapeStarted() { SetShaping(true); }
void AShieldbreakCharacter::Input_ShapeCompleted() { SetShaping(false); }
void AShieldbreakCharacter::Input_Melee() { Melee(); }
void AShieldbreakCharacter::Input_Grenade() { ThrowGrenade(); }
void AShieldbreakCharacter::Input_ReloadVent() { ReloadOrVent(); }
void AShieldbreakCharacter::Input_Swap() { SwapWeapon(); }
void AShieldbreakCharacter::Input_Interact() { Interact(); }
void AShieldbreakCharacter::Input_SprintStarted() { SetSprinting(true); }
void AShieldbreakCharacter::Input_SprintCompleted() { SetSprinting(false); }
void AShieldbreakCharacter::Input_CrouchStarted() { StartCrouchOrSlide(); }
void AShieldbreakCharacter::Input_CrouchCompleted() { StopCrouchOrSlide(); }

bool AShieldbreakCharacter::CanJumpInternal_Implementation() const
{
	// Unlike the default, jumping is allowed while crouched (and so out of a slide).
	return IsAlive() && JumpIsAllowedInternal();
}

// ---- Actions ----

void AShieldbreakCharacter::StartFire()
{
	if (!IsAlive() || !CurrentWeapon)
	{
		return;
	}
	SetSprinting(false);
	CurrentWeapon->StartFire();
}

void AShieldbreakCharacter::StopFire()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFire();
	}
}

void AShieldbreakCharacter::SetSprinting(bool bSprint)
{
	if (UShieldbreakMovementComponent* Movement = GetShieldbreakMovement())
	{
		Movement->SetSprinting(bSprint && IsAlive());
	}
}

void AShieldbreakCharacter::SetShaping(bool bShape)
{
	if (ShieldHealth && (IsAlive() || !bShape))
	{
		ShieldHealth->SetShaping(bShape);
	}
}

void AShieldbreakCharacter::SetZoomed(bool bZoom)
{
	bZoomed = bZoom && IsAlive() && CurrentWeapon && CurrentWeapon->GetStats().ZoomFOV > 0.f;
}

void AShieldbreakCharacter::ToggleZoom()
{
	SetZoomed(!bZoomed);
}

void AShieldbreakCharacter::StartCrouchOrSlide()
{
	if (IsAlive())
	{
		Crouch();
	}
}

void AShieldbreakCharacter::StopCrouchOrSlide()
{
	UnCrouch();
}

void AShieldbreakCharacter::ReloadOrVent()
{
	if (!IsAlive())
	{
		return;
	}
	if (ShieldHealth && ShieldHealth->GetVentMarkerPosition() >= 0.f)
	{
		ShieldHealth->TryVent();
		return;
	}
	if (CurrentWeapon)
	{
		CurrentWeapon->StartReload();
	}
}

void AShieldbreakCharacter::Melee()
{
	if (!IsAlive())
	{
		return;
	}

	// Break Pulse is checked on every melee press, even during the melee cooldown.
	if (ShieldHealth)
	{
		ShieldHealth->TryBreakPulse();
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Now - LastLocalMeleeTime < MeleeCooldown)
	{
		return;
	}
	LastLocalMeleeTime = Now;
	SetSprinting(false);
	if (IsPlayerControlled())
	{
		FShieldbreakSFX::Play2D(this, EShieldbreakSound::Melee, 0.8f);
	}

	if (HasAuthority())
	{
		DoMelee();
	}
	else
	{
		ServerMelee();
	}
}

void AShieldbreakCharacter::ServerMelee_Implementation()
{
	DoMelee();
}

void AShieldbreakCharacter::DoMelee()
{
	using namespace ShieldbreakCharacterPrivate;

	UWorld* World = GetWorld();
	if (!World || !IsAlive())
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastServerMeleeTime < MeleeCooldown * 0.8f)
	{
		return;
	}
	LastServerMeleeTime = Now;

	const FVector Origin = GetActorLocation();
	const FVector AimForward = FRotator(0.f, GetAimRotation().Yaw, 0.f).Vector();

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectParams(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakMelee), false, this);
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(MeleeLungeRange + 40.f), Params);

	AShieldbreakCharacter* Best = nullptr;
	float BestScore = -1.f;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AShieldbreakCharacter* Candidate = Cast<AShieldbreakCharacter>(Overlap.GetActor());
		if (!Candidate || Candidate == this || !Candidate->IsAlive())
		{
			continue;
		}

		const FVector ToTarget = Candidate->GetActorLocation() - Origin;
		const float Dot = FVector::DotProduct(ToTarget.GetSafeNormal2D(), AimForward);
		if (Dot < MeleeConeDot)
		{
			continue;
		}

		// Line of sight at chest height.
		FHitResult Block;
		FCollisionQueryParams LosParams(SCENE_QUERY_STAT(ShieldbreakMeleeLOS), false, this);
		LosParams.AddIgnoredActor(Candidate);
		if (World->LineTraceSingleByChannel(Block, Origin, Candidate->GetActorLocation(), ECC_Visibility, LosParams))
		{
			continue;
		}

		const float Score = Dot - static_cast<float>(ToTarget.Size()) / (MeleeLungeRange * 4.f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	if (!Best)
	{
		MulticastMeleeFX(GetAimOrigin() + GetAimRotation().Vector() * MeleeRange * 0.6f, false);
		return;
	}

	const FVector ToBest = Best->GetActorLocation() - Origin;
	const float Distance = static_cast<float>(ToBest.Size2D()) - Best->GetCapsuleComponent()->GetScaledCapsuleRadius();
	if (Distance > MeleeRange)
	{
		// Lunge: close the gap. The hit still lands this swing (Halo-style magnetism).
		LaunchCharacter(ToBest.GetSafeNormal2D() * LungeSpeed, true, false);
	}

	// Assassination from behind.
	const FVector AttackDir = ToBest.GetSafeNormal2D();
	const FVector TargetForward = Best->GetActorForwardVector().GetSafeNormal2D();
	const bool bAssassination = FVector::DotProduct(TargetForward, AttackDir) > AssassinationDot;
	const float DamageAmount = bAssassination ? AssassinationDamage : MeleeDamage;

	const bool bWasAlive = Best->IsAlive();
	UGameplayStatics::ApplyDamage(Best, DamageAmount, GetController(), this, UDamageType_Melee::StaticClass());

	if (bWasAlive)
	{
		const bool bKill = !Best->IsAlive();
		if (AShieldbreakPlayerController* MyPC = Cast<AShieldbreakPlayerController>(GetController()))
		{
			MyPC->ClientHitConfirmed(bKill, false);
		}
		if (AShieldbreakPlayerController* VictimPC = Cast<AShieldbreakPlayerController>(Best->GetController()))
		{
			VictimPC->ClientDamagedFrom(FVector_NetQuantize(GetActorLocation()));
		}
	}

	MulticastMeleeFX(Best->GetActorLocation(), bAssassination);
}

void AShieldbreakCharacter::MulticastMeleeFX_Implementation(FVector_NetQuantize ImpactLocation, bool bAssassination)
{
	AShieldbreakTracer::SpawnBurst(GetWorld(), ImpactLocation, 8.f, bAssassination ? 70.f : 35.f, 0.15f,
		bAssassination ? FLinearColor(1.f, 0.1f, 0.05f) : FLinearColor(0.9f, 0.9f, 0.9f));
}

void AShieldbreakCharacter::ThrowGrenade()
{
	if (!IsAlive() || GrenadeCount <= 0)
	{
		return;
	}
	SetSprinting(false);

	if (HasAuthority())
	{
		DoThrowGrenade();
	}
	else
	{
		ServerThrowGrenade();
	}
}

void AShieldbreakCharacter::ServerThrowGrenade_Implementation()
{
	DoThrowGrenade();
}

void AShieldbreakCharacter::DoThrowGrenade()
{
	UWorld* World = GetWorld();
	if (!World || !IsAlive() || GrenadeCount <= 0 || !GrenadeClass)
	{
		return;
	}

	const FVector AimDir = GetAimRotation().Vector();
	// Slight upward bias so throws arc like Halo frags.
	const FVector ThrowDir = (AimDir + FVector(0.f, 0.f, 0.12f)).GetSafeNormal();
	const FVector SpawnLocation = GetAimOrigin() + AimDir * 60.f;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AShieldbreakGrenade* Grenade = World->SpawnActor<AShieldbreakGrenade>(GrenadeClass, SpawnLocation, AimDir.Rotation(), Params);
	if (Grenade)
	{
		Grenade->Launch(ThrowDir, GetVelocity());
		--GrenadeCount;
	}
}

void AShieldbreakCharacter::SwapWeapon()
{
	if (!IsAlive() || !GetHolsteredWeapon())
	{
		return;
	}
	StopFire();
	SetZoomed(false);

	if (HasAuthority())
	{
		DoSwapWeapon();
	}
	else
	{
		ServerSwapWeapon();
	}
}

void AShieldbreakCharacter::ServerSwapWeapon_Implementation()
{
	DoSwapWeapon();
}

void AShieldbreakCharacter::DoSwapWeapon()
{
	if (AShieldbreakWeapon* Other = GetHolsteredWeapon())
	{
		if (CurrentWeapon)
		{
			CurrentWeapon->StopFire();
		}
		EquipWeapon(Other);
	}
}

void AShieldbreakCharacter::Interact()
{
	if (!IsAlive())
	{
		return;
	}
	if (HasAuthority())
	{
		DoInteract();
	}
	else
	{
		ServerInteract();
	}
}

void AShieldbreakCharacter::ServerInteract_Implementation()
{
	DoInteract();
}

void AShieldbreakCharacter::DoInteract()
{
	TArray<AActor*> Overlapping;
	GetOverlappingActors(Overlapping, AShieldbreakWeaponPickup::StaticClass());
	for (AActor* Candidate : Overlapping)
	{
		if (AShieldbreakWeaponPickup* Pickup = Cast<AShieldbreakWeaponPickup>(Candidate))
		{
			if (Pickup->TryGiveTo(this))
			{
				return;
			}
		}
	}
}

// ---- Events ----

void AShieldbreakCharacter::HandleShieldChanged(float NewValue, float MaxValue)
{
	// Taking shield damage knocks you out of zoom (Halo descope).
	if (LastShieldValue >= 0.f && NewValue < LastShieldValue && IsLocallyControlled())
	{
		SetZoomed(false);
	}
	LastShieldValue = NewValue;
}

void AShieldbreakCharacter::HandleBreakPulse()
{
	float Radius = 400.f;
	if (ShieldHealth)
	{
		if (const UShieldTuningData* Tuning = ShieldHealth->GetTuningData())
		{
			Radius = Tuning->BreakPulseRadius;
		}
	}
	AShieldbreakTracer::SpawnBurst(GetWorld(), GetActorLocation(), 40.f, Radius, 0.35f, FLinearColor(0.3f, 0.6f, 1.f));
}

void AShieldbreakCharacter::HandleDeath(AController* Killer)
{
	if (bDeathHandled)
	{
		return;
	}
	bDeathHandled = true;

	bZoomed = false;
	StopFire();
	SetSprinting(false);

	// Visuals on every machine: the mannequin ragdolls, or the greybox body collapses into physics debris.
	ShapeShieldMesh->SetVisibility(false);
	const bool bRagdoll = BodyAnimator && BodyAnimator->HasBodyArt();
	if (bRagdoll)
	{
		BodyAnimator->StartRagdoll(FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 150.f));
		HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Piece : { BodyMesh.Get(), HeadMesh.Get(), VisorMesh.Get() })
	{
		if (bRagdoll)
		{
			break;
		}
		if (!Piece)
		{
			continue;
		}
		Piece->SetOwnerNoSee(false);
		Piece->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Piece->SetCollisionObjectType(ECC_PhysicsBody);
		Piece->SetCollisionResponseToAllChannels(ECR_Block);
		Piece->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Piece->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Piece->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
		Piece->SetSimulatePhysics(true);
		Piece->AddImpulse(FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 200.f), NAME_None, true);
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// A local player keeps watching their body.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->IsLocalController())
		{
			PC->SetViewTarget(this);
		}
	}

	if (!HasAuthority())
	{
		return;
	}

	AController* VictimController = GetController();
	if (UWorld* World = GetWorld())
	{
		if (AShieldbreakGameMode* GameMode = World->GetAuthGameMode<AShieldbreakGameMode>())
		{
			GameMode->NotifyKilled(Killer, VictimController, this);
		}
	}

	DetachFromControllerPendingDestroy();
	SetLifeSpan(6.f);
	DestroyInventory();
}
