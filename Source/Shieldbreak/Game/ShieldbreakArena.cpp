#include "Game/ShieldbreakArena.h"

#include "Shieldbreak.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DecalComponent.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Audio/ShieldbreakAmbience.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/HitResult.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/ShieldbreakGameMode.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/ShieldbreakWeaponPickup.h"
#include "Weapons/ShieldbreakPowerup.h"
#include "Game/ShieldbreakGravLift.h"
#include "Game/ShieldbreakMapArt.h"
#include "Visuals/ShieldbreakArt.h"

namespace ShieldbreakArenaPrivate
{
	constexpr float WaypointLift = 50.f;
	/** Grid spacing of the navigation scan, and the longest walk link (covers two grid steps and diagonals). */
	constexpr float NavGridStep = 300.f;
	constexpr float MaxLinkDistance = 650.f;
	constexpr float MaxDropHeight = 700.f;

	const TArray<int32> EmptyNeighbors;

	/** Surface kind (an MI_SB_World_<Kind> instance) for a palette style; None keeps the flat colour. */
	FName SurfaceKind(FName Style)
	{
		static const FName Glow(TEXT("glow")), Water(TEXT("water")), Floor(TEXT("floor")), Grass(TEXT("grass"));
		static const FName Stone(TEXT("stone")), Rock(TEXT("rock")), Sand(TEXT("sand"));
		if (Style.IsNone() || Style == Glow || Style == Water)
		{
			return NAME_None;
		}
		if (Style == Floor)
		{
			return TEXT("Tile");
		}
		if (Style == Grass)
		{
			return TEXT("Grass");
		}
		if (Style == Stone || Style == Rock || Style == Sand)
		{
			return TEXT("Stone");
		}
		// Walls, towers, bridges, team bases, cover, ramps: Forerunner-style metal panels.
		return TEXT("Panel");
	}

	/** Built structures (not the floor or natural rock) get glowing trim. */
	bool HasTrim(FName Style)
	{
		return SurfaceKind(Style) == FName(TEXT("Panel")) && Style != FName(TEXT("ramp")) && Style != FName(TEXT("slide"));
	}

	const FLinearColor TrimRed(1.f, 0.22f, 0.12f);
	const FLinearColor TrimBlue(0.2f, 0.45f, 1.f);

	/** Lumen bounce light and reflections are on unless -SBNoLumen (see SpawnLighting). */
	bool UsesLumen()
	{
		return !FParse::Param(FCommandLine::Get(), TEXT("SBNoLumen"));
	}

	/** Emissive share of the mountain ring's colour; -SBMountainLight=<x> overrides it for comparisons. */
	float MountainSelfLight()
	{
		// Lumen's sky light reaches less of the far ring than the old flat sky light did.
		float Value = UsesLumen() ? 0.6f : 0.35f;
		FParse::Value(FCommandLine::Get(), TEXT("SBMountainLight="), Value);
		return FMath::Max(0.f, Value);
	}
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeFinder.Succeeded())
	{
		ConeMesh = ConeFinder.Object;
	}
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
	// Canonicalize before naming: defaults, unknown ids and case variants must produce the
	// same actor path on both machines. Map-specific names also separate a replacement arena.
	const FName ResolvedMapId = ShieldbreakMaps::GetMapIds().Contains(InMapId)
		? InMapId : ShieldbreakMaps::GetDefaultMapId();
	if (AShieldbreakArena* Existing = Find(World))
	{
		if (InMapId.IsNone() || Existing->GetMapId() == ResolvedMapId)
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

	// Characters stand on the arena's pieces, and replicated movement references its movement base by path, so
	// the arena and its pieces must have the same names on every machine and be net-addressable.
	FActorSpawnParameters Params;
	Params.Name = FName(*FString::Printf(TEXT("ShieldbreakArena_%s"), *ResolvedMapId.ToString().ToLower()));
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	if (UObject* NameHolder = StaticFindObjectFast(nullptr, World->PersistentLevel, Params.Name))
	{
		// A destroyed arena awaiting garbage collection still holds the name (map changes).
		NameHolder->Rename(*MakeUniqueObjectName(World->PersistentLevel, NameHolder->GetClass()).ToString(), nullptr,
			REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional);
	}

	AShieldbreakArena* Arena = World->SpawnActor<AShieldbreakArena>(AShieldbreakArena::StaticClass(), FTransform::Identity, Params);
	if (Arena)
	{
		Arena->SetNetAddressable();
		Arena->MapId = ResolvedMapId;
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
		LoadSurfaceMaterials();
		BuildGeometry();
		BuildTrim();
		BuildSpireArt();
		BuildMapArt();
		BuildCladding();
		BuildScanArt();
		BuildTerrainScan();
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
		if (Layout.bIndoor)
		{
			BuildInteriorLights();
			BuildSpireHeroLights();
		}
		else
		{
			BuildBackdrop();
		}
	}

	// Bots and pickups only in a match. The front end does not build an arena; this also covers a backdrop.
	const AShieldbreakGameMode* Rules = GetWorld()->GetAuthGameMode<AShieldbreakGameMode>();
	if (GetNetMode() != NM_DedicatedServer && !(Rules && Rules->IsFrontEnd()))
	{
		ShieldbreakAmbience::Start(this, MapId, Layout.bIndoor, ShieldbreakMapArt::GetLook(MapId).Water);
	}
	if (HasAuthority() && GetNetMode() != NM_Client && Rules && !Rules->IsFrontEnd())
	{
		BuildWaypointGraph();
		SpawnPickups();
	}
}

void AShieldbreakArena::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed && GetWorld())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->GetOwner() == this)
			{
				It->Destroy();
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

// ---- Geometry helpers ----

void AShieldbreakArena::LoadSurfaceMaterials()
{
	SurfaceMaterials.Reset();
	GlowMaterial = nullptr;
	// -SBFlatMaterials keeps the old flat colours (for comparison shots and slow machines).
	if (FParse::Param(FCommandLine::Get(), TEXT("SBFlatMaterials")))
	{
		return;
	}
	for (const TCHAR* Kind : { TEXT("Panel"), TEXT("Tile"), TEXT("Stone"), TEXT("Grass") })
	{
		const FString Path = FString::Printf(TEXT("/Game/Shieldbreak/Art/Materials/MI_SB_World_%s.MI_SB_World_%s"), Kind, Kind);
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			SurfaceMaterials.Add(FName(Kind), Material);
		}
	}
	GlowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Shieldbreak/Art/Materials/M_SB_Surface.M_SB_Surface"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

UMaterialInstanceDynamic* AShieldbreakArena::ApplySurface(UMeshComponent* Mesh, FName Style, const FLinearColor& Color, float TileScale) const
{
	if (Style == FName(TEXT("glow")) && GlowMaterial)
	{
		Mesh->SetMaterial(0, GlowMaterial);
		UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color * 0.2f);
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
			MID->SetScalarParameterValue(TEXT("EmissiveStrength"), ShieldbreakMapArt::GetLook(MapId).KitGlowStrength);
			MID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			MID->SetScalarParameterValue(TEXT("Roughness"), 0.4f);
		}
		return MID;
	}

	// Keel: the bare block faces (team pillars, walls, ramps) take the same scanned hull materials as its kit, so no flat
	// panel-pattern block shows between the plating. Team blocks keep a muted hue of their colour over the scanned
	// steel; ramps get the hazard paint. -SBLegacyKeelHull keeps the world materials.
	if (MapId == FName(TEXT("Keel")) && !FParse::Param(FCommandLine::Get(), TEXT("SBLegacyKeelHull")))
	{
		const FString Name = Style.ToString();
		// With the Fab pack grid on (pack tiles on the ramp tops), the ramp slabs' sides and undersides go dark steel.
		const bool bPackRamps = !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelPacks")) && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelPackKit"))
			&& FPackageName::DoesPackageExist(TEXT("/Game/Modular_Scifi_Mechanic_Base/Mesh/SM/SRTUCTURE/FLOOR/SM_FLOOR_08"));
		const TCHAR* Kind = Name == TEXT("floor") ? TEXT("Deck") : (Name == TEXT("ramp") || Name == TEXT("slide")) ? (!FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelAlienSlice")) ? TEXT("MauveDark") : bPackRamps ? TEXT("Frame") : TEXT("Hazard"))
			: (Name == TEXT("red") || Name == TEXT("blue") || Name == TEXT("wall") || Name == TEXT("metal") || Name == TEXT("bridge")
				|| Name == TEXT("dark") || Name == TEXT("cover")) ? TEXT("Plate") : nullptr;
		if (Kind && FCString::Strcmp(Kind, TEXT("Plate")) == 0 && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelAlienSlice")))
		{
			Kind = (Name == TEXT("bridge") || Name == TEXT("cover") || Name == TEXT("metal")) ? TEXT("MauveDark") : TEXT("Mauve");
		}
		if (UMaterialInterface* Hull = Kind ? ShieldbreakArt::Load<UMaterialInterface>(*FString::Printf(
			TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_Keel%s.MI_SB_Keel%s"), Kind, Kind)) : nullptr)
		{
			Mesh->SetMaterial(0, Hull);
			if (FCString::Strcmp(Kind, TEXT("Plate")) == 0 && (Name == TEXT("red") || Name == TEXT("blue") || Name == TEXT("dark") || Name == TEXT("wall")))
			{
				if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
				{
					// Team blocks: only a hint of the team colour over the gunmetal. At full strength the red side's pillars
					// read as terracotta brick (k10 shots); the team colour belongs to the lights and accents.
					const FLinearColor Hue = Name == TEXT("red") ? FLinearColor(0.165f, 0.13f, 0.125f)
						: Name == TEXT("blue") ? FLinearColor(0.12f, 0.135f, 0.165f) : FLinearColor(0.08f, 0.085f, 0.095f);
					MID->SetVectorParameterValue(TEXT("BaseColor"), Hue);
					return MID;
				}
			}
			return nullptr;
		}
	}

	const TObjectPtr<UMaterialInterface>* World = SurfaceMaterials.Find(ShieldbreakArenaPrivate::SurfaceKind(Style));
	if (World && *World)
	{
		Mesh->SetMaterial(0, *World);
		UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Tint"), Color);
			float TileSize = 0.f;
			if (TileScale != 1.f && MID->GetScalarParameterValue(FHashedMaterialParameterInfo(FName(TEXT("TileSize"))), TileSize))
			{
				MID->SetScalarParameterValue(TEXT("TileSize"), TileSize * TileScale);
			}
		}
		return MID;
	}

	if (BaseMaterial)
	{
		Mesh->SetMaterial(0, BaseMaterial);
	}
	UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0);
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return MID;
}

template <typename T>
T* AShieldbreakArena::AddDecoration(const TCHAR* BaseName, UStaticMesh* Mesh)
{
	T* Component = NewObject<T>(this, FName(BaseName, Decorations.Num() + 1), RF_Transient);
	if (!Component)
	{
		return nullptr;
	}
	// Deterministic names plus this flag keep the self-test's stable-path check true for trim and backdrop meshes.
	Component->SetNetAddressable();
	Component->SetMobility(EComponentMobility::Static);
	if (Mesh)
	{
		Component->SetStaticMesh(Mesh);
	}
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetupAttachment(Root);
	Decorations.Add(Component);
	return Component;
}

UStaticMeshComponent* AShieldbreakArena::AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color, FName Style, const FRotator& Rotation)
{
	// Blocks, then ramps, are built in layout order on every machine. Never rely on the
	// process-wide auto-name counter: it differs between listen server and joining clients.
	const FName PieceName(*FString::Printf(TEXT("Piece_%03d"), Pieces.Num()));
	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this, PieceName, RF_Transient);
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
	Piece->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Piece->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);
	Piece->SetGenerateOverlapEvents(false);
	Piece->SetupAttachment(Root);
	Piece->SetRelativeLocationAndRotation(Center, Rotation);
	Piece->SetRelativeScale3D(Size / 100.f);
	// Same build order on every machine, so the same name: a movement base the server can reference.
	Piece->RegisterComponent();
	AddInstanceComponent(Piece);

	ApplySurface(Piece, Style, Color);

	Pieces.Add(Piece);
	return Piece;
}

UStaticMeshComponent* AShieldbreakArena::AddBlock(const FVector& Min, const FVector& Max, const FLinearColor& Color, FName Style)
{
	return AddBox((Min + Max) * 0.5f, (Max - Min).GetAbs(), Color, Style);
}

UStaticMeshComponent* AShieldbreakArena::AddRamp(const FVector& Start, const FVector& End, float Width, float Thickness, const FLinearColor& Color, FName Style)
{
	const FVector Delta = End - Start;
	const float Length = static_cast<float>(Delta.Size()) + 30.f; // overlap both ends a little
	const FRotator Rotation = Delta.Rotation();
	const FVector Up = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z);
	const FVector Center = (Start + End) * 0.5f - Up * (Thickness * 0.5f);
	return AddBox(Center, FVector(Length, Width, Thickness), Color, Style, Rotation);
}

// ---- Layout ----

void AShieldbreakArena::BuildGeometry()
{
	for (const FShieldbreakMapBlock& Block : Layout.Blocks)
	{
		UStaticMeshComponent* Piece = AddBlock(Block.Min, Block.Max, Block.Color, Block.Style);
		// An enclosed map's ceiling: the block that spans the whole map well above the floor.
		if (Layout.bIndoor && Block.Min.Z > Layout.FloorZ + 300.0 && Block.Min.X <= -Layout.HalfExtent.X && Block.Max.X >= Layout.HalfExtent.X
			&& Block.Min.Y <= -Layout.HalfExtent.Y && Block.Max.Y >= Layout.HalfExtent.Y)
		{
			Ceiling = Piece;
		}
	}
	for (const FShieldbreakMapRamp& RampData : Layout.Ramps)
	{
		AddRamp(RampData.Start, RampData.End, RampData.Width, RampData.Thickness, RampData.Color, RampData.Style);
	}
}

namespace ShieldbreakScanPrivate
{
	bool IsPerimeter(const FShieldbreakMapLayout& Layout, const FShieldbreakMapBlock& Block);
	UStaticMesh* AltarFor(const FShieldbreakMapLayout& Layout, const FShieldbreakMapBlock& Block);
}

void AShieldbreakArena::BuildTrim()
{
	using namespace ShieldbreakArenaPrivate;
	if (!GlowMaterial || !CubeMesh)
	{
		return;
	}

	// One instanced mesh per colour: three draw calls for every trim line on the map.
	const FName RedStyle(TEXT("red")), BlueStyle(TEXT("blue"));
	UInstancedStaticMeshComponent* Strips[3] = { nullptr, nullptr, nullptr };
	FLinearColor Colors[3] = { ShieldbreakMapArt::GetLook(MapId).Trim, TrimRed, TrimBlue };
	// Keel: the team trim lines were most of the light in the bridges, so their bounce painted the scanned steel
	// terracotta red (-SBTrimGlow=0 shot the pillars near black). Mostly the hall's cold white with a hint of team.
	// -SBKeelTeamTrim keeps the full team colours.
	if (MapId == FName(TEXT("Keel")) && !FParse::Param(FCommandLine::Get(), TEXT("SBKeelTeamTrim")))
	{
		Colors[1] = FMath::Lerp(TrimRed, Colors[0], 0.7f);
		Colors[2] = FMath::Lerp(TrimBlue, Colors[0], 0.7f);
	}
	// Enclosed maps: half as bright, or the strips bloom the dim halls out to a wash. -SBTrimGlow=<strength> to compare.
	float TrimGlow = Layout.bIndoor ? 7.f : 14.f;
	const bool bSpireStructure = MapId == FName(TEXT("Spire"))
		&& !FParse::Param(FCommandLine::Get(), TEXT("SBLegacySpirePanels"));
	if (bSpireStructure) TrimGlow = 2.5f;
	FParse::Value(FCommandLine::Get(), TEXT("SBTrimGlow="), TrimGlow);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Strips[Index] = AddDecoration<UInstancedStaticMeshComponent>(TEXT("Trim"), CubeMesh);
		Strips[Index]->SetCastShadow(false);
		// Brighter than glow blocks: the lines are thin and mostly seen from across the map.
		if (UMaterialInstanceDynamic* MID = ApplySurface(Strips[Index], TEXT("glow"), Colors[Index]))
		{
			MID->SetScalarParameterValue(TEXT("EmissiveStrength"), TrimGlow);
		}
	}

	constexpr float StripHeight = 14.f, StripDepth = 6.f, TopInset = 30.f, FootHeight = 60.f, EndInset = 30.f;
	const bool bCarvedStone = ShieldbreakMapArt::GetLook(MapId).bScan && !FParse::Param(FCommandLine::Get(), TEXT("SBNoScan"))
		&& !FParse::Param(FCommandLine::Get(), TEXT("SBNoCarve"));
	// Terrain-scan maps bury the perimeter walls in cliffs: a glow line along them reads as a white seam in the rock.
	const bool bTerrainHidesPerimeter = ShieldbreakMapArt::TerrainScanReady(MapId) && !FParse::Param(FCommandLine::Get(), TEXT("SBNoTerrainScan"));
	const FVector2D Half = Layout.HalfExtent;
	int32 Count = 0;
	for (const FShieldbreakMapBlock& Block : Layout.Blocks)
	{
		const FVector3f Size(Block.Max - Block.Min);
		// Quiet structural piers and perimeter bays. Team portals retain their colour, and the central
		// mechanism retains its energy; neutral supports no longer wear glowing outlines at both ends.
		if (bSpireStructure && Block.Style != RedStyle && Block.Style != BlueStyle
			&& Block.Style != FName(TEXT("tower")))
		{
			continue;
		}
		// Sanctum's twist on the stone walkways: a thin line of hard light along each long edge, just under the lip.
		if (bCarvedStone && Block.Style == FName(TEXT("bridge")) && Size.Z < 120.f)
		{
			const FVector Center = (Block.Min + Block.Max) * 0.5;
			const bool bAlongY = Size.Y > Size.X;
			const float Length = (bAlongY ? Size.Y : Size.X) - 2.f * EndInset;
			const float Offset = (bAlongY ? Size.X : Size.Y) * 0.5f + StripDepth * 0.5f - 1.f;
			if (Length >= 250.f)
			{
				const FVector Scale = bAlongY ? FVector(StripDepth, Length, 8.f) : FVector(Length, StripDepth, 8.f);
				for (const float Side : { -1.f, 1.f })
				{
					const FVector At = Center + (bAlongY ? FVector(Side * Offset, 0.0, 0.0) : FVector(0.0, Side * Offset, 0.0));
					Strips[0]->AddInstance(FTransform(FRotator::ZeroRotator, FVector(At.X, At.Y, Block.Min.Z + 10.0), Scale / 100.f));
					++Count;
				}
			}
			continue;
		}
		if (!HasTrim(Block.Style) || Size.Z < 120.f || (bTerrainHidesPerimeter && ShieldbreakScanPrivate::IsPerimeter(Layout, Block))
			|| (bCarvedStone && (ShieldbreakScanPrivate::IsPerimeter(Layout, Block)
			|| ShieldbreakScanPrivate::AltarFor(Layout, Block))))
		{
			continue;
		}
		UInstancedStaticMeshComponent* Strip = Strips[Block.Style == RedStyle ? 1 : Block.Style == BlueStyle ? 2 : 0];
		TArray<float, TInlineAllocator<2>> Heights;
		Heights.Add(static_cast<float>(Block.Max.Z) - TopInset);
		// Carved scanned stone (BuildScanArt) leans out at the foot over where this line would sit: top line only.
		if (Size.Z >= 400.f && !bCarvedStone)
		{
			Heights.Add(static_cast<float>(Block.Min.Z) + FootHeight);
		}

		const FVector Center = (Block.Min + Block.Max) * 0.5;
		// The four side faces: outward normal, distance from the centre and length along the face.
		struct FFace { FVector Normal; float Offset; float Length; };
		const FFace Faces[4] = {
			{ FVector(1.0, 0.0, 0.0), Size.X * 0.5f, Size.Y }, { FVector(-1.0, 0.0, 0.0), Size.X * 0.5f, Size.Y },
			{ FVector(0.0, 1.0, 0.0), Size.Y * 0.5f, Size.X }, { FVector(0.0, -1.0, 0.0), Size.Y * 0.5f, Size.X } };
		for (const FFace& Face : Faces)
		{
			if (Face.Length < 250.f)
			{
				continue;
			}
			const FVector FaceCenter = Center + Face.Normal * (Face.Offset + StripDepth * 0.5f - 1.f);
			// Skip faces that look out of the map (the back of the perimeter walls).
			if (FMath::Abs(FaceCenter.X) > Half.X + 1.f || FMath::Abs(FaceCenter.Y) > Half.Y + 1.f)
			{
				continue;
			}
			const bool bAlongY = Face.Normal.X != 0.0;
			const float Length = Face.Length - 2.f * EndInset;
			const FVector Scale = bAlongY ? FVector(StripDepth, Length, StripHeight) : FVector(Length, StripDepth, StripHeight);
			for (const float Z : Heights)
			{
				Strip->AddInstance(FTransform(FRotator::ZeroRotator, FVector(FaceCenter.X, FaceCenter.Y, Z), Scale / 100.f));
				++Count;
			}
		}
	}
	for (UInstancedStaticMeshComponent* Strip : Strips)
	{
		Strip->RegisterComponent();
		AddInstanceComponent(Strip);
	}
	UE_LOG(LogShieldbreak, Log, TEXT("%s: %d trim lines."), *Layout.DisplayName, Count);
}

void AShieldbreakArena::BuildSpireArt()
{
	if (MapId != FName(TEXT("Spire")) || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Authoring coordinates are local centimetres. Modules stay outside the walls,
	// or flush with the floor at lifts, so collision, sight lines and jumps stay authored.
	auto Module = [this](const TCHAR* Name, const TCHAR* Style, FLinearColor Tint, const TCHAR* SurfaceKind = nullptr)
	{
		const FString Path = FString::Printf(TEXT("/Game/Shieldbreak/Art/Spire/%s.%s"), Name, Name);
		UStaticMesh* Mesh = ShieldbreakArt::Load<UStaticMesh>(*Path);
		if (!Mesh) return static_cast<UInstancedStaticMeshComponent*>(nullptr);
		auto* Instances = AddDecoration<UInstancedStaticMeshComponent>(TEXT("SpireArt"), Mesh);
		Instances->SetCastShadow(false);
		ApplySurface(Instances, Style, Tint);
		if (SurfaceKind)
		{
			if (UMaterialInterface* Surface = ShieldbreakArt::Load<UMaterialInterface>(*FString::Printf(
				TEXT("/Game/Shieldbreak/Art/Spire/MI_SB_Spire%s.MI_SB_Spire%s"), SurfaceKind, SurfaceKind)))
			{
				Instances->SetMaterial(0, Surface);
				if (UMaterialInstanceDynamic* MID = Instances->CreateDynamicMaterialInstance(0))
				{
					MID->SetVectorParameterValue(TEXT("Tint"), Tint);
				}
			}
		}
		return Instances;
	};
	auto* Collectors = Module(TEXT("SM_SB_Spire_Collector"), TEXT("metal"), FLinearColor(0.32f, 0.40f, 0.43f));
	auto* Panels = Module(TEXT("SM_SB_Spire_ServicePanel"), TEXT("metal"), FLinearColor(0.16f, 0.22f, 0.25f));
	auto* Collars = Module(TEXT("SM_SB_Spire_LiftCollar"), TEXT("glow"), FLinearColor(0.16f, 0.65f, 0.8f));
	for (const float Side : { -1.f, 1.f })
	{
		for (const float X : { -3300.f, -1800.f, 0.f, 1800.f, 3300.f })
		{
			if (Collectors)
			{
				const float HeightScale = X == 0.f ? 1.8f : (FMath::Abs(X) < 2000.f ? 1.35f : 1.1f);
				Collectors->AddInstance(FTransform(FRotator(0.f, Side < 0.f ? 0.f : 180.f, 0.f),
					FVector(X * Layout.HalfExtent.X / 4200.f, Side * (Layout.HalfExtent.Y + 180.f), 700.f), FVector(1.f, 1.f, HeightScale)));
			}
			if (Panels)
			{
				// Face inward, set into the wall's inner surface. Raised above the lower route.
				Panels->AddInstance(FTransform(FRotator(0.f, Side < 0.f ? 180.f : 0.f, 0.f),
					FVector(X * Layout.HalfExtent.X / 4200.f, Side * (Layout.HalfExtent.Y + 25.f), 1080.f), FVector(2.f, 1.f, 1.f)));
			}
		}
	}
	if (Collars)
	{
		for (const auto& Lift : Layout.Lifts)
		{
			Collars->AddInstance(FTransform(FRotator::ZeroRotator, Lift.Base + FVector(0.f, 0.f, 1.f), FVector(1.f)));
		}
	}
	UInstancedStaticMeshComponent* ArchAlloy = nullptr;
	UInstancedStaticMeshComponent* ArchInsets = nullptr;
	UInstancedStaticMeshComponent* Spines = nullptr;
	if (Layout.bIndoor && Ceiling && !FParse::Param(FCommandLine::Get(), TEXT("SBNoSpireArchitecture")))
	{
		const FShieldbreakMapLook& Look = ShieldbreakMapArt::GetLook(MapId);
		ArchAlloy = Module(TEXT("SM_SB_Spire_ArchAlloy"), TEXT("metal"), Look.Metal, TEXT("Alloy"));
		ArchInsets = Module(TEXT("SM_SB_Spire_ArchInset"), TEXT("metal"), Look.DarkMetal, TEXT("Inset"));
		Spines = Module(TEXT("SM_SB_Spire_CeilingSpine"), TEXT("metal"), Look.DarkMetal, TEXT("Inset"));
		const double CeilingZ = Ceiling->Bounds.GetBox().Min.Z - GetActorLocation().Z;
		const double InnerY = Layout.HalfExtent.Y * 0.25;
		const double SpanScale = (Layout.HalfExtent.Y - InnerY) / 2700.0;
		const double StationSpacing = Layout.HalfExtent.X / 3.0;
		for (int32 Station = -2; Station <= 2; ++Station)
		{
			for (const double Side : { -1.0, 1.0 })
			{
				const FTransform At(FRotator(0.f, Side * 90.0, 0.f),
					FVector(Station * StationSpacing, Side * InnerY, CeilingZ - 300.0), FVector(SpanScale, 1.0, 1.0));
				if (ArchAlloy) ArchAlloy->AddInstance(At);
				if (ArchInsets) ArchInsets->AddInstance(At);
				if (Station < 2 && Spines)
				{
					Spines->AddInstance(FTransform(FRotator::ZeroRotator,
						FVector((Station + 0.5) * StationSpacing, Side * InnerY, CeilingZ - 160.0),
						FVector(StationSpacing / 2400.0, 1.0, 1.0)));
				}
			}
		}
	}
	for (auto* Instances : { Collectors, Panels, Collars, ArchAlloy, ArchInsets, Spines })
	{
		if (Instances)
		{
			Instances->RegisterComponent();
			AddInstanceComponent(Instances);
		}
	}
	UE_LOG(LogShieldbreak, Log, TEXT("Spire art: %d collectors, %d service panels, %d lift collars (visual only)."),
		Collectors ? Collectors->GetInstanceCount() : 0, Panels ? Panels->GetInstanceCount() : 0, Collars ? Collars->GetInstanceCount() : 0);
	UE_LOG(LogShieldbreak, Log, TEXT("Spire ceiling: %d alloy ribs, %d recessed webs, %d spine sections (visual only, above ordinary jump/lift arcs)."),
		ArchAlloy ? ArchAlloy->GetInstanceCount() : 0, ArchInsets ? ArchInsets->GetInstanceCount() : 0, Spines ? Spines->GetInstanceCount() : 0);
}

void AShieldbreakArena::ApplyKitSurfaces(UMeshComponent* Mesh, const FShieldbreakMapLook& Look, float SelfLight) const
{
	const UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Mesh);
	const UStaticMesh* Asset = StaticMesh ? StaticMesh->GetStaticMesh() : nullptr;
	if (!Asset)
	{
		return;
	}
	// Palette slot (Art/blender/sb_kit.py PALETTE) -> world surface kind and this map's tint for it.
	struct FSlotLook { const TCHAR* Prefix; const TCHAR* Kind; FLinearColor Tint; };
	const FSlotLook SlotLooks[] = {
		{ TEXT("M_Steel"), TEXT("Panel"), Look.Metal }, { TEXT("M_Gunmetal"), TEXT("Panel"), Look.DarkMetal },
		{ TEXT("M_Ivory"), TEXT("Stone"), Look.Carved }, { TEXT("M_Sand"), TEXT("Stone"), Look.Rock },
		{ TEXT("M_Bark"), TEXT("Stone"), Look.Bark }, { TEXT("M_Olive"), TEXT("Grass"), Look.Foliage } };
	// -SBKitAmber=<strength>: Keel's amber accents (M_HullAmber) on the glow material at that strength (0 turns them
	// off), to find how much of the hull's warm cast is their bounce light. Unset keeps the authored material.
	float KitAmber = -1.f;
	FParse::Value(FCommandLine::Get(), TEXT("SBKitAmber="), KitAmber);
	// Keel's hull kit takes the scanned hull materials (Tools/create_keel_surfaces.py: CC0 scans, one world-projected
	// master) on its plate, deck and frame slots. -SBLegacyKeelHull keeps the flat palette instances, for comparisons.
	const bool bKeelScanHull = MapId == FName(TEXT("Keel")) && !FParse::Param(FCommandLine::Get(), TEXT("SBLegacyKeelHull"));
	const TArray<FStaticMaterial>& Slots = Asset->GetStaticMaterials();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FString Slot = Slots[Index].MaterialSlotName.ToString();
		if (bKeelScanHull)
		{
			const TCHAR* HullKinds[] = { TEXT("Plate"), TEXT("Deck"), TEXT("Frame"), TEXT("Hazard") };
			const bool bAlienHazard = !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelAlienSlice"));
			UMaterialInterface* Scanned = nullptr;
			for (const TCHAR* Kind : HullKinds)
			{
				if (Slot.StartsWith(FString(TEXT("M_Hull")) + Kind))
				{
					// The alien look: plates go mauve, frames and hazard paint dark mauve, so no grey or orange human hull is left.
					const TCHAR* Use = !bAlienHazard ? Kind : FCString::Strcmp(Kind, TEXT("Plate")) == 0 ? TEXT("Mauve")
						: (FCString::Strcmp(Kind, TEXT("Frame")) == 0 || FCString::Strcmp(Kind, TEXT("Hazard")) == 0
							|| FCString::Strcmp(Kind, TEXT("Deck")) == 0) ? TEXT("MauveDark") : Kind;
					Scanned = ShieldbreakArt::Load<UMaterialInterface>(*FString::Printf(
						TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_Keel%s.MI_SB_Keel%s"), Use, Use));
					break;
				}
			}
			// The wall plates' big amber panels read as glowing terracotta, and as hazard paint they still turned whole
			// pillars orange: they take the gunmetal plate, so orange stays an accent (pod bands, ramp edges).
			// -SBKeelHazardPlates paints them hazard orange instead. The thin amber accents on the bands keep their glow.
			// The reactor pylons breathe: their light slots and columns pulse slowly, out of step (Tools/create_keel_fx.py).
			if (!Scanned && Slot.StartsWith(TEXT("M_HullLight")) && Asset->GetName().StartsWith(TEXT("SM_SB_Kit_ReactorPylon"))
				&& !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelPulse")))
			{
				Scanned = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelPulse.MI_SB_KeelPulse"));
				if (Scanned && bAlienHazard)
				{
					Mesh->SetMaterial(Index, Scanned);
					if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(Index))
					{
						MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.55f, 0.18f, 1.0f));
					}
					continue;
				}
			}
			// Other light and amber accents glow violet in the alien look.
			if (!Scanned && bAlienHazard && (Slot.StartsWith(TEXT("M_HullLight")) || Slot.StartsWith(TEXT("M_HullAmber"))))
			{
				Scanned = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Materials/MI_SB_AlienViolet.MI_SB_AlienViolet"));
			}
			// The alien kit's hull: iridescent violet-teal gloss (Tools/create_keel_alien_material.py).
			if (!Scanned && Slot.StartsWith(TEXT("M_AlienDark")))
			{
				Scanned = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelMauveDark.MI_SB_KeelMauveDark"));
			}
			// Rose accents on the alien panels: the mauve hull, tinted warmer.
			if (!Scanned && Slot.StartsWith(TEXT("M_AlienRose")) && FParse::Param(FCommandLine::Get(), TEXT("SBKeelRoseScan")))
			{
				if (UMaterialInterface* Rose = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelMauve.MI_SB_KeelMauve")))
				{
					Mesh->SetMaterial(Index, Rose);
					if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(Index))
					{
						MID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.15f, 0.055f, 0.10f));
					}
					continue;
				}
			}
			if (!Scanned && Slot.StartsWith(TEXT("M_AlienHull")))
			{
				Scanned = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelMauve.MI_SB_KeelMauve"));
			}
			if (!Scanned && Slot.StartsWith(TEXT("M_HullAmber")) && Asset->GetName().StartsWith(TEXT("SM_H_WallPlate")))
			{
				Scanned = FParse::Param(FCommandLine::Get(), TEXT("SBKeelHazardPlates"))
					? ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelHazard.MI_SB_KeelHazard"))
					: ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelPlate.MI_SB_KeelPlate"));
			}
			if (Scanned)
			{
				Mesh->SetMaterial(Index, Scanned);
				continue;
			}
		}
		if (KitAmber >= 0.f && Slot.StartsWith(TEXT("M_HullAmber")) && GlowMaterial)
		{
			Mesh->SetMaterial(Index, GlowMaterial);
			if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(Index))
			{
				const FLinearColor Amber(1.0f, 0.5f, 0.12f);
				MID->SetVectorParameterValue(TEXT("BaseColor"), Amber * 0.2f);
				MID->SetVectorParameterValue(TEXT("EmissiveColor"), Amber);
				MID->SetScalarParameterValue(TEXT("EmissiveStrength"), KitAmber);
				MID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
				MID->SetScalarParameterValue(TEXT("Roughness"), 0.4f);
			}
			continue;
		}
		if (Look.KitGlow.A > 0.f && Slot.StartsWith(TEXT("M_Glow")) && GlowMaterial)
		{
			// This map's glow colour instead of the kit's modelled cyan or amber.
			Mesh->SetMaterial(Index, GlowMaterial);
			if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(Index))
			{
				MID->SetVectorParameterValue(TEXT("BaseColor"), Look.KitGlow * 0.2f);
				MID->SetVectorParameterValue(TEXT("EmissiveColor"), Look.KitGlow);
				MID->SetScalarParameterValue(TEXT("EmissiveStrength"), Look.KitGlowStrength);
				MID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
				MID->SetScalarParameterValue(TEXT("Roughness"), 0.4f);
			}
			continue;
		}
		for (const FSlotLook& Entry : SlotLooks)
		{
			if (!Slot.StartsWith(Entry.Prefix))
			{
				continue;
			}
			const TObjectPtr<UMaterialInterface>* World = SurfaceMaterials.Find(FName(Entry.Kind));
			if (World && *World)
			{
				UMaterialInterface* Surface = *World;
				const bool bSpireSurface = MapId == FName(TEXT("Spire"))
					&& !FParse::Param(FCommandLine::Get(), TEXT("SBLegacySpirePanels"));
				const bool bAlloy = Slot.StartsWith(TEXT("M_Steel")) || Slot.StartsWith(TEXT("M_Ivory"));
				const bool bInset = Slot.StartsWith(TEXT("M_Gunmetal"));
				if (bSpireSurface && (bAlloy || bInset))
				{
					const TCHAR* Kind = bInset ? TEXT("Inset") : Asset->GetName().StartsWith(TEXT("SM_F_Floor")) ? TEXT("Deck") : TEXT("Alloy");
					if (UMaterialInterface* SpireMaterial = ShieldbreakArt::Load<UMaterialInterface>(*FString::Printf(
						TEXT("/Game/Shieldbreak/Art/Spire/MI_SB_Spire%s.MI_SB_Spire%s"), Kind, Kind)))
					{
						Surface = SpireMaterial;
					}
				}
				Mesh->SetMaterial(Index, Surface);
				if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(Index))
				{
					MID->SetVectorParameterValue(TEXT("Tint"), bSpireSurface && Slot.StartsWith(TEXT("M_Ivory")) ? Look.Metal : Entry.Tint);
					MID->SetScalarParameterValue(TEXT("SelfLight"), SelfLight);
				}
			}
			break;
		}
	}
}

void AShieldbreakArena::BuildMapArt()
{
	// -SBNoMapArt drops the set pieces, for before/after shots and performance comparisons.
	// Enclosed maps have no outside to dress; ShieldbreakMapArt::Plan gives them only their hero piece.
	if (GetNetMode() == NM_DedicatedServer || FParse::Param(FCommandLine::Get(), TEXT("SBNoMapArt")))
	{
		return;
	}
	const FShieldbreakMapLook& Look = ShieldbreakMapArt::GetLook(MapId);
	// With the scanned terrain (BuildTerrainScan) the scanned rock replaces the faceted rocks and the procedural rock
	// skin, and the dressed-stone tiles take the scanned stone.
	const bool bTerrainScan = ShieldbreakMapArt::TerrainScanReady(MapId);
	UMaterialInterface* ScanStone = bTerrainScan ? ShieldbreakArt::Load<UMaterialInterface>(*FString::Printf(
		TEXT("/Game/Shieldbreak/Art/Scan/MI_SB_Scan_%sStone.MI_SB_Scan_%sStone"), *Look.ScanSet.ToString(), *Look.ScanSet.ToString())) : nullptr;
	int32 Kinds = 0, Placed = 0;
	for (const FShieldbreakArtGroup& Group : ShieldbreakMapArt::Plan(MapId, Layout))
	{
		if (bTerrainScan && Group.Piece.ToString().StartsWith(TEXT("Rock")))
		{
			continue;
		}
		const FString Name = TEXT("SM_SB_Kit_") + Group.Piece.ToString();
		UStaticMesh* Mesh = Group.Instances.Num() > 0 ? ShieldbreakArt::Load<UStaticMesh>(*FString::Printf(TEXT("/Game/Shieldbreak/Art/MapKit/%s.%s"), *Name, *Name)) : nullptr;
		if (!Mesh && Group.Piece == FName(TEXT("CondenserV2")))
		{
			UE_LOG(LogShieldbreak, Warning, TEXT("Condenser V2 asset missing; retaining the original Spire condenser."));
			Mesh = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Game/Shieldbreak/Art/MapKit/SM_SB_Kit_Condenser.SM_SB_Kit_Condenser"));
		}
		if (!Mesh)
		{
			continue;
		}
		// One instanced component per piece: a draw call or two per kind, however many stand around the map.
		auto* Instances = AddDecoration<UInstancedStaticMeshComponent>(TEXT("MapArt"), Mesh);
		Instances->SetCastShadow(Group.Piece == FName(TEXT("CondenserV2")));
		// Without Lumen, distance-field sky occlusion left the kit near black up close (sunlit tops included), so it
		// stays out of the distance fields. Lumen needs them to light it, so with Lumen it stays in.
		Instances->bAffectDistanceFieldLighting = ShieldbreakArenaPrivate::UsesLumen();
		// Pieces laid a few centimetres proud of a block (reliefs, edge bands, the floor inlay) sit inside the block's
		// distance field, so the sky light's occlusion blacks them out; a little self light keeps them reading.
		const bool bFlush = Group.Piece == TEXT("Relief") || Group.Piece == TEXT("Fascia") || Group.Piece == TEXT("Inlay");
		// The rock skin takes its block's colour, so a sand bank and a red-rock mesa stay apart.
		FShieldbreakMapLook GroupLook = Look;
		if (Group.Piece == FName(TEXT("CondenserV2")))
		{
			// The hero's machined alloy is darker than the architectural ceramic around it. Its practical
			// lights reveal the chamfers instead of bleaching the entire assembly into one white shape.
			GroupLook.Metal = FLinearColor(0.30f, 0.35f, 0.40f);
			GroupLook.DarkMetal = FLinearColor(0.06f, 0.08f, 0.10f);
		}
		if (Group.SurfaceTint.A > 0.f)
		{
			GroupLook.Rock = Group.SurfaceTint;
			GroupLook.Carved = Group.SurfaceTint;
		}
		ApplyKitSurfaces(Instances, GroupLook, bFlush ? 0.35f : 0.f);
		if (ScanStone && Group.Piece.ToString().StartsWith(TEXT("StoneFace")))
		{
			for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				Instances->SetMaterial(Slot, ScanStone);
			}
		}
		if (Group.TeamTint.A > 0.f)
		{
			const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
			for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
			{
				UMaterialInstanceDynamic* MID = Slots[Slot].MaterialSlotName.ToString().StartsWith(TEXT("M_TeamGlow"))
					? Instances->CreateDynamicMaterialInstance(Slot) : nullptr;
				if (MID)
				{
					MID->SetVectorParameterValue(TEXT("BaseColor"), Group.TeamTint * 0.2f);
					MID->SetVectorParameterValue(TEXT("EmissiveColor"), Group.TeamTint);
				}
			}
		}
		for (const FTransform& Instance : Group.Instances)
		{
			Instances->AddInstance(Instance);
		}
		Instances->RegisterComponent();
		AddInstanceComponent(Instances);
		++Kinds;
		Placed += Group.Instances.Num();
	}
	// Keel's landmark paint (Tools/create_keel_decals.py): a big stencilled bridge number over each team's window, read
	// from the Cradle ("01" red at -X, "02" blue at +X), and hazard chevrons along the trench walls under the approach
	// decks. Decals only, no geometry. -SBNoKeelDecals leaves them off.
	if (MapId == FName(TEXT("Keel")) && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelDecals")))
	{
		struct FKeelDecal { const TCHAR* Material; FVector At; float Yaw; FVector HalfSize; };
		const FKeelDecal Decals[] = {
			{ TEXT("Stencil01"), FVector(-3870.0, 0.0, 1160.0), 180.f, FVector(45.0, 320.0, 320.0) },
			{ TEXT("Stencil02"), FVector(3870.0, 0.0, 1160.0), 0.f, FVector(45.0, 320.0, 320.0) },
			{ TEXT("Chevrons"), FVector(-1300.0, 0.0, 175.0), 0.f, FVector(45.0, 112.0, 900.0) },
			{ TEXT("Chevrons"), FVector(1300.0, 0.0, 175.0), 180.f, FVector(45.0, 112.0, 900.0) },
		};
		int32 DecalCount = 0;
		for (const FKeelDecal& Decal : Decals)
		{
			if (FCString::Strcmp(Decal.Material, TEXT("Chevrons")) == 0 && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelAlienSlice")))
			{
				continue;   // hazard chevrons are human paint; the alien look leaves them off
			}
			UMaterialInterface* Paint = ShieldbreakArt::Load<UMaterialInterface>(*FString::Printf(
				TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_Keel%s.MI_SB_Keel%s"), Decal.Material, Decal.Material));
			if (!Paint)
			{
				continue;
			}
			UDecalComponent* Component = NewObject<UDecalComponent>(this);
			Component->SetupAttachment(Root);
			Component->SetDecalMaterial(Paint);
			Component->DecalSize = Decal.HalfSize;
			// The decal projects along its X axis into the wall; rolled a quarter turn so the texture stands upright.
			Component->SetRelativeLocationAndRotation(Decal.At + FVector(0.0, 0.0, Layout.FloorZ), FRotator(0.f, Decal.Yaw, -90.f));
			Component->RegisterComponent();
			AddInstanceComponent(Component);
			++DecalCount;
		}
		// Vent fans on the Galleries hull (under the pipework, over the upper walkway): a housing and a rotor that turns
		// (Art/blender/build_keel_hero.py). Six rotors, each with its own rotating movement; -SBNoKeelFans leaves them off.
		UStaticMesh* FanHousing = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Game/Shieldbreak/Art/MapKit/SM_SB_Kit_FanHousing.SM_SB_Kit_FanHousing"));
		UStaticMesh* FanBlades = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Game/Shieldbreak/Art/MapKit/SM_SB_Kit_FanBlades.SM_SB_Kit_FanBlades"));
		if (FanHousing && FanBlades && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelFans")))
		{
			const FShieldbreakMapLook& KeelLook = ShieldbreakMapArt::GetLook(MapId);
			auto* Housings = AddDecoration<UInstancedStaticMeshComponent>(TEXT("KeelFanHousing"), FanHousing);
			Housings->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Housings->SetCastShadow(false);
			ApplyKitSurfaces(Housings, KeelLook, 0.f);
			int32 Fan = 0;
			for (const double X : { -3400.0, -2600.0, -1000.0, 1000.0, 2600.0, 3400.0 })
			{
				// South hull, facing +Y into the Galleries.
				const FTransform At(FRotator(0.f, 180.f, 0.f), FVector(X, -Layout.HalfExtent.Y + 25.0, Layout.FloorZ + 850.0));
				Housings->AddInstance(At);
				UStaticMeshComponent* Rotor = AddDecoration<UStaticMeshComponent>(TEXT("KeelFanRotor"), FanBlades);
				Rotor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Rotor->SetCastShadow(false);
				Rotor->SetMobility(EComponentMobility::Movable);
				Rotor->SetRelativeTransform(At);
				Rotor->RegisterComponent();
				AddInstanceComponent(Rotor);
				ApplyKitSurfaces(Rotor, KeelLook, 0.f);
				URotatingMovementComponent* Spin = NewObject<URotatingMovementComponent>(this);
				Spin->RotationRate = FRotator(Fan % 2 ? 150.f : 190.f, 0.f, 0.f);   // pitch: about the rotor's own Y, the wall normal
				Spin->bRotationInLocalSpace = true;
				Spin->SetUpdatedComponent(Rotor);
				Spin->RegisterComponent();
				AddInstanceComponent(Spin);
				++Fan;
			}
			Housings->RegisterComponent();
			AddInstanceComponent(Housings);
		}
		// Zone light: cold starlight spilling in from the Breach window and a cyan glow over the reactor ring, so the two
		// flanks read differently at a glance (unshadowed points). -SBNoKeelZoneLights for comparisons.
		if (!FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelZoneLights")))
		{
			auto AddZoneLight = [&](const FVector& At, const FLinearColor& Colour, float Candelas, float Radius)
			{
				UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
				Light->SetupAttachment(Root);
				Light->SetMobility(EComponentMobility::Movable);
				Light->SetRelativeLocation(At + FVector(0.0, 0.0, Layout.FloorZ));
				Light->SetIntensityUnits(ELightUnits::Candelas);
				Light->SetIntensity(Candelas);
				Light->SetAttenuationRadius(Radius);
				Light->SetSourceRadius(200.f);
				Light->SetLightColor(Colour);
				Light->SetCastShadows(false);
				Light->RegisterComponent();
			};
			const FLinearColor Starlight(0.55f, 0.68f, 1.f), ReactorGlow(0.45f, 0.95f, 1.f);
			for (const double X : { -2400.0, 0.0, 2400.0 })
			{
				AddZoneLight(FVector(X, Layout.HalfExtent.Y - 300.0, 1050.0), Starlight, 70.f, 1900.f);
			}
			AddZoneLight(FVector(0.0, -2800.0, 900.0), ReactorGlow, 170.f, 1700.f);
			// Spawn rooms: a faint team hint in each bridge (warm on red, cool on blue). The trench: low amber work lamps
			// on its walls, against the cold under-glow of the pod above.
			// The alien look trades the amber for rose and violet.
			const bool bAlienLights = !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelAlienSlice"));
			AddZoneLight(FVector(-4500.0, 0.0, 1100.0), bAlienLights ? FLinearColor(1.f, 0.55f, 0.78f) : FLinearColor(1.f, 0.72f, 0.55f), 90.f, 1600.f);
			AddZoneLight(FVector(4500.0, 0.0, 1100.0), FLinearColor(0.62f, 0.75f, 1.f), 90.f, 1600.f);
			for (const double X : { -1150.0, 1150.0 })
			{
				for (const double Y : { -700.0, 700.0 })
				{
					AddZoneLight(FVector(X, Y, 420.0), bAlienLights ? FLinearColor(0.8f, 0.4f, 1.f) : FLinearColor(1.f, 0.62f, 0.28f), 22.f, 1100.f);
				}
			}
		}
		// Fab Mechanic Base dressing (Epic content on Vincent's personal licence, local only and git-ignored; see
		// docs/KEEL_AAA_PASS.md): conduit trays under the ceiling and strip lights high on the Cradle's band walls,
		// cable bundles hanging in its corners, and junction boxes and panels on the approach-deck walls. All of it is
		// thin or above any jump, so it is visual only. Missing pack (or -SBNoKeelPacks): nothing is added.
		if (!FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelPacks")))
		{
			int32 PackPieces = 0;
			auto Pack = [&](const TCHAR* Path, const TArray<FTransform>& Where, float CullDistance)
			{
				const FString Name = FPaths::GetBaseFilename(Path);
				UStaticMesh* Mesh = ShieldbreakArt::Load<UStaticMesh>(*FString::Printf(TEXT("/Game/Modular_Scifi_Mechanic_Base/Mesh/SM/%s.%s"), Path, *Name));
				if (!Mesh)
				{
					return;
				}
				auto* Instances = AddDecoration<UInstancedStaticMeshComponent>(TEXT("KeelPack"), Mesh);
				Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Instances->SetCastShadow(false);
				Instances->SetCullDistances(0, CullDistance);
				// The alien look: the pack's grey steel goes dark mauve (door frames, trays, boxes, panels); lamps and their
				// glowing slots keep the pack's materials.
				if (!FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelAlienSlice")) && !Name.Contains(TEXT("LAMP")))
				{
					if (UMaterialInterface* Dark = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelMauveDark.MI_SB_KeelMauveDark")))
					{
						for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
						{
							const UMaterialInterface* Own = Mesh->GetMaterial(Slot);
							const FString Kind = Own ? Own->GetName() : FString();
							if (!Kind.Contains(TEXT("Emis")) && !Kind.Contains(TEXT("Light")) && !Kind.Contains(TEXT("Glow")) && !Kind.Contains(TEXT("Screen")))
							{
								Instances->SetMaterial(Slot, Dark);
							}
						}
					}
				}
				for (const FTransform& At : Where)
				{
					Instances->AddInstance(FTransform(At.GetRotation(), At.GetLocation() + FVector(0.0, 0.0, Layout.FloorZ), At.GetScale3D()));
				}
				Instances->RegisterComponent();
				AddInstanceComponent(Instances);
				PackPieces += Where.Num();
			};
			// Facing: +1 for the wall at y = +1970 (its face looks toward -Y), -1 for the wall at y = -1970.
			auto OnBandWall = [](double X, double Side, double Inset, double Z)
			{
				return FTransform(FRotator(0.f, Side > 0.0 ? 0.f : 180.f, 0.f), FVector(X, Side * (1970.0 - Inset), Z));
			};
			TArray<FTransform> Trays, Strips, Cables, Boxes, Panels;
			for (const double Side : { -1.0, 1.0 })
			{
				for (double X = -3700.0; X <= 3700.0 + 1.0; X += 400.0)
				{
					Trays.Add(OnBandWall(X, Side, 36.0, 1500.0));
				}
				for (const double X : { -3000.0, -1000.0, 1000.0, 3000.0 })
				{
					Strips.Add(OnBandWall(X, Side, 12.0, 1350.0));
				}
				for (const double X : { -3550.0, 3550.0 })
				{
					Cables.Add(FTransform(FRotator(0.f, 90.f, 0.f), FVector(X, Side * 1880.0, 1500.0 - 259.0)));
				}
				for (const double X : { -3500.0, -2000.0, 2000.0, 3500.0 })
				{
					Boxes.Add(OnBandWall(X, Side, 2.0, 490.0));
					Panels.Add(OnBandWall(X + (X < 0.0 ? 160.0 : -160.0), Side, 1.5, 520.0));
				}
			}
			// Ceiling: the pack's 7 x 7 m coffered tiles (55 cm deep, hung from their top face) over the whole ship.
			TArray<FTransform> Tiles;
			for (double X = -4900.0; X <= 4900.0 + 1.0; X += 700.0)
			{
				for (double Y = -3500.0; Y <= 3500.0 + 1.0; Y += 700.0)
				{
					if (!FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelAlienSlice")) && (!FParse::Param(FCommandLine::Get(), TEXT("SBKeelAlienBridgesOnly")) || (FMath::Abs(X) > 3860.0 && FMath::Abs(Y) < 2000.0)))
					{
						continue;   // the alien slice's bridges get their own mauve ceiling (BuildKeelPackKit)
					}
					Tiles.Add(FTransform(FRotator::ZeroRotator, FVector(X, Y, (Ceiling ? (Ceiling->Bounds.GetBox().Min.Z - GetActorLocation().Z) : 1500.0) - Layout.FloorZ)));
				}
			}
			Pack(TEXT("SRTUCTURE/CEILING/SM_CEILING_05"), Tiles, 0.f);
			// Door frames round the 5 x 3.5 m deck doors (bottom at the 3.5 m decks): the band-wall doors to the docks and
			// flank ramps, and the bridges' window-wall doors. The pack frame is 4 m wide; widened to clear the openings.
			TArray<FTransform> DoorFrames;
			const FVector FrameScale(1.55, 1.0, 1.0);
			for (const double Side : { -1.0, 1.0 })
			{
				for (const double X : { -4450.0, -2550.0, 2550.0, 4450.0 })
				{
					DoorFrames.Add(FTransform(FRotator::ZeroRotator, FVector(X, Side * 2000.0, 350.0), FrameScale));
				}
				for (const double X : { -3900.0, 3900.0 })
				{
					DoorFrames.Add(FTransform(FRotator(0.f, 90.f, 0.f), FVector(X, Side * 1550.0, 350.0), FrameScale));
				}
			}
			Pack(TEXT("SRTUCTURE/DOOR_FRAME/SM_DOOR_FRAME_01_OUTSIDE"), DoorFrames, 0.f);
			Pack(TEXT("PROP/BRACKET/SM_WALL_INSTALLATION_02"), Trays, 9000.f);
			Pack(TEXT("PROP/LAMP/SM_LAMP_04"), Strips, 9000.f);
			Pack(TEXT("CABLE_PIPE/SM_CABLE_MASS_01"), Cables, 7000.f);
			Pack(TEXT("PROP/MACHINE/SM_ELECTRIC_BOX_01_CLOSE"), Boxes, 4000.f);
			Pack(TEXT("PROP/OTHERS/SM_PANEL_01"), Panels, 4000.f);
			UE_LOG(LogShieldbreak, Log, TEXT("Keel Fab pack dressing: %d pieces."), PackPieces);
		}
		// Views out (Tools/create_keel_space.py: the space panorama, unlit, parallax), 30 cm proud of the hull in front
		// of its cladding and split by heavy frame mullions; visual only. The Breach's long window over its catwalk (north
		// hull, x +-3200, z 760..1380) and one over each bridge's overwatch (end walls, y +-1600, z 760..1340).
		UMaterialInterface* Space = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/M_SB_KeelSpace.M_SB_KeelSpace"));
		UMaterialInterface* Frame = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelFrame.MI_SB_KeelFrame"));
		if (Space && CubeMesh && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelWindow")))
		{
			auto* Mullions = AddDecoration<UInstancedStaticMeshComponent>(TEXT("KeelWindowFrame"), CubeMesh);
			Mullions->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (Frame)
			{
				Mullions->SetMaterial(0, Frame);
			}
			// Center on the hull face, Yaw turns the window's length from X (0) to Y (+-90); the panel's -Y face, which
			// shows the panorama, then faces the room.
			auto AddWindow = [&](const FVector& Center, float Yaw, double HalfLength, double Bottom, double Top)
			{
				const FRotator Turn(0.f, Yaw, 0.f);
				const FVector Middle = Center + FVector(0.0, 0.0, Layout.FloorZ + (Bottom + Top) * 0.5);
				const FVector Inward = Turn.RotateVector(FVector(0.0, -1.0, 0.0));
				UStaticMeshComponent* Window = AddDecoration<UStaticMeshComponent>(TEXT("KeelWindow"), CubeMesh);
				Window->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Window->SetCastShadow(false);
				Window->SetRelativeLocationAndRotation(Middle, Turn);
				Window->SetRelativeScale3D(FVector(HalfLength / 50.0, 0.02, (Top - Bottom) / 100.0));
				Window->SetMaterial(0, Space);
				Window->RegisterComponent();
				AddInstanceComponent(Window);
				for (double Along = -HalfLength; Along <= HalfLength + 1.0; Along += 800.0)
				{
					Mullions->AddInstance(FTransform(Turn, Middle + Turn.RotateVector(FVector(Along, 0.0, 0.0)) + Inward * 15.0,
						FVector(0.5, 0.3, (Top - Bottom + 80.0) / 100.0)));
				}
				for (const double Z : { Bottom, Top })
				{
					Mullions->AddInstance(FTransform(Turn, Center + FVector(0.0, 0.0, Layout.FloorZ + Z) + Inward * 15.0,
						FVector(HalfLength / 50.0 + 0.5, 0.3, 0.4)));
				}
			};
			AddWindow(FVector(0.0, Layout.HalfExtent.Y - 30.0, 0.0), 0.f, 3200.0, 760.0, 1380.0);
			AddWindow(FVector(-Layout.HalfExtent.X + 30.0, 0.0, 0.0), 90.f, 1600.0, 760.0, 1340.0);
			AddWindow(FVector(Layout.HalfExtent.X - 30.0, 0.0, 0.0), -90.f, 1600.0, 760.0, 1340.0);
			Mullions->RegisterComponent();
			AddInstanceComponent(Mullions);
		}
		UE_LOG(LogShieldbreak, Log, TEXT("Keel landmark decals: %d."), DecalCount);
	}
	UE_LOG(LogShieldbreak, Log, TEXT("%s map art: %d kit pieces, %d instances (visual only)."), *Layout.DisplayName, Kinds, Placed);
}

bool AShieldbreakArena::BuildKeelPackKit()
{
	// Every exposed wall face of every block gets the pack's 4 m wall modules at full height and width (stretched a
	// little to fill the span exactly), their detailed +Y face 1 cm proud of the block face and their depth set into
	// the block (at most half its thickness, so a thin wall's two faces never poke through each other). Every
	// walkable top gets the pack's 3 m floor tiles, flush with the collision surface (sunk by their thickness), and
	// every ramp gets floor tiles laid along its slope (tile tops a few millimetres over the collision surface, so they show). Collision stays the blocks and ramps: visual only.
	auto Load = [](const TCHAR* Path) -> UStaticMesh*
	{
		const FString Name = FPaths::GetBaseFilename(Path);
		return ShieldbreakArt::Load<UStaticMesh>(*FString::Printf(TEXT("/Game/Modular_Scifi_Mechanic_Base/Mesh/SM/%s.%s"), Path, *Name));
	};
	const TCHAR* WallPaths[] = { TEXT("SRTUCTURE/WALL/SM_WALL_12"), TEXT("SRTUCTURE/WALL/SM_WALL_07"),
		TEXT("SRTUCTURE/WALL/SM_WALL_09"), TEXT("SRTUCTURE/WALL/SM_WALL_13") };
	const TCHAR* FloorPaths[] = { TEXT("SRTUCTURE/FLOOR/SM_FLOOR_08"), TEXT("SRTUCTURE/FLOOR/SM_FLOOR_07"),
		TEXT("SRTUCTURE/FLOOR/SM_FLOOR_05") };
	TArray<UStaticMesh*> Walls, Floors;
	for (const TCHAR* Path : WallPaths) { if (UStaticMesh* Mesh = Load(Path)) Walls.Add(Mesh); }
	// Floors (2026-11-16, Vincent: "make the floor completely different"): the Modular SciFi Season 1 interior deck
	// panels (Content/StarterBundle, local only) with their own dark plating and inlays, so the floor no longer echoes
	// the Mechanic Base walls. Falls back to the Mechanic Base tiles without that pack (or -SBKeelMechFloors).
	const bool bSeasonFloors = !FParse::Param(FCommandLine::Get(), TEXT("SBKeelMechFloors"));
	const TCHAR* SeasonFloors[] = { TEXT("ModularSci_Int/Meshes/SM_FloorPanel_D"), TEXT("ModularSci_Int/Meshes/SM_FloorPanel_A_2"),
		TEXT("ModularScifiProps/Meshes/SM_Floor_B") };
	for (const TCHAR* Path : SeasonFloors)
	{
		const FString Name = FPaths::GetBaseFilename(Path);
		UStaticMesh* Mesh = bSeasonFloors ? ShieldbreakArt::Load<UStaticMesh>(*FString::Printf(TEXT("/Game/StarterBundle/%s.%s"), Path, *Name)) : nullptr;
		if (Mesh) Floors.Add(Mesh);
	}
	const bool bUsingSeasonFloors = Floors.Num() > 0;
	if (!bUsingSeasonFloors)
	{
		for (const TCHAR* Path : FloorPaths) { if (UStaticMesh* Mesh = Load(Path)) Floors.Add(Mesh); }
	}
	if (Walls.Num() == 0 || Floors.Num() == 0)
	{
		return false;
	}
	TArray<UInstancedStaticMeshComponent*> WallSets, FloorSets;
	auto MakeSet = [this](UStaticMesh* Mesh)
	{
		auto* Set = AddDecoration<UInstancedStaticMeshComponent>(TEXT("KeelPackKit"), Mesh);
		Set->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Set->SetCastShadow(false);
		Set->bAffectDistanceFieldLighting = false;
		return Set;
	};
	for (UStaticMesh* Mesh : Walls) WallSets.Add(MakeSet(Mesh));
	for (UStaticMesh* Mesh : Floors) FloorSets.Add(MakeSet(Mesh));
	// Alien vertical slice (2026-11-16, Vincent chose a true alien ship, reference: his Midship video): inside both
	// bridges the pack modules give way to smooth mottled mauve hull (Art/textures/make_keel_alien_surfaces.py) on
	// every face, dark mauve decks, and thin purple under-glow strips along the foot of each wall. -SBNoKeelAlienSlice
	// keeps the pack there.
	const bool bAlienSlice = !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelAlienSlice")) && CubeMesh;
	// 2026-11-17: the look now covers the whole ship (-SBKeelAlienBridgesOnly keeps it to the two bridges).
	const bool bBridgesOnly = FParse::Param(FCommandLine::Get(), TEXT("SBKeelAlienBridgesOnly"));
	auto InSlice = [bAlienSlice, bBridgesOnly](const FVector& P) { return bAlienSlice && (!bBridgesOnly || (FMath::Abs(P.X) > 3860.0 && FMath::Abs(P.Y) < 2000.0)); };
	UInstancedStaticMeshComponent* SkinWalls = nullptr;
	UInstancedStaticMeshComponent* SkinDecks = nullptr;
	UInstancedStaticMeshComponent* Underglow = nullptr;
	UInstancedStaticMeshComponent* Panels = nullptr;
	UInstancedStaticMeshComponent* PanelsB = nullptr;
	UInstancedStaticMeshComponent* Decks = nullptr;
	UInstancedStaticMeshComponent* Coves = nullptr;
	UInstancedStaticMeshComponent* PanelsC = nullptr;
	UInstancedStaticMeshComponent* Glyphs = nullptr;
	if (bAlienSlice)
	{
		// The sculpted alien wall panel (Art/blender/build_keel_alien.py) on every wall cell, glowing glyph plaques
		// (Art/textures/make_keel_glyphs.py) on some of them.
		if (UStaticMesh* PanelMesh = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Game/Shieldbreak/Art/MapKit/SM_SB_Kit_AlienPanel.SM_SB_Kit_AlienPanel")))
		{
			Panels = MakeSet(PanelMesh);
			ApplyKitSurfaces(Panels, ShieldbreakMapArt::GetLook(MapId), 0.f);
		}
		// The sculpted deck tile (rounded plates, dim violet seams) on the walkable tops; -SBKeelFlatDecks keeps the skin.
		if (!FParse::Param(FCommandLine::Get(), TEXT("SBKeelFlatDecks")))
		{
			if (UStaticMesh* DeckMesh = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Game/Shieldbreak/Art/MapKit/SM_SB_Kit_AlienDeck.SM_SB_Kit_AlienDeck")))
			{
				Decks = MakeSet(DeckMesh);
				ApplyKitSurfaces(Decks, ShieldbreakMapArt::GetLook(MapId), 0.f);
			}
		}
		// Curved coves rounding every wall top into the ceiling: opt-in (-SBKeelCoves); under the flat ceiling light they
		// barely read (sl20 shots), so they are not worth the instances by default.
		if (FParse::Param(FCommandLine::Get(), TEXT("SBKeelCoves")))
		{
			if (UStaticMesh* CoveMesh = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Game/Shieldbreak/Art/MapKit/SM_SB_Kit_AlienWallCove.SM_SB_Kit_AlienWallCove")))
			{
				Coves = MakeSet(CoveMesh);
				ApplyKitSurfaces(Coves, ShieldbreakMapArt::GetLook(MapId), 0.f);
			}
		}
		// Two variants break up the repeat (-SBKeelOnePanel keeps one): stacked plates with vents, and a rose inset.
		if (!FParse::Param(FCommandLine::Get(), TEXT("SBKeelOnePanel")))
		{
			if (UStaticMesh* B = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Game/Shieldbreak/Art/MapKit/SM_SB_Kit_AlienPanelB.SM_SB_Kit_AlienPanelB")))
			{
				PanelsB = MakeSet(B);
				ApplyKitSurfaces(PanelsB, ShieldbreakMapArt::GetLook(MapId), 0.f);
			}
			if (UStaticMesh* C = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Game/Shieldbreak/Art/MapKit/SM_SB_Kit_AlienPanelC.SM_SB_Kit_AlienPanelC")))
			{
				PanelsC = MakeSet(C);
				ApplyKitSurfaces(PanelsC, ShieldbreakMapArt::GetLook(MapId), 0.f);
			}
		}
		if (UMaterialInterface* GlyphMaterial = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/M_SB_KeelGlyph.M_SB_KeelGlyph")))
		{
			Glyphs = MakeSet(CubeMesh);
			Glyphs->SetMaterial(0, GlyphMaterial);
		}
		SkinWalls = MakeSet(CubeMesh);
		SkinDecks = MakeSet(CubeMesh);
		Underglow = MakeSet(CubeMesh);
		if (UMaterialInterface* Mauve = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelMauve.MI_SB_KeelMauve")))
		{
			SkinWalls->SetMaterial(0, Mauve);
		}
		if (UMaterialInterface* Dark = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelMauveDark.MI_SB_KeelMauveDark")))
		{
			SkinDecks->SetMaterial(0, Dark);
		}
		if (GlowMaterial)
		{
			Underglow->SetMaterial(0, GlowMaterial);
			if (UMaterialInstanceDynamic* MID = Underglow->CreateDynamicMaterialInstance(0))
			{
				const FLinearColor Purple(0.62f, 0.22f, 1.0f);
				MID->SetVectorParameterValue(TEXT("BaseColor"), Purple * 0.2f);
				MID->SetVectorParameterValue(TEXT("EmissiveColor"), Purple);
				MID->SetScalarParameterValue(TEXT("EmissiveStrength"), 6.f);
			}
		}
	}

	// Blocks the hero set pieces already dress (the Cradle pod, the six reactor pylons) stay as they are.
	auto Dressed = [](const FShieldbreakMapBlock& Block)
	{
		const FVector Size = Block.Max - Block.Min, Center = (Block.Min + Block.Max) * 0.5f;
		const bool bPod = FMath::IsNearlyEqual(Size.X, 1400.f, 5.f) && FMath::IsNearlyEqual(Size.Y, 800.f, 5.f) && FMath::Abs(Center.X) < 5.f;
		const bool bPylon = FMath::IsNearlyEqual(Size.X, 200.f, 5.f) && FMath::IsNearlyEqual(Size.Y, 200.f, 5.f)
			&& FMath::IsNearlyEqual(Size.Z, 600.f, 5.f) && Center.Y < -2000.f && FMath::Abs(Center.X) < 700.f;
		return bPod || bPylon;
	};
	const double CeilingZ = Ceiling ? Ceiling->Bounds.GetBox().Min.Z - GetActorLocation().Z : Layout.FloorZ + 1500.0;
	auto Inside = [this](const FVector& Point)
	{
		return Layout.Blocks.ContainsByPredicate([&Point](const FShieldbreakMapBlock& Block)
		{
			return Point.X > Block.Min.X && Point.X < Block.Max.X && Point.Y > Block.Min.Y && Point.Y < Block.Max.Y
				&& Point.Z > Block.Min.Z && Point.Z < Block.Max.Z;
		});
	};
	const FVector Half(Layout.HalfExtent.X, Layout.HalfExtent.Y, 0.0);
	int32 WallCount = 0, FloorCount = 0;
	uint32 Hash = 17;
	struct FFace { FVector Normal; double Plane; double A0, A1; double Thickness; bool bAlongX; };
	for (const FShieldbreakMapBlock& Block : Layout.Blocks)
	{
		if (Dressed(Block) || Block.Min.Z >= CeilingZ - 1.0 || Block.Style == FName(TEXT("glow")))
		{
			continue;
		}
		const FVector Size = Block.Max - Block.Min;
		const FFace Faces[] = {
			{ FVector(1, 0, 0), Block.Max.X, Block.Min.Y, Block.Max.Y, Size.X, false },
			{ FVector(-1, 0, 0), Block.Min.X, Block.Min.Y, Block.Max.Y, Size.X, false },
			{ FVector(0, 1, 0), Block.Max.Y, Block.Min.X, Block.Max.X, Size.Y, true },
			{ FVector(0, -1, 0), Block.Min.Y, Block.Min.X, Block.Max.X, Size.Y, true } };
		const double Z0 = Block.Min.Z, Z1 = FMath::Min<double>(Block.Max.Z, CeilingZ);
		if (Z1 - Z0 >= 20.0)
		{
			for (const FFace& Face : Faces)
			{
				const double Span = Face.A1 - Face.A0, Height = Z1 - Z0;
				const int32 Columns = FMath::Max(1, FMath::RoundToInt(Span / 400.0));
				const int32 Rows = FMath::Max(1, FMath::RoundToInt(Height / 400.0));
				const double Width = Span / Columns, RowHeight = Height / Rows;
				// The yaw that turns the module's +Y (detailed) face to the face normal.
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Face.Normal.Y, Face.Normal.X)) - 90.f;
				const FRotator Turn(0.f, Yaw, 0.f);
				for (int32 Column = 0; Column < Columns; ++Column)
				{
					const double Along = Face.A0 + (Column + 0.5) * Width;
					for (int32 Row = 0; Row < Rows; ++Row)
					{
						const double MidZ = Z0 + (Row + 0.5) * RowHeight;
						const FVector OnFace = Face.bAlongX ? FVector(Along, Face.Plane, MidZ) : FVector(Face.Plane, Along, MidZ);
						const FVector Probe = OnFace + Face.Normal * 15.0;
						if (Inside(Probe) || FMath::Abs(Probe.X) > Half.X || FMath::Abs(Probe.Y) > Half.Y)
						{
							continue;   // covered by a neighbour, or the outside of the hull
						}
						if (InSlice(OnFace))
						{
							// A smooth mauve skin over this cell (the cube is 100 cm), 1 cm proud of the face.
							const FRotator Skin(0.f, Yaw, 0.f);
							const FVector CellBase(OnFace.X, OnFace.Y, Z0 + Row * RowHeight);
							const bool bGlyphCell = Row == 0 && RowHeight > 250.0 && (Column % 3) == 1;
							// Light seams: a thin violet slit glowing in the gap between two panels, every fourth column, floor to
							// ceiling (Midship's vertical light lines). -SBNoKeelLightSeams turns them off.
							if (Underglow && Column % 4 == 3 && Column + 1 < Columns && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelLightSeams")))
							{
								const FVector AlongDir = Face.bAlongX ? FVector(1.0, 0.0, 0.0) : FVector(0.0, 1.0, 0.0);
								Underglow->AddInstance(FTransform(Skin, FVector(OnFace.X, OnFace.Y, Z0 + (Row + 0.5) * RowHeight)
									+ AlongDir * (Width * 0.5) + Face.Normal * 9.0, FVector(0.04, 0.02, RowHeight * 0.86 / 100.0)));
							}
							if (Coves && Row == Rows - 1 && Block.Max.Z >= CeilingZ - 5.0 && RowHeight > 200.0)
							{
								Coves->AddInstance(FTransform(Skin, FVector(OnFace.X, OnFace.Y, CeilingZ - 1.0) + Face.Normal * 1.0,
									FVector(Width / 400.0, 1.0, 1.0)));
							}
							const int32 Variant = bGlyphCell ? 0 : (Column * 7 + Row * 3 + FMath::Abs(FMath::FloorToInt32(Face.Plane / 100.0))) % 5;
							UInstancedStaticMeshComponent* PanelSet = Variant == 4 && PanelsC ? PanelsC : (Variant == 2 || Variant == 3) && PanelsB ? PanelsB : Panels;
							if (PanelSet)
							{
								// The variants are deeper (30 and 39 cm); squash them to panel A's 27 cm so none pokes through the
								// space windows set into the wall.
								const double Depth = PanelSet == PanelsC ? 27.0 / 39.0 : PanelSet == PanelsB ? 27.0 / 30.0 : 1.0;
								PanelSet->AddInstance(FTransform(Skin, CellBase + Face.Normal * 1.0, FVector(Width / 400.0, Depth, RowHeight / 400.0)));
							}
							else
							{
								SkinWalls->AddInstance(FTransform(Skin, OnFace + Face.Normal * 1.5, FVector(Width / 100.0, 0.02, RowHeight / 100.0)));
							}
							if (Glyphs && Row == 0 && RowHeight > 250.0 && (Column % 3) == 1)
							{
								Glyphs->AddInstance(FTransform(Skin, FVector(OnFace.X, OnFace.Y, Z0 + RowHeight * 0.55) + Face.Normal * 28.0,
									FVector(Width * 0.42 / 100.0, 0.02, RowHeight * 0.6 / 100.0)));
							}
							if (Row == 0)
							{
								// A leaning dark skirting slab along the foot of the wall (tilted 22 degrees out, 25 cm at the
								// floor), the angled lower walls of the reference; the glow strip runs along its top edge.
								const FVector Out = Face.Normal;
								const FRotator Lean = FRotator(0.f, Yaw, 0.f);
								const FQuat Tilt = FQuat(Lean.RotateVector(FVector(1.0, 0.0, 0.0)), FMath::DegreesToRadians(22.0));
								SkinDecks->AddInstance(FTransform(Tilt * Lean.Quaternion(), FVector(OnFace.X, OnFace.Y, Z0 + 60.0) + Out * 14.0,
									FVector(Width / 100.0, 0.06, 1.25)));
								Underglow->AddInstance(FTransform(Skin, FVector(OnFace.X, OnFace.Y, Z0 + 118.0) + Face.Normal * 4.0,
									FVector(Width / 100.0, 0.03, 0.03)));
							}
							continue;
						}
						Hash = Hash * 31u + static_cast<uint32>(Column * 7 + Row * 13 + FMath::Abs(static_cast<int32>(Along)));
						const int32 Pick = static_cast<int32>(Hash % static_cast<uint32>(Walls.Num()));
						const FBox Bounds = Walls[Pick]->GetBoundingBox();
						const double Front = Bounds.Max.Y - 10.0, Depth = Front - Bounds.Min.Y;
						const double DepthScale = FMath::Clamp(Face.Thickness * 0.5 / Depth, 0.1, 1.0);
						const FVector Scale(Width / Bounds.GetSize().X, DepthScale, RowHeight / Bounds.GetSize().Z);
						// Place the pivot so the module's front plane sits 1 cm proud of the face, centred on the column.
						const FVector OnBase = Face.bAlongX ? FVector(Along, Face.Plane, Z0 + Row * RowHeight) : FVector(Face.Plane, Along, Z0 + Row * RowHeight);
						const FVector Base = OnBase + Face.Normal * (1.0 - Front * DepthScale)
							- Turn.RotateVector(FVector(Bounds.GetCenter().X * Scale.X, 0.0, Bounds.Min.Z * Scale.Z));
						WallSets[Pick]->AddInstance(FTransform(Turn, Base, Scale));
						++WallCount;
					}
				}
			}
		}
		// The walkable top, tile by tile where nothing stands on it.
		if (Block.Max.Z < CeilingZ - 100.0)
		{
			const int32 CountX = FMath::Max(1, FMath::RoundToInt(Size.X / 300.0)), CountY = FMath::Max(1, FMath::RoundToInt(Size.Y / 300.0));
			const double StepX = Size.X / CountX, StepY = Size.Y / CountY;
			for (int32 IX = 0; IX < CountX; ++IX)
			{
				for (int32 IY = 0; IY < CountY; ++IY)
				{
					const FVector Top(Block.Min.X + (IX + 0.5) * StepX, Block.Min.Y + (IY + 0.5) * StepY, Block.Max.Z);
					if (Inside(Top + FVector(0.0, 0.0, 30.0)))
					{
						continue;
					}
					if (InSlice(Top))
					{
						if (Decks)
						{
							// The tile's base sits 2 cm under its pivot: lift it clear of the block top (plates end 3.6 cm up).
							Decks->AddInstance(FTransform(FRotator::ZeroRotator, Top + FVector(0.0, 0.0, 2.1), FVector(StepX / 300.0, StepY / 300.0, 1.0)));
						}
						else
						{
							SkinDecks->AddInstance(FTransform(FRotator::ZeroRotator, Top + FVector(0.0, 0.0, 0.5), FVector(StepX / 100.0, StepY / 100.0, 0.01)));
						}
						continue;
					}
					const int32 Pick = (IX * 3 + IY * 5) % Floors.Num();
					const FBox Bounds = Floors[Pick]->GetBoundingBox();
					FloorSets[Pick]->AddInstance(FTransform(FRotator::ZeroRotator, Top - FVector(0.0, 0.0, Bounds.Max.Z - 0.6),
						FVector(StepX / Bounds.GetSize().X, StepY / Bounds.GetSize().Y, 1.0)));
					++FloorCount;
				}
			}
		}
	}
	// Corner posts: the pack's convex corner piece, thinned to 30 cm, on every vertical block edge where both faces
	// show, so the wall modules meet at a capped corner instead of a raw seam.
	if (UStaticMesh* CornerMesh = Load(TEXT("SRTUCTURE/WALL/SM_WALL_03_CORNER_CONVEX")))
	{
		auto* Corners = MakeSet(CornerMesh);
		const FBox CornerBounds = CornerMesh->GetBoundingBox();
		int32 CornerCount = 0;
		for (const FShieldbreakMapBlock& Block : Layout.Blocks)
		{
			const double Z0 = Block.Min.Z, Z1 = FMath::Min<double>(Block.Max.Z, CeilingZ);
			if (Dressed(Block) || Z1 - Z0 < 150.0 || Block.Style == FName(TEXT("glow")) || InSlice((Block.Min + Block.Max) * 0.5f))
			{
				continue;
			}
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const double SX = (Corner & 1) ? 1.0 : -1.0, SY = (Corner & 2) ? 1.0 : -1.0;
				const FVector Edge(SX > 0 ? Block.Max.X : Block.Min.X, SY > 0 ? Block.Max.Y : Block.Min.Y, (Z0 + Z1) * 0.5);
				if (Inside(Edge + FVector(SX * 15.0, 0.0, 0.0)) || Inside(Edge + FVector(0.0, SY * 15.0, 0.0))
					|| Inside(Edge + FVector(SX * 15.0, SY * 15.0, 0.0)) || FMath::Abs(Edge.X) >= Half.X - 1.0 || FMath::Abs(Edge.Y) >= Half.Y - 1.0)
				{
					continue;
				}
				// The piece spans +X/+Y from its pivot: mirror it into this corner's quadrant with the scale signs.
				const FVector Scale(SX * 0.3, SY * 0.3, (Z1 - Z0) / CornerBounds.GetSize().Z);
				const FVector Pivot(Edge.X - SX * 18.0, Edge.Y - SY * 18.0, Z0);
				Corners->AddInstance(FTransform(FRotator::ZeroRotator, Pivot, Scale));
				++CornerCount;
			}
		}
		Corners->RegisterComponent();
		AddInstanceComponent(Corners);
		UE_LOG(LogShieldbreak, Log, TEXT("Keel pack grid: %d corner posts."), CornerCount);
	}
	UStaticMesh* StairMesh = FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelStairs")) ? nullptr : Load(TEXT("PROP/STAIRS/SM_STAIRS_01"));
	UInstancedStaticMeshComponent* Stairs = StairMesh ? MakeSet(StairMesh) : nullptr;
	if (Stairs && bAlienSlice)
	{
		// The step treads in the alien hull's dark mauve.
		if (UMaterialInterface* Dark = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelMauveDark.MI_SB_KeelMauveDark")))
		{
			for (int32 Slot = 0; Slot < Stairs->GetNumMaterials(); ++Slot) { Stairs->SetMaterial(Slot, Dark); }
		}
	}
	int32 StairCount = 0;
	// Ramps: floor tiles laid along the slope, flush with the walking surface.
	for (const FShieldbreakMapRamp& Ramp : Layout.Ramps)
	{
		const FVector Run = Ramp.End - Ramp.Start;
		const double Length = Run.Size();
		if (Length < 50.0)
		{
			continue;
		}
		// Sloped walkways (the "ramp" style, steeper than 8 degrees) become flights of the pack's step piece: one tread
		// every 15 cm of rise, each touching the collision slope at its back edge, so the walking surface stays the
		// smooth ramp (bots, sliding and movement unchanged) and the eye sees stairs. -SBNoKeelStairs keeps tiles.
		const double Rise = Ramp.End.Z - Ramp.Start.Z, Flat = FVector2D(Run.X, Run.Y).Size();
		if (StairMesh && Ramp.Style == FName(TEXT("ramp")) && Flat > 1.0 && FMath::Abs(Rise) / Flat > FMath::Tan(FMath::DegreesToRadians(8.0)))
		{
			const bool bUp = Rise > 0.0;
			const FVector Low = bUp ? Ramp.Start : Ramp.End, High = bUp ? Ramp.End : Ramp.Start;
			const FVector Climb = High - Low;
			const FVector2D Dir = FVector2D(Climb.X, Climb.Y).GetSafeNormal();
			const int32 Steps = FMath::Max(2, FMath::CeilToInt(FMath::Abs(Rise) / 15.0));
			const double Tread = Flat / Steps, Riser = FMath::Abs(Rise) / Steps;
			const FBox StepBounds = StairMesh->GetBoundingBox();
			const FRotator Across(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)) + 90.f, 0.f);
			const FVector Scale(Ramp.Width / StepBounds.GetSize().X, Tread / StepBounds.GetSize().Y, Riser / StepBounds.GetSize().Z);
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const double Mid = (Step + 0.5) * Tread;
				const double TopZ = Low.Z + (Step + 1) * Riser + 0.5;
				const FVector At(Low.X + Dir.X * Mid, Low.Y + Dir.Y * Mid, TopZ - StepBounds.Max.Z * Scale.Z);
				Stairs->AddInstance(FTransform(Across, At - Across.RotateVector(FVector(StepBounds.GetCenter().X * Scale.X,
					StepBounds.GetCenter().Y * Scale.Y, 0.0)), Scale));
				++StairCount;
			}
			continue;
		}
		const FRotator Turn = Run.Rotation();
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Length / 300.0));
		const int32 Across = FMath::Max(1, FMath::RoundToInt(Ramp.Width / 300.0));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			for (int32 Lane = 0; Lane < Across; ++Lane)
			{
				const int32 Pick = Index % Floors.Num();
				const FBox Bounds = Floors[Pick]->GetBoundingBox();
				const FVector Center = Ramp.Start + Run * ((Index + 0.5) / Count)
					+ Turn.RotateVector(FVector(0.0, (Lane + 0.5) * Ramp.Width / Across - Ramp.Width * 0.5, 0.0));
				if (Decks && InSlice(Center))
				{
					// The alien deck tile on ramp tops too (the pack's pale floor panels clashed on the crane arms).
					Decks->AddInstance(FTransform(Turn, Center + Turn.RotateVector(FVector(0.0, 0.0, 2.1)),
						FVector(Length / Count / 300.0, Ramp.Width / Across / 300.0, 1.0)));
					continue;
				}
				const FVector Down = Turn.RotateVector(FVector(0.0, 0.0, 0.8 - Bounds.Max.Z));
				FloorSets[Pick]->AddInstance(FTransform(Turn, Center + Down,
					FVector(Length / Count / Bounds.GetSize().X, Ramp.Width / Across / Bounds.GetSize().Y, 1.0)));
				++FloorCount;
			}
		}
	}
	// Wall colour (2026-11-16, Vincent: "wish the walls weren't a full gray"): each of the four wall modules takes its
	// own two-tone scheme on Keel's scanned hull materials: the main panels a deep alien tint (violet, teal, plum or
	// midnight), the big insets another of those tints, and the frames dark teal-steel. The pack's decal
	// sheets stay. -SBKeelGrayWalls keeps the pack's own grey metals.
	if (!FParse::Param(FCommandLine::Get(), TEXT("SBKeelGrayWalls")))
	{
		UMaterialInterface* Plate = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelPlate.MI_SB_KeelPlate"));
		UMaterialInterface* Frame = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelFrame.MI_SB_KeelFrame"));
		const FLinearColor PanelTints[] = { FLinearColor(0.11f, 0.055f, 0.17f), FLinearColor(0.03f, 0.10f, 0.11f),
			FLinearColor(0.13f, 0.045f, 0.10f), FLinearColor(0.045f, 0.055f, 0.13f) };
		const FLinearColor Trim(0.035f, 0.075f, 0.085f);
		for (int32 Index = 0; Plate && Frame && Index < WallSets.Num(); ++Index)
		{
			UInstancedStaticMeshComponent* Set = WallSets[Index];
			for (int32 Slot = 0; Slot < Set->GetNumMaterials(); ++Slot)
			{
				const UMaterialInterface* Current = Set->GetMaterial(Slot);
				const FString Name = Current ? Current->GetName() : FString();
				if (Name.Contains(TEXT("DECALS")) || !Name.StartsWith(TEXT("MI_Metal")))
				{
					continue;
				}
				if (Name == TEXT("MI_Metal_01"))
				{
					Set->SetMaterial(Slot, Plate);
					if (UMaterialInstanceDynamic* MID = Set->CreateDynamicMaterialInstance(Slot))
					{
						MID->SetVectorParameterValue(TEXT("BaseColor"), PanelTints[Index % UE_ARRAY_COUNT(PanelTints)]);
					}
				}
				else if (Name == TEXT("MI_Metal_04"))
				{
					// The big inset panels: another scheme's tint, so every module is two-tone (no glow: as emissive they
					// lit whole walls and blew the frames out, c1 shots).
					Set->SetMaterial(Slot, Plate);
					if (UMaterialInstanceDynamic* MID = Set->CreateDynamicMaterialInstance(Slot))
					{
						MID->SetVectorParameterValue(TEXT("BaseColor"), PanelTints[(Index + 2) % UE_ARRAY_COUNT(PanelTints)] * 0.8f);
					}
				}
				else
				{
					Set->SetMaterial(Slot, Frame);
					if (UMaterialInstanceDynamic* MID = Set->CreateDynamicMaterialInstance(Slot))
					{
						MID->SetVectorParameterValue(TEXT("BaseColor"), Trim);
					}
				}
			}
		}
	}
	if (SkinWalls)
	{
		// The bridges' ceilings: one smooth mauve skin under each (the pack tiles skip the slice).
		if (bBridgesOnly)
		{
			for (const double Side : { -1.0, 1.0 })
			{
				SkinWalls->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Side * 4550.0, 0.0, CeilingZ - 1.5), FVector(13.0, 40.0, 0.02)));
			}
		}
		else
		{
			SkinWalls->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, CeilingZ - 1.5), FVector(Half.X / 50.0, Half.Y / 50.0, 0.02)));
		}
	}
	// Thick hull ribs across the ceiling every 6 m (Midship's heavy curved structure), 4 m segments wherever the ceiling
	// is open above a room. -SBNoKeelCeilingRibs turns them off.
	UInstancedStaticMeshComponent* CeilingRibs = nullptr;
	if (bAlienSlice && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelCeilingRibs")))
	{
		if (UStaticMesh* RibMesh = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Game/Shieldbreak/Art/MapKit/SM_SB_Kit_AlienCeilingRib.SM_SB_Kit_AlienCeilingRib")))
		{
			CeilingRibs = MakeSet(RibMesh);
			ApplyKitSurfaces(CeilingRibs, ShieldbreakMapArt::GetLook(MapId), 0.f);
			for (double X = -Half.X + 300.0; X < Half.X - 100.0; X += 600.0)
			{
				for (double Y = -Half.Y + 200.0; Y < Half.Y - 100.0; Y += 400.0)
				{
					bool bClear = true;
					for (const double Dy : { -180.0, 0.0, 180.0 })
					{
						for (const double Dz : { -20.0, -60.0 })
						{
							const FVector Probe(X, Y + Dy, CeilingZ + Dz);
							bClear = bClear && !Inside(Probe) && InSlice(Probe);
						}
					}
					if (bClear)
					{
						CeilingRibs->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y, CeilingZ - 1.0), FVector(1.0)));
					}
				}
			}
		}
	}
	UE_LOG(LogShieldbreak, Log, TEXT("Keel alien kit: ceiling %.0f, %d coves, %d ceiling ribs, %d deck tiles."), CeilingZ,
		Coves ? Coves->GetInstanceCount() : 0, CeilingRibs ? CeilingRibs->GetInstanceCount() : 0, Decks ? Decks->GetInstanceCount() : 0);
	for (UInstancedStaticMeshComponent* Set : { Panels, PanelsB, PanelsC, Glyphs, Decks, CeilingRibs, Coves })
	{
		if (Set) { Set->RegisterComponent(); AddInstanceComponent(Set); }
	}
	for (UInstancedStaticMeshComponent* Set : { SkinWalls, SkinDecks, Underglow })
	{
		if (Set) { Set->RegisterComponent(); AddInstanceComponent(Set); }
	}
	for (UInstancedStaticMeshComponent* Set : WallSets) { Set->RegisterComponent(); AddInstanceComponent(Set); }
	// The pack's floor metals read pale under Keel's lamps (their colour parameters had no visible effect): the floor
	// tiles keep their shapes but take the scanned tread deck, tinted dark violet steel. -SBKeelPaleFloors keeps them.
	UMaterialInterface* Deck = (bUsingSeasonFloors || FParse::Param(FCommandLine::Get(), TEXT("SBKeelPaleFloors"))) ? nullptr
		: ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Keel/MI_SB_KeelDeck.MI_SB_KeelDeck"));
	for (UInstancedStaticMeshComponent* Set : FloorSets)
	{
		for (int32 Slot = 0; Deck && Slot < Set->GetNumMaterials(); ++Slot)
		{
			const UMaterialInterface* Current = Set->GetMaterial(Slot);
			if (Current && Current->GetName().Contains(TEXT("DECALS")))
			{
				continue;   // keep the pack's floor markings
			}
			Set->SetMaterial(Slot, Deck);
			if (UMaterialInstanceDynamic* MID = Set->CreateDynamicMaterialInstance(Slot))
			{
				MID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.055f, 0.05f, 0.075f));
			}
		}
		Set->RegisterComponent();
		AddInstanceComponent(Set);
	}
	if (Stairs) { Stairs->RegisterComponent(); AddInstanceComponent(Stairs); }
	UE_LOG(LogShieldbreak, Log, TEXT("Keel on the Fab pack grid: %d wall modules, %d floor tiles, %d stair treads (visual only)."), WallCount, FloorCount, StairCount);
	return true;
}

void AShieldbreakArena::BuildCladding()
{
	// -SBNoCladding leaves the blocks bare, for before/after shots and performance comparisons.
	if (GetNetMode() == NM_DedicatedServer || FParse::Param(FCommandLine::Get(), TEXT("SBNoCladding")))
	{
		return;
	}
	// Keel on the Fab pack's grid replaces the hull-kit cladding (local pack only; -SBNoKeelPackKit keeps the hull kit).
	if (MapId == FName(TEXT("Keel")) && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelPacks"))
		&& !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelPackKit")) && BuildKeelPackKit())
	{
		return;
	}
	const FShieldbreakMapLook& Look = ShieldbreakMapArt::GetLook(MapId);
	// -SBCladTint=R,G,B overrides the map's plate tint, for before/after shots.
	FLinearColor CladTint = Look.CladTint;
	FString TintArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("SBCladTint="), TintArg, false))
	{
		TArray<FString> Parts;
		if (TintArg.ParseIntoArray(Parts, TEXT(","), true) == 3)
		{
			CladTint = FLinearColor(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
		}
	}
	int32 Kinds = 0, Placed = 0;
	for (const FShieldbreakArtGroup& Group : ShieldbreakMapArt::Cladding(MapId, Layout))
	{
		// Our own kits (Spire "F" Forerunner plates, Keel "H" hull plating, Art/blender/build_*_kit.py) use palette slots;
		// "Q" is the Quaternius trim-sheet kit. -SBQuaterniusClad shows any map on the Quaternius panels.
		const bool bForerunner = Look.CladKit != FName(TEXT("Q")) && !FParse::Param(FCommandLine::Get(), TEXT("SBQuaterniusClad"));
		const FString Name = (bForerunner ? FString::Printf(TEXT("SM_%s_"), *Look.CladKit.ToString()) : FString(TEXT("SM_Q_"))) + Group.Piece.ToString();
		UStaticMesh* Mesh = Group.Instances.Num() > 0 ? ShieldbreakArt::Load<UStaticMesh>(*FString::Printf(TEXT("/Game/Shieldbreak/Art/%s/%s.%s"),
			bForerunner ? TEXT("MapKit") : TEXT("SciFiKit"), *Name, *Name)) : nullptr;
		if (!Mesh)
		{
			continue;
		}
		// Keel with the Fab Mechanic Base on this machine (local only, git-ignored): its wall modules replace the hull
		// kit's wall plates, each fitted into the hull piece's box (walls up to 20 cm deep, so they
		// never reach into the play space). The bands and trims stay hull kit.
		// -SBKeelPackFlip turns the walls round (the detailed face is the pack's +Y side: yaw -90 shows it).
		// Opt-in for now (-SBKeelPackWalls): squeezed into the hull kit's 4 x 2 m, 20 cm slots the pack walls lose their
		// depth and read flatter than the hull kit (2026-11-15 before/after); the real fix is rebuilding on the pack's grid.
		if (MapId == FName(TEXT("Keel")) && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelPacks"))
			&& FParse::Param(FCommandLine::Get(), TEXT("SBKeelPackWalls")))
		{
			static const TMap<FName, const TCHAR*> PackFor = {
				{ FName(TEXT("WallMetal")), TEXT("SRTUCTURE/WALL/SM_WALL_07") }, { FName(TEXT("WallPlate")), TEXT("SRTUCTURE/WALL/SM_WALL_13") },
				{ FName(TEXT("WallDark")), TEXT("SRTUCTURE/WALL/SM_WALL_12") },
				// The light-band walls (WallBand) stay hull kit: their light slots are the pillars' landmark bands.
				// Floors stay the scanned tread deck: the pack's pale floor tiles washed the decks out (w1 shots).
				};
			const TCHAR* const* PackPath = PackFor.Find(Group.Piece);
			const FString PackName = PackPath ? FPaths::GetBaseFilename(*PackPath) : FString();
			UStaticMesh* PackMesh = PackPath ? ShieldbreakArt::Load<UStaticMesh>(*FString::Printf(
				TEXT("/Game/Modular_Scifi_Mechanic_Base/Mesh/SM/%s.%s"), *PackPath, *PackName)) : nullptr;
			if (PackMesh)
			{
				const bool bWall = !Group.Piece.ToString().StartsWith(TEXT("Floor"));
				FBox Hull = Mesh->GetBoundingBox();
				if (bWall)
				{
					Hull.Max.X = FMath::Max(Hull.Max.X, Hull.Min.X + 20.0);
				}
				const FBox Source = PackMesh->GetBoundingBox();
				// Walls: the pack module's width (X) runs along the hull piece's width (Y), its depth (Y) out of the wall (X).
				const FRotator Turn(0.f, bWall ? (FParse::Param(FCommandLine::Get(), TEXT("SBKeelPackFlip")) ? 90.f : -90.f) : 0.f, 0.f);
				const FBox Turned = Source.TransformBy(FTransform(Turn));
				const FVector Fit = Hull.GetSize() / Turned.GetSize().ComponentMax(FVector(1.0));
				FTransform Local(Turn, FVector::ZeroVector, bWall ? FVector(Fit.Y, Fit.X, Fit.Z) : Fit);
				Local.SetTranslation(Hull.GetCenter() - Local.TransformPosition(Source.GetCenter()));
				const FMatrix LocalMatrix = Local.ToMatrixWithScale();
				auto* PackInstances = AddDecoration<UInstancedStaticMeshComponent>(TEXT("KeelPackClad"), PackMesh);
				PackInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				PackInstances->SetCastShadow(false);
				PackInstances->bAffectDistanceFieldLighting = false;
				for (const FTransform& Instance : Group.Instances)
				{
					PackInstances->AddInstance(FTransform(LocalMatrix * Instance.ToMatrixWithScale()));
				}
				PackInstances->RegisterComponent();
				AddInstanceComponent(PackInstances);
				++Kinds;
				Placed += Group.Instances.Num();
				continue;
			}
		}
		auto* Instances = AddDecoration<UInstancedStaticMeshComponent>(TEXT("Cladding"), Mesh);
		// The blocks underneath already cast the shadows; the panels sit 1-2 cm off them.
		Instances->SetCastShadow(false);
		// Indoors the thousands of panels stay out of the distance fields: the blocks under them already stand there.
		Instances->bAffectDistanceFieldLighting = ShieldbreakArenaPrivate::UsesLumen() && !Layout.bIndoor;
		// The Forerunner plates use palette slots: the map's tinted panel material and glow, like the hero kit (a little
		// self light, as thin pieces laid on a block go dark under the sky light's occlusion).
		if (bForerunner)
		{
			const bool bSpireSurface = MapId == FName(TEXT("Spire"))
				&& !FParse::Param(FCommandLine::Get(), TEXT("SBLegacySpirePanels"));
			FShieldbreakMapLook SurfaceLook = Look;
			if (bSpireSurface && Group.Piece.ToString().StartsWith(TEXT("Floor")))
			{
				SurfaceLook.Metal = FLinearColor(0.23f, 0.28f, 0.32f);
			}
			ApplyKitSurfaces(Instances, SurfaceLook, bSpireSurface ? 0.06f : 0.3f);
		}
		// The trim-sheet slots (Tools/import_scifi_kit.py's M_SB_Trim) take this map's metal and accent colours.
		const TArray<FStaticMaterial>& Slots = bForerunner ? TArray<FStaticMaterial>() : Mesh->GetStaticMaterials();
		for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
		{
			UMaterialInstanceDynamic* MID = Slots[Slot].MaterialSlotName.ToString().StartsWith(TEXT("M_Trim"))
				? Instances->CreateDynamicMaterialInstance(Slot) : nullptr;
			if (MID)
			{
				// A group's own colour (sandstone paving) overrides the map's metal.
				MID->SetVectorParameterValue(TEXT("Tint"), Group.TeamTint.A > 0.f ? Group.TeamTint : CladTint);
				MID->SetVectorParameterValue(TEXT("AccentColor"), Look.CladAccent);
				// -SBCladScalar=Name:Value sets one of M_SB_Trim's scalars on every plate, for diagnosis.
				FString ScalarArg, ScalarName, ScalarValue;
				if (FParse::Value(FCommandLine::Get(), TEXT("SBCladScalar="), ScalarArg) && ScalarArg.Split(TEXT(":"), &ScalarName, &ScalarValue))
				{
					MID->SetScalarParameterValue(FName(*ScalarName), FCString::Atof(*ScalarValue));
				}
				// Floor plates keep their accent lines painted but barely lit, or the whole floor reads as a glowing grid.
				if (Group.Piece.ToString().StartsWith(TEXT("Floor")))
				{
					MID->SetScalarParameterValue(TEXT("AccentGlow"), Layout.bIndoor ? 0.3f : 0.6f);
				}
				else if (Layout.bIndoor)
				{
					// Indoors the bands' accent lines are a hint, not a light source: the lamps and trim light the halls.
					MID->SetScalarParameterValue(TEXT("AccentGlow"), 2.f);
				}
				if (Layout.bIndoor)
				{
					// Brushed rather than polished: past Lumen's reflection roughness cut-off, so the floors and walls
					// skip traced reflections (1.8 ms on Keel, R13) and read as worn hull metal.
					MID->SetScalarParameterValue(TEXT("RoughnessScale"), 1.7f);
				}
			}
		}
		for (const FTransform& Instance : Group.Instances)
		{
			Instances->AddInstance(Instance);
		}
		Instances->RegisterComponent();
		AddInstanceComponent(Instances);
		++Kinds;
		Placed += Group.Instances.Num();
	}
	UE_LOG(LogShieldbreak, Log, TEXT("%s cladding: %d kit pieces, %d instances (visual only)."), *Layout.DisplayName, Kinds, Placed);
}

void AShieldbreakArena::BuildBackdrop()
{
	if (!CubeMesh)
	{
		return;
	}

	// The same scenery on every machine for a map, so seed from its id.
	FRandomStream Random(static_cast<int32>(GetTypeHash(MapId.ToString())));
	const float Reach = static_cast<float>(FMath::Max(Layout.HalfExtent.X, Layout.HalfExtent.Y));
	const FShieldbreakMapLook& Look = ShieldbreakMapArt::GetLook(MapId);
	const FLinearColor GroundColor = Look.Ground, RockColor = Look.Rock;

	// Ground out to the horizon, a few metres below the map floor so the arena reads as a raised platform.
	UStaticMeshComponent* Ground = AddDecoration<UStaticMeshComponent>(TEXT("Backdrop"), CubeMesh);
	Ground->SetCastShadow(false);
	Ground->SetRelativeLocation(FVector(0.f, 0.f, Layout.FloorZ - 400.f));
	Ground->SetRelativeScale3D(FVector(2000.f, 2000.f, 1.f));
	Ground->RegisterComponent();
	AddInstanceComponent(Ground);
	ApplySurface(Ground, Look.GroundStyle, GroundColor, 6.f);
	if (UMaterialInterface* Scanned = Look.bScan && !FParse::Param(FCommandLine::Get(), TEXT("SBNoScan"))
		? ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Scan/MI_SB_Scan_Ground.MI_SB_Scan_Ground")) : nullptr)
	{
		Ground->SetMaterial(0, Scanned);
	}
	const bool bTerrainScan = ShieldbreakMapArt::TerrainScanReady(MapId);
	if (bTerrainScan)
	{
		const FString Set = Look.ScanSet.ToString();
		if (UMaterialInterface* Scanned = ShieldbreakArt::Load<UMaterialInterface>(*FString::Printf(
			TEXT("/Game/Shieldbreak/Art/Scan/MI_SB_Scan_%sGround.MI_SB_Scan_%sGround"), *Set, *Set)))
		{
			Ground->SetMaterial(0, Scanned);
		}
	}

	// A ring of mountains, hazy in the fog: the faceted ring from Art/blender/build_environment.py (inner edge at
	// 100 m, scaled so it starts just past this map's walls), turned and stretched per map so each skyline differs.
	// With the scanned look the far crags (BuildScanArt) are the skyline.
	if ((Look.bScan && !FParse::Param(FCommandLine::Get(), TEXT("SBNoScan"))) || bTerrainScan)
	{
		return;
	}
	if (UStaticMesh* Ring = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Game/Shieldbreak/Art/Environment/SM_SB_Mountains.SM_SB_Mountains")))
	{
		UStaticMeshComponent* Mountains = AddDecoration<UStaticMeshComponent>(TEXT("Backdrop"), Ring);
		Mountains->SetCastShadow(false);
		const float Spread = (Reach + 3000.f) / 10000.f;
		Mountains->SetRelativeLocation(FVector(0.f, 0.f, Layout.FloorZ - 400.f));
		Mountains->SetRelativeRotation(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f));
		Mountains->SetRelativeScale3D(FVector(Spread, Spread, Random.FRandRange(0.8f, 1.25f)));
		Mountains->RegisterComponent();
		AddInstanceComponent(Mountains);
		// Far past Lumen's range and the shadow distance, the ring came out near black on the PC (only a fog-pale rim):
		// keep it out of distance-field lighting and let it glow faintly in its own colour so the skyline reads.
		Mountains->bAffectDistanceFieldLighting = false;
		if (UMaterialInstanceDynamic* Rock = ApplySurface(Mountains, TEXT("rock"), RockColor, 4.f))
		{
			Rock->SetScalarParameterValue(TEXT("SelfLight"), ShieldbreakArenaPrivate::MountainSelfLight());
		}
		return;
	}
	// Without the imported ring: the old engine cones.
	if (!ConeMesh)
	{
		return;
	}
	constexpr int32 Peaks = 22;
	for (int32 Index = 0; Index < Peaks; ++Index)
	{
		const float Angle = (Index + Random.FRandRange(-0.35f, 0.35f)) * static_cast<float>(UE_TWO_PI) / Peaks;
		const float Distance = Reach * 2.f + Random.FRandRange(9000.f, 22000.f);
		const float Width = Random.FRandRange(9000.f, 18000.f);
		const float Height = Random.FRandRange(3000.f, 9000.f) * (Distance / (Reach * 2.f + 15000.f));
		UStaticMeshComponent* Peak = AddDecoration<UStaticMeshComponent>(TEXT("Backdrop"), ConeMesh);
		Peak->SetCastShadow(false);
		// The engine cone is 100 x 100 x 100 with its pivot at the centre.
		Peak->SetRelativeLocation(FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, Layout.FloorZ - 400.f + Height * 0.5f));
		Peak->SetRelativeRotation(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f));
		Peak->SetRelativeScale3D(FVector(Width, Width * Random.FRandRange(0.6f, 1.f), Height) / 100.f);
		Peak->RegisterComponent();
		AddInstanceComponent(Peak);
		ApplySurface(Peak, TEXT("rock"), RockColor * Random.FRandRange(0.85f, 1.15f));
	}
}

void AShieldbreakArena::SetCeilingHidden(bool bHideCeiling)
{
	if (Ceiling)
	{
		Ceiling->SetVisibility(!bHideCeiling);
	}
}

void AShieldbreakArena::BuildInteriorLights()
{
	if (!Ceiling || FParse::Param(FCommandLine::Get(), TEXT("SBNoInteriorLights")))
	{
		return;
	}
	// Pools of light, not an even wash: a grid of work lamps hung under the ceiling, each a downward spot (the ceiling
	// stays dark, the floor gets pools with shade between) with its fixture drawn as a glowing panel, so every light
	// has a visible source. Unshadowed (cheap); the volumetric haze (SpawnLighting) shows their cones. Spacing and
	// strength are overridable (-SBLampSpacing= -SBLampCd=), and -SBPointLamps brings back the old even point lamps.
	const FShieldbreakMapLook& Look = ShieldbreakMapArt::GetLook(MapId);
	const bool bSpots = !FParse::Param(FCommandLine::Get(), TEXT("SBPointLamps"));
	// Spots: 24 m apart at 250 cd with an 18 m reach. Same gunmetal look as the old 17 m / 150 cd / 26 m grid in the
	// 2026-11-03 Keel comparison (R41), for about half the lamps and roughly 5 FPS back.
	float Spacing = bSpots ? 2400.f : 1300.f, Candelas = bSpots ? 250.f : 90.f;
	const bool bSpireLighting = MapId == FName(TEXT("Spire")) && bSpots
		&& !FParse::Param(FCommandLine::Get(), TEXT("SBLegacySpirePanels"));
	if (bSpireLighting) Candelas = 2200.f;
	FParse::Value(FCommandLine::Get(), TEXT("SBLampSpacing="), Spacing);
	FParse::Value(FCommandLine::Get(), TEXT("SBLampCd="), Candelas);
	// How strongly each lamp lights the volumetric haze (its visible cone). -SBLampVol= to compare the cost.
	float VolumetricScattering = bSpots ? 1.5f : 0.f;
	FParse::Value(FCommandLine::Get(), TEXT("SBLampVol="), VolumetricScattering);
	// Spire's 22 m ceiling put the floor beyond the old 18 m lamp radius, leaving the cyan core to light
	// the whole room. Its neutral key light must actually reach the lower routes.
	float Reach = bSpireLighting ? 2800.f : bSpots ? 1800.f : Spacing * 1.9f;
	FParse::Value(FCommandLine::Get(), TEXT("SBLampRadius="), Reach);
	Spacing = FMath::Max(Spacing, 400.f);
	const float CeilingZ = static_cast<float>(Ceiling->Bounds.GetBox().Min.Z - GetActorLocation().Z);
	const float LampZ = CeilingZ - (bSpots ? 40.f : 120.f);
	const int32 CountX = FMath::Max(1, FMath::RoundToInt(2.0 * Layout.HalfExtent.X / Spacing));
	const int32 CountY = FMath::Max(1, FMath::RoundToInt(2.0 * Layout.HalfExtent.Y / Spacing));
	UInstancedStaticMeshComponent* Fixtures = nullptr;
	UInstancedStaticMeshComponent* LampHousings = nullptr;
	bool bPackLampFixtures = false;
	if (bSpots && CubeMesh && GlowMaterial)
	{
		Fixtures = AddDecoration<UInstancedStaticMeshComponent>(TEXT("LampFixture"), CubeMesh);
		Fixtures->SetCastShadow(false);
		if (UMaterialInstanceDynamic* MID = ApplySurface(Fixtures, TEXT("glow"), Look.InteriorLight))
		{
			MID->SetScalarParameterValue(TEXT("EmissiveStrength"), 9.f);
		}
		if (MapId == FName(TEXT("Spire")) && !FParse::Param(FCommandLine::Get(), TEXT("SBNoSpireArchitecture")))
		{
			if (UStaticMesh* Housing = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Game/Shieldbreak/Art/Spire/SM_SB_Spire_LampHousing.SM_SB_Spire_LampHousing")))
			{
				LampHousings = AddDecoration<UInstancedStaticMeshComponent>(TEXT("LampHousing"), Housing);
				LampHousings->SetCastShadow(false);
				if (UMaterialInterface* Inset = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Spire/MI_SB_SpireInset.MI_SB_SpireInset")))
				{
					LampHousings->SetMaterial(0, Inset);
				}
			}
		}
		// Keel with the Fab Mechanic Base pack on this machine (local only, git-ignored): its ceiling lamp, with its own
		// emissive panel, replaces the plain glow slab. Without the pack (or -SBNoKeelPacks) the slab stays.
		if (MapId == FName(TEXT("Keel")) && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelPacks")))
		{
			if (UStaticMesh* PackLamp = ShieldbreakArt::Load<UStaticMesh>(
				TEXT("/Game/Modular_Scifi_Mechanic_Base/Mesh/SM/PROP/LAMP/SM_LAMP_01.SM_LAMP_01")))
			{
				LampHousings = AddDecoration<UInstancedStaticMeshComponent>(TEXT("LampHousing"), PackLamp);
				LampHousings->SetCastShadow(false);
				bPackLampFixtures = true;
			}
		}
	}
	int32 Lamps = 0;
	for (int32 IX = 0; IX < CountX; ++IX)
	{
		for (int32 IY = 0; IY < CountY; ++IY)
		{
			const FVector At(-Layout.HalfExtent.X + (IX + 0.5) * 2.0 * Layout.HalfExtent.X / CountX,
				-Layout.HalfExtent.Y + (IY + 0.5) * 2.0 * Layout.HalfExtent.Y / CountY, LampZ);
			// No margin on Z: the lamps hang 40 cm under the ceiling slab, so a 50 cm margin counted every lamp as inside
			// the ceiling and Keel had none at all (R38: "0 interior spot lamps"; its halls were lit only by the amber
			// trim's bounce light, which is why they read orange).
			const bool bInside = Layout.Blocks.ContainsByPredicate([&At](const FShieldbreakMapBlock& Block)
			{
				return At.X > Block.Min.X - 50.0 && At.X < Block.Max.X + 50.0 && At.Y > Block.Min.Y - 50.0 && At.Y < Block.Max.Y + 50.0
					&& At.Z > Block.Min.Z && At.Z < Block.Max.Z;
			});
			if (bInside)
			{
				continue;
			}
			ULocalLightComponent* Lamp = nullptr;
			if (bSpots)
			{
				USpotLightComponent* Spot = NewObject<USpotLightComponent>(this);
				Spot->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
				Spot->SetInnerConeAngle(28.f);
				Spot->SetOuterConeAngle(58.f);
				Spot->SetSourceRadius(60.f);
				Lamp = Spot;
				if (Fixtures)
				{
					// A 1.6 x 0.5 m light panel flush under the ceiling, turned a quarter on alternate rows.
					if (!bPackLampFixtures)
					{
						Fixtures->AddInstance(FTransform(FRotator(0.f, (IX + IY) % 2 ? 90.f : 0.f, 0.f), FVector(At.X, At.Y, CeilingZ - 6.f),
							FVector(1.6f, 0.5f, 0.06f)));
					}
					if (LampHousings)
					{
						LampHousings->AddInstance(FTransform(FRotator(0.f, (IX + IY) % 2 ? 90.f : 0.f, 0.f),
							FVector(At.X, At.Y, CeilingZ - (bPackLampFixtures ? 61.f : 22.f)), FVector(1.f)));
					}
				}
			}
			else
			{
				UPointLightComponent* Point = NewObject<UPointLightComponent>(this);
				Point->SetSourceRadius(120.f);
				Lamp = Point;
			}
			Lamp->SetupAttachment(Root);
			Lamp->SetMobility(EComponentMobility::Movable);
			Lamp->SetRelativeLocation(At);
			Lamp->SetIntensityUnits(ELightUnits::Candelas);
			Lamp->SetIntensity(Candelas);
			Lamp->SetAttenuationRadius(Reach);
			Lamp->SetLightColor(Look.InteriorLight);
			const bool bSpireKeyShadow = bSpireLighting && !FParse::Param(FCommandLine::Get(), TEXT("SBLegacySpireCondenser"))
				&& !FParse::Param(FCommandLine::Get(), TEXT("SBNoSpireKeyShadows"))
				&& IY == CountY / 2 && (IX == CountX / 2 || IX + 1 == CountX / 2);
			Lamp->SetCastShadows(bSpireKeyShadow);
			Lamp->SetVolumetricScatteringIntensity(VolumetricScattering);
			Lamp->RegisterComponent();
			++Lamps;
		}
	}
	if (Fixtures)
	{
		Fixtures->RegisterComponent();
		AddInstanceComponent(Fixtures);
	}
	if (LampHousings)
	{
		LampHousings->RegisterComponent();
		AddInstanceComponent(LampHousings);
	}
	UE_LOG(LogShieldbreak, Log, TEXT("%s: enclosed, %d interior %s (%.0f cm grid, %.0f cd)."), *Layout.DisplayName, Lamps,
		bSpots ? TEXT("spot lamps") : TEXT("point lamps"), Spacing, Candelas);
}

void AShieldbreakArena::BuildSpireHeroLights()
{
	if (MapId != FName(TEXT("Spire")) || !Ceiling || GetNetMode() == NM_DedicatedServer
		|| FParse::Param(FCommandLine::Get(), TEXT("SBLegacySpireCondenser"))
		|| FParse::Param(FCommandLine::Get(), TEXT("SBNoSpireHeroLights"))
		|| FParse::Param(FCommandLine::Get(), TEXT("SBNoMapArt")))
	{
		return;
	}
	const double CeilingZ = Ceiling->Bounds.GetBox().Min.Z - GetActorLocation().Z;
	for (const double Angle : { 0.0, 120.0, 240.0 })
	{
		const double Radians = FMath::DegreesToRadians(Angle);
		const FVector Housing(840.0 * FMath::Cos(Radians), 840.0 * FMath::Sin(Radians), CeilingZ - 210.0);
		const FVector Direction = (FVector(0.0, 0.0, CeilingZ - 110.0) - Housing).GetSafeNormal();
		auto* Projector = NewObject<USpotLightComponent>(this);
		Projector->SetupAttachment(Root);
		Projector->SetMobility(EComponentMobility::Movable);
		// Forty cm puts the source just in front of the modeled barrel/lens, rather than inside its shadow.
		Projector->SetRelativeLocation(Housing + Direction * 40.0);
		Projector->SetRelativeRotation(Direction.Rotation());
		Projector->SetInnerConeAngle(24.f);
		Projector->SetOuterConeAngle(42.f);
		Projector->SetSourceRadius(8.f);
		Projector->SetIntensityUnits(ELightUnits::Candelas);
		Projector->SetIntensity(400.f);
		Projector->SetAttenuationRadius(1400.f);
		Projector->SetLightColor(FLinearColor(0.90f, 0.94f, 1.f));
		Projector->SetCastShadows(true);
		Projector->SetVolumetricScatteringIntensity(0.f);
		Projector->RegisterComponent();
		AddInstanceComponent(Projector);
	}
	UE_LOG(LogShieldbreak, Log, TEXT("Spire condenser V2: three shadowed practical projectors, 400 cd / 14 m, no volumetric scattering."));
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
	// Owned by the arena so EndPlay can take them down when the menu backdrop swaps maps.
	Params.Owner = this;

	// Light levels, overridable on the command line (-SBSun= -SBFill= -SBSky=) so the PC can compare settings in one batch.
	// PC comparison (2026-10-22, Keel): sky 6 lifted the shaded walls from black to readable grey; a stronger fill (5) made
	// them darker (exposure adapts to the brighter lit faces), so the sky light carries the ambient.
	// Lumen (on by default since the desktop's RTX 2080 Ti became the build machine; -SBNoLumen for the old look) bounces
	// the sun itself, so the fill light goes and the sky light drops to a natural level.
	const bool bLumen = ShieldbreakArenaPrivate::UsesLumen();
	float SunIntensity = 8.f, FillIntensity = bLumen ? 0.f : 1.5f, SkyIntensity = bLumen ? 3.f : 6.f;
	// Keel's alien look: a dimmer sky light, so the rooms get depth from their own lamps and glows (lx1 comparison).
	if (MapId == FName(TEXT("Keel")) && bLumen && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelAlienSlice")))
	{
		SkyIntensity = 1.5f;
	}
	FParse::Value(FCommandLine::Get(), TEXT("SBSun="), SunIntensity);
	FParse::Value(FCommandLine::Get(), TEXT("SBFill="), FillIntensity);
	FParse::Value(FCommandLine::Get(), TEXT("SBSky="), SkyIntensity);

	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0.f, 0.f, 3000.f), Layout.SunRotation, Params))
	{
		if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			SunLight->SetMobility(EComponentMobility::Movable);
			SunLight->SetIntensity(SunIntensity);
			SunLight->SetAtmosphereSunLight(true);
			SunLight->SetForwardShadingPriority(1); // the sun, not the fill, drives fog and translucency
			SunLight->SetCastShadows(true);
			// Enclosed maps: the roof keeps the sun out anyway; its shadows (cascades and distance-field shadows over
			// the whole interior) cost about 0.5 ms for no light. -SBIndoorSun keeps it for comparisons.
			if (Layout.bIndoor && !FParse::Param(FCommandLine::Get(), TEXT("SBIndoorSun")))
			{
				SunLight->SetIntensity(0.f);
				SunLight->SetCastShadows(false);
			}
			// Scanned-terrain maps: the fitted Nanite rocks in every cascade cost 1.1-1.3 ms of shadow depths (R15
			// profile). Two cascades over the arena itself, and distance-field shadows (Lumen already builds the
			// fields) past that. -SBSunCascades=, -SBSunShadowDist= and -SBNoSunDF for comparisons. The scanned-art
			// maps (Sanctum: Nanite cliffs, trees and carved blocks) had the same cost and more: 3.2 ms of shadow
			// depths on an 8192x2048 atlas (R44 profile). -SBFullSunShadows keeps the engine default there.
			else if (ShieldbreakMapArt::TerrainScanReady(MapId)
				|| (ShieldbreakMapArt::GetLook(MapId).bScan && !FParse::Param(FCommandLine::Get(), TEXT("SBNoScan"))
					&& !FParse::Param(FCommandLine::Get(), TEXT("SBFullSunShadows"))))
			{
				int32 Cascades = 2;
				float ShadowDistance = 6000.f;
				FParse::Value(FCommandLine::Get(), TEXT("SBSunCascades="), Cascades);
				FParse::Value(FCommandLine::Get(), TEXT("SBSunShadowDist="), ShadowDistance);
				SunLight->SetDynamicShadowCascades(Cascades);
				SunLight->SetDynamicShadowDistanceMovableLight(ShadowDistance);
				if (!FParse::Param(FCommandLine::Get(), TEXT("SBNoSunDF")))
				{
					SunLight->bUseRayTracedDistanceFieldShadows = true;
					SunLight->DistanceFieldShadowDistance = 25000.f;
					SunLight->MarkRenderStateDirty();
				}
			}
		}
	}

	// Fill light from the opposite side, no shadows: on low scalability (the Iris Xe laptop) Lumen GI and the sky
	// light's real-time capture contribute little, so faces turned away from the sun rendered nearly black.
	const FRotator FillRotation(-25.f, Layout.SunRotation.Yaw + 180.f, 0.f);
	if (ADirectionalLight* Fill = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0.f, 0.f, 3000.f), FillRotation, Params))
	{
		if (UDirectionalLightComponent* FillLight = Cast<UDirectionalLightComponent>(Fill->GetLightComponent()))
		{
			FillLight->SetMobility(EComponentMobility::Movable);
			FillLight->SetIntensity(FillIntensity);
			FillLight->SetLightColor(FLinearColor(0.75f, 0.85f, 1.f));
			FillLight->SetAtmosphereSunLight(false);
			FillLight->SetCastShadows(false);
		}
	}

	const bool bScannedSky = (ShieldbreakMapArt::GetLook(MapId).bScan && !FParse::Param(FCommandLine::Get(), TEXT("SBNoScan")))
		|| ShieldbreakMapArt::TerrainScanReady(MapId);
	const bool bWantClouds = bScannedSky && !FParse::Param(FCommandLine::Get(), TEXT("SBNoClouds"))
		&& (ShieldbreakMapArt::GetLook(MapId).bClouds || FParse::Param(FCommandLine::Get(), TEXT("SBClouds")));
	if (ASkyAtmosphere* Atmosphere = World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FTransform::Identity, Params))
	{
		// A scanned map without clouds (Creekbed): more aerosol haze and a tighter sun glow, so the sky has a bright
		// band at the horizon and a halo round the sun instead of a flat gradient. Free next to the cloud layer.
		// -SBNoSkyHaze keeps the engine default for comparisons.
		USkyAtmosphereComponent* Air = Atmosphere->FindComponentByClass<USkyAtmosphereComponent>();
		if (Air && bScannedSky && !bWantClouds && !FParse::Param(FCommandLine::Get(), TEXT("SBNoSkyHaze")))
		{
			Air->SetMieScatteringScale(0.008f); // engine default 0.004
			Air->SetMieAnisotropy(0.85f);
			Air->SetMieExponentialDistribution(1.6f);
		}
	}

	// The scanned maps get a sky of real clouds (the engine's volumetric cloud layer) instead of a bare gradient,
	// except where the map's look turns them off. -SBNoClouds turns them off for comparisons, -SBClouds forces them on.
	if (bWantClouds)
	{
		if (AVolumetricCloud* Clouds = World->SpawnActor<AVolumetricCloud>(AVolumetricCloud::StaticClass(), FTransform::Identity, Params))
		{
			UVolumetricCloudComponent* CloudLayer = Clouds->FindComponentByClass<UVolumetricCloudComponent>();
			// Out of the real-time sky capture: rendering the clouds into the sky light's cube faces cost up to 1.5 ms
			// on the frames that recapture (R16 Creekbed profile); the sky light barely changes without them.
			// -SBCloudsInSkyLight keeps them in.
			if (CloudLayer && !FParse::Param(FCommandLine::Get(), TEXT("SBCloudsInSkyLight")))
			{
				CloudLayer->bVisibleInRealTimeSkyCaptures = false;
				CloudLayer->MarkRenderStateDirty();
			}
			UE_LOG(LogShieldbreak, Log, TEXT("Sky: volumetric clouds%s."), CloudLayer ? TEXT("") : TEXT(" (no component)"));
		}
	}

	if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector(0.f, 0.f, 2000.f), FRotator::ZeroRotator, Params))
	{
		if (USkyLightComponent* SkyComponent = Sky->GetLightComponent())
		{
			SkyComponent->SetMobility(EComponentMobility::Movable);
			SkyComponent->bRealTimeCapture = true;
			SkyComponent->SetIntensity(SkyIntensity);
			// The ground under the map is just as lit as the sky, so undersides and overhangs aren't black either.
			SkyComponent->bLowerHemisphereIsBlack = false;
			SkyComponent->SetLowerHemisphereColor(FLinearColor(0.35f, 0.33f, 0.3f));
			SkyComponent->RecaptureSky();

			// PC finding (2026-10-22): shaded walls stay near-black from startup, but turn a readable blue-grey as soon as
			// any renderer cvar with a recreate-render-state sink is set mid-match (even to its current value); timed
			// RecaptureSky() calls alone didn't help. So once the arena and the first players exist, recreate every
			// component's render state the way that cvar does, then recapture the sky. A one-off hitch at load.
			// -SBNoSkyRecapture keeps the old behaviour for comparison; -SBRecreateAt=<s> moves it (default 1.5 s).
			if (!FParse::Param(FCommandLine::Get(), TEXT("SBNoSkyRecapture")))
			{
				float RecreateDelay = 1.5f;
				FParse::Value(FCommandLine::Get(), TEXT("SBRecreateAt="), RecreateDelay);
				TWeakObjectPtr<USkyLightComponent> WeakSky = SkyComponent;
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(Handle, [WeakSky]()
				{
					{
						FGlobalComponentRecreateRenderStateContext Recreate;
					}
					if (USkyLightComponent* Component = WeakSky.Get())
					{
						Component->RecaptureSky();
					}
					UE_LOG(LogShieldbreak, Log, TEXT("Lighting: recreated render state and recaptured the sky."));
				}, FMath::Max(RecreateDelay, 0.1f), false);
			}
		}
	}

	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector(0.f, 0.f, Layout.FloorZ - 200.f), FRotator::ZeroRotator, Params))
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			FogComponent->SetFogDensity(Layout.FogDensity);
			// Haze starting past the arena so the playspace stays crisp and the backdrop fades.
			// Each map's own haze colour (ShieldbreakMapArt::GetLook).
			FogComponent->SetFogInscatteringColor(ShieldbreakMapArt::GetLook(MapId).Haze);
			FogComponent->SetFogHeightFalloff(0.08f);
			FogComponent->SetStartDistance(4000.f);
			// Enclosed maps: a thin volumetric haze from the start, so the lamps' cones show as shafts of light in the
			// dusty air. -SBNoVolFog for before/after and frame-time comparisons.
			if (Layout.bIndoor && !FParse::Param(FCommandLine::Get(), TEXT("SBNoVolFog")))
			{
				// Neutral, dim haze: the map's warm outdoor haze colour tinted every surface orange from start 0.
				FogComponent->SetFogInscatteringColor(FLinearColor(0.05f, 0.055f, 0.065f));
				FogComponent->SetVolumetricFogAlbedo(FColor(200, 205, 215));
				if (MapId == FName(TEXT("Keel")) && FParse::Param(FCommandLine::Get(), TEXT("SBKeelAlienLook")))
				{
					// Alien look: a violet haze that catches the teal lamps and the reactor glow as shafts.
					FogComponent->SetFogInscatteringColor(FLinearColor(0.045f, 0.02f, 0.07f));
					FogComponent->SetVolumetricFogAlbedo(FColor(185, 140, 235));
					FogComponent->SetVolumetricFogExtinctionScale(1.6f);
				}
				FogComponent->SetVolumetricFog(true);
				FogComponent->SetVolumetricFogScatteringDistribution(0.5f);
				FogComponent->SetVolumetricFogExtinctionScale(1.f);
				FogComponent->SetVolumetricFogDistance(5000.f);
				FogComponent->SetStartDistance(0.f);
				FogComponent->SetFogDensity(Layout.FogDensity * 1.5f);
				FogComponent->SetFogHeightFalloff(0.001f);
			}
		}
	}

	// A light grade: some bloom for the trim and glow, a touch more saturation and contrast, and a soft vignette.
	// The same unbound volume switches on Lumen bounce light and reflections (the project default is off).
	if (APostProcessVolume* Grade = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params))
	{
		Grade->bUnbound = true;
		FPostProcessSettings& Settings = Grade->Settings;
		if (bLumen)
		{
			Settings.bOverride_DynamicGlobalIlluminationMethod = true;
			Settings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
			Settings.bOverride_ReflectionMethod = true;
			Settings.ReflectionMethod = EReflectionMethod::Lumen;
			// Scanned maps: more bounce light, so the ground under walkways and overhangs reads as shade rather than
			// black (and the eye does not adapt so far that the daylight blows out). -SBIndirect=<scale> to tune.
			if ((ShieldbreakMapArt::GetLook(MapId).bScan && !FParse::Param(FCommandLine::Get(), TEXT("SBNoScan")))
				|| ShieldbreakMapArt::TerrainScanReady(MapId))
			{
				float Indirect = 1.8f;
				FParse::Value(FCommandLine::Get(), TEXT("SBIndirect="), Indirect);
				Settings.bOverride_IndirectLightingIntensity = true;
				Settings.IndirectLightingIntensity = Indirect;
			}
		}
		if (!FParse::Param(FCommandLine::Get(), TEXT("SBNoGrade")))
		{
			Settings.bOverride_BloomIntensity = true;
			Settings.BloomIntensity = 0.9f;
			Settings.bOverride_BloomThreshold = true;
			Settings.BloomThreshold = 1.2f;
			Settings.bOverride_ColorSaturation = true;
			Settings.ColorSaturation = FVector4(1.12f, 1.12f, 1.12f, 1.f);
			Settings.bOverride_ColorContrast = true;
			Settings.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.f);
			Settings.bOverride_VignetteIntensity = true;
			Settings.VignetteIntensity = 0.35f;
			if (MapId == FName(TEXT("Keel")) && FParse::Param(FCommandLine::Get(), TEXT("SBKeelAlienLook")))
			{
				// Alien look: saturated and contrasty, shadows pushed toward violet and teal, highlights a touch warm,
				// and more bloom so the trim and glow read as light sources.
				Settings.ColorSaturation = FVector4(1.35f, 1.35f, 1.35f, 1.f);
				Settings.ColorContrast = FVector4(1.18f, 1.18f, 1.18f, 1.f);
				Settings.bOverride_ColorGammaShadows = true;
				Settings.ColorGammaShadows = FVector4(0.94f, 0.96f, 1.12f, 1.f);
				Settings.bOverride_ColorGainShadows = true;
				Settings.ColorGainShadows = FVector4(0.85f, 0.85f, 1.05f, 1.f);
				Settings.bOverride_ColorGainHighlights = true;
				Settings.ColorGainHighlights = FVector4(1.04f, 1.0f, 0.98f, 1.f);
				Settings.BloomIntensity = 1.4f;
				Settings.BloomThreshold = 0.8f;
				Settings.VignetteIntensity = 0.45f;
			}
			// The first shots read washed out (pale walls and floors in full sun): pull exposure down a little.
			// -SBExposure=<stops> overrides it for comparisons.
			float ExposureBias = MapId == FName(TEXT("Keel")) && !FParse::Param(FCommandLine::Get(), TEXT("SBNoKeelAlienSlice")) ? -1.1f : -0.4f;
			FParse::Value(FCommandLine::Get(), TEXT("SBExposure="), ExposureBias);
			Settings.bOverride_AutoExposureBias = true;
			Settings.AutoExposureBias = ExposureBias;
			// -SBExposureMin=<EV100>: the darkest scene the eye adapts to, so stepping out from under cover does not
			// blow the daylight out (being tuned on Sanctum).
			float MinEV = 0.f;
			if (FParse::Value(FCommandLine::Get(), TEXT("SBExposureMin="), MinEV))
			{
				Settings.bOverride_AutoExposureMinBrightness = true;
				Settings.AutoExposureMinBrightness = MinEV;
			}
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
// ---- Scanned art ----

namespace ShieldbreakScanPrivate
{
	/** A Megascans mesh copied by Tools/GetScanArt.bat, e.g. Mesh(TEXT("3D_Assets/MossyForestBoulder"), TEXT("SM_MossyForestBoulder_01")). */
	UStaticMesh* Mesh(const TCHAR* Folder, const TCHAR* Name)
	{
		return ShieldbreakArt::Load<UStaticMesh>(*FString::Printf(TEXT("/Game/Megascans/%s/%s.%s"), Folder, Name, Name));
	}

	/** Every mesh of a numbered family that loaded (names Prefix + two-digit number, e.g. SM_MossyRockFace_01). */
	TArray<UStaticMesh*> Family(const TCHAR* Folder, const TCHAR* Prefix, std::initializer_list<int32> Numbers)
	{
		TArray<UStaticMesh*> Out;
		for (const int32 Number : Numbers)
		{
			if (UStaticMesh* Found = Mesh(Folder, *FString::Printf(TEXT("%s%02d"), Prefix, Number)))
			{
				Out.Add(Found);
			}
		}
		return Out;
	}

	/** The axis-aligned box (arena space) of Mesh's bounds under Transform. */
	FBox PlacedBounds(const UStaticMesh* Mesh, const FTransform& Transform)
	{
		return Mesh->GetBoundingBox().TransformBy(Transform);
	}

	bool InsideBlock(const FShieldbreakMapLayout& Layout, const FVector& Point)
	{
		for (const FShieldbreakMapBlock& Block : Layout.Blocks)
		{
			if (Point.X > Block.Min.X && Point.X < Block.Max.X && Point.Y > Block.Min.Y && Point.Y < Block.Max.Y
				&& Point.Z > Block.Min.Z && Point.Z < Block.Max.Z)
			{
				return true;
			}
		}
		return false;
	}

	/** Something walkable right under Point: the floor or a block top at its height. */
	bool OnSurface(const FShieldbreakMapLayout& Layout, const FVector& Point)
	{
		for (const FShieldbreakMapBlock& Block : Layout.Blocks)
		{
			if (Point.X > Block.Min.X && Point.X < Block.Max.X && Point.Y > Block.Min.Y && Point.Y < Block.Max.Y
				&& FMath::Abs(Point.Z - Block.Max.Z) < 5.0)
			{
				return true;
			}
		}
		return false;
	}
}

namespace ShieldbreakScanPrivate
{
	/** A perimeter wall: a block standing outside the playable half extent (the map's boundary). */
	bool IsPerimeter(const FShieldbreakMapLayout& Layout, const FShieldbreakMapBlock& Block)
	{
		return Block.Max.Z > Layout.FloorZ + 1.0 && (Block.Min.X <= -Layout.HalfExtent.X - 50.0 || Block.Max.X >= Layout.HalfExtent.X + 50.0
			|| Block.Min.Y <= -Layout.HalfExtent.Y - 50.0 || Block.Max.Y >= Layout.HalfExtent.Y + 50.0);
	}

	/** The altar hero piece, for a 600 x 600 x 320 cm stone plinth standing on the floor (Sanctum's centre, the
	 *  rockets): it is the plinth's shell, with its own light channels. Null for any other block, or with -SBNoAltar. */
	UStaticMesh* AltarFor(const FShieldbreakMapLayout& Layout, const FShieldbreakMapBlock& Block)
	{
		const FVector Size = Block.Max - Block.Min;
		if (Block.Style != FName(TEXT("stone")) || !FMath::IsNearlyEqual(Size.X, 600.0, 1.0) || !FMath::IsNearlyEqual(Size.Y, 600.0, 1.0)
			|| !FMath::IsNearlyEqual(Size.Z, 320.0, 1.0) || !FMath::IsNearlyEqual(Block.Min.Z, static_cast<double>(Layout.FloorZ), 1.0)
			|| FParse::Param(FCommandLine::Get(), TEXT("SBNoAltar")))
		{
			return nullptr;
		}
		return ShieldbreakArt::Load<UStaticMesh>(TEXT("/Game/Shieldbreak/Art/MapKit/SM_SB_Kit_Altar.SM_SB_Kit_Altar"));
	}

	/** One horizontal ring of a carved block's outline: height above the block's foot and how far the face sits out
	 *  from the collision box (negative = inset). */
	struct FRing { double Z; double Out; };

	/**
	 * The profile of a Forerunner-style carved block of Size (cm): a raised base, walls that lean in over the lower
	 * half (a battered wall), a recessed channel under the top where the light trim sits (Arena BuildTrim puts it
	 * 30 cm below the top), and a bevelled cap. Features shrink on thin or short blocks so they never eat the shape.
	 */
	TArray<FRing> CarvedProfile(const FVector& Size)
	{
		const double H = Size.Z;
		const double S = FMath::Clamp(FMath::Min(Size.X, Size.Y) / 150.0, 0.35, 1.0);
		const double Batter = FMath::Clamp(H * 0.05, 0.0, 30.0) * S;
		const double Plinth = 14.0 * S;
		const double Bevel = FMath::Min(10.0 * S, H * 0.08);
		TArray<FRing> Rings;
		Rings.Add({ 0.0, Plinth + Batter });
		if (H >= 160.0)
		{
			Rings.Add({ 40.0, Plinth + Batter });
			Rings.Add({ 55.0, Batter });
			Rings.Add({ FMath::Max(H * 0.55, 60.0), 0.0 });
			const double Depth = 7.0 * S;
			const double Low = H - 30.0 - 11.0, High = H - 30.0 + 11.0;
			Rings.Add({ Low, 0.0 });
			Rings.Add({ Low, -Depth });
			Rings.Add({ High, -Depth });
			Rings.Add({ High, 0.0 });
		}
		else
		{
			Rings.Add({ FMath::Min(25.0, H * 0.2), Plinth * 0.5 + Batter });
			Rings.Add({ FMath::Min(45.0, H * 0.35), 0.0 });
		}
		Rings.Add({ H - Bevel, 0.0 });
		Rings.Add({ H, -Bevel });
		return Rings;
	}

	/** The eight corners of a box outline Out cm proud of the half extents, the vertical edges cut by Chamfer. */
	void Outline(const FVector& Half, double Out, double Chamfer, FVector2D (&Points)[8])
	{
		const double X = Half.X + Out, Y = Half.Y + Out;
		const double C = FMath::Min(Chamfer, FMath::Min(X, Y) * 0.45);
		Points[0] = FVector2D(X, -Y + C); Points[1] = FVector2D(X, Y - C);
		Points[2] = FVector2D(X - C, Y); Points[3] = FVector2D(-X + C, Y);
		Points[4] = FVector2D(-X, Y - C); Points[5] = FVector2D(-X, -Y + C);
		Points[6] = FVector2D(-X + C, -Y); Points[7] = FVector2D(X - C, -Y);
	}

	/**
	 * A carved block mesh (local space, origin at the block's centre), flat-shaded. Each face is emitted in both
	 * windings with its outward normal, so it renders right whichever winding the renderer treats as front.
	 */
	/** Carved blocks sharing one material, merged into one mesh section (one draw per material, not per block). */
	struct FCarvedMesh
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
	};

	void AppendCarvedBlock(FCarvedMesh& Out, const FVector& Size, const FVector& Center)
	{
		const FVector Half = Size * 0.5;
		const double Chamfer = FMath::Clamp(FMath::Min(Size.X, Size.Y) * 0.12, 6.0, 35.0);
		const TArray<FRing> Rings = CarvedProfile(Size);
		TArray<FVector>& Vertices = Out.Vertices;
		TArray<int32>& Triangles = Out.Triangles;
		TArray<FVector>& Normals = Out.Normals;
		TArray<FVector2D>& UVs = Out.UVs;
		auto Face = [&](TConstArrayView<FVector> Corners, const FVector& Outward)
		{
			const int32 First = Vertices.Num();
			for (const FVector& Corner : Corners)
			{
				Vertices.Add(Corner + Center);
				Normals.Add(Outward);
				UVs.Add(FVector2D(Corner.X + Corner.Y, Corner.Z) / 100.0);
			}
			for (int32 Index = 1; Index + 1 < Corners.Num(); ++Index)
			{
				Triangles.Append({ First, First + Index, First + Index + 1 });
				Triangles.Append({ First, First + Index + 1, First + Index });
			}
		};
		FVector2D Lower[8], Upper[8];
		for (int32 Ring = 0; Ring + 1 < Rings.Num(); ++Ring)
		{
			Outline(Half, Rings[Ring].Out, Chamfer, Lower);
			Outline(Half, Rings[Ring + 1].Out, Chamfer, Upper);
			const double Z0 = Rings[Ring].Z - Half.Z, Z1 = Rings[Ring + 1].Z - Half.Z;
			for (int32 Side = 0; Side < 8; ++Side)
			{
				const int32 Next = (Side + 1) % 8;
				const FVector A(Lower[Side], Z0), B(Lower[Next], Z0), C(Upper[Next], Z1), D(Upper[Side], Z1);
				const FVector2D Edge = (Lower[Next] - Lower[Side]).GetSafeNormal();
				// The band's plane normal, turned to face out of the block; a ledge (both rings at one height) faces up
				// where the outline steps in going up (the channel's lower lip) and down where it steps out.
				const FVector Horizontal(Edge.Y, -Edge.X, 0.0);
				FVector Outward = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
				if (FMath::IsNearlyEqual(Z0, Z1))
				{
					Outward = FVector(0, 0, Rings[Ring + 1].Out < Rings[Ring].Out ? 1.0 : -1.0);
				}
				else if (FVector::DotProduct(Outward, Horizontal) < 0.0)
				{
					Outward = -Outward;
				}
				Face({ A, B, C, D }, Outward);
			}
		}
		Outline(Half, Rings.Last().Out, Chamfer, Upper);
		Outline(Half, Rings[0].Out, Chamfer, Lower);
		// The top only: the foot stands on the ground and is never seen.
		TArray<FVector, TInlineAllocator<8>> Top;
		for (int32 Side = 0; Side < 8; ++Side)
		{
			Top.Add(FVector(Upper[Side], Half.Z));
		}
		Face(Top, FVector(0, 0, 1));
	}
}

void AShieldbreakArena::BuildScanArt()
{
	using namespace ShieldbreakScanPrivate;
	const FShieldbreakMapLook& Look = ShieldbreakMapArt::GetLook(MapId);
	// -SBNoScan keeps the old surfaces, for before/after shots and performance comparisons.
	if (!Look.bScan || GetNetMode() == NM_DedicatedServer || FParse::Param(FCommandLine::Get(), TEXT("SBNoScan")))
	{
		return;
	}
	auto Material = [](const TCHAR* Kind)
	{
		return ShieldbreakArt::Load<UMaterialInterface>(*FString::Printf(TEXT("/Game/Shieldbreak/Art/Scan/MI_SB_Scan_%s.MI_SB_Scan_%s"), Kind, Kind));
	};
	UMaterialInterface* Ground = Material(TEXT("Ground"));
	UMaterialInterface* Stone = Material(TEXT("Stone"));
	UMaterialInterface* Path = Material(TEXT("Path"));
	UMaterialInterface* Dark = Material(TEXT("Dark"));
	if (!Ground || !Stone || !Path || !Dark)
	{
		UE_LOG(LogShieldbreak, Warning, TEXT("%s: no scanned materials (run Tools/GetScanArt.bat and Tools/MakeScanArt.bat); keeping the old surfaces."), *Layout.DisplayName);
		return;
	}
	const bool bLumen = ShieldbreakArenaPrivate::UsesLumen();
	FRandomStream Random(static_cast<int32>(GetTypeHash(MapId.ToString())) ^ 0x5CA9);
	const TArray<UStaticMesh*> Boulders = Family(TEXT("3D_Assets/MossyForestBoulder"), TEXT("SM_MossyForestBoulder_"), { 1, 2, 3, 4, 5, 6, 7 });

	// 1. The blocks and ramps: scanned stone with moss on top, earth and grass underfoot, worn paving on walkways.
	static const FName Floor(TEXT("floor")), Sand(TEXT("sand")), Grass(TEXT("grass")), Bridge(TEXT("bridge")), Ramp(TEXT("ramp"));
	static const FName RockStyle(TEXT("rock")), DarkStyle(TEXT("dark")), Red(TEXT("red")), Blue(TEXT("blue"));
	const int32 NumBlocks = Layout.Blocks.Num();
	TArray<int32> RockBlocks;
	TArray<int32> AltarBlocks;
	// -SBNoCarve keeps the plain boxes (with the scanned materials) for comparisons.
	const bool bCarve = !FParse::Param(FCommandLine::Get(), TEXT("SBNoCarve"));
	// -SBScanRockOwn keeps the scanned rocks' own materials (they come out black; see Place).
	const bool bRockOverride = !FParse::Param(FCommandLine::Get(), TEXT("SBScanRockOwn"));
	int32 CarvedCount = 0;
	TMap<int32, TMap<TPair<UMaterialInterface*, int32>, FCarvedMesh>> CarvedMeshes;
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		UStaticMeshComponent* Piece = Pieces[Index];
		const FName Style = Index < NumBlocks ? Layout.Blocks[Index].Style
			: Layout.Ramps.IsValidIndex(Index - NumBlocks) ? Layout.Ramps[Index - NumBlocks].Style : NAME_None;
		if (!Piece || Style == FName(TEXT("glow")) || Style == FName(TEXT("water")))
		{
			continue;
		}
		if (Index < NumBlocks && IsPerimeter(Layout, Layout.Blocks[Index]))
		{
			// The boundary is the canyon itself (cliffs, terraces and gates below): the wall stays only as an
			// invisible barrier and Lumen occluder.
			Piece->bAffectIndirectLightingWhileHidden = true;
			Piece->SetVisibility(false);
			continue;
		}
		if (Index < NumBlocks && AltarFor(Layout, Layout.Blocks[Index]))
		{
			// The centre plinth: the Blender altar (placed with the hero pieces below) is its shell.
			Piece->bAffectIndirectLightingWhileHidden = true;
			Piece->SetVisibility(false);
			AltarBlocks.Add(Index);
			continue;
		}
		if (Style == RockStyle && Index < NumBlocks && Boulders.Num() > 0)
		{
			// The box keeps its collision; a boulder fitted over it is what players see.
			Piece->SetVisibility(false);
			RockBlocks.Add(Index);
			continue;
		}
		UMaterialInterface* Surface = (Style == Floor || Style == Sand || Style == Grass) ? Ground
			: (Style == Bridge || Style == Ramp || Index >= NumBlocks) ? Path
			: Style == DarkStyle ? Dark : Stone;
		UMeshComponent* Shown = Piece;
		if (Index < NumBlocks && Surface != Ground && Surface != Path && bCarve)
		{
			const FShieldbreakMapBlock& Block = Layout.Blocks[Index];
			const FVector Size = Block.Max - Block.Min;
			if (Size.Z >= 120.0 && FMath::Min(Size.X, Size.Y) >= 40.0)
			{
				// Carved stone in place of the box; the box stays as the (hidden) collision and Lumen occluder. Blocks
				// are merged per material (team tint included) and built as one mesh after this loop.
				// Also split into 40 m cells, so each part still culls and gets its own Lumen cards.
				const FVector Center = (Block.Min + Block.Max) * 0.5;
				const int32 Cell = (FMath::FloorToInt32(Center.X / 4000.0) + 64) * 128 + FMath::FloorToInt32(Center.Y / 4000.0) + 64;
				const int32 Tint = Style == Red ? 1 : Style == Blue ? 2 : 0;
				AppendCarvedBlock(CarvedMeshes.FindOrAdd(Cell).FindOrAdd(TPair<UMaterialInterface*, int32>(Surface, Tint)), Size, Center);
				Piece->bAffectIndirectLightingWhileHidden = true;
				Piece->SetVisibility(false);
				++CarvedCount;
				continue;
			}
		}
		Shown->SetMaterial(0, Surface);
		if (Style == Red || Style == Blue)
		{
			// The temples keep a hint of their team: warm or cool stone (the trim lines carry the colour).
			if (UMaterialInstanceDynamic* MID = Shown->CreateDynamicMaterialInstance(0))
			{
				MID->SetVectorParameterValue(TEXT("Tint"), Style == Red ? FLinearColor(1.02f, 0.86f, 0.8f) : FLinearColor(0.82f, 0.9f, 1.0f));
			}
		}
	}

	// The merged carved stone: one component per cell, one section per material.
	for (const TPair<int32, TMap<TPair<UMaterialInterface*, int32>, FCarvedMesh>>& CellEntry : CarvedMeshes)
	{
		UProceduralMeshComponent* Carved = NewObject<UProceduralMeshComponent>(this, FName(TEXT("Carved"), CellEntry.Key), RF_Transient);
		Carved->SetNetAddressable();
		Carved->SetMobility(EComponentMobility::Static);
		Carved->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Carved->SetCanEverAffectNavigation(false);
		Carved->SetupAttachment(Root);
		int32 Section = 0;
		for (const TPair<TPair<UMaterialInterface*, int32>, FCarvedMesh>& Entry : CellEntry.Value)
		{
			const FCarvedMesh& Mesh = Entry.Value;
			Carved->CreateMeshSection(Section, Mesh.Vertices, Mesh.Triangles, Mesh.Normals, Mesh.UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
			Carved->SetMaterial(Section, Entry.Key.Key);
			if (Entry.Key.Value != 0)
			{
				// The temples keep a hint of their team: warm or cool stone (the trim lines carry the colour).
				if (UMaterialInstanceDynamic* MID = Carved->CreateDynamicMaterialInstance(Section))
				{
					MID->SetVectorParameterValue(TEXT("Tint"), Entry.Key.Value == 1 ? FLinearColor(1.02f, 0.86f, 0.8f) : FLinearColor(0.82f, 0.9f, 1.0f));
				}
			}
			++Section;
		}
		Carved->RegisterComponent();
		AddInstanceComponent(Carved);
		Decorations.Add(Carved);
	}

	auto Instanced = [this, bLumen](UStaticMesh* Mesh, bool bShadow)
	{
		auto* Instances = AddDecoration<UInstancedStaticMeshComponent>(TEXT("Scan"), Mesh);
		Instances->SetCastShadow(bShadow);
		Instances->bAffectDistanceFieldLighting = bLumen;
		return Instances;
	};
	TMap<UStaticMesh*, UInstancedStaticMeshComponent*> Groups;
	auto Place = [&](UStaticMesh* Mesh, const FTransform& Transform, bool bShadow = true, int32 CullDistance = 0, UMaterialInterface* Override = nullptr)
	{
		UInstancedStaticMeshComponent*& Instances = Groups.FindOrAdd(Mesh);
		if (!Instances)
		{
			Instances = Instanced(Mesh, bShadow);
			if (Override)
			{
				// The scanned rocks' own master material renders black in this project: they take the scanned stone
				// (their modelled shape is what matters; the world-space stone and moss match the structures).
				for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
				{
					Instances->SetMaterial(Slot, Override);
				}
			}
			if (CullDistance > 0)
			{
				// Small plants fade out past this distance (cm); they are noise from further away.
				Instances->SetCullDistances(CullDistance * 3 / 4, CullDistance);
			}
		}
		Instances->AddInstance(Transform);
	};

	// 2. Boulders over the rock cover, squared to the box (yaw in quarter turns) and a little proud of it.
	for (const int32 Index : RockBlocks)
	{
		const FShieldbreakMapBlock& Block = Layout.Blocks[Index];
		UStaticMesh* Mesh = Boulders[Random.RandRange(0, Boulders.Num() - 1)];
		const int32 Quarter = Random.RandRange(0, 3);
		const FBox Local = Mesh->GetBoundingBox();
		FVector Extent = Local.GetSize();
		if (Quarter % 2)
		{
			Swap(Extent.X, Extent.Y);
		}
		const FVector Size = Block.Max - Block.Min;
		const FVector Scale(Size.X * 1.12 / FMath::Max(Extent.X, 1.0), Size.Y * 1.12 / FMath::Max(Extent.Y, 1.0), Size.Z * 1.08 / FMath::Max(Extent.Z, 1.0));
		const FRotator Yaw(0.f, Quarter * 90.f, 0.f);
		FTransform Transform(Yaw, FVector::ZeroVector, FVector(Scale.X, Scale.Y, Scale.Z));
		if (Quarter % 2)
		{
			Transform.SetScale3D(FVector(Scale.Y, Scale.X, Scale.Z));
		}
		const FBox Placed = PlacedBounds(Mesh, Transform);
		const FVector Center = (Block.Min + Block.Max) * 0.5;
		Transform.SetTranslation(FVector(Center.X - Placed.GetCenter().X, Center.Y - Placed.GetCenter().Y, Block.Min.Z - 15.0 - Placed.Min.Z));
		Place(Mesh, Transform, true, 0, bRockOverride ? Dark : nullptr);
	}

	// 3. The boundary: a canyon. Each short side (a team's back) gets a Forerunner gate standing in the barrier line,
	// each long side two stepped terraces, and scanned cliffs fill everything else in two rows, the near row's inner
	// edge right on the barrier so no wall shows. Nothing reaches into the playable area.
	struct FHero { int32 Side; double Along; double HalfSpan; };
	TArray<FHero> Heroes;
	auto Kit = [](const TCHAR* Name) { return ShieldbreakArt::Load<UStaticMesh>(*FString::Printf(TEXT("/Game/Shieldbreak/Art/MapKit/SM_SB_Kit_%s.SM_SB_Kit_%s"), Name, Name)); };
	UStaticMesh* GateMesh = Kit(TEXT("Gate"));
	UStaticMesh* TerraceMesh = Kit(TEXT("Terrace"));
	UStaticMesh* ShardMesh = Kit(TEXT("Shard"));
	const FVector2D Half = Layout.HalfExtent;
	struct FSide { FVector Out; FVector Along; double Wall; double Length; };
	const FSide Sides[4] = {
		{ FVector(1, 0, 0), FVector(0, 1, 0), Half.X, Half.Y }, { FVector(-1, 0, 0), FVector(0, 1, 0), Half.X, Half.Y },
		{ FVector(0, 1, 0), FVector(1, 0, 0), Half.Y, Half.X }, { FVector(0, -1, 0), FVector(1, 0, 0), Half.Y, Half.X } };
	auto Facing = [](const FSide& Side, double Out, double Along, double Z)
	{
		// Kit pieces face +X: turn that to look back into the arena.
		const FRotator Yaw(0.f, static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(-Side.Out.Y, -Side.Out.X))), 0.f);
		return FTransform(Yaw, Side.Out * Out + Side.Along * Along + FVector(0, 0, Z));
	};
	auto KitMaterials = [&](UInstancedStaticMeshComponent* Instances, UStaticMesh* Mesh)
	{
		const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
		for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
		{
			const FString Name = Slots[Slot].MaterialSlotName.ToString();
			if (Name.StartsWith(TEXT("M_Ivory")))
			{
				Instances->SetMaterial(Slot, Stone);
			}
			else if (Name.StartsWith(TEXT("M_Sand")))
			{
				Instances->SetMaterial(Slot, Path);
			}
		}
	};
	auto PlaceHero = [&](UStaticMesh* Mesh, const FTransform& Transform)
	{
		const bool bNew = !Groups.Contains(Mesh);
		Place(Mesh, Transform);
		if (bNew)
		{
			KitMaterials(Groups[Mesh], Mesh);
		}
	};
	for (int32 SideIndex = 0; SideIndex < 4; ++SideIndex)
	{
		const FSide& Side = Sides[SideIndex];
		const bool bShort = Side.Out.X != 0.0;
		if (bShort && GateMesh)
		{
			// The gate's front face (local +X 500) on the barrier line; its plinth sunk into the outer ground.
			PlaceHero(GateMesh, Facing(Side, Side.Wall + 500.0, 0.0, Layout.FloorZ - 200.0));
			Heroes.Add({ SideIndex, 0.0, 1800.0 });
		}
		if (!bShort && TerraceMesh)
		{
			for (const double Along : { -0.55, 0.55 })
			{
				PlaceHero(TerraceMesh, Facing(Side, Side.Wall + 60.0, Along * Side.Length, Layout.FloorZ - 350.0));
				Heroes.Add({ SideIndex, Along * Side.Length, 2000.0 });
			}
		}
		if (!bShort && ShardMesh)
		{
			// A levitating obelisk far out past the middle of each long side: the skyline landmark.
			PlaceHero(ShardMesh, Facing(Side, Side.Wall + 4200.0, (SideIndex == 2 ? 0.2 : -0.2) * Side.Length, Layout.FloorZ - 350.0));
		}
	}

	// Angled crowns (the Blender Cap) on the tops of the tall blocks, high out of reach: towers, temple fins, the
	// tall stones. They break the flat box tops into Forerunner-style silhouettes. Visual only. -SBNoCaps.
	int32 CapCount = 0;
	if (UStaticMesh* CapMesh = FParse::Param(FCommandLine::Get(), TEXT("SBNoCaps")) ? nullptr : Kit(TEXT("Cap")))
	{
		for (const FShieldbreakMapBlock& Block : Layout.Blocks)
		{
			const FVector Size = Block.Max - Block.Min;
			const bool bGround = Block.Style == Floor || Block.Style == Sand || Block.Style == Grass || Block.Style == FName(TEXT("glow"));
			if (bGround || IsPerimeter(Layout, Block) || Block.Max.Z < Layout.FloorZ + 1050.0 || FMath::Max(Size.X, Size.Y) > 800.0
				|| FMath::Min(Size.X, Size.Y) < 150.0)
			{
				continue;
			}
			// The blades run along the cap's X: turn it so they follow the block's long side.
			const bool bAlongY = Size.Y > Size.X;
			const double Long = bAlongY ? Size.Y : Size.X, Short = bAlongY ? Size.X : Size.Y;
			const double Height = FMath::Clamp(Short * 1.1, 180.0, 360.0);
			const FVector Center = (Block.Min + Block.Max) * 0.5;
			PlaceHero(CapMesh, FTransform(FRotator(0.f, bAlongY ? 90.f : 0.f, 0.f), FVector(Center.X, Center.Y, Block.Max.Z - 4.0),
				FVector(Long / 100.0, Short / 100.0, Height / 100.0)));
			++CapCount;
		}
	}
	UE_LOG(LogShieldbreak, Log, TEXT("%s: %d angled caps."), *Layout.DisplayName, CapCount);

	// Buttress fins (the Blender Fin) leaning out from the upper part of the tall walls, every few metres along each
	// face, so the temples and towers read as angled Forerunner masonry instead of boxes. Visual only, so each fin
	// stays well above anything a player can stand on in front of it (3.3 m of clearance under its foot). -SBNoFins.
	int32 FinCount = 0;
	if (UStaticMesh* FinMesh = FParse::Param(FCommandLine::Get(), TEXT("SBNoFins")) ? nullptr : Kit(TEXT("Fin")))
	{
		for (const FShieldbreakMapBlock& Block : Layout.Blocks)
		{
			const FVector Size = Block.Max - Block.Min;
			const bool bGround = Block.Style == Floor || Block.Style == Sand || Block.Style == Grass || Block.Style == FName(TEXT("glow"));
			if (bGround || IsPerimeter(Layout, Block) || Size.Z < 500.0 || AltarFor(Layout, Block))
			{
				continue;
			}
			const FVector Center = (Block.Min + Block.Max) * 0.5;
			const FVector Normals[4] = { FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0) };
			for (const FVector& Normal : Normals)
			{
				const bool bAlongY = Normal.X != 0.0;
				const double FaceLength = bAlongY ? Size.Y : Size.X;
				const double Offset = bAlongY ? Size.X * 0.5 : Size.Y * 0.5;
				if (FaceLength < 380.0)
				{
					continue;
				}
				const double Height = FMath::Min(Size.Z * 0.55, 650.0);
				const double Foot = Block.Max.Z - Height;
				const int32 Count = FMath::Clamp(FMath::FloorToInt32(FaceLength / 450.0), 1, 8);
				const FVector Tangent = bAlongY ? FVector(0, 1, 0) : FVector(1, 0, 0);
				for (int32 Index = 0; Index < Count; ++Index)
				{
					const double Along = (Count == 1) ? 0.0 : -FaceLength * 0.5 + 120.0 + (FaceLength - 240.0) * Index / (Count - 1);
					const FVector At = FVector(Center.X, Center.Y, Foot) + Normal * Offset + Tangent * Along;
					// In front of the fin (it juts about 1.3 m): not inside another block, and nothing to stand on within 3.3 m below its foot.
					const FVector Probe = At + Normal * 90.0;
					if (InsideBlock(Layout, Probe + FVector(0, 0, 10.0)) || FMath::Abs(Probe.X) > Half.X || FMath::Abs(Probe.Y) > Half.Y)
					{
						continue;
					}
					double Stand = Layout.FloorZ;
					for (const FShieldbreakMapBlock& Other : Layout.Blocks)
					{
						if (Probe.X > Other.Min.X - 200.0 && Probe.X < Other.Max.X + 200.0 && Probe.Y > Other.Min.Y - 200.0 && Probe.Y < Other.Max.Y + 200.0
							&& Other.Max.Z <= Foot + 1.0 && &Other != &Block)
						{
							Stand = FMath::Max(Stand, static_cast<double>(Other.Max.Z));
						}
					}
					for (const FShieldbreakMapRamp& Slope : Layout.Ramps)
					{
						const FVector2D A(Slope.Start), B(Slope.End), P(Probe);
						const double T = FMath::Clamp(FVector2D::DotProduct(P - A, B - A) / FMath::Max((B - A).SizeSquared(), 1.0), 0.0, 1.0);
						if (FVector2D::Distance(P, A + (B - A) * T) < Slope.Width * 0.5 + 200.0)
						{
							Stand = FMath::Max(Stand, FMath::Max(Slope.Start.Z, Slope.End.Z));
						}
					}
					if (Foot - Stand < 330.0)
					{
						continue;
					}
					const FRotator Yaw(0.f, static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Normal.Y, Normal.X))), 0.f);
					PlaceHero(FinMesh, FTransform(Yaw, At - Normal * 4.0, FVector(3.0, 2.6, Height / 100.0)));
					++FinCount;
				}
			}
		}
	}
	UE_LOG(LogShieldbreak, Log, TEXT("%s: %d buttress fins."), *Layout.DisplayName, FinCount);

	// Angled cornices (the Blender Eave) along the top of the walls, in runs of up to 6 m: a steep chamfered lip just
	// under each roof edge, so the flat-topped temples get a Forerunner profile. The lip falls away from the roof line,
	// so it never reads as a ledge, and it is only placed where nothing stands near it (2.6 m clear under it, and no
	// block or ramp beside it at that height). Visual only. -SBNoEaves.
	int32 EaveCount = 0;
	if (UStaticMesh* EaveMesh = FParse::Param(FCommandLine::Get(), TEXT("SBNoEaves")) ? nullptr : Kit(TEXT("Eave")))
	{
		for (const FShieldbreakMapBlock& Block : Layout.Blocks)
		{
			const FVector Size = Block.Max - Block.Min;
			const bool bGround = Block.Style == Floor || Block.Style == Sand || Block.Style == Grass || Block.Style == FName(TEXT("glow"));
			if (bGround || IsPerimeter(Layout, Block) || Size.Z < 380.0 || AltarFor(Layout, Block))
			{
				continue;
			}
			const FVector Center = (Block.Min + Block.Max) * 0.5;
			const double Under = Block.Max.Z - 110.0;
			const FVector Normals[4] = { FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0) };
			for (const FVector& Normal : Normals)
			{
				const bool bAlongY = Normal.X != 0.0;
				const double FaceLength = bAlongY ? Size.Y : Size.X;
				const double Offset = bAlongY ? Size.X * 0.5 : Size.Y * 0.5;
				if (FaceLength < 300.0)
				{
					continue;
				}
				const int32 Runs = FMath::CeilToInt32(FaceLength / 600.0);
				const double Pitch = FaceLength / Runs;
				const FVector Tangent = bAlongY ? FVector(0, 1, 0) : FVector(1, 0, 0);
				for (int32 Index = 0; Index < Runs; ++Index)
				{
					const double Along = -FaceLength * 0.5 + Pitch * (Index + 0.5);
					const FVector At = FVector(Center.X, Center.Y, Block.Max.Z) + Normal * Offset + Tangent * Along;
					bool bClear = true;
					// Check the run's middle and both ends, 40 cm out.
					for (const double Step : { -0.5, 0.0, 0.5 })
					{
						const FVector Probe = At + Normal * 40.0 + Tangent * (Pitch - 40.0) * Step;
						if (FMath::Abs(Probe.X) > Half.X || FMath::Abs(Probe.Y) > Half.Y || InsideBlock(Layout, FVector(Probe.X, Probe.Y, Under)))
						{
							bClear = false;
							break;
						}
						for (const FShieldbreakMapBlock& Other : Layout.Blocks)
						{
							if (&Other != &Block && Probe.X > Other.Min.X - 150.0 && Probe.X < Other.Max.X + 150.0
								&& Probe.Y > Other.Min.Y - 150.0 && Probe.Y < Other.Max.Y + 150.0 && Other.Max.Z > Under - 260.0)
							{
								bClear = false;
								break;
							}
						}
						for (const FShieldbreakMapRamp& Slope : Layout.Ramps)
						{
							const FVector2D A(Slope.Start), B(Slope.End), P(Probe);
							const double T = FMath::Clamp(FVector2D::DotProduct(P - A, B - A) / FMath::Max((B - A).SizeSquared(), 1.0), 0.0, 1.0);
							if (FVector2D::Distance(P, A + (B - A) * T) < Slope.Width * 0.5 + 150.0
								&& FMath::Max(Slope.Start.Z, Slope.End.Z) > Under - 260.0)
							{
								bClear = false;
							}
						}
						if (!bClear)
						{
							break;
						}
					}
					if (!bClear || Under - Layout.FloorZ < 260.0)
					{
						continue;
					}
					const FRotator Yaw(0.f, static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Normal.Y, Normal.X))), 0.f);
					PlaceHero(EaveMesh, FTransform(Yaw, At - Normal * 2.0, FVector(1.0, (Pitch - 30.0) / 100.0, 1.0)));
					++EaveCount;
				}
			}
		}
	}
	UE_LOG(LogShieldbreak, Log, TEXT("%s: %d angled eaves."), *Layout.DisplayName, EaveCount);

	for (const int32 Index : AltarBlocks)
	{
		const FShieldbreakMapBlock& Block = Layout.Blocks[Index];
		const FVector Center = (Block.Min + Block.Max) * 0.5;
		PlaceHero(AltarFor(Layout, Block), FTransform(FVector(Center.X, Center.Y, Block.Min.Z)));
	}

	// Lamps under the roofs over spawns: the temple interiors are lit only by bounce, so the eye adapted to them
	// blew the sunlit courts outside to white. A soft unshadowed light under each roof evens that out.
	// -SBNoRoofLights turns them off.
	int32 RoofLights = 0;
	if (!FParse::Param(FCommandLine::Get(), TEXT("SBNoRoofLights")))
	{
		TArray<FVector> Lamps;
		for (const FVector& Feet : Layout.Spawns)
		{
			double Roof = TNumericLimits<double>::Max();
			for (const FShieldbreakMapBlock& Block : Layout.Blocks)
			{
				if (Feet.X > Block.Min.X && Feet.X < Block.Max.X && Feet.Y > Block.Min.Y && Feet.Y < Block.Max.Y
					&& Block.Min.Z >= Feet.Z + 220.0 && Block.Min.Z <= Feet.Z + 900.0)
				{
					Roof = FMath::Min(Roof, Block.Min.Z);
				}
			}
			const FVector At(Feet.X, Feet.Y, Roof - 80.0);
			if (Roof == TNumericLimits<double>::Max() || Lamps.ContainsByPredicate([&At](const FVector& Other) { return FVector::Dist(Other, At) < 800.0; }))
			{
				continue;
			}
			Lamps.Add(At);
			UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this);
			Lamp->SetupAttachment(Root);
			Lamp->SetMobility(EComponentMobility::Movable);
			Lamp->SetWorldLocation(At);
			Lamp->SetIntensityUnits(ELightUnits::Candelas);
			Lamp->SetIntensity(60.f);
			Lamp->SetAttenuationRadius(1500.f);
			Lamp->SetSourceRadius(150.f);
			Lamp->SetLightColor(FLinearColor(1.f, 0.92f, 0.82f));
			Lamp->SetCastShadows(false);
			Lamp->RegisterComponent();
			++RoofLights;
		}
	}
	UE_LOG(LogShieldbreak, Log, TEXT("%s: %d roof lights over spawns."), *Layout.DisplayName, RoofLights);

	TArray<UStaticMesh*> Cliffs = Family(TEXT("3D_Assets/MossyRockFace"), TEXT("SM_MossyRockFace_"), { 1, 2, 3 });
	Cliffs.Append(Family(TEXT("3D_Assets/ForestRockFormation"), TEXT("SM_ForestRockFormation_"), { 1, 2, 4, 5, 6 }));
	Cliffs.Append(Family(TEXT("3D_Assets/QuarryCliff"), TEXT("SM_QuarryCliff_"), { 1, 2 }));
	int32 CliffCount = 0;
	if (Cliffs.Num() > 0)
	{
		for (int32 SideIndex = 0; SideIndex < 4; ++SideIndex)
		{
			const FSide& Side = Sides[SideIndex];
			// Three rows: on the barrier, just behind it, and a far ring of giant crags that is the skyline.
			for (int32 Row = 0; Row < 3; ++Row)
			{
				const double Gap = Row == 0 ? 10.0 : Row == 1 ? 1500.0 : 6500.0;
				const double Reach = Side.Length + (Row == 0 ? 1800.0 : Row == 1 ? 3500.0 : 9000.0);
				double Along = -Reach;
				while (Along < Reach)
				{
					UStaticMesh* Mesh = Cliffs[Random.RandRange(0, Cliffs.Num() - 1)];
					const FVector Extent = Mesh->GetBoundingBox().GetSize();
					const double Height = Row == 0 ? Random.FRandRange(2000.f, 3000.f) : Row == 1 ? Random.FRandRange(3200.f, 5200.f)
						: Random.FRandRange(7000.f, 11000.f);
					// Fitted, not just scaled up: a set width along the barrier and its own height, so the near row is a
					// continuous rock face of many pieces rather than a few giant ones.
					const double Width = Row == 0 ? Random.FRandRange(1200.f, 2000.f) : Row == 1 ? Random.FRandRange(2200.f, 3500.f)
						: Random.FRandRange(5000.f, 8000.f);
					const double Flat = FMath::Clamp(Width / FMath::Max(FMath::Max(Extent.X, Extent.Y), 1.0), 0.5, 60.0);
					const double Tall = FMath::Clamp(Height / FMath::Max(Extent.Z, 1.0), 0.5, 60.0);
					FTransform Transform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector::ZeroVector, FVector(Flat, Flat, Tall));
					const FBox Placed = PlacedBounds(Mesh, Transform);
					const double Spread = FVector::DotProduct(Side.Along, Placed.GetSize());
					// The near row leaves the gates and terraces their stretch of the barrier.
					const double Middle = Along + Spread * 0.5;
					const bool bBlocked = Row == 0 && Heroes.ContainsByPredicate([&](const FHero& Hero)
					{
						return Hero.Side == SideIndex && FMath::Abs(Middle - Hero.Along) < Hero.HalfSpan + Spread * 0.3;
					});
					if (!bBlocked)
					{
						// Push out until the side of the rock that faces the arena sits Gap behind the barrier.
						const bool bPositive = Side.Out.X + Side.Out.Y > 0.0;
						const double Inner = FVector::DotProduct(Side.Out, bPositive ? Placed.Min : Placed.Max);
						const double Offset = Side.Wall + Gap - Inner;
						const FVector At = Side.Out * Offset + Side.Along * (Middle - FVector::DotProduct(Side.Along, Placed.GetCenter()));
						Transform.SetTranslation(FVector(At.X, At.Y, Layout.FloorZ - 350.0 - Placed.Min.Z));
						Place(Mesh, Transform, true, 0, bRockOverride ? Dark : nullptr);
						++CliffCount;
					}
					// The near row overlaps well so no gap shows the outside through it.
					Along += FMath::Max(Spread * (Row == 0 ? 0.45 : Row == 1 ? 0.62 : 0.7), 500.0);
				}
			}
		}
	}

	// 4. Trees rooted on the ground outside the walls, among the cliffs, tall enough for their crowns to show over the rim.
	const TArray<UStaticMesh*> Trees = Family(TEXT("3D_Plants/EuropeanHornbeam/Geometry/SimpleWind"), TEXT("SM_EuropeanHornbeam_Forest_"), { 1, 2, 3, 4, 5, 7, 8 });
	int32 TreeCount = 0;
	if (Trees.Num() > 0)
	{
		for (int32 Index = 0; Index < 90; ++Index)
		{
			const double Angle = Random.FRandRange(0.f, static_cast<float>(UE_TWO_PI));
			const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			// Out along the direction to just past the walls, then further.
			const double ToWall = FMath::Min(Layout.HalfExtent.X / FMath::Max(FMath::Abs(Dir.X), 1e-3), Layout.HalfExtent.Y / FMath::Max(FMath::Abs(Dir.Y), 1e-3));
			const double Distance = ToWall + Random.FRandRange(900.f, 4500.f);
			UStaticMesh* Mesh = Trees[Random.RandRange(0, Trees.Num() - 1)];
			const double Height = Random.FRandRange(2600.f, 4000.f);
			const double Scale = FMath::Clamp(Height / FMath::Max(Mesh->GetBoundingBox().GetSize().Z, 1.0), 0.3, 6.0);
			FTransform Transform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector::ZeroVector, FVector(Scale));
			const FBox Placed = PlacedBounds(Mesh, Transform);
			// The backdrop ground's top, where the cliffs stand too.
			Transform.SetTranslation(FVector(Dir.X * Distance, Dir.Y * Distance, Layout.FloorZ - 350.0 - Placed.Min.Z));
			Place(Mesh, Transform);
			++TreeCount;
		}
		// Brighter leaves (MakeScanArt's MI_SB_Leaves: more light through the leaf, lighter albedo); the stock leaf
		// material reads near black in the shade under Lumen.
		if (UMaterialInterface* Leaves = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Scan/MI_SB_Leaves.MI_SB_Leaves")))
		{
			for (UStaticMesh* Mesh : Trees)
			{
				if (UInstancedStaticMeshComponent** Instances = Groups.Find(Mesh))
				{
					// Out of the distance fields: a crown's own field shades its leaves near black under Lumen.
					(*Instances)->bAffectDistanceFieldLighting = false;
					const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
					for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
					{
						if (Slots[Slot].MaterialSlotName == FName(TEXT("TwoSided")))
						{
							(*Instances)->SetMaterial(Slot, Leaves);
						}
					}
				}
			}
		}
	}

	// 5. Long grass and loose stones at the foot of the walls and structures, where the ground meets the stone. (The
	// fern scans read as solid green mounds at this size, and the stones' own material as moss blobs: grass, and the
	// stones take the dark scanned stone.)
	// Kikuyu grass. -SBGroundCover tries the leafy ground cover instead (its Megascans foliage material renders near
	// black here, like the scanned rocks' own, so it is off by default).
	const bool bKikuyu = !FParse::Param(FCommandLine::Get(), TEXT("SBGroundCover"));
	TArray<UStaticMesh*> Plants = bKikuyu ? TArray<UStaticMesh*>() : Family(TEXT("3D_Plants/GroundCover"), TEXT("SM_GroundCover_"), { 1, 6, 7, 8 });
	if (Plants.Num() == 0)
	{
		Plants = Family(TEXT("3D_Plants/KikuyuGrass"), TEXT("SM_KikuyuGrass_"), { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 });
	}
	const TArray<UStaticMesh*> Stones = Family(TEXT("3D_Assets/SmallStonesPack"), TEXT("SM_SmallStonesPack_"), { 1, 2, 3, 4, 5, 6, 7 });
	int32 PlantCount = 0;
	if (Plants.Num() > 0)
	{
		for (const FShieldbreakMapBlock& Block : Layout.Blocks)
		{
			const FVector Size = Block.Max - Block.Min;
			if (Size.Z < 120.0 || Block.Style == Floor || Block.Style == FName(TEXT("glow")) || IsPerimeter(Layout, Block))
			{
				continue;
			}
			const FVector Center = (Block.Min + Block.Max) * 0.5;
			const FVector Normals[4] = { FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0) };
			for (const FVector& Normal : Normals)
			{
				const bool bAlongY = Normal.X != 0.0;
				const double FaceLength = bAlongY ? Size.Y : Size.X;
				const double Offset = bAlongY ? Size.X * 0.5 : Size.Y * 0.5;
				for (double Along = -FaceLength * 0.5 + 60.0; Along < FaceLength * 0.5 - 60.0; Along += 170.0)
				{
					if (Random.FRand() > 0.6f)
					{
						continue;
					}
					const FVector Tangent = bAlongY ? FVector(0, 1, 0) : FVector(1, 0, 0);
					const FVector Foot = FVector(Center.X, Center.Y, Block.Min.Z) + Normal * (Offset + Random.FRandRange(35.f, 85.f)) + Tangent * (Along + Random.FRandRange(-50.f, 50.f));
					if (FMath::Abs(Foot.X) > Half.X - 30.0 || FMath::Abs(Foot.Y) > Half.Y - 30.0)
					{
						continue;
					}
					const FVector Probe(Foot.X, Foot.Y, Foot.Z + 20.0);
					if (InsideBlock(Layout, Probe) || !(OnSurface(Layout, Foot) || FMath::Abs(Foot.Z - Layout.FloorZ) < 5.0))
					{
						continue;
					}
					const bool bStone = Stones.Num() > 0 && Random.FRand() < 0.25f;
					UStaticMesh* Mesh = bStone ? Stones[Random.RandRange(0, Stones.Num() - 1)] : Plants[Random.RandRange(0, Plants.Num() - 1)];
					const FVector Extent = Mesh->GetBoundingBox().GetSize();
					const double Target = bStone ? Random.FRandRange(22.f, 48.f) : Random.FRandRange(45.f, 75.f);
					const double Scale = FMath::Clamp(Target / FMath::Max(bStone ? FMath::Max(Extent.X, Extent.Y) : Extent.Z, 1.0), 0.05, 8.0);
					FTransform Transform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector::ZeroVector, FVector(Scale));
					const FBox Placed = PlacedBounds(Mesh, Transform);
					Transform.SetTranslation(FVector(Foot.X, Foot.Y, Foot.Z - Placed.Min.Z - (bStone ? 6.0 : 7.0)));
					Place(Mesh, Transform, false, bStone ? 0 : 6000, bStone ? Dark : nullptr);
					++PlantCount;
				}
			}
		}
	}

	// 6. Ivy hanging over the tops of the taller walls and structures, on faces that open onto the map.
	TArray<UStaticMesh*> Ivy = Family(TEXT("3D_Plants/Ivy"), TEXT("SM_Ivy_"), { 9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 21, 22, 23, 24, 25 });
	int32 IvyCount = 0;
	if (Ivy.Num() > 0)
	{
		for (const FShieldbreakMapBlock& Block : Layout.Blocks)
		{
			const FVector Size = Block.Max - Block.Min;
			if (Size.Z < 300.0 || Block.Style == Floor || Block.Style == FName(TEXT("glow")) || Block.Style == RockStyle || IsPerimeter(Layout, Block))
			{
				continue;
			}
			const FVector Center = (Block.Min + Block.Max) * 0.5;
			const FVector Normals[4] = { FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0) };
			for (const FVector& Normal : Normals)
			{
				const bool bAlongY = Normal.X != 0.0;
				const double FaceLength = bAlongY ? Size.Y : Size.X;
				const double Offset = bAlongY ? Size.X * 0.5 : Size.Y * 0.5;
				const FVector Tangent = bAlongY ? FVector(0, 1, 0) : FVector(1, 0, 0);
				for (double Along = -FaceLength * 0.5 + 80.0; Along < FaceLength * 0.5 - 80.0; Along += 230.0)
				{
					if (Random.FRand() > 0.38f)
					{
						continue;
					}
					const FVector Lip = FVector(Center.X, Center.Y, Block.Max.Z - 4.0) + Normal * (Offset + 2.0) + Tangent * (Along + Random.FRandRange(-60.f, 60.f));
					// Only faces that open onto the map: not outside the walls, not pressed against another block.
					if (FMath::Abs(Lip.X) > Half.X - 20.0 || FMath::Abs(Lip.Y) > Half.Y - 20.0
						|| InsideBlock(Layout, Lip + Normal * 40.0 - FVector(0, 0, 60.0)))
					{
						continue;
					}
					UStaticMesh* Mesh = Ivy[Random.RandRange(0, Ivy.Num() - 1)];
					const double Length = FMath::Max(Mesh->GetBoundingBox().GetSize().Z, 1.0);
					const double Scale = FMath::Min(Random.FRandRange(0.8f, 1.4f), (Size.Z - 40.0) / Length);
					// The ivy's leaves grow toward its +Y: turn that to face out of the wall.
					const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Normal.X, Normal.Y));
					FTransform Transform(FRotator(0.f, static_cast<float>(Yaw), 0.f), Lip, FVector(Scale));
					Place(Mesh, Transform, false, 9000);
					++IvyCount;
				}
			}
		}
	}

	// 7. Grass in drifts across the ground, short enough not to hide a crouching player.
	TArray<UStaticMesh*> GrassMeshes = Plants;
	int32 GrassCount = 0;
	if (GrassMeshes.Num() > 0)
	{
		const int32 Drifts = FMath::Clamp(static_cast<int32>(Half.X * Half.Y / 250000.0), 40, 160);
		for (int32 Drift = 0; Drift < Drifts; ++Drift)
		{
			const FVector2D Middle(Random.FRandRange(-Half.X, Half.X), Random.FRandRange(-Half.Y, Half.Y));
			const int32 Tufts = Random.RandRange(12, 22);
			for (int32 Tuft = 0; Tuft < Tufts; ++Tuft)
			{
				const FVector2D At = Middle + FVector2D(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * Random.FRandRange(40.f, 320.f);
				if (FMath::Abs(At.X) > Half.X - 40.0 || FMath::Abs(At.Y) > Half.Y - 40.0)
				{
					continue;
				}
				// The ground here: the highest floor, sand or grass top under the point (Sanctum's courts are raised).
				double GroundZ = Layout.FloorZ;
				for (const FShieldbreakMapBlock& Block : Layout.Blocks)
				{
					if ((Block.Style == Floor || Block.Style == Sand || Block.Style == Grass) && At.X > Block.Min.X && At.X < Block.Max.X
						&& At.Y > Block.Min.Y && At.Y < Block.Max.Y)
					{
						GroundZ = FMath::Max(GroundZ, Block.Max.Z);
					}
				}
				const FVector Foot(At.X, At.Y, GroundZ);
				if (InsideBlock(Layout, Foot + FVector(0, 0, 20.0)))
				{
					continue;
				}
				UStaticMesh* Mesh = GrassMeshes[Random.RandRange(0, GrassMeshes.Num() - 1)];
				const double Height = FMath::Max(Mesh->GetBoundingBox().GetSize().Z, 1.0);
				const double Scale = FMath::Clamp(Random.FRandRange(25.f, 45.f) / Height, 0.2, 3.0);
				FTransform Transform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector::ZeroVector, FVector(Scale));
				const FBox Placed = PlacedBounds(Mesh, Transform);
				Transform.SetTranslation(FVector(At.X, At.Y, GroundZ - Placed.Min.Z - 7.0));
				Place(Mesh, Transform, false, 5000);
				++GrassCount;
			}
		}
	}

	// The grass tufts (sections 5 and 7): out of the distance fields, which shade them near black under Lumen like
	// the tree crowns, and on MakeScanArt's lighter MI_SB_Grass.
	{
		// MI_SB_Grass is a Kikuyu instance: only for those meshes.
		const bool bKikuyuMeshes = GrassMeshes.Num() > 0 && GrassMeshes[0]->GetName().StartsWith(TEXT("SM_KikuyuGrass_"));
		// M_SB_Grass (our own masked material on the grass's albedo) when MakeScanArt has built it: the Megascans
		// foliage master renders the tufts near black. -SBGrassMI keeps the instance of that master.
		UMaterialInterface* GrassSurface = nullptr;
		if (bKikuyuMeshes)
		{
			if (!FParse::Param(FCommandLine::Get(), TEXT("SBGrassMI")))
			{
				GrassSurface = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Scan/M_SB_Grass.M_SB_Grass"));
			}
			if (!GrassSurface)
			{
				GrassSurface = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Scan/MI_SB_Grass.MI_SB_Grass"));
			}
		}
		TSet<UStaticMesh*> GrassSet(GrassMeshes);
		GrassSet.Append(Plants);
		for (UStaticMesh* Mesh : GrassSet)
		{
			if (UInstancedStaticMeshComponent** Instances = Groups.Find(Mesh))
			{
				(*Instances)->bAffectDistanceFieldLighting = false;
				for (int32 Slot = 0; GrassSurface && Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
				{
					(*Instances)->SetMaterial(Slot, GrassSurface);
				}
			}
		}
	}

	// 8. Old paving: worn stone slabs round the middle of the map, thinning out toward the edges with grass between
	// and gaps where slabs are missing. Visual only and 5 cm proud, so movement is untouched.
	int32 SlabCount = 0;
	if (CubeMesh)
	{
		UInstancedStaticMeshComponent* Slabs = AddDecoration<UInstancedStaticMeshComponent>(TEXT("Scan"), CubeMesh);
		Slabs->SetCastShadow(false);
		Slabs->bAffectDistanceFieldLighting = false;
		Slabs->SetMaterial(0, Path);
		constexpr double Tile = 190.0, Joint = 14.0;
		const double Radius = FMath::Min(Half.X, Half.Y) * 0.75;
		for (double X = -Half.X + Tile; X < Half.X - Tile; X += Tile)
		{
			for (double Y = -Half.Y + Tile; Y < Half.Y - Tile; Y += Tile)
			{
				// Denser near the middle, broken up by a coarse pattern of patches so it reads as worn, not tiled.
				const double Falloff = 1.0 - FVector2D(X / Half.X, Y / Half.Y).Size() * 0.85;
				const double Patch = FMath::PerlinNoise2D(FVector2D(X, Y) / 1700.0) * 0.9;
				if (Falloff + Patch < 0.45 || Random.FRand() < 0.12f)
				{
					continue;
				}
				double GroundZ = Layout.FloorZ;
				for (const FShieldbreakMapBlock& Block : Layout.Blocks)
				{
					if ((Block.Style == Floor || Block.Style == Sand || Block.Style == Grass) && X > Block.Min.X && X < Block.Max.X
						&& Y > Block.Min.Y && Y < Block.Max.Y)
					{
						GroundZ = FMath::Max(GroundZ, Block.Max.Z);
					}
				}
				// Clear of every block's footprint (with a margin), so slabs never poke out of a wall's foot.
				const FVector Probe(X, Y, GroundZ + 20.0);
				bool bClear = true;
				for (const FShieldbreakMapBlock& Block : Layout.Blocks)
				{
					if (Probe.Z > Block.Min.Z && Probe.Z < Block.Max.Z + 30.0 && X > Block.Min.X - Tile && X < Block.Max.X + Tile
						&& Y > Block.Min.Y - Tile && Y < Block.Max.Y + Tile && Block.Style != Floor && Block.Style != Sand && Block.Style != Grass)
					{
						bClear = false;
						break;
					}
				}
				if (!bClear)
				{
					continue;
				}
				const double Rise = Random.FRandRange(1.5f, 3.5f);
				const FVector Size(Tile - Joint, Tile - Joint, Rise + 10.0);
				const FRotator Tilt(Random.FRandRange(-0.3f, 0.3f), Random.FRandRange(-2.f, 2.f), Random.FRandRange(-0.3f, 0.3f));
				Slabs->AddInstance(FTransform(Tilt, FVector(X, Y, GroundZ + Rise - Size.Z * 0.5), Size / 100.0));
				++SlabCount;
			}
		}
		Slabs->RegisterComponent();
		AddInstanceComponent(Slabs);
	}

	for (const TPair<UStaticMesh*, UInstancedStaticMeshComponent*>& Group : Groups)
	{
		Group.Value->RegisterComponent();
		AddInstanceComponent(Group.Value);
	}
	UE_LOG(LogShieldbreak, Log, TEXT("%s scanned art: %d carved blocks, %d rock cover boulders, %d cliffs, %d trees, %d plants and stones, %d ivy, %d grass, %d paving slabs, %d meshes."),
		*Layout.DisplayName, CarvedCount, RockBlocks.Num(), CliffCount, TreeCount, PlantCount, IvyCount, GrassCount, SlabCount, Groups.Num());
}

// ---- Scanned terrain (Creekbed, Bastion) ----

namespace ShieldbreakTerrainScanPrivate
{
	/** A family of Megascans meshes in 3D_Assets/<Folder>, named SM_<Folder>_01 .. SM_<Folder>_<Count> (missing ones skipped). */
	struct FFamily { const TCHAR* Folder; int32 Count; };

	/** What a scan set draws: rock fitted over the rock blocks, the cliff rows round the map, and stones at block feet. */
	struct FTerrainSet
	{
		TArray<FFamily> Rocks;
		TArray<FFamily> Cliffs;
		TArray<FFamily> Giants;
		/** The perimeter walls give way to the canyon (hidden, still the barrier), or stay (Bastion's fortress walls). */
		bool bHidePerimeter = true;
		/** Cliff rows: 0 on the barrier, 1 just behind, 2 the far skyline. */
		bool bRow0 = true;
		bool bStones = true;
		/** Lush (Creekbed): grass drifts on the ground, trees along the rim, a running creek and a waterfall. */
		bool bLush = false;
		/** The skyline row of giant crags (row 2). */
		bool bGiants = true;
		/** Stone slabs up to 4 m tall take the ground material (a walkable snowfield, not quarry stone). */
		bool bStoneSlabsAsGround = false;
	};

	const FTerrainSet* SetFor(FName Set)
	{
		static const TMap<FName, FTerrainSet> Sets = []
		{
			TMap<FName, FTerrainSet> Out;
			FTerrainSet& Canyon = Out.Add(FName(TEXT("Canyon")));
			Canyon.Rocks = { { TEXT("SandstoneBoulder"), 10 }, { TEXT("CanyonSandstoneRidge"), 6 } };
			Canyon.Cliffs = { { TEXT("HugeSandstoneCliff"), 6 }, { TEXT("CanyonSandstoneRidge"), 6 },
				{ TEXT("MassiveSandstoneCliff"), 6 } };
			Canyon.Giants = { { TEXT("MassiveCanyonSandstoneMesa"), 6 }, { TEXT("MassiveSandstoneCliff"), 6 } };
			// Creek: the canyon's sandstone gone green (a creek-fed valley, grass on the ground and on the rock tops).
			// The ridge scans are 1-3 m pieces: boulders only, never stretched into cliffs.
			FTerrainSet& Creek = Out.Add(FName(TEXT("Creek")));
			Creek.Rocks = Canyon.Rocks;
			Creek.Cliffs = { { TEXT("HugeSandstoneCliff"), 6 }, { TEXT("MassiveSandstoneCliff"), 6 } };
			// No skyline row: at 70-110 m the sandstone cliffs tore into floating flakes over the canyon (R19 isolated it to
			// row 2), and the row cost 8 FPS. The trees and the two near rows close the valley on their own.
			Creek.bGiants = false;
			Creek.bLush = true;
			FTerrainSet& Tundra = Out.Add(FName(TEXT("Tundra")));
			Tundra.Rocks = { { TEXT("NordicBoulder"), 8 }, { TEXT("TundraMossyBoulder"), 8 },
				{ TEXT("NordicBeachRock"), 6 } };
			Tundra.Cliffs = { { TEXT("MassiveTundraRockFormation"), 6 }, { TEXT("HugeNordicCoastalCliff"), 6 } };
			Tundra.Giants = { { TEXT("MassiveNordicCoastalCliff"), 6 }, { TEXT("MassiveTundraRockFormation"), 6 } };
			Tundra.bHidePerimeter = false;
			Tundra.bRow0 = false;
			Tundra.bStoneSlabsAsGround = !FParse::Param(FCommandLine::Get(), TEXT("SBStoneSlabs"));
			return Out;
		}();
		return Sets.Find(Set);
	}

	// Some scans carry extra non-opaque slots (dust and debris cards). With the
	// set's opaque rock forced on every slot they rendered as torn, floating streaks in front of the sky (R13, R14).
	// Those scans stay out; -SBAllTerrainScans keeps them for comparison. Each scan is logged once.
	bool UsableScan(UStaticMesh* Mesh)
	{
		const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
		FString Detail;
		bool bUsable = true;
		for (const FStaticMaterial& Slot : Slots)
		{
			const EBlendMode Blend = Slot.MaterialInterface ? Slot.MaterialInterface->GetBlendMode() : BLEND_Opaque;
			Detail += FString::Printf(TEXT(" [%s %s %d]"), *Slot.MaterialSlotName.ToString(),
				Slot.MaterialInterface ? *Slot.MaterialInterface->GetName() : TEXT("none"), static_cast<int32>(Blend));
			bUsable &= Blend == BLEND_Opaque;
		}
		bUsable &= Slots.Num() <= 1;
		const FVector Size = Mesh->GetBoundingBox().GetSize();
		// SM_MassiveSandstoneCliff_06 is a 220 m vista piece with no material of its own: not a cliff to place.
		bUsable &= Size.GetMax() < 10000.0;
		UE_LOG(LogShieldbreak, Log, TEXT("terrain scan %s: %s, %d slots, %.0fx%.0fx%.0f cm,%s"), *Mesh->GetName(),
			bUsable ? TEXT("used") : TEXT("SKIPPED"), Slots.Num(), Size.X, Size.Y, Size.Z, *Detail);
		return bUsable || FParse::Param(FCommandLine::Get(), TEXT("SBAllTerrainScans"));
	}

	TArray<UStaticMesh*> LoadFamilies(const TArray<FFamily>& Families)
	{
		TArray<UStaticMesh*> Out;
		for (const FFamily& Family : Families)
		{
			for (int32 Number = 1; Number <= Family.Count; ++Number)
			{
				if (UStaticMesh* Found = ShieldbreakScanPrivate::Mesh(*FString::Printf(TEXT("3D_Assets/%s"), Family.Folder),
					*FString::Printf(TEXT("SM_%s_%02d"), Family.Folder, Number)))
				{
					if (UsableScan(Found))
					{
						Out.Add(Found);
					}
				}
			}
		}
		return Out;
	}

	UMaterialInterface* SetMaterial(FName Set, const TCHAR* Kind)
	{
		const FString Name = FString::Printf(TEXT("MI_SB_Scan_%s%s"), *Set.ToString(), Kind);
		return ShieldbreakArt::Load<UMaterialInterface>(*FString::Printf(TEXT("/Game/Shieldbreak/Art/Scan/%s.%s"), *Name, *Name));
	}
}

bool ShieldbreakMapArt::TerrainScanReady(FName MapId)
{
	const FShieldbreakMapLook& Look = GetLook(MapId);
	if (Look.ScanSet.IsNone() || FParse::Param(FCommandLine::Get(), TEXT("SBNoTerrainScan")) || IsRunningDedicatedServer())
	{
		return false;
	}
	return ShieldbreakTerrainScanPrivate::SetFor(Look.ScanSet) && ShieldbreakTerrainScanPrivate::SetMaterial(Look.ScanSet, TEXT("Ground"))
		&& ShieldbreakTerrainScanPrivate::SetMaterial(Look.ScanSet, TEXT("Rock"));
}

void AShieldbreakArena::BuildTerrainScan()
{
	using namespace ShieldbreakScanPrivate;
	using namespace ShieldbreakTerrainScanPrivate;
	if (GetNetMode() == NM_DedicatedServer || !ShieldbreakMapArt::TerrainScanReady(MapId))
	{
		return;
	}
	const FShieldbreakMapLook& Look = ShieldbreakMapArt::GetLook(MapId);
	const FTerrainSet& Set = *SetFor(Look.ScanSet);
	UMaterialInterface* Ground = SetMaterial(Look.ScanSet, TEXT("Ground"));
	UMaterialInterface* Rock = SetMaterial(Look.ScanSet, TEXT("Rock"));
	UMaterialInterface* Stone = SetMaterial(Look.ScanSet, TEXT("Stone"));
	const bool bLumen = ShieldbreakArenaPrivate::UsesLumen();
	FRandomStream Random(static_cast<int32>(GetTypeHash(MapId.ToString())) ^ 0x7E44);
	const TArray<UStaticMesh*> Rocks = LoadFamilies(Set.Rocks);
	const TArray<UStaticMesh*> Cliffs = LoadFamilies(Set.Cliffs);
	const TArray<UStaticMesh*> Giants = LoadFamilies(Set.Giants);

	// Grouped by mesh AND shadow casting: Canyon uses CanyonSandstoneRidge both as fitted rock (shadowed) and as
	// cliff (unshadowed); keyed by mesh alone, the first rock turned every ridge cliff into a shadow caster
	// (R14: Nanite shadows 1.4 ms, Creekbed 123 FPS against 179 without the scans).
	TMap<TTuple<UStaticMesh*, bool>, UInstancedStaticMeshComponent*> Groups;
	auto Place = [&](UStaticMesh* Mesh, const FTransform& Transform, bool bShadow, int32 CullDistance = 0)
	{
		UInstancedStaticMeshComponent*& Instances = Groups.FindOrAdd(MakeTuple(Mesh, bShadow));
		if (!Instances)
		{
			Instances = AddDecoration<UInstancedStaticMeshComponent>(TEXT("Terrain"), Mesh);
			Instances->SetCastShadow(bShadow);
			Instances->bAffectDistanceFieldLighting = bLumen;
			// The scans' own master material renders black in this project: the set's world-projected rock instead.
			for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				Instances->SetMaterial(Slot, Rock);
			}
			if (CullDistance > 0)
			{
				Instances->SetCullDistances(CullDistance * 3 / 4, CullDistance);
			}
		}
		Instances->AddInstance(Transform);
	};

	// 1. Surfaces: scanned ground underfoot (floor, sand banks, the creek's slopes), scanned rock on the rock blocks
	// (hidden under their fitted rock below, as on Sanctum: drawn, the flat box faces showed through everywhere),
	// scanned stone on the stone.
	static const FName Floor(TEXT("floor")), Sand(TEXT("sand")), RockStyle(TEXT("rock")), StoneStyle(TEXT("stone"));
	const int32 NumBlocks = Layout.Blocks.Num();
	TArray<int32> RockBlocks;
	int32 Surfaced = 0;
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		UStaticMeshComponent* Piece = Pieces[Index];
		const bool bBlock = Index < NumBlocks;
		const FName Style = bBlock ? Layout.Blocks[Index].Style
			: Layout.Ramps.IsValidIndex(Index - NumBlocks) ? Layout.Ramps[Index - NumBlocks].Style : NAME_None;
		if (!Piece)
		{
			continue;
		}
		if (bBlock && Set.bHidePerimeter && IsPerimeter(Layout, Layout.Blocks[Index]))
		{
			Piece->bAffectIndirectLightingWhileHidden = true;
			Piece->SetVisibility(false);
			continue;
		}
		// Low stone slabs are ground to walk on (Bastion's plateau): the set's ground, which on the tundra carries the
		// snow the pit floor shows; the quarry-stone instance stays on the taller stone (walls, plinths).
		const bool bLowSlab = bBlock && Style == StoneStyle && Set.bStoneSlabsAsGround
			&& Layout.Blocks[Index].Max.Z - Layout.Blocks[Index].Min.Z <= 400.0;
		UMaterialInterface* Surface = (Style == Floor || Style == Sand || bLowSlab) ? Ground
			: Style == RockStyle ? (bBlock ? Rock : Ground)
			: (Style == StoneStyle && Stone) ? Stone : nullptr;
		if (!Surface)
		{
			continue;
		}
		Piece->SetMaterial(0, Surface);
		++Surfaced;
		if (bBlock && Style == RockStyle)
		{
			RockBlocks.Add(Index);
		}
	}
	if (Rocks.Num() > 0)
	{
		for (const int32 Index : RockBlocks)
		{
			Pieces[Index]->bAffectIndirectLightingWhileHidden = true;
			Pieces[Index]->SetVisibility(false);
		}
	}

	// 2. Scanned rock fitted over each rock block. Long blocks are split into pieces no more than twice as long as
	// they are deep, each fitted to the scan whose proportions match best, so a mesa reads as a run of rock, not one
	// stretched boulder. The fit stands 30 cm proud of the box at most, so cover and footing stay where they look.
	int32 RockCount = 0;
	if (Rocks.Num() > 0)
	{
		for (const int32 Index : RockBlocks)
		{
			const FShieldbreakMapBlock& Block = Layout.Blocks[Index];
			const FVector Size = Block.Max - Block.Min;
			const bool bAlongX = Size.X >= Size.Y;
			const double Long = bAlongX ? Size.X : Size.Y, Deep = bAlongX ? Size.Y : Size.X;
			const int32 Count = FMath::Clamp(FMath::CeilToInt32(Long / FMath::Max(Deep * 2.0, 300.0)), 1, 12);
			const double Step = Long / Count;
			for (int32 Part = 0; Part < Count; ++Part)
			{
				const double PartLong = Step + (Count > 1 ? 80.0 : 0.0);
				const FVector PartSize = bAlongX ? FVector(PartLong, Size.Y, Size.Z) : FVector(Size.X, PartLong, Size.Z);
				FVector Center = (Block.Min + Block.Max) * 0.5;
				(bAlongX ? Center.X : Center.Y) = (bAlongX ? Block.Min.X : Block.Min.Y) + Step * (Part + 0.5);
				// Best proportions (footprint aspect and height to depth), from a random two of the best three.
				const double Aspect = FMath::Max(PartSize.X, PartSize.Y) / FMath::Max(FMath::Min(PartSize.X, PartSize.Y), 1.0);
				const double Rise = PartSize.Z / FMath::Max(FMath::Min(PartSize.X, PartSize.Y), 1.0);
				TArray<TPair<double, UStaticMesh*>> Ranked;
				for (UStaticMesh* Mesh : Rocks)
				{
					const FVector E = Mesh->GetBoundingBox().GetSize();
					const double MA = FMath::Max(E.X, E.Y) / FMath::Max(FMath::Min(E.X, E.Y), 1.0);
					const double MR = E.Z / FMath::Max(FMath::Min(E.X, E.Y), 1.0);
					Ranked.Add({ FMath::Abs(FMath::Loge(MA / Aspect)) + 0.6 * FMath::Abs(FMath::Loge(FMath::Max(MR, 0.05) / FMath::Max(Rise, 0.05))), Mesh });
				}
				Ranked.Sort([](const TPair<double, UStaticMesh*>& A, const TPair<double, UStaticMesh*>& B) { return A.Key < B.Key; });
				UStaticMesh* Mesh = Ranked[Random.RandRange(0, FMath::Min(2, Ranked.Num() - 1))].Value;
				const FVector E = Mesh->GetBoundingBox().GetSize();
				// Turn the mesh so its long side follows the part's long side (and sometimes flip it end for end).
				const bool bMeshAlongX = E.X >= E.Y;
				const bool bPartAlongX = PartSize.X >= PartSize.Y;
				const float Yaw = (bMeshAlongX == bPartAlongX ? 0.f : 90.f) + (Random.FRand() < 0.5f ? 180.f : 0.f);
				const FVector Turned = FMath::IsNearlyEqual(FMath::Fmod(Yaw, 180.f), 90.f) ? FVector(E.Y, E.X, E.Z) : E;
				// A scan is rounder than its bounds: 18% over the box (at most 1.2 m a side) so it wraps the collision.
				const FVector Fit((PartSize.X + FMath::Min(PartSize.X * 0.18, 240.0)) / FMath::Max(Turned.X, 1.0),
					(PartSize.Y + FMath::Min(PartSize.Y * 0.18, 240.0)) / FMath::Max(Turned.Y, 1.0), (PartSize.Z * 1.08 + 20.0) / FMath::Max(Turned.Z, 1.0));
				const FVector Scale = FMath::IsNearlyEqual(FMath::Fmod(Yaw, 180.f), 90.f) ? FVector(Fit.Y, Fit.X, Fit.Z) : Fit;
				FTransform Transform(FRotator(0.f, Yaw, 0.f), FVector::ZeroVector, Scale);
				const FBox Placed = PlacedBounds(Mesh, Transform);
				Transform.SetTranslation(FVector(Center.X - Placed.GetCenter().X, Center.Y - Placed.GetCenter().Y, Block.Min.Z - 15.0 - Placed.Min.Z));
				Place(Mesh, Transform, true);
				++RockCount;
			}
		}
	}

	// 3. Stones at the feet of the rock blocks, where the rock meets the ground. Low (under 45 cm), so never cover.
	const TArray<UStaticMesh*> Stones = Family(TEXT("3D_Assets/SmallStonesPack"), TEXT("SM_SmallStonesPack_"), { 1, 2, 3, 4, 5, 6, 7 });
	const FVector2D Half = Layout.HalfExtent;
	int32 StoneCount = 0;
	if (Set.bStones && Stones.Num() > 0 && !FParse::Param(FCommandLine::Get(), TEXT("SBNoStones"))) // -SBNoStones: diagnosis
	{
		for (const int32 Index : RockBlocks)
		{
			const FShieldbreakMapBlock& Block = Layout.Blocks[Index];
			const FVector Size = Block.Max - Block.Min;
			const FVector Center = (Block.Min + Block.Max) * 0.5;
			const FVector Normals[4] = { FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0) };
			for (const FVector& Normal : Normals)
			{
				const bool bAlongY = Normal.X != 0.0;
				const double FaceLength = bAlongY ? Size.Y : Size.X;
				const double Offset = bAlongY ? Size.X * 0.5 : Size.Y * 0.5;
				const FVector Tangent = bAlongY ? FVector(0, 1, 0) : FVector(1, 0, 0);
				for (double Along = -FaceLength * 0.5 + 60.0; Along < FaceLength * 0.5 - 60.0; Along += 220.0)
				{
					if (Random.FRand() > 0.45f)
					{
						continue;
					}
					const FVector Foot = FVector(Center.X, Center.Y, Block.Min.Z) + Normal * (Offset + Random.FRandRange(40.f, 90.f)) + Tangent * (Along + Random.FRandRange(-60.f, 60.f));
					if (FMath::Abs(Foot.X) > Half.X - 30.0 || FMath::Abs(Foot.Y) > Half.Y - 30.0)
					{
						continue;
					}
					if (InsideBlock(Layout, Foot + FVector(0, 0, 20.0)) || !(OnSurface(Layout, Foot) || FMath::Abs(Foot.Z - Layout.FloorZ) < 5.0))
					{
						continue;
					}
					UStaticMesh* Mesh = Stones[Random.RandRange(0, Stones.Num() - 1)];
					const FVector Extent = Mesh->GetBoundingBox().GetSize();
					const double Scale = FMath::Clamp(Random.FRandRange(25.f, 55.f) / FMath::Max(FMath::Max(Extent.X, Extent.Y), 1.0), 0.05, 8.0);
					FTransform Transform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector::ZeroVector, FVector(Scale));
					const FBox Placed = PlacedBounds(Mesh, Transform);
					if (Placed.GetSize().Z > 45.0)
					{
						continue;
					}
					Transform.SetTranslation(FVector(Foot.X, Foot.Y, Foot.Z - Placed.Min.Z - 6.0));
					Place(Mesh, Transform, false, 6000);
					++StoneCount;
				}
			}
		}
	}

	// 4. Cliffs round the map: rows on the barrier (the canyon walls), just behind it, and giant crags for the skyline.
	// The landmark side (the arch, the watchtower) leaves its middle open in the near rows so the landmark shows.
	// The cliffs stand beyond the walls; Lumen lights them, so they cast no sun shadows (see below).
	int32 CliffCount = 0;
	struct FSide { FVector Out; FVector Along; double Wall; double Length; };
	const FSide Sides[4] = {
		{ FVector(1, 0, 0), FVector(0, 1, 0), Half.X, Half.Y }, { FVector(-1, 0, 0), FVector(0, 1, 0), Half.X, Half.Y },
		{ FVector(0, 1, 0), FVector(1, 0, 0), Half.Y, Half.X }, { FVector(0, -1, 0), FVector(1, 0, 0), Half.Y, Half.X } };
	const double BackdropZ = Layout.FloorZ - 400.0;
	int32 RowMask = 7; // -SBTerrainRows=<bits>: which cliff rows to place (1 barrier, 2 behind it, 4 giants), for diagnosis
	FParse::Value(FCommandLine::Get(), TEXT("SBTerrainRows="), RowMask);
	for (int32 SideIndex = 0; SideIndex < 4; ++SideIndex)
	{
		const FSide& Side = Sides[SideIndex];
		// Creekbed's arch stands past the south (-Y) side, Bastion's watchtower past the north (+Y).
		const bool bCanyon = Look.ScanSet == FName(TEXT("Canyon")) || Look.ScanSet == FName(TEXT("Creek"));
		const bool bLandmarkSide = (bCanyon && Side.Out.Y < 0.0) || (Look.ScanSet == FName(TEXT("Tundra")) && Side.Out.Y > 0.0);
		const double LandmarkAlong = bCanyon ? 600.0 : 0.0;
		for (int32 Row = Set.bRow0 ? 0 : 1; Row < (Set.bGiants ? 3 : 2); ++Row)
		{
			if (!(RowMask & (1 << Row)))
			{
				continue;
			}
			const TArray<UStaticMesh*>& Pool = (Row == 2 && Giants.Num() > 0) ? Giants : Cliffs;
			if (Pool.Num() == 0)
			{
				continue;
			}
			const double Gap = Row == 0 ? 10.0 : Row == 1 ? 1500.0 : 6500.0;
			const double Reach = Side.Length + (Row == 0 ? 1800.0 : Row == 1 ? 3500.0 : 9000.0);
			double Along = -Reach;
			while (Along < Reach)
			{
				UStaticMesh* Mesh = Pool[Random.RandRange(0, Pool.Num() - 1)];
				const FVector Extent = Mesh->GetBoundingBox().GetSize();
				const double Height = Row == 0 ? Random.FRandRange(1800.f, 2800.f) : Row == 1 ? Random.FRandRange(3000.f, 5000.f) : Random.FRandRange(7000.f, 11000.f);
				const double Width = Row == 0 ? Random.FRandRange(1200.f, 2000.f) : Row == 1 ? Random.FRandRange(2200.f, 3500.f) : Random.FRandRange(5000.f, 8000.f);
				double Flat = FMath::Clamp(Width / FMath::Max(FMath::Max(Extent.X, Extent.Y), 1.0), 0.3, 60.0);
				double Tall = FMath::Clamp(Height / FMath::Max(Extent.Z, 1.0), 0.3, 60.0);
				// A flat scan stretched 5-30x upward (a ridge or a low formation made into a cliff) tore into streaks
				// against the sky (R13-R15). Height may be at most twice the width's scale: past that, the two meet
				// halfway (wider and lower).
				if (Tall > Flat * 2.0)
				{
					const double Meet = FMath::Sqrt(Tall / (Flat * 2.0));
					Flat *= Meet;
					Tall /= Meet;
				}
				FTransform Transform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector::ZeroVector, FVector(Flat, Flat, Tall));
				const FBox Placed = PlacedBounds(Mesh, Transform);
				const double Spread = FVector::DotProduct(Side.Along, Placed.GetSize());
				const double Middle = Along + Spread * 0.5;
				const bool bOpen = bLandmarkSide && Row < 2 && FMath::Abs(Middle - LandmarkAlong) < 1800.0 + Spread * 0.3;
				if (!bOpen)
				{
					const bool bPositive = Side.Out.X + Side.Out.Y > 0.0;
					const double Inner = FVector::DotProduct(Side.Out, bPositive ? Placed.Min : Placed.Max);
					const double Offset = Side.Wall + Gap - Inner;
					const FVector At = Side.Out * Offset + Side.Along * (Middle - FVector::DotProduct(Side.Along, Placed.GetCenter()));
					Transform.SetTranslation(FVector(At.X, At.Y, BackdropZ + 50.0 - Placed.Min.Z));
					// No shadows from the cliffs: Nanite cliffs in the sun's cascades cost 1.3 ms (R13 GPU profile).
					Place(Mesh, Transform, false);
					++CliffCount;
				}
				Along += FMath::Max(Spread * (Row == 0 ? 0.45 : Row == 1 ? 0.62 : 0.7), 500.0);
			}
		}
	}


	// 5. Lush sets (Creekbed): the creek's water and its waterfall, trees along the rim, grass drifts on the banks.
	// Plants keep their own materials (or ours below), so they are grouped apart from the rock.
	int32 TreeCount = 0, GrassCount = 0, WaterCount = 0;
	TMap<UStaticMesh*, UInstancedStaticMeshComponent*> PlantGroups;
	auto PlacePlant = [&](UStaticMesh* Mesh, const FTransform& Transform, int32 CullDistance)
	{
		UInstancedStaticMeshComponent*& Instances = PlantGroups.FindOrAdd(Mesh);
		if (!Instances)
		{
			Instances = AddDecoration<UInstancedStaticMeshComponent>(TEXT("Terrain"), Mesh);
			Instances->SetCastShadow(false);
			// Out of the distance fields: a crown's or a tuft's own field shades it near black under Lumen.
			Instances->bAffectDistanceFieldLighting = false;
			if (CullDistance > 0)
			{
				Instances->SetCullDistances(CullDistance * 3 / 4, CullDistance);
			}
		}
		Instances->AddInstance(Transform);
	};
	if (Set.bLush && !FParse::Param(FCommandLine::Get(), TEXT("SBNoLush")))
	{
		// 5a. Water: a sheet just over the creek bed (ankle deep: feet stay on the bed, no collision), and a
		// waterfall down the north cliff into the gorge under the arch. MakeScanArt's MI_SB_Water / MI_SB_Waterfall.
		UStaticMesh* Plane = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Plane.Plane"));
		UMaterialInterface* Water = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Scan/MI_SB_Water.MI_SB_Water"));
		UMaterialInterface* Fall = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Scan/MI_SB_Waterfall.MI_SB_Waterfall"));
		if (Plane && Water)
		{
			for (const FBox& Block : Look.Water)
			{
				UStaticMeshComponent* Sheet = AddDecoration<UStaticMeshComponent>(TEXT("Water"), Plane);
				const FVector Size = Block.Max - Block.Min;
				const bool bFall = Size.Z > 300.0 && Fall;
				Sheet->SetCastShadow(false);
				Sheet->bAffectDistanceFieldLighting = false;
				Sheet->SetMaterial(0, bFall ? Fall : Water);
				if (bFall)
				{
					// Upright, across the block's long side.
					const bool bAlongX = Size.X >= Size.Y;
					const FVector Center((Block.Min.X + Block.Max.X) * 0.5, (Block.Min.Y + Block.Max.Y) * 0.5, (Block.Min.Z + Block.Max.Z) * 0.5);
					Sheet->SetRelativeTransform(FTransform(bAlongX ? FRotator(0.f, 0.f, 90.f) : FRotator(0.f, 90.f, 90.f), Center,
						FVector((bAlongX ? Size.X : Size.Y) / 100.0, Size.Z / 100.0, 1.0)));
				}
				else
				{
					Sheet->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector((Block.Min.X + Block.Max.X) * 0.5, (Block.Min.Y + Block.Max.Y) * 0.5, Block.Max.Z),
						FVector(Size.X / 100.0, Size.Y / 100.0, 1.0)));
				}
				Sheet->RegisterComponent();
				AddInstanceComponent(Sheet);
				++WaterCount;
			}
		}

		// 5b. Trees along the rim, rooted on the backdrop ground between the barrier cliffs and the next row; crowns
		// show over the rim. Visual only, outside the play space.
		const TArray<UStaticMesh*> Trees = Family(TEXT("3D_Plants/EuropeanHornbeam/Geometry/SimpleWind"), TEXT("SM_EuropeanHornbeam_Forest_"), { 1, 2, 3, 4, 5, 7, 8 });
		// The Megascans foliage master renders black here (like the grass): MakeScanArt's own M_SB_Leaves on the leaf
		// cards, the mossy creek rock (world projected, so it reads as bark at this range) on the trunks.
		UMaterialInterface* Leaves = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Scan/M_SB_Leaves.M_SB_Leaves"));
		if (!Leaves)
		{
			Leaves = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Scan/MI_SB_Leaves.MI_SB_Leaves"));
		}
		UMaterialInterface* Bark = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Scan/MI_SB_Scan_CreekRock.MI_SB_Scan_CreekRock"));
		int32 TreeTotal = 50; // 70 cost about 8 FPS (R19)
		FParse::Value(FCommandLine::Get(), TEXT("SBTrees="), TreeTotal);
		for (int32 Index = 0; Trees.Num() > 0 && Index < TreeTotal; ++Index)
		{
			const double Angle = Random.FRandRange(0.f, static_cast<float>(UE_TWO_PI));
			const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			const double ToWall = FMath::Min(Half.X / FMath::Max(FMath::Abs(Dir.X), 1e-3), Half.Y / FMath::Max(FMath::Abs(Dir.Y), 1e-3));
			const double Distance = ToWall + Random.FRandRange(1600.f, 4200.f); // crowns clear of the corner cameras and the rim
			UStaticMesh* Mesh = Trees[Random.RandRange(0, Trees.Num() - 1)];
			const double Height = Random.FRandRange(2400.f, 3800.f);
			const double Scale = FMath::Clamp(Height / FMath::Max(Mesh->GetBoundingBox().GetSize().Z, 1.0), 0.3, 6.0);
			FTransform Transform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector::ZeroVector, FVector(Scale));
			const FBox Placed = PlacedBounds(Mesh, Transform);
			Transform.SetTranslation(FVector(Dir.X * Distance, Dir.Y * Distance, BackdropZ + 50.0 - Placed.Min.Z));
			PlacePlant(Mesh, Transform, 0);
			++TreeCount;
		}
		for (UStaticMesh* Mesh : Trees)
		{
			UInstancedStaticMeshComponent** Instances = PlantGroups.Find(Mesh);
			for (int32 Slot = 0; Instances && Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				const bool bLeafSlot = Mesh->GetStaticMaterials()[Slot].MaterialSlotName == FName(TEXT("TwoSided"));
				if (UMaterialInterface* Surface = bLeafSlot ? Leaves : Bark)
				{
					(*Instances)->SetMaterial(Slot, Surface);
				}
			}
		}

		// 5c. Grass in drifts on the banks and ledges (never in the water), short enough not to hide a crouching
		// player, on our own M_SB_Grass (the Megascans foliage master renders it near black).
		const TArray<UStaticMesh*> Grass = Family(TEXT("3D_Plants/KikuyuGrass"), TEXT("SM_KikuyuGrass_"), { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 });
		UMaterialInterface* GrassSurface = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Scan/M_SB_Grass.M_SB_Grass"));
		const int32 Drifts = FMath::Clamp(static_cast<int32>(Half.X * Half.Y / 440000.0), 30, 90);
		for (int32 Drift = 0; Grass.Num() > 0 && GrassSurface && Drift < Drifts; ++Drift)
		{
			const FVector2D Middle(Random.FRandRange(-Half.X, Half.X), Random.FRandRange(-Half.Y, Half.Y));
			const int32 Tufts = Random.RandRange(14, 26);
			for (int32 Tuft = 0; Tuft < Tufts; ++Tuft)
			{
				const FVector2D At = Middle + FVector2D(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * Random.FRandRange(40.f, 340.f);
				if (FMath::Abs(At.X) > Half.X - 40.0 || FMath::Abs(At.Y) > Half.Y - 40.0)
				{
					continue;
				}
				double GroundZ = Layout.FloorZ;
				for (const FShieldbreakMapBlock& Block : Layout.Blocks)
				{
					if ((Block.Style == Floor || Block.Style == Sand) && At.X > Block.Min.X && At.X < Block.Max.X && At.Y > Block.Min.Y && At.Y < Block.Max.Y)
					{
						GroundZ = FMath::Max(GroundZ, Block.Max.Z);
					}
				}
				bool bWet = false;
				for (const FBox& Sheet : Look.Water)
				{
					bWet |= At.X > Sheet.Min.X - 60.0 && At.X < Sheet.Max.X + 60.0 && At.Y > Sheet.Min.Y - 60.0 && At.Y < Sheet.Max.Y + 60.0
						&& GroundZ < Sheet.Max.Z + 20.0;
				}
				const FVector Foot(At.X, At.Y, GroundZ);
				if (bWet || InsideBlock(Layout, Foot + FVector(0, 0, 20.0)))
				{
					continue;
				}
				UStaticMesh* Mesh = Grass[Random.RandRange(0, Grass.Num() - 1)];
				const double Height = FMath::Max(Mesh->GetBoundingBox().GetSize().Z, 1.0);
				const double Scale = FMath::Clamp(Random.FRandRange(28.f, 50.f) / Height, 0.2, 3.0);
				FTransform Transform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector::ZeroVector, FVector(Scale));
				const FBox Placed = PlacedBounds(Mesh, Transform);
				Transform.SetTranslation(FVector(At.X, At.Y, GroundZ - Placed.Min.Z - 7.0));
				PlacePlant(Mesh, Transform, 4000);
				++GrassCount;
			}
		}
		for (UStaticMesh* Mesh : Grass)
		{
			UInstancedStaticMeshComponent** Instances = PlantGroups.Find(Mesh);
			for (int32 Slot = 0; Instances && GrassSurface && Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				(*Instances)->SetMaterial(Slot, GrassSurface);
			}
		}
	}
	for (const TPair<UStaticMesh*, UInstancedStaticMeshComponent*>& Group : PlantGroups)
	{
		Group.Value->RegisterComponent();
		AddInstanceComponent(Group.Value);
	}

	for (const TPair<TTuple<UStaticMesh*, bool>, UInstancedStaticMeshComponent*>& Group : Groups)
	{
		Group.Value->RegisterComponent();
		AddInstanceComponent(Group.Value);
	}
	UE_LOG(LogShieldbreak, Log, TEXT("%s scanned terrain (%s): %d surfaces, %d fitted rocks over %d rock blocks, %d stones, %d cliffs, %d meshes (%d rock, %d cliff, %d giant scans loaded); %d water sheets, %d trees, %d grass tufts."),
		*Layout.DisplayName, *Look.ScanSet.ToString(), Surfaced, RockCount, RockBlocks.Num(), StoneCount, CliffCount, Groups.Num(), Rocks.Num(), Cliffs.Num(), Giants.Num(),
		WaterCount, TreeCount, GrassCount);
}
