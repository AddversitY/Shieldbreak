#include "Game/ShieldbreakObjectives.h"

#include "Audio/ShieldbreakSFX.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/ShieldbreakGameMode.h"
#include "Game/ShieldbreakTeams.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/ShieldbreakCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Visuals/ShieldbreakArt.h"

namespace ShieldbreakObjectivesPrivate
{
	float ServerNow(const UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : (World ? World->GetTimeSeconds() : 0.f);
	}

	const FLinearColor BallColor(1.f, 0.9f, 0.3f);
}

// ---- Carriable (flag / ball) ----

AShieldbreakCarriable::AShieldbreakCarriable()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true; // objectives are tracked map-wide on the HUD
	SetCanBeDamaged(false);

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(90.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	auto MakePiece = [&](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Piece->SetupAttachment(Trigger);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetGenerateOverlapEvents(false);
		if (MaterialFinder.Succeeded())
		{
			Piece->SetMaterial(0, MaterialFinder.Object);
		}
		return Piece;
	};

	PoleMesh = MakePiece(TEXT("PoleMesh"));
	BannerMesh = MakePiece(TEXT("BannerMesh"));
	BaseMesh = MakePiece(TEXT("BaseMesh"));
	if (CylinderFinder.Succeeded())
	{
		PoleMesh->SetStaticMesh(CylinderFinder.Object);
		BaseMesh->SetStaticMesh(CylinderFinder.Object);
	}
	if (CubeFinder.Succeeded())
	{
		BannerMesh->SetStaticMesh(CubeFinder.Object);
	}
}

void AShieldbreakCarriable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShieldbreakCarriable, Kind);
	DOREPLIFETIME(AShieldbreakCarriable, Team);
	DOREPLIFETIME(AShieldbreakCarriable, Carrier);
	DOREPLIFETIME(AShieldbreakCarriable, bAtHome);
	DOREPLIFETIME(AShieldbreakCarriable, HomeLocation);
}

void AShieldbreakCarriable::BeginPlay()
{
	Super::BeginPlay();
	UpdateVisuals();
}

void AShieldbreakCarriable::Setup(EShieldbreakCarriableKind InKind, uint8 InTeam, const FVector& InHomeLocation)
{
	Kind = InKind;
	Team = InKind == EShieldbreakCarriableKind::Ball ? ShieldbreakTeams::NoTeam : InTeam;
	HomeLocation = InHomeLocation;
	ReturnHome();
	UpdateVisuals();
}

void AShieldbreakCarriable::OnRep_Setup()
{
	UpdateVisuals();
}

void AShieldbreakCarriable::UpdateVisuals()
{
	const bool bFlag = Kind == EShieldbreakCarriableKind::Flag;
	const FLinearColor Color = bFlag ? ShieldbreakTeams::GetTeamColor(Team) : ShieldbreakObjectivesPrivate::BallColor;

	if (!BannerMID && BannerMesh)
	{
		BannerMID = BannerMesh->CreateDynamicMaterialInstance(0);
	}
	if (BannerMID)
	{
		BannerMID->SetVectorParameterValue(TEXT("Color"), Color);
	}

	// Flag: a 170 uu pole with a banner near the top and a base plate while home. Ball: a 40 uu orb.
	PoleMesh->SetVisibility(bFlag);
	PoleMesh->SetRelativeLocation(FVector(0.f, 0.f, -5.f));
	PoleMesh->SetRelativeScale3D(FVector(0.05f, 0.05f, 1.7f));
	if (bFlag)
	{
		BannerMesh->SetRelativeLocation(FVector(0.f, 30.f, 55.f));
		BannerMesh->SetRelativeScale3D(FVector(0.02f, 0.55f, 0.35f));
	}
	else
	{
		UStaticMesh* SphereAsset = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		if (SphereAsset && BannerMesh->GetStaticMesh() != SphereAsset)
		{
			BannerMesh->SetStaticMesh(SphereAsset);
		}
		BannerMesh->SetRelativeLocation(FVector::ZeroVector);
		BannerMesh->SetRelativeScale3D(FVector(0.4f));
	}
	BaseMesh->SetVisibility(bFlag && bAtHome);
	BaseMesh->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
	BaseMesh->SetRelativeScale3D(FVector(1.4f, 1.4f, 0.04f));
}

void AShieldbreakCarriable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	// Carried: nothing to do but follow the carrier (attached). Lose it if the carrier is gone.
	if (Carrier)
	{
		if (!IsValid(Carrier) || !Carrier->IsAlive())
		{
			Drop();
		}
		return;
	}

	// Dropped too long: home it goes.
	if (!bAtHome && DroppedServerTime >= 0.f)
	{
		const float ReturnTime = Kind == EShieldbreakCarriableKind::Flag ? FlagReturnTime : BallReturnTime;
		if (ShieldbreakObjectivesPrivate::ServerNow(GetWorld()) - DroppedServerTime >= ReturnTime)
		{
			ReturnHome();
			if (AShieldbreakGameMode* GameMode = GetWorld()->GetAuthGameMode<AShieldbreakGameMode>())
			{
				GameMode->AnnounceObjective(nullptr, Kind == EShieldbreakCarriableKind::Flag
					? FString::Printf(TEXT("%s flag returned"), *ShieldbreakTeams::GetTeamName(Team))
					: FString(TEXT("Ball reset")), Team);
			}
		}
	}

	// Touches are polled (not just begin-overlap) so standing on a base still captures once your flag returns.
	TArray<AActor*> Overlapping;
	Trigger->GetOverlappingActors(Overlapping, AShieldbreakCharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		HandleTouch(Cast<AShieldbreakCharacter>(Actor));
		if (Carrier)
		{
			break;
		}
	}
}

void AShieldbreakCarriable::HandleTouch(AShieldbreakCharacter* Character)
{
	if (!Character || !Character->IsAlive() || Carrier)
	{
		return;
	}
	AShieldbreakGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AShieldbreakGameMode>() : nullptr;
	if (!GameMode || GameMode->IsMatchOver())
	{
		return;
	}

	if (Kind == EShieldbreakCarriableKind::Ball)
	{
		if (!Character->GetCarriedObjective())
		{
			PickUp(Character);
		}
		return;
	}

	const uint8 CharacterTeam = ShieldbreakTeams::GetTeam(Character);
	if (CharacterTeam == Team)
	{
		if (bAtHome)
		{
			// Capture: bring the enemy flag to your own flag while it's home.
			AShieldbreakCarriable* Held = Character->GetCarriedObjective();
			if (Held && Held->GetKind() == EShieldbreakCarriableKind::Flag && Held->GetTeam() != Team)
			{
				Held->ReturnHome();
				GameMode->ScoreFlagCapture(Character, Team);
			}
		}
		else
		{
			ReturnHome();
			GameMode->AnnounceObjective(Character, TEXT("returned the flag"), Team);
		}
	}
	else if (CharacterTeam != ShieldbreakTeams::NoTeam && !Character->GetCarriedObjective())
	{
		PickUp(Character);
		GameMode->AnnounceObjective(Character, TEXT("took the flag"), CharacterTeam);
	}
}

void AShieldbreakCarriable::PickUp(AShieldbreakCharacter* Character)
{
	Carrier = Character;
	bAtHome = false;
	DroppedServerTime = -1.f;
	Character->SetCarriedObjective(this);
	if (Kind == EShieldbreakCarriableKind::Ball)
	{
		if (AShieldbreakGameMode* GameMode = GetWorld()->GetAuthGameMode<AShieldbreakGameMode>())
		{
			GameMode->AnnounceObjective(Character, TEXT("has the ball"), ShieldbreakTeams::GetTeam(Character));
		}
	}
	OnRep_Carrier();
	ForceNetUpdate();
}

void AShieldbreakCarriable::Drop()
{
	if (!HasAuthority())
	{
		return;
	}

	FVector DropLocation = GetActorLocation();
	if (AShieldbreakCharacter* Previous = Carrier; IsValid(Previous))
	{
		DropLocation = Previous->GetActorLocation();
		if (Previous->GetCarriedObjective() == this)
		{
			Previous->SetCarriedObjective(nullptr);
		}
	}
	Carrier = nullptr;

	// Settle on the floor below.
	if (UWorld* World = GetWorld())
	{
		FHitResult Floor;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakObjectiveDrop), false, this);
		if (World->LineTraceSingleByChannel(Floor, DropLocation, DropLocation - FVector(0.f, 0.f, 3000.f), ECC_Visibility, Params))
		{
			DropLocation = Floor.ImpactPoint + FVector(0.f, 0.f, 90.f);
		}
	}

	DroppedServerTime = ShieldbreakObjectivesPrivate::ServerNow(GetWorld());
	OnRep_Carrier();
	SetActorLocation(DropLocation);
	ForceNetUpdate();
}

void AShieldbreakCarriable::ReturnHome()
{
	if (AShieldbreakCharacter* Previous = Carrier; IsValid(Previous))
	{
		if (Previous->GetCarriedObjective() == this)
		{
			Previous->SetCarriedObjective(nullptr);
		}
	}
	Carrier = nullptr;
	bAtHome = true;
	DroppedServerTime = -1.f;
	OnRep_Carrier();
	SetActorLocation(HomeLocation);
	ForceNetUpdate();
}

void AShieldbreakCarriable::OnRep_Carrier()
{
	if (Carrier)
	{
		// Flags ride on the back; the ball is held out front at chest height.
		Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AttachToComponent(Carrier->GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		SetActorRelativeLocation(Kind == EShieldbreakCarriableKind::Flag ? FVector(-30.f, 0.f, 40.f) : FVector(45.f, 20.f, 10.f));
		SetActorRelativeRotation(FRotator::ZeroRotator);
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetActorRotation(FRotator::ZeroRotator);
		Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	UpdateVisuals();
}

// ---- Hill ----

AShieldbreakHill::AShieldbreakHill()
{
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;
	SetCanBeDamaged(false);

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetBoxExtent(Extent);
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Zone->SetGenerateOverlapEvents(true);
	RootComponent = Zone;
}

void AShieldbreakHill::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShieldbreakHill, ControllingTeam);
}

void AShieldbreakHill::BeginPlay()
{
	Super::BeginPlay();
	Zone->SetBoxExtent(Extent);
	BuildBorder();
	UpdateVisuals();
}

void AShieldbreakHill::BuildBorder()
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Cube)
	{
		return;
	}

	// Four floor strips around the zone edge plus four corner posts (cube is 100 uu, so scale = size / 100).
	const float Floor = -Extent.Z + 2.f;
	struct FPiece { FVector Location; FVector Scale; };
	const FPiece Pieces[] =
	{
		{ FVector(Extent.X, 0.f, Floor), FVector(0.12f, Extent.Y / 50.f, 0.04f) },
		{ FVector(-Extent.X, 0.f, Floor), FVector(0.12f, Extent.Y / 50.f, 0.04f) },
		{ FVector(0.f, Extent.Y, Floor), FVector(Extent.X / 50.f, 0.12f, 0.04f) },
		{ FVector(0.f, -Extent.Y, Floor), FVector(Extent.X / 50.f, 0.12f, 0.04f) },
		{ FVector(Extent.X, Extent.Y, Floor + 40.f), FVector(0.12f, 0.12f, 0.8f) },
		{ FVector(-Extent.X, Extent.Y, Floor + 40.f), FVector(0.12f, 0.12f, 0.8f) },
		{ FVector(Extent.X, -Extent.Y, Floor + 40.f), FVector(0.12f, 0.12f, 0.8f) },
		{ FVector(-Extent.X, -Extent.Y, Floor + 40.f), FVector(0.12f, 0.12f, 0.8f) },
	};

	for (const FPiece& Piece : Pieces)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
		Mesh->SetStaticMesh(Cube);
		if (Material)
		{
			Mesh->SetMaterial(0, Material);
		}
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetupAttachment(Zone);
		Mesh->SetRelativeLocation(Piece.Location);
		Mesh->SetRelativeScale3D(Piece.Scale);
		Mesh->RegisterComponent();
		BorderPieces.Add(Mesh);
		BorderMIDs.Add(Mesh->CreateDynamicMaterialInstance(0));
	}
}

uint8 AShieldbreakHill::EvaluateControl()
{
	bool bTeamInside[ShieldbreakTeams::NumTeams] = { false, false };
	TArray<AActor*> Overlapping;
	Zone->GetOverlappingActors(Overlapping, AShieldbreakCharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		const AShieldbreakCharacter* Character = Cast<AShieldbreakCharacter>(Actor);
		const uint8 CharacterTeam = Character && Character->IsAlive() ? ShieldbreakTeams::GetTeam(Character) : ShieldbreakTeams::NoTeam;
		if (CharacterTeam < ShieldbreakTeams::NumTeams)
		{
			bTeamInside[CharacterTeam] = true;
		}
	}

	uint8 NewControl = ShieldbreakTeams::NoTeam;
	if (bTeamInside[0] && bTeamInside[1])
	{
		NewControl = ContestedTeam;
	}
	else if (bTeamInside[0] || bTeamInside[1])
	{
		NewControl = bTeamInside[0] ? 0 : 1;
	}

	if (NewControl != ControllingTeam)
	{
		ControllingTeam = NewControl;
		UpdateVisuals();
		ForceNetUpdate();
	}
	return ControllingTeam;
}

void AShieldbreakHill::MoveTo(const FVector& NewLocation)
{
	SetActorLocation(NewLocation);
	ControllingTeam = ShieldbreakTeams::NoTeam;
	UpdateVisuals();
	ForceNetUpdate();
}

void AShieldbreakHill::OnRep_ControllingTeam()
{
	UpdateVisuals();
}

void AShieldbreakHill::UpdateVisuals()
{
	FLinearColor Color(0.9f, 0.9f, 0.9f);
	if (ControllingTeam == ContestedTeam)
	{
		Color = FLinearColor(0.7f, 0.3f, 1.f);
	}
	else if (ControllingTeam < ShieldbreakTeams::NumTeams)
	{
		Color = ShieldbreakTeams::GetTeamColor(ControllingTeam);
	}
	for (UMaterialInstanceDynamic* MID : BorderMIDs)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}
