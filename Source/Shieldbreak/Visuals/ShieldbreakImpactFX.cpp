#include "Visuals/ShieldbreakImpactFX.h"

#include "Shieldbreak.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Player/ShieldbreakCharacter.h"
#include "Shield/ShieldHealthComponent.h"
#include "Visuals/ShieldbreakArt.h"

namespace ShieldbreakImpactPrivate
{
	TAutoConsoleVariable<int32> CVarEnabled(TEXT("sb.ImpactFX"), 1, TEXT("1: Shieldbreak's bullet hits and muzzle flashes; 0: the old single Lyra impact effect (FPS A/B)."));
	TAutoConsoleVariable<int32> CVarDecals(TEXT("sb.ImpactDecals"), 64, TEXT("Bullet holes kept at once (0 = none, max 128)."));
	TAutoConsoleVariable<float> CVarDecalLife(TEXT("sb.ImpactDecalLife"), 20.f, TEXT("Seconds a bullet hole stays before it fades."));
	TAutoConsoleVariable<int32> CVarLights(TEXT("sb.ImpactLights"), 1, TEXT("1: hits and muzzle flashes throw a short, shadowless light."));
	TAutoConsoleVariable<float> CVarSparks(TEXT("sb.ImpactSparks"), 1.f, TEXT("Spark count scale (0 = none)."));
	TAutoConsoleVariable<float> CVarMuzzleLight(TEXT("sb.MuzzleLight"), 6.f, TEXT("Muzzle flash light candelas at strength 1 (0 = no light)."));
	TAutoConsoleVariable<int32> CVarDebug(TEXT("sb.ImpactFXDebug"), 0, TEXT("1: muzzle flash cards stay lit and log where they are (capture diagnosis)."));
	TAutoConsoleVariable<int32> CVarMuzzle(TEXT("sb.MuzzleFlash"), 1, TEXT("1: Shieldbreak's muzzle flash cards and light."));
	TAutoConsoleVariable<int32> CVarLyraMuzzle(TEXT("sb.LyraMuzzleFlash"), 0, TEXT("1: Lyra's Niagara muzzle flash also plays under ours (off: ours draws on the first-person gun since FX4)."));

	const TCHAR* const SparkMaterial = TEXT("/Game/Shieldbreak/Art/FX/M_SB_FXSpark.M_SB_FXSpark");
	const TCHAR* const GlowMaterial = TEXT("/Game/Shieldbreak/Art/FX/M_SB_FXGlowInst.M_SB_FXGlowInst");
	const TCHAR* const PuffMaterial = TEXT("/Game/Shieldbreak/Art/FX/M_SB_FXPuffInst.M_SB_FXPuffInst");
	const TCHAR* const MuzzleMaterial = TEXT("/Game/Shieldbreak/Art/FX/M_SB_MuzzleFlash.M_SB_MuzzleFlash");
	const TCHAR* const HoleMaterial = TEXT("/Game/Shieldbreak/Art/FX/M_SB_BulletHole.M_SB_BulletHole");
	const TCHAR* const ShieldMaterial = TEXT("/Game/Shieldbreak/Art/Materials/M_SB_ShieldImpact.M_SB_ShieldImpact");
	const TCHAR* const CubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const PlaneMesh = TEXT("/Engine/BasicShapes/Plane.Plane");
	const TCHAR* const SphereMesh = TEXT("/Engine/BasicShapes/Sphere.Sphere");

	constexpr int32 MaxSparks = 384;
	constexpr int32 MaxCards = 160;
	constexpr int32 MaxDecals = 128;
	constexpr int32 LightCount = 4;
	constexpr int32 FlareCount = 6;
	constexpr int32 MuzzleSlotCount = 6;
	constexpr int32 CardsPerMuzzle = 3;
	constexpr float DecalFade = 3.f;

	const FLinearColor ShieldBlue(0.25f, 0.72f, 1.f);
	const FLinearColor OvershieldAmber(1.f, 0.62f, 0.12f);

	struct FSurfaceInfo
	{
		EShieldbreakImpactSurface Surface = EShieldbreakImpactSurface::Metal;
		/** Dust colour for dirt (snow is white, sandstone warm). */
		FLinearColor Dust = FLinearColor(0.38f, 0.32f, 0.24f);
	};

	bool ContainsAny(const FString& Text, std::initializer_list<const TCHAR*> Words)
	{
		for (const TCHAR* Word : Words)
		{
			if (Text.Contains(Word))
			{
				return true;
			}
		}
		return false;
	}

	FSurfaceInfo ClassifyWorld(const FHitResult& Hit)
	{
		FSurfaceInfo Info;
		const UPrimitiveComponent* Component = Hit.GetComponent();
		if (!Component)
		{
			return Info;
		}
		// The surface rarely changes, so remember each component's answer (cleared if it ever grows large).
		static TMap<TWeakObjectPtr<const UPrimitiveComponent>, FSurfaceInfo> Cache;
		if (const FSurfaceInfo* Found = Cache.Find(Component))
		{
			return *Found;
		}
		if (Cache.Num() > 4096)
		{
			Cache.Reset();
		}

		FString Name = Component->GetClass()->GetName();
		if (const UMaterialInterface* Material = Component->GetMaterial(0))
		{
			Name += TEXT(" ") + Material->GetName();
		}
		if (const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			if (const UStaticMesh* Mesh = MeshComponent->GetStaticMesh())
			{
				Name += TEXT(" ") + Mesh->GetName();
			}
		}
		Name.ToLowerInline();
		if (ContainsAny(Name, { TEXT("water"), TEXT("river") }))
		{
			Info.Surface = EShieldbreakImpactSurface::Water;
		}
		else if (ContainsAny(Name, { TEXT("landscape"), TEXT("terrain"), TEXT("grass"), TEXT("dirt"), TEXT("sand"), TEXT("rock"),
			TEXT("soil"), TEXT("mud"), TEXT("ground"), TEXT("cliff"), TEXT("moss"), TEXT("gravel"), TEXT("stone"), TEXT("boulder"),
			TEXT("earth"), TEXT("snow") }))
		{
			Info.Surface = EShieldbreakImpactSurface::Dirt;
			if (Name.Contains(TEXT("snow")))
			{
				Info.Dust = FLinearColor(0.7f, 0.72f, 0.75f);
			}
			else if (ContainsAny(Name, { TEXT("sand"), TEXT("canyon") }))
			{
				Info.Dust = FLinearColor(0.5f, 0.38f, 0.26f);
			}
		}
		Cache.Add(Component, Info);
		static int32 Logged = 0;
		if (Logged < 40)
		{
			++Logged;
			UE_LOG(LogShieldbreak, Log, TEXT("ImpactFX surface: %d for '%s'"), static_cast<int32>(Info.Surface), *Name);
		}
		return Info;
	}

	float EaseOut(float T) { return 1.f - FMath::Square(1.f - FMath::Clamp(T, 0.f, 1.f)); }

	UMaterialInterface* LoadMaterial(const TCHAR* Path) { return ShieldbreakArt::Load<UMaterialInterface>(Path); }

	void QuietPrimitive(UPrimitiveComponent* Primitive)
	{
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Primitive->SetGenerateOverlapEvents(false);
		Primitive->SetCastShadow(false);
		Primitive->bAffectDistanceFieldLighting = false;
		Primitive->bReceivesDecals = false;
		Primitive->SetCanEverAffectNavigation(false);
		Primitive->SetMobility(EComponentMobility::Movable);
	}

	UPointLightComponent* MakeLight(AActor* FXActor)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(FXActor);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetCastShadows(false);
		Light->SetAffectTranslucentLighting(false);
		Light->SetVolumetricScatteringIntensity(0.f);
		Light->SetSourceRadius(4.f);
		Light->SetVisibility(false);
		Light->SetupAttachment(FXActor->GetRootComponent());
		Light->RegisterComponent();
		return Light;
	}
}

// ---- Lifetime ----

bool UShieldbreakImpactFXSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE)
		&& !IsRunningDedicatedServer() && FApp::CanEverRender() && Super::ShouldCreateSubsystem(Outer);
}

UShieldbreakImpactFXSubsystem* UShieldbreakImpactFXSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer || ShieldbreakImpactPrivate::CVarEnabled.GetValueOnGameThread() == 0)
	{
		return nullptr;
	}
	return World->GetSubsystem<UShieldbreakImpactFXSubsystem>();
}

void UShieldbreakImpactFXSubsystem::Deinitialize()
{
	Sparks.Reset();
	Cards.Reset();
	bActive = false;
	Super::Deinitialize();
}

TStatId UShieldbreakImpactFXSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UShieldbreakImpactFXSubsystem, STATGROUP_Tickables);
}

float UShieldbreakImpactFXSubsystem::StrengthForDamage(float Damage)
{
	return FMath::Clamp(FMath::Sqrt(FMath::Max(Damage, 0.f) / 12.f), 0.75f, 2.5f);
}

bool UShieldbreakImpactFXSubsystem::UseLyraMuzzleFlash()
{
	return ShieldbreakImpactPrivate::CVarLyraMuzzle.GetValueOnGameThread() != 0 || ShieldbreakImpactPrivate::CVarEnabled.GetValueOnGameThread() == 0;
}

bool UShieldbreakImpactFXSubsystem::EnsureActor()
{
	using namespace ShieldbreakImpactPrivate;
	if (FXActor)
	{
		return true;
	}
	UWorld* World = GetWorld();
	if (bTriedActor || !World)
	{
		return false;
	}
	bTriedActor = true;

	FActorSpawnParameters Params;
	Params.Name = MakeUniqueObjectName(World->PersistentLevel, AActor::StaticClass(), TEXT("ShieldbreakImpactFX"));
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FXActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!FXActor)
	{
		return false;
	}
	USceneComponent* Root = NewObject<USceneComponent>(FXActor, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	FXActor->SetRootComponent(Root);
	Root->RegisterComponent();

	auto MakeInstanced = [this](const TCHAR* MeshPath, const TCHAR* MaterialPath, int32 CustomFloats, int32 SortPriority) -> UInstancedStaticMeshComponent*
	{
		UStaticMesh* Mesh = ShieldbreakArt::Load<UStaticMesh>(MeshPath);
		UMaterialInterface* Material = LoadMaterial(MaterialPath);
		if (!Mesh || !Material)
		{
			return nullptr;
		}
		UInstancedStaticMeshComponent* Instanced = NewObject<UInstancedStaticMeshComponent>(FXActor);
		Instanced->SetStaticMesh(Mesh);
		Instanced->SetMaterial(0, Material);
		QuietPrimitive(Instanced);
		Instanced->SetTranslucentSortPriority(SortPriority);
		Instanced->SetNumCustomDataFloats(CustomFloats);
		Instanced->SetupAttachment(FXActor->GetRootComponent());
		Instanced->SetVisibility(false);
		Instanced->RegisterComponent();
		return Instanced;
	};
	SparkMesh = MakeInstanced(CubeMesh, SparkMaterial, 4, 2);
	GlowMesh = MakeInstanced(PlaneMesh, GlowMaterial, 4, 3);
	PuffMesh = MakeInstanced(PlaneMesh, PuffMaterial, 5, 1);

	for (int32 Index = 0; Index < LightCount; ++Index)
	{
		Lights.Add(MakeLight(FXActor));
	}
	LightSlots.SetNum(LightCount);

	UStaticMesh* Sphere = ShieldbreakArt::Load<UStaticMesh>(SphereMesh);
	if (UMaterialInterface* Shield = Sphere ? LoadMaterial(ShieldMaterial) : nullptr)
	{
		for (int32 Index = 0; Index < FlareCount; ++Index)
		{
			UStaticMeshComponent* Flare = NewObject<UStaticMeshComponent>(FXActor);
			Flare->SetStaticMesh(Sphere);
			QuietPrimitive(Flare);
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Shield, this);
			Flare->SetMaterial(0, MID);
			Flare->SetupAttachment(FXActor->GetRootComponent());
			Flare->SetVisibility(false);
			Flare->RegisterComponent();
			Flares.Add(Flare);
			FlareMIDs.Add(MID);
		}
	}
	FlareSlots.SetNum(Flares.Num());

	for (int32 Index = 0; Index < MuzzleSlotCount; ++Index)
	{
		MuzzleLights.Add(MakeLight(FXActor));
	}
	MuzzleSlots.SetNum(MuzzleSlotCount);

	UE_LOG(LogShieldbreak, Log, TEXT("ImpactFX ready: sparks %d, glow %d, puff %d, shield flares %d, muzzle cards %d, decal material %d"),
		SparkMesh != nullptr, GlowMesh != nullptr, PuffMesh != nullptr, Flares.Num(), LoadMaterial(MuzzleMaterial) != nullptr, LoadMaterial(HoleMaterial) != nullptr);
	return true;
}

// ---- Classification ----

EShieldbreakImpactSurface UShieldbreakImpactFXSubsystem::ClassifySurface(const FHitResult& Hit)
{
	if (const APawn* Pawn = Cast<APawn>(Hit.GetActor()))
	{
		const AShieldbreakCharacter* Character = Cast<AShieldbreakCharacter>(Pawn);
		const UShieldHealthComponent* Shield = Character ? Character->GetShieldHealth() : nullptr;
		if (Shield && Shield->GetOvershield() > 0.f)
		{
			return EShieldbreakImpactSurface::Overshield;
		}
		if (Shield && Shield->GetShield() > 0.f)
		{
			return EShieldbreakImpactSurface::Shield;
		}
		return EShieldbreakImpactSurface::Armor;
	}
	return ShieldbreakImpactPrivate::ClassifyWorld(Hit).Surface;
}

// ---- Effects ----

void UShieldbreakImpactFXSubsystem::BulletImpact(const FHitResult& Hit, const FVector& ShotDir, float Strength, const FLinearColor& Tint)
{
	using namespace ShieldbreakImpactPrivate;
	if (!EnsureActor())
	{
		return;
	}
	const float S = FMath::Clamp(Strength, 0.25f, 4.f);
	const FVector Point = Hit.ImpactPoint;
	const FVector Normal = Hit.ImpactNormal.IsNearlyZero() ? -ShotDir : FVector(Hit.ImpactNormal);
	const FVector Dir = ShotDir.IsNearlyZero() ? -Normal : ShotDir.GetSafeNormal();
	const FVector Reflect = (Dir - 2.f * FVector::DotProduct(Dir, Normal) * Normal).GetSafeNormal();
	const FVector SprayAxis = (Reflect + Normal).GetSafeNormal();
	const float SparkScale = FMath::Max(0.f, CVarSparks.GetValueOnGameThread());
	const bool bLights = CVarLights.GetValueOnGameThread() != 0;

	const EShieldbreakImpactSurface Surface = ClassifySurface(Hit);
	LastSurface = Surface;
	++ImpactCount;
	++SurfaceCounts[static_cast<int32>(Surface)];

	// Energy rounds (the Rail Lance) tint the sparks toward their own colour.
	FLinearColor Hot(1.f, 0.86f, 0.6f);
	FLinearColor Cool(1.f, 0.32f, 0.05f);
	if (Tint.A > 0.f)
	{
		Hot = FMath::Lerp(Hot, Tint, Tint.A);
		Cool = FMath::Lerp(Cool, Tint * 0.6f, Tint.A);
	}

	switch (Surface)
	{
	case EShieldbreakImpactSurface::Metal:
	{
		SpawnSparks(Point, Normal, SprayAxis, FMath::RoundToInt((6.f + 6.f * S) * SparkScale), 500.f, 1500.f * (0.8f + 0.2f * S),
			40.f, 0.36f, Hot, Cool, 45.f, 0.5f + 0.15f * S, 980.f);
		AddCard(true, Point + Normal * 2.f, FVector::ZeroVector, Hot, 0.07f, 5.f * S, 16.f * S, 22.f);
		// Grit: a fast small puff and a slower one that lingers.
		AddCard(false, Point + Normal * 3.f, Normal * 140.f, FLinearColor(0.16f, 0.155f, 0.15f), 0.45f, 5.f * S, 26.f * S, 0.5f, 5.f);
		AddCard(false, Point + Normal * 4.f, Normal * 45.f + FVector(0.f, 0.f, 15.f), FLinearColor(0.13f, 0.125f, 0.12f), 1.1f, 8.f * S, 42.f * S, 0.32f, 2.f);
		if (bLights)
		{
			FlashLight(Point + Normal * 12.f, Hot, 3.f * S, 120.f + 40.f * S, 0.06f);
		}
		PlaceDecal(Hit, 4.5f * FMath::Sqrt(S) + FMath::FRandRange(-0.4f, 0.4f), FLinearColor(0.035f, 0.03f, 0.028f), 0.8f, 0.7f);
		break;
	}
	case EShieldbreakImpactSurface::Dirt:
	{
		const FSurfaceInfo Info = ClassifyWorld(Hit);
		SpawnSparks(Point, Normal, SprayAxis, FMath::RoundToInt(2.f * S * SparkScale), 400.f, 900.f, 35.f, 0.2f, Hot, Cool, 18.f, 0.25f, 980.f);
		// A jet of dust along the normal, a wider cloud, and a low skirt that hangs.
		AddCard(false, Point + Normal * 4.f, Normal * 320.f, Info.Dust, 0.6f, 8.f * S, 42.f * S, 0.95f, 6.f);
		AddCard(false, Point + Normal * 6.f, SprayAxis * 160.f + FVector(0.f, 0.f, 30.f), Info.Dust * 0.9f, 1.3f, 12.f * S, 70.f * S, 0.8f, 3.f);
		AddCard(false, Point + Normal * 3.f, FVector(0.f, 0.f, 12.f), Info.Dust * 0.8f, 1.8f, 16.f * S, 85.f * S, 0.55f, 2.f);
		PlaceDecal(Hit, 5.5f * FMath::Sqrt(S), Info.Dust * 0.25f, 0.65f, 0.f);
		break;
	}
	case EShieldbreakImpactSurface::Water:
	{
		const FLinearColor Spray(0.62f, 0.68f, 0.7f);
		AddCard(false, Point + FVector(0.f, 0.f, 4.f), FVector(0.f, 0.f, 420.f), Spray, 0.5f, 5.f * S, 26.f * S, 0.7f, 5.f);
		AddCard(false, Point + FVector(0.f, 0.f, 3.f), FVector(0.f, 0.f, 120.f), Spray, 0.9f, 10.f * S, 48.f * S, 0.45f, 3.f);
		AddCard(false, Point, FVector::ZeroVector, Spray * 0.8f, 1.2f, 12.f * S, 60.f * S, 0.3f, 2.f);
		break;
	}
	case EShieldbreakImpactSurface::Armor:
	{
		SpawnSparks(Point, Normal, SprayAxis, FMath::RoundToInt((7.f + 4.f * S) * SparkScale), 450.f, 1200.f, 55.f, 0.28f,
			FLinearColor(1.f, 0.78f, 0.45f), FLinearColor(0.9f, 0.22f, 0.04f), 34.f, 0.35f, 900.f);
		AddCard(true, Point + Normal * 3.f, FVector::ZeroVector, FLinearColor(1.f, 0.6f, 0.3f), 0.06f, 5.f * S, 14.f * S, 16.f);
		AddCard(false, Point + Normal * 4.f, Normal * 90.f, FLinearColor(0.07f, 0.065f, 0.06f), 0.5f, 6.f * S, 28.f * S, 0.55f, 4.f);
		if (bLights)
		{
			FlashLight(Point + Normal * 15.f, FLinearColor(1.f, 0.55f, 0.25f), 2.5f * S, 140.f, 0.05f);
		}
		break;
	}
	case EShieldbreakImpactSurface::Shield:
	case EShieldbreakImpactSurface::Overshield:
	{
		const FLinearColor Energy = Surface == EShieldbreakImpactSurface::Overshield ? OvershieldAmber : ShieldBlue;
		ShieldFlare(Point, Normal, Energy, S);
		// Energy sparks: brighter, lighter (they arc less) and quick.
		SpawnSparks(Point, Normal, SprayAxis, FMath::RoundToInt((5.f + 4.f * S) * SparkScale), 350.f, 950.f, 60.f, 0.22f,
			Energy * 0.6f + FLinearColor(0.4f, 0.4f, 0.4f), Energy, 45.f, 0.3f, 250.f);
		AddCard(true, Point + Normal * 3.f, FVector::ZeroVector, Energy, 0.1f, 8.f * S, 26.f * S, 18.f);
		if (bLights)
		{
			FlashLight(Point + Normal * 18.f, Energy, 3.f * S, 160.f, 0.08f);
		}
		break;
	}
	}
	bActive = true;
}

void UShieldbreakImpactFXSubsystem::SpawnSparks(const FVector& Point, const FVector& Normal, const FVector& Axis, int32 Count, float SpeedMin,
	float SpeedMax, float ConeDegrees, float Life, const FLinearColor& Hot, const FLinearColor& Cool, float Brightness, float Width, float Gravity)
{
	using namespace ShieldbreakImpactPrivate;
	if (!SparkMesh || Count <= 0)
	{
		return;
	}
	const float HalfCone = FMath::DegreesToRadians(ConeDegrees);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Sparks.Num() >= MaxSparks)
		{
			Sparks.RemoveAt(0, EAllowShrinking::No);
		}
		FSpark& Spark = Sparks.AddDefaulted_GetRef();
		FVector Direction = FMath::VRandCone(Axis, HalfCone);
		// Never into the surface.
		if (FVector::DotProduct(Direction, Normal) < 0.05f)
		{
			Direction = (Direction + Normal * (0.05f - FVector::DotProduct(Direction, Normal) + 0.1f)).GetSafeNormal();
		}
		// A few slow, fat embers among fast thin streaks.
		const bool bEmber = FMath::FRand() < 0.25f;
		Spark.Position = Point + Normal * 1.5f;
		Spark.Velocity = Direction * FMath::FRandRange(SpeedMin, SpeedMax) * (bEmber ? 0.45f : 1.f);
		Spark.Life = Life * FMath::FRandRange(0.55f, 1.25f) * (bEmber ? 1.6f : 1.f);
		Spark.Width = Width * (bEmber ? 1.6f : 1.f);
		Spark.HotColor = Hot;
		Spark.CoolColor = Cool;
		Spark.Brightness = Brightness * FMath::FRandRange(0.7f, 1.2f);
		Spark.Drag = bEmber ? 3.f : 1.6f;
		Spark.Gravity = Gravity;
	}
}

void UShieldbreakImpactFXSubsystem::AddCard(bool bGlow, const FVector& Position, const FVector& Velocity, const FLinearColor& Color, float Life,
	float StartSize, float EndSize, float Peak, float Drag)
{
	using namespace ShieldbreakImpactPrivate;
	if (!(bGlow ? GlowMesh : PuffMesh))
	{
		return;
	}
	if (Cards.Num() >= MaxCards)
	{
		Cards.RemoveAt(0, EAllowShrinking::No);
	}
	FCard& Card = Cards.AddDefaulted_GetRef();
	Card.bGlow = bGlow;
	Card.Position = Position;
	Card.Velocity = Velocity;
	Card.Color = Color;
	Card.Life = Life * (bGlow ? 1.f : FMath::FRandRange(0.85f, 1.15f));
	Card.StartSize = StartSize;
	Card.EndSize = EndSize;
	Card.Peak = Peak;
	Card.Drag = Drag;
	Card.Roll = FMath::FRandRange(0.f, 2.f * UE_PI);
	Card.Spin = FMath::FRandRange(-0.6f, 0.6f);
	Card.Seed = FMath::FRand();
}

void UShieldbreakImpactFXSubsystem::PlaceDecal(const FHitResult& Hit, float Radius, const FLinearColor& Tint, float Darkness, float Rim)
{
	using namespace ShieldbreakImpactPrivate;
	const int32 Wanted = FMath::Clamp(CVarDecals.GetValueOnGameThread(), 0, MaxDecals);
	const UPrimitiveComponent* Surface = Hit.GetComponent();
	if (Wanted == 0 || !Surface || Surface->IsSimulatingPhysics() || Cast<APawn>(Hit.GetActor()))
	{
		return;
	}
	UMaterialInterface* Material = LoadMaterial(HoleMaterial);
	if (!Material)
	{
		return;
	}
	if (NextDecal >= Wanted)
	{
		NextDecal = 0;
	}
	if (!Decals.IsValidIndex(NextDecal))
	{
		UDecalComponent* Decal = NewObject<UDecalComponent>(FXActor);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
		Decal->SetDecalMaterial(MID);
		Decal->SetFadeScreenSize(0.0004f);
		Decal->SetupAttachment(FXActor->GetRootComponent());
		Decal->RegisterComponent();
		NextDecal = Decals.Add(Decal);
		DecalMIDs.Add(MID);
		DecalTimes.Add(0.f);
	}
	UDecalComponent* Decal = Decals[NextDecal];
	UMaterialInstanceDynamic* MID = DecalMIDs[NextDecal];
	// Projects along -normal; random spin so holes never repeat.
	const FVector Normal = Hit.ImpactNormal;
	FRotator Rotation = (-Normal).Rotation();
	Rotation.Roll = FMath::FRandRange(-180.f, 180.f);
	// Deep box: arena walls are collision cubes behind separate cladding meshes, so the visible surface can sit
	// centimetres off the hit point (FX1-FX3: holes showed on floors, never on walls, with a 4 cm box).
	Decal->DecalSize = FVector(20.f, Radius, Radius);
	Decal->SetWorldLocationAndRotation(Hit.ImpactPoint, Rotation);
	MID->SetVectorParameterValue(TEXT("Tint"), Tint);
	MID->SetScalarParameterValue(TEXT("Darkness"), Darkness);
	MID->SetScalarParameterValue(TEXT("Rim"), Rim);
	MID->SetScalarParameterValue(TEXT("Seed"), FMath::FRand());
	MID->SetScalarParameterValue(TEXT("Opacity"), 1.f);
	DecalTimes[NextDecal] = GetWorld()->GetTimeSeconds();
	Decal->SetVisibility(true);
	Decal->MarkRenderStateDirty();
	++NextDecal;
	++DecalsPlaced;
}

void UShieldbreakImpactFXSubsystem::FlashLight(const FVector& Position, const FLinearColor& Color, float Candelas, float Radius, float Duration)
{
	if (Lights.Num() == 0)
	{
		return;
	}
	NextLight = (NextLight + 1) % Lights.Num();
	UPointLightComponent* Light = Lights[NextLight];
	Light->SetWorldLocation(Position);
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(Radius);
	Light->SetIntensity(Candelas);
	Light->SetVisibility(true);
	FTimedSlot& Slot = LightSlots[NextLight];
	Slot.StartTime = GetWorld()->GetTimeSeconds();
	Slot.Duration = Duration;
	Slot.Peak = Candelas;
}

void UShieldbreakImpactFXSubsystem::ShieldFlare(const FVector& Point, const FVector& Normal, const FLinearColor& Color, float Strength)
{
	if (Flares.Num() == 0)
	{
		return;
	}
	NextFlare = (NextFlare + 1) % Flares.Num();
	UStaticMeshComponent* Flare = Flares[NextFlare];
	// Sits just outside the armor so the rim of the shell reads as a ring of shield energy around the hit.
	Flare->SetWorldLocation(Point + Normal * 4.f);
	Flare->SetWorldScale3D(FVector(0.05f));
	FlareMIDs[NextFlare]->SetVectorParameterValue(TEXT("ShieldColor"), Color);
	FlareMIDs[NextFlare]->SetScalarParameterValue(TEXT("Intensity"), 1.5f);
	Flare->SetVisibility(true);
	FTimedSlot& Slot = FlareSlots[NextFlare];
	Slot.StartTime = GetWorld()->GetTimeSeconds();
	Slot.Duration = 0.2f;
	Slot.Peak = 1.6f;
	// The Engine sphere is 100 cm across: a battle rifle round flares ~30 cm, a sniper round ~55 cm.
	Slot.Size = FMath::Clamp(0.2f + 0.12f * Strength, 0.25f, 0.6f);
}

void UShieldbreakImpactFXSubsystem::MuzzleFlash(USceneComponent* AttachTo, const FVector& Muzzle, const FVector& Direction, const FLinearColor& Tint, float Strength)
{
	using namespace ShieldbreakImpactPrivate;
	if (CVarMuzzle.GetValueOnGameThread() == 0 || !AttachTo || !EnsureActor())
	{
		return;
	}
	++MuzzleFlashCount;
	const float S = FMath::Clamp(Strength, 0.5f, 3.f);
	const FVector Dir = Direction.GetSafeNormal();
	const float Now = GetWorld()->GetTimeSeconds();
	NextMuzzle = (NextMuzzle + 1) % MuzzleSlotCount;
	FTimedSlot& Slot = MuzzleSlots[NextMuzzle];
	Slot.StartTime = Now;
	// Two or three frames: long enough to register, short enough to never hide the target.
	Slot.Duration = CVarDebug.GetValueOnGameThread() != 0 ? 3.f : 0.04f + 0.01f * S;
	// Keel's exposure turned 100+ cd into a white-out of the first-person gun; a few candelas a little ahead of the
	// muzzle lights the walls and the gun's front without blowing it out.
	Slot.Peak = CVarMuzzleLight.GetValueOnGameThread() * S;

	const UPrimitiveComponent* GunPrimitive = Cast<UPrimitiveComponent>(AttachTo);
	const EFirstPersonPrimitiveType ViewType = GunPrimitive ? GunPrimitive->FirstPersonPrimitiveType : EFirstPersonPrimitiveType::None;
	// The cards belong to the gun's own actor and stay attached to it: first-person primitives only draw for their
	// owner's view, so cards owned by the shared effects actor never showed on the first-person gun (FX1-FX3).
	FMuzzleRig* Rig = FindOrCreateMuzzleRig(AttachTo);
	if (Rig)
	{
		Rig->StartTime = Now;
		Rig->Duration = Slot.Duration;
		const float Roll = FMath::FRandRange(0.f, 360.f);
		const FQuat Facing = FRotationMatrix::MakeFromZ(Dir).ToQuat();
		const FQuat Spin(Dir, FMath::DegreesToRadians(Roll));
		const FVector Side = (Spin * Facing).GetAxisX();
		const float Jitter = FMath::FRandRange(0.85f, 1.15f);
		const float StarSize = 13.f * S * Jitter;
		const float FlameLength = 24.f * S * Jitter;
		const float FlameWidth = 9.f * S;
		static int32 DebugLogged = 0;
		for (int32 Card = 0; Card < CardsPerMuzzle; ++Card)
		{
			UStaticMeshComponent* Mesh = Rig->Cards[Card].Get();
			UMaterialInstanceDynamic* MID = Rig->MIDs[Card].Get();
			if (!Mesh || !MID)
			{
				continue;
			}
			if (Card == 0)
			{
				Mesh->SetWorldLocationAndRotation(Muzzle + Dir * 2.f, Spin * Facing);
				Mesh->SetWorldScale3D(FVector(StarSize / 100.f, StarSize / 100.f, 1.f));
			}
			else
			{
				// Planes that contain the barrel: X along the shot, normal sideways (the second one turned 90 degrees).
				const FVector Across = Card == 1 ? Side : FVector::CrossProduct(Dir, Side);
				Mesh->SetWorldLocationAndRotation(Muzzle + Dir * (FlameLength * 0.45f), FRotationMatrix::MakeFromXZ(Dir, Across).ToQuat());
				Mesh->SetWorldScale3D(FVector(FlameLength / 100.f, FlameWidth / 100.f, 1.f));
			}
			MID->SetVectorParameterValue(TEXT("Color"), Tint);
			MID->SetScalarParameterValue(TEXT("Intensity"), Card == 0 ? 34.f : 24.f);
			MID->SetScalarParameterValue(TEXT("Rotation"), FMath::FRandRange(0.f, 6.283f));
			if (Mesh->FirstPersonPrimitiveType != ViewType)
			{
				Mesh->FirstPersonPrimitiveType = ViewType;
				Mesh->MarkRenderStateDirty();
			}
			Mesh->SetVisibility(true);
			if (CVarDebug.GetValueOnGameThread() != 0 && DebugLogged < 6)
			{
				++DebugLogged;
				const APlayerController* PC = GetWorld()->GetFirstPlayerController();
				UE_LOG(LogShieldbreak, Log, TEXT("ImpactFXDebug: card %d at %s scale %s rot %s visible %d fp %d owner %s attached %s; muzzle %s dir %s camera %s; material %s"),
					Card, *Mesh->GetComponentLocation().ToString(), *Mesh->GetComponentScale().ToString(), *Mesh->GetComponentRotation().ToString(),
					Mesh->IsVisible(), static_cast<int32>(Mesh->FirstPersonPrimitiveType), *GetNameSafe(Mesh->GetOwner()), *GetNameSafe(Mesh->GetAttachParent()),
					*Muzzle.ToString(), *Dir.ToString(), PC && PC->PlayerCameraManager ? *PC->PlayerCameraManager->GetCameraLocation().ToString() : TEXT("-"),
					*GetNameSafe(MID->Parent));
			}
		}
	}
	if (CVarLights.GetValueOnGameThread() != 0 && MuzzleLights.IsValidIndex(NextMuzzle))
	{
		UPointLightComponent* Light = MuzzleLights[NextMuzzle];
		Light->SetWorldLocation(Muzzle + Dir * 45.f);
		Light->SetLightColor(Tint);
		Light->SetAttenuationRadius(300.f + 100.f * S);
		Light->SetIntensity(Slot.Peak);
		Light->SetVisibility(true);
	}
	bActive = true;
}

UShieldbreakImpactFXSubsystem::FMuzzleRig* UShieldbreakImpactFXSubsystem::FindOrCreateMuzzleRig(USceneComponent* Gun)
{
	using namespace ShieldbreakImpactPrivate;
	if (FMuzzleRig* Found = MuzzleRigs.Find(Gun))
	{
		if (Found->Cards[0].IsValid())
		{
			return Found;
		}
	}
	UStaticMesh* Plane = ShieldbreakArt::Load<UStaticMesh>(PlaneMesh);
	UMaterialInterface* Material = LoadMaterial(MuzzleMaterial);
	AActor* GunActor = Gun ? Gun->GetOwner() : nullptr;
	if (!Plane || !Material || !GunActor)
	{
		return nullptr;
	}
	FMuzzleRig& Rig = MuzzleRigs.Add(Gun);
	for (int32 Card = 0; Card < CardsPerMuzzle; ++Card)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(GunActor, NAME_None, RF_Transient);
		Mesh->SetStaticMesh(Plane);
		QuietPrimitive(Mesh);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, Mesh);
		// Card 0 is the front star; 1 and 2 are side flames (two long spikes).
		MID->SetScalarParameterValue(TEXT("Spikes"), Card == 0 ? 6.f : 2.f);
		MID->SetScalarParameterValue(TEXT("Sharpness"), Card == 0 ? 10.f : 2.f);
		Mesh->SetMaterial(0, MID);
		Mesh->SetTranslucentSortPriority(4);
		Mesh->SetupAttachment(Gun);
		Mesh->SetVisibility(false);
		Mesh->RegisterComponent();
		Rig.Cards[Card] = Mesh;
		Rig.MIDs[Card] = MID;
	}
	return &Rig;
}

// ---- Simulation ----

FVector UShieldbreakImpactFXSubsystem::GetViewLocation() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	return Player && Player->PlayerCameraManager ? Player->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
}

void UShieldbreakImpactFXSubsystem::UpdateInstances(UInstancedStaticMeshComponent* Mesh, int32 Used, int32& LastUsed)
{
	if (!Mesh)
	{
		return;
	}
	// Instances past the live count are parked at zero scale until reused; the count only ever grows to the pool cap.
	const FTransform Parked(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
	for (int32 Index = Used; Index < LastUsed; ++Index)
	{
		Mesh->UpdateInstanceTransform(Index, Parked, false, false, true);
	}
	LastUsed = Used;
	Mesh->MarkRenderStateDirty();
	Mesh->SetVisibility(Used > 0);
}

void UShieldbreakImpactFXSubsystem::Tick(float DeltaTime)
{
	using namespace ShieldbreakImpactPrivate;
	UWorld* World = GetWorld();
	if (!World || !FXActor)
	{
		bActive = false;
		return;
	}
	const float Now = World->GetTimeSeconds();
	const float Dt = FMath::Min(DeltaTime, 0.05f);
	bool bStillActive = false;

	// Sparks: ballistic streaks that stretch with speed and cool from white-yellow to deep orange.
	if (SparkMesh)
	{
		static TArray<FTransform> Transforms;
		Transforms.Reset(Sparks.Num());
		for (int32 Index = Sparks.Num() - 1; Index >= 0; --Index)
		{
			FSpark& Spark = Sparks[Index];
			Spark.Age += Dt;
			if (Spark.Age >= Spark.Life)
			{
				Sparks.RemoveAtSwap(Index, EAllowShrinking::No);
				continue;
			}
			Spark.Velocity.Z -= Spark.Gravity * Dt;
			Spark.Velocity *= FMath::Exp(-Spark.Drag * Dt);
			Spark.Position += Spark.Velocity * Dt;
		}
		const int32 Live = Sparks.Num();
		while (SparkMesh->GetInstanceCount() < Live)
		{
			SparkMesh->AddInstance(FTransform::Identity, false);
		}
		float Custom[4];
		for (int32 Index = 0; Index < Live; ++Index)
		{
			const FSpark& Spark = Sparks[Index];
			const float Speed = Spark.Velocity.Size();
			const FVector Dir = Speed > 1.f ? Spark.Velocity / Speed : FVector::UpVector;
			const float Length = FMath::Clamp(Speed * 0.03f, 2.f, 30.f);
			const float T = Spark.Age / Spark.Life;
			const FLinearColor Color = FMath::Lerp(Spark.HotColor, Spark.CoolColor, FMath::Min(1.f, T * 1.4f));
			Transforms.Emplace(Dir.ToOrientationQuat(), Spark.Position - Dir * (Length * 0.5f),
				FVector(Length / 100.f, Spark.Width / 100.f, Spark.Width / 100.f));
			Custom[0] = Color.R;
			Custom[1] = Color.G;
			Custom[2] = Color.B;
			Custom[3] = Spark.Brightness * FMath::Pow(1.f - T, 1.5f);
			SparkMesh->SetCustomData(Index, MakeArrayView(Custom, 4), false);
		}
		if (Live > 0)
		{
			SparkMesh->BatchUpdateInstancesTransforms(0, Transforms, false, false, true);
		}
		UpdateInstances(SparkMesh, Live, LastSparkInstances);
		bStillActive |= Live > 0;
	}

	// Cards: camera-facing glows (additive) and puffs (translucent) that drift, grow and fade.
	if (GlowMesh || PuffMesh)
	{
		const FVector View = GetViewLocation();
		static TArray<FTransform> GlowTransforms;
		static TArray<FTransform> PuffTransforms;
		GlowTransforms.Reset();
		PuffTransforms.Reset();
		for (int32 Index = Cards.Num() - 1; Index >= 0; --Index)
		{
			FCard& Card = Cards[Index];
			Card.Age += Dt;
			if (Card.Age >= Card.Life)
			{
				Cards.RemoveAtSwap(Index, EAllowShrinking::No);
				continue;
			}
			Card.Velocity *= FMath::Exp(-Card.Drag * Dt);
			Card.Velocity.Z += (Card.bGlow ? 0.f : 18.f) * Dt;
			Card.Position += Card.Velocity * Dt;
			Card.Roll += Card.Spin * Dt;
		}
		int32 GlowIndex = 0;
		int32 PuffIndex = 0;
		float Custom[5];
		for (const FCard& Card : Cards)
		{
			UInstancedStaticMeshComponent* Mesh = Card.bGlow ? GlowMesh : PuffMesh;
			if (!Mesh)
			{
				continue;
			}
			const float T = Card.Age / Card.Life;
			const float Size = FMath::Lerp(Card.StartSize, Card.EndSize, EaseOut(T));
			const FVector ToView = (View - Card.Position).GetSafeNormal();
			const FQuat Facing = FQuat(ToView, Card.Roll) * FRotationMatrix::MakeFromZ(ToView).ToQuat();
			const FTransform Transform(Facing, Card.Position, FVector(Size / 100.f, Size / 100.f, 1.f));
			Custom[0] = Card.Color.R;
			Custom[1] = Card.Color.G;
			Custom[2] = Card.Color.B;
			if (Card.bGlow)
			{
				Custom[3] = Card.Peak * FMath::Square(1.f - T);
				const int32 Slot = GlowIndex++;
				if (Mesh->GetInstanceCount() <= Slot) { Mesh->AddInstance(FTransform::Identity, false); }
				Mesh->SetCustomData(Slot, MakeArrayView(Custom, 4), false);
				GlowTransforms.Add(Transform);
			}
			else
			{
				// Quick fade in (no pop), long fade out.
				Custom[3] = Card.Peak * FMath::Min(1.f, T / 0.08f) * FMath::Pow(1.f - T, 1.4f);
				Custom[4] = Card.Seed;
				const int32 Slot = PuffIndex++;
				if (Mesh->GetInstanceCount() <= Slot) { Mesh->AddInstance(FTransform::Identity, false); }
				Mesh->SetCustomData(Slot, MakeArrayView(Custom, 5), false);
				PuffTransforms.Add(Transform);
			}
		}
		if (GlowMesh)
		{
			if (GlowIndex > 0) { GlowMesh->BatchUpdateInstancesTransforms(0, GlowTransforms, false, false, true); }
			UpdateInstances(GlowMesh, GlowIndex, LastGlowInstances);
		}
		if (PuffMesh)
		{
			if (PuffIndex > 0) { PuffMesh->BatchUpdateInstancesTransforms(0, PuffTransforms, false, false, true); }
			UpdateInstances(PuffMesh, PuffIndex, LastPuffInstances);
		}
		bStillActive |= Cards.Num() > 0;
	}

	// Short lights: quadratic fall-off, then out of the scene entirely.
	for (int32 Index = 0; Index < LightSlots.Num(); ++Index)
	{
		FTimedSlot& Slot = LightSlots[Index];
		if (Slot.StartTime < 0.f) { continue; }
		const float K = 1.f - (Now - Slot.StartTime) / Slot.Duration;
		if (K <= 0.f)
		{
			Lights[Index]->SetVisibility(false);
			Slot.StartTime = -1.f;
			continue;
		}
		Lights[Index]->SetIntensity(Slot.Peak * K * K);
		bStillActive = true;
	}

	// Shield flares: the shell swells from the hit point and fades.
	for (int32 Index = 0; Index < FlareSlots.Num(); ++Index)
	{
		FTimedSlot& Slot = FlareSlots[Index];
		if (Slot.StartTime < 0.f) { continue; }
		const float T = (Now - Slot.StartTime) / Slot.Duration;
		if (T >= 1.f)
		{
			Flares[Index]->SetVisibility(false);
			Slot.StartTime = -1.f;
			continue;
		}
		Flares[Index]->SetWorldScale3D(FVector(Slot.Size * (0.35f + 0.65f * EaseOut(T))));
		FlareMIDs[Index]->SetScalarParameterValue(TEXT("Intensity"), Slot.Peak * FMath::Square(1.f - T));
		bStillActive = true;
	}

	// Muzzle flash lights: a touch longer than the cards.
	for (int32 Index = 0; Index < MuzzleSlots.Num(); ++Index)
	{
		FTimedSlot& Slot = MuzzleSlots[Index];
		if (Slot.StartTime < 0.f) { continue; }
		const float Age = Now - Slot.StartTime;
		const float LightK = 1.f - Age / (Slot.Duration * 1.4f);
		if (MuzzleLights.IsValidIndex(Index))
		{
			if (LightK <= 0.f)
			{
				MuzzleLights[Index]->SetVisibility(false);
			}
			else
			{
				MuzzleLights[Index]->SetIntensity(Slot.Peak * LightK * LightK);
			}
		}
		if (LightK <= 0.f)
		{
			Slot.StartTime = -1.f;
			continue;
		}
		bStillActive = true;
	}

	// Muzzle cards on each gun: fade over their few frames, then hide. Rigs of destroyed guns are dropped.
	for (auto It = MuzzleRigs.CreateIterator(); It; ++It)
	{
		FMuzzleRig& Rig = It.Value();
		if (!It.Key().IsValid() || !Rig.Cards[0].IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		if (Rig.StartTime < 0.f) { continue; }
		const float CardK = 1.f - (Now - Rig.StartTime) / FMath::Max(Rig.Duration, KINDA_SMALL_NUMBER);
		for (int32 Card = 0; Card < CardsPerMuzzle; ++Card)
		{
			UStaticMeshComponent* Mesh = Rig.Cards[Card].Get();
			UMaterialInstanceDynamic* MID = Rig.MIDs[Card].Get();
			if (!Mesh || !MID) { continue; }
			if (CardK <= 0.f) { Mesh->SetVisibility(false); }
			else { MID->SetScalarParameterValue(TEXT("Intensity"), (Card == 0 ? 34.f : 24.f) * CardK); }
		}
		if (CardK <= 0.f) { Rig.StartTime = -1.f; }
		else { bStillActive = true; }
	}

	// Bullet holes: fade over their last seconds, then hide (the slot is reused round-robin).
	const float DecalLife = FMath::Max(1.f, CVarDecalLife.GetValueOnGameThread());
	for (int32 Index = 0; Index < Decals.Num(); ++Index)
	{
		UDecalComponent* Decal = Decals[Index];
		if (!Decal->IsVisible()) { continue; }
		const float Age = Now - DecalTimes[Index];
		if (Age >= DecalLife)
		{
			Decal->SetVisibility(false);
			continue;
		}
		if (Age > DecalLife - DecalFade)
		{
			DecalMIDs[Index]->SetScalarParameterValue(TEXT("Opacity"), (DecalLife - Age) / DecalFade);
		}
		bStillActive = true;
	}

	bActive = bStillActive;
}
