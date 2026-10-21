#include "Weapons/ShieldbreakRocket.h"

#include "Audio/ShieldbreakSFX.h"
#include "Shieldbreak.h"
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
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Visuals/ShieldbreakArt.h"
#include "Weapons/ShieldbreakDamageTypes.h"
#include "Weapons/ShieldbreakTracer.h"

AShieldbreakRocket::AShieldbreakRocket()
{
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.f);
	SetCanBeDamaged(false);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(10.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	RootComponent = Collision;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Engine cylinder/cone are 100 uu tall along +Z; pitch them onto +X (the flight direction).
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Collision);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	BodyMesh->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.5f));

	NoseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NoseMesh"));
	NoseMesh->SetupAttachment(Collision);
	NoseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NoseMesh->SetRelativeLocation(FVector(30.f, 0.f, 0.f));
	NoseMesh->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	NoseMesh->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.12f));

	if (CylinderFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderFinder.Object);
	}
	if (ConeFinder.Succeeded())
	{
		NoseMesh->SetStaticMesh(ConeFinder.Object);
	}
	if (MaterialFinder.Succeeded())
	{
		BodyMesh->SetMaterial(0, MaterialFinder.Object);
		NoseMesh->SetMaterial(0, MaterialFinder.Object);
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = Collision;
	ProjectileMovement->InitialSpeed = 0.f;
	ProjectileMovement->MaxSpeed = 4000.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
}

void AShieldbreakRocket::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShieldbreakRocket, bExploded);
}

void AShieldbreakRocket::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* BodyMID = BodyMesh->CreateDynamicMaterialInstance(0))
	{
		BodyMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.3f, 0.32f, 0.28f));
	}
	if (UMaterialInstanceDynamic* NoseMID = NoseMesh->CreateDynamicMaterialInstance(0))
	{
		NoseMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.8f, 0.6f, 0.1f));
	}
	ShieldbreakArt::SpawnFXAttached(ShieldbreakArt::GrenadeTrailFX, BodyMesh, NAME_None, GetActorLocation(), GetActorRotation());

	if (HasAuthority())
	{
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &AShieldbreakRocket::HandleStop);
		if (AActor* Shooter = GetInstigator())
		{
			Collision->IgnoreActorWhenMoving(Shooter, true);
		}
		GetWorldTimerManager().SetTimer(LifetimeTimerHandle, this, &AShieldbreakRocket::Explode, MaxLifetime, false);
	}
}

void AShieldbreakRocket::Launch(const FVector& Direction)
{
	ProjectileMovement->Velocity = Direction.GetSafeNormal() * Speed;
	ProjectileMovement->UpdateComponentVelocity();
}

void AShieldbreakRocket::HandleStop(const FHitResult& ImpactResult)
{
	Explode();
}

void AShieldbreakRocket::Explode()
{
	if (bExploded || !HasAuthority())
	{
		return;
	}
	bExploded = true;
	GetWorldTimerManager().ClearTimer(LifetimeTimerHandle);

	UWorld* World = GetWorld();
	const FVector Origin = GetActorLocation();
	AController* InstigatorController = GetInstigatorController();

	TArray<AActor*> Ignore;
	Ignore.Add(this);
	UGameplayStatics::ApplyRadialDamageWithFalloff(this, BaseDamage, MinDamage, Origin, InnerRadius, OuterRadius, 1.f,
		UDamageType_Explosive::StaticClass(), Ignore, this, InstigatorController, ECC_Visibility);

	// Knockback everyone in range, the shooter included (rocket jumps).
	if (World)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakRocketKnockback), false, this);
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
			HitCharacter->LaunchCharacter(Offset.GetSafeNormal() * KnockbackStrength * Falloff + FVector(0.f, 0.f, KnockbackUp * Falloff), false, false);
		}
	}

	ProjectileMovement->StopMovementImmediately();
	SetActorEnableCollision(false);
	PlayExplosionFX();
	ForceNetUpdate();
	SetLifeSpan(0.5f);
}

void AShieldbreakRocket::OnRep_Exploded()
{
	if (bExploded)
	{
		ProjectileMovement->StopMovementImmediately();
		PlayExplosionFX();
	}
}

void AShieldbreakRocket::PlayExplosionFX()
{
	BodyMesh->SetVisibility(false);
	NoseMesh->SetVisibility(false);

	const FVector Location = GetActorLocation();
	AShieldbreakTracer::SpawnBurst(GetWorld(), Location, 40.f, InnerRadius * 1.2f, 0.35f, FLinearColor(1.f, 0.5f, 0.1f));
	ShieldbreakArt::SpawnFX(this, ShieldbreakArt::GrenadeExplosionFX, Location, FRotator::ZeroRotator, 1.6f);
	if (!ShieldbreakArt::PlaySound(this, ShieldbreakArt::GrenadeExplosionSound, &Location, 1.f, 0.85f))
	{
		FShieldbreakSFX::PlayAt(this, EShieldbreakSound::GrenadeExplosion, Location, 1.f, 0.85f);
	}
}
