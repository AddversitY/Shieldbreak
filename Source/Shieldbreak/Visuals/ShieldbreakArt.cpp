#include "Visuals/ShieldbreakArt.h"

#include "Shieldbreak.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/SoftObjectPath.h"

namespace ShieldbreakArt
{
	// Named (not anonymous) so unity builds can't collide with another file's helpers.
	namespace ArtPrivate
	{
		bool IsDedicatedServer(const UObject* WorldContext)
		{
			const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
			return !World || World->GetNetMode() == NM_DedicatedServer;
		}
	}

	UObject* LoadAsset(const TCHAR* ObjectPath)
	{
		// Rooted so the cache never dangles; this is a small, fixed set of shared assets.
		static TMap<FString, UObject*> Loaded;
		static TSet<FString> Missing;

		const FString Key(ObjectPath);
		if (UObject** Found = Loaded.Find(Key))
		{
			return *Found;
		}
		if (Missing.Contains(Key))
		{
			return nullptr;
		}

		UObject* Asset = FSoftObjectPath(Key).TryLoad();
		if (!Asset)
		{
			Missing.Add(Key);
			UE_LOG(LogShieldbreak, Warning, TEXT("Art asset missing, using greybox fallback: %s"), ObjectPath);
			return nullptr;
		}

		Asset->AddToRoot();
		Loaded.Add(Key, Asset);
		return Asset;
	}

	void SpawnFX(const UObject* WorldContext, const TCHAR* SystemPath, const FVector& Location, const FRotator& Rotation, float Scale)
	{
		if (ArtPrivate::IsDedicatedServer(WorldContext))
		{
			return;
		}
		if (UNiagaraSystem* System = Load<UNiagaraSystem>(SystemPath))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, System, Location, Rotation, FVector(Scale), true);
		}
	}

	UNiagaraComponent* SpawnFXAttached(const TCHAR* SystemPath, USceneComponent* AttachTo, FName Socket, const FVector& WorldLocation, const FRotator& WorldRotation)
	{
		if (!AttachTo || ArtPrivate::IsDedicatedServer(AttachTo))
		{
			return nullptr;
		}
		UNiagaraSystem* System = Load<UNiagaraSystem>(SystemPath);
		if (!System)
		{
			return nullptr;
		}
		return UNiagaraFunctionLibrary::SpawnSystemAttached(System, AttachTo, Socket, WorldLocation, WorldRotation, EAttachLocation::KeepWorldPosition, true);
	}

	bool PlaySound(const UObject* WorldContext, const TCHAR* SoundPath, const FVector* Location, float Volume, float Pitch)
	{
		if (ArtPrivate::IsDedicatedServer(WorldContext))
		{
			return true;
		}
		USoundBase* Sound = Load<USoundBase>(SoundPath);
		if (!Sound)
		{
			return false;
		}
		if (Location)
		{
			UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, *Location, Volume, Pitch);
		}
		else
		{
			UGameplayStatics::PlaySound2D(WorldContext, Sound, Volume, Pitch);
		}
		return true;
	}
}
