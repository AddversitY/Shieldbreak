#include "Weapons/ShieldbreakTracer.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AShieldbreakTracer::AShieldbreakTracer()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetCanBeDamaged(false);

	ShapeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShapeMesh"));
	ShapeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShapeMesh->SetCastShadow(false);
	ShapeMesh->SetGenerateOverlapEvents(false);
	ShapeMesh->SetMobility(EComponentMobility::Movable);
	RootComponent = ShapeMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	if (CylinderFinder.Succeeded())
	{
		CylinderAsset = CylinderFinder.Object;
	}
	if (SphereFinder.Succeeded())
	{
		SphereAsset = SphereFinder.Object;
	}
	if (MaterialFinder.Succeeded())
	{
		ShapeMesh->SetMaterial(0, MaterialFinder.Object);
	}
}

AShieldbreakTracer* AShieldbreakTracer::SpawnRaw(UWorld* World, const FVector& Location)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	return World->SpawnActor<AShieldbreakTracer>(AShieldbreakTracer::StaticClass(), Location, FRotator::ZeroRotator, Params);
}

void AShieldbreakTracer::Spawn(UWorld* World, const FVector& Start, const FVector& End, const FLinearColor& Color)
{
	if (AShieldbreakTracer* Beam = SpawnRaw(World, Start))
	{
		Beam->InitAsBeam(Start, End, Color, 0.06f);
	}
	if (AShieldbreakTracer* Puff = SpawnRaw(World, End))
	{
		Puff->InitAsSphere(End, 6.f, 22.f, 0.12f, Color);
	}
}

void AShieldbreakTracer::SpawnBurst(UWorld* World, const FVector& Location, float StartRadius, float EndRadius, float Duration, const FLinearColor& Color)
{
	if (AShieldbreakTracer* Burst = SpawnRaw(World, Location))
	{
		Burst->InitAsSphere(Location, StartRadius, EndRadius, Duration, Color);
	}
}

void AShieldbreakTracer::InitAsBeam(const FVector& Start, const FVector& End, const FLinearColor& Color, float Duration)
{
	if (CylinderAsset)
	{
		ShapeMesh->SetStaticMesh(CylinderAsset);
	}
	MID = ShapeMesh->CreateDynamicMaterialInstance(0);
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}

	const FVector Delta = End - Start;
	const float Length = FMath::Max(1.f, static_cast<float>(Delta.Size()));
	// The engine cylinder is 100 uu tall along Z with its pivot at the center.
	const FRotator Rotation = FRotationMatrix::MakeFromZ(Delta.GetSafeNormal()).Rotator();
	SetActorLocationAndRotation(Start + Delta * 0.5f, Rotation);
	BeamScale = FVector(0.025f, 0.025f, Length / 100.f);
	SetActorScale3D(BeamScale);

	bSphere = false;
	Lifetime = Duration;
	Age = 0.f;
}

void AShieldbreakTracer::InitAsSphere(const FVector& Location, float StartRadius, float EndRadius, float Duration, const FLinearColor& Color)
{
	if (SphereAsset)
	{
		ShapeMesh->SetStaticMesh(SphereAsset);
	}
	MID = ShapeMesh->CreateDynamicMaterialInstance(0);
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}

	SetActorLocation(Location);
	// The engine sphere is 100 uu across: scale 1 = radius 50.
	StartScale = StartRadius / 50.f;
	EndScale = EndRadius / 50.f;
	SetActorScale3D(FVector(StartScale));

	bSphere = true;
	Lifetime = FMath::Max(0.01f, Duration);
	Age = 0.f;
}

void AShieldbreakTracer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	const float Alpha = FMath::Clamp(Age / Lifetime, 0.f, 1.f);

	if (bSphere)
	{
		SetActorScale3D(FVector(FMath::Lerp(StartScale, EndScale, FMath::Sqrt(Alpha))));
	}
	else
	{
		// Beam thins out as it fades.
		const float Thickness = FMath::Lerp(BeamScale.X, BeamScale.X * 0.3f, Alpha);
		SetActorScale3D(FVector(Thickness, Thickness, BeamScale.Z));
	}

	if (Age >= Lifetime)
	{
		Destroy();
	}
}
