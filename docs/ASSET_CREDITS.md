# Asset Credits and Imported Content Paths

## Sources

| Source | Publisher | License | Where it lives |
|---|---|---|---|
| Lyra Starter Game (Fab listing `93faede1-4434-47c0-85f1-bf27c0820ad0`, UE 5.8 build, published 2026-09-30) | Epic Games | Fab Standard License, "UE-Only Content": licensed for use only in Unreal Engine-based products | `Content/Characters/Heroes/`, `Content/Weapons/`, `Content/Effects/`, `Content/Audio/`, `Content/PhysicsMaterials/` |

Phase 1 note: the assets were copied file-for-file from the Lyra download **at their original Lyra
paths** (`/Game/Characters/...`, `/Game/Weapons/...`, etc.) instead of being migrated under
`/Game/Shieldbreak/Imported/Lyra/`. Unreal assets reference each other by package path, so keeping the
paths means nothing had to be re-saved in the editor (this PC has 8 GB RAM and cannot comfortably open
Lyra). They can be moved later inside the editor with redirector fix-up.

Removed after copying (not needed, saves Git LFS space): `Weapons/Healthpack`, `Weapons/Tests`,
`Weapons/Spawnpad`, `Audio/Sounds/Music`, `Audio/Blueprints`.

Verified with `Tools/CheckImports.bat` (headless `Tools/check_imported_assets.py`): **1,512 meshes,
skeletons, animations, Niagara systems and sounds, 0 load failures.**

### Things that do NOT work outside Lyra (ignore them)
Lyra Blueprints and data assets that depend on Lyra C++ (`/Script/LyraGame`, GAS gameplay abilities and
effects) load with errors and must not be used: `Weapons/B_Weapon`, `Weapons/GA_*`, `Weapons/*/B_*`,
`Weapons/*/ID_*`, `Weapons/*/WID_*`, `Weapons/*/GE_*`, `Characters/Heroes/Mannequin/Animations/ABP_*`
(the Lyra anim blueprints), `.../Animations/LinkedLayers/*`, `.../Animations/AnimModifiers/*`,
`Effects/Blueprints/*`. Anim notifies inside montages that point at Lyra classes are simply skipped.

Plugins the content uses (all ship with UE 5.8): Niagara, MetaSound, ControlRig, IKRig,
AnimationWarping, AnimationLocomotionLibrary, AudioModulation (only `Audio/Modulation` control buses).

## Exact paths for the C++ hookup

### Characters
| What | Path |
|---|---|
| Skeleton | `/Game/Characters/Heroes/Mannequin/Meshes/SK_Mannequin.SK_Mannequin` |
| Body mesh (male) | `/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny` |
| Body mesh (female) | `/Game/Characters/Heroes/Mannequin/Meshes/SKM_Quinn.SKM_Quinn` |
| Physics asset | `/Game/Characters/Heroes/Mannequin/Rig/PA_Mannequin.PA_Mannequin` |
| Mannequin material | `/Game/Characters/Heroes/Mannequin/Materials/M_Mannequin.M_Mannequin` (instances in `Materials/Instances/Manny`, `/Quinn`) |

Locomotion (`/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/`, prefix `MM_Rifle_`):
`Idle_Hipfire`, `Idle_ADS`, `Jog_Fwd`, `Jog_Bwd`, `Jog_Left`, `Jog_Right` (+ `_Start`/`_Stop`/`_Pivot`),
`Walk_Fwd/Bwd/Left/Right`, `Crouch_Idle`, `Crouch_Walk_Fwd/Bwd/Left/Right`, `Jump_Start`,
`Jump_Start_Loop`, `Jump_Apex`, `Jump_Fall_Loop`, `Jump_Fall_Land`, `Jump_RecoveryAdditive`,
`TurnLeft_90/180`, `TurnRight_90/180`.
Same set exists under `.../Locomotion/Unarmed/` (prefix `MM_Unarmed_`) and `.../Locomotion/Pistol/`.
Blend spaces: `.../Locomotion/Rifle/BS_MM_Rifle_Jog_Leans`, `BS_MM_Rifle_Crouch_Walk`,
`.../Locomotion/Unarmed/BS_MM_Unarmed_Jog_Walk`.
Aim offsets: `/Game/Characters/Heroes/Mannequin/Animations/AimOffsets/AO_MM_Rifle_Idle_Hipfire`,
`AO_MM_Rifle_Idle_ADS`, `AO_MM_Rifle_Crouch_Idle`, `AO_MM_Pistol_Idle_ADS`, `AO_MM_Unarmed_Idle_Ready`.

Action montages (`/Game/Characters/Heroes/Mannequin/Animations/Actions/`):
`AM_MM_Rifle_Melee`, `AM_MM_Pistol_Melee`, `AM_MM_Shotgun_Melee`, `AM_MM_Rifle_GrenadeToss`,
`AM_MM_Death_Front_01..03`, `AM_MM_Death_Back_01`, `AM_MM_Death_Left_01`, `AM_MM_Death_Right_01`,
`AM_MM_HitReact_{Front,Back,Left,Right}_{Lgt,Med}_01` (+ `Front_Lgt_02..04`, `Front_Med_02`, `Front_Hvy_01`).

### Weapons
| Weapon | Skeletal mesh | Static mesh | Character montages | Weapon montages |
|---|---|---|---|---|
| Rifle | `/Game/Weapons/Rifle/Mesh/SK_Rifle` | `/Game/Weapons/Rifle/Mesh/SM_Rifle` | `/Game/Weapons/Rifle/Animations/AM_MM_Rifle_{Fire,Reload,Equip,DryFire}` | `AM_Weap_Rifle_{Fire,Reload}` |
| Pistol | `/Game/Weapons/Pistol/Mesh/SK_Pistol` | `/Game/Weapons/Pistol/Mesh/SM_Pistol` | `/Game/Weapons/Pistol/Animations/AM_MM_Pistol_{Fire,Reload,Equip,DryFire}` | `AM_Weap_Pistol_{Fire,Reload}` |
| Shotgun | `/Game/Weapons/Shotgun/Mesh/SKM_Shotgun` | `/Game/Weapons/Shotgun/Mesh/SM_Shotgun` | `/Game/Weapons/Shotgun/Animations/AM_MM_Shotgun_{Fire,Reload}` | `AM_Weap_Shotgun_{Fire,Reload}` |
| Grenade | — | `/Game/Weapons/Grenade/Mesh/SM_grenade` | `AM_MM_Rifle_GrenadeToss` (above) | — |

Muzzle socket names were not inspected (needs the editor); look up the sockets on `SK_Rifle` etc. at
runtime with `GetAllSocketNames()` and log them on first use, or fall back to the mesh bounds.

### Effects (Niagara)
| Use | Path |
|---|---|
| Muzzle flash (rifle) | `/Game/Effects/Particles/Weapons/NS_WeaponFire_MuzzleFlash_Rifle` |
| Generic weapon fire (flash + tracer + shell) | `/Game/Effects/Particles/Weapons/NS_WeaponFire` |
| Tracer | `/Game/Effects/Particles/Weapons/NS_WeaponFire_Tracer` (shotgun: `NS_WeaponFire_Tracer_Shotgun`) |
| Shell eject | `/Game/Effects/Particles/Weapons/NS_WeaponFire_ShellEject` |
| Impacts | `/Game/Effects/Particles/Impacts/NS_ImpactConcrete`, `NS_ImpactGlass`, `NS_ImactSparksCharacter` (sic), `NS_ImpactDecals` |
| Grenade | `/Game/Effects/Particles/Explosion/NS_Grenade_Explosion`, `NS_Grenade_Trail` |
| Death | `/Game/Effects/Particles/Impacts/NS_DeathCubes` |
| Spawn-in | `/Game/Effects/Particles/Environmental/NS_CharacterSpawnIn` |
| Damage direction (screen) | `/Game/Effects/Camera/Damage/NS_Screen_DamageDirection` |
| Footsteps / dash | `/Game/Effects/Particles/Footsteps/NS_Footsteps`, `/Game/Effects/Particles/Environmental/NS_CharacterDash` |
| Pickup pad | `/Game/Effects/Particles/Item/NS_GunPad`, `NS_GunPad_Pickup` |

Lyra's Niagara systems expose user parameters (tracer end points, impact normals, etc.); inspect them with
`UNiagaraComponent::GetOverrideParameters()` before relying on names.

### Audio (MetaSound sources, play with `UGameplayStatics::PlaySoundAtLocation`)
| Use | Path |
|---|---|
| Rifle fire | `/Game/Audio/Sounds/Weapons/Rifle/MSS_Weapons_Rifle_Fire` (alt: `/Game/Audio/Sounds/Weapons/Rifle2/MSS_Weapons_Rifle2_Fire`, preset `/Game/Audio/MetaSounds/sfx_Weapon_SemiAutomaticRifle_nl_metaPreset`) |
| Pistol fire | `/Game/Audio/Sounds/Weapons/Pistol/MSS_Weapons_Pistol_Fire` |
| Shotgun fire | `/Game/Audio/Sounds/Weapons/Shotgun/MSS_Weapons_Shotgun_Fire` |
| Grenade explosion | `/Game/Audio/Sounds/Explosions/MSS_Explosions_Grenade` (or `/Game/Audio/MetaSounds/sfx_Weapon_GrenadeExplosion_nl_meta`) |
| Grenade bounce | `/Game/Audio/MetaSounds/sfx_Weapon_GrenadeImpact_nl_meta` |
| Melee swing / hit | `/Game/Audio/MetaSounds/sfx_MeleeWhoosh_nl_meta_Preset`, `sfx_Weapon_MeleeImpact_nl_meta` |
| Bullet impacts | `/Game/Audio/MetaSounds/sfx_ImpactDefault_nl_meta_Preset`, `sfx_ImpactCharacter_nl_meta_Preset`, `sfx_ImpactGlass_nl_meta_Preset` |
| Whiz-bys | `/Game/Audio/MetaSounds/sfx_WhizBys_nl_meta` |
| Zoom in/out | `/Game/Audio/MetaSounds/sfx_ZoomIn_nl_meta_Preset`, `sfx_ZoomOut_nl_meta_Preset` |
| More | `/Game/Audio/Sounds/{Weapons,Impacts,Explosions,WhizBys,Movement,Footsteps,UI,Grenade,SFX}/` |

## Git LFS budget
The imported content is about 1.45 GB of `.uasset` files in Git LFS. GitHub Free includes 10 GiB of LFS
storage and 10 GiB of download bandwidth per month, so each full clone with LFS costs ~1.45 GB of that.
Cloud sessions that only need the code should clone with `GIT_LFS_SKIP_SMUDGE=1`.
