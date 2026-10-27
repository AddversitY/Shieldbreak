# Desktop setup status

Setup completed and verified 2026-10-27 on the desktop at
`C:\Users\addve\Documents\ChatGPT\Shieldbreak`.

## Ready

- Synced `main` through `a9ce29b` before setup.
- Installed standalone Git for Windows 2.56.0.windows.3 with Git LFS 3.8.1.
- Initialized repository-local LFS hooks and configured the authorized private commit identity.
- Full `git lfs fsck` passed using the pre-existing bundled LFS 3.7.1;
  tracked binary assets are present and their hashes verified.
- Visual Studio Community 2022 17.14.43 is installed with Desktop development
  with C++ and Game development with C++ workloads, MSVC 14.44.35207 and
  Windows SDK 10.0.26100.0.
- Epic Games Launcher is installed.
- Hardware inventory: i9-9900K (8 cores / 16 threads), 16 GB RAM and NVIDIA
  GeForce RTX 2080 Ti. Windows 11 Pro; x64 Visual C++ runtime installed.
- Desktop shortcuts: Shieldbreak (normal game/menu launch), Shieldbreak Editor,
  and Shieldbreak Playtest (existing build/screenshot script).
- UE **5.8.4**, including editor debugging symbols, installed and verified at
  `C:\Users\addve\Epic Games\UE_5.8`, matching the batch tools' expected path.
- Per-user engine registration and `.uproject` file association configured.
  Launching `Shieldbreak.uproject` through Windows successfully opened the
  project with the installed engine. Explorer also has Open, Launch game and
  Generate Visual Studio project files commands through UnrealVersionSelector.

## Verification

- Visual Studio project generation succeeded after completed installation.
- `ShieldbreakEditor Win64 Development` build succeeded, exit code 0, using
  MSVC 14.44 and Windows SDK 26100 (103.17 seconds on the initial build).
- `Tools/SelfTest.bat`: **85 passed, 0 failed**. The suite reported 116.6 average
  FPS over 80 real seconds with four bots; this is a test-run measurement,
  not a controlled hardware benchmark.
- `Tools/TestArenaNetwork.ps1 -Map Keel` and `-Map Spire`: both PASS, with a real
  local client joined for 30 game seconds and no arena reference errors.
- `Tools/Test4v4Network.ps1`: PASS, including host-only controls, rejection of
  remote match-control requests, bot replacement on join, refill on disconnect,
  and all 21 sampled rosters staying within balanced 4v4 capacity.
- Visually verified the normal game home screen, the four-item Play screen,
  separate Multiplayer browser, Done return, Practice selection, Keel Slayer
  practice launch with rendered arena/weapons/bots, Escape match menu, Leave
  confirmation, return to the main menu and Exit.
- Opened the project in Unreal Editor and visually verified the loaded Entry
  level. Its empty editor viewport is expected: the arena is built at runtime.
- Closed the test game/editor processes afterward. No assets were edited.

Reports are under `Saved/Logs`: `SelfTest.txt`, `SelfTestRun.log`,
`DesktopProjectFiles.log`, `DesktopNetworkKeel.txt`, `DesktopNetworkSpire.txt`,
`DesktopNetwork4v4.txt`, `ArenaNetwork-*`, `4v4Network-*`, `DesktopVisual.log`
and `DesktopEditor.log`. The self-test batch wrapper emitted file-sharing
messages while appending its run-end log markers; the gameplay report itself
completed with 85/85 PASS, and the later independent network checks passed.

## Setup notes and scope

Epic's installation manifest (`.item`) confirms `bIsIncompleteInstall=False`.
The older LauncherInstalled.dat did not update, so the one-off local setup
watcher was updated to recognize `.item` manifests. Restarting the launcher
resumed stalled final verification without redownloading the engine.

VS 2022 warns that it cannot load .NET 10 C# engine automation projects;
the C++ editor target builds successfully with the engine's bundled .NET SDK.
Generated `.slnx` and `.vsconfig` files are ignored. First editor launch generated
Android file-server settings in DefaultEngine.ini; that unrelated block was
removed, leaving the tracked configuration unchanged.

Approximately 39 GiB remains free after installation and build. No personal
files or games were removed. Launcher account is signed in. Assets held only
on the laptop and untracked laptop preferences have not been transferred or
compared. These checks establish local desktop readiness, not internet hosting,
online matchmaking, dedicated hosting or eight-human multiplayer verification.
The overnight setup heartbeat is stopped after the final handoff is pushed.
