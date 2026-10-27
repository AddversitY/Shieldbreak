# Desktop setup status

Inspected 2026-10-26 on the desktop at
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
- Hardware inventory: 16 GB RAM and NVIDIA GeForce RTX 2080 Ti.

## Required before building

Unreal Engine 5.8 is not registered or installed in the inspected Epic locations.
Only C: is available, with approximately 25 GiB free at inspection. Free at least
150 GB as the project's planning guideline, or provide another SSD before engine
installation. No personal files or games were removed.

Install **5.8** through Epic Games Launcher. The existing batch tools expect
`%USERPROFILE%\Epic Games\UE_5.8`, which on this desktop means
`C:\Users\addve\Epic Games\UE_5.8`. If using another drive, update the engine
resolution in the batch tools before using them; the PowerShell network tools
already accept `-EngineRoot`.

After installation:

1. Run the engine prerequisite installer if needed and generate project files
   for `Shieldbreak.uproject`.
2. Build `ShieldbreakEditor Win64 Development` and run `Tools\SelfTest.bat`.
3. Sequentially run `Tools\TestArenaNetwork.ps1` for Keel and Spire, followed
   by `Tools\Test4v4Network.ps1`, supplying `-EngineRoot` if needed.
4. Open the game and verify the menu and a practice match visually.
5. Record actual results in `docs/COORDINATION.md` and push the completed setup.

No engine build or gameplay/network tests have run on this desktop yet. Laptop
test results in the coordination document remain historical. Launcher account
access and any assets held only on the laptop have not been verified.
