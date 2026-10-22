#include "Game/ShieldbreakGravLift.h"

#include "Audio/ShieldbreakSFX.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AShieldbreakGravLift::AShieldbreakGravLift()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	bReplicates = false;
	SetCanBeDamaged(false);

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(90.f, 90.f, 60.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(Trigger);
	PadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PadMesh->SetRelativeLocation(FVector(0.f, 0.f, -58.f));
	PadMesh->SetRelativeScale3D(FVector(1.8f, 1.8f, 0.04f));

	// A thin glowing column above the pad so the lift reads from across the map.
	BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamMesh"));
	BeamMesh->SetupAttachment(Trigger);
	BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamMesh->SetCastShadow(false);
	BeamMesh->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
	BeamMesh->SetRelativeScale3D(FVector(0.12f, 0.12f, 4.f));

	if (CylinderFinder.Succeeded())
	{
		PadMesh->SetStaticMesh(CylinderFinder.Object);
		BeamMesh->SetStaticMesh(CylinderFinder.Object);
	}
	if (MaterialFinder.Succeeded())
	{
		PadMesh->SetMaterial(0, MaterialFinder.Object);
		BeamMesh->SetMaterial(0, MaterialFinder.Object);
	}
}

void AShieldbreakGravLift::BeginPlay()
{
	Super::BeginPlay();

	for (UStaticMeshComponent* Mesh : { PadMesh.Get(), BeamMesh.Get() })
	{
		if (UMaterialInstanceDynamic* MID = Mesh ? Mesh->CreateDynamicMaterialInstance(0) : nullptr)
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.75f, 0.95f));
		}
	}
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AShieldbreakGravLift::HandleOverlap);
}

FVector AShieldbreakGravLift::ComputeLaunchVelocity(const FVector& From, const FVector& To, float GravityZ, float ApexClearance)
{
	const float Gravity = FMath::Max(1.f, -GravityZ);
	const float ApexZ = FMath::Max(From.Z, To.Z) + ApexClearance;
	const float Rise = FMath::Max(1.f, static_cast<float>(ApexZ - From.Z));
	const float Fall = FMath::Max(1.f, static_cast<float>(ApexZ - To.Z));
	const float UpSpeed = FMath::Sqrt(2.f * Gravity * Rise);
	const float FlightTime = UpSpeed / Gravity + FMath::Sqrt(2.f * Fall / Gravity);
	const FVector Horizontal = FVector(To.X - From.X, To.Y - From.Y, 0.f) / FlightTime;
	return FVector(Horizontal.X, Horizontal.Y, UpSpeed);
}

void AShieldbreakGravLift::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryLaunch(OtherActor, false);
}

void AShieldbreakGravLift::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// A begin-overlap doesn't fire for a character that was already inside the trigger (spawned there, or the
	// overlap fired before its movement was ready), so anyone standing on the pad gets launched here too.
	TArray<AActor*> Overlapping;
	Trigger->GetOverlappingActors(Overlapping, ACharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		TryLaunch(Actor, true);
	}
}

void AShieldbreakGravLift::TryLaunch(AActor* OtherActor, bool bRequireGround)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	// The server and the owning client both launch (the client predicts); other machines just watch.
	if (!Movement || !(Character->HasAuthority() || Character->IsLocallyControlled()))
	{
		return;
	}
	if (bRequireGround && !Movement->IsMovingOnGround())
	{
		return;
	}

	const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight());
	// A tall climb onto a ledge needs a high arc, or the flight clips the ledge's front face before it's above it
	// (Spire's base lifts did with a fixed 150 clearance).
	const float Clearance = FMath::Max(150.f, 0.6f * static_cast<float>(FMath::Abs(Target.Z - Feet.Z)));
	Character->LaunchCharacter(ComputeLaunchVelocity(Feet, Target, Movement->GetGravityZ(), Clearance), true, true);
	if (Character->IsLocallyControlled())
	{
		FShieldbreakSFX::PlayAt(this, EShieldbreakSound::AegisRaise, GetActorLocation(), 1.f, 0.7f);
	}
}
