#include "Game/ShieldbreakArena.h"

#include "Shieldbreak.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/HitResult.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/ShieldbreakWeaponClasses.h"
#include "Weapons/ShieldbreakWeaponPickup.h"
#include "Weapons/ShieldbreakPowerup.h"

namespace ShieldbreakArenaPrivate
{
	// Region tints.
	const FLinearColor FloorColor(0.2f, 0.21f, 0.23f);
	const FLinearColor WallColor(0.16f, 0.17f, 0.19f);
	const FLinearColor BaseAColor(0.55f, 0.16f, 0.13f);
	const FLinearColor BaseBColor(0.13f, 0.24f, 0.55f);
	const FLinearColor TowerColor(0.62f, 0.62f, 0.58f);
	const FLinearColor RampColor(0.70f, 0.58f, 0.22f);
	const FLinearColor SlideColor(0.85f, 0.40f, 0.08f);
	const FLinearColor SkyColor(0.36f, 0.40f, 0.30f);
	const FLinearColor CoverColor(0.25f, 0.38f, 0.28f);

	constexpr float HalfX = 3500.f;
	constexpr float HalfY = 2500.f;
	constexpr float WallHeight = 900.f;
	constexpr float WaypointLift = 50.f;
	constexpr float MaxLinkDistance = 1400.f;

	const TArray<int32> EmptyNeighbors;
}

AShieldbreakArena::AShieldbreakArena()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CubeFinder.Succeeded())
	{
		CubeMesh = CubeFinder.Object;
	}
	if (MaterialFinder.Succeeded())
	{
		BaseMaterial = MaterialFinder.Object;
	}
}

AShieldbreakArena* AShieldbreakArena::Find(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AShieldbreakArena> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

AShieldbreakArena* AShieldbreakArena::EnsureLocal(UWorld* World)
{
	if (AShieldbreakArena* Existing = Find(World))
	{
		return Existing;
	}
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	return World->SpawnActor<AShieldbreakArena>(AShieldbreakArena::StaticClass(), FTransform::Identity, Params);
}

void AShieldbreakArena::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!bGeometryBuilt)
	{
		bGeometryBuilt = true;
		BuildGeometry();
		BuildSpawnPoints();
	}
}

void AShieldbreakArena::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		SpawnLighting();
	}

	if (HasAuthority() && GetNetMode() != NM_Client)
	{
		BuildWaypointGraph();
		SpawnPickups();
	}
}

// ---- Geometry helpers ----

UStaticMeshComponent* AShieldbreakArena::AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color, const FRotator& Rotation)
{
	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	if (!Piece)
	{
		return nullptr;
	}

	Piece->SetMobility(EComponentMobility::Static);
	if (CubeMesh)
	{
		Piece->SetStaticMesh(CubeMesh);
	}
	if (BaseMaterial)
	{
		Piece->SetMaterial(0, BaseMaterial);
	}
	Piece->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Piece->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);
	Piece->SetGenerateOverlapEvents(false);
	Piece->SetupAttachment(Root);
	Piece->SetRelativeLocationAndRotation(Center, Rotation);
	Piece->SetRelativeScale3D(Size / 100.f);
	Piece->RegisterComponent();
	AddInstanceComponent(Piece);

	if (UMaterialInstanceDynamic* MID = Piece->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}

	Pieces.Add(Piece);
	return Piece;
}

UStaticMeshComponent* AShieldbreakArena::AddBlock(const FVector& Min, const FVector& Max, const FLinearColor& Color)
{
	return AddBox((Min + Max) * 0.5f, (Max - Min).GetAbs(), Color);
}

UStaticMeshComponent* AShieldbreakArena::AddRamp(const FVector& Start, const FVector& End, float Width, float Thickness, const FLinearColor& Color)
{
	const FVector Delta = End - Start;
	const float Length = static_cast<float>(Delta.Size()) + 30.f; // overlap both ends a little
	const FRotator Rotation = Delta.Rotation();
	const FVector Up = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z);
	const FVector Center = (Start + End) * 0.5f - Up * (Thickness * 0.5f);
	return AddBox(Center, FVector(Length, Width, Thickness), Color, Rotation);
}

// ---- Layout (rotationally symmetric under (x, y) -> (-x, -y)) ----

void AShieldbreakArena::BuildGeometry()
{
	using namespace ShieldbreakArenaPrivate;

	// Floor and outer walls.
	AddBlock(FVector(-HalfX - 100.f, -HalfY - 100.f, -100.f), FVector(HalfX + 100.f, HalfY + 100.f, 0.f), FloorColor);
	AddBlock(FVector(-HalfX - 100.f, HalfY, 0.f), FVector(HalfX + 100.f, HalfY + 100.f, WallHeight), WallColor);
	AddBlock(FVector(-HalfX - 100.f, -HalfY - 100.f, 0.f), FVector(HalfX + 100.f, -HalfY, WallHeight), WallColor);
	AddBlock(FVector(HalfX, -HalfY, 0.f), FVector(HalfX + 100.f, HalfY, WallHeight), WallColor);
	AddBlock(FVector(-HalfX - 100.f, -HalfY, 0.f), FVector(-HalfX, HalfY, WallHeight), WallColor);

	// Everything below is placed for side A and mirrored for side B.
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float S = Side == 0 ? 1.f : -1.f;
		const FLinearColor BaseColor = Side == 0 ? BaseAColor : BaseBColor;
		auto P = [S](float X, float Y, float Z) { return FVector(X * S, Y * S, Z); };
		auto Block = [this, S](float X0, float Y0, float Z0, float X1, float Y1, float Z1, const FLinearColor& Color)
		{
			const FVector A(X0 * S, Y0 * S, Z0);
			const FVector B(X1 * S, Y1 * S, Z1);
			AddBlock(FVector(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), Z0), FVector(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), Z1), Color);
		};

		// Base: raised platform (top 300) against the end wall.
		Block(-3500.f, -700.f, 0.f, -2500.f, 700.f, 300.f, BaseColor);
		// Base ramp down toward the middle.
		AddRamp(P(-2500.f, 450.f, 300.f), P(-1800.f, 450.f, 0.f), 300.f, 40.f, RampColor);
		// Low wall on the base lip: crouch cover.
		Block(-2620.f, -500.f, 300.f, -2580.f, 100.f, 410.f, BaseColor * 0.7f);

		// Ramp from the base up to the sky platform (top 450).
		AddRamp(P(-3000.f, 700.f, 300.f), P(-3000.f, 1500.f, 450.f), 300.f, 40.f, RampColor);
		// Sky platform: a floating slab, open underneath.
		Block(-3500.f, 1500.f, 410.f, -1600.f, 2500.f, 450.f, SkyColor);
		// Long downhill slide ramp: the Momentum Carry lane.
		AddRamp(P(-1600.f, 1900.f, 450.f), P(1400.f, 1900.f, 0.f), 400.f, 40.f, SlideColor);

		// Tower access ramp (to level 1, top 350).
		AddRamp(P(300.f, 175.f, 350.f), P(1100.f, 175.f, 0.f), 250.f, 40.f, RampColor);

		// Cover.
		Block(1200.f, -900.f, 0.f, 1400.f, -700.f, 150.f, CoverColor);   // crate
		Block(825.f, 1025.f, 0.f, 975.f, 1175.f, 150.f, CoverColor);     // small crate (jump ledge)
		Block(1940.f, 940.f, 0.f, 2060.f, 1060.f, 600.f, WallColor);     // tall pillar
		Block(1600.f, 200.f, 0.f, 1700.f, 300.f, 400.f, WallColor);      // pillar near base ramp
		Block(-300.f, 1120.f, 0.f, 300.f, 1180.f, 110.f, CoverColor);    // low wall
	}

	// Central tower: level 1 (top 350) and level 2 (top 500) with a short ramp; the east face
	// of level 2 is a 150 uu ledge for jumps.
	AddBlock(FVector(-300.f, -300.f, 0.f), FVector(300.f, 300.f, 350.f), TowerColor);
	AddBlock(FVector(-300.f, 0.f, 350.f), FVector(0.f, 300.f, 500.f), TowerColor * 0.85f);
	AddRamp(FVector(-150.f, -280.f, 350.f), FVector(-150.f, 0.f, 500.f), 200.f, 30.f, RampColor);
}

void AShieldbreakArena::BuildSpawnPoints()
{
	SpawnTransforms.Reset();

	const FVector Spots[] =
	{
		FVector(-3000.f, 0.f, 300.f),
		FVector(-3000.f, -1900.f, 0.f),
		FVector(-2200.f, 2000.f, 450.f),
		FVector(-600.f, 1400.f, 0.f),
		FVector(-2200.f, -1000.f, 0.f),
		FVector(-1100.f, -1500.f, 0.f),
	};

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float S = Side == 0 ? 1.f : -1.f;
		for (const FVector& Spot : Spots)
		{
			const FVector Location(Spot.X * S, Spot.Y * S, Spot.Z);
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Location.Y, -Location.X));
			SpawnTransforms.Add(FTransform(FRotator(0.f, Yaw, 0.f), GetActorTransform().TransformPosition(Location)));
		}
	}
}

// ---- Lighting ----

void AShieldbreakArena::SpawnLighting()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0.f, 0.f, 3000.f), FRotator(-48.f, 35.f, 0.f), Params))
	{
		if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			SunLight->SetMobility(EComponentMobility::Movable);
			SunLight->SetIntensity(8.f);
			SunLight->SetAtmosphereSunLight(true);
			SunLight->SetCastShadows(true);
		}
	}

	if (ASkyAtmosphere* Atmosphere = World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FTransform::Identity, Params))
	{
		(void)Atmosphere;
	}

	if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector(0.f, 0.f, 2000.f), FRotator::ZeroRotator, Params))
	{
		if (USkyLightComponent* SkyComponent = Sky->GetLightComponent())
		{
			SkyComponent->SetMobility(EComponentMobility::Movable);
			SkyComponent->bRealTimeCapture = true;
			SkyComponent->SetIntensity(1.5f);
			SkyComponent->RecaptureSky();
		}
	}

	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector(0.f, 0.f, -200.f), FRotator::ZeroRotator, Params))
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			FogComponent->SetFogDensity(0.008f);
		}
	}
}

// ---- Pickups ----

void AShieldbreakArena::SpawnPickups()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Power weapons sit on contested high ground with long timers: rockets on the tower top, one sniper at the
	// far end of each sky platform. Plasma rifles stay on the floor lanes.
	struct FPickupSpot
	{
		FVector Location;
		TSubclassOf<AShieldbreakWeapon> WeaponClass;
		float RespawnTime;
		int32 AmmoGrant;
	};
	const FPickupSpot Spots[] =
	{
		{ FVector(2600.f, 1900.f, 70.f), AWeapon_PlasmaRifle::StaticClass(), 30.f, 0 },
		{ FVector(-2600.f, -1900.f, 70.f), AWeapon_PlasmaRifle::StaticClass(), 30.f, 0 },
		{ FVector(-150.f, 150.f, 570.f), AWeapon_RocketLauncher::StaticClass(), 120.f, 2 },
		{ FVector(-3300.f, 2300.f, 520.f), AWeapon_SniperRifle::StaticClass(), 90.f, 4 },
		{ FVector(3300.f, -2300.f, 520.f), AWeapon_SniperRifle::StaticClass(), 90.f, 4 },
	};

	PickupLocations.Reset();
	for (const FPickupSpot& Spot : Spots)
	{
		const FVector Location = GetActorTransform().TransformPosition(Spot.Location);
		PickupLocations.Add(Location);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		if (AShieldbreakWeaponPickup* Pickup = World->SpawnActor<AShieldbreakWeaponPickup>(AShieldbreakWeaponPickup::StaticClass(), Location, FRotator::ZeroRotator, Params))
		{
			Pickup->SetWeaponClass(Spot.WeaponClass);
			Pickup->RespawnTime = Spot.RespawnTime;
			if (Spot.AmmoGrant > 0)
			{
				Pickup->AmmoGrant = Spot.AmmoGrant;
			}
		}
	}

	// Overshield on the tower's lower level, under the rockets.
	FActorSpawnParameters PowerupParams;
	PowerupParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PowerupParams.ObjectFlags |= RF_Transient;
	World->SpawnActor<AShieldbreakPowerup>(AShieldbreakPowerup::StaticClass(),
		GetActorTransform().TransformPosition(FVector(150.f, -150.f, 420.f)), FRotator::ZeroRotator, PowerupParams);
}

// ---- Bot navigation ----

const TArray<int32>& AShieldbreakArena::GetWaypointNeighbors(int32 Index) const
{
	return WaypointLinks.IsValidIndex(Index) ? WaypointLinks[Index] : ShieldbreakArenaPrivate::EmptyNeighbors;
}

bool AShieldbreakArena::IsLinkWalkable(const FVector& From, const FVector& To) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// From/To are feet positions. Clear at knee and chest height...
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakWaypointLink), false);
	FHitResult Hit;
	for (const float Lift : { 55.f, 130.f })
	{
		if (World->LineTraceSingleByObjectType(Hit, From + FVector(0.f, 0.f, Lift), To + FVector(0.f, 0.f, Lift), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			return false;
		}
	}

	// ...and solid ground all along, never far below the straight line (no gaps or drops).
	const float Distance = static_cast<float>(FVector::Dist2D(From, To));
	const int32 Samples = FMath::Max(2, FMath::CeilToInt(Distance / 100.f));
	for (int32 Step = 1; Step < Samples; ++Step)
	{
		const FVector Point = FMath::Lerp(From, To, static_cast<float>(Step) / Samples);
		if (!World->LineTraceSingleByObjectType(Hit, Point + FVector(0.f, 0.f, 60.f), Point - FVector(0.f, 0.f, 90.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			return false;
		}
	}
	return true;
}

void AShieldbreakArena::BuildWaypointGraph()
{
	using namespace ShieldbreakArenaPrivate;

	if (bGraphBuilt)
	{
		return;
	}
	bGraphBuilt = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FVector> Feet;

	// Hand-placed nodes: ramp ends, platforms, tower levels (side A, mirrored for B).
	const FVector Placed[] =
	{
		FVector(-3000.f, 0.f, 300.f), FVector(-2700.f, -450.f, 300.f),
		FVector(-2560.f, 450.f, 300.f), FVector(-1740.f, 450.f, 0.f),      // base ramp
		FVector(-3000.f, 640.f, 300.f), FVector(-3000.f, 1560.f, 450.f),   // sky ramp
		FVector(-2600.f, 2000.f, 450.f),
		FVector(-1660.f, 1900.f, 450.f), FVector(1460.f, 1900.f, 0.f),     // slide ramp
		FVector(1160.f, 175.f, 0.f), FVector(240.f, 175.f, 350.f),         // tower ramp
		FVector(200.f, -200.f, 350.f),
	};
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float S = Side == 0 ? 1.f : -1.f;
		for (const FVector& Spot : Placed)
		{
			Feet.Add(FVector(Spot.X * S, Spot.Y * S, Spot.Z));
		}
	}
	Feet.Add(FVector(-150.f, -290.f, 350.f));
	Feet.Add(FVector(-150.f, 60.f, 500.f));
	Feet.Add(FVector(-150.f, 200.f, 500.f));

	// Floor grid wherever there is open floor with headroom.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakWaypointGrid), false);
	for (float X = -3200.f; X <= 3200.f; X += 400.f)
	{
		for (float Y = -2200.f; Y <= 2200.f; Y += 440.f)
		{
			FHitResult Hit;
			const FVector Top(X, Y, 1500.f);
			if (!World->LineTraceSingleByObjectType(Hit, Top, FVector(X, Y, -50.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				continue;
			}
			// Floor spots, or floor under a high slab/ramp, as long as a standing capsule fits.
			if (Hit.ImpactPoint.Z > 5.f && Hit.ImpactPoint.Z < 250.f)
			{
				continue;
			}
			const FCollisionShape Capsule = FCollisionShape::MakeCapsule(38.f, 90.f);
			if (World->OverlapAnyTestByObjectType(FVector(X, Y, 100.f), FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Capsule, Params))
			{
				continue;
			}
			Feet.Add(FVector(X, Y, 0.f));
		}
	}

	// Transform into world space.
	Waypoints.Reset(Feet.Num());
	for (const FVector& Foot : Feet)
	{
		Waypoints.Add(GetActorTransform().TransformPosition(Foot) + FVector(0.f, 0.f, WaypointLift));
	}

	WaypointLinks.SetNum(Waypoints.Num());
	int32 LinkCount = 0;
	for (int32 A = 0; A < Waypoints.Num(); ++A)
	{
		for (int32 B = A + 1; B < Waypoints.Num(); ++B)
		{
			const FVector FootA = Waypoints[A] - FVector(0.f, 0.f, WaypointLift);
			const FVector FootB = Waypoints[B] - FVector(0.f, 0.f, WaypointLift);
			if (FVector::Dist2D(FootA, FootB) > MaxLinkDistance)
			{
				continue;
			}
			// Steeper than ~33 degrees on average is not a walkable straight line.
			if (FMath::Abs(FootA.Z - FootB.Z) > 0.65f * FVector::Dist2D(FootA, FootB) + 20.f)
			{
				continue;
			}
			if (IsLinkWalkable(FootA, FootB))
			{
				WaypointLinks[A].Add(B);
				WaypointLinks[B].Add(A);
				++LinkCount;
			}
		}
	}

	UE_LOG(LogShieldbreak, Log, TEXT("Arena waypoint graph: %d nodes, %d links."), Waypoints.Num(), LinkCount);
}

int32 AShieldbreakArena::FindNearestWaypoint(const FVector& Location, bool bRequireLineOfSight) const
{
	const UWorld* World = GetWorld();
	int32 Best = INDEX_NONE;
	double BestScore = TNumericLimits<double>::Max();

	for (int32 Index = 0; Index < Waypoints.Num(); ++Index)
	{
		const FVector Delta = Waypoints[Index] - Location;
		// Height differences count triple so we prefer nodes on our own level.
		const double Score = FVector(Delta.X, Delta.Y, Delta.Z * 3.0).SizeSquared();
		if (Score >= BestScore)
		{
			continue;
		}
		if (bRequireLineOfSight && World)
		{
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakNearestWaypoint), false);
			if (World->LineTraceSingleByObjectType(Hit, Location, Waypoints[Index], FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				continue;
			}
		}
		Best = Index;
		BestScore = Score;
	}

	if (Best == INDEX_NONE && bRequireLineOfSight)
	{
		return FindNearestWaypoint(Location, false);
	}
	return Best;
}

bool AShieldbreakArena::FindPath(int32 FromIndex, int32 ToIndex, TArray<int32>& OutPath) const
{
	OutPath.Reset();
	if (!Waypoints.IsValidIndex(FromIndex) || !Waypoints.IsValidIndex(ToIndex))
	{
		return false;
	}
	if (FromIndex == ToIndex)
	{
		OutPath.Add(FromIndex);
		return true;
	}

	const int32 Count = Waypoints.Num();
	TArray<float> GScore;
	TArray<int32> CameFrom;
	TArray<bool> Closed;
	GScore.Init(TNumericLimits<float>::Max(), Count);
	CameFrom.Init(INDEX_NONE, Count);
	Closed.Init(false, Count);

	struct FOpenEntry
	{
		int32 Index;
		float F;
		bool operator<(const FOpenEntry& Other) const { return F < Other.F; }
	};

	TArray<FOpenEntry> Open;
	GScore[FromIndex] = 0.f;
	Open.HeapPush({ FromIndex, static_cast<float>(FVector::Dist(Waypoints[FromIndex], Waypoints[ToIndex])) });

	while (Open.Num() > 0)
	{
		FOpenEntry Current;
		Open.HeapPop(Current);
		if (Closed[Current.Index])
		{
			continue;
		}
		if (Current.Index == ToIndex)
		{
			for (int32 Walk = ToIndex; Walk != INDEX_NONE; Walk = CameFrom[Walk])
			{
				OutPath.Insert(Walk, 0);
			}
			return true;
		}
		Closed[Current.Index] = true;

		for (const int32 Next : WaypointLinks[Current.Index])
		{
			if (Closed[Next])
			{
				continue;
			}
			const float Tentative = GScore[Current.Index] + static_cast<float>(FVector::Dist(Waypoints[Current.Index], Waypoints[Next]));
			if (Tentative < GScore[Next])
			{
				GScore[Next] = Tentative;
				CameFrom[Next] = Current.Index;
				Open.HeapPush({ Next, Tentative + static_cast<float>(FVector::Dist(Waypoints[Next], Waypoints[ToIndex])) });
			}
		}
	}
	return false;
}
