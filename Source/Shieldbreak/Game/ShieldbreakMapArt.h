// Per-map look: the colours of the set pieces outside the walls (Art/blender/build_map_kit.py), the ground, rocks and
// haze. Each launch map has its own mood; see docs/MAP_ART_STYLE.md. Visual only, so it never changes gameplay.

#pragma once

#include "CoreMinimal.h"

struct FShieldbreakMapLayout;

struct FShieldbreakMapLook
{
	/** Surface kind of the ground outside the walls (Grass or Stone) and its tint. */
	FName GroundStyle = TEXT("grass");
	FLinearColor Ground = FLinearColor(0.20f, 0.27f, 0.15f);
	/** Rocks, cliffs and the mountain ring. */
	FLinearColor Rock = FLinearColor(0.30f, 0.29f, 0.27f);
	/** Kit slots: main metal (M_Steel), dark metal (M_Gunmetal), carved stone (M_Ivory / M_Sand), bark and foliage. */
	FLinearColor Metal = FLinearColor(0.32f, 0.36f, 0.40f);
	FLinearColor DarkMetal = FLinearColor(0.12f, 0.14f, 0.17f);
	FLinearColor Carved = FLinearColor(0.55f, 0.52f, 0.46f);
	FLinearColor Bark = FLinearColor(0.20f, 0.13f, 0.08f);
	FLinearColor Foliage = FLinearColor(0.10f, 0.22f, 0.08f);
	/** Height-fog inscattering colour. */
	FLinearColor Haze = FLinearColor(0.32f, 0.43f, 0.6f);
};

/** One kit mesh (SM_SB_Kit_<Piece> in /Game/Shieldbreak/Art/MapKit) and where it stands, in arena-local space. */
struct FShieldbreakArtGroup
{
	FName Piece;
	TArray<FTransform> Instances;
	/** Colour for the mesh's M_TeamGlow slots (team sigils); alpha 0 keeps the imported glow. */
	FLinearColor TeamTint = FLinearColor::Transparent;
};

namespace ShieldbreakMapArt
{
	/** The look for a map id (unknown ids and Crucible get the defaults above). */
	SHIELDBREAK_API const FShieldbreakMapLook& GetLook(FName MapId);

	/** Where the kit pieces go for a built layout. Deterministic per map id, so every machine gets the same scenery.
	 *  Everything is outside the playable area, except rib tops that lean over the perimeter walls' own footprint. */
	SHIELDBREAK_API TArray<FShieldbreakArtGroup> Plan(FName MapId, const FShieldbreakMapLayout& Layout);

	/** Top of the perimeter walls above the floor (the tallest wall block on the edge). */
	SHIELDBREAK_API float PerimeterWallHeight(const FShieldbreakMapLayout& Layout);
}
