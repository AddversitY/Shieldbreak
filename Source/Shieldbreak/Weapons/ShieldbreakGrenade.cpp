#include "Weapons/ShieldbreakGrenade.h"

#include "Audio/ShieldbreakSFX.h"
#include "Visuals/ShieldbreakArt.h"
#include "Shieldbreak.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/ShieldbreakDamageTypes.h"
#include "Visuals/ShieldbreakExplosionFX.h"

namespace ShieldbreakGrenadePrivate
{
	TAutoConsoleVariable<float> CVarReleaseDelay(TEXT("sb.GrenadeReleaseDelay"), 0.15f,
		TEXT("Seconds from the grenade press until the hand lets go (the grenade spawns then; the throw animations are timed to it)."));
	TAutoConsoleVariable<float> CVarThrowTime(TEXT("sb.GrenadeThrowTime"), 0.6f,
		TEXT("Seconds a grenade throw lasts: minimum time between throws, and no firing meanwhile."));
	TAutoConsoleVariable<int32> CVarLyraBurst(TEXT("sb.GrenadeLyraBurst"), 1,
		TEXT("1: Lyra's grenade sparks and debris play under Shieldbreak's explosion."));
	/** Model radius (cm) for the rolling spin; the collision sphere is a little bigger. */
	constexpr float ModelRadius = 4.f;
	/** Arming light: EmissiveStrength of the glow strip at full blink, and the light's candelas. */
	constexpr float GlowStrength = 8.f;
	constexpr float LightCandelas = 6.f;
}

float AShieldbreakGrenade::GetReleaseDelay()
{
	return FMath::Clamp(ShieldbreakGrenadePrivate::CVarReleaseDelay.GetValueOnGameThread(), 0.f, 1.f);
}

float AShieldbreakGrenade::GetThrowDuration()
{
	return FMath::Max(GetReleaseDelay() + 0.05f, ShieldbreakGrenadePrivate::CVarThrowTime.GetValueOnGameThread());
}

UStaticMesh* AShieldbreakGrenade::GetGrenadeMesh()
{
	if (UStaticMesh* Ours = ShieldbreakArt::Load<UStaticMesh>(ShieldbreakArt::SBFragGrenadeMesh))
	{
		return Ours;
	}
	return ShieldbreakArt::Load<UStaticMesh>(ShieldbreakArt::GrenadeMesh);
}

AShieldbreakGrenade::AShieldbreakGrenade()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.f);
	SetCanBeDamaged(false);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(9.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	RootComponent = Collision;

	GrenadeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrenadeMesh"));
	GrenadeMesh->SetupAttachment(Collision);
	GrenadeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GrenadeMesh->SetRelativeScale3D(FVector(0.2f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SphereFinder.Succeeded())
	{
		GrenadeMesh->SetStaticMesh(SphereFinder.Object);
	}
	if (MaterialFinder.Succeeded())
	{
		GrenadeMesh->SetMaterial(0, MaterialFinder.Object);
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = Collision;
	ProjectileMovement->InitialSpeed = 0.f;
	ProjectileMovement->MaxSpeed = 4000.f;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.35f;
	ProjectileMovement->Friction = 0.4f;
	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->ProjectileGravityScale = 1.f;
	ProjectileMovement->bAutoActivate = true;

	ArmingLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ArmingLight"));
	ArmingLight->SetupAttachment(Collision);
	ArmingLight->SetRelativeLocation(FVector(0.f, 0.f, 6.f));
	ArmingLight->SetMobility(EComponentMobility::Movable);
	ArmingLight->SetIntensityUnits(ELightUnits::Candelas);
	ArmingLight->SetIntensity(0.f);
	ArmingLight->SetAttenuationRadius(160.f);
	ArmingLight->SetSourceRadius(2.f);
	ArmingLight->SetLightColor(FLinearColor(1.f, 0.5f, 0.12f));
	ArmingLight->SetCastShadows(false);
	ArmingLight->SetAffectTranslucentLighting(false);
	ArmingLight->SetVolumetricScatteringIntensity(0.f);
}

void AShieldbreakGrenade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShieldbreakGrenade, bExploded);
	DOREPLIFETIME(AShieldbreakGrenade, bArmed);
}

void AShieldbreakGrenade::BeginPlay()
{
	Super::BeginPlay();

	if (UStaticMesh* Art = GetGrenadeMesh())
	{
		GrenadeMesh->SetStaticMesh(Art);
		GrenadeMesh->EmptyOverrideMaterials();
		GrenadeMesh->SetRelativeScale3D(FVector::OneVector);
		// The arming light's strip (our model's GlowAmber slot) blinks through its EmissiveStrength.
		const TArray<FName> Slots = GrenadeMesh->GetMaterialSlotNames();
		for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
		{
			if (Slots[Slot].ToString().Contains(TEXT("GlowAmber")))
			{
				GlowMID = GrenadeMesh->CreateDynamicMaterialInstance(Slot);
				break;
			}
		}
	}
	else if (UMaterialInstanceDynamic* MID = GrenadeMesh->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.25f, 0.4f, 0.15f));
	}
	GrenadeMesh->bAffectDistanceFieldLighting = false;
	// A random start orientation and tumble, so no two throws look alike.
	Spin = FRotator(FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f)).Quaternion();
	if (GetNetMode() == NM_DedicatedServer)
	{
		ArmingLight->SetVisibility(false);
		SetActorTickEnabled(false);
	}

	if (!HasAuthority())
	{
		// Clients just follow the replicated movement.
		ProjectileMovement->SetComponentTickEnabled(false);
	}
	else
	{
		ProjectileMovement->OnProjectileBounce.AddDynamic(this, &AShieldbreakGrenade::HandleBounce);
		if (AActor* ThrowerActor = GetInstigator())
		{
			Collision->IgnoreActorWhenMoving(ThrowerActor, true);
		}
	}
}

void AShieldbreakGrenade::Launch(const FVector& Direction, const FVector& InheritedVelocity)
{
	ProjectileMovement->Velocity = Direction.GetSafeNormal() * LaunchSpeed + InheritedVelocity;
	ProjectileMovement->UpdateComponentVelocity();

	GetWorldTimerManager().SetTimer(FuseTimerHandle, this, &AShieldbreakGrenade::Explode, MaxFuse, false);
}

void AShieldbreakGrenade::HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	if (bExploded)
	{
		return;
	}
	// Bounce sound for everyone, by how hard it hit along the surface normal; rolling doesn't spam it.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float Impact = static_cast<float>(FMath::Abs(FVector::DotProduct(ImpactVelocity, ImpactResult.ImpactNormal)));
	if (Now - LastBounceTime >= 0.08f && Impact > 60.f)
	{
		LastBounceTime = Now;
		MulticastBounce(ImpactResult.ImpactPoint, FMath::Clamp(Impact / 1200.f, 0.15f, 1.f));
	}
	if (bBounced)
	{
		return;
	}
	bBounced = true;
	bArmed = true;
	OnRep_Armed();

	// Short fuse once it lands, unless the long fuse is about to fire anyway.
	const float Remaining = GetWorldTimerManager().GetTimerRemaining(FuseTimerHandle);
	if (Remaining < 0.f || Remaining > FuseAfterBounce)
	{
		GetWorldTimerManager().SetTimer(FuseTimerHandle, this, &AShieldbreakGrenade::Explode, FuseAfterBounce, false);
	}
}

void AShieldbreakGrenade::Explode()
{
	if (bExploded || !HasAuthority())
	{
		return;
	}
	bExploded = true;

	UWorld* World = GetWorld();
	const FVector Origin = GetActorLocation() + FVector(0.f, 0.f, 10.f);
	AController* InstigatorController = GetInstigatorController();

	TArray<AActor*> Ignore;
	Ignore.Add(this);
	UGameplayStatics::ApplyRadialDamageWithFalloff(this, BaseDamage, MinDamage, Origin, InnerRadius, OuterRadius, 1.f,
		UDamageType_Explosive::StaticClass(), Ignore, this, InstigatorController, ECC_Visibility);

	// Knockback everyone in range, the thrower included (grenade jumps).
	if (World)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakGrenadeKnockback), false, this);
		World->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(OuterRadius), Params);

		TSet<ACharacter*> Launched;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			ACharacter* HitCharacter = Cast<ACharacter>(Overlap.GetActor());
			if (!HitCharacter || Launched.Contains(HitCharacter))
			{
				continue;
			}
			Launched.Add(HitCharacter);

			const FVector Offset = HitCharacter->GetActorLocation() - Origin;
			const float Distance = static_cast<float>(Offset.Size());
			const float Falloff = 1.f - FMath::Clamp((Distance - InnerRadius) / FMath::Max(1.f, OuterRadius - InnerRadius), 0.f, 1.f);
			const FVector Push = Offset.GetSafeNormal() * KnockbackStrength * Falloff + FVector(0.f, 0.f, KnockbackUp * Falloff);
			HitCharacter->LaunchCharacter(Push, false, false);
		}
	}

	ProjectileMovement->StopMovementImmediately();
	SetActorEnableCollision(false);
	PlayExplosionFX();
	ForceNetUpdate();
	SetLifeSpan(0.5f);
}

void AShieldbreakGrenade::OnRep_Armed()
{
	ArmedAge = 0.f;
}

void AShieldbreakGrenade::MulticastBounce_Implementation(FVector_NetQuantize Location, float Strength)
{
	FShieldbreakSFX::PlayAt(this, EShieldbreakSound::GrenadeBounce, Location, Strength, FMath::FRandRange(0.94f, 1.06f));
	// A hard hit knocks the tumble about.
	SpinAxis = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-0.3f, 0.3f)).GetSafeNormal();
}

void AShieldbreakGrenade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateVisuals(DeltaSeconds);
}

void AShieldbreakGrenade::UpdateVisuals(float DeltaSeconds)
{
	using namespace ShieldbreakGrenadePrivate;
	if (bExploded || DeltaSeconds <= 0.f)
	{
		return;
	}
	Age += DeltaSeconds;
	ArmedAge += bArmed ? DeltaSeconds : 0.f;

	// Velocity from the movement this machine saw (clients don't run the projectile movement).
	const FVector Location = GetActorLocation();
	const FVector Velocity = bHasLastLocation ? (Location - LastLocation) / DeltaSeconds : FVector::ZeroVector;
	LastLocation = Location;
	bHasLastLocation = true;

	// Tumble: in the air it keeps the spin it left the hand with (end over end, a little off axis); on the ground it
	// rolls the way it moves, at the speed its size gives, and slows with it.
	const FVector Flat(Velocity.X, Velocity.Y, 0.f);
	const float Speed = static_cast<float>(Velocity.Size());
	if (!bBounced)
	{
		if (SpinRate <= 0.f && Speed > 100.f)
		{
			SpinAxis = (FVector::CrossProduct(FVector::UpVector, Velocity.GetSafeNormal()) + FVector(0.f, 0.f, FMath::FRandRange(-0.35f, 0.35f))).GetSafeNormal();
			SpinRate = FMath::FRandRange(720.f, 1000.f);
		}
	}
	else
	{
		const FVector Roll = FVector::CrossProduct(FVector::UpVector, Flat.GetSafeNormal());
		const float RollRate = FMath::RadiansToDegrees(static_cast<float>(Flat.Size()) / ModelRadius);
		const float Blend = 1.f - FMath::Exp(-8.f * DeltaSeconds);
		if (!Roll.IsNearlyZero())
		{
			SpinAxis = FMath::Lerp(SpinAxis, Roll, Blend).GetSafeNormal();
		}
		SpinRate = FMath::Lerp(SpinRate, FMath::Min(RollRate, 1400.f), Blend);
	}
	if (SpinRate > 0.f && !SpinAxis.IsNearlyZero())
	{
		Spin = (FQuat(SpinAxis, FMath::DegreesToRadians(SpinRate * DeltaSeconds)) * Spin).GetNormalized();
	}
	GrenadeMesh->SetWorldRotation(Spin);

	// Arming light: a slow blink in flight, then faster and faster once it lands and the short fuse runs.
	float Period = 0.45f;
	if (bArmed)
	{
		Period = FMath::Lerp(0.22f, 0.07f, FMath::Clamp(ArmedAge / FMath::Max(0.1f, FuseAfterBounce), 0.f, 1.f));
	}
	const float Phase = FMath::Fmod(bArmed ? ArmedAge : Age, Period) / Period;
	BlinkLevel = Phase < 0.35f ? FMath::Sin(Phase / 0.35f * UE_PI) : 0.f;
	if (GlowMID)
	{
		GlowMID->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.4f + GlowStrength * BlinkLevel);
	}
	ArmingLight->SetIntensity(LightCandelas * BlinkLevel);
}

void AShieldbreakGrenade::OnRep_Exploded()
{
	if (bExploded)
	{
		PlayExplosionFX();
	}
}

void AShieldbreakGrenade::PlayExplosionFX()
{
	if (GrenadeMesh)
	{
		GrenadeMesh->SetVisibility(false);
	}
	if (ArmingLight)
	{
		ArmingLight->SetVisibility(false);
	}
	const FVector Location = GetActorLocation();
	AShieldbreakExplosionFX::Spawn(GetWorld(), Location, 1.f);
	if (ShieldbreakGrenadePrivate::CVarLyraBurst.GetValueOnGameThread() != 0)
	{
		ShieldbreakArt::SpawnFX(this, ShieldbreakArt::GrenadeExplosionFX, Location);
	}
	FShieldbreakSFX::PlayAt(this, EShieldbreakSound::GrenadeExplosion, Location);
}
