#include "Visuals/ShieldbreakExplosionFX.h"

#include "Shieldbreak.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Visuals/ShieldbreakArt.h"

namespace ShieldbreakExplosionPrivate
{
	TAutoConsoleVariable<float> CVarFlash(TEXT("sb.ExplosionFlash"), 800.f, TEXT("Peak candelas of an explosion's flash light (0 = no light)."));
	TAutoConsoleVariable<float> CVarShake(TEXT("sb.ExplosionShake"), 1.f, TEXT("Explosion camera shake strength (0 = off)."));
	TAutoConsoleVariable<int32> CVarCards(TEXT("sb.ExplosionCards"), 1, TEXT("1: fireball and smoke cards; 0: light, scorch and shake only (for FPS A/B)."));
	TAutoConsoleVariable<int32> CVarScorch(TEXT("sb.ExplosionScorch"), 1, TEXT("1: explosions leave a scorch decal that fades out."));

	const TCHAR* const FireMaterial = TEXT("/Game/Shieldbreak/Art/FX/M_SB_FXFire.M_SB_FXFire");
	const TCHAR* const SmokeMaterial = TEXT("/Game/Shieldbreak/Art/FX/M_SB_FXSmoke.M_SB_FXSmoke");
	const TCHAR* const ScorchMaterial = TEXT("/Game/Shieldbreak/Art/FX/M_SB_Scorch.M_SB_Scorch");
	const TCHAR* const CardMesh = TEXT("/Engine/BasicShapes/Plane.Plane");
	/** Frames in the fire flipbook (the material holds its columns and rows). */
	constexpr float FireFrames = 64.f;

	float EaseOut(float T, float Power) { return 1.f - FMath::Pow(1.f - FMath::Clamp(T, 0.f, 1.f), Power); }
}

// ---- Shake ----

void UShieldbreakJoltShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(Duration);
}

void UShieldbreakJoltShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Time = 0.f;
	for (float& Phase : Phases)
	{
		Phase = FMath::FRandRange(0.f, 2.f * UE_PI);
	}
}

void UShieldbreakJoltShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	Time += Params.DeltaTime;
	const float Decay = FMath::Square(FMath::Max(0.f, 1.f - Time / FMath::Max(0.05f, Duration)));
	const float Scale = Params.GetTotalScale() * Decay;
	auto Wave = [this](int32 Axis)
	{
		const float W = 2.f * UE_PI * Frequency;
		return 0.65f * FMath::Sin(W * Time + Phases[Axis]) + 0.35f * FMath::Sin(W * 1.73f * Time + Phases[(Axis + 3) % 6]);
	};
	OutResult.Rotation = FRotator(RotationAmplitude.X * Wave(0), RotationAmplitude.Y * Wave(1), RotationAmplitude.Z * Wave(2)) * Scale;
	OutResult.Location = FVector(LocationAmplitude.X * Wave(3), LocationAmplitude.Y * Wave(4), LocationAmplitude.Z * Wave(5)) * Scale;
}

bool UShieldbreakJoltShakePattern::IsFinishedImpl() const
{
	return Time >= Duration;
}

UShieldbreakExplosionShake::UShieldbreakExplosionShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = false;
	SetRootShakePattern(CreateDefaultSubobject<UShieldbreakJoltShakePattern>(TEXT("Jolt")));
}

// ---- Explosion ----

AShieldbreakExplosionFX::AShieldbreakExplosionFX()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bReplicates = false;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AShieldbreakExplosionFX* AShieldbreakExplosionFX::Spawn(UWorld* World, const FVector& Location, float Scale)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	AShieldbreakExplosionFX* FX = World->SpawnActor<AShieldbreakExplosionFX>(StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (FX)
	{
		FX->FXScale = FMath::Max(0.1f, Scale);
		FX->FinishSpawning(FTransform(Location));
	}
	return FX;
}

void AShieldbreakExplosionFX::BeginPlay()
{
	Super::BeginPlay();
	Build();
	SpawnScorch();
	ShakeLocalPlayers();
	SetLifeSpan(Duration + 0.1f);
}

void AShieldbreakExplosionFX::Build()
{
	using namespace ShieldbreakExplosionPrivate;
	const float S = FXScale;

	const float FlashCandelas = CVarFlash.GetValueOnGameThread();
	if (FlashCandelas > 0.f)
	{
		Flash = NewObject<UPointLightComponent>(this, TEXT("Flash"));
		Flash->SetupAttachment(RootComponent);
		Flash->SetRelativeLocation(FVector(0.f, 0.f, 40.f * S));
		Flash->SetMobility(EComponentMobility::Movable);
		Flash->SetIntensityUnits(ELightUnits::Candelas);
		Flash->SetIntensity(FlashCandelas * S);
		Flash->SetAttenuationRadius(700.f * S);
		Flash->SetSourceRadius(40.f * S);
		Flash->SetLightColor(FLinearColor(1.f, 0.62f, 0.3f));
		Flash->SetCastShadows(false);
		Flash->SetAffectTranslucentLighting(false);
		Flash->SetVolumetricScatteringIntensity(0.f);
		Flash->RegisterComponent();
	}

	if (CVarCards.GetValueOnGameThread() == 0)
	{
		Duration = 0.6f;
		return;
	}
	UMaterialInterface* Fire = ShieldbreakArt::Load<UMaterialInterface>(FireMaterial);
	UMaterialInterface* Smoke = ShieldbreakArt::Load<UMaterialInterface>(SmokeMaterial);
	if (!Fire || !Smoke || !ShieldbreakArt::Load<UStaticMesh>(CardMesh))
	{
		Duration = 0.6f;
		return;
	}

	auto Jitter = [S](float Horizontal, float Up) { return FVector(FMath::FRandRange(-Horizontal, Horizontal), FMath::FRandRange(-Horizontal, Horizontal), Up) * S; };
	// Hot core: one big bright card for the first instant (the "pop").
	AddCard(false, 0.f, 0.12f, 110.f * S, 240.f * S, FVector(0.f, 0.f, 45.f * S), FVector::ZeroVector);
	// Fireball: three rolling flipbooks, offset so it isn't a flat disc, each a little later and higher.
	for (int32 Index = 0; Index < 3; ++Index)
	{
		AddCard(false, 0.02f * Index, 0.62f + 0.08f * Index, 140.f * S, 300.f * S, Jitter(30.f, 50.f + 25.f * Index), FVector(0.f, 0.f, 70.f * S));
	}
	// Smoke: rises and spreads for a few seconds, darker and slower than the fire; the lowest puff hugs the floor.
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FVector Out = Jitter(70.f, 30.f + 35.f * Index);
		AddCard(true, 0.06f + 0.05f * Index, 2.4f + 0.25f * Index, 170.f * S, (380.f + 50.f * Index) * S, Out,
			FVector(Out.X * 0.4f, Out.Y * 0.4f, (45.f + 12.f * Index) * S));
	}
	for (const FCard& Card : Cards)
	{
		Duration = FMath::Max(Duration, Card.Delay + Card.Life);
	}
}

void AShieldbreakExplosionFX::AddCard(bool bSmoke, float Delay, float Life, float StartSize, float EndSize, const FVector& Offset, const FVector& Drift)
{
	using namespace ShieldbreakExplosionPrivate;
	UMaterialInterface* Material = ShieldbreakArt::Load<UMaterialInterface>(bSmoke ? SmokeMaterial : FireMaterial);
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
	Mesh->SetStaticMesh(ShieldbreakArt::Load<UStaticMesh>(CardMesh));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	Mesh->bAffectDistanceFieldLighting = false;
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetMobility(EComponentMobility::Movable);
	// Smoke sorts behind the fire.
	Mesh->SetTranslucentSortPriority(bSmoke ? 0 : 1);
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetHiddenInGame(true);
	Mesh->RegisterComponent();
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
	Mesh->SetMaterial(0, MID);
	CardMeshes.Add(Mesh);
	CardMIDs.Add(MID);

	FCard& Card = Cards.AddDefaulted_GetRef();
	Card.Mesh = Mesh;
	Card.MID = MID;
	Card.bSmoke = bSmoke;
	Card.Delay = Delay;
	Card.Life = Life;
	Card.StartSize = StartSize;
	Card.EndSize = EndSize;
	Card.Offset = Offset;
	Card.Drift = Drift;
	Card.Roll = FMath::FRandRange(0.f, 360.f);
	Card.Spin = FMath::FRandRange(-40.f, 40.f) * (bSmoke ? 0.4f : 1.f);
	Card.StartFrame = bSmoke ? FMath::FRandRange(0.f, 8.f) : 0.f;
}

void AShieldbreakExplosionFX::SpawnScorch()
{
	using namespace ShieldbreakExplosionPrivate;
	UWorld* World = GetWorld();
	UMaterialInterface* Scorch = CVarScorch.GetValueOnGameThread() != 0 ? ShieldbreakArt::Load<UMaterialInterface>(ScorchMaterial) : nullptr;
	if (!World || !Scorch)
	{
		return;
	}
	FHitResult Hit;
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 20.f);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ShieldbreakScorch), false);
	if (!World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, 180.f * FXScale), ECC_Visibility, Query))
	{
		return;
	}
	FRotator Rotation = (-Hit.ImpactNormal).Rotation();
	Rotation.Roll = FMath::FRandRange(0.f, 360.f);
	if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(World, Scorch, FVector(30.f, 135.f, 135.f) * FXScale, Hit.ImpactPoint, Rotation, 14.f))
	{
		Decal->SetFadeScreenSize(0.002f);
		Decal->SetFadeOut(9.f, 5.f, false);
	}
}

void AShieldbreakExplosionFX::ShakeLocalPlayers()
{
	using namespace ShieldbreakExplosionPrivate;
	const float Strength = CVarShake.GetValueOnGameThread();
	UWorld* World = GetWorld();
	if (!World || Strength <= 0.f)
	{
		return;
	}
	const float Inner = 300.f * FXScale;
	const float Outer = 1800.f * FXScale;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager)
		{
			continue;
		}
		const float Distance = static_cast<float>(FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), GetActorLocation()));
		const float Falloff = FMath::Square(1.f - FMath::Clamp((Distance - Inner) / (Outer - Inner), 0.f, 1.f));
		if (Falloff > 0.01f)
		{
			PC->ClientStartCameraShake(UShieldbreakExplosionShake::StaticClass(), Strength * Falloff);
		}
	}
}

void AShieldbreakExplosionFX::Tick(float DeltaSeconds)
{
	using namespace ShieldbreakExplosionPrivate;
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (Flash)
	{
		// A sharp flash, then the fireball's own glow fading with it.
		const float Peak = CVarFlash.GetValueOnGameThread() * FXScale;
		// The 30k cd first version whited out the whole frame at 7 m for 0.3 s (desktop G3 shots).
		const float Glow = 0.25f * FMath::Max(0.f, 1.f - Age / 0.7f);
		const float Intensity = Peak * (FMath::Exp(-Age / 0.05f) + Glow);
		Flash->SetIntensity(Intensity);
		Flash->SetLightColor(FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.75f, 0.45f), FLinearColor(1.f, 0.35f, 0.08f), FMath::Clamp(Age / 0.5f, 0.f, 1.f)));
		if (Age > 0.75f)
		{
			Flash->DestroyComponent();
			Flash = nullptr;
		}
	}

	// Cards face the local view (there is one camera per machine that draws this).
	FVector Eye = GetActorLocation() + FVector(-1000.f, 0.f, 0.f);
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (PC->PlayerCameraManager)
		{
			Eye = PC->PlayerCameraManager->GetCameraLocation();
		}
	}
	for (FCard& Card : Cards)
	{
		const float T = (Age - Card.Delay) / Card.Life;
		if (T < 0.f || T > 1.f)
		{
			Card.Mesh->SetHiddenInGame(true);
			continue;
		}
		Card.Mesh->SetHiddenInGame(false);
		const float Grow = EaseOut(T, Card.bSmoke ? 2.2f : 3.f);
		const float Size = FMath::Lerp(Card.StartSize, Card.EndSize, Grow);
		// Drift slows down as it spreads (the smoke "hangs").
		const FVector Location = GetActorLocation() + Card.Offset + Card.Drift * Card.Life * EaseOut(T, 2.f);
		const FVector ToEye = (Eye - Location).GetSafeNormal();
		FRotator Facing = FRotationMatrix::MakeFromZ(ToEye).Rotator();
		const FQuat Roll(ToEye, FMath::DegreesToRadians(Card.Roll + Card.Spin * (Age - Card.Delay)));
		Card.Mesh->SetWorldLocationAndRotation(Location, Roll * Facing.Quaternion());
		Card.Mesh->SetWorldScale3D(FVector(Size / 100.f));

		if (Card.bSmoke)
		{
			// In fast, out slow; lit orange by the fire for its first moments, then a warm dark grey.
			// Mid grey rather than near black: the first version vanished against Keel's dark corridors (G3 shots).
			const float Opacity = FMath::Min(1.f, T / 0.08f) * FMath::Pow(1.f - T, 0.9f) * 0.85f;
			const float Firelit = FMath::Max(0.f, 1.f - (Age - Card.Delay) / 0.4f);
			Card.MID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			Card.MID->SetVectorParameterValue(TEXT("Color"), FLinearColor::LerpUsingHSV(FLinearColor(0.2f, 0.185f, 0.17f), FLinearColor(1.f, 0.45f, 0.15f), Firelit));
			Card.MID->SetScalarParameterValue(TEXT("Frame"), Card.StartFrame + T * 6.f);
		}
		else
		{
			// White-hot to orange to dull red as it burns out.
			const FLinearColor Hot(1.f, 0.85f, 0.6f), Mid(1.f, 0.45f, 0.1f), Cool(0.5f, 0.1f, 0.03f);
			const FLinearColor Color = T < 0.3f ? FLinearColor::LerpUsingHSV(Hot, Mid, T / 0.3f) : FLinearColor::LerpUsingHSV(Mid, Cool, (T - 0.3f) / 0.7f);
			Card.MID->SetVectorParameterValue(TEXT("Color"), Color);
			Card.MID->SetScalarParameterValue(TEXT("Opacity"), FMath::Pow(1.f - T, 1.2f));
			// Additive: 10 (and still 5) blew the whole frame white at 5 m (G3/G4 shots); the auto exposure is low indoors.
			Card.MID->SetScalarParameterValue(TEXT("Intensity"), 1.5f);
			Card.MID->SetScalarParameterValue(TEXT("Frame"), FMath::Min(FireFrames - 1.f, Card.StartFrame + T * (FireFrames - 1.f)));
		}
	}
}
