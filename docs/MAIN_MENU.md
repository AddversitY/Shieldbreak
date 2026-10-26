# Main menu and 4v4 foundations

A plain game launch opens the main menu. Explicit `?Map=...`, `?Mode=...`, listen-server URLs,
dedicated servers and `-SBSelfTest` retain their gameplay entry path.

## Menu

- **Play:** choose one of five launch maps and Slayer, CTF, KOTH or Oddball.
- **Host Match:** starts a listen server, with eight human slots and no bots initially.
- **Practice:** starts a local team match with seven bots.
- **Join:** accepts a direct hostname or IPv4 address, with an optional port. Default port is 7777.
- **Settings:** cycles Low/Medium/High/Epic graphics presets and saves immediately.
- **Escape in a match:** opens Resume, Leave Match, Settings and Exit. Leaving a match asks
  for confirmation; leaving as the host disconnects other players.

The menu uses native Slate widgets and engine fonts; no new runtime art is required. Its dark,
single-accent presentation keeps the home screen to four actions and reveals match choices
only after Play. Navigation references:
[Halo Infinite menus](https://support.halowaypoint.com/hc/en-us/articles/4408368163348-Guide-to-Halo-Infinite-Menus)
and [Call of Duty's UI redesign](https://news.blizzard.com/en-us/article/24150096/the-road-to-black-ops-6-launch-preload-and-new-user-interface).

![Play menu, with Bastion selected](playtests/2026-10-26_menu.png)

## Efficiency and match limits

- Match-rule ticking is disabled while in the front end; no arena, objectives or bots are built there.
- Menu labels update on selection/error events. They do not allocate new label text through
  per-frame attribute bindings. Removed page widgets release their retained text/input references.
- Session configuration caps humans at eight. Bot reconciliation limits bots to remaining slots,
  and human arrivals remove excess bots before team assignment.
- Direct host/join uses Unreal's existing replicated gameplay. Matchmaking, NAT traversal,
  parties, ready-up lobbies, dedicated hosting and eight-human internet tests are future work.

## Verification (2026-10-26)

- UE 5.8 editor build passed.
- Gameplay suite: **84 passed, 0 failed** (`Saved/Logs/MenuGameplaySelfTest.log`, report `Saved/Logs/SelfTest.txt`).
- Visually inspected the 1280x720 home/Play screens. Clicked Play and changed Keel to Bastion;
  verified the event-driven label update. Screenshot: `Saved/Screenshots/AutoShot/menu_review_00.png`.
- Full click-through of Host/Join/Settings/Leave and gamepad navigation is still pending.
- Four-bot test averaged 10.9 FPS on this run. This was not a controlled performance benchmark,
  and no gameplay FPS gain is claimed from the menu optimizations.
- Keel and Spire headless server/client regression tests both passed after these changes.
- PlayTest.bat explicitly opens Keel Slayer; it continues to capture gameplay rather than the menu.
