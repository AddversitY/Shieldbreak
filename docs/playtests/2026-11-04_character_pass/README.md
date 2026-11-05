# Desktop character verification — 2026-11-04

## Automatic task (5), `aad1a24`

Final gameplay **144/144 PASS**, FP **7/7 PASS**; build and 19-mesh/three-LOD import pass.
The new matte-suit assertion confirms `MI_SB_Polymer`. Step-camera eye change **5.17 cm**, curb **29.95 cm**,
capsule frame **8.23 cm**, trail **13.20 cm**.

The initial run exposed a real shader integration error: Polymer lacked SkeletalMesh usage and Unreal explicitly
fell back to Default Material. That run was stopped and excluded from results/FPS. The importer now enables,
recompiles and saves the required usage on `M_SB_Surface`. Final gameplay/FP logs have no missing-usage/fallback
warnings. The master usage change is intentional; its graph/parameters and the shared Gunmetal instance are unchanged.

Actual palette values verified against source: ArmorMetal RGB **(0.20,0.21,0.23)** / metallic **0.15** / roughness **0.50**;
WeaponMetal **(0.24,0.245,0.26)** / **0.20** / **0.42**; Polymer **(0.06,0.062,0.068)** / **0.00** / **0.62**.

| Normal observation, same desktop and actual 1280x720 | After4 | After5 |
|---|---:|---:|
| Crucible, four bots | 132.2 FPS (74 s) | 120.6 FPS (76 s) |
| Keel fixed FP, no bots | 145.7 FPS (20.4 s) | 113.4 FPS (20.9 s) |

These lower single observations need follow-up, but are not a controlled benchmark or proof of the cause.

**The normal glove still clips white.** The actual matte material is loaded and no longer shows the old woven
mannequin pattern. With only `sb.FPLight 0`, the same view has a dark matte/cyan-lit glove. This isolates the fill
light as a contributor to clipping; Claude should inspect fill/exposure. No production light value was changed.
The fill-off FP check passes 7/7; its 120.1 FPS is a different lighting condition, excluded from the table above.
Gun body remains very dark overall, with readable rails/edges; mid-grey appearance is partial. The bots' suit is
matte dark between plates. Backlit bot views remain dark, consistent with Claude's accepted task (4) lighting review.

- [Normal FP idle](after5/keel_fp_fp_0_idle_br_0.jpg), [same view, fill off](after5/fill-off/keel_fp_filloff_fp_0_idle_br_0.jpg).
- [Bot view](after5/bot_1.jpg); 23 normal JPGs in `after5/` and one diagnostic comparison.
- [Gameplay](reports/after5-selftest.txt), [normal FP](reports/after5-fp.txt), [fill-off FP](reports/after5-fp-filloff.txt).
- [Import](reports/after5-import.txt), [actual values/usage/bindings](reports/after5-material-verification.json),
  [initial rendering failure](reports/after5-initial-rendering-fallback.txt).

Full initial log and original PNGs retained in `Saved/CharacterVerification-20261104/After5/`.
No C++ gameplay edits, network/latency tests, human movement acceptance or main merge in this task.

## Automatic task (4), `4d1bfa6`

Build/import PASS; gameplay **143/143 PASS**, FP **7/7 PASS**. Actual palette values match the updated source:
ArmorMetal RGB **(0.20,0.21,0.23)**, metallic **0.15**, roughness **0.50**; WeaponMetal RGB
**(0.13,0.135,0.15)**, metallic **0.30**, roughness **0.40**. Bindings and three LODs verified on 19 meshes;
shared map Gunmetal unchanged. Codex added import-report logging of the two palette instances; no production source edits.

The revised step assertion passes: maximum eye change **5.75 cm**, curb **29.93 cm**, capsule frame **6.66 cm**,
trail **12.63 cm**. Claude changed the assertion to compare against curb height; production camera behavior
was not changed in this pass. Passing it does not establish all-frame-rate or human animation acceptance.

| Observation, same desktop and actual 1280x720 | After3 | After4 |
|---|---:|---:|
| Crucible, four bots | 128.5 FPS (76 s) | 132.2 FPS (74 s) |
| Keel fixed FP, no bots | 149.9 FPS (19.5 s) | 145.7 FPS (20.4 s) |

Individual script observations, not a controlled benchmark or demonstrated gain/loss.

- [Gameplay](reports/after4-selftest.txt), [FP](reports/after4-fp.txt), [import with values](reports/after4-import.txt).
- [Actual material verification](reports/after4-material-verification.json).
- [FP idle](after4/keel_fp_fp_0_idle_br_0.jpg), [bot view](after4/bot_1.jpg); 23 JPGs in `after4/`.

**Visual goals remain open:** gloves still appear white; armor is still very dark, with accents more legible
than its grey surfaces. Gun rails/edges show highlights, but much of the body remains near-black; grey/detail
goal is only partial. The scope eyepiece stays dark. Palette values/bindings are correct, so Claude needs
runtime material/lighting investigation rather than assuming those visual goals passed. No network/latency,
human feel or other-map tests in this task. Originals and read-only verification in `Saved/CharacterVerification-20261104/After4/`.

## Task (3): sprint/slide and lighting (`9716ade` + `71047da`)

Build and narrow import pass. Gameplay **142 pass / 1 fail**; FP **7/7 pass**.
One focused 60 FPS capped presentation run passes **42/42**, under a different frame sampling condition.
The original failure is preserved: maximum eye change **5.95 cm**, capsule **7.93 cm**, trail **13.66 cm**,
narrowly missing the `eye < capsule * 0.75` assertion. At 60 FPS these measure **11.65 / 16.45 / 16.45 cm**.
Claude should review both the assertion and camera behavior across frame rates; no threshold was changed.

| Observation, same desktop and actual 1280x720 | After2 | After3 |
|---|---:|---:|
| Crucible, four bots, 76 real seconds | 130.5 FPS | 128.5 FPS |
| Keel fixed FP, no bots | 146.4 FPS (20.3 s) | 149.9 FPS (19.5 s) |
| Sprint sample | 924.03 uu/s | 1020.00 uu/s |
| Slide sample at 0.15 s | 1036.96 uu/s | 1293.60 uu/s |
| Slide sample at 0.45 s | 835.13 uu/s | 1014.16 uu/s |

Individual script observations, not a controlled benchmark. New faster-than-sprint slide check passes.
Slide duration and distance were not separately measured; the tuning's ~19% distance description is not
an actual desktop measurement. No network/latency or human feel acceptance in this task.

The import initially retained old `M_Gunmetal` display slot names despite the FBX containing the new names.
`Tools/import_art.py` now resets character material conflicts and uses the authoritative imported names
before binding the palette; reports include every slot/material path. Final verification loads both new
materials, confirms their bindings and three LODs on all 19 meshes. Map Gunmetal's before/after SHA256 is
identical. Existing map and material assets are untouched; two new character material instances are included.

- [Gameplay](reports/after3-selftest.txt), [FP](reports/after3-fp.txt), [60 FPS diagnostic](reports/after3-presentation60.txt).
- [Import and slot paths](reports/after3-import.txt), [material verification](reports/after3-material-verification.json).
- [FP view](after3/keel_fp_fp_0_idle_br_0.jpg), [bot view](after3/bot_1.jpg); 23 JPGs in `after3/`.

Visual acceptance: the BR eyepiece is dark, resolving the white disc. Gloves remain white and armor remains
near-black in the actual captures, despite correct new palette bindings. These two goals remain open for Claude.
Local originals and stale-slot diagnostics are retained under `Saved/CharacterVerification-20261104/After3/`.

## Follow-up after Claude's `eba0b73`

Build PASS; narrow character reimport PASS (19 meshes, three LODs each, all slots matched).
Gameplay **142/142 PASS**, FP **7/7 PASS**. Both first-pass movement failures are resolved.

| Observation, same desktop and actual 1280x720 | First character pass | Follow-up |
|---|---:|---:|
| Crucible, four bots, 76 real seconds | 125.4 FPS | 130.5 FPS |
| Keel fixed first-person, no bots, about 20 seconds | 142.6 FPS | 146.4 FPS |

These remain individual script observations, not a controlled benchmark.
Buffered jump now reaches 891.60 uu/s; coyote jump 891.67, with expired-window rejection passing.
The curb rises 29.79 cm; maximum eye change 5.46 cm versus capsule 9.24 cm, with 14.26 cm trail.

- [Follow-up gameplay](reports/after2-selftest.txt), [FP report](reports/after2-fp.txt), [import report](reports/after2-import.txt).
- [FP view](after2/keel_fp_fp_0_idle_br_0.jpg), [bot view](after2/bot_1.jpg); all 23 follow-up JPGs are in `after2/`.
- [Manny material parameter defaults](reports/mannequin_material_params.txt): both slots and complete parent chains,
  80 scalar and seven vector names each, instance effective defaults, master defaults and local overrides.
  Reproduce read-only using `Tools/dump_mannequin_materials.py` with the Unreal Python commandlet.

The weapon is visibly lower/further and the central rear-scope glow is gone. The scope cap is still pale,
gloves are still white, and armor remains very dark in these bot shots. Those visual issues need further work.
The parameter dump changes no assets. Its first run finished the report but hit a Python shutdown crash;
function-scoped Unreal wrappers and explicit collection fixed shutdown (final exit 0, no errors, three warnings).
The first failure log and original run artifacts are retained in `Saved/CharacterVerification-20261104/After2/`.
No production source changes by Codex, network rerun or human feel acceptance in this follow-up.

## First-pass evidence (historical)

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
