#include "Visuals/ShieldbreakShieldVisual.h"

#include "Shieldbreak.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Shield/ShieldHealthComponent.h"
#include "Visuals/ShieldbreakArt.h"

namespace ShieldbreakShieldVisualPrivate
{
	constexpr float BurstDuration = 0.3f;
	/** Engine sphere (100 cm across), scaled to an egg around the 184 cm capsule. */
	const FVector BurstStartScale(1.05f, 1.05f, 1.95f);
	const FVector BurstEndScale(1.6f, 1.6f, 2.5f);
}

UShieldbreakShieldVisual::UShieldbreakShieldVisual()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UShieldbreakShieldVisual::BeginPlay()
{
	Super::BeginPlay();
	if (UShieldHealthComponent* Shield = GetOwner()->FindComponentByClass<UShieldHealthComponent>())
	{
		Shield->OnShieldImpact.AddDynamic(this, &UShieldbreakShieldVisual::HandleImpact);
		Shield->OnShieldBroken.AddDynamic(this, &UShieldbreakShieldVisual::HandleBreak);
	}
	if (GetNetMode() != NM_DedicatedServer && FApp::CanEverRender())
	{
		Material = ShieldbreakArt::Load<UMaterialInterface>(TEXT("/Game/Shieldbreak/Art/Materials/M_SB_ShieldImpact.M_SB_ShieldImpact"));
	}
}

bool UShieldbreakShieldVisual::IsPulseActive() const
{
	return GetWorld() && StartedAt >= 0.f && GetWorld()->GetTimeSeconds() < StartedAt + PulseDuration;
}

void UShieldbreakShieldVisual::HandleImpact(float AbsorbedDamage, bool bBroken, bool bOvershield)
{
	++ImpactCount;
	if (FParse::Param(FCommandLine::Get(), TEXT("SBShieldFeedbackTest")) || FParse::Param(FCommandLine::Get(), TEXT("SBShieldShots")))
	{
		UE_LOG(LogShieldbreak, Log, TEXT("ShieldFeedback: impact=%d absorbed=%.1f broken=%d overshield=%d role=%d"),
			ImpactCount, AbsorbedDamage, bBroken, bOvershield, static_cast<int32>(GetOwner()->GetLocalRole()));
	}
	// Ordinary hits shimmer at about half strength: at full strength the bloom whited out the whole soldier (FX1/FX2
	// captures), and the localised flare at the hit point (ShieldbreakImpactFX) now carries the "where".
	Pulse(bBroken ? 1.f : FMath::Clamp(AbsorbedDamage / 35.f, 0.65f, 1.f) * 0.45f, bBroken ? 0.5f : 0.32f,
		bOvershield ? FLinearColor(1.f, 0.65f, 0.12f) : FLinearColor(0.18f, 0.7f, 1.f));
}

void UShieldbreakShieldVisual::HandleBreak()
{
	// Replicated break count also covers a dropped cosmetic impact RPC.
	Pulse(1.f, 0.5f, FLinearColor(0.55f, 0.85f, 1.f));
	StartBurst();
}

bool UShieldbreakShieldVisual::IsBurstActive() const
{
	return GetWorld() && BurstStartedAt >= 0.f && GetWorld()->GetTimeSeconds() < BurstStartedAt + ShieldbreakShieldVisualPrivate::BurstDuration;
}

void UShieldbreakShieldVisual::StartBurst()
{
	using namespace ShieldbreakShieldVisualPrivate;
	AActor* Owner = GetOwner();
	if (!Owner || !GetWorld() || GetNetMode() == NM_DedicatedServer || !FApp::CanEverRender())
	{
		return;
	}
	const FVector Chest = Owner->GetActorLocation() + FVector(0.f, 0.f, 30.f);
	ShieldbreakArt::SpawnFX(this, ShieldbreakArt::ImpactCharacter, Chest, FRotator(90.f, 0.f, 0.f), 1.8f);

	// One reused, collision-free, shadowless sphere per character: it costs nothing until a shield pops.
	if (!BurstMesh && Material)
	{
		if (UStaticMesh* Sphere = ShieldbreakArt::Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere")))
		{
			BurstMesh = NewObject<UStaticMeshComponent>(Owner, TEXT("ShieldBurst"));
			BurstMesh->SetStaticMesh(Sphere);
			BurstMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BurstMesh->SetCastShadow(false);
			BurstMesh->SetCanEverAffectNavigation(false);
			BurstMesh->bReceivesDecals = false;
			// The popped player sees the blue flash on their HUD instead of a shell around their own camera.
			BurstMesh->SetOwnerNoSee(true);
			BurstMesh->SetupAttachment(Owner->GetRootComponent());
			BurstMesh->RegisterComponent();
			BurstMaterial = UMaterialInstanceDynamic::Create(Material, this);
			BurstMaterial->SetVectorParameterValue(TEXT("ShieldColor"), FLinearColor(0.55f, 0.85f, 1.f));
			BurstMesh->SetMaterial(0, BurstMaterial);
		}
	}
	if (!BurstMesh)
	{
		return;
	}
	BurstStartedAt = GetWorld()->GetTimeSeconds();
	BurstMesh->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
	BurstMesh->SetRelativeScale3D(BurstStartScale);
	BurstMesh->SetVisibility(true);
	UpdateBurst();
	SetComponentTickEnabled(true);
}

void UShieldbreakShieldVisual::UpdateBurst()
{
	using namespace ShieldbreakShieldVisualPrivate;
	if (!BurstMesh || !BurstMesh->IsVisible())
	{
		return;
	}
	if (!IsBurstActive())
	{
		BurstMesh->SetVisibility(false);
		return;
	}
	const float T = (GetWorld()->GetTimeSeconds() - BurstStartedAt) / BurstDuration;
	const float Ease = 1.f - FMath::Square(1.f - T);
	BurstMesh->SetRelativeScale3D(FMath::Lerp(BurstStartScale, BurstEndScale, Ease));
	if (BurstMaterial)
	{
		BurstMaterial->SetScalarParameterValue(TEXT("Intensity"), 1.4f * FMath::Square(1.f - T));
	}
}

void UShieldbreakShieldVisual::Pulse(float Strength, float Duration, const FLinearColor& Color)
{
	if (!GetWorld()) { return; }
	StartedAt = GetWorld()->GetTimeSeconds();
	PulseDuration = Duration;
	PeakIntensity = Intensity = Strength;
	if (Material && !PulseMaterial) { PulseMaterial = UMaterialInstanceDynamic::Create(Material, this); }
	if (PulseMaterial)
	{
		PulseMaterial->SetVectorParameterValue(TEXT("ShieldColor"), Color);
		PulseMaterial->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		ApplyOverlay();
	}
	SetComponentTickEnabled(GetNetMode() != NM_DedicatedServer);
}

void UShieldbreakShieldVisual::ApplyOverlay()
{
	if (!PulseMaterial) { return; }
	TInlineComponentArray<UMeshComponent*> Meshes(GetOwner());
	for (UMeshComponent* Mesh : Meshes)
	{
		const FString Name = Mesh->GetName();
		if (Name == TEXT("CharacterMesh0") || Name == TEXT("FirstPersonArms") || Name == TEXT("BodyMesh")
			|| Name.StartsWith(TEXT("Armor_")) || Name.StartsWith(TEXT("FPArmor_")))
		{
			Mesh->SetOverlayMaterial(PulseMaterial);
			PulseMeshes.AddUnique(Mesh);
		}
	}
}

void UShieldbreakShieldVisual::ClearOverlay()
{
	for (const TWeakObjectPtr<UMeshComponent>& WeakMesh : PulseMeshes)
	{
		if (UMeshComponent* Mesh = WeakMesh.Get())
		{
			if (Mesh->GetOverlayMaterial() == PulseMaterial) { Mesh->SetOverlayMaterial(nullptr); }
		}
	}
	PulseMeshes.Reset();
	Intensity = 0.f;
	SetComponentTickEnabled(false);
}

void UShieldbreakShieldVisual::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateBurst();
	if (!IsPulseActive()) { ClearOverlay(); return; }
	const float Remaining = 1.f - (GetWorld()->GetTimeSeconds() - StartedAt) / PulseDuration;
	Intensity = PeakIntensity * FMath::SmoothStep(0.f, 1.f, Remaining);
	if (PulseMaterial) { PulseMaterial->SetScalarParameterValue(TEXT("Intensity"), Intensity); }
}
