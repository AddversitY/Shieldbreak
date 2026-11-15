// Bullet hits and muzzle flashes, built for many shots per second at a fixed, small cost.
//
// Everything here is cosmetic and local to the machine that shows it (never on a dedicated server, never replicated).
// One world subsystem owns fixed pools that are reused round-robin, so a 4v4 firefight never allocates:
//  - sparks: CPU-simulated streaks on ONE instanced cube mesh (one draw call for every spark in the match),
//  - glows and dust/smoke puffs: camera-facing cards on two more instanced meshes,
//  - bullet-hole decals in a ring buffer that fade out,
//  - small shield-flare shells at the exact point a shot meets a shield,
//  - a few short, shadowless impact lights and muzzle-flash slots (cards + light) that follow the gun.
// Materials come from Tools/create_impact_fx_materials.py (Tools/CreateImpactFX.bat). Without them the matching layer
// is skipped and the rest still plays. sb.ImpactFX 0 turns it all off for FPS A/B.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ShieldbreakImpactFX.generated.h"

class AActor;
class UDecalComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
struct FHitResult;

UENUM()
enum class EShieldbreakImpactSurface : uint8
{
	/** Forerunner-style alloy, concrete, anything hard: hot sparks, a little grit, a bullet hole. */
	Metal,
	/** Ground, rock, sand, grass: a dust kick and dark divot, few sparks. */
	Dirt,
	/** Water: a white splash, no hole. */
	Water,
	/** A soldier whose shield is down: armor sparks and a dark puff. */
	Armor,
	/** A soldier's shield took it: a blue flare where the round hit. */
	Shield,
	/** Overshield: the same flare in amber. */
	Overshield,
};

UCLASS()
class SHIELDBREAK_API UShieldbreakImpactFXSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Null on dedicated servers and in editor (non-game) worlds. */
	static UShieldbreakImpactFXSubsystem* Get(const UObject* WorldContext);

	/** Strength from weapon damage: battle rifle round ~0.75, sidearm 1, sniper ~2.2, rail ~2.5. */
	static float StrengthForDamage(float Damage);

	/** What a hit struck. Characters report their shield state at the moment of the hit. */
	static EShieldbreakImpactSurface ClassifySurface(const FHitResult& Hit);

	/** A bullet landing. ShotDir is the round's travel direction; Tint colours energy weapons' sparks (alpha = how much). */
	void BulletImpact(const FHitResult& Hit, const FVector& ShotDir, float Strength, const FLinearColor& Tint = FLinearColor(1.f, 1.f, 1.f, 0.f));

	/** Muzzle flash at Muzzle, along Direction, following AttachTo (the gun mesh; its first-person draw mode is copied). */
	void MuzzleFlash(USceneComponent* AttachTo, const FVector& Muzzle, const FVector& Direction, const FLinearColor& Tint, float Strength);

	/** Whether Lyra's Niagara muzzle flash still plays under ours (sb.LyraMuzzleFlash). */
	static bool UseLyraMuzzleFlash();

	// Self-test and capture introspection.
	int32 GetLiveSparkCount() const { return Sparks.Num(); }
	int32 GetLiveCardCount() const { return Cards.Num(); }
	int32 GetDecalsPlaced() const { return DecalsPlaced; }
	int32 GetImpactCount() const { return ImpactCount; }
	int32 GetMuzzleFlashCount() const { return MuzzleFlashCount; }
	bool HasMaterials() const { return SparkMesh != nullptr; }
	EShieldbreakImpactSurface GetLastSurface() const { return LastSurface; }
	int32 GetSurfaceCount(EShieldbreakImpactSurface Surface) const { return SurfaceCounts[static_cast<int32>(Surface)]; }

	// UTickableWorldSubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bActive; }

private:
	struct FSpark
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FLinearColor HotColor = FLinearColor::White;
		FLinearColor CoolColor = FLinearColor::White;
		float Age = 0.f;
		float Life = 0.3f;
		float Width = 0.4f;
		float Brightness = 20.f;
		float Drag = 2.f;
		float Gravity = 980.f;
	};

	struct FCard
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FLinearColor Color = FLinearColor::White;
		float Age = 0.f;
		float Life = 0.5f;
		float StartSize = 10.f;
		float EndSize = 30.f;
		/** Peak opacity (puffs) or brightness (glows). */
		float Peak = 1.f;
		float Roll = 0.f;
		float Spin = 0.f;
		float Seed = 0.f;
		float Drag = 3.f;
		bool bGlow = false;
	};

	struct FTimedSlot
	{
		float StartTime = -1.f;
		float Duration = 0.f;
		float Peak = 0.f;
		float Size = 1.f;
	};

	/** Front star and two crossed side flames, owned by the gun's actor (first-person primitives only draw for their owner). */
	struct FMuzzleRig
	{
		TWeakObjectPtr<UStaticMeshComponent> Cards[3];
		TWeakObjectPtr<UMaterialInstanceDynamic> MIDs[3];
		float StartTime = -1.f;
		float Duration = 0.f;
	};
	FMuzzleRig* FindOrCreateMuzzleRig(USceneComponent* Gun);
	TMap<TWeakObjectPtr<USceneComponent>, FMuzzleRig> MuzzleRigs;

	bool EnsureActor();
	void SpawnSparks(const FVector& Point, const FVector& Normal, const FVector& Reflect, int32 Count, float SpeedMin, float SpeedMax,
		float ConeDegrees, float Life, const FLinearColor& Hot, const FLinearColor& Cool, float Brightness, float Width, float Gravity);
	void AddCard(bool bGlow, const FVector& Position, const FVector& Velocity, const FLinearColor& Color, float Life,
		float StartSize, float EndSize, float Peak, float Drag = 3.f);
	void PlaceDecal(const FHitResult& Hit, float Size, const FLinearColor& Tint, float Darkness, float Rim);
	void FlashLight(const FVector& Position, const FLinearColor& Color, float Candelas, float Radius, float Duration);
	void ShieldFlare(const FVector& Point, const FVector& Normal, const FLinearColor& Color, float Strength);
	void UpdateInstances(UInstancedStaticMeshComponent* Mesh, int32 Used, int32& LastUsed);
	FVector GetViewLocation() const;

	UPROPERTY(Transient) TObjectPtr<AActor> FXActor;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> SparkMesh;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> GlowMesh;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> PuffMesh;
	UPROPERTY(Transient) TArray<TObjectPtr<UDecalComponent>> Decals;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> DecalMIDs;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Flares;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> FlareMIDs;
	/** Muzzle flash lights, pooled; the cards live on each gun (FMuzzleRig). */
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> MuzzleLights;

	TArray<float> DecalTimes;
	TArray<FSpark> Sparks;
	TArray<FCard> Cards;
	TArray<FTimedSlot> LightSlots;
	TArray<FTimedSlot> FlareSlots;
	TArray<FTimedSlot> MuzzleSlots;
	int32 NextDecal = 0;
	int32 NextLight = 0;
	int32 NextFlare = 0;
	int32 NextMuzzle = 0;
	int32 LastSparkInstances = 0;
	int32 LastGlowInstances = 0;
	int32 LastPuffInstances = 0;
	int32 DecalsPlaced = 0;
	int32 ImpactCount = 0;
	int32 MuzzleFlashCount = 0;
	int32 SurfaceCounts[6] = {};
	EShieldbreakImpactSurface LastSurface = EShieldbreakImpactSurface::Metal;
	bool bActive = false;
	bool bTriedActor = false;
};
