#include "Weapons/ShieldbreakWeapon.h"

#include "Components/AudioComponent.h"

#include "Shieldbreak.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/ShieldbreakCharacter.h"
#include "Player/ShieldbreakPlayerController.h"
#include "Shield/ShieldHealthComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/ShieldbreakDamageTypes.h"
#include "Weapons/ShieldbreakTracer.h"
#include "Weapons/ShieldbreakRocket.h"
#include "Audio/ShieldbreakSFX.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMeshSocket.h"
#include "Visuals/ShieldbreakArt.h"
#include "Visuals/ShieldbreakBodyAnimator.h"
#include "NiagaraComponent.h"
#include "EngineUtils.h"
#include "Game/ShieldbreakTeams.h"

namespace ShieldbreakWeaponPrivate
{
	// Halo-style viewmodel: low on the right, angled slightly in toward the crosshair.
	const FVector EquippedOffset(46.f, 13.f, -15.f);
	const FRotator EquippedRotation(1.5f, -4.f, 0.f);
	constexpr float OriginTolerance = 150.f;
	constexpr float DirectionToleranceDegrees = 5.f;
	constexpr float RateTolerance = 1.25f;
	constexpr float HeatIdleDelay = 0.25f;
	/** Bullet magnetism never bends a shot that passes farther than this (cm) from the body line. */
	constexpr float MagnetismMaxMiss = 40.f;
	/** How far from the body line a shot still hits anyway (the capsule's radius). */
	constexpr float MagnetismBodyRadius = 38.f;
}

AShieldbreakWeapon::AShieldbreakWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	bNetUseOwnerRelevancy = true;
	SetCanBeDamaged(false);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->SetCastShadow(false);
	RootComponent = WeaponMesh;

	BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
	BarrelMesh->SetupAttachment(WeaponMesh);
	BarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrelMesh->SetGenerateOverlapEvents(false);
	BarrelMesh->SetCastShadow(false);

	ChargeGlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChargeGlowMesh"));
	ChargeGlowMesh->SetupAttachment(WeaponMesh);
	ChargeGlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChargeGlowMesh->SetGenerateOverlapEvents(false);
	ChargeGlowMesh->SetCastShadow(false);
	ChargeGlowMesh->SetUsingAbsoluteScale(true);
	ChargeGlowMesh->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		ChargeGlowMesh->SetStaticMesh(SphereFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	if (CubeFinder.Succeeded())
	{
		WeaponMesh->SetStaticMesh(CubeFinder.Object);
	}
	if (CylinderFinder.Succeeded())
	{
		BarrelMesh->SetStaticMesh(CylinderFinder.Object);
	}
	if (MaterialFinder.Succeeded())
	{
		WeaponMesh->SetMaterial(0, MaterialFinder.Object);
		BarrelMesh->SetMaterial(0, MaterialFinder.Object);
	}

	Stats.DamageType = UDamageType_Ballistic::StaticClass();
}

void AShieldbreakWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AShieldbreakWeapon, AmmoInMagazine, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AShieldbreakWeapon, ReserveAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AShieldbreakWeapon, Heat, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AShieldbreakWeapon, bOverheated, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AShieldbreakWeapon, bReloading, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AShieldbreakWeapon, bCharging, COND_SkipOwner);
	// Every machine attaches the gun itself in OnEquipped (to the body, or to the owner's local first-person arms,
	// which other machines don't have), so the server's attachment must not replicate.
	DISABLE_REPLICATED_PRIVATE_PROPERTY(AActor, AttachmentReplication);
}

void AShieldbreakWeapon::BeginPlay()
{
	Super::BeginPlay();

	// Body is a stretched cube; the barrel is a thin cylinder poking out the front.
	WeaponMesh->SetRelativeScale3D(Stats.MeshScale);
	const float BodyLength = Stats.MeshScale.X * 100.f;
	BarrelMesh->SetUsingAbsoluteScale(true);
	BarrelMesh->SetWorldScale3D(FVector(0.04f, 0.04f, 0.3f));
	BarrelMesh->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	// Relative location is in the parent's scaled space, so undo the body scale.
	BarrelMesh->SetRelativeLocation(FVector((BodyLength * 0.5f + 12.f) / FMath::Max(Stats.MeshScale.X, 0.01f), 0.f, 0.f));

	BodyMID = WeaponMesh->CreateDynamicMaterialInstance(0);
	if (BodyMID)
	{
		BodyMID->SetVectorParameterValue(TEXT("Color"), Stats.Color);
	}
	if (UMaterialInstanceDynamic* BarrelMID = BarrelMesh->CreateDynamicMaterialInstance(0))
	{
		BarrelMID->SetVectorParameterValue(TEXT("Color"), Stats.TracerColor * 0.6f);
	}

	ApplyArt();

	CurrentSpread = Stats.BaseSpreadDegrees;

	if (HasAuthority())
	{
		AmmoInMagazine = Stats.bUsesHeat ? 0 : Stats.MagazineSize;
		ReserveAmmo = Stats.bUsesHeat ? 0 : Stats.MaxReserveAmmo;
		ServerShotTokens = static_cast<float>(FMath::Max(1, Stats.BurstCount)) + 1.f;
	}

	// Hidden until equipped (clients may see the actor before its attachment).
	const AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
	if (!OwnerCharacter || OwnerCharacter->GetCurrentWeapon() != this)
	{
		SetActorHiddenInGame(true);
	}
}

AShieldbreakCharacter* AShieldbreakWeapon::GetOwningCharacter() const
{
	return Cast<AShieldbreakCharacter>(GetOwner());
}

void AShieldbreakWeapon::SetOwningCharacter(AShieldbreakCharacter* NewOwner)
{
	SetOwner(NewOwner);
	SetInstigator(NewOwner);
}

AShieldbreakWeapon::EEquippedLayout AShieldbreakWeapon::GetDesiredLayout() const
{
	const AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
	if (!OwnerCharacter || !OwnerCharacter->GetFirstPersonCamera())
	{
		return EEquippedLayout::None;
	}

	// Everyone else sees the imported gun in the mannequin's hand; the local player gets a camera viewmodel.
	const bool bViewModel = OwnerCharacter->IsLocallyControlled() && OwnerCharacter->IsPlayerControlled();
	const USkeletalMeshComponent* Body = OwnerCharacter->GetMesh();
	if (bHasArt && !bViewModel && Body && Body->GetSkeletalMeshAsset())
	{
		return EEquippedLayout::Hand;
	}
	// The local player's first-person arms hold the gun the same way the body does.
	const UShieldbreakBodyAnimator* Animator = OwnerCharacter->GetBodyAnimator();
	if (bHasArt && bViewModel && Animator && Animator->HasFirstPersonArms())
	{
		return EEquippedLayout::FirstPersonHand;
	}
	return bHasArt ? EEquippedLayout::CameraArt : EEquippedLayout::CameraPlain;
}

bool AShieldbreakWeapon::NeedsReattach() const
{
	return GetDesiredLayout() != EquippedLayout;
}

void AShieldbreakWeapon::OnEquipped()
{
	if (EquippedLayout == EEquippedLayout::None)
	{
		const AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
		if (OwnerCharacter && OwnerCharacter->IsLocallyControlled() && OwnerCharacter->IsPlayerControlled())
		{
			FShieldbreakSFX::Play2D(this, EShieldbreakSound::WeaponRaise, 0.5f);
		}
	}
	AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
	EquippedLayout = GetDesiredLayout();
	if (EquippedLayout == EEquippedLayout::None)
	{
		return;
	}

	if (EquippedLayout == EEquippedLayout::Hand || EquippedLayout == EEquippedLayout::FirstPersonHand)
	{
		const bool bFirstPerson = EquippedLayout == EEquippedLayout::FirstPersonHand;
		USkeletalMeshComponent* Body = bFirstPerson ? OwnerCharacter->GetBodyAnimator()->GetFirstPersonArms() : OwnerCharacter->GetMesh();
		const FName HandSocket = Body->DoesSocketExist(TEXT("weapon_r")) ? FName(TEXT("weapon_r")) : FName(TEXT("hand_r"));
		AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocket);
		SetActorRelativeTransform(FTransform::Identity);
		// The body's shadow already carries a gun; the first-person one would add a second.
		WeaponMesh->SetCastShadow(!bFirstPerson);
	}
	else
	{
		AttachToComponent(OwnerCharacter->GetFirstPersonCamera(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		if (bHasArt)
		{
			// Align the barrel with the view, scale to viewmodel size and center the mesh on the grip offset.
			const FQuat ViewRotation = ShieldbreakWeaponPrivate::EquippedRotation.Quaternion() * ArtAlign.Quaternion();
			SetActorRelativeRotation(ViewRotation);
			SetActorRelativeScale3D(FVector(ArtViewScale));
			SetActorRelativeLocation(ShieldbreakWeaponPrivate::EquippedOffset - ViewRotation.RotateVector(ArtCenterLocal * ArtViewScale));
		}
		else
		{
			SetActorRelativeLocation(ShieldbreakWeaponPrivate::EquippedOffset);
			SetActorRelativeRotation(ShieldbreakWeaponPrivate::EquippedRotation);
		}
		WeaponMesh->SetCastShadow(false);
	}
	// The local player's gun draws with the first-person FOV and scale, like the arms holding it.
	const EFirstPersonPrimitiveType ViewType = EquippedLayout == EEquippedLayout::Hand ? EFirstPersonPrimitiveType::None : EFirstPersonPrimitiveType::FirstPerson;
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive->FirstPersonPrimitiveType != ViewType)
		{
			Primitive->FirstPersonPrimitiveType = ViewType;
			Primitive->MarkRenderStateDirty();
		}
		// In first person the gun also takes the camera's fill light, like the arms (ShieldbreakBodyAnimator).
		const bool bFillLit = ViewType == EFirstPersonPrimitiveType::FirstPerson;
		if (Primitive->LightingChannels.bChannel1 != bFillLit)
		{
			Primitive->SetLightingChannels(true, bFillLit, Primitive->LightingChannels.bChannel2);
		}
	}
	SetActorHiddenInGame(false);
	CurrentSpread = Stats.BaseSpreadDegrees;
}

void AShieldbreakWeapon::OnUnequipped()
{
	EquippedLayout = EEquippedLayout::None;
	StopWeaponAudio(ReloadAudio);
	StopWeaponAudio(ChargeAudio);
	CancelCharge();
	StopFire();
	BurstShotsRemaining = 0;
	SetActorHiddenInGame(true);

	if (bReloading && HasAuthority())
	{
		// Swapping cancels the reload.
		GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
		bReloading = false;
	}
	ReloadStartLocalTime = -1.f;
}

// ---- Local fire loop ----

void AShieldbreakWeapon::StartFire()
{
	if (Stats.FireMode == EShieldbreakFireMode::Charge)
	{
		const UWorld* World = GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.f;
		if (!bCharging && CanFireShotLocal() && Now >= NextShotLocalTime)
		{
			bCharging = true;
			ChargeStartLocalTime = Now;
			ServerSetCharging(true);
			OnRep_Charging(); // local tell (sound + glow)
		}
		else if (!HasAmmo() && CanReload())
		{
			StartReload();
		}
		else if (!HasAmmo())
		{
			PlayDryFire();
		}
		return;
	}

	bWantsToFire = true;
	bSemiAutoLatched = false;

	if (!HasAmmo() && !Stats.bUsesHeat)
	{
		if (CanReload())
		{
			StartReload();
		}
		else if (!bReloading && ReloadStartLocalTime < 0.f)
		{
			PlayDryFire();
		}
	}
}

void AShieldbreakWeapon::PlayDryFire()
{
	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() - LastDryFireTime > 0.15f && Sounds.DryFire != EShieldbreakSound::Count)
	{
		LastDryFireTime = World->GetTimeSeconds();
		FShieldbreakSFX::Play2D(this, Sounds.DryFire, 0.7f);
	}
}

void AShieldbreakWeapon::StopWeaponAudio(TWeakObjectPtr<UAudioComponent>& Audio)
{
	if (UAudioComponent* Component = Audio.Get())
	{
		Component->FadeOut(0.06f, 0.f);
	}
	Audio.Reset();
}

void AShieldbreakWeapon::PlayReloadSound(bool bLocalView)
{
	if (Sounds.Reload == EShieldbreakSound::Count)
	{
		return;
	}
	StopWeaponAudio(ReloadAudio);
	ReloadAudio = bLocalView
		? FShieldbreakSFX::Play2D(this, Sounds.Reload, 0.65f, 1.f)
		: FShieldbreakSFX::PlayAttached(GetRootComponent(), Sounds.Reload, 0.8f, 1.f);
}

void AShieldbreakWeapon::StopFire()
{
	if (bCharging)
	{
		ReleaseCharge();
	}
	bWantsToFire = false;
	bSemiAutoLatched = false;
}

bool AShieldbreakWeapon::HasAmmo() const
{
	return Stats.bUsesHeat ? !bOverheated : AmmoInMagazine > 0;
}

bool AShieldbreakWeapon::CanFireShotLocal() const
{
	const AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
	if (!OwnerCharacter || !OwnerCharacter->IsAlive() || OwnerCharacter->GetCurrentWeapon() != this)
	{
		return false;
	}
	if (bReloading || ReloadStartLocalTime >= 0.f)
	{
		return false;
	}
	if (Stats.bUsesHeat)
	{
		const UWorld* World = GetWorld();
		const bool bLocallyLocked = World && OverheatEndLocalTime > World->GetTimeSeconds();
		return !bOverheated && !bLocallyLocked;
	}
	return AmmoInMagazine > 0;
}

void AShieldbreakWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	const AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
	if (!World || !OwnerCharacter)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();

	// Heat cools on every machine that simulates it (server authoritative, owner predicted).
	if (Stats.bUsesHeat && Now - LastShotLocalTime > ShieldbreakWeaponPrivate::HeatIdleDelay)
	{
		Heat = FMath::Max(0.f, Heat - Stats.HeatCooldownPerSecond * DeltaSeconds);
	}
	if (HasAuthority() && bOverheated && GetServerTime() >= ServerOverheatEndTime)
	{
		bOverheated = false;
	}

	UpdateChargeGlow();

	if (!OwnerCharacter->IsLocallyControlled())
	{
		return;
	}

	// Spread recovers toward the base value.
	CurrentSpread = FMath::Max(Stats.BaseSpreadDegrees, CurrentSpread - Stats.SpreadRecoveryPerSecond * DeltaSeconds);

	// Local reload finished (server timer is authoritative; this only drives the HUD).
	if (ReloadStartLocalTime >= 0.f && Now - ReloadStartLocalTime >= Stats.ReloadTime + 0.3f && !bReloading)
	{
		ReloadStartLocalTime = -1.f;
	}

	// Charge weapons fire on release (StopFire), never from the loop.
	if (Stats.FireMode == EShieldbreakFireMode::Charge)
	{
		return;
	}

	// Fire loop. Several shots can be due in one frame at low frame rates.
	int32 Guard = 0;
	while (Now >= NextShotLocalTime && Guard++ < 4)
	{
		if (BurstShotsRemaining > 0)
		{
			if (!CanFireShotLocal())
			{
				BurstShotsRemaining = 0;
				break;
			}
			FireShotLocal();
			--BurstShotsRemaining;
			NextShotLocalTime = FMath::Max(NextShotLocalTime, Now - DeltaSeconds)
				+ (BurstShotsRemaining > 0 ? Stats.BurstInterval : Stats.TimeBetweenShots);
			continue;
		}

		if (!bWantsToFire || bSemiAutoLatched)
		{
			break;
		}

		if (!CanFireShotLocal())
		{
			if (!Stats.bUsesHeat && AmmoInMagazine <= 0 && CanReload())
			{
				StartReload();
			}
			break;
		}

		switch (Stats.FireMode)
		{
		case EShieldbreakFireMode::Burst:
			BurstShotsRemaining = FMath::Max(1, Stats.BurstCount);
			continue;
		case EShieldbreakFireMode::SemiAuto:
			bSemiAutoLatched = true;
			break;
		default:
			break;
		}

		FireShotLocal();
		NextShotLocalTime = FMath::Max(NextShotLocalTime, Now - DeltaSeconds) + Stats.TimeBetweenShots;
	}
}

FVector AShieldbreakWeapon::GetMuzzleLocation() const
{
	if (bHasArt)
	{
		if (MuzzleSocket != NAME_None)
		{
			return WeaponMesh->GetSocketLocation(MuzzleSocket);
		}
		const FVector Center = WeaponMesh->GetComponentTransform().TransformPosition(ArtCenterLocal);
		return Center + GetBarrelDirection() * ArtHalfLength * WeaponMesh->GetComponentScale().X;
	}
	return BarrelMesh ? BarrelMesh->GetComponentLocation() + GetActorForwardVector() * 15.f : GetActorLocation();
}

float AShieldbreakWeapon::GetCurrentSpreadDegrees() const
{
	const AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
	return CurrentSpread * (OwnerCharacter ? OwnerCharacter->GetSpreadMultiplier() : 1.f);
}

void AShieldbreakWeapon::FireShotLocal()
{
	AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
	UWorld* World = GetWorld();
	if (!OwnerCharacter || !World)
	{
		return;
	}

	const FVector Origin = OwnerCharacter->GetAimOrigin();
	const FVector AimDir = OwnerCharacter->GetAimRotation().Vector();
	const float HalfAngle = FMath::DegreesToRadians(GetCurrentSpreadDegrees());
	const FVector SpreadDir = HalfAngle > 0.f ? FMath::VRandCone(AimDir, HalfAngle) : AimDir;
	// Player shots bend onto an enemy they narrowly miss (bots aim with their own error model).
	const FVector Direction = OwnerCharacter->IsPlayerControlled() ? ApplyMagnetism(Origin, SpreadDir) : SpreadDir;

	// Predict ammo / heat locally; the server's replicated values take over.
	LastShotLocalTime = World->GetTimeSeconds();
	// (On the server ServerFireShot below updates the real values.)
	if (!HasAuthority())
	{
		if (Stats.bUsesHeat)
		{
			Heat = FMath::Min(1.f, Heat + Stats.HeatPerShot);
			if (Heat >= 1.f)
			{
				OverheatEndLocalTime = LastShotLocalTime + Stats.OverheatLockout;
			}
		}
		else
		{
			AmmoInMagazine = FMath::Max(0, AmmoInMagazine - 1);
		}
	}

	CurrentSpread = FMath::Min(Stats.MaxSpreadDegrees, CurrentSpread + Stats.SpreadPerShot);

	// Cosmetic tracer to where this machine thinks the shot lands.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakCosmeticShot), true, OwnerCharacter);
	Params.AddIgnoredActor(this);
	const FVector End = Origin + Direction * Stats.Range;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Weapon, Params);
	if (OwnerCharacter->IsPlayerControlled())
	{
		if (!Stats.ProjectileClass)
		{
			AShieldbreakTracer::Spawn(World, GetMuzzleLocation(), bHit ? Hit.ImpactPoint : End, Stats.TracerColor);
			if (bHit)
			{
				SpawnImpactFX(Hit);
			}
		}
		// No recoil: the gun model jumps in the hands, the aim never moves.
		PlayShotEffects(true);
	}

	ServerFireShot(Origin, Direction);
}

FVector AShieldbreakWeapon::ApplyMagnetism(const FVector& Origin, const FVector& Direction) const
{
	using namespace ShieldbreakWeaponPrivate;
	const AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
	UWorld* World = GetWorld();
	if (!OwnerCharacter || !World || Stats.MagnetismDegrees <= 0.f || Stats.MagnetismRange <= 0.f || !OwnerCharacter->IsAimAssistEnabled())
	{
		return Direction;
	}
	const FVector Dir = Direction.GetSafeNormal();

	// A shot that already hits someone (or something closer) is left exactly as aimed: magnetism never pulls a
	// headshot onto the body, and never bends a shot around a wall.
	FHitResult DirectHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakMagnetism), true, OwnerCharacter);
	Params.AddIgnoredActor(this);
	const bool bDirectHit = World->LineTraceSingleByChannel(DirectHit, Origin, Origin + Dir * Stats.Range, ECC_Weapon, Params);
	if (bDirectHit && Cast<AShieldbreakCharacter>(DirectHit.GetActor()))
	{
		return Direction;
	}
	const float BlockedAt = bDirectHit ? static_cast<float>(DirectHit.Distance) : Stats.Range;

	const float Range = FMath::Min(Stats.MagnetismRange, Stats.Range);
	const AShieldbreakCharacter* Best = nullptr;
	FVector BestPoint = FVector::ZeroVector;
	float BestScore = 1.f;
	for (TActorIterator<AShieldbreakCharacter> It(World); It; ++It)
	{
		const AShieldbreakCharacter* Other = *It;
		if (Other == OwnerCharacter || !Other->IsAlive() || Other->IsHidden() || ShieldbreakTeams::AreTeammates(OwnerCharacter, Other))
		{
			continue;
		}
		// The body as a line from the hips to the neck (the capsule's straight part). The shot's closest pass to that
		// line is its miss distance. The line stops below the head on purpose: magnetism finds the body, headshots
		// still have to be aimed.
		const FVector Center = Other->GetActorLocation();
		const FVector Bottom = Center - FVector(0.f, 0.f, 50.f);
		const FVector Top = (Other->GetHeadMesh() ? Other->GetHeadMesh()->GetComponentLocation() : Center + FVector(0.f, 0.f, 72.f)) - FVector(0.f, 0.f, 20.f);
		FVector OnRay, OnBody;
		FMath::SegmentDistToSegmentSafe(Origin, Origin + Dir * Range, Bottom, Top, OnRay, OnBody);
		const float Distance = static_cast<float>(FVector::Dist(Origin, OnBody));
		if (Distance < 200.f || Distance > Range || Distance > BlockedAt + 150.f)
		{
			continue;
		}
		// Extra reach past the body's edge: MagnetismDegrees wide, never more than MagnetismMaxMiss, at full strength
		// to half range and then fading out, so long shots still need a steady hand.
		const float Falloff = FMath::Clamp(2.f * (1.f - Distance / Range), 0.f, 1.f);
		const float Reach = FMath::Min(Distance * FMath::Tan(FMath::DegreesToRadians(Stats.MagnetismDegrees)), MagnetismMaxMiss) * Falloff;
		const float Miss = static_cast<float>(FVector::Dist(OnRay, OnBody)) - MagnetismBodyRadius;
		if (Reach <= 1.f || Miss >= Reach)
		{
			continue;
		}
		const float Score = FMath::Max(0.f, Miss) / Reach;
		if (Score >= BestScore)
		{
			continue;
		}
		// The bent path must be clear too.
		FHitResult Blocker;
		FCollisionQueryParams ClearParams(SCENE_QUERY_STAT(ShieldbreakMagnetismClear), false, OwnerCharacter);
		ClearParams.AddIgnoredActor(this);
		ClearParams.AddIgnoredActor(Other);
		if (World->LineTraceSingleByChannel(Blocker, Origin, OnBody, ECC_Visibility, ClearParams))
		{
			continue;
		}
		// Bend only as far as needed: to just inside the body's edge on the side the shot passed.
		const FVector Side = (OnRay - OnBody).GetSafeNormal();
		Best = Other;
		BestPoint = OnBody + Side * FMath::Min(MagnetismBodyRadius - 10.f, static_cast<float>(FVector::Dist(OnRay, OnBody)));
		BestScore = Score;
	}
	return Best ? (BestPoint - Origin).GetSafeNormal() : Direction;
}

// ---- Server ----

void AShieldbreakWeapon::ServerFireShot_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction)
{
	using namespace ShieldbreakWeaponPrivate;

	AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
	UWorld* World = GetWorld();
	if (!OwnerCharacter || !World || !OwnerCharacter->IsAlive() || OwnerCharacter->GetCurrentWeapon() != this
		|| OwnerCharacter->IsAegisRaised() || !OwnerCharacter->CanFireWhileCarrying())
	{
		return;
	}

	const float Now = GetServerTime();

	// Charge weapons: only a full charge (timed on the server, so it can't be faked) does full damage.
	float ChargeDamageScale = 1.f;
	if (Stats.FireMode == EShieldbreakFireMode::Charge)
	{
		const float Charged = ServerChargeStartTime < 0.f ? 0.f : (Now - ServerChargeStartTime) / FMath::Max(Stats.ChargeTime, 0.01f);
		// Small allowance for the start/release RPCs arriving with different delays.
		ChargeDamageScale = Charged + 0.1f >= 1.f ? 1.f : Stats.MinChargeDamageFraction;
		ServerChargeStartTime = -1.f;
		bCharging = false;
	}

	// Rate: a token bucket refilled a bit faster than the weapon's real sustained rate.
	const int32 Burst = Stats.FireMode == EShieldbreakFireMode::Burst ? FMath::Max(1, Stats.BurstCount) : 1;
	const float CycleTime = Stats.FireMode == EShieldbreakFireMode::Burst
		? Stats.TimeBetweenShots + Stats.BurstInterval * (Burst - 1)
		: Stats.TimeBetweenShots;
	const float ShotsPerSecond = static_cast<float>(Burst) / FMath::Max(0.01f, CycleTime);
	const float MaxTokens = static_cast<float>(Burst) + 1.f;
	if (LastServerTokenTime >= 0.f)
	{
		ServerShotTokens = FMath::Min(MaxTokens, ServerShotTokens + (Now - LastServerTokenTime) * ShotsPerSecond * RateTolerance);
	}
	LastServerTokenTime = Now;
	if (ServerShotTokens < 1.f)
	{
		return;
	}
	ServerShotTokens -= 1.f;

	if (bReloading)
	{
		return;
	}

	// Ammo / heat.
	if (Stats.bUsesHeat)
	{
		if (bOverheated)
		{
			return;
		}
		Heat = FMath::Min(1.f, Heat + Stats.HeatPerShot);
		if (Heat >= 1.f)
		{
			bOverheated = true;
			ServerOverheatEndTime = Now + Stats.OverheatLockout;
		}
	}
	else
	{
		if (AmmoInMagazine <= 0)
		{
			return;
		}
		--AmmoInMagazine;
	}
	LastServerShotTime = Now;
	LastShotLocalTime = World->GetTimeSeconds();

	// Origin and direction must be close to what the server believes.
	const FVector ServerOrigin = OwnerCharacter->GetAimOrigin();
	const float OriginSlack = OriginTolerance + OwnerCharacter->GetVelocity().Size() * 0.25f;
	if (FVector::DistSquared(FVector(Origin), ServerOrigin) > FMath::Square(OriginSlack))
	{
		UE_LOG(LogShieldbreak, Verbose, TEXT("Rejected shot from %s: origin too far."), *OwnerCharacter->GetName());
		return;
	}

	const FVector ShotDir = FVector(Direction).GetSafeNormal();
	const FVector ServerAim = OwnerCharacter->GetAimRotation().Vector();
	const float MaxDeviation = Stats.MaxSpreadDegrees * 2.f + Stats.MagnetismDegrees + DirectionToleranceDegrees;
	if (FVector::DotProduct(ShotDir, ServerAim) < FMath::Cos(FMath::DegreesToRadians(MaxDeviation)))
	{
		UE_LOG(LogShieldbreak, Verbose, TEXT("Rejected shot from %s: direction off aim."), *OwnerCharacter->GetName());
		return;
	}

	if (Stats.ProjectileClass)
	{
		// Spawned a little ahead of the eye so it clears the shooter's own capsule.
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = OwnerCharacter;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector SpawnLocation = FVector(Origin) + ShotDir * 80.f;
		AActor* Projectile = World->SpawnActor<AActor>(Stats.ProjectileClass, SpawnLocation, ShotDir.Rotation(), SpawnParams);
		if (AShieldbreakRocket* Rocket = Cast<AShieldbreakRocket>(Projectile))
		{
			Rocket->Launch(ShotDir);
		}
		MulticastShotFX(SpawnLocation);
		return;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakServerShot), true, OwnerCharacter);
	Params.AddIgnoredActor(this);
	const FVector End = FVector(Origin) + ShotDir * Stats.Range;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Weapon, Params);
	const FVector ImpactPoint = bHit ? FVector(Hit.ImpactPoint) : End;

	// A raised Aegis barrier soaks the bullet instead of its owner.
	AShieldbreakCharacter* BarrierOwner = bHit ? Cast<AShieldbreakCharacter>(Hit.GetActor()) : nullptr;
	if (BarrierOwner && Hit.GetComponent() && Hit.GetComponent() == BarrierOwner->GetAegisMesh())
	{
		BarrierOwner->AbsorbWithAegis(Stats.Damage * ChargeDamageScale, Hit.ImpactPoint);
	}
	else if (bHit && Hit.GetActor())
	{
		AActor* HitActor = Hit.GetActor();
		AShieldbreakCharacter* Victim = Cast<AShieldbreakCharacter>(HitActor);
		const bool bVictimWasAlive = Victim && Victim->IsAlive();

		float DamageAmount = Stats.Damage * ChargeDamageScale;
		bool bHeadshot = false;
		// The head sphere sits mostly inside the body capsule, so a level shot usually hits the
		// capsule first. Test the same ray against the head on its own.
		bool bHitHead = Victim && Hit.GetComponent() == Victim->GetHeadMesh();
		if (Victim && !bHitHead && Victim->GetHeadMesh())
		{
			FHitResult HeadHit;
			FCollisionQueryParams HeadParams(SCENE_QUERY_STAT(ShieldbreakHeadCheck), true);
			bHitHead = Victim->GetHeadMesh()->LineTraceComponent(HeadHit, FVector(Origin), FVector(Hit.ImpactPoint) + ShotDir * 120.f, HeadParams);
		}
		if (Victim && Stats.bPrecision && bHitHead)
		{
			const UShieldHealthComponent* VictimShield = Victim->GetShieldHealth();
			if (VictimShield && (Stats.bHeadshotIgnoresShields || VictimShield->GetShield() + VictimShield->GetOvershield() <= 0.f))
			{
				DamageAmount *= Stats.HeadshotMultiplier;
				bHeadshot = true;
			}
		}

		AController* InstigatorController = OwnerCharacter->GetController();
		TSubclassOf<UDamageType> DamageClass = Stats.DamageType ? Stats.DamageType : TSubclassOf<UDamageType>(UDamageType_Ballistic::StaticClass());
		if (Victim && Victim->GetShieldHealth())
		{
			Victim->GetShieldHealth()->NoteHeadshot(bHeadshot);
		}
		UGameplayStatics::ApplyPointDamage(HitActor, DamageAmount, ShotDir, Hit, InstigatorController, this, DamageClass);

		if (bVictimWasAlive)
		{
			// The hit marker comes from the victim's shield component (every damage source shares it).
			if (AShieldbreakPlayerController* VictimPC = Cast<AShieldbreakPlayerController>(Victim->GetController()))
			{
				VictimPC->ClientDamagedFrom(FVector_NetQuantize(OwnerCharacter->GetActorLocation()));
			}
		}
	}

	MulticastShotFX(ImpactPoint);
}

void AShieldbreakWeapon::MulticastShotFX_Implementation(FVector_NetQuantize End)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// The shooting player already drew this locally.
	const AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
	if (OwnerCharacter && OwnerCharacter->IsLocallyControlled() && OwnerCharacter->IsPlayerControlled())
	{
		return;
	}

	const FVector Muzzle = GetMuzzleLocation();
	PlayShotEffects(false);
	if (Stats.ProjectileClass)
	{
		return; // the replicated projectile is its own tracer
	}
	AShieldbreakTracer::Spawn(World, Muzzle, End, Stats.TracerColor);

	// Only the end point is replicated; a short trace there recovers the surface for the impact effect.
	const FVector ShotDir = (FVector(End) - Muzzle).GetSafeNormal();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakImpactFX), false, GetOwner());
	Params.AddIgnoredActor(this);
	if (World->LineTraceSingleByChannel(Hit, FVector(End) - ShotDir * 20.f, FVector(End) + ShotDir * 20.f, ECC_Weapon, Params))
	{
		SpawnImpactFX(Hit);
	}
}

UStaticMesh* AShieldbreakWeapon::LoadArtMesh(const FShieldbreakWeaponStats& WeaponStats)
{
	UStaticMesh* Art = WeaponStats.ArtMesh.IsNull() ? nullptr : ShieldbreakArt::Load<UStaticMesh>(*WeaponStats.ArtMesh.ToString());
	if (!Art && !WeaponStats.ArtMeshFallback.IsNull())
	{
		Art = ShieldbreakArt::Load<UStaticMesh>(*WeaponStats.ArtMeshFallback.ToString());
	}
	return Art;
}

void AShieldbreakWeapon::ApplyArt()
{
	UStaticMesh* Art = LoadArtMesh(Stats);
	if (!Art)
	{
		return;
	}

	WeaponMesh->SetStaticMesh(Art);
	WeaponMesh->EmptyOverrideMaterials();
	WeaponMesh->SetRelativeScale3D(FVector::OneVector);
	BarrelMesh->SetVisibility(false);
	BodyMID = nullptr;
	bHasArt = true;

	static const FName SocketCandidates[] = { TEXT("Muzzle"), TEXT("MuzzleFlash"), TEXT("muzzle"), TEXT("Muzzle_01") };
	for (const FName& Candidate : SocketCandidates)
	{
		if (Art->FindSocket(Candidate))
		{
			MuzzleSocket = Candidate;
			break;
		}
	}

	// Barrel axis: toward the muzzle socket if there is one, else the longest horizontal extent.
	const FBox Box = Art->GetBoundingBox();
	const FVector Extent = Box.GetExtent();
	ArtCenterLocal = Box.GetCenter();
	FVector Barrel = Extent.X >= Extent.Y ? FVector::ForwardVector : FVector::RightVector;
	if (const UStaticMeshSocket* Socket = MuzzleSocket != NAME_None ? Art->FindSocket(MuzzleSocket) : nullptr)
	{
		const FVector ToMuzzle = FVector(Socket->RelativeLocation.X - ArtCenterLocal.X, Socket->RelativeLocation.Y - ArtCenterLocal.Y, 0.f);
		if (!ToMuzzle.IsNearlyZero())
		{
			Barrel = ToMuzzle.GetSafeNormal();
		}
	}
	ArtBarrelLocal = Barrel;
	ArtAlign = FRotator(0.f, -FMath::RadiansToDegrees(FMath::Atan2(Barrel.Y, Barrel.X)), 0.f);
	ArtHalfLength = FMath::Max(FMath::Abs(Barrel.X) * Extent.X + FMath::Abs(Barrel.Y) * Extent.Y, 1.f);
	ArtViewScale = Stats.ArtViewLength / (2.f * ArtHalfLength);

	UE_LOG(LogShieldbreak, Log, TEXT("%s art: %s, muzzle socket %s, barrel %s, view scale %.3f"),
		*GetClass()->GetName(), *Art->GetName(), *MuzzleSocket.ToString(), *Barrel.ToString(), ArtViewScale);

	// On clients the weapon can be equipped before it began play; redo the attachment with the art layout.
	const AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
	if (OwnerCharacter && OwnerCharacter->GetCurrentWeapon() == this)
	{
		OnEquipped();
	}
}

FVector AShieldbreakWeapon::GetBarrelDirection() const
{
	return WeaponMesh->GetComponentQuat().RotateVector(ArtBarrelLocal).GetSafeNormal();
}

void AShieldbreakWeapon::PlayShotEffects(bool bLocalView)
{
	const FVector Muzzle = GetMuzzleLocation();
	// The shooter sees the gun kick on screen; everyone else sees the body rock with the shot.
	if (UShieldbreakBodyAnimator* Animator = GetOwningCharacter() ? GetOwningCharacter()->GetBodyAnimator() : nullptr)
	{
		if (bLocalView)
		{
			Animator->AddFirstPersonKick(Stats.RecoilPitch);
		}
		else
		{
			Animator->AddBodyKick(Stats.RecoilPitch);
		}
	}
	if (bHasArt && !Stats.MuzzleFlash.IsNull())
	{
		UNiagaraComponent* Flash = ShieldbreakArt::SpawnFXAttached(*Stats.MuzzleFlash.ToString(), WeaponMesh, NAME_None, Muzzle, GetBarrelDirection().Rotation());
		if (Flash && WeaponMesh->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson)
		{
			Flash->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
			Flash->MarkRenderStateDirty();
		}
	}

	// Layered close/tail/distant banks (FShieldbreakSFX). Stats.FireSound is no longer used: Lyra's rifle MetaSound is an
	// auto-fire loop that stays silent when played as a one-shot, which is why the guns had no sound at all.
	FShieldbreakSFX::PlayShot(this, Sounds, Muzzle, bLocalView);
}

void AShieldbreakWeapon::SpawnImpactFX(const FHitResult& Hit) const
{
	const bool bHitCharacter = Cast<APawn>(Hit.GetActor()) != nullptr;
	ShieldbreakArt::SpawnFX(this, bHitCharacter ? ShieldbreakArt::ImpactCharacter : ShieldbreakArt::ImpactConcrete,
		Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
}

// ---- Reload ----

bool AShieldbreakWeapon::CanReload() const
{
	return !Stats.bUsesHeat && !bReloading && ReloadStartLocalTime < 0.f
		&& AmmoInMagazine < Stats.MagazineSize && ReserveAmmo > 0;
}

void AShieldbreakWeapon::StartReload()
{
	if (!CanReload())
	{
		return;
	}

	BurstShotsRemaining = 0;
	if (const UWorld* World = GetWorld())
	{
		ReloadStartLocalTime = World->GetTimeSeconds();
	}
	if (AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter())
	{
		OwnerCharacter->PlayActionAnimation(EShieldbreakAction::Reload, Stats.ReloadTime);
		if (OwnerCharacter->IsLocallyControlled() && OwnerCharacter->IsPlayerControlled())
		{
			PlayReloadSound(true);
		}
	}

	if (HasAuthority())
	{
		BeginReload();
	}
	else
	{
		ServerStartReload();
	}
}

void AShieldbreakWeapon::ServerStartReload_Implementation()
{
	BeginReload();
}

void AShieldbreakWeapon::BeginReload()
{
	if (bReloading || Stats.bUsesHeat || AmmoInMagazine >= Stats.MagazineSize || ReserveAmmo <= 0)
	{
		ReloadStartLocalTime = -1.f;
		return;
	}

	bReloading = true;
	if (const UWorld* World = GetWorld())
	{
		ReloadStartLocalTime = World->GetTimeSeconds();
	}
	if (AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter())
	{
		OwnerCharacter->MulticastPlayAction(static_cast<uint8>(EShieldbreakAction::Reload));
		// A listen-server host hears other players reload here (OnRep_Reloading doesn't run on the server).
		if (!(OwnerCharacter->IsLocallyControlled() && OwnerCharacter->IsPlayerControlled()))
		{
			PlayReloadSound(false);
		}
	}
	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &AShieldbreakWeapon::FinishReload, Stats.ReloadTime, false);
}

void AShieldbreakWeapon::FinishReload()
{
	const int32 Needed = Stats.MagazineSize - AmmoInMagazine;
	const int32 Moved = FMath::Min(Needed, ReserveAmmo);
	AmmoInMagazine += Moved;
	ReserveAmmo -= Moved;
	bReloading = false;
	ReloadStartLocalTime = -1.f;
}

void AShieldbreakWeapon::OnRep_Reloading()
{
	const UWorld* World = GetWorld();
	if (bReloading)
	{
		const AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
		if (!(OwnerCharacter && OwnerCharacter->IsLocallyControlled() && OwnerCharacter->IsPlayerControlled()))
		{
			PlayReloadSound(false); // the reloading player already started it locally
		}
		if (ReloadStartLocalTime < 0.f && World)
		{
			ReloadStartLocalTime = World->GetTimeSeconds();
		}
	}
	else
	{
		ReloadStartLocalTime = -1.f;
	}
}

float AShieldbreakWeapon::GetReloadProgress() const
{
	const UWorld* World = GetWorld();
	if (!World || ReloadStartLocalTime < 0.f)
	{
		return -1.f;
	}
	return FMath::Clamp((World->GetTimeSeconds() - ReloadStartLocalTime) / FMath::Max(0.01f, Stats.ReloadTime), 0.f, 1.f);
}

void AShieldbreakWeapon::AddReserveAmmo(int32 Amount)
{
	if (!HasAuthority() || Stats.bUsesHeat)
	{
		return;
	}
	ReserveAmmo = FMath::Clamp(ReserveAmmo + Amount, 0, Stats.MaxReserveAmmo);
}

float AShieldbreakWeapon::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return static_cast<float>(GameState->GetServerWorldTimeSeconds());
	}
	return World->GetTimeSeconds();
}

// ---- Charge weapons ----

float AShieldbreakWeapon::GetChargeFraction() const
{
	const UWorld* World = GetWorld();
	if (!bCharging || ChargeStartLocalTime < 0.f || !World)
	{
		return 0.f;
	}
	return FMath::Clamp((World->GetTimeSeconds() - ChargeStartLocalTime) / FMath::Max(Stats.ChargeTime, 0.01f), 0.f, 1.f);
}

void AShieldbreakWeapon::ReleaseCharge()
{
	const UWorld* World = GetWorld();
	bCharging = false;
	ChargeStartLocalTime = -1.f;
	StopWeaponAudio(ChargeAudio);
	UpdateChargeGlow();
	if (World && CanFireShotLocal())
	{
		FireShotLocal();
		NextShotLocalTime = World->GetTimeSeconds() + Stats.TimeBetweenShots;
	}
}

void AShieldbreakWeapon::CancelCharge()
{
	if (!bCharging)
	{
		return;
	}
	bCharging = false;
	ChargeStartLocalTime = -1.f;
	StopWeaponAudio(ChargeAudio);
	UpdateChargeGlow();
	ServerSetCharging(false);
}

void AShieldbreakWeapon::ServerSetCharging_Implementation(bool bNewCharging)
{
	bCharging = bNewCharging;
	ServerChargeStartTime = bNewCharging ? GetServerTime() : -1.f;
	OnRep_Charging();
}

void AShieldbreakWeapon::OnRep_Charging()
{
	const UWorld* World = GetWorld();
	if (bCharging && World)
	{
		ChargeStartLocalTime = World->GetTimeSeconds();
		const AShieldbreakCharacter* OwnerCharacter = GetOwningCharacter();
		const bool bLocalView = OwnerCharacter && OwnerCharacter->IsLocallyControlled() && OwnerCharacter->IsPlayerControlled();
		StopWeaponAudio(ChargeAudio);
		ChargeAudio = bLocalView
			? FShieldbreakSFX::Play2D(this, EShieldbreakSound::RailCharge, 0.6f)
			: FShieldbreakSFX::PlayAttached(GetRootComponent(), EShieldbreakSound::RailCharge, 0.9f);
	}
	else
	{
		StopWeaponAudio(ChargeAudio);
	}
	UpdateChargeGlow();
}

void AShieldbreakWeapon::UpdateChargeGlow()
{
	if (!ChargeGlowMesh)
	{
		return;
	}
	const bool bShow = bCharging && !IsHidden();
	ChargeGlowMesh->SetVisibility(bShow);
	if (bShow)
	{
		const float Fraction = GetChargeFraction();
		ChargeGlowMesh->SetWorldLocation(GetMuzzleLocation());
		ChargeGlowMesh->SetWorldScale3D(FVector(0.03f + 0.09f * Fraction));
	}
}
