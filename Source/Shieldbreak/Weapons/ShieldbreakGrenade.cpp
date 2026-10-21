#include "Weapons/ShieldbreakGrenade.h"

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
#include "Weapons/ShieldbreakDamageTypes.h"
#include "Weapons/ShieldbreakTracer.h"

AShieldbreakGrenade::AShieldbreakGrenade()
{
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
}

void AShieldbreakGrenade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShieldbreakGrenade, bExploded);
}

void AShieldbreakGrenade::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* MID = GrenadeMesh->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.25f, 0.4f, 0.15f));
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
	if (bBounced || bExploded)
	{
		return;
	}
	bBounced = true;

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
	AShieldbreakTracer::SpawnBurst(GetWorld(), GetActorLocation(), 30.f, InnerRadius, 0.3f, FLinearColor(1.f, 0.45f, 0.05f));
}
