#include "Game/ShieldbreakArena.h"

#include "Shieldbreak.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
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

	const FLinearColor TrimCyan(0.25f, 0.8f, 1.f);
	const FLinearColor TrimRed(1.f, 0.22f, 0.12f);
	const FLinearColor TrimBlue(0.2f, 0.45f, 1.f);

	/** Emissive share of the mountain ring's colour; -SBMountainLight=<x> overrides it for comparisons. */
	float MountainSelfLight()
	{
		float Value = 0.35f;
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
		BuildBackdrop();
	}

	// Bots and pickups only in a match. The front end does not build an arena; this also covers a backdrop.
	const AShieldbreakGameMode* Rules = GetWorld()->GetAuthGameMode<AShieldbreakGameMode>();
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
			MID->SetScalarParameterValue(TEXT("EmissiveStrength"), 6.f);
			MID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			MID->SetScalarParameterValue(TEXT("Roughness"), 0.4f);
		}
		return MID;
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
		AddBlock(Block.Min, Block.Max, Block.Color, Block.Style);
	}
	for (const FShieldbreakMapRamp& RampData : Layout.Ramps)
	{
		AddRamp(RampData.Start, RampData.End, RampData.Width, RampData.Thickness, RampData.Color, RampData.Style);
	}
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
	const FLinearColor Colors[3] = { TrimCyan, TrimRed, TrimBlue };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Strips[Index] = AddDecoration<UInstancedStaticMeshComponent>(TEXT("Trim"), CubeMesh);
		Strips[Index]->SetCastShadow(false);
		// Brighter than glow blocks: the lines are thin and mostly seen from across the map.
		if (UMaterialInstanceDynamic* MID = ApplySurface(Strips[Index], TEXT("glow"), Colors[Index]))
		{
			MID->SetScalarParameterValue(TEXT("EmissiveStrength"), 14.f);
		}
	}

	constexpr float StripHeight = 14.f, StripDepth = 6.f, TopInset = 30.f, FootHeight = 60.f, EndInset = 30.f;
	const FVector2D Half = Layout.HalfExtent;
	int32 Count = 0;
	for (const FShieldbreakMapBlock& Block : Layout.Blocks)
	{
		const FVector3f Size(Block.Max - Block.Min);
		if (!HasTrim(Block.Style) || Size.Z < 120.f)
		{
			continue;
		}
		UInstancedStaticMeshComponent* Strip = Strips[Block.Style == RedStyle ? 1 : Block.Style == BlueStyle ? 2 : 0];
		TArray<float, TInlineAllocator<2>> Heights;
		Heights.Add(static_cast<float>(Block.Max.Z) - TopInset);
		if (Size.Z >= 400.f)
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
	auto Module = [this](const TCHAR* Name, const TCHAR* Style, FLinearColor Tint)
	{
		const FString Path = FString::Printf(TEXT("/Game/Shieldbreak/Art/Spire/%s.%s"), Name, Name);
		UStaticMesh* Mesh = ShieldbreakArt::Load<UStaticMesh>(*Path);
		if (!Mesh) return static_cast<UInstancedStaticMeshComponent*>(nullptr);
		auto* Instances = AddDecoration<UInstancedStaticMeshComponent>(TEXT("SpireArt"), Mesh);
		Instances->SetCastShadow(false);
		ApplySurface(Instances, Style, Tint);
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
	for (auto* Instances : { Collectors, Panels, Collars })
	{
		if (Instances)
		{
			Instances->RegisterComponent();
			AddInstanceComponent(Instances);
		}
	}
	UE_LOG(LogShieldbreak, Log, TEXT("Spire art: %d collectors, %d service panels, %d lift collars (visual only)."),
		Collectors ? Collectors->GetInstanceCount() : 0, Panels ? Panels->GetInstanceCount() : 0, Collars ? Collars->GetInstanceCount() : 0);
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
	const TArray<FStaticMaterial>& Slots = Asset->GetStaticMaterials();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FString Slot = Slots[Index].MaterialSlotName.ToString();
		for (const FSlotLook& Entry : SlotLooks)
		{
			if (!Slot.StartsWith(Entry.Prefix))
			{
				continue;
			}
			const TObjectPtr<UMaterialInterface>* World = SurfaceMaterials.Find(FName(Entry.Kind));
			if (World && *World)
			{
				Mesh->SetMaterial(Index, *World);
				if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(Index))
				{
					MID->SetVectorParameterValue(TEXT("Tint"), Entry.Tint);
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
	if (GetNetMode() == NM_DedicatedServer || FParse::Param(FCommandLine::Get(), TEXT("SBNoMapArt")))
	{
		return;
	}
	const FShieldbreakMapLook& Look = ShieldbreakMapArt::GetLook(MapId);
	int32 Kinds = 0, Placed = 0;
	for (const FShieldbreakArtGroup& Group : ShieldbreakMapArt::Plan(MapId, Layout))
	{
		const FString Name = TEXT("SM_SB_Kit_") + Group.Piece.ToString();
		UStaticMesh* Mesh = Group.Instances.Num() > 0 ? ShieldbreakArt::Load<UStaticMesh>(*FString::Printf(TEXT("/Game/Shieldbreak/Art/MapKit/%s.%s"), *Name, *Name)) : nullptr;
		if (!Mesh)
		{
			continue;
		}
		// One instanced component per piece: a draw call or two per kind, however many stand around the map.
		auto* Instances = AddDecoration<UInstancedStaticMeshComponent>(TEXT("MapArt"), Mesh);
		Instances->SetCastShadow(false);
		// Like the backdrop mountains: in distance-field lighting the kit came out near black up close (sunlit tops
		// included) and only lit beyond its range, so keep it to the shadow maps and the sky light.
		Instances->bAffectDistanceFieldLighting = false;
		// Pieces laid a few centimetres proud of a block (reliefs, edge bands, the floor inlay) sit inside the block's
		// distance field, so the sky light's occlusion blacks them out; a little self light keeps them reading.
		const bool bFlush = Group.Piece == TEXT("Relief") || Group.Piece == TEXT("Fascia") || Group.Piece == TEXT("Inlay");
		ApplyKitSurfaces(Instances, Look, bFlush ? 0.35f : 0.f);
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
	UE_LOG(LogShieldbreak, Log, TEXT("%s map art: %d kit pieces, %d instances (visual only)."), *Layout.DisplayName, Kinds, Placed);
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

	// A ring of mountains, hazy in the fog: the faceted ring from Art/blender/build_environment.py (inner edge at
	// 100 m, scaled so it starts just past this map's walls), turned and stretched per map so each skyline differs.
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
	float SunIntensity = 8.f, FillIntensity = 1.5f, SkyIntensity = 6.f;
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
		}
	}

	// A light grade: some bloom for the trim and glow, a touch more saturation and contrast, and a soft vignette.
	if (!FParse::Param(FCommandLine::Get(), TEXT("SBNoGrade")))
	{
		if (APostProcessVolume* Grade = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params))
		{
			Grade->bUnbound = true;
			FPostProcessSettings& Settings = Grade->Settings;
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
			// The first shots read washed out (pale walls and floors in full sun): pull exposure down a little.
			// -SBExposure=<stops> overrides it for comparisons.
			float ExposureBias = -0.4f;
			FParse::Value(FCommandLine::Get(), TEXT("SBExposure="), ExposureBias);
			Settings.bOverride_AutoExposureBias = true;
			Settings.AutoExposureBias = ExposureBias;
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
