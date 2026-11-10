// Combat sound effects. Each sound id maps to a bank of imported variations (/Game/Shieldbreak/Audio/SFX/SFX_<Bank>_NN,
// rendered by Art/audio/make_sfx.py and imported by Tools/ImportSFX.bat). A play picks a random variation (never the
// same one twice in a row) with a small pitch/volume jitter. 2D plays use the bank's first-person (stereo) version;
// positional plays use the mono version and, for guns and explosions, crossfade in a distant version by listener
// distance. If a bank is not imported yet the sound falls back to the original procedural synth (or stays silent).

#pragma once

#include "CoreMinimal.h"

class UAudioComponent;
class USceneComponent;

enum class EShieldbreakSound : uint8
{
	BattleRifleShot,
	SMGShot,
	PlasmaShot,
	GrenadeExplosion,
	ShieldRecharge,
	ShieldPop,
	ShieldLowBeep,
	HitMarker,
	KillMarker,
	VentSuccess,
	VentFail,
	BreakPulse,
	Melee,
	Death,
	RailCharge,
	RailFire,
	AegisRaise,
	AegisHit,
	CamoOn,
	/** Hit marker for a hit absorbed by the shield: a glassier tick. */
	ShieldHitMarker,
	/** Hit marker when your shot pops an enemy's shield: a bright crack. */
	ShieldBreakMarker,
	/** Grenade leaving the hand (play at release, sb.GrenadeReleaseDelay after the throw input). */
	GrenadeThrow,
	/** Grenade hitting a surface; scale Volume by impact speed. */
	GrenadeBounce,

	// Layered weapon banks (BattleRifleShot and RailFire above are the close layers of their guns).
	BattleRifleTail,
	SidearmShot,
	SidearmTail,
	SniperShot,
	SniperTail,
	/** The sniper's bolt cycling, ~0.3 s after each shot. */
	SniperBolt,
	RocketShot,
	RocketTail,
	RailTail,
	ReloadBattleRifle,
	ReloadSidearm,
	ReloadSniper,
	ReloadRocket,
	DryFireLight,
	DryFireRifle,
	DryFireHeavy,
	DryFireEnergy,
	WeaponRaise,
	Footstep,
	Land,
	Count
};

/** The sounds one weapon makes. Count means "none". */
struct FShieldbreakWeaponSounds
{
	EShieldbreakSound Shot = EShieldbreakSound::BattleRifleShot;
	EShieldbreakSound Tail = EShieldbreakSound::BattleRifleTail;
	EShieldbreakSound Reload = EShieldbreakSound::ReloadBattleRifle;
	EShieldbreakSound DryFire = EShieldbreakSound::DryFireRifle;
	/** Optional mechanism played CycleDelay seconds after each shot (the sniper's bolt). */
	EShieldbreakSound Cycle = EShieldbreakSound::Count;
	float CycleDelay = 0.3f;
};

class SHIELDBREAK_API FShieldbreakSFX
{
public:
	/** Loads every imported bank once (call on the client at match start so the first shot doesn't hitch). */
	static void Preload();

	/** Self-test: names of banks with missing variations (empty = all imported); OutLoaded counts loaded variations. */
	static FString DescribeMissingBanks(int32& OutLoaded);
	/** Non-spatialized (UI / first-person) sound. Returns null on dedicated servers. */
	static UAudioComponent* Play2D(const UObject* WorldContext, EShieldbreakSound Sound, float Volume = 1.f, float Pitch = 1.f);

	/** Spatialized one-shot at a world location. */
	static UAudioComponent* PlayAt(const UObject* WorldContext, EShieldbreakSound Sound, const FVector& Location, float Volume = 1.f, float Pitch = 1.f);

	/**
	 * Plays one gunshot: close + tail layers in 2D for the shooter (bLocalView), otherwise positional close/tail/distant
	 * layers crossfaded by listener distance. Plays nothing on dedicated servers.
	 */
	static void PlayShot(const UObject* WorldContext, const FShieldbreakWeaponSounds& Sounds, const FVector& Muzzle, bool bLocalView);

	/** Positional sound attached to a component (follows it), e.g. a reload in someone else's hands. */
	static UAudioComponent* PlayAttached(USceneComponent* AttachTo, EShieldbreakSound Sound, float Volume = 1.f, float Pitch = 1.f);

	/** Length in seconds of a synthesized sound at pitch 1. */
	static float GetDuration(EShieldbreakSound Sound);
};
