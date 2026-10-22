// Map layouts as data. The layouts themselves are generated into ShieldbreakMapsData.cpp by
// Design/maps/shieldbreak_maps.py (which also validates them and renders top-down previews); AShieldbreakArena
// builds whichever one the match picked. Coordinates are map-local; red plays the -X half, blue the +X half.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Weapons/ShieldbreakPowerup.h"

class AShieldbreakWeapon;

struct FShieldbreakMapBlock
{
	FVector Min;
	FVector Max;
	FLinearColor Color;
};

struct FShieldbreakMapRamp
{
	/** Top-surface centerline. */
	FVector Start;
	FVector End;
	float Width;
	float Thickness;
	FLinearColor Color;
};

struct FShieldbreakMapLift
{
	/** Feet position at the lift pad, and where it throws you (feet position on the landing surface). */
	FVector Base;
	FVector Target;
};

struct FShieldbreakMapWeapon
{
	FVector Location;
	TSubclassOf<AShieldbreakWeapon> WeaponClass;
	float RespawnTime;
	int32 AmmoGrant;
};

struct FShieldbreakMapPowerup
{
	FVector Location;
	EShieldbreakPowerupType Type;
	float RespawnTime;
};

struct FShieldbreakMapLayout
{
	FString DisplayName;
	FString InspiredBy;
	/** Playable half size (inside the walls); the bot navigation scan covers this area. */
	FVector2D HalfExtent = FVector2D(3500.0, 2500.0);
	float FloorZ = 0.f;
	/** Top of the navigation scan. */
	float MaxZ = 1600.f;
	FRotator SunRotation = FRotator(-48.f, 35.f, 0.f);
	float FogDensity = 0.008f;

	TArray<FShieldbreakMapBlock> Blocks;
	TArray<FShieldbreakMapRamp> Ramps;
	TArray<FShieldbreakMapLift> Lifts;
	/** Feet positions; X < 0 are red-side spawns, X > 0 blue-side. */
	TArray<FVector> Spawns;
	TArray<FShieldbreakMapWeapon> Weapons;
	TArray<FShieldbreakMapPowerup> Powerups;

	FVector FlagLocations[2] = { FVector::ZeroVector, FVector::ZeroVector };
	TArray<FVector> HillLocations;
	FVector BallLocation = FVector::ZeroVector;
};

namespace ShieldbreakMaps
{
	/** Every playable map id, launch maps first. */
	SHIELDBREAK_API const TArray<FName>& GetMapIds();
	SHIELDBREAK_API FName GetDefaultMapId();
	/** Fills OutLayout for MapId (unknown ids fall back to the default) and returns the id actually built. */
	SHIELDBREAK_API FName Build(FName MapId, FShieldbreakMapLayout& OutLayout);
}
