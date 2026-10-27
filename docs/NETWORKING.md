# 4v4 networking foundations

Current multiplayer uses an Unreal listen server with direct-address joins. Matchmaking,
parties, ready-up lobbies, internet/NAT traversal, dedicated hosting and eight-human play
are not yet verified or implemented as a complete service.

## Server rules

- `ShieldbreakMatchRules::MaxPlayers` is eight (two teams of four). The server applies this
  to its live GameSession, even if its launch URL requests a larger MaxPlayers value.
- Human players take priority over bots. When a human joins a full practice roster, a bot
  is removed before the new player's team is assigned. Reconciliation fills an available
  bot slot after a human disconnects, up to the configured desired bot count.
- `SB_Bots`, `SB_Skill` and `SB_Map` are authorized inside their server RPC implementations.
  Only the local listen host or standalone controller may change these match-wide settings.
  A remote client's request is rejected without changing the match.
- Dedicated-server operator/admin commands need a separate future administration path;
  a connected client is not treated as an administrator.

## Repeatable local regression

Build the editor target, then run `Tools/Test4v4Network.ps1`. Keep local UDP port 7777 free.
The runner starts only its own local server/client processes and stops them when finished.
It disables rendering to reduce memory use. No external internet connection is required.

The opt-in `-SBNetworkTest` probes run only in this regression. No probe or roster-sampling
timers are created in an ordinary game. Do not add that test flag to normal play shortcuts.

Verified on 2026-10-26:

1. The actual listen host's bot-count and difficulty commands are accepted.
2. A MaxPlayers=16 launch URL and 16 requested bots still produce only 1 human + 7 bots,
   split four per team, with the runtime player limit fixed at eight.
3. A real client joins and replaces a bot: 2 humans + 6 bots, four per team.
4. The client's bot-count, difficulty and map-change RPCs are rejected by the server.
5. After that client disconnects, its slot refills: 1 human + 7 bots, four per team.
6. All 82 sampled rosters stayed within team/session limits and kept the host's settings.

Reports: `Saved/Logs/4v4Network-Server.log` and `4v4Network-Client.log`.
The general gameplay suite also passed **85/85** with an oversized MaxPlayers URL;
report `Saved/Logs/SelfTest.txt`, run log `Saved/Logs/HostControlsSelfTest.log`.

This is a two-process local test with bots, not an eight-human or internet latency test.
The existing imported FootstepEffectTagModifier asset logs missing Lyra type/compiler
errors in both old and current runs; asset cleanup remains a separate backlog item.

## Connection recovery

`UShieldbreakGameInstance` keeps local connection feedback across world/controller
replacement. It tracks the pending connection's owning game instance and driver, including
direct startup URLs, so pending failures with no `World` are still attributed correctly.
Connected clients use Unreal's normal disconnect travel back to the default map. The new
front end consumes the saved message, opens Join Match and restores the host address.

Messages distinguish join timeouts, host loss and version mismatch without showing raw
server/driver strings. Successful joins, host launches and practice launches clear old
feedback. Ordinary client departures do not put a listen host into a retry screen.
No reconnect attempt is automatic; the player controls retry or chooses another address.

After building, run `Tools/TestConnectionRecovery.ps1` (optional `-EngineRoot` and `-Port`,
default test port 17877). It starts only its own local processes, uses rendering-disabled
clients and checks:

1. A real client joins, then returns to a visible retry screen after its host process stops.
2. A join to an unavailable local port returns to Join with its address intact.
3. One subsequent attempt joins a real local host, with no stale menu or saved error.

The opt-in `-SBConnectionTest` helper runs only in this regression. Normal matches do not
create its timers or perform its connection attempts. Reports are
`Saved/Logs/ConnectionRecovery-HostLoss-{Server,Client}.log` and
`Saved/Logs/ConnectionRecovery-Retry-{Server,Client}.log`.
These are local transport/UI recovery checks; internet reachability and NAT traversal
remain unverified.

Verified 2026-10-27: editor build and all connection-recovery scenarios passed. The
existing 4v4 regression also passed with 21 balanced roster samples. General gameplay
finished 86/86 on an unchanged rerun after the first run missed one existing rocket-jump
velocity sample; see `COORDINATION.md` for both results. A rendered direct-startup timeout
also showed the retained address, feedback and usable Join screen at 1280x720.
