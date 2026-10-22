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
#include "UObject/UObjectGlobals.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/ShieldbreakWeaponPickup.h"
#include "Weapons/ShieldbreakPowerup.h"
#include "Game/ShieldbreakGravLift.h"

namespace ShieldbreakArenaPrivate
{
	constexpr float WaypointLift = 50.f;
	/** Grid spacing of the navigation scan, and the longest walk link (covers two grid steps and diagonals). */
	constexpr float NavGridStep = 300.f;
	constexpr float MaxLinkDistance = 650.f;
	constexpr float MaxDropHeight = 700.f;

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

AShieldbreakArena* AShieldbreakArena::EnsureLocal(UWorld* World, FName InMapId)
{
	if (AShieldbreakArena* Existing = Find(World))
	{
		if (InMapId.IsNone() || Existing->GetMapId() == InMapId)
		{
			return Existing;
		}
		// Built for another map (e.g. a client that guessed before the map id replicated): rebuild.
		Existing->Destroy();
	}
	if (!World)
	{
		return nullptr;
	}

	const FName ResolvedMapId = InMapId.IsNone() ? ShieldbreakMaps::GetDefaultMapId() : InMapId;

	// The arena isn't replicated, but characters stand on its pieces, and movement sends the floor under the player
	// (the movement base) over the network by name. So every machine gives the arena and its pieces the same names
	// and marks them net addressable; otherwise the host can't resolve the client's floor and the client's movement
	// corrections stutter. A destroyed arena may still hold the name until garbage collection: move it aside.
	const FName StableName(*FString::Printf(TEXT("ShieldbreakArena_%s"), *ResolvedMapId.ToString()));
	if (UObject* Stale = StaticFindObjectFast(nullptr, World->PersistentLevel, StableName))
	{
		Stale->Rename(nullptr, nullptr, REN_DontCreateRedirectors | REN_NonTransactional);
	}

	// Deferred so MapId is set before OnConstruction builds the geometry.
	FActorSpawnParameters Params;
	Params.Name = StableName;
	Params.OverrideLevel = World->PersistentLevel;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	AShieldbreakArena* Arena = World->SpawnActor<AShieldbreakArena>(AShieldbreakArena::StaticClass(), FTransform::Identity, Params);
	if (Arena)
	{
		Arena->MapId = ResolvedMapId;
		Arena->SetNetAddressable();
		Arena->FinishSpawning(FTransform::Identity);
	}
	return Arena;
}

void AShieldbreakArena::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!bGeometryBuilt)
	{
		bGeometryBuilt = true;
		MapId = ShieldbreakMaps::Build(MapId.IsNone() ? ShieldbreakMaps::GetDefaultMapId() : MapId, Layout);
		BuildGeometry();
		BuildSpawnPoints();
		UE_LOG(LogShieldbreak, Log, TEXT("Built map %s (%s): %d blocks, %d ramps, %d lifts, %d spawns."), *Layout.DisplayName,
			*Layout.InspiredBy, Layout.Blocks.Num(), Layout.Ramps.Num(), Layout.Lifts.Num(), Layout.Spawns.Num());
	}
}

void AShieldbreakArena::BeginPlay()
{
	Super::BeginPlay();

	// Lifts exist on every machine so the owning client can predict its launch.
	SpawnLifts();

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
	// Same name on every machine (pieces are built in layout order), so it can be the movement base over the network.
	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this, FName(TEXT("Piece"), Pieces.Num() + 1), RF_Transient);
	if (!Piece)
	{
		return nullptr;
	}
	Piece->SetNetAddressable();

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

// ---- Layout ----

void AShieldbreakArena::BuildGeometry()
{
	for (const FShieldbreakMapBlock& Block : Layout.Blocks)
	{
		AddBlock(Block.Min, Block.Max, Block.Color);
	}
	for (const FShieldbreakMapRamp& RampData : Layout.Ramps)
	{
		AddRamp(RampData.Start, RampData.End, RampData.Width, RampData.Thickness, RampData.Color);
	}
}

void AShieldbreakArena::BuildSpawnPoints()
{
	const FTransform& ToWorld = GetActorTransform();
	SpawnTransforms.Reset();
	for (const FVector& Feet : Layout.Spawns)
	{
		// Face the middle of the map.
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Feet.Y, -Feet.X));
		SpawnTransforms.Add(FTransform(FRotator(0.f, Yaw, 0.f), ToWorld.TransformPosition(Feet)));
	}

	FlagLocations[0] = ToWorld.TransformPosition(Layout.FlagLocations[0]);
	FlagLocations[1] = ToWorld.TransformPosition(Layout.FlagLocations[1]);
	HillLocations.Reset();
	for (const FVector& Hill : Layout.HillLocations)
	{
		HillLocations.Add(ToWorld.TransformPosition(Hill));
	}
	BallLocation = ToWorld.TransformPosition(Layout.BallLocation);

	PickupLocations.Reset();
	for (const FShieldbreakMapWeapon& Weapon : Layout.Weapons)
	{
		PickupLocations.Add(ToWorld.TransformPosition(Weapon.Location));
	}
	for (const FShieldbreakMapPowerup& Powerup : Layout.Powerups)
	{
		PickupLocations.Add(ToWorld.TransformPosition(Powerup.Location));
	}
}

void AShieldbreakArena::SpawnLifts()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const FShieldbreakMapLift& LiftData : Layout.Lifts)
	{
		const FVector Base = GetActorTransform().TransformPosition(LiftData.Base);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Params.bDeferConstruction = true;
		if (AShieldbreakGravLift* Lift = World->SpawnActor<AShieldbreakGravLift>(AShieldbreakGravLift::StaticClass(), Base + FVector(0.f, 0.f, 60.f), FRotator::ZeroRotator, Params))
		{
			Lift->SetTarget(GetActorTransform().TransformPosition(LiftData.Target));
			Lift->FinishSpawning(FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, 60.f)));
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

	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0.f, 0.f, 3000.f), Layout.SunRotation, Params))
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

	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector(0.f, 0.f, Layout.FloorZ - 200.f), FRotator::ZeroRotator, Params))
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			FogComponent->SetFogDensity(Layout.FogDensity);
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

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;

	for (const FShieldbreakMapWeapon& Spot : Layout.Weapons)
	{
		const FVector Location = GetActorTransform().TransformPosition(Spot.Location);
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

	for (const FShieldbreakMapPowerup& Spot : Layout.Powerups)
	{
		const FVector Location = GetActorTransform().TransformPosition(Spot.Location);
		if (AShieldbreakPowerup* Powerup = World->SpawnActor<AShieldbreakPowerup>(AShieldbreakPowerup::StaticClass(), Location, FRotator::ZeroRotator, Params))
		{
			Powerup->SetType(Spot.Type);
			Powerup->RespawnTime = Spot.RespawnTime;
		}
	}
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

bool AShieldbreakArena::IsDropLink(const FVector& From, const FVector& To) const
{
	const UWorld* World = GetWorld();
	const float Drop = static_cast<float>(From.Z - To.Z);
	if (!World || Drop < 60.f || Drop > ShieldbreakArenaPrivate::MaxDropHeight || FVector::Dist2D(From, To) > 500.f)
	{
		return false;
	}

	// Walk off at chest height toward the spot above To, then fall straight down onto it.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakWaypointDrop), false);
	FHitResult Hit;
	const FVector Above(To.X, To.Y, From.Z + 130.f);
	if (World->LineTraceSingleByObjectType(Hit, From + FVector(0.f, 0.f, 130.f), Above, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}
	return !World->LineTraceSingleByObjectType(Hit, Above, To + FVector(0.f, 0.f, 40.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

void AShieldbreakArena::AddLink(int32 From, int32 To)
{
	WaypointLinks[From].AddUnique(To);
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

	// Scan a grid over the map and drop a node on every walkable surface in each column (floor, platforms,
	// bridges, ramps), as long as a standing capsule fits. Works for any layout without hand-placed nodes.
	const FTransform& ToWorld = GetActorTransform();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShieldbreakWaypointGrid), false);
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(38.f, 90.f);
	TArray<FVector> Feet;

	const float HalfX = static_cast<float>(Layout.HalfExtent.X) - 120.f;
	const float HalfY = static_cast<float>(Layout.HalfExtent.Y) - 120.f;
	for (float X = -HalfX; X <= HalfX; X += NavGridStep)
	{
		for (float Y = -HalfY; Y <= HalfY; Y += NavGridStep)
		{
			float TopZ = Layout.MaxZ;
			for (int32 Guard = 0; Guard < 200 && TopZ > Layout.FloorZ - 50.f; ++Guard)
			{
				FHitResult Hit;
				const FVector Start = ToWorld.TransformPosition(FVector(X, Y, TopZ));
				const FVector End = ToWorld.TransformPosition(FVector(X, Y, Layout.FloorZ - 60.f));
				if (!World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
				{
					break;
				}
				if (Hit.bStartPenetrating)
				{
					TopZ -= 20.f; // started inside a block: step down through it
					continue;
				}

				const FVector Surface = Hit.ImpactPoint;
				const bool bWalkable = Hit.ImpactNormal.Z >= 0.7f;
				const bool bFits = !World->OverlapAnyTestByObjectType(Surface + FVector(0.f, 0.f, 97.f), FQuat::Identity,
					FCollisionObjectQueryParams(ECC_WorldStatic), Capsule, Params);
				if (bWalkable && bFits)
				{
					Feet.Add(Surface);
				}
				TopZ = static_cast<float>(ToWorld.InverseTransformPosition(Surface).Z) - 5.f;
			}
		}
	}

	// Lift pads and landings are nodes too, so lifts can be one-way links.
	TArray<TPair<int32, int32>> LiftNodes;
	for (const FShieldbreakMapLift& LiftData : Layout.Lifts)
	{
		const int32 BaseIndex = Feet.Add(ToWorld.TransformPosition(LiftData.Base));
		const int32 TargetIndex = Feet.Add(ToWorld.TransformPosition(LiftData.Target));
		LiftNodes.Add(TPair<int32, int32>(BaseIndex, TargetIndex));
	}

	Waypoints.Reset(Feet.Num());
	for (const FVector& Foot : Feet)
	{
		Waypoints.Add(Foot + FVector(0.f, 0.f, WaypointLift));
	}

	WaypointLinks.Reset();
	WaypointLinks.SetNum(Waypoints.Num());
	int32 WalkLinks = 0;
	int32 DropLinks = 0;
	for (int32 A = 0; A < Feet.Num(); ++A)
	{
		for (int32 B = A + 1; B < Feet.Num(); ++B)
		{
			const double Distance2D = FVector::Dist2D(Feet[A], Feet[B]);
			if (Distance2D > MaxLinkDistance)
			{
				continue;
			}
			const double Rise = FMath::Abs(Feet[A].Z - Feet[B].Z);
			// Walkable both ways: no steeper than ~33 degrees on average and clear all along.
			if (Rise <= 0.65 * Distance2D + 20.0 && IsLinkWalkable(Feet[A], Feet[B]))
			{
				AddLink(A, B);
				AddLink(B, A);
				++WalkLinks;
				continue;
			}
			// Otherwise, possibly a one-way drop off a ledge.
			if (IsDropLink(Feet[A], Feet[B]))
			{
				AddLink(A, B);
				++DropLinks;
			}
			else if (IsDropLink(Feet[B], Feet[A]))
			{
				AddLink(B, A);
				++DropLinks;
			}
		}
	}
	for (const TPair<int32, int32>& Lift : LiftNodes)
	{
		AddLink(Lift.Key, Lift.Value);
	}

	UE_LOG(LogShieldbreak, Log, TEXT("%s waypoint graph: %d nodes, %d walk links, %d drop links, %d lifts."),
		*Layout.DisplayName, Waypoints.Num(), WalkLinks, DropLinks, LiftNodes.Num());
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

FVector AShieldbreakArena::GetFlagLocation(uint8 Team) const
{
	return Team < 2 ? FlagLocations[Team] : GetActorLocation();
}
