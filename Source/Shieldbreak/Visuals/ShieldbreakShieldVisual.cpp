#include "Visuals/ShieldbreakShieldVisual.h"

#include "Shieldbreak.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Shield/ShieldHealthComponent.h"
#include "Visuals/ShieldbreakArt.h"

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
	Pulse(bBroken ? 1.f : FMath::Clamp(AbsorbedDamage / 35.f, 0.65f, 1.f), bBroken ? 0.5f : 0.32f,
		bOvershield ? FLinearColor(1.f, 0.65f, 0.12f) : FLinearColor(0.18f, 0.7f, 1.f));
}

void UShieldbreakShieldVisual::HandleBreak()
{
	// Replicated break count also covers a dropped cosmetic impact RPC.
	Pulse(1.f, 0.5f, FLinearColor(0.55f, 0.85f, 1.f));
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
	if (!IsPulseActive()) { ClearOverlay(); return; }
	const float Remaining = 1.f - (GetWorld()->GetTimeSeconds() - StartedAt) / PulseDuration;
	Intensity = PeakIntensity * FMath::SmoothStep(0.f, 1.f, Remaining);
	if (PulseMaterial) { PulseMaterial->SetScalarParameterValue(TEXT("Intensity"), Intensity); }
}
