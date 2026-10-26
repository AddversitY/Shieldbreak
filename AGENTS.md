# Shieldbreak development

Read `docs/COORDINATION.md` before changing the project. Preserve other sessions' work and
check Git status and remote changes before starting. `main` is the shared integration branch.

## User direction (2026-10-26)

- Shieldbreak is a multiplayer **4v4** arena FPS.
- The main menu should be clean, sleek, simple and concise, informed by current AAA titles.
- At the end of each completed change, update `docs/COORDINATION.md`, run appropriate checks,
  commit and push to GitHub so Claude can pick development back up. The user has authorized
  these GitHub updates; do not leave completed changes only on this PC.
- Describe actual test results and unresolved work accurately. Do not claim online matchmaking,
  internet reachability, dedicated hosting or eight-human testing without verification.

## Development constraints

- Unreal Engine 5.8: `C:\Users\vince\Epic Games\UE_5.8` on the development PC.
- This PC has 8 GB RAM. Run expensive builds/game tests sequentially.
- Binary assets use Git LFS. Never replace downloaded assets with LFS pointers.
- `Tools/SelfTest.bat` runs gameplay checks; `Tools/TestArenaNetwork.ps1` checks real local
  server/client joins. Keep explicit match URLs working alongside the startup menu.
- Git commits can use the connected account's private address when local identity is absent:
  `AddversitY <341202389+AddversitY@users.noreply.github.com>`.
