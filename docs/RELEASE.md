# Release checklist

What stands between Shieldbreak and a public release, in rough order. Status as of 2026-10-25.
Legend: **done**, **in progress**, **todo**, **decide** (Vincent's call).

## 1. A build people can run
| Item | Status | Notes |
|---|---|---|
| Standalone Windows build (no editor) | done | `Tools/Package.bat` (RunUAT BuildCookRun Win64 Development): 1.2 GB, menu and a full match work in the packaged exe (2026-10-25). Only the folders the code loads by path are always-cooked. |
| Shipping configuration | todo | Package Development first; switch to Shipping (no console, smaller, faster) once the Development build is clean. |
| Window title, version, company, copyright | done | `GeneralProjectSettings` in Config/DefaultGame.ini (v0.2.0). |
| Game icon and splash screen | done (first pass) | Original cracked-shield emblem and wordmark from `Art/branding/make_branding.py`: `Build/Windows/Application.ico`, `Content/Splash/Splash.bmp`, store banners in `Art/Export/Branding/`. Vincent may want a different look. |
| First-launch shader compile | todo | Development builds compile shaders on first run (the laptop took minutes). Ship a PSO cache or show a "preparing shaders" screen. |
| Minimum spec | todo | Measured: Iris Xe laptop 20-30 FPS at 720p with 4 bots. Pick a target (e.g. GTX 1060 or better at 60 FPS) and test on it. |

## 2. Legal
| Item | Status | Notes |
|---|---|---|
| Unreal Engine EULA notice in the credits | done | Main menu footer. |
| Unreal royalty | note | 5% of gross revenue above the first US$1 million per product, reported quarterly. Free games owe nothing. |
| Lyra content (mannequin, animations, effects, sounds) | done | Fab Standard License, UE-only: fine to ship inside an Unreal game, not as raw files. Credited in the menu and in `docs/ASSET_CREDITS.md`. |
| No Halo or Microsoft IP | ongoing | Our maps, guns and armor are original. Store text and trailers must not use "Halo", "Spartan", "Master Chief", Halo map names or Halo art. The design docs mention Halo as inspiration only. |
| Name check | decide | Search for "Shieldbreak" on Steam, itch.io and the trademark registries before buying a store page. |
| Privacy policy | todo | Needed only if the game collects data (accounts, analytics, online services). LAN-only has nothing to collect. |

## 3. Store and distribution
| Item | Status | Notes |
|---|---|---|
| Where to release | decide | **itch.io**: free to list, good for a first public test. **Steam**: US$100 per game, Steamworks review takes a few days, store page needs to be up 2+ weeks before launch. |
| Store page assets | in progress | Text draft, tags and requirements in `docs/STORE_PAGE.md`; capsule/header banners made. Screenshots and a trailer once the art pass lands. |
| Age rating | todo | Steam uses the IARC questionnaire (free). Expect a teen-type rating for sci-fi shooting without gore. |

## 4. The game itself
| Item | Status | Notes |
|---|---|---|
| Core loop: 5 maps x 4 modes, bots, shields, weapons | done | Self-test 99/99, every map tour passes. Bastion KOTH has bots getting stuck: being fixed. |
| First-person arms, throw, melee, reload, swap animations | in progress | Placement tuned on the PC; throw readability fix pending verification. |
| Art pass: characters, weapons, maps | in progress | Vincent picks the direction on the art direction page; maps got textured materials, glow trims and mountains. |
| Online play over the internet | decide | Today: LAN and join-by-IP. For internet play: Steam networking (Steam only) or Epic Online Services (free, any store) for lobbies and NAT traversal. Listen servers are fine for small matches. |
| Audio: music and more sound variety | in progress | Original synthesized menu theme (48 s loop, `Art/audio/make_music.py`) in the menu and on the results screen, music volume setting, menu click sounds. In-match music is deliberately absent for now. |
| Controller support | in progress | Full controller bindings plus aim assist (friction and gentle tracking); feel check on a real pad pending. |
| Settings | done | Settings screen: sensitivity, FOV, invert, tips, graphics quality, resolution scale, display mode, VSync, frame limit, master and music volume. Controls screen: keyboard/mouse rebinding. Pending a PC check. |
| Tutorial or how-to-play | in progress | Six new-player tips in the first match of each launch (can be turned off); F1 shows every control. A short training map would still help. |
| Crash reporting | todo | Turn on the crash reporter in packaged builds and collect logs from testers. |

## 5. Testing before launch
- Two real PCs on a LAN (the two-window test passes on one PC).
- A clean Windows machine with no Unreal installed: install the build and play a match.
- A handful of outside testers on the itch.io build, collecting feedback on movement, weapons and maps.
