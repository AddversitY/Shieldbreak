#include "Game/ShieldbreakMapArt.h"

#include "Game/ShieldbreakMaps.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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

	/** Spire's structure reads in long vertical bays, with quiet decks. The generic recipe deliberately
	 *  alternates service bands for Keel; using it here turned every pier into a stack of glowing tiles. */
	const FCladRecipe& SpireCladRecipe(FName Style)
	{
		static const FCladRecipe Structure = { TEXT("BandDark"), { TEXT("WallMetal"), TEXT("WallMetal") },
			TEXT("BandDark"), TEXT("BandDark"), { TEXT("FloorMetal2"), TEXT("FloorMetal2") } };
		static const FCladRecipe Deck = { TEXT("BandDark"), { TEXT("WallDark"), TEXT("WallDark") },
			TEXT("BandDark"), TEXT("BandDark"), { TEXT("FloorMetal2"), TEXT("FloorMetal2") } };
		return Style == FName(TEXT("floor")) || Style == FName(TEXT("dark")) || Style == FName(TEXT("bridge"))
			|| Style == FName(TEXT("ramp")) ? Deck : Structure;
	}

	double CladRowHeight(const TCHAR* Piece) { return FCString::Strncmp(Piece, TEXT("Band"), 4) == 0 ? 100.0 : 200.0; }

	/** Panels over one wall face (At: the face's centre on the ground plan, Normal: out of the face), in rows from
	 *  Bottom up, stretched a little so whole rows and whole tiles fill the face exactly. */
	void CladFace(TArray<FShieldbreakArtGroup>& Groups, const FCladRecipe& Recipe, const FVector2D& At, const FVector2D& Normal,
		double Width, double Bottom, double Height, bool bStructuralBays = false)
	{
		constexpr double Proud = 2.0;
		const double TileWidth = bStructuralBays ? 800.0 : 400.0;
		TArray<const TCHAR*, TInlineAllocator<12>> Rows;
		if (bStructuralBays && Height > 150.0)
		{
			// Whole-height panels on the freestanding piers; large bays on broad walls. Keep the bevel depth
			// in X unchanged, so this surface change does not encroach farther into routes or conceal players.
			const int32 Panels = Width < 650.0 ? 1 : FMath::Max(1, FMath::RoundToInt(Height / 650.0));
			for (int32 Index = 0; Index < Panels; ++Index)
			{
				Rows.Add(Recipe.Main[0]);
			}
		}
		else if (Height <= 70.0)
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
				Instances.Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, Z), FVector(1.0, Tile / 400.0, Stretch)));
			}
			Z += RowHeight;
		}
	}

	/** Plates over a block's top, Lift above it, run along its long side and checkered between the recipe's two. */
	void CladTop(TArray<FShieldbreakArtGroup>& Groups, const FCladRecipe& Recipe, const FVector& Min, const FVector& Max,
		const TCHAR* Piece = nullptr, double Z = 0.0, double TileSize = 400.0)
	{
		// The plates are two-sided (M_SB_Trim), so the same piece also lines an underside.
		constexpr double Lift = 1.0;
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
						FVector(Center.X + Offset.X, Center.Y + Offset.Y, bUnder ? Z - Lift : Max.Z + Lift), FVector(TileLong / 400.0, TileShort / 400.0, 1.0)));
			}
		}
	}

	/** Sandstone skin over the visible faces of the layout's rock and sand blocks (FShieldbreakMapLook::bRockSkin), and
	 *  dressed-stone (ashlar) StoneFace tiles over its stone blocks (bStoneSkin):
	 *  RockFace tiles (400 x 400, bulging up to 36 cm out of the face) in a grid stretched to fill each face, a
	 *  RockCorner column down each exposed vertical edge, and a RockCap dome on the tops of the small rocks. Faces a ramp runs into are left bare, so no rock pokes through the
	 *  ramp's surface. Each block's tiles take the block's own colour. */
	void RockSkin(TArray<FShieldbreakArtGroup>& Groups, const FShieldbreakMapLayout& Layout, bool bRock, bool bStone)
	{
		const FVector2D Half = Layout.HalfExtent;
		auto Inside = [&Half](const FVector2D& P) { return FMath::Abs(P.X) < Half.X - 1.0 && FMath::Abs(P.Y) < Half.Y - 1.0; };
		auto TileGroup = [&Groups](const TCHAR* Piece, const FLinearColor& Tint) -> TArray<FTransform>&
		{
			const FName Name(Piece);
			for (FShieldbreakArtGroup& Existing : Groups)
			{
				if (Existing.Piece == Name && Existing.SurfaceTint == Tint)
				{
					return Existing.Instances;
				}
			}
			FShieldbreakArtGroup& Added = Groups.AddDefaulted_GetRef();
			Added.Piece = Name;
			Added.SurfaceTint = Tint;
			return Added.Instances;
		};
		static const TCHAR* const RockFaces[3] = { TEXT("RockFaceA"), TEXT("RockFaceB"), TEXT("RockFaceC") };
		static const TCHAR* const StoneFaces[3] = { TEXT("StoneFaceA"), TEXT("StoneFaceB"), TEXT("StoneFaceA") };
		constexpr double TileSize = 400.0;
		int32 Seed = 0;
		for (const FShieldbreakMapBlock& Block : Layout.Blocks)
		{
			++Seed;
			const bool bIsRock = bRock && (Block.Style == FName(TEXT("rock")) || Block.Style == FName(TEXT("sand")));
			const bool bIsStone = bStone && Block.Style == FName(TEXT("stone"));
			if (!bIsRock && !bIsStone)
			{
				continue;
			}
			const TCHAR* const* Faces = bIsRock ? RockFaces : StoneFaces;
			const FLinearColor Tint(Block.Color.R, Block.Color.G, Block.Color.B, 1.f);
			const FVector Size = Block.Max - Block.Min;
			const FVector2D Center((Block.Min.X + Block.Max.X) * 0.5, (Block.Min.Y + Block.Max.Y) * 0.5);
			const double Bottom = FMath::Max(Block.Min.Z, static_cast<double>(Layout.FloorZ));
			const double Height = Block.Max.Z - Bottom;
			struct FFace { FVector2D Normal; double Offset; double Width; };
			const FFace BlockFaces[4] = { { FVector2D(1.0, 0.0), Size.X * 0.5, Size.Y }, { FVector2D(-1.0, 0.0), Size.X * 0.5, Size.Y },
				{ FVector2D(0.0, 1.0), Size.Y * 0.5, Size.X }, { FVector2D(0.0, -1.0), Size.Y * 0.5, Size.X } };
			for (const FFace& Face : BlockFaces)
			{
				const FVector2D At = Center + Face.Normal * Face.Offset;
				if (Height < 60.0 || Face.Width < (bIsStone ? 100.0 : 60.0) || !Inside(At))
				{
					continue;
				}
				const FVector2D Tangent(-Face.Normal.Y, Face.Normal.X);
				bool bRampHere = false;
				for (const FShieldbreakMapRamp& Ramp : Layout.Ramps)
				{
					for (const FVector& End : { Ramp.Start, Ramp.End })
					{
						const FVector2D Rel = FVector2D(End.X, End.Y) - At;
						if (FMath::Abs(FVector2D::DotProduct(Rel, Face.Normal)) < 60.0
							&& FMath::Abs(FVector2D::DotProduct(Rel, Tangent)) < Face.Width * 0.5 + Ramp.Width * 0.5
							&& End.Z > Bottom - 50.0 && End.Z < Block.Max.Z + 50.0)
						{
							bRampHere = true;
						}
					}
				}
				// A face pressed against another block (a bank against the next bank) is never seen.
				const FVector Probe(At.X + Face.Normal.X * 50.0, At.Y + Face.Normal.Y * 50.0, Bottom + Height * 0.5);
				bool bCovered = false;
				for (const FShieldbreakMapBlock& Other : Layout.Blocks)
				{
					bCovered |= &Other != &Block && FBox(Other.Min, Other.Max).IsInsideOrOn(Probe);
				}
				if (bRampHere || bCovered)
				{
					continue;
				}
				const int32 Across = FMath::Max(1, FMath::RoundToInt(Face.Width / TileSize));
				const int32 Up = FMath::Max(1, FMath::RoundToInt(Height / TileSize));
				const double TileW = Face.Width / Across, TileH = Height / Up;
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Face.Normal.Y, Face.Normal.X));
				for (int32 I = 0; I < Across; ++I)
				{
					for (int32 J = 0; J < Up; ++J)
					{
						const FVector2D P = At + Tangent * (-Face.Width * 0.5 + TileW * (I + 0.5));
						const uint32 Pick = HashCombine(GetTypeHash(Seed), HashCombine(GetTypeHash(I), GetTypeHash(J * 7 + static_cast<int32>(Yaw))));
						TileGroup(Faces[Pick % 3], Tint).Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, Bottom + TileH * J),
							FVector(1.0, TileW / TileSize, TileH / TileSize)));
					}
				}
			}
			// A weathered column down each exposed vertical edge, so the boxes lose their sharp corners (rock only:
			// dressed stone keeps its square arrises).
			if (bIsRock && Height >= 60.0)
			{
				const int32 Up = FMath::Max(1, FMath::RoundToInt(Height / TileSize));
				const double TileH = Height / Up;
				for (const double CX : { Block.Min.X, Block.Max.X })
				{
					for (const double CY : { Block.Min.Y, Block.Max.Y })
					{
						const FVector2D Corner(CX, CY);
						const FVector Probe(CX + (CX > Center.X ? 50.0 : -50.0), CY + (CY > Center.Y ? 50.0 : -50.0), Bottom + Height * 0.5);
						bool bCovered = !Inside(Corner);
						for (const FShieldbreakMapBlock& Other : Layout.Blocks)
						{
							bCovered |= &Other != &Block && FBox(Other.Min, Other.Max).IsInsideOrOn(Probe);
						}
						// Not beside a ramp, where the column would stick up through the ramp's surface.
						for (const FShieldbreakMapRamp& Ramp : Layout.Ramps)
						{
							const FVector2D A(Ramp.Start.X, Ramp.Start.Y), B(Ramp.End.X, Ramp.End.Y);
							const FVector2D Closest = FMath::ClosestPointOnSegment2D(Corner, A, B);
							bCovered |= FVector2D::Distance(Closest, Corner) < Ramp.Width * 0.5 + 60.0;
						}
						for (int32 J = 0; !bCovered && J < Up; ++J)
						{
							TileGroup(TEXT("RockCorner"), Tint).Add(FTransform(FRotator(0.f, ((Seed + J) % 8) * 45.f, 0.f),
								FVector(CX, CY, Bottom + TileH * J), FVector(1.0, 1.0, TileH / TileSize)));
						}
					}
				}
			}
			// Small rocks (cover, pillars, hoodoos) get a domed top; big walkable tops stay flat underfoot.
			if (Block.Style == FName(TEXT("rock")) && Size.X <= 700.0 && Size.Y <= 700.0 && Inside(Center))
			{
				TileGroup(TEXT("RockCap"), Tint).Add(FTransform(FRotator(0.f, (Seed % 4) * 90.f, 0.f), FVector(Center.X, Center.Y, Block.Max.Z),
					FVector(Size.X / TileSize, Size.Y / TileSize, 1.0)));
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
		// Keel: the inside of a derelict warship. Gunmetal hull, hazard-orange accents, sodium work lamps, smoky haze.
		FShieldbreakMapLook& Keel = Out.Add(FName(TEXT("Keel")));
		Keel.GroundStyle = TEXT("stone");
		Keel.Ground = FLinearColor(0.14f, 0.15f, 0.16f);
		Keel.Rock = FLinearColor(0.22f, 0.22f, 0.23f);
		Keel.Metal = FLinearColor(0.30f, 0.32f, 0.35f);
		Keel.DarkMetal = FLinearColor(0.09f, 0.10f, 0.12f);
		Keel.Haze = FLinearColor(0.30f, 0.32f, 0.36f); // cool: was warm, and the hull still read orange (R34)
		// Cool white trim strips: they light the halls as much as the lamps do, and in amber they turned the whole hull
		// orange (R47: the same camera with the strips off reads gunmetal). Amber stays on the hull kit's thin accents.
		Keel.Trim = FLinearColor(0.45f, 0.52f, 0.62f);
		// Cool white kit glow (2026-11-03, R33): with amber glow strips everywhere the Lumen bounce turned the whole hull
		// orange; amber stays on the trim, accents and the hull kit's rail lines.
		Keel.KitGlow = FLinearColor(0.72f, 0.84f, 1.0f);
		Keel.CladAccent = FLinearColor(1.0f, 0.45f, 0.08f);
		// Its own hull plating kit (2026-11-03, build_hull_kit.py): heavy gunmetal plates and frames with louvred amber
		// light slots, so Keel reads as a warship's insides against Spire's white plates. Its colours are fixed in the
		// kit's own palette slots (M_Hull*): the per-map tints set on the cladding at runtime showed no visible effect
		// (R25-R28). -SBQuaterniusClad shows the old panels.
		Keel.CladKit = TEXT("H");
		Keel.bClad = true; // panelled hull plating, bands and deck plates (Quaternius kit) over every block
		// Cool white work lamps, so the hull reads as gunmetal in pools of light and the amber trim and hazard ramps
		// carry the warmth (sodium-orange lamps turned every surface the same orange).
		Keel.InteriorLight = FLinearColor(0.86f, 0.9f, 1.f);
		// Bastion: a snowbound fortress at dusk. Snow-pale ground and stone, dark rock, lavender-orange dusk haze.
		FShieldbreakMapLook& Bastion = Out.Add(FName(TEXT("Bastion")));
		Bastion.GroundStyle = TEXT("stone");
		Bastion.Ground = FLinearColor(0.80f, 0.82f, 0.87f);
		Bastion.Rock = FLinearColor(0.24f, 0.24f, 0.28f);
		Bastion.Metal = FLinearColor(0.30f, 0.33f, 0.38f);
		Bastion.DarkMetal = FLinearColor(0.19f, 0.20f, 0.24f);
		Bastion.Haze = FLinearColor(0.66f, 0.54f, 0.56f);
		Bastion.Trim = FLinearColor(1.0f, 0.62f, 0.26f);
		Bastion.KitGlow = FLinearColor(1.0f, 0.6f, 0.25f);
		Bastion.bRockSkin = true;
		Bastion.bStoneSkin = true;
		Bastion.ScanSet = TEXT("Tundra");
		// Sanctum: an overgrown Forerunner-style sanctuary in a canyon. Scanned stone with moss on top, grassy earth,
		// cliffs and hornbeams round the walls, soft blue-green haze (BuildScanArt).
		FShieldbreakMapLook& Sanctum = Out.Add(FName(TEXT("Sanctum")));
		Sanctum.GroundStyle = TEXT("grass");
		Sanctum.Ground = FLinearColor(0.20f, 0.26f, 0.13f);
		Sanctum.Rock = FLinearColor(0.34f, 0.33f, 0.30f);
		Sanctum.Carved = FLinearColor(0.62f, 0.56f, 0.46f);
		Sanctum.Haze = FLinearColor(0.50f, 0.58f, 0.62f);
		Sanctum.bScan = true;
		// Creekbed (2026-11-02 rework): a green, creek-fed sandstone canyon in warm low sun. Grass on the banks and on
		// the rock tops, tan sandstone, a running creek and a waterfall under the arch, trees along the rim, blue haze.
		FShieldbreakMapLook& Creekbed = Out.Add(FName(TEXT("Creekbed")));
		Creekbed.GroundStyle = TEXT("stone");
		Creekbed.Ground = FLinearColor(0.30f, 0.36f, 0.20f);
		Creekbed.Rock = FLinearColor(0.58f, 0.47f, 0.34f);
		Creekbed.Foliage = FLinearColor(0.22f, 0.30f, 0.12f);
		Creekbed.Haze = FLinearColor(0.60f, 0.70f, 0.82f);
		// Forerunner blue on silver metal against the warm rock (the reference's strongest contrast).
		Creekbed.Trim = FLinearColor(0.30f, 0.75f, 1.0f);
		Creekbed.KitGlow = FLinearColor(0.30f, 0.72f, 1.0f);
		Creekbed.bRockSkin = true;
		Creekbed.bClouds = false;
		Creekbed.ScanSet = TEXT("Creek");
		// The creek over the bed between the banks (ankle deep), through the gorge under the arch, and the waterfall
		// down the north cliff behind it.
		Creekbed.Water = { FBox(FVector(-760.0, -2700.0, 0.0), FVector(760.0, 4000.0, 22.0)) };
		// Spire: the hollow inside of a Forerunner tower. Silver-white metal, blue hard light, cool pale haze.
		FShieldbreakMapLook& Spire = Out.Add(FName(TEXT("Spire")));
		Spire.GroundStyle = TEXT("stone");
		Spire.Ground = FLinearColor(0.36f, 0.38f, 0.42f);
		Spire.Rock = FLinearColor(0.40f, 0.42f, 0.46f);
		Spire.Metal = FLinearColor(0.46f, 0.51f, 0.56f); // satin alloy: highlights carry the silver, not a near-white albedo
		Spire.DarkMetal = FLinearColor(0.10f, 0.13f, 0.16f);
		Spire.Haze = FLinearColor(0.55f, 0.66f, 0.80f);
		Spire.Trim = FLinearColor(0.22f, 0.62f, 1.0f);
		Spire.KitGlow = FLinearColor(0.3f, 0.7f, 1.0f);
		Spire.KitGlowStrength = 0.8f;
		Spire.CladAccent = FLinearColor(0.2f, 0.6f, 1.0f);
		Spire.bClad = true;
		// Its own kit (2026-11-02): large silver plates split by angled seams, so it no longer reads as Keel in blue.
		Spire.CladKit = TEXT("F");
		Spire.InteriorLight = FLinearColor(0.90f, 0.94f, 1.f); // near-neutral key; cyan belongs to the energy mechanism
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
	// -SBClad forces the kit on (comparison shots of a map that does not use it yet).
	if (!GetLook(MapId).bClad && !FParse::Param(FCommandLine::Get(), TEXT("SBClad")))
	{
		return Groups;
	}
	const FVector2D Half = Layout.HalfExtent;
	const bool bSpireBays = MapId == FName(TEXT("Spire")) && !FParse::Param(FCommandLine::Get(), TEXT("SBLegacySpirePanels"));
	auto Inside = [&Half](const FVector2D& P) { return FMath::Abs(P.X) < Half.X - 1.0 && FMath::Abs(P.Y) < Half.Y - 1.0; };
	for (const FShieldbreakMapBlock& Block : Layout.Blocks)
	{
		const FCladRecipe& Recipe = bSpireBays ? SpireCladRecipe(Block.Style) : CladRecipe(Block.Style);
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
			CladFace(Groups, Recipe, At, Face.Normal, Face.Width, Bottom, Height, bSpireBays);
		}
		if (Recipe.Floor[0] && Inside(Center))
		{
			CladTop(Groups, Recipe, Block.Min, Block.Max, nullptr, 0.0, bSpireBays ? 800.0 : 400.0);
		}
		// Raised slabs (roofs, walkways) are seen from below too.
		if (Inside(Center) && Block.Min.Z - Layout.FloorZ >= 150.0 && Size.Z <= 100.0)
		{
			CladTop(Groups, Recipe, Block.Min, Block.Max, TEXT("FloorDark"), Block.Min.Z, bSpireBays ? 800.0 : 400.0);
		}
	}
	for (const FShieldbreakMapRamp& Ramp : Layout.Ramps)
	{
		const FCladRecipe& Recipe = bSpireBays ? SpireCladRecipe(Ramp.Style) : CladRecipe(Ramp.Style);
		if (!Recipe.Floor[0])
		{
			continue;
		}
		// The ramp's top surface runs along the centreline from Start to End (AShieldbreakArena::AddRamp).
		constexpr double Lift = 1.0;
		const double TileSize = bSpireBays ? 800.0 : 400.0;
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
				const FVector Scale(TileLong / 400.0, TileShort / 400.0, 1.0);
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
				Group(Groups, Recipe.Edge).Add(FTransform(EdgeRotation, P, FVector(1.0, TileLong / 400.0, Ramp.Thickness / 100.0)));
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

	// Enclosed maps have no outside to dress: their one hero piece hangs from the ceiling over the middle, above
	// anywhere a player can stand or jump to (visual only, so it must never hide a player or seem to block a shot).
	if (Layout.bIndoor)
	{
		double CeilingZ = Layout.FloorZ + 1500.0;
		for (const FShieldbreakMapBlock& Block : Layout.Blocks)
		{
			if (Block.Max.X - Block.Min.X >= 1.8 * Layout.HalfExtent.X && Block.Max.Y - Block.Min.Y >= 1.8 * Layout.HalfExtent.Y
				&& Block.Min.Z > Layout.FloorZ + 300.0)
			{
				CeilingZ = Block.Min.Z;
			}
		}
		if (Id == TEXT("Keel"))
		{
			// The Cradle (Art/blender/build_keel_cradle.py, docs/KEEL_MIDSHIP_STUDY.md): the cargo pod's shell and its
			// cables, the overhead crane it hangs from, and each side's crane arm and trench wall. The pieces are
			// authored in map coordinates round the blockout's pod (z 400..650), ramps and trench, so they sit at the
			// origin; blue's are red's turned half round. Visual only: each hugs a block or hangs clear of players.
			const FVector Floor(0.0, 0.0, Layout.FloorZ);
			Group(Groups, TEXT("CradleGantry")).Add(FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, CeilingZ), FVector(1.f)));
			Group(Groups, TEXT("CradlePod")).Add(FTransform(FRotator::ZeroRotator, Floor, FVector(1.f)));
			for (const float Yaw : { 0.f, 180.f })
			{
				Group(Groups, TEXT("CraneArm")).Add(FTransform(FRotator(0.f, Yaw, 0.f), Floor, FVector(1.f)));
				Group(Groups, TEXT("TrenchWall")).Add(FTransform(FRotator(0.f, Yaw, 0.f), Floor, FVector(1.f)));
			}
		}
		else if (Id == TEXT("Spire"))
		{
			// Original triad induction assembly above the ordinary player/lift envelope. Keep Claude's earlier
			// stepped-ring model available for direct comparison, without deleting or reimporting its asset.
			const TCHAR* Piece = FParse::Param(FCommandLine::Get(), TEXT("SBLegacySpireCondenser")) ? TEXT("Condenser") : TEXT("CondenserV2");
			Group(Groups, Piece).Add(FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, CeilingZ), FVector(1.f)));
		}
		return Groups;
	}

	// With the scanned terrain drawn (Bastion's snow-topped quarry stone), the ashlar tiles would hide it: rock skin only.
	const bool bStoneSkin = GetLook(MapId).bStoneSkin && !TerrainScanReady(MapId);
	if (GetLook(MapId).bRockSkin || bStoneSkin)
	{
		RockSkin(Groups, Layout, GetLook(MapId).bRockSkin, bStoneSkin);
	}

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
		// Hero landmark: a keep-tower with a beacon fire beyond the north wall, on the centre line (neutral: the map
		// mirrors across x = 0), so both teams share one skyline marker for the long-range north.
		for (const FSide& Side : Sides(Layout))
		{
			if (IsLongSide(Side) && Side.Normal.Y > 0.0)
			{
				Group(Groups, TEXT("Watchtower")).Add(FacingArena(Layout, Side, 2600.f, 0.f, FVector(1.1f)));
			}
		}
	}
	else if (Id == TEXT("Sanctum"))
	{
		// A sun temple in a desert basin: monoliths line the long walls, broken arches stand behind each temple end
		// (not with the scanned canyon, whose cliffs stand right behind the walls).
		for (const FSide& Side : Sides(Layout))
		{
			if (GetLook(MapId).bScan)
			{
				break;
			}
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
		// With the scanned art the canyon cliffs (BuildScanArt) replace the faceted rocks, and the bare scanned stone
		// replaces the crowns and carved reliefs.
		if (!GetLook(MapId).bScan)
		{
			Rocks(TEXT("RockShelf"), { 18, 2000.f, 10000.f, 1.2f, 2.4f, 2000.f });
			Rocks(TEXT("RockBoulder"), { 26, 1200.f, 9000.f, 0.7f, 1.5f, 1000.f });
			Rocks(TEXT("RockTall"), { 8, 4000.f, 10000.f, 1.4f, 2.2f, 1200.f });
			// Inside: crowns on the spires, colossi and obelisks, and carved reliefs on the temples, shrines, annexes
			// and ruins unless the sci-fi kit clads them (Cladding).
			DressStructures(Groups, Layout, { TEXT("red"), TEXT("blue"), TEXT("stone"), TEXT("tower"), TEXT("bridge") }, 1050.f,
				!GetLook(MapId).bClad);
		}
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
		// A red-rock canyon: sandstone shelves and boulders crowd the walls, tall buttes further out. No trees.
		Rocks(TEXT("RockShelf"), { 24, 600.f, 8000.f, 1.3f, 2.6f, 1800.f });
		Rocks(TEXT("RockBoulder"), { 40, 200.f, 7000.f, 0.6f, 1.6f, 900.f });
		Rocks(TEXT("RockTall"), { 18, 2500.f, 10000.f, 1.6f, 2.8f, 1400.f });
		// Hero landmark: a natural sandstone arch beyond the south cliff, spanning the skyline behind the cave.
		for (const FSide& Side : Sides(Layout))
		{
			if (IsLongSide(Side) && Side.Normal.Y < 0.0)
			{
				Group(Groups, TEXT("SandstoneArch")).Add(FacingArena(Layout, Side, 3800.f, 600.f, FVector(1.35f)));
			}
		}
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
		if (GetLook(MapId).bScan)
		{
			break;
		}
		if (!IsLongSide(Side) && (Side.Normal.X > 0.0) == (Random.FRand() < 0.5f))
		{
			Group(Groups, TEXT("Monument")).Add(FacingArena(Layout, Side, 3500.f, Random.FRandRange(-0.3f, 0.3f) * Side.Along, FVector(1.f)));
			break;
		}
	}
	return Groups;
}
