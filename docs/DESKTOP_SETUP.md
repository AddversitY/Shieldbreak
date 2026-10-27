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
- Hardware inventory: i9-9900K (8 cores / 16 threads), 16 GB RAM and NVIDIA
  GeForce RTX 2080 Ti. Windows 11 Pro; x64 Visual C++ runtime installed.
- Desktop shortcuts: Shieldbreak (normal game/menu launch), Shieldbreak Editor,
  and Shieldbreak Playtest (existing build/screenshot script).

## Required before building

As of 2026-10-27, UAC cleared and UE **5.8.4** files are downloaded at
`C:\Users\addve\Epic Games\UE_5.8`, matching the existing batch tools' path.
Selected core, templates, source and editor debugging symbols total 83.309 GB
per Epic Launcher. Space increased to approximately 127 GiB before installation
without deletion by this session; approximately 43 GiB remains afterward.
The launcher appeared inactive before completion; restarting it resumed file
verification. Verification is now complete and Epic offers Launch. Its `.item`
manifest records `bIsIncompleteInstall=False`; the older LauncherInstalled.dat
did not update. The local watcher now checks both manifest formats.

A project-generation probe during installation logged a missing UnrealEditor
target rule. Generation was rerun after installation and succeeded without
that exception. VS 2022 still warns that it cannot load .NET 10 C# engine
automation projects; the C++ editor build is currently running with MSVC 14.44
and SDK 26100. Generated
`.slnx` and `.vsconfig` files are ignored with the other local IDE output.

The user explicitly authorized overnight continuation. Local ignored
`Saved/DesktopSetup.ps1` waits for Epic's installation record, then generates
project files, runs SelfTest, and runs the three local network regressions
sequentially. Read `Saved/Logs/DesktopSetupPhase.txt` and check running processes
before starting duplicate work. A 15-minute heartbeat named
`finish-shieldbreak-desktop-setup` continues the chat, diagnoses failures and
completes the handoff; stop it once setup is complete and pushed.

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
is signed in; any assets held only on the laptop have not been verified.
