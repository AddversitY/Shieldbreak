#include "Game/ShieldbreakMapArt.h"

#include "Game/ShieldbreakMaps.h"
#include "Math/RandomStream.h"

namespace ShieldbreakMapArtPrivate
{
	/** The ground outside the walls is a slab whose top is this far below the arena floor (see BuildBackdrop). */
	constexpr float GroundDrop = 350.f;
	/** Perimeter walls are this thick, outside the playable half extent. */
	constexpr float WallThickness = 100.f;

	/** One perimeter wall: its outward normal, the direction along it, the distance of its inner face from the
	 *  centre and its half length. */
	struct FSide
	{
		FVector2D Normal;
		FVector2D Tangent;
		float Out;
		float Along;
	};

	TArray<FSide, TFixedAllocator<4>> Sides(const FShieldbreakMapLayout& Layout)
	{
		const float HX = static_cast<float>(Layout.HalfExtent.X), HY = static_cast<float>(Layout.HalfExtent.Y);
		TArray<FSide, TFixedAllocator<4>> Out;
		Out.Add({ FVector2D(1.0, 0.0), FVector2D(0.0, 1.0), HX, HY });
		Out.Add({ FVector2D(-1.0, 0.0), FVector2D(0.0, -1.0), HX, HY });
		Out.Add({ FVector2D(0.0, 1.0), FVector2D(-1.0, 0.0), HY, HX });
		Out.Add({ FVector2D(0.0, -1.0), FVector2D(1.0, 0.0), HY, HX });
		return Out;
	}

	bool IsLongSide(const FSide& Side) { return Side.Along >= Side.Out; }

	/** A piece standing on the ground Distance outside the wall's outer face, its local +X facing the arena. */
	FTransform FacingArena(const FShieldbreakMapLayout& Layout, const FSide& Side, float Distance, float Along, const FVector& Scale)
	{
		const FVector2D At = Side.Normal * (Side.Out + WallThickness + Distance) + Side.Tangent * Along;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Side.Normal.Y, -Side.Normal.X));
		return FTransform(FRotator(0.f, Yaw, 0.f), FVector(At.X, At.Y, Layout.FloorZ - GroundDrop), Scale);
	}

	/** Evenly spaced positions along a side, keeping Margin clear of the corners. */
	TArray<float> Stations(const FSide& Side, float Spacing, float Margin)
	{
		TArray<float> Out;
		const float Span = Side.Along - Margin;
		const int32 Count = FMath::Max(1, FMath::FloorToInt(2.f * Span / Spacing) + 1);
		const float Step = Count > 1 ? 2.f * Span / (Count - 1) : 0.f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Out.Add(Count > 1 ? -Span + Step * Index : 0.f);
		}
		return Out;
	}

	struct FScatter
	{
		int32 Count = 0;
		/** Band outside the walls' outer faces, in cm. */
		float Inner = 800.f;
		float Outer = 6000.f;
		float ScaleMin = 1.f;
		float ScaleMax = 1.f;
		/** No two instances of this group closer than this (scaled by their size). */
		float Spacing = 600.f;
	};

	/** Random positions in a rectangular band around the walls, inside the mountain ring, with random yaw and a
	 *  little uneven stretch so repeats don't read as copies. */
	void Scatter(TArray<FTransform>& Out, FRandomStream& Random, const FShieldbreakMapLayout& Layout, const FScatter& Spec)
	{
		const float HX = static_cast<float>(Layout.HalfExtent.X) + WallThickness;
		const float HY = static_cast<float>(Layout.HalfExtent.Y) + WallThickness;
		// The mountain ring's inner edge (BuildBackdrop) sits this far from the centre.
		const float RingRadius = static_cast<float>(FMath::Max(Layout.HalfExtent.X, Layout.HalfExtent.Y)) + 3000.f;
		const int32 First = Out.Num();
		for (int32 Attempt = 0; Attempt < Spec.Count * 20 && Out.Num() - First < Spec.Count; ++Attempt)
		{
			const float X = Random.FRandRange(-HX - Spec.Outer, HX + Spec.Outer);
			const float Y = Random.FRandRange(-HY - Spec.Outer, HY + Spec.Outer);
			const bool bInside = FMath::Abs(X) < HX + Spec.Inner && FMath::Abs(Y) < HY + Spec.Inner;
			if (bInside || FMath::Sqrt(X * X + Y * Y) > RingRadius + 2500.f)
			{
				continue;
			}
			const float Scale = Random.FRandRange(Spec.ScaleMin, Spec.ScaleMax);
			bool bCrowded = false;
			for (int32 Index = First; Index < Out.Num() && !bCrowded; ++Index)
			{
				const FVector Other = Out[Index].GetLocation();
				const float Clearance = Spec.Spacing * 0.5f * (Scale + static_cast<float>(Out[Index].GetScale3D().Z));
				bCrowded = FVector2D::DistSquared(FVector2D(X, Y), FVector2D(Other.X, Other.Y)) < Clearance * Clearance;
			}
			if (bCrowded)
			{
				continue;
			}
			const FVector Stretch(Scale * Random.FRandRange(0.85f, 1.15f), Scale * Random.FRandRange(0.85f, 1.15f), Scale);
			Out.Add(FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector(X, Y, Layout.FloorZ - GroundDrop), Stretch));
		}
	}

	TArray<FTransform>& Group(TArray<FShieldbreakArtGroup>& Groups, const TCHAR* Piece, const FLinearColor& TeamTint = FLinearColor::Transparent)
	{
		const FName Name(Piece);
		for (FShieldbreakArtGroup& Existing : Groups)
		{
			if (Existing.Piece == Name && Existing.TeamTint == TeamTint)
			{
				return Existing.Instances;
			}
		}
		FShieldbreakArtGroup& Added = Groups.AddDefaulted_GetRef();
		Added.Piece = Name;
		Added.TeamTint = TeamTint;
		return Added.Instances;
	}

	/** Hull ribs / buttresses on the outside of the chosen walls, scaled so they rise RiseAboveWall over the wall top.
	 *  The kit rib is 1700 cm tall, authored against a 1000 cm wall. */
	void Ribs(TArray<FShieldbreakArtGroup>& Groups, const FShieldbreakMapLayout& Layout, bool bLongSidesOnly, float Spacing,
		float RiseAboveWall, float Thickness)
	{
		const float WallTop = ShieldbreakMapArt::PerimeterWallHeight(Layout) + GroundDrop;
		const float ZScale = (WallTop + RiseAboveWall) / 1700.f;
		for (const FSide& Side : Sides(Layout))
		{
			if (bLongSidesOnly && !IsLongSide(Side))
			{
				continue;
			}
			for (const float Along : Stations(Side, Spacing, 700.f))
			{
				Group(Groups, TEXT("Rib")).Add(FacingArena(Layout, Side, 0.f, Along, FVector(1.f, Thickness, ZScale)));
			}
		}
	}

	/** Inside-the-map dressing for built structures of the given styles: a carved relief panel stretched over each
	 *  wall face (6 cm proud, so it never changes what you can stand on or shoot past), and a crown on every
	 *  unwalkably tall block. Faces looking out of the map are skipped, as BuildTrim does. */
	void DressStructures(TArray<FShieldbreakArtGroup>& Groups, const FShieldbreakMapLayout& Layout, std::initializer_list<const TCHAR*> Styles,
		float CrownAbove, bool bFaces = true)
	{
		constexpr float Margin = 40.f, MinWidth = 220.f, MinHeight = 250.f;
		for (const FShieldbreakMapBlock& Block : Layout.Blocks)
		{
			bool bStyled = false;
			for (const TCHAR* Style : Styles)
			{
				bStyled = bStyled || Block.Style == FName(Style);
			}
			const FVector Size = Block.Max - Block.Min;
			if (!bStyled)
			{
				continue;
			}
			const FVector Center = (Block.Min + Block.Max) * 0.5;
			struct FFace { FVector2D Normal; double Offset; double Width; };
			const FFace Faces[4] = { { FVector2D(1.0, 0.0), Size.X * 0.5, Size.Y }, { FVector2D(-1.0, 0.0), Size.X * 0.5, Size.Y },
				{ FVector2D(0.0, 1.0), Size.Y * 0.5, Size.X }, { FVector2D(0.0, -1.0), Size.Y * 0.5, Size.X } };
			for (const FFace& Face : Faces)
			{
				if (Size.Z < MinHeight || !bFaces)
				{
					break;
				}
				const FVector2D At = FVector2D(Center.X, Center.Y) + Face.Normal * Face.Offset;
				if (Face.Width < MinWidth || FMath::Abs(At.X) > Layout.HalfExtent.X + 1.0 || FMath::Abs(At.Y) > Layout.HalfExtent.Y + 1.0)
				{
					continue;
				}
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Face.Normal.Y, Face.Normal.X));
				Group(Groups, TEXT("Relief")).Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(At.X, At.Y, Block.Min.Z + Margin),
					FVector(1.0, (Face.Width - 2.0 * Margin) / 100.0, (Size.Z - 2.0 * Margin) / 100.0)));
			}
			// Thin slabs (floors, walkways) get an edge band instead of a relief.
			if (bFaces && Size.Z <= 60.0 && Block.Max.Z - Layout.FloorZ >= 400.0)
			{
				for (const FFace& Face : Faces)
				{
					const FVector2D At = FVector2D(Center.X, Center.Y) + Face.Normal * Face.Offset;
					if (Face.Width < 300.0 || FMath::Abs(At.X) > Layout.HalfExtent.X + 1.0 || FMath::Abs(At.Y) > Layout.HalfExtent.Y + 1.0)
					{
						continue;
					}
					const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Face.Normal.Y, Face.Normal.X));
					Group(Groups, TEXT("Fascia")).Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(At.X, At.Y, Block.Min.Z - 10.0),
						FVector(1.0, (Face.Width - 20.0) / 100.0, (Size.Z + 20.0) / 100.0)));
				}
			}
			const bool bOnEdge = FMath::Abs(Center.X) + Size.X * 0.5 > Layout.HalfExtent.X || FMath::Abs(Center.Y) + Size.Y * 0.5 > Layout.HalfExtent.Y;
			if (Block.Max.Z - Layout.FloorZ >= CrownAbove && !bOnEdge)
			{
				// The crown is authored on a 100 cm footprint and 206 cm tall; keep it under about 6 m on wide blocks.
				const double Height = FMath::Min(FMath::Min(Size.X, Size.Y) / 100.0, 3.0);
				Group(Groups, TEXT("Crown")).Add(FTransform(FRotator::ZeroRotator, FVector(Center.X, Center.Y, Block.Max.Z),
					FVector(Size.X / 100.0, Size.Y / 100.0, Height)));
			}
		}
	}

	/** The kit pieces that clad one block style: the bottom band, the two main panels (alternating up the face), the
	 *  top band, the band for thin slab edges and the two plates (checkered) over the top; no plates leaves the top
	 *  bare. Bands are 100 cm tall, panels 200 cm, plates 400 x 400 cm (Art/blender/build_scifi_kit.py). */
	struct FCladRecipe
	{
		const TCHAR* Base;
		const TCHAR* Main[2];
		const TCHAR* Top;
		const TCHAR* Edge;
		const TCHAR* Floor[2];
		/** Tint for the plates instead of the map's (alpha 0: the map's CladTint). */
		FLinearColor FloorTint = FLinearColor::Transparent;
	};

	const FCladRecipe& CladRecipe(FName Style)
	{
		struct FEntry { const TCHAR* Style; FCladRecipe Recipe; };
		static const FEntry Entries[] = {
			{ TEXT("stone"), { TEXT("BandPlates"), { TEXT("WallMetal"), TEXT("WallBand") }, TEXT("BandAccent"), TEXT("BandAccent"), { TEXT("FloorMetal"), TEXT("FloorMetal") } } },
			{ TEXT("floor"), { TEXT("BandDark"), { TEXT("WallDark"), TEXT("WallDark") }, TEXT("BandDark"), TEXT("BandDark"), { TEXT("FloorMetal2"), TEXT("FloorMetal2") } } },
			{ TEXT("wall"), { TEXT("BandTriangles"), { TEXT("WallDark"), TEXT("WallMetal") }, TEXT("BandAccent"), TEXT("BandAccent"), { nullptr, nullptr } } },
			// The plateaus are paved in sandstone-tinted plates, so the courtyards read as built, not as greybox.
			{ TEXT("sand"), { TEXT("BandTriangles"), { TEXT("WallMetal"), TEXT("WallMetal") }, TEXT("BandPlates"), TEXT("BandPlates"),
				{ TEXT("FloorSquares"), TEXT("FloorSquares") }, FLinearColor(0.64f, 0.55f, 0.42f) } },
			{ TEXT("red"), { TEXT("BandDark"), { TEXT("WallPlate"), TEXT("WallPlate") }, TEXT("BandAccent"), TEXT("BandAccent"), { TEXT("FloorCenter"), TEXT("FloorCenter") } } },
			{ TEXT("blue"), { TEXT("BandDark"), { TEXT("WallPlate"), TEXT("WallPlate") }, TEXT("BandAccent"), TEXT("BandAccent"), { TEXT("FloorCenter"), TEXT("FloorCenter") } } },
			{ TEXT("tower"), { TEXT("BandDark"), { TEXT("WallDark"), TEXT("WallMetal") }, TEXT("BandAccent"), TEXT("BandAccent"), { TEXT("FloorX"), TEXT("FloorX") } } },
			{ TEXT("dark"), { TEXT("BandDark"), { TEXT("WallDark"), TEXT("WallDark") }, TEXT("BandDark"), TEXT("BandDark"), { TEXT("FloorDark"), TEXT("FloorDark") } } },
			{ TEXT("bridge"), { TEXT("BandPlates"), { TEXT("WallMetal"), TEXT("WallMetal") }, TEXT("BandAccent"), TEXT("BandAccent"), { TEXT("FloorPlates"), TEXT("FloorPlates") } } },
			{ TEXT("rock"), { TEXT("BandDark"), { TEXT("WallDark"), TEXT("WallDark") }, TEXT("BandTriangles"), TEXT("BandDark"), { TEXT("FloorDark"), TEXT("FloorDark") } } },
			{ TEXT("ramp"), { TEXT("BandDark"), { TEXT("WallDark"), TEXT("WallDark") }, TEXT("BandDark"), TEXT("BandDark"), { TEXT("FloorMetal2"), TEXT("FloorMetal2") } } },
		};
		for (const FEntry& Entry : Entries)
		{
			if (Style == FName(Entry.Style))
			{
				return Entry.Recipe;
			}
		}
		return Entries[0].Recipe; // anything else clads like stone
	}

	double CladRowHeight(const TCHAR* Piece) { return FCString::Strncmp(Piece, TEXT("Band"), 4) == 0 ? 100.0 : 200.0; }

	/** Panels over one wall face (At: the face's centre on the ground plan, Normal: out of the face), in rows from
	 *  Bottom up, stretched a little so whole rows and whole tiles fill the face exactly. */
	void CladFace(TArray<FShieldbreakArtGroup>& Groups, const FCladRecipe& Recipe, const FVector2D& At, const FVector2D& Normal,
		double Width, double Bottom, double Height)
	{
		constexpr double Proud = 2.0, TileWidth = 400.0;
		TArray<const TCHAR*, TInlineAllocator<12>> Rows;
		if (Height <= 70.0)
		{
			Rows.Add(Recipe.Edge);
		}
		else if (Height <= 150.0)
		{
			Rows.Add(Recipe.Top);
		}
		else if (Height <= 250.0)
		{
			Rows.Add(Recipe.Base);
			Rows.Add(Recipe.Top);
		}
		else
		{
			Rows.Add(Recipe.Base);
			const int32 Panels = FMath::Max(1, FMath::RoundToInt((Height - 200.0) / 200.0));
			for (int32 Index = 0; Index < Panels; ++Index)
			{
				Rows.Add(Recipe.Main[Index % 2]);
			}
			Rows.Add(Recipe.Top);
		}
		double Nominal = 0.0;
		for (const TCHAR* Row : Rows)
		{
			Nominal += CladRowHeight(Row);
		}
		const double Stretch = Height / Nominal;
		const int32 Tiles = FMath::Max(1, FMath::RoundToInt(Width / TileWidth));
		const double Tile = Width / Tiles;
		const FVector2D Tangent(-Normal.Y, Normal.X);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Normal.Y, Normal.X));
		double Z = Bottom;
		for (const TCHAR* Row : Rows)
		{
			const double RowHeight = CladRowHeight(Row) * Stretch;
			TArray<FTransform>& Instances = Group(Groups, Row);
			for (int32 Index = 0; Index < Tiles; ++Index)
			{
				const FVector2D P = At + Normal * Proud + Tangent * (-Width * 0.5 + Tile * (Index + 0.5));
				Instances.Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, Z), FVector(1.0, Tile / TileWidth, Stretch)));
			}
			Z += RowHeight;
		}
	}

	/** Plates over a block's top, Lift above it, run along its long side and checkered between the recipe's two. */
	void CladTop(TArray<FShieldbreakArtGroup>& Groups, const FCladRecipe& Recipe, const FVector& Min, const FVector& Max,
		const TCHAR* Piece = nullptr, double Z = 0.0)
	{
		// The plates are two-sided (M_SB_Trim), so the same piece also lines an underside.
		constexpr double Lift = 1.0, TileSize = 400.0;
		const bool bUnder = Piece != nullptr;
		const FVector Size = Max - Min;
		const bool bAlongX = Size.X >= Size.Y;
		const double Long = bAlongX ? Size.X : Size.Y, Short = bAlongX ? Size.Y : Size.X;
		const int32 LongTiles = FMath::Max(1, FMath::RoundToInt(Long / TileSize)), ShortTiles = FMath::Max(1, FMath::RoundToInt(Short / TileSize));
		const double TileLong = Long / LongTiles, TileShort = Short / ShortTiles;
		const FVector Center = (Min + Max) * 0.5;
		for (int32 I = 0; I < LongTiles; ++I)
		{
			for (int32 J = 0; J < ShortTiles; ++J)
			{
				const double U = -Long * 0.5 + TileLong * (I + 0.5), V = -Short * 0.5 + TileShort * (J + 0.5);
				const FVector2D Offset = bAlongX ? FVector2D(U, V) : FVector2D(V, U);
				Group(Groups, bUnder ? Piece : Recipe.Floor[(I + J) % 2], bUnder ? FLinearColor::Transparent : Recipe.FloorTint).Add(
					FTransform(FRotator(0.f, bAlongX ? 0.f : 90.f, 0.f),
					FVector(Center.X + Offset.X, Center.Y + Offset.Y, bUnder ? Z - Lift : Max.Z + Lift), FVector(TileLong / TileSize, TileShort / TileSize, 1.0)));
			}
		}
	}

	/** A pylon at each of the given corners (signs of X and Y), Distance outside the walls diagonally. */
	void CornerPylons(TArray<FShieldbreakArtGroup>& Groups, FRandomStream& Random, const FShieldbreakMapLayout& Layout,
		std::initializer_list<FVector2D> Corners, float Distance, float ScaleMin, float ScaleMax)
	{
		for (const FVector2D& Corner : Corners)
		{
			const FVector At(Corner.X * (Layout.HalfExtent.X + WallThickness + Distance), Corner.Y * (Layout.HalfExtent.Y + WallThickness + Distance),
				Layout.FloorZ - GroundDrop);
			Group(Groups, TEXT("Pylon")).Add(FTransform(FRotator(0.f, Random.FRandRange(0.f, 90.f), 0.f), At, FVector(Random.FRandRange(ScaleMin, ScaleMax))));
		}
	}
}

const FShieldbreakMapLook& ShieldbreakMapArt::GetLook(FName MapId)
{
	static const TMap<FName, FShieldbreakMapLook> Looks = []
	{
		TMap<FName, FShieldbreakMapLook> Out;
		// Keel: an orbital drydock. Cool steel, a concrete apron, grey-blue haze.
		FShieldbreakMapLook& Keel = Out.Add(FName(TEXT("Keel")));
		Keel.GroundStyle = TEXT("stone");
		Keel.Ground = FLinearColor(0.21f, 0.22f, 0.23f);
		Keel.Rock = FLinearColor(0.27f, 0.27f, 0.28f);
		Keel.Metal = FLinearColor(0.34f, 0.38f, 0.42f);
		Keel.DarkMetal = FLinearColor(0.11f, 0.13f, 0.16f);
		Keel.Haze = FLinearColor(0.34f, 0.42f, 0.54f);
		// Bastion: a cliff fortress in cold, high country. Pale rock, cold blue-white haze.
		FShieldbreakMapLook& Bastion = Out.Add(FName(TEXT("Bastion")));
		Bastion.GroundStyle = TEXT("stone");
		Bastion.Ground = FLinearColor(0.44f, 0.46f, 0.50f);
		Bastion.Rock = FLinearColor(0.36f, 0.37f, 0.41f);
		Bastion.Metal = FLinearColor(0.30f, 0.33f, 0.38f);
		Bastion.DarkMetal = FLinearColor(0.13f, 0.14f, 0.17f);
		Bastion.Haze = FLinearColor(0.55f, 0.62f, 0.72f);
		// Sanctum: sun temple ruins in a desert basin. Sand, carved ivory stone, warm amber haze.
		FShieldbreakMapLook& Sanctum = Out.Add(FName(TEXT("Sanctum")));
		Sanctum.GroundStyle = TEXT("sand");
		Sanctum.Ground = FLinearColor(0.50f, 0.40f, 0.26f);
		Sanctum.Rock = FLinearColor(0.46f, 0.36f, 0.25f);
		Sanctum.Carved = FLinearColor(0.62f, 0.56f, 0.46f);
		Sanctum.Haze = FLinearColor(0.62f, 0.52f, 0.40f);
		Sanctum.bClad = true;
		Sanctum.CladTint = FLinearColor(0.56f, 0.53f, 0.48f);
		Sanctum.CladAccent = FLinearColor(1.0f, 0.52f, 0.12f);
		// Creekbed: a forest valley. Green banks, pines, mossy rock, soft green-grey haze.
		FShieldbreakMapLook& Creekbed = Out.Add(FName(TEXT("Creekbed")));
		Creekbed.Ground = FLinearColor(0.17f, 0.27f, 0.11f);
		Creekbed.Rock = FLinearColor(0.29f, 0.29f, 0.27f);
		Creekbed.Foliage = FLinearColor(0.08f, 0.19f, 0.07f);
		Creekbed.Haze = FLinearColor(0.38f, 0.47f, 0.50f);
		// Spire: a storm-harvesting exchange above a dark caldera. Basalt, blue-steel towers, stormy haze.
		FShieldbreakMapLook& Spire = Out.Add(FName(TEXT("Spire")));
		Spire.GroundStyle = TEXT("stone");
		Spire.Ground = FLinearColor(0.15f, 0.15f, 0.17f);
		Spire.Rock = FLinearColor(0.21f, 0.20f, 0.22f);
		Spire.Metal = FLinearColor(0.32f, 0.40f, 0.43f);
		Spire.DarkMetal = FLinearColor(0.12f, 0.15f, 0.18f);
		Spire.Haze = FLinearColor(0.30f, 0.37f, 0.55f);
		return Out;
	}();
	static const FShieldbreakMapLook Default;
	const FShieldbreakMapLook* Look = Looks.Find(MapId);
	return Look ? *Look : Default;
}

TArray<FShieldbreakArtGroup> ShieldbreakMapArt::Cladding(FName MapId, const FShieldbreakMapLayout& Layout)
{
	using namespace ShieldbreakMapArtPrivate;
	TArray<FShieldbreakArtGroup> Groups;
	if (!GetLook(MapId).bClad)
	{
		return Groups;
	}
	const FVector2D Half = Layout.HalfExtent;
	auto Inside = [&Half](const FVector2D& P) { return FMath::Abs(P.X) < Half.X - 1.0 && FMath::Abs(P.Y) < Half.Y - 1.0; };
	for (const FShieldbreakMapBlock& Block : Layout.Blocks)
	{
		const FCladRecipe& Recipe = CladRecipe(Block.Style);
		const FVector Size = Block.Max - Block.Min;
		const FVector2D Center((Block.Min.X + Block.Max.X) * 0.5, (Block.Min.Y + Block.Max.Y) * 0.5);
		const double Bottom = FMath::Max(Block.Min.Z, static_cast<double>(Layout.FloorZ));
		const double Height = Block.Max.Z - Bottom;
		struct FFace { FVector2D Normal; double Offset; double Width; };
		const FFace Faces[4] = { { FVector2D(1.0, 0.0), Size.X * 0.5, Size.Y }, { FVector2D(-1.0, 0.0), Size.X * 0.5, Size.Y },
			{ FVector2D(0.0, 1.0), Size.Y * 0.5, Size.X }, { FVector2D(0.0, -1.0), Size.Y * 0.5, Size.X } };
		for (const FFace& Face : Faces)
		{
			const FVector2D At = Center + Face.Normal * Face.Offset;
			// Faces inside the map, and the perimeter walls' inner faces (on the edge, looking in). Not the backs of the
			// walls, nor block faces pressed against them.
			const bool bOnEdge = FMath::Abs(At.X) <= Half.X + 1.0 && FMath::Abs(At.Y) <= Half.Y + 1.0 && !Inside(At);
			const bool bLooksIn = FVector2D::DotProduct(Face.Normal, At) < 0.0;
			if (Height < 25.0 || Face.Width < 30.0 || !(Inside(At) || (bOnEdge && bLooksIn)))
			{
				continue;
			}
			CladFace(Groups, Recipe, At, Face.Normal, Face.Width, Bottom, Height);
		}
		if (Recipe.Floor[0] && Inside(Center))
		{
			CladTop(Groups, Recipe, Block.Min, Block.Max);
		}
		// Raised slabs (roofs, walkways) are seen from below too.
		if (Inside(Center) && Block.Min.Z - Layout.FloorZ >= 150.0 && Size.Z <= 100.0)
		{
			CladTop(Groups, Recipe, Block.Min, Block.Max, TEXT("FloorDark"), Block.Min.Z);
		}
	}
	for (const FShieldbreakMapRamp& Ramp : Layout.Ramps)
	{
		const FCladRecipe& Recipe = CladRecipe(Ramp.Style);
		if (!Recipe.Floor[0])
		{
			continue;
		}
		// The ramp's top surface runs along the centreline from Start to End (AShieldbreakArena::AddRamp).
		constexpr double Lift = 1.0, TileSize = 400.0;
		const FVector Delta = Ramp.End - Ramp.Start;
		const double Length = Delta.Size();
		const FRotator Rotation = Delta.Rotation();
		const FRotationMatrix Axes(Rotation);
		const FVector Along = Axes.GetUnitAxis(EAxis::X), Across = Axes.GetUnitAxis(EAxis::Y), Up = Axes.GetUnitAxis(EAxis::Z);
		const int32 LongTiles = FMath::Max(1, FMath::RoundToInt(Length / TileSize)), ShortTiles = FMath::Max(1, FMath::RoundToInt(Ramp.Width / TileSize));
		const double TileLong = Length / LongTiles, TileShort = Ramp.Width / ShortTiles;
		for (int32 I = 0; I < LongTiles; ++I)
		{
			for (int32 J = 0; J < ShortTiles; ++J)
			{
				const FVector P = Ramp.Start + Along * (TileLong * (I + 0.5)) + Across * (-Ramp.Width * 0.5 + TileShort * (J + 0.5));
				const FVector Scale(TileLong / TileSize, TileShort / TileSize, 1.0);
				Group(Groups, Recipe.Floor[(I + J) % 2], Recipe.FloorTint).Add(FTransform(Rotation, P + Up * Lift, Scale));
				Group(Groups, TEXT("FloorDark")).Add(FTransform(Rotation, P - Up * (Ramp.Thickness + Lift), Scale));
			}
		}
		// Both edges: one band row, standing on the slab's underside and tilted with it.
		for (const double Side : { -1.0, 1.0 })
		{
			const FVector Normal = Across * Side;
			const FRotator EdgeRotation = FRotationMatrix::MakeFromXZ(Normal, Up).Rotator();
			for (int32 I = 0; I < LongTiles; ++I)
			{
				const FVector P = Ramp.Start + Along * (TileLong * (I + 0.5)) + Normal * (Ramp.Width * 0.5 + 2.0) - Up * Ramp.Thickness;
				Group(Groups, Recipe.Edge).Add(FTransform(EdgeRotation, P, FVector(1.0, TileLong / TileSize, Ramp.Thickness / 100.0)));
			}
		}
	}
	return Groups;
}

float ShieldbreakMapArt::PerimeterWallHeight(const FShieldbreakMapLayout& Layout)
{
	float Top = Layout.FloorZ;
	for (const FShieldbreakMapBlock& Block : Layout.Blocks)
	{
		const bool bOnEdge = Block.Min.X <= -Layout.HalfExtent.X - 50.0 || Block.Max.X >= Layout.HalfExtent.X + 50.0
			|| Block.Min.Y <= -Layout.HalfExtent.Y - 50.0 || Block.Max.Y >= Layout.HalfExtent.Y + 50.0;
		if (bOnEdge && Block.Max.Z > Layout.FloorZ + 1.0)
		{
			Top = FMath::Max(Top, static_cast<float>(Block.Max.Z));
		}
	}
	return Top - Layout.FloorZ;
}

TArray<FShieldbreakArtGroup> ShieldbreakMapArt::Plan(FName MapId, const FShieldbreakMapLayout& Layout)
{
	using namespace ShieldbreakMapArtPrivate;
	TArray<FShieldbreakArtGroup> Groups;
	FRandomStream Random(static_cast<int32>(GetTypeHash(MapId.ToString() + TEXT("/art"))));
	const FString Id = MapId.ToString();

	auto Rocks = [&](const TCHAR* Piece, const FScatter& Spec) { Scatter(Group(Groups, Piece), Random, Layout, Spec); };

	if (Id == TEXT("Keel"))
	{
		// The arena is the deck of a ship in dock: hull ribs along both long sides, cranes over the ends,
		// beacon towers at the corners of the yard.
		Ribs(Groups, Layout, true, 1200.f, 700.f, 1.f);
		for (const FSide& Side : Sides(Layout))
		{
			if (!IsLongSide(Side))
			{
				for (const float Along : { -0.5f, 0.5f })
				{
					Group(Groups, TEXT("Gantry")).Add(FacingArena(Layout, Side, 1800.f, Along * Side.Along, FVector(1.f)));
				}
			}
		}
		CornerPylons(Groups, Random, Layout, { FVector2D(1.0, 1.0), FVector2D(1.0, -1.0), FVector2D(-1.0, 1.0), FVector2D(-1.0, -1.0) }, 3600.f, 0.85f, 1.05f);
		Rocks(TEXT("RockShelf"), { 12, 2500.f, 9000.f, 1.2f, 2.2f, 2200.f });
		Rocks(TEXT("RockBoulder"), { 24, 1500.f, 9000.f, 0.8f, 1.6f, 1100.f });
	}
	else if (Id == TEXT("Bastion"))
	{
		// A fortress on a cliff top: heavy buttresses on every wall, crags around it, two beacon pylons.
		Ribs(Groups, Layout, false, 1600.f, 450.f, 1.5f);
		CornerPylons(Groups, Random, Layout, { FVector2D(1.0, 1.0), FVector2D(-1.0, -1.0) }, 3200.f, 0.75f, 0.9f);
		Rocks(TEXT("RockTall"), { 34, 900.f, 9000.f, 1.2f, 2.6f, 900.f });
		Rocks(TEXT("RockShelf"), { 18, 1200.f, 9000.f, 1.4f, 2.6f, 2000.f });
		Rocks(TEXT("RockBoulder"), { 24, 600.f, 7000.f, 0.8f, 1.8f, 1000.f });
	}
	else if (Id == TEXT("Sanctum"))
	{
		// A sun temple in a desert basin: monoliths line the long walls, broken arches stand behind each temple end.
		for (const FSide& Side : Sides(Layout))
		{
			if (IsLongSide(Side))
			{
				int32 Index = 0;
				for (const float Along : Stations(Side, 2000.f, 900.f))
				{
					const float Scale = (Index++ % 2) ? 0.9f : Random.FRandRange(1.1f, 1.3f);
					Group(Groups, TEXT("Monolith")).Add(FacingArena(Layout, Side, 900.f, Along, FVector(Scale)));
				}
			}
			else
			{
				for (const float Along : { -0.55f, 0.f, 0.55f })
				{
					Group(Groups, TEXT("Arch")).Add(FacingArena(Layout, Side, Along == 0.f ? 2600.f : 1800.f, Along * Side.Along,
						FVector(Along == 0.f ? 1.5f : 1.2f)));
				}
			}
		}
		Rocks(TEXT("RockShelf"), { 18, 2000.f, 10000.f, 1.2f, 2.4f, 2000.f });
		Rocks(TEXT("RockBoulder"), { 26, 1200.f, 9000.f, 0.7f, 1.5f, 1000.f });
		Rocks(TEXT("RockTall"), { 8, 4000.f, 10000.f, 1.4f, 2.2f, 1200.f });
		// Inside: crowns on the spires, colossi and obelisks, and carved reliefs on the temples, shrines, annexes and
		// ruins unless the sci-fi kit clads them (Cladding).
		DressStructures(Groups, Layout, { TEXT("red"), TEXT("blue"), TEXT("stone"), TEXT("tower"), TEXT("bridge") }, 1050.f,
			!GetLook(MapId).bClad);
		// A glowing diamond inlaid round the rockets plinth, an amber prism hung high above it, and each team's
		// cracked-shield sigil floating over its temple atrium.
		const float Floor = Layout.FloorZ;
		Group(Groups, TEXT("Inlay")).Add(FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, Floor), FVector(18.0, 18.0, 1.0)));
		Group(Groups, TEXT("Beacon")).Add(FTransform(FRotator(0.f, 45.f, 0.f), FVector(0.0, 0.0, Floor + 1700.0), FVector(1.6)));
		for (const float Side : { -1.f, 1.f })
		{
			const FLinearColor Tint = Side < 0.f ? FLinearColor(1.f, 0.22f, 0.12f) : FLinearColor(0.2f, 0.45f, 1.f);
			Group(Groups, TEXT("Sigil"), Tint).Add(FTransform(FRotator::ZeroRotator, FVector(Side * 6000.0, 0.0, Floor + 1250.0), FVector(1.5)));
		}
	}
	else if (Id == TEXT("Creekbed"))
	{
		// A forest valley: pines crowd every wall, boulders and a few crags further out.
		Rocks(TEXT("Pine"), { 200, 250.f, 5500.f, 0.75f, 1.45f, 520.f });
		Rocks(TEXT("RockBoulder"), { 36, 200.f, 7000.f, 0.6f, 1.5f, 900.f });
		Rocks(TEXT("RockTall"), { 12, 3500.f, 9000.f, 1.5f, 2.5f, 1400.f });
	}
	else if (Id == TEXT("Spire"))
	{
		// The storm exchange: a line of collector pylons along each long side and one past each base, over a basalt
		// caldera floor (the collectors, service panels and lift collars are BuildSpireArt's).
		for (const FSide& Side : Sides(Layout))
		{
			if (IsLongSide(Side))
			{
				for (const float Along : Stations(Side, 4200.f, 1500.f))
				{
					Group(Groups, TEXT("Pylon")).Add(FacingArena(Layout, Side, 3800.f, Along, FVector(Random.FRandRange(0.9f, 1.25f))));
				}
			}
			else
			{
				Group(Groups, TEXT("Pylon")).Add(FacingArena(Layout, Side, 2400.f, 0.f, FVector(1.4f)));
			}
		}
		Rocks(TEXT("RockShelf"), { 22, 1500.f, 10000.f, 1.5f, 3.f, 2400.f });
		Rocks(TEXT("RockTall"), { 20, 1200.f, 10000.f, 1.2f, 2.4f, 1200.f });
	}
	else
	{
		return Groups; // Crucible, the test arena, stays bare.
	}

	// Every launch map gets one far landmark past a short wall, so each base has a skyline to orient by
	// (red looks toward +X, blue toward -X; the side is picked per map).
	for (const FSide& Side : Sides(Layout))
	{
		if (!IsLongSide(Side) && (Side.Normal.X > 0.0) == (Random.FRand() < 0.5f))
		{
			Group(Groups, TEXT("Monument")).Add(FacingArena(Layout, Side, 3500.f, Random.FRandRange(-0.3f, 0.3f) * Side.Along, FVector(1.f)));
			break;
		}
	}
	return Groups;
}
