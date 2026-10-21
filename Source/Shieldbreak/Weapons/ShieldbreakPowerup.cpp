#include "Weapons/ShieldbreakPowerup.h"

#include "Audio/ShieldbreakSFX.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/ShieldbreakCharacter.h"
#include "Shield/ShieldHealthComponent.h"
#include "Shield/ShieldTuningData.h"
#include "UObject/ConstructorHelpers.h"

AShieldbreakPowerup::AShieldbreakPowerup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetCanBeDamaged(false);

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(110.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	OrbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMesh"));
	OrbMesh->SetupAttachment(Trigger);
	OrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OrbMesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	OrbMesh->SetRelativeScale3D(FVector(0.45f));

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(Trigger);
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BaseMesh->SetRelativeLocation(FVector(0.f, 0.f, -65.f));
	BaseMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.06f));

	if (SphereFinder.Succeeded())
	{
		OrbMesh->SetStaticMesh(SphereFinder.Object);
	}
	if (CylinderFinder.Succeeded())
	{
		BaseMesh->SetStaticMesh(CylinderFinder.Object);
	}
	if (MaterialFinder.Succeeded())
	{
		OrbMesh->SetMaterial(0, MaterialFinder.Object);
		BaseMesh->SetMaterial(0, MaterialFinder.Object);
	}
}

void AShieldbreakPowerup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShieldbreakPowerup, bAvailable);
}

void AShieldbreakPowerup::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* OrbMID = OrbMesh->CreateDynamicMaterialInstance(0))
	{
		OrbMID->SetVectorParameterValue(TEXT("Color"), GetPowerupColor());
	}
	if (UMaterialInstanceDynamic* BaseMID = BaseMesh->CreateDynamicMaterialInstance(0))
	{
		BaseMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 0.15f, 0.17f));
	}

	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &AShieldbreakPowerup::HandleOverlap);
	}
	UpdateVisuals();
}

FLinearColor AShieldbreakPowerup::GetPowerupColor() const
{
	switch (Type)
	{
	case EShieldbreakPowerupType::Overshield:
	default:
		return FLinearColor(1.f, 0.75f, 0.1f);
	}
}

void AShieldbreakPowerup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAvailable || !OrbMesh)
	{
		return;
	}
	SpinAngle = FMath::Fmod(SpinAngle + 90.f * DeltaSeconds, 360.f);
	BobTime += DeltaSeconds;
	OrbMesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f + 10.f * FMath::Sin(BobTime * 2.f)));
	OrbMesh->SetRelativeRotation(FRotator(0.f, SpinAngle, 0.f));
}

void AShieldbreakPowerup::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bAvailable)
	{
		TryConsume(Cast<AShieldbreakCharacter>(OtherActor));
	}
}

bool AShieldbreakPowerup::TryConsume(AShieldbreakCharacter* Character)
{
	UShieldHealthComponent* ShieldComponent = Character ? Character->GetShieldHealth() : nullptr;
	if (!HasAuthority() || !bAvailable || !Character || !Character->IsAlive() || !ShieldComponent)
	{
		return false;
	}

	switch (Type)
	{
	case EShieldbreakPowerupType::Overshield:
		ShieldComponent->GrantOvershield(ShieldComponent->GetTuningData()->PowerupOvershield);
		break;
	}

	bAvailable = false;
	ForceNetUpdate();
	UpdateVisuals();
	FShieldbreakSFX::PlayAt(this, EShieldbreakSound::VentSuccess, GetActorLocation(), 1.f, 0.8f);
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AShieldbreakPowerup::Respawn, RespawnTime, false);
	return true;
}

void AShieldbreakPowerup::Respawn()
{
	bAvailable = true;
	ForceNetUpdate();
	UpdateVisuals();

	// Someone camping the spawn gets it the moment it appears.
	TArray<AActor*> Overlapping;
	GetOverlappingActors(Overlapping, AShieldbreakCharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		if (TryConsume(Cast<AShieldbreakCharacter>(Actor)))
		{
			break;
		}
	}
}

void AShieldbreakPowerup::OnRep_Available()
{
	if (!bAvailable)
	{
		FShieldbreakSFX::PlayAt(this, EShieldbreakSound::VentSuccess, GetActorLocation(), 1.f, 0.8f);
	}
	UpdateVisuals();
}

void AShieldbreakPowerup::UpdateVisuals()
{
	if (OrbMesh)
	{
		OrbMesh->SetVisibility(bAvailable);
	}
}
