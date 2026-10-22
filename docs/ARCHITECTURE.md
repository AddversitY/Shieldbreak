# Shieldbreak Architecture (code-only prototype)

Goal: a **fully playable greybox prototype built from C++ and config alone**, with no `.uasset`/`.umap` files required.
The user installs UE5, double-clicks `Shieldbreak.uproject`, accepts the rebuild prompt, and presses Play:
an arena builds itself, the player spawns with a Battle Rifle + SMG, and bots fight them in Free-For-All Slayer.

Everything visual uses **engine content only**:
- Meshes: `/Engine/BasicShapes/Cube.Cube`, `Sphere.Sphere`, `Cylinder.Cylinder`, `Cone.Cone`, `Plane.Plane`
  (each 100 uu across, pivot at center).
- Material: `/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial` tinted with a
  `UMaterialInstanceDynamic` vector parameter named `Color`.
- Load in constructors with `ConstructorHelpers::FObjectFinder` and **null-check every result**.
- No sounds, no Niagara, no UMG assets. The HUD draws with `AHUD` Canvas.

Enhanced Input actions and mapping contexts are **created at runtime in C++** (`UShieldbreakInputSet`).

---

## Engine/version rules (apply to every file)

- Target **UE 5.4+** APIs (build settings `Latest`). Module name `Shieldbreak`, API macro `SHIELDBREAK_API`.
- Includes are module-root relative, e.g. `#include "Shield/ShieldHealthComponent.h"`. Every `.h` starts with
  `#pragma once`, includes `CoreMinimal.h`, and ends its include list with its own `X.generated.h`.
- `TObjectPtr<>` for every UPROPERTY object pointer. Raw pointers are fine for locals and parameters.
- **No variable shadowing** (UE5 treats it as an error). Never name locals/params after members of the class or
  its bases: `Owner`, `Instigator`, `Controller`, `World`, `Tags`, `Health`, `Shield`, `Mesh`, `Stats`, etc.
- Never put `BlueprintReadOnly`/`BlueprintReadWrite` on a `private:` UPROPERTY without
  `meta = (AllowPrivateAccess = "true")`.
- UFUNCTIONs must not return `const UObject*` (keep such getters plain C++).
- Server RPCs: `UFUNCTION(Server, Reliable)` (or Unreliable). No `WithValidation` needed.
  Implement as `Foo_Implementation`.
- `DOREPLIFETIME` / `DOREPLIFETIME_CONDITION` require `#include "Net/UnrealNetwork.h"`.
- `FOverlapResult` → `#include "Engine/OverlapResult.h"`; `FHitResult` → `"Engine/HitResult.h"`;
  `UGameplayStatics` → `"Kismet/GameplayStatics.h"`; `DrawDebug*` → `"DrawDebugHelpers.h"`;
  `TActorIterator` → `"EngineUtils.h"`.
- `AGameStateBase::GetServerWorldTimeSeconds()` returns `double`; cast to `float` where stored as float.
- Weapon traces use the custom trace channel **`ECC_Weapon`** (= `ECC_GameTraceChannel1`, defined in
  `Shieldbreak.h`, configured in `Config/DefaultEngine.ini` with default response Block).
- Logging: `UE_LOG(LogShieldbreak, ...)`, declared in `Shieldbreak.h`.
- Everything gameplay-relevant is **server-authoritative**. Clients send intent via Server RPCs. Local
  cosmetics (tracers, recoil) run immediately on the owning client.
- `AIController`-driven pawns run on the server where `IsLocallyControlled()` is true, so the same
  "local" code paths (StartFire etc.) work for bots. Never send Client RPCs expecting a HUD on bots;
  feedback goes through `AShieldbreakPlayerController` Client RPCs, and bots simply don't have one.

---

## File map and ownership

```
Shieldbreak.uproject
Config/DefaultEngine.ini, DefaultGame.ini, DefaultInput.ini, DefaultEditor.ini
Source/Shieldbreak.Target.cs, ShieldbreakEditor.Target.cs
Source/Shieldbreak/
  Shieldbreak.Build.cs
  Shieldbreak.h / Shieldbreak.cpp               module, LogShieldbreak, ECC_Weapon
  Shield/      ShieldHealthComponent, ShieldTuningData, ShieldbreakDamageType     (exists)
  Player/      ShieldbreakCharacter, ShieldbreakPlayerController, ShieldbreakInputSet, ShieldbreakPlayerState
  Player/      ShieldbreakMovementComponent
  Weapons/     ShieldbreakWeapon (base + FShieldbreakWeaponStats), ShieldbreakWeaponClasses (BR, SMG, Plasma Rifle),
               ShieldbreakDamageTypes, ShieldbreakGrenade, ShieldbreakWeaponPickup, ShieldbreakTracer
  Game/        ShieldbreakGameMode, ShieldbreakGameState, ShieldbreakArena
  AI/          ShieldbreakBotController
  UI/          ShieldbreakHUD, ShieldbreakMenu (game instance, menu game mode/controller), SShieldbreakMenus (Slate)
```

---

## Module (`Shieldbreak.h`)

```cpp
#pragma once
#include "CoreMinimal.h"
DECLARE_LOG_CATEGORY_EXTERN(LogShieldbreak, Log, All);
#define ECC_Weapon ECC_GameTraceChannel1
```
`Shieldbreak.cpp`: `DEFINE_LOG_CATEGORY(LogShieldbreak); IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, Shieldbreak, "Shieldbreak");`

`Shieldbreak.Build.cs` deps: Core, CoreUObject, Engine, InputCore, EnhancedInput, AIModule, GameplayTasks, PhysicsCore, NetCore.
Also add `PublicIncludePaths.Add(ModuleDirectory);`.

Config essentials:
- `DefaultEngine.ini`: `GameDefaultMap=/Engine/Maps/Entry`, `EditorStartupMap=/Engine/Maps/Entry`,
  `GlobalDefaultGameMode=/Script/Shieldbreak.ShieldbreakGameMode`; collision channel
  `+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Block,bTraceType=True,bStaticObject=False,Name="Weapon")`;
  Lumen GI/reflections + virtual shadow maps; `[/Script/Engine.Engine] +ActiveGameNameRedirects` not needed.
- `DefaultInput.ini`: `DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput`,
  `DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent`.

---

## Shield (exists, `Shield/`)

`UShieldHealthComponent`. Public API used by others:
`SetShaping(bool)`, `TryVent()`, `TryBreakPulse()`, `ApplyShieldbreakDamage(...)`, `ApplyShieldOnlyDamage(...)`,
`GetShield()`, `GetMaxShield()`, `GetOvershield()`, `GetHealth()`, `GetMaxHealth()`, `IsShaping()`,
`IsRecharging()`, `IsDead()`, `GetVentMarkerPosition()` (0..1 or -1), `GetTuningData()` (never null),
`GetLastShieldBreakLocalTime()`; delegates `OnShieldChanged`, `OnOvershieldChanged`, `OnHealthChanged`
(float NewValue, float MaxValue), `OnShieldBroken`, `OnRechargeStarted`, `OnBreakPulse`, `OnVentResult(bool)`,
`OnDeath(AController* Killer)`. It binds the owner's `OnTakeAnyDamage` itself, so **all damage must go
through `UGameplayStatics::ApplyPointDamage/ApplyRadialDamage*/ApplyDamage`**. Never call
`ApplyShieldbreakDamage` directly in addition.

`UShieldbreakDamageType` fields: `ShieldDamageMultiplier`, `HealthDamageMultiplier`,
`RechargeDelayExtension`, `bIgnoresShieldShaping`.

---

## Player

### `UShieldbreakInputSet` (`Player/ShieldbreakInputSet.h`) : UObject
Runtime-built input. `static UShieldbreakInputSet* Get();` lazily creates one instance
(`NewObject<UShieldbreakInputSet>(GetTransientPackage())` + `AddToRoot()`) and builds everything once.

Public `UPROPERTY() TObjectPtr<UInputAction>` members (exact names):
`Move` (Axis2D), `Look` (Axis2D), `Jump`, `Fire`, `Zoom`, `Shape`, `Melee`, `Grenade`, `ReloadVent`,
`SwapWeapon`, `Interact`, `Sprint`, `CrouchSlide`, `Scoreboard`, `ToggleHelp` (all others Boolean), plus
`TObjectPtr<UInputMappingContext> Context`.

Value conventions: `Move` gives X = right, Y = forward. `Look` gives X = yaw right, Y = **look up positive**.
The handler applies `AddControllerYawInput(X)` and `AddControllerPitchInput(-Y)`.

Bindings (keyboard/mouse | gamepad):
| Action | KBM | Gamepad |
|---|---|---|
| Move | WASD (swizzle/negate modifiers) | Gamepad_Left2D (dead zone) |
| Look | Mouse2D | Gamepad_Right2D (dead zone, scalar ~ (300,200,1) × ScaleByDeltaTime) |
| Jump | SpaceBar | Gamepad_FaceButton_Bottom |
| Fire | LeftMouseButton | Gamepad_RightTrigger |
| Zoom | RightMouseButton | Gamepad_RightThumbstick |
| Shape (hold) | LeftAlt, ThumbMouseButton | Gamepad_LeftTrigger |
| Melee | F | Gamepad_RightShoulder |
| Grenade | G | Gamepad_LeftShoulder |
| ReloadVent | R | Gamepad_FaceButton_Left |
| SwapWeapon | Q, MouseScrollUp, MouseScrollDown | Gamepad_FaceButton_Top |
| Interact | E | Gamepad_DPad_Up |
| Sprint (hold) | LeftShift | Gamepad_LeftThumbstick |
| CrouchSlide (hold) | LeftControl, C | Gamepad_FaceButton_Right |
| Scoreboard (hold) | Tab | Gamepad_Special_Left |
| ToggleHelp | F1 | Gamepad_Special_Right |

### `AShieldbreakPlayerController` : APlayerController
- `BeginPlay`: if local, add `UShieldbreakInputSet::Get()->Context` to the `UEnhancedInputLocalPlayerSubsystem` (priority 0).
- `SetupInputComponent`: bind `Scoreboard` (Started/Completed) and `ToggleHelp` (Started) on the
  `UEnhancedInputComponent` so they work while dead.
- Local UI state readable by HUD: `bool IsScoreboardVisible() const`, `bool IsHelpVisible() const`
  (help visible by default for the first 12 s of play).
- Feedback Client RPCs (server calls these; implementations store local `GetWorld()->GetTimeSeconds()` stamps):
  - `UFUNCTION(Client, Unreliable) void ClientHitConfirmed(bool bKill, bool bHeadshot);`
  - `UFUNCTION(Client, Unreliable) void ClientDamagedFrom(FVector_NetQuantize SourceLocation);`
  - `UFUNCTION(Client, Reliable) void ClientSetRespawnTime(float ServerTimeOfRespawn);`
  - Getters: `float GetLastHitConfirmTime() const`, `bool WasLastHitKill() const`, `bool WasLastHitHeadshot() const`,
    `FVector GetLastDamageSource() const`, `float GetLastDamagedTime() const`,
    `float GetRespawnServerTime() const` (-1 when not pending).
- Console commands: `UFUNCTION(Exec) void SB_Bots(int32 Count);` and `UFUNCTION(Exec) void SB_Skill(int32 Skill);`
  forward to `ServerSetBots`/`ServerSetBotSkill` → `AShieldbreakGameMode::SetBotCount/SetBotSkill`.
- On unpossess/death, keep viewing the dead pawn until respawn (`SetViewTarget`).

### `AShieldbreakPlayerState` : APlayerState
Replicated `int32 Kills`, `int32 Deaths`. `AddKill()` / `AddDeath()` / `ResetStats()` (server; `AddKill` also `SetScore(Kills)`).
Getters `GetKills()`, `GetDeaths()`. Replicated `FLinearColor PlayerColor` (assigned by GameMode on join)
with getter `GetPlayerColor()`.

### `UShieldbreakMovementComponent` : UCharacterMovementComponent (`Player/ShieldbreakMovementComponent.h`)
Halo-style feel plus the **Momentum Carry** mechanic, network-predicted with a custom `FSavedMove`.
- `enum ECustomMovementMode_Shieldbreak { CMOVE_None = 0, CMOVE_Slide = 1 };` (plain enum in header, `UENUM` not required)
- Defaults: MaxWalkSpeed 600, MaxWalkSpeedCrouched 260, SprintSpeed 880, JumpZVelocity 640, GravityScale 0.9,
  AirControl 0.45, MaxAcceleration 4096, BrakingDecelerationWalking 2400, GroundFriction 8,
  BrakingDecelerationFalling 0, FallingLateralFriction 0, crouched half height 58, `bCanWalkOffLedgesWhenCrouching`,
  `NavAgentProps.bCanCrouch = true`.
- Sprint: `void SetSprinting(bool)` sets `Safe_bWantsToSprint` (sent as `FLAG_Custom_0`). Sprint speed only
  applies while walking, not crouched, and moving forward-ish (dot(velocity dir, actor forward) > 0.5).
- Slide: entered in `UpdateCharacterStateBeforeMovement` when walking + `bWantsToCrouch` + sprinting + 2D speed ≥ 650.
  Entry boost +350 along velocity, capped at 1150 total. `PhysSlide` (`MOVE_Custom`/`CMOVE_Slide`): friction 0.35,
  braking 350, gravity pulls along slopes (downhill slides accelerate), only lateral steering input. Exits
  (to walking) when speed < 380, crouch released, or no floor. Capsule stays crouched while sliding.
- `IsMovingOnGround()` returns true while sliding (so jumping out of a slide works).
- **Momentum Carry**: leaving a slide into falling (a jump) sets `bMomentumCarry` until landing. Horizontal speed
  is preserved in the air because falling braking/friction are 0.
- Public: `bool IsSliding() const`, `bool IsSprinting() const`, `bool IsMomentumCarrying() const`, `void SetSprinting(bool)`.

### `AShieldbreakCharacter` : ACharacter (`Player/ShieldbreakCharacter.h`)
Constructor: `AShieldbreakCharacter(const FObjectInitializer& ObjectInitializer);` using
`Super(ObjectInitializer.SetDefaultSubobjectClass<UShieldbreakMovementComponent>(ACharacter::CharacterMovementComponentName))`.

Components: capsule radius 40 / half height 92; `UCameraComponent* FirstPersonCamera` at (0,0,64),
`bUsePawnControlRotation`, FOV 78. Static-mesh greybox body: `BodyMesh` (cylinder), `HeadMesh` (sphere,
**the headshot hitbox**: QueryOnly, ignore all channels except Block on `ECC_Weapon`), `VisorMesh` (cube, gold),
`ShapeShieldMesh` (thin wide cube in front of the torso, visible to others only while shaping, blue).
Body/head/visor are `SetOwnerNoSee(true)`. `BaseEyeHeight = CrouchedEyeHeight = 64`. Capsule blocks `ECC_Weapon`.
`UShieldHealthComponent* ShieldHealth`.

Public API:
```cpp
UCameraComponent* GetFirstPersonCamera() const;
UShieldHealthComponent* GetShieldHealth() const;
UShieldbreakMovementComponent* GetShieldbreakMovement() const;
UStaticMeshComponent* GetHeadMesh() const;
AShieldbreakWeapon* GetCurrentWeapon() const;
AShieldbreakWeapon* GetHolsteredWeapon() const;       // other inventory slot or nullptr
int32 GetGrenadeCount() const;
bool IsAlive() const;                                   // ShieldHealth && !ShieldHealth->IsDead()
bool IsZoomed() const;
FVector GetAimOrigin() const;                           // GetPawnViewLocation()
FRotator GetAimRotation() const;                        // locally controlled: GetControlRotation(), else GetBaseAimRotation()
float GetSpreadMultiplier() const;                      // ×1.75 momentum carry, ×1.3 airborne, ×0.75 crouched, ×0.5 zoomed
FLinearColor GetPlayerColor() const;

// Actions: safe to call from input (owning client) or AI (server). They route RPCs internally.
void StartFire(); void StopFire();
void Melee();                // calls ShieldHealth->TryBreakPulse() too
void ThrowGrenade();
void ReloadOrVent();         // vent if ShieldHealth->GetVentMarkerPosition() >= 0, else reload
void SwapWeapon();
void Interact();             // pick up/swap with an overlapping AShieldbreakWeaponPickup
void SetSprinting(bool bSprint);
void SetShaping(bool bShape);
void SetZoomed(bool bZoom);  void ToggleZoom();
void StartCrouchOrSlide();   void StopCrouchOrSlide();   // Crouch()/UnCrouch(); the movement component decides slide

// Server only
void GiveWeapon(TSubclassOf<AShieldbreakWeapon> WeaponClass);  // fills an empty slot, else replaces the current weapon (destroying it)
void EquipWeapon(AShieldbreakWeapon* Weapon);
void AddGrenades(int32 Count);
```
Defaults: `PrimaryWeaponClass = AWeapon_BattleRifle`, `SecondaryWeaponClass = AWeapon_Sidearm`,
`GrenadeClass = AShieldbreakGrenade`, 2 grenades (max 4), melee damage 70, range 190, lunge range 320,
cooldown 0.9 s, **melee from behind (target forward · attacker→target dir > 0.6) = 999 damage (assassination)**.
Melee is a server sphere sweep on `ECC_Pawn` objects using `UDamageType_Melee`.

Replicated: `Inventory` (`TArray<TObjectPtr<AShieldbreakWeapon>>`), `CurrentWeapon` (OnRep → equip visuals),
`GrenadeCount` (OwnerOnly). Inventory spawned on the server in `BeginPlay` (or `PossessedBy`).
Jumping is allowed while crouched/sliding (`CanJumpInternal_Implementation` override). Crouching mid-air works
(Halo crouch-jump). Firing, meleeing or throwing cancels sprint. Zoom is dropped when the shield takes damage.

Death (server, bound to `ShieldHealth->OnDeath`): stop fire, call
`AShieldbreakGameMode::NotifyKilled(AController* Killer, AController* Victim, AShieldbreakCharacter* VictimPawn)`,
disable capsule collision and movement, `DetachFromControllerPendingDestroy()`, `SetLifeSpan(6)`, destroy weapons.
Death visuals run on all machines from `OnDeath`: body/head become physics-simulating debris and the shaping
plate hides. For non-locally-controlled characters, `Tick` sets the camera's relative pitch from
`GetBaseAimRotation().Pitch` so third-person weapons aim. Player tint comes from
`AShieldbreakPlayerState::GetPlayerColor()` (apply in `OnRep_PlayerState` and `PossessedBy`).

---

## Weapons

### `ShieldbreakDamageTypes.h`
`UDamageType_Ballistic` (shield ×1.0, health ×1.0), `UDamageType_BallisticLight` (shield ×0.8, health ×1.25),
`UDamageType_Plasma` (shield ×1.6, health ×0.5, RechargeDelayExtension 1.0), `UDamageType_Explosive`
(bIgnoresShieldShaping), `UDamageType_Melee` (neutral). All subclass `UShieldbreakDamageType` and set their values
in constructors.

### `AShieldbreakWeapon` : AActor (`Weapons/ShieldbreakWeapon.h`, Abstract)
```cpp
UENUM(BlueprintType) enum class EShieldbreakFireMode : uint8 { SemiAuto, Burst, FullAuto };

USTRUCT(BlueprintType) struct FShieldbreakWeaponStats {
  FText DisplayName; EShieldbreakFireMode FireMode; float Damage; bool bPrecision; float HeadshotMultiplier;
  float TimeBetweenShots; int32 BurstCount; float BurstInterval;
  float BaseSpreadDegrees; float SpreadPerShot; float MaxSpreadDegrees; float SpreadRecoveryPerSecond;
  float Range; int32 MagazineSize; int32 MaxReserveAmmo; float ReloadTime;
  bool bUsesHeat; float HeatPerShot; float HeatCooldownPerSecond; float OverheatLockout;
  float ZoomFOV;  /* 0 = no zoom */ float RecoilPitch; FLinearColor Color; FLinearColor TracerColor;
  TSubclassOf<UDamageType> DamageType; FVector MeshScale;
};
```
Public:
```cpp
const FShieldbreakWeaponStats& GetStats() const;
AShieldbreakCharacter* GetOwningCharacter() const;
void SetOwningCharacter(AShieldbreakCharacter* NewOwner);   // server: SetOwner + SetInstigator
void OnEquipped(); void OnUnequipped();                      // attach to owner's camera at (42,16,-20), visibility
void StartFire(); void StopFire();                           // local (owning client / server AI)
void StartReload(); bool CanReload() const; bool IsReloading() const;
int32 GetAmmoInMagazine() const; int32 GetReserveAmmo() const;
float GetHeat() const; bool IsOverheated() const;            // heat 0..1
float GetCurrentSpreadDegrees() const;                       // includes owner's GetSpreadMultiplier()
void AddReserveAmmo(int32 Amount);                           // server
```
Firing: the local machine runs the fire-rate/burst timer and computes the spread direction from
`GetAimRotation()`, with a cosmetic tracer and recoil. Each shot calls
`UFUNCTION(Server, Reliable) ServerFireShot(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction)`.
The server validates rate (with tolerance), ammo/heat, origin within 150 uu of `GetAimOrigin()`, and direction
within `MaxSpread + 5°` of the server aim. It then traces `ECC_Weapon` (ignoring owner + self) and applies
`UGameplayStatics::ApplyPointDamage` with `DamageCauser = this`. **Headshot rule**: if the hit component is the
victim's `GetHeadMesh()` and `bPrecision` and the victim's shield + overshield ≤ 0 → damage × `HeadshotMultiplier`.
After damage it calls `ClientHitConfirmed` on the instigator's `AShieldbreakPlayerController` and
`ClientDamagedFrom` on the victim's. It plays `MulticastShotFX(End)` (unreliable; skip the owning
client, which already drew it). Ammo/heat/reload state is replicated `COND_OwnerOnly`. Heat weapons have no
magazine. The cosmetic tracer is `AShieldbreakTracer::Spawn(World, Start, End, Color)`.

### `ShieldbreakWeaponClasses.h`
- `AWeapon_BattleRifle`: Burst 3, BurstInterval 0.055, TimeBetweenShots 0.42 (between bursts; hold = repeat),
  Damage 6.5, precision, HeadshotMultiplier 10, spread 0.25→max 0.6, mag 36, reserve 108, reload 2.0, range 12000,
  ZoomFOV 40, Ballistic, recoil 0.35.
- (The SMG and Plasma Rifle were removed on 2026-10-22; `UDamageType_Plasma` stays for the Shield Bleed tests.)

### `AShieldbreakGrenade` : AActor
Replicated movement. Sphere collision (radius 9) blocks world, ignores `ECC_Weapon`. `UProjectileMovementComponent` with
bounce 0.35 and friction 0.4, launched at 1900 uu/s plus owner velocity. Fuse: explodes 1.1 s after first bounce or 3.0 s
after throw. Explosion on server: `UGameplayStatics::ApplyRadialDamageWithFalloff` (base 110, min 10, inner 180, outer 520,
`UDamageType_Explosive`, instigator controller), plus knockback `LaunchCharacter` on characters in 520 uu, including the
thrower (**grenade jumps**). Replicated `bExploded` (OnRep) plays a cosmetic expanding orange sphere locally. `SetLifeSpan(0.5)`.
Spawned by `AShieldbreakCharacter::ThrowGrenade` (server) from `GetAimOrigin()` + aim forward × 60.
`void Launch(const FVector& Direction, const FVector& InheritedVelocity)` is public.

### `AShieldbreakWeaponPickup` : AActor
`UPROPERTY(EditAnywhere) TSubclassOf<AShieldbreakWeapon> WeaponClass;` `RespawnTime` 30 s. A floating, spinning greybox of the
weapon (local cosmetic) plus an overlap sphere (radius 120, QueryOnly, overlaps Pawn only). Overlapping a character that already
has this class grants reserve ammo (server, then the pickup goes on cooldown). Otherwise the player picks it up with
`AShieldbreakCharacter::Interact()`, which calls `bool TryGiveTo(AShieldbreakCharacter* Character)` (server) to swap it in.
Replicated `bAvailable` hides it. Public `TSubclassOf<AShieldbreakWeapon> GetWeaponClass() const`, `bool IsAvailable() const`.

### `AShieldbreakTracer` : AActor (local-only cosmetic)
`static void Spawn(UWorld* World, const FVector& Start, const FVector& End, const FLinearColor& Color);` A stretched thin cylinder
that lives 0.06 s, plus a 0.12 s impact puff. Not replicated.

---

## Game

### `AShieldbreakGameState` : AGameStateBase
```cpp
USTRUCT(BlueprintType) struct FShieldbreakKillFeedEntry { FString KillerName; FString VictimName; FLinearColor KillerColor; FLinearColor VictimColor; float ServerTime; };
```
Replicated: `TArray<FShieldbreakKillFeedEntry> KillFeed` (max 6, newest last), `int32 ScoreLimit` (25), `bool bMatchOver`,
`FString WinnerName`, `float MatchRestartServerTime`. Server: `AddKillFeedEntry(...)`, `SetMatchOver(const FString& Winner, float RestartAt)`,
`ResetMatch()`. Getters for all, plus `TArray<APlayerState*> GetSortedPlayers() const` (by kills desc).

### `AShieldbreakGameMode` : AGameModeBase
Defaults: pawn `AShieldbreakCharacter`, controller `AShieldbreakPlayerController`, player state, game state, HUD `AShieldbreakHUD`.
`NumBots` 5 (URL option `?Bots=N`), `BotSkill` 1 (0 Easy, 1 Normal, 2 Heroic, 3 Legendary; `?Skill=N`), `RespawnDelay` 3 s,
`ScoreLimit` 25.
- `EnsureArena()`: spawns one `AShieldbreakArena` at the origin if none exists (`TActorIterator`). Called from `InitGame`'s
  first world access point, `RestartPlayer`, and `StartPlay`, so spawning works before BeginPlay.
- `RestartPlayer(AController*)` override: `EnsureArena()` then
  `RestartPlayerAtTransform(NewPlayer, ChooseSpawnTransform(NewPlayer))`. Picks the arena spawn farthest from living enemies,
  with randomness among the top 3.
- `PostLogin`/`InitNewPlayer`: assign a distinct `PlayerColor` from a 12-color palette.
- `StartPlay`: `EnsureArena()`, then spawn bots (timer 0.5 s). Bots: `SpawnActor<AShieldbreakBotController>`, name
  `"Bot <Name>"` from a list, set skill, `RestartPlayer(Bot)`.
- `NotifyKilled(Killer, Victim, VictimPawn)`: update kills/deaths (suicide → −1 kill floor 0), kill feed, respawn timer
  (`ClientSetRespawnTime` for players), and score-limit check → `SetMatchOver`. 8 s later, reset stats, destroy all pawns,
  respawn everyone.
- `SetBotCount(int32)`, `SetBotSkill(int32)` (live).
- `bool IsMatchOver() const`.

### `AShieldbreakArena` : AActor (`Game/ShieldbreakArena.h`)
Builds a symmetrical greybox arena from engine cubes in **`OnConstruction`** (guarded, so geometry exists right after
`SpawnActor`): about 7000 × 5000 uu floor, 900 uu outer walls, two bases with elevated platforms and ramps (≤ 30°),
a central two-level tower, a long **downhill slide ramp** that rewards Momentum Carry, cover crates/pillars, and
crouch-jump ledges (about 150 uu). Distinct tints per region for readability. All pieces use `BlockAll` collision
and block `ECC_Weapon`. `BeginPlay` (any net mode; lights are local, not replicated) spawns a movable
`ADirectionalLight` (atmosphere sun), `ASkyLight` (real-time capture), `ASkyAtmosphere`, `AExponentialHeightFog`.
Not replicated; geometry is deterministic so every machine builds it locally (`bReplicates = false`, spawned by the
GameMode on the server and **also** ensured on clients by `AShieldbreakGameState::BeginPlay` → `AShieldbreakArena::EnsureLocal(World)`).
`AShieldbreakArena* Arena = AShieldbreakArena::Find(World)` is the static lookup.

Public:
```cpp
static AShieldbreakArena* Find(const UWorld* World);
static AShieldbreakArena* EnsureLocal(UWorld* World);            // spawn at origin if missing
const TArray<FTransform>& GetSpawnTransforms() const;             // >= 10, standing on floors, yaw facing center
const TArray<FVector>& GetWaypoints() const;                      // bot navigation graph nodes (feet height + 50)
const TArray<int32>& GetWaypointNeighbors(int32 Index) const;     // walkable straight-line links
int32 FindNearestWaypoint(const FVector& Location, bool bRequireLineOfSight = true) const;
bool FindPath(int32 FromIndex, int32 ToIndex, TArray<int32>& OutPath) const;  // A*
```
Pickups: on the **server**, `BeginPlay` spawns `AShieldbreakWeaponPickup` (replicated) for Plasma Rifles at two symmetric spots
and one in the tower top.

---

## AI — `AShieldbreakBotController` : AAIController
`bWantsPlayerState = true`. `void SetSkill(int32 Skill)`. A tick-driven state machine with no navmesh or behavior tree:
- **Perceive** every 0.15 s: nearest living enemy with line of sight (eye to head/body trace on `ECC_Visibility`) within
  9000 uu in a 140° FOV (or when the bot was recently damaged by them). Remember last-known position.
- **Move**: follow the arena waypoint graph (`FindPath`) toward the target's last-known position, a pickup, or a random
  waypoint when idle. Steer with `GetPawn()->AddMovementInput`. When in combat, strafe left/right on random 0.4–1.4 s
  intervals, sometimes jump or crouch. Sprint when out of combat on long straight segments, and try a slide on
  downhill segments (Heroic+).
- **Aim**: `SetControlRotation` interpolated toward the target's head with a skill-scaled error that shrinks while
  tracking. Fire when the error is under the weapon's spread + skill margin, using BR bursts at range and SMG close up
  (swap with `SwapWeapon`).
- **Mastery use by skill**: shaping when 1v1 frontal with shield < 60% (Normal+); vent attempt during recharge with
  skill-scaled timing accuracy (Heroic+ mostly hits, Easy never tries); Break Pulse melee when shield pops and enemy
  < 350 uu (Legendary); grenades at last-known positions behind cover; melee under 180 uu.
- Skill table: reaction 0.6/0.4/0.25/0.15 s; aim error 9/5/2.5/1.2°; turn speed 180/280/420/600 °/s.

---

## UI — `AShieldbreakHUD` : AHUD (Canvas only)
- **Shield bar** top-center: segmented blue fill; gold overshield overlay; flashes red when empty; brightens while
  recharging. **Vent sweep**: when `GetVentMarkerPosition() >= 0`, draw the sweet-spot window (from
  `GetTuningData()`) and a moving white marker. "VENT!" flash on success, "VENT FAILED" on miss (bind `OnVentResult`).
- **Health pips** under the shield bar (only visible when shield is down, Halo-like). Low health turns red.
- **Shaping indicator**: frontal-arc chevron when `IsShaping()`.
- **Break Pulse prompt**: for `BreakPulseWindow + 0.25` s after `GetLastShieldBreakLocalTime()`, flash "PULSE" near the crosshair
  (teaches the timing).
- Crosshair per weapon: dot + ring scaled by `GetCurrentSpreadDegrees()`. Red when a trace from the camera hits an enemy
  character within range. Hit marker (X) for 0.2 s after `GetLastHitConfirmTime()`, larger and red on kills.
  "HEADSHOT" text on headshot kills.
- Damage direction arc toward `GetLastDamageSource()` for 1 s.
- Bottom-right: weapon name, magazine/reserve or heat bar (OVERHEATED), reload progress. Grenade count. Holstered weapon name.
- Momentum: when 2D speed > 650, show speed (uu/s) and a "CARRY" tag while momentum-carrying.
- Top-left kill feed (last 6 s). Top-right: top 4 scores plus your rank. Scoreboard (hold): full table. Match-over banner
  with the winner and a restart countdown. Death: "RESPAWN IN N".
- Help overlay listing all controls and the four mastery mechanics in one line each.
Scale everything by `Canvas->ClipY / 1080`. Fonts: `GEngine->GetSmallFont()/GetMediumFont()/GetLargeFont()`.

## UI — front end (`UI/ShieldbreakMenu.h`, `UI/SShieldbreakMenus.h`, Slate in code, no UMG assets)
- `UShieldbreakGameInstance` (set as `GameInstanceClass` in DefaultEngine.ini) overrides `OverrideGameModeClass`: when
  `ShieldbreakMenu::ShouldShowMainMenu` says so it swaps in `AShieldbreakMenuGameMode` (no arena, no pawns). Rule: `?Menu`
  always opens the menu; any of `?Map ?Mode ?Bots ?listen`, a dedicated server, `-SBSelfTest`, `-SBAutoShot=` or
  `-SBNoMenu` goes straight to a match. So a bare `-game` launch shows the menu and every test script is unaffected.
- `SShieldbreakMainMenu`: map / mode / bots / bot-skill choosers, PLAY (`OpenLevel Entry?Map=..?Mode=..?Bots=..?Skill=..`),
  HOST LAN GAME (same plus `?listen`), JOIN (address box, `ClientTravel`), QUIT. Choices persist in
  GameUserSettings.ini `[Shieldbreak.Menu]`.
- Both menus show the player settings (`Player/ShieldbreakSettings.h`): sensitivity 0.2-3.0 (multiplies look input),
  hip-fire field of view 70-110 (default 78; scopes keep their zoom FOV), invert look. Stored in GameUserSettings.ini
  `[Shieldbreak.Settings]`, read by `AShieldbreakCharacter::Input_Look` and the camera FOV in Tick. Graphics
  (Low-Epic) sets `UGameUserSettings` overall scalability and applies it at once.
- In a match, Esc (`UShieldbreakInputSet::Pause`) opens `SShieldbreakPauseMenu` (Resume / Leave match / Quit) with UI-only
  input; the match keeps running. Leave match = `OpenLevel Entry?Menu`.
