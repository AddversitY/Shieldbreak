// Greybox arena built entirely from engine cubes at construction time: floor, walls, two
// elevated bases, a two-level central tower, two long downhill slide ramps (Momentum Carry
// lanes) and cover. Also owns spawn points, the bot waypoint graph and the runtime sky/sun.
// Not replicated: every machine builds the same deterministic copy.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShieldbreakArena.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(NotPlaceable)
class SHIELDBREAK_API AShieldbreakArena : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakArena();

	virtual void OnConstruction(const FTransform& Transform) override;

	static AShieldbreakArena* Find(const UWorld* World);
	/** Spawns an arena at the origin on this machine if none exists. */
	static AShieldbreakArena* EnsureLocal(UWorld* World);

	/** Feet-height spawn points, yaw facing the center. */
	const TArray<FTransform>& GetSpawnTransforms() const { return SpawnTransforms; }

	/** Bot navigation nodes (feet height + 50). Built on the server in BeginPlay. */
	const TArray<FVector>& GetWaypoints() const { return Waypoints; }
	const TArray<int32>& GetWaypointNeighbors(int32 Index) const;
	int32 FindNearestWaypoint(const FVector& Location, bool bRequireLineOfSight = true) const;
	/** A* over the waypoint graph. OutPath includes both ends. */
	bool FindPath(int32 FromIndex, int32 ToIndex, TArray<int32>& OutPath) const;

	/** Locations of the map's weapon pickups (for bots). */
	const TArray<FVector>& GetPickupLocations() const { return PickupLocations; }

protected:
	virtual void BeginPlay() override;

private:
	void BuildGeometry();
	void BuildSpawnPoints();
	void BuildWaypointGraph();
	void SpawnLighting();
	void SpawnPickups();

	UStaticMeshComponent* AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color, const FRotator& Rotation = FRotator::ZeroRotator);
	/** Box spanning Min..Max (axis aligned). */
	UStaticMeshComponent* AddBlock(const FVector& Min, const FVector& Max, const FLinearColor& Color);
	/** Slab whose top surface runs along the centerline from Start to End. */
	UStaticMeshComponent* AddRamp(const FVector& Start, const FVector& End, float Width, float Thickness, const FLinearColor& Color);

	bool IsLinkWalkable(const FVector& From, const FVector& To) const;

	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BaseMaterial;

	TArray<FTransform> SpawnTransforms;
	TArray<FVector> Waypoints;
	TArray<TArray<int32>> WaypointLinks;
	TArray<FVector> PickupLocations;

	bool bGeometryBuilt = false;
	bool bGraphBuilt = false;
};
