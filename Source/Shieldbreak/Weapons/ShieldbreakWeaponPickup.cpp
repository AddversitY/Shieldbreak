#include "Weapons/ShieldbreakWeaponPickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/ShieldbreakCharacter.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/ShieldbreakWeapon.h"

AShieldbreakWeaponPickup::AShieldbreakWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetCanBeDamaged(false);

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(120.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(Trigger);
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BaseMesh->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
	BaseMesh->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.05f));

	DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
	DisplayMesh->SetupAttachment(Trigger);
	DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CubeFinder.Succeeded())
	{
		DisplayMesh->SetStaticMesh(CubeFinder.Object);
	}
	if (CylinderFinder.Succeeded())
	{
		BaseMesh->SetStaticMesh(CylinderFinder.Object);
	}
	if (MaterialFinder.Succeeded())
	{
		DisplayMesh->SetMaterial(0, MaterialFinder.Object);
		BaseMesh->SetMaterial(0, MaterialFinder.Object);
	}
}

void AShieldbreakWeaponPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShieldbreakWeaponPickup, bAvailable);
	DOREPLIFETIME(AShieldbreakWeaponPickup, WeaponClass);
}

void AShieldbreakWeaponPickup::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &AShieldbreakWeaponPickup::HandleOverlap);
	}
	UpdateVisuals();
}

void AShieldbreakWeaponPickup::SetWeaponClass(TSubclassOf<AShieldbreakWeapon> NewClass)
{
	WeaponClass = NewClass;
	UpdateVisuals();
}

void AShieldbreakWeaponPickup::OnRep_WeaponClass()
{
	UpdateVisuals();
}

void AShieldbreakWeaponPickup::UpdateVisuals()
{
	const AShieldbreakWeapon* WeaponDefaults = WeaponClass ? WeaponClass->GetDefaultObject<AShieldbreakWeapon>() : nullptr;
	const FLinearColor WeaponColor = WeaponDefaults ? WeaponDefaults->GetStats().TracerColor : FLinearColor::White;

	// The weapon's own model when it has one (shown 1.25x so it reads from a distance), else a tinted greybox block.
	UStaticMesh* Art = WeaponDefaults ? AShieldbreakWeapon::LoadArtMesh(WeaponDefaults->GetStats()) : nullptr;
	if (Art)
	{
		constexpr float ArtScale = 1.25f;
		DisplayMesh->SetStaticMesh(Art);
		DisplayMesh->EmptyOverrideMaterials();
		DisplayMesh->SetRelativeScale3D(FVector(ArtScale));
		DisplayCenter = Art->GetBoundingBox().GetCenter() * ArtScale;
	}
	else if (WeaponDefaults)
	{
		DisplayMesh->SetRelativeScale3D(WeaponDefaults->GetStats().MeshScale * 1.4f);
		DisplayCenter = FVector::ZeroVector;
	}
	if (!Art)
	{
		if (UMaterialInstanceDynamic* MID = DisplayMesh->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), WeaponColor);
		}
	}
	if (UMaterialInstanceDynamic* BaseMID = BaseMesh->CreateDynamicMaterialInstance(0))
	{
		BaseMID->SetVectorParameterValue(TEXT("Color"), bAvailable ? WeaponColor * 0.5f : FLinearColor(0.08f, 0.08f, 0.08f));
	}

	DisplayMesh->SetVisibility(bAvailable);
}

void AShieldbreakWeaponPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Local cosmetic spin and bob.
	if (GetNetMode() != NM_DedicatedServer && bAvailable)
	{
		SpinAngle = FMath::Fmod(SpinAngle + 90.f * DeltaSeconds, 360.f);
		BobTime += DeltaSeconds;
		const FRotator Spin(0.f, SpinAngle, 0.f);
		DisplayMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 10.f + 8.f * FMath::Sin(BobTime * 2.5f)) - Spin.RotateVector(DisplayCenter), Spin);
	}
}

void AShieldbreakWeaponPickup::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bAvailable || !WeaponClass)
	{
		return;
	}

	AShieldbreakCharacter* Character = Cast<AShieldbreakCharacter>(OtherActor);
	if (!Character || !Character->IsAlive())
	{
		return;
	}

	// Already carrying one: top up its reserve.
	for (AShieldbreakWeapon* Carried : { Character->GetCurrentWeapon(), Character->GetHolsteredWeapon() })
	{
		if (Carried && Carried->GetClass() == WeaponClass.Get())
		{
			if (Carried->GetStats().bUsesHeat || Carried->GetReserveAmmo() >= Carried->GetStats().MaxReserveAmmo)
			{
				return;
			}
			Carried->AddReserveAmmo(AmmoGrant);
			SetAvailable(false);
			return;
		}
	}
}

bool AShieldbreakWeaponPickup::TryGiveTo(AShieldbreakCharacter* Character)
{
	if (!HasAuthority() || !bAvailable || !WeaponClass || !Character || !Character->IsAlive())
	{
		return false;
	}

	for (AShieldbreakWeapon* Carried : { Character->GetCurrentWeapon(), Character->GetHolsteredWeapon() })
	{
		if (Carried && Carried->GetClass() == WeaponClass.Get())
		{
			return false;
		}
	}

	Character->GiveWeapon(WeaponClass);
	SetAvailable(false);
	return true;
}

void AShieldbreakWeaponPickup::SetAvailable(bool bNewAvailable)
{
	bAvailable = bNewAvailable;
	UpdateVisuals();
	ForceNetUpdate();

	if (!bAvailable)
	{
		GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AShieldbreakWeaponPickup::Respawn, RespawnTime, false);
	}
}

void AShieldbreakWeaponPickup::Respawn()
{
	SetAvailable(true);
}

void AShieldbreakWeaponPickup::OnRep_Available()
{
	UpdateVisuals();
}
