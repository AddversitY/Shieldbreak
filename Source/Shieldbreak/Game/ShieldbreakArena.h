// Builds the match's map (see Game/ShieldbreakMaps.h) from engine cubes at construction time: geometry, gravity
// lifts, spawn points, weapon/power-up placements, objective spots, the bot waypoint graph and the sky/sun.
// Not replicated: every machine builds the same deterministic copy of the map id the GameState carries.
// The arena and its pieces get the same names on every machine and are net addressable, so a piece can be a
// character's replicated movement base.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/ShieldbreakMaps.h"
#include "ShieldbreakArena.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(NotPlaceable)
class SHIELDBREAK_API AShieldbreakArena : public AActor
{
	GENERATED_BODY()

public:
	AShieldbreakArena();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	static AShieldbreakArena* Find(const UWorld* World);
	/** Spawns the arena for MapId at the origin on this machine if none exists (or replaces one built for another map). */
	static AShieldbreakArena* EnsureLocal(UWorld* World, FName MapId);

	FName GetMapId() const { return MapId; }
	const FShieldbreakMapLayout& GetLayout() const { return Layout; }

	/** Feet-height spawn points, yaw facing the center. */
	const TArray<FTransform>& GetSpawnTransforms() const { return SpawnTransforms; }

	/** Bot navigation nodes (feet height + 50). Built on the server in BeginPlay. */
	const TArray<FVector>& GetWaypoints() const { return Waypoints; }
	const TArray<int32>& GetWaypointNeighbors(int32 Index) const;
	int32 FindNearestWaypoint(const FVector& Location, bool bRequireLineOfSight = true) const;
	/** A* over the waypoint graph. OutPath includes both ends. */
	bool FindPath(int32 FromIndex, int32 ToIndex, TArray<int32>& OutPath) const;

	/** Objective modes: flag home for a team (ShieldbreakTeams::Red/Blue), hill rotation, ball spawn. */
	FVector GetFlagLocation(uint8 Team) const;
	const TArray<FVector>& GetHillLocations() const { return HillLocations; }
	FVector GetBallLocation() const { return BallLocation; }
	/** Locations of the map's weapon pickups and power-ups (for bots). */
	const TArray<FVector>& GetPickupLocations() const { return PickupLocations; }
	/** The floor, wall and ramp pieces (net addressable; see EnsureLocal). */
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetPieces() const { return Pieces; }

protected:
	virtual void BeginPlay() override;

private:
	void BuildGeometry();
	void BuildSpawnPoints();
	void SpawnLifts();
	void BuildWaypointGraph();
	void SpawnLighting();
	void SpawnPickups();
	/** Glowing trim lines along the top (and foot, on tall ones) of the structures' walls. Visual only. */
	void BuildTrim();
	/** Ground and mountains outside the walls. Visual only. */
	void BuildBackdrop();

	/** Loads the world materials made by Tools/import_art.py (missing ones fall back to the flat palette colour). */
	void LoadSurfaceMaterials();
	/** Gives Mesh its surface for a palette style: a tinted world material, the glow, or the flat colour. */
	/** TileScale > 1 stretches the world material's texture (for the huge ground and mountains). Returns its instance. */
	UMaterialInstanceDynamic* ApplySurface(UMeshComponent* Mesh, FName Style, const FLinearColor& Color, float TileScale = 1.f) const;

	UStaticMeshComponent* AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color, FName Style, const FRotator& Rotation = FRotator::ZeroRotator);
	/** Box spanning Min..Max (axis aligned). */
	UStaticMeshComponent* AddBlock(const FVector& Min, const FVector& Max, const FLinearColor& Color, FName Style);
	/** Slab whose top surface runs along the centerline from Start to End. */
	UStaticMeshComponent* AddRamp(const FVector& Start, const FVector& End, float Width, float Thickness, const FLinearColor& Color, FName Style);
	/** A visual-only component (no collision, not net addressable). */
	template <typename T>
	T* AddDecoration(const TCHAR* BaseName, UStaticMesh* Mesh);

	bool IsLinkWalkable(const FVector& From, const FVector& To) const;
	/** One-way: dropping off a ledge from From lands at To. */
	bool IsDropLink(const FVector& From, const FVector& To) const;
	void AddLink(int32 From, int32 To);

	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BaseMaterial;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> ConeMesh;
	/** World materials by surface kind (Panel, Tile, Stone, Grass), and the emissive surface for glow. */
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UMaterialInterface>> SurfaceMaterials;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> GlowMaterial;
	UPROPERTY(Transient) TArray<TObjectPtr<UPrimitiveComponent>> Decorations;

	/** Which map to build; set before construction (deferred spawn in EnsureLocal). */
	UPROPERTY()
	FName MapId;

	FShieldbreakMapLayout Layout;

	TArray<FTransform> SpawnTransforms;
	TArray<FVector> Waypoints;
	TArray<TArray<int32>> WaypointLinks;
	TArray<FVector> HillLocations;
	FVector BallLocation = FVector::ZeroVector;
	FVector FlagLocations[2] = { FVector::ZeroVector, FVector::ZeroVector };
	TArray<FVector> PickupLocations;

	bool bGeometryBuilt = false;
	bool bGraphBuilt = false;
};
