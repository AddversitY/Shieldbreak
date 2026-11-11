# Rifle reload: continuous motion pass (2026-11-11)

Branch `claude/project-thread-cj978d`, starting at `ace8b62`. Claude took over from Codex. Vincent had not approved the `ace8b62` reload. This pass changes keyframe data in `Source/Shieldbreak/Visuals/ShieldbreakViewmodelMoves.cpp` only. Visual acceptance is still Vincent's call.

## What was wrong in `ace8b62` (frame review of `before.mp4`)

- The rifle snapped into its reload cant in about 0.17 s, then held almost still for over a second.
- After the seat, the left hand opened with splayed fingers and the palm toward the camera (`before-sheet-1.jpg`, 1.10 s).
- The rifle snapped back to idle in about 0.13 s (`before-sheet-recovery.jpg`, frames 208 to 212).

## Changes

- **Gun keys.** The gun eases into the cant, yanks down with the strip and tips further toward the returning hand to present the well. It dips into the push, kicks on the seat with a damped rebound, and settles with a small overshoot.
- **Hand after the seat.** The heel of the hand follows through on the magazine base, then slides forward under the receiver to the forend. The old path swung it out to the side.
- **Fingers.** They relax partway after the seat instead of opening fully. The "open" curl set is now a relaxed hand rather than a splayed one.
- **Lift envelope.** It eases over 20% of the reload at each end, up from 15%. This envelope is shared, so the sniper, sidearm and launcher reloads also ease in and out a little more slowly.
- **Unchanged.** Magazine event times, gameplay reload time, ammunition, imported art and audio are unchanged.

## Results

| Check | Result |
|---|---|
| Build (`-MaxParallelActions=2`) | Succeeded |
| Focused first-person moves, fixed 30 fps | **15 / 15** (wrist 65.4°, middle joint 50.4°, grip drift 0.000 cm) |
| Hands never pass through guns | **3 / 3** |
| Full gameplay suite | **177 / 177** (camera-step check passed this run; it has been intermittent) |
| First-person captures | **9 / 9** |

The first clipping run (`reports/clipping-invalid-checkout-switch.txt`) failed on every weapon's gun hand. Another session had switched the shared checkout to a Keel branch while that run was in progress. The rerun on the rebuilt branch passed. The invalid run is kept for the record.

**FPS** (fixed-view Keel, 1280x720, rendered offscreen, no bots):

| Run | Build | FPS |
|---|---|---|
| Morning | Old keys | 185.6 |
| Evening, single run | New keys | 117.1 |
| Evening, back-to-back | Old keys | **119.6** |
| Evening, back-to-back | New keys | **125.0** |

The morning-to-evening drop also shows up on the old keys, so it comes from background load on the desktop, not from this change. The back-to-back pair shows no regression. These are single observations, not a controlled benchmark. The four-bot figure in the full suite was 114.5 FPS (new keys only).

## Media

- `before.mp4`: the `ace8b62` clip.
- `after.mp4`: 100 real game frames (1280x720, 30 fps, 3.33 s, silent), from the same capture window as before.
- `*-sheet-*.jpg`: frame-by-frame contact sheets with timestamps.

## Still open

- The hand is out of frame for about 0.3 s while it fetches the fresh magazine.
- The return to idle is smoother but still fairly quick.
- Finger contact on the fresh magazine, audio timing against this sequence, and polish on other weapon styles are not reviewed.
- No network, internet or eight-human testing was done for this cosmetic change.
