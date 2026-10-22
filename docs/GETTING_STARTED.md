# Getting Started

These are the steps to take, in this order, before writing any gameplay.

## 1. Install the toolchain (Windows recommended)

| Tool | Why |
|---|---|
| **Epic Games Launcher** → latest **Unreal Engine 5.x** | The engine. Also install "Editor symbols for debugging" so C++ crashes give readable stacks. |
| **Visual Studio 2022** (Community is fine) | Workloads: *Game development with C++* and *Desktop development with C++*. Include the Unreal Engine installer component and a Windows 10/11 SDK. |
| **JetBrains Rider** (optional) | Easier to work in for Unreal C++ than VS. Free for non-commercial use. |
| **Git + Git LFS** | Run `git lfs install` once. `.uasset`/`.umap` files are binary and will bloat the repo without LFS. |

Hardware guideline: 32 GB RAM, a recent 8+ core CPU, a GPU with 8 GB+ VRAM, and 150 GB+ free SSD space. The first shader compile takes a long time; that is expected.

macOS works with Xcode, but Windows is the main platform for Unreal FPS development.

## 2. Create the project inside this repo

1. Clone this repo: `git clone https://github.com/AddversitY/Shieldbreak.git`, then run `git lfs install`.
2. In the Unreal Project Browser choose **Games → First Person**.
3. Set **Project Type: C++** (not Blueprint). Shields, networking, and movement need C++.
4. Leave **Starter Content off**, then pick the **Arena Shooter** variant if your engine version offers one. Otherwise use the base template.
5. Set **Location** to the folder that *contains* your clone and **Name** to `Shieldbreak`, so `Shieldbreak.uproject` ends up at the repo root next to this `docs/` folder.
6. Open the project and press Play to confirm it builds and runs.
7. Commit: `git add . && git commit -m "Create UE5 C++ first-person project" && git push`.

The `.gitignore` and `.gitattributes` in this repo already handle Unreal's generated folders and LFS tracking.

## 3. Change these project settings right away

- **Edit → Project Settings → Engine → Input**: use **Enhanced Input** (the template already does). You will add actions for Sprint/Slide, Shield Shape, and Overcharge Vent.
- **Maps & Modes**: create `Maps/L_Greybox_Arena` and make it the default editor and game map.
- **Collision**: add custom object channels `Projectile` and `ShieldPulse`.
- **Plugins**: enable *Gameplay Abilities* (GAS). The ability and attribute system fits shields, cooldowns, and the advanced mechanics well.

## 4. Milestone 0: "30 seconds of fun" (about 2–4 weeks)

Before adding any of the advanced mechanics, get the core loop to *feel* like Halo:

1. **Movement tuning**: lower gravity than default (about 0.8–0.9×), a floaty high jump, moderate walk speed, little or no air-control penalty. Use the Halo 3 pace as the reference.
2. **Shield + health component** (C++ `UShieldHealthComponent`):
   - Shield absorbs all damage until empty; overflow damage carries into health.
   - Recharge delay of about 4–5 s after the last hit, then recharge to full over about 2 s.
   - Health does not regenerate until shields are full (Halo 2 style), or regenerates slowly (Halo 3 style). Pick one and keep it.
   - Hooks for the HUD, audio (the recharge "whine"), and the shield-pop VFX.
3. **One hitscan rifle** (BR-like 3-round burst) with headshot-only kills on unshielded targets.
4. **Melee** that removes most of a full shield.
5. **Frag grenade** with a bounce and a fixed fuse.
6. **Greybox arena** built from BSP/modeling-tool blocks with symmetrical lanes.
7. **Bots** (simple Behavior Tree) so you can playtest alone.

Only start on the "hard to master" mechanics in `DESIGN.md` once this loop feels good.

## 5. Plan multiplayer from day one

Halo-style games live or die on multiplayer. Even if you only ship bots at first:

- Write shields, weapons, and movement as **server-authoritative, replicated** C++ from the start. Retrofitting replication later is painful.
- Test in the editor with **Play → Net Mode: Play As Listen Server**, 2 players, and *Emulate Network* lag (~80 ms) turned on.
