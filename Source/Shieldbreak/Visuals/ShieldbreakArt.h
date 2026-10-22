// Imported art (Lyra Starter Game content, see docs/ASSET_CREDITS.md). Everything is loaded at runtime and
// optional: when an asset is missing (e.g. a clone made with GIT_LFS_SKIP_SMUDGE=1) callers keep the greybox.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Casts.h"
#include "UObject/Object.h"

class UNiagaraComponent;
class USceneComponent;

namespace ShieldbreakArt
{
	// Characters
	inline constexpr const TCHAR* MannyMesh = TEXT("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny");
	/** Folder of the MM_Rifle_* locomotion sequences; append e.g. "Jog_Fwd". */
	inline constexpr const TCHAR* RifleLocomotionPrefix = TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_");

	// Weapons
	inline constexpr const TCHAR* RifleMesh = TEXT("/Game/Weapons/Rifle/Mesh/SM_Rifle.SM_Rifle");
	inline constexpr const TCHAR* PistolMesh = TEXT("/Game/Weapons/Pistol/Mesh/SM_Pistol.SM_Pistol");
	inline constexpr const TCHAR* ShotgunMesh = TEXT("/Game/Weapons/Shotgun/Mesh/SM_Shotgun.SM_Shotgun");
	inline constexpr const TCHAR* GrenadeMesh = TEXT("/Game/Weapons/Grenade/Mesh/SM_grenade.SM_grenade");

	// Niagara
	inline constexpr const TCHAR* MuzzleFlashRifle = TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_MuzzleFlash_Rifle.NS_WeaponFire_MuzzleFlash_Rifle");
	inline constexpr const TCHAR* ImpactConcrete = TEXT("/Game/Effects/Particles/Impacts/NS_ImpactConcrete.NS_ImpactConcrete");
	inline constexpr const TCHAR* ImpactCharacter = TEXT("/Game/Effects/Particles/Impacts/NS_ImactSparksCharacter.NS_ImactSparksCharacter");
	inline constexpr const TCHAR* GrenadeExplosionFX = TEXT("/Game/Effects/Particles/Explosion/NS_Grenade_Explosion.NS_Grenade_Explosion");
	inline constexpr const TCHAR* GrenadeTrailFX = TEXT("/Game/Effects/Particles/Explosion/NS_Grenade_Trail.NS_Grenade_Trail");

	// Audio (MetaSound sources)
	inline constexpr const TCHAR* RifleFire = TEXT("/Game/Audio/Sounds/Weapons/Rifle/MSS_Weapons_Rifle_Fire.MSS_Weapons_Rifle_Fire");
	inline constexpr const TCHAR* Rifle2Fire = TEXT("/Game/Audio/Sounds/Weapons/Rifle2/MSS_Weapons_Rifle2_Fire.MSS_Weapons_Rifle2_Fire");
	inline constexpr const TCHAR* PistolFire = TEXT("/Game/Audio/Sounds/Weapons/Pistol/MSS_Weapons_Pistol_Fire.MSS_Weapons_Pistol_Fire");
	inline constexpr const TCHAR* ShotgunFire = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/MSS_Weapons_Shotgun_Fire.MSS_Weapons_Shotgun_Fire");
	inline constexpr const TCHAR* GrenadeExplosionSound = TEXT("/Game/Audio/Sounds/Explosions/MSS_Explosions_Grenade.MSS_Explosions_Grenade");

	/** Loads once and keeps the asset alive. Returns null (logged once per path) when it is missing. */
	SHIELDBREAK_API UObject* LoadAsset(const TCHAR* ObjectPath);

	template <typename T>
	T* Load(const TCHAR* ObjectPath)
	{
		return Cast<T>(LoadAsset(ObjectPath));
	}

	/** Fire-and-forget Niagara system. No-op on dedicated servers or when the system is missing. */
	SHIELDBREAK_API void SpawnFX(const UObject* WorldContext, const TCHAR* SystemPath, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator, float Scale = 1.f);

	/** Niagara attached to a component (follows it, e.g. a muzzle flash on a moving viewmodel). */
	SHIELDBREAK_API UNiagaraComponent* SpawnFXAttached(const TCHAR* SystemPath, USceneComponent* AttachTo, FName Socket, const FVector& WorldLocation, const FRotator& WorldRotation);

	/** Plays an imported sound, 2D when Location is null. Returns false if it is missing so callers can fall back. */
	SHIELDBREAK_API bool PlaySound(const UObject* WorldContext, const TCHAR* SoundPath, const FVector* Location, float Volume = 1.f, float Pitch = 1.f);
}
