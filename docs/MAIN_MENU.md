# Main menu and 4v4 foundations

A plain game launch opens the main menu. Explicit `?Map=...`, `?Mode=...`, listen-server URLs,
dedicated servers and `-SBSelfTest` retain their gameplay entry path.

## Menu

- **Play:** shows only Multiplayer, Host Match, Practice and Back. No map or game-type selectors appear here.
- **Multiplayer:** an organized list of Keel, Bastion, Sanctum, Creekbed and Spire. Each row lists
  its game types; selecting a map shows Slayer, Capture the Flag, King of the Hill and Oddball
  in the adjacent panel. Done returns to Play and retains the selection.
- **Host Match:** opens the map/game-type browser, then starts a listen server from its Host Match
  button, with eight human slots and no bots initially.
- **Practice:** opens the same browser, then Start Practice launches the selected local team match with seven bots.
- **Join:** accepts a direct hostname or IPv4 address, with an optional port. Default port is 7777.
- **Settings:** cycles Low/Medium/High/Epic graphics presets and saves immediately.
- **Escape in a match:** opens Resume, Leave Match, Settings and Exit. Leaving a match asks
  for confirmation; leaving as the host disconnects other players.

The menu uses native Slate widgets and engine fonts; no new runtime art is required. Its dark,
single-accent presentation keeps the home screen to four actions and reveals match choices
only inside Multiplayer. Escape/Back from the browser returns to Play. Navigation references:
[Halo Infinite menus](https://support.halowaypoint.com/hc/en-us/articles/4408368163348-Guide-to-Halo-Infinite-Menus)
and [Call of Duty's UI redesign](https://news.blizzard.com/en-us/article/24150096/the-road-to-black-ops-6-launch-preload-and-new-user-interface).

![Dedicated Multiplayer map and game-type browser](playtests/2026-10-26_multiplayer-browser.png)

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

## Multiplayer browser revision (2026-10-26)

- User-requested navigation: Play contains exactly Multiplayer, Host Match, Practice and Back.
- Map and game-type selection is confined to the two-column Multiplayer browser.
- UE 5.8 build passed. In-game visual checks verified the four Play actions, opening Multiplayer,
  the map/game-type columns, default selection highlighting, and the unclipped layout.
- Changing selections, return navigation and launching matches from this revised screen still
  need a complete click-through check; the screenshot run closed automatically before those checks.
- Compact row padding keeps all five maps and the action buttons inside the 1280x720 layout.
- This revision changes only Slate UI/navigation. The earlier 84-check gameplay suite and
  Keel/Spire network results above were not rerun for this UI-only change.