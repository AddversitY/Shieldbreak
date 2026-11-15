# Reload hand / magazine correction — 2026-11-11

Starting revision `5548bb6`, branch `handoff-2026-11-10`. Vincent reviewed the prior reload video and authorized this follow-up.

## What changed

The carry was reading the previous evaluated hand while the gun and hand target advanced to a new pose. Hand anchors now refresh after applying the current gun pose. A bone-finalization callback updates the attached gun first, then carries the magazine from the current palm. The grasp captures the actual socket-to-palm transform, and the last portion of insertion blends into the socket before seating.

The Battle Rifle's pull/return keys now stay near the magazine well instead of sending the hand to camera-space Z -40/-42 cm. The magazine clears the well before moving to the viewer's left, then aligns for insertion. A smaller cant and a forward/left/up reload offset make more of the magazine work visible. Reload duration, ammunition, network behavior and imported assets are unchanged. The shared carry timing also applies to the sidearm and sniper; their authored paths were retained.

## Evidence

- `before-reload.mp4`: the previously delivered clip on `5548bb6`.
- `after-reload.mp4`: the corrected Battle Rifle reload, 100 rendered frames at 30 fps, 1280x720, 3.33 seconds, no audio. Original game frames; no generated animation or interpolated frames.
- `before-after.jpg`: matching extraction, carry and insertion poses, resized for comparison.
- `reports/focused-30fps.txt`: **11 passed / 0 failed**. New checks observe palm and magazine visibility during carry and grip drift across evaluated frames. Palm **612,679**, magazine centre **586,553** in 1280x720; maximum carry drift **0.000 cm**. Existing magazine-out, final seat, sidearm and camera checks pass.
- `reports/gameplay.txt`: final build **173 passed / 0 failed**. The reload and hand-clipping checks pass. Camera-step also passes in this run (**9.23 cm** eye change, **30.00 cm** curb); the failures below remain evidence of variability. Four-bot Crucible observation: **97.3 FPS**, with no new matched before-run for that fixture.
- `reports/intermediate-gameplay.txt`: **172 passed / 1 failed**, before the final BR pull-arc refinement. Reload checks and the five-weapon / BR reload / TP BR clipping regression pass. The documented intermittent **eye-trails-step** check fails: largest eye change **12.66 cm**, curb **29.99 cm**, capsule frame **13.12 cm**, trail **18.98 cm**. Its limit remains 40% of curb height; no assertion was loosened. The isolated recheck also fails (**4 passed / 1 failed**, eye **17.36 cm**, curb **29.93 cm**); this remains unresolved.
- `reports/other-weapons-fp.txt`: **9 passed / 0 failed** with the final shared carry implementation (before the last BR-only key refinement). Sidearm/sniper reload pose captures are in `other-weapons.jpg`; these are visual regression samples, not proof of complete animation quality.

Builds pass. The animation build retains the pre-existing `RemoteViewPitch` deprecation / character death-camera warning. An uninitialized diagnostic screen-coordinate warning introduced during the test edit was corrected before final validation. See `build-animation.log` and `build-final.log`.

Matching fixed-view Keel, 1280x720 rendered offscreen, no bots: **91.5 FPS before / 93.7 after**, with **2/2** filtered checks in both runs. Intermediate matched observation **96.4**; broader FP-capture run **89.1**. Background desktop load was not controlled. These compare this reload pass only and do not resolve the prior grip fix's performance comparison with `9c72567`. Measurements are individual desktop observations, not a statistical benchmark.

## Reproduction

Build `ShieldbreakEditor Win64 Development` with the installed UE 5.8 and `-MaxParallelActions=2`. Run expensive jobs sequentially. For the focused animation fixture, launch the project with:

```text
-game -windowed -ResX=1280 -ResY=720 -RenderOffscreen -unattended -SBSelfTest -SBNoHelp -SBTestFilter="First-person moves"
```

Add `-DumpMovie -UseFixedTimeStep -FPS=30` to save actual rendered frames, then encode the reload range to H.264 MP4 at 30 fps. Omit the filter and movie flags for the full gameplay suite. The matching FPS fixture uses `/Engine/Maps/Entry?Map=Keel?Mode=Slayer -SBFPShots -SBTestFilter="First-person fixed-view FPS"` with the same resolution and render flags.

## Limits

The palm centre and magazine are in view; this does not promise every part of the glove stays on screen. Visual evidence here covers the Battle Rifle at the shown camera pose. Broader animation polish, the prior sidearm idle framing / texture issues and the intermittent camera-step check remain separate work. No network script, internet hosting or eight-human test was run for this cosmetic pass.
