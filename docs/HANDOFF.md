# Handoff (2026-10-25)

Start here if you are an AI or person picking up Shieldbreak. It gives where things stand, the rules Vincent has
set, and what comes next. The other docs go deeper: `ARCHITECTURE.md` (code), `DESIGN.md` (gameplay),
`ART_DIRECTION.md` (look), `MAPS.md`, `RELEASE.md`, `STORE_PAGE.md`, `ASSET_CREDITS.md`, and `COORDINATION.md`
(test results and the known-issues backlog).

## Where the code is
- Repo `AddversitY/Shieldbreak`. All work is on branch **`claude/project-thread-iqwexu`** (PR #2, open, not
  merged). `main` only has the initial commit. **Never merge to main or rewrite history unless Vincent says so.**
- UE 5.8, C++ only (Slate UI built in code, no Blueprints needed). Source is in `Source/Shieldbreak/`, grouped as
  Game, Player, Weapons, Shield, UI, Audio, Visuals and Test.
- Vincent's Windows laptop (8 GB RAM, Intel Iris Xe) has the repo at `C:\Users\vince\development\Shieldbreak` and is
  where things build and run. **Run only one Unreal process at a time** on it.
- The self-test (`Test/ShieldbreakSelfTest.cpp`, run with `-SBSelfTest`) has about 80 gameplay checks. Results are
  logged in `COORDINATION.md`.

## Rules from Vincent
- Work end to end without asking. He gave blanket permission for anything that doesn't need his hands. App and
  firewall permission prompts on the laptop are his to click.
- **No Halo or Microsoft IP.** That means no Halo assets, names, map names or weapon names, and no ring/halo in the
  sky (he ruled that out as a copyright risk). Halo-*inspired* is fine: angled Forerunner-ish metal, glowing trim,
  haze.
- **Send him a shortlist with prices before downloading any Fab or marketplace asset.**
- Don't commit the editor's `AndroidFileServer` block in `Config/DefaultEngine.ini`. It holds a SecurityToken.
- Git LFS uploads don't work from the cloud sandbox (lfs.github.com is blocked), so binaries generated in the cloud
  are committed as plain blobs via `.gitattributes` exceptions (`Art/Export/**`, `Content/Splash/*.png`). The laptop
  can push LFS normally.

## Done so far (highlights)
- Core game: recharging shields (vent, Break Pulse), battle rifle, sidearm, sniper, rockets, Rail Lance, Aegis
  barrier, active camo, overshield, sprint/slide/Power Jump, grenades, medals, bots (4 skills), Team Slayer, CTF,
  KOTH and Oddball on five maps (Keel, Bastion, Sanctum, Creekbed, Spire), plus LAN and direct-IP multiplayer.
- Our own art, all scripted: weapons and armor in Blender (`Art/blender/`), the logo, icon
  and splash in Pillow (`Art/branding/make_branding.py`), and menu music in numpy (`Art/audio/make_music.py`). The
  laptop imports with `Tools/ImportArt.bat`.
- Main menu redesign (2026-10-25): a logo panel on the left over a live, slowly orbiting view of the selected map,
  a map card at bottom right, and menu music. Verified at 720p. Shots are in `docs/playtests/2026-10-25_menu/`.
- Settings (graphics auto-detect, window mode, VSync, frame cap, volumes, FOV, sensitivity) and key rebinding.
- **Emblem:** Vincent saw alternatives B-G (`Art/branding/emblem_options.py`) and **kept the original cracked
  shield (A)**. He likes the music.

## What's next
1. **Map and character art. Latest decision: AI 3D generation** (Vincent, 2026-10-25, later the same day; it
   replaces the hybrid plan below, which stays as background). Recommended tool: **Meshy Pro ($20/mo)**. It has a
   private commercial license, an API and an Unreal bridge. Avoid free tiers: Meshy's free output is CC BY 4.0 and
   Tripo's is non-commercial. Plan: generate helmets, armor, weapons and map pieces from prompts or Vincent's
   references, clean up and retexture in Blender, fit armor onto the Lyra mannequin (AI output isn't rigged), import
   with `Tools/ImportArt.bat`, then show in-game shots. **Blocked on Vincent:** he creates the Meshy account and API
   key himself. For the cloud to call the API, the project needs a custom cloud environment with env var
   `MESHY_API_KEY` and `api.meshy.ai` in Allowed domains (the default environment blocks it). Never ask for the key
   in chat.

   Earlier decision, **"Hybrid (free Fab + ours)"** (Vincent, 2026-10-25). Use free or permissively
   licensed packs for the bulk of the environment kit and character base, and keep making our own (Blender scripts)
   for the signature pieces: weapons, armor and trims. Steps:
   - Build the shortlist and **send it to Vincent before downloading**. Candidates found so far:
     - Quaternius "Modular Sci-Fi MegaKit", https://quaternius.itch.io/modular-sci-fi-megakit. The Standard tier
       is free (190 assets, CC0, commercial OK). Pro costs $9.99 and Source (with an Unreal project and .blend)
       costs $14.99. Stylized and low poly, so it suits the laptop.
     - Fab "Modular SciFi Station" (Martin Milz, 147 meshes, Fab Standard License),
       https://www.fab.com/listings/0667e321-31a7-40ed-b85c-6db5bbc4366b. It was limited-time free until
       2026-06-16, so check the current price and whether Vincent already claimed it.
     - Fab "Science Fiction Desert City Kit" (LAYA DESIGN, 120 meshes),
       https://www.fab.com/listings/81880856-a0e1-492f-8686-3afbda15ab43. It was free until 2026-07-14, so
       check the price. It might suit Creekbed.
     - Characters: browse Fab for a rigged sci-fi armored soldier on the UE5 mannequin skeleton (search results
       include "Sci-fi Trooper" and "Sci-Fi Armored Character"). Check the price and license of each. The current
       bodies are Lyra mannequins wearing our Blender armor.
   - Check every license (Fab Standard License or CC0 is fine for a commercial UE game) and record it in
     `ASSET_CREDITS.md`.
   - Fab's free Megascans are no longer free (since 2024), so don't promise them.
   - Vincent's art-direction page is https://claude.ai/artifact/UkNvMowRe8jwaosVVYAbsk. He hasn't made picks yet.
2. Map dressing pass once a kit is chosen: replace the cube greybox (`Game/ShieldbreakArena.cpp` builds the maps in
   code) with kit meshes, keeping the layouts and collision.
3. Steam page assets after the art pass (see `STORE_PAGE.md`): 1920x1080 screenshots and a trailer.
4. Backlog from `COORDINATION.md`: performance on the laptop (about 30-35 FPS at 720p Low), bot animation blend
   spaces, and Plasma Rifle art.

## Small open items
- Two Windows Firewall prompts on the laptop were waiting for Vincent's click (Unreal asking for network access).
- Two old git stashes on the laptop hold earlier local config edits. They haven't been touched, so leave them
  unless Vincent asks.
