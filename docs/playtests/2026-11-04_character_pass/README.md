# Desktop character verification — 2026-11-04

Baseline: main `abfe0fa`, with identical FP measurement instrumentation from `0a2f52b`.
After: Claude's `f7d3adc` character branch plus merged instrumentation and desktop fixes.
Machine: RTX 2080 Ti desktop, UE 5.8.4. Actual captures are 1280x720 in both sets.

| Observation | Before | After |
|---|---:|---:|
| Crucible gameplay, four bots, 76 real seconds | 127.8 FPS | 125.4 FPS |
| Keel fixed first-person, no bots, about 20 seconds | 144.0 FPS | 142.6 FPS |
| Gameplay assertions | 136 pass, 0 fail | 140 pass, 2 fail |
| FP assertions | 7 pass, 0 fail | 7 pass, 0 fail |

These are single script observations, not a controlled benchmark. Bot fights differ between runs.
Neither the test pass counts nor still images certify human movement feel or latency behavior.

## Failures requiring review

1. The curb fixture climbs 29.95 cm but requires a >20 cm single-frame capsule change; observed 9.05 cm.
   Its separate smoothing checks pass: eye change 5.22 cm, maximum trail 15.28 cm, settles to zero.
   Review this fixture assumption rather than treating the successful climb as a movement failure.
2. The buffered jump records 0.00 upward velocity. Unreal calls `Landed()` before setting walking mode;
   the subsequent `ACharacter::OnMovementModeChanged` resets jump input. Claude currently calls `Jump()`
   in `Landed()`, so that input is cleared. Rework consumption after the transition and review prediction/replay.

Both failing assertions and the exact report are preserved. No production movement code was changed by Codex.
Coyote time passes (891.95 upward velocity), including expired-window rejection. Existing interrupted-fade,
root-drift and directional-gait assertions pass. Human feel and network movement review remain outstanding.

## Assets and desktop corrections

All 14 armor and five weapon meshes imported with three LODs, matched material slots and outward winding.
See [import report](reports/import.txt) and [winding report](reports/winding.txt).
The new tests needed their private namespace imported. LOD generation needed unattended editor mode,
initialized editor subsystems and current UE 5.8 reduction structs; both art batch entry points now use
`Tools/ImportArt.ps1`. Only the character/weapon import was run. Unrelated palette resaves were restored;
narrow imports reuse those materials. Map content and the pre-existing local config were preserved.

## Captures and reports

- [Baseline FP](baseline/keel_fp_fp_0_idle_br_0.jpg), [after FP](after/keel_fp_fp_0_idle_br_0.jpg).
- [Baseline bot view](baseline/bot_1.jpg), [after bot view](after/bot_1.jpg).
- Each image folder contains idle, weapon swap, grenade, melee, reload and three bot-match stills: 23 each.
- [Baseline gameplay](reports/baseline-selftest.txt), [after gameplay](reports/after-selftest.txt).
- [Baseline FP report](reports/baseline-fp.txt), [after FP report](reports/after-fp.txt).

Armor is dark in both bot runs; team accents are visible, but surface readability needs more work.
The new gun detail is present; the first-person grip and poses still need polish. These are review captures,
not evidence of final AAA visual or animation quality.

JPGs were exported with `Tools/shots_to_jpg.ps1 -SourcePath <preserved run folder> -Out <review folder>`.
The optional source/filter arguments preserve the converter's default behavior and avoid exporting 1,968
older unrelated shots. Original PNGs and failure logs remain in ignored `Saved/CharacterVerification-20261104/`.
