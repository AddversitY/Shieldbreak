# Rifle reload: grip and timing pass — 2026-11-11

Starting revision `ee4c366`, branch `handoff-2026-11-10`. Vincent rejected the visual quality of that reload and requested the realism of [this reference](https://www.youtube.com/watch?v=XI90HrSe3pc). These are new authored poses and timing; visual acceptance remains Vincent's decision.

## Changes

- An authored wrist frame turns the hand from the forend hold into a side grip, with the thumb above the magazine. Fifteen joint curls change through reach, grasp, release, fresh pickup, insertion and recovery. Contact corrections fit the fingers against the magazine and receiver; the thumb stays more open near the well.
- The rifle sequence now strips and releases the old magazine, reaches below the frame and brings up a fresh one. Magazine events are consumed once, so the old drop cannot replay after the fresh pickup.
- The gun has a short reaction to extraction and a firm seating impulse, with fewer floating oscillations. The former cross-receiver finishing sweep is replaced by release and a return to the forend. The refreshed wrist/hand target is retained for cancellation blending.
- Gameplay reload duration and ammunition rules are unchanged. Imported assets and the existing audio bank were not changed. The detailed hand shapes apply to the rifle-style move; other styles retain their authored paths.

## Review media

`before.mp4` is the rejected `ee4c366` clip. `after.mp4` is 100 actual rendered frames at 1280x720 / 30 fps (3.33 seconds), without audio. `before-after.jpg` compares corresponding elapsed frames; the action changes in the middle because the new sequence exchanges the magazine. `grip.jpg` shows the new insertion grip at full resolution. Captures are from the game, with no generated or interpolated animation frames.

The hand briefly leaves the frame for the fresh pickup. This is an intentional exchange phase; the new visibility check samples the fresh carry at about 1.08 seconds instead of the old continuous carry at 0.9 seconds. It retains the 97%-of-viewport boundary. Drift is measured on the new grasp before seating, not across the intentional release/exchange.

## Validation

Builds pass. Fixed-30-fps focused checks: **15 passed / 0 failed**, including release then fresh pickup, an evaluated wrist turn of **65.4 degrees**, middle-joint bend **50.4 degrees**, visible palm / magazine, seated magazine and recovery. Maximum new-grasp drift: **0.000 cm**. The separate receiver-clipping fixture passed **3/3** on the receiver-aware candidate before the final fresh-grasp correction; the final full suite repeated and passed clipping afterward.

Final full suite: **176 passed / 1 failed**. The only failure is the known camera-step fixture: eye **5.35 cm**, curb **29.99 cm**, capsule/trail **8.80 cm**. The trail is below its unchanged `>10 cm` requirement. Cross-weapon FP captures: **9/9**.

Matching configured fixed-view Keel fixture, 1280x720 rendered offscreen, no bots: **108.8 FPS before / 174.3 after**. Broader FP capture **177.3 FPS**, full four-bot Crucible **165.8** (after only). Background desktop load was uncontrolled; no optimization or packaged-performance claim follows from the increase.

An earlier incomplete run caught a thumb-tip crossing the receiver at insertion and a release fixture observing old state after a screenshot hitch. The thumb pose and contact geometry were corrected; release is now observed as an actual falling state across frames, and absence of that observation fails. Both earlier failures are retained in `reports/intermediate-failures.txt`. No receiver-clipping threshold was relaxed.

Visual review also rejected an intermediate pickup that reused the old grasp and left the fresh magazine above the fingers, despite low measured drift. The final pickup uses the authored fresh grasp. The existing camera-step check remains intermittent and is separate from this animation work. Its failures are retained in the full report. New checks inspect evaluated bones and state, but they do not certify AAA animation quality or complete finger-surface contact in every frame. The silent clip also does not verify audio timing; the older audio bank still needs timing review against this sequence.

## Reproduce

Build `ShieldbreakEditor Win64 Development` using UE 5.8 and `-MaxParallelActions=2`. Run expensive jobs sequentially. Launch with:

```text
-game -windowed -ResX=1280 -ResY=720 -RenderOffscreen -unattended -SBSelfTest -SBNoHelp -SBTestFilter="First-person moves"
```

Add `-DumpMovie -UseFixedTimeStep -FPS=30` for frame capture. Use filter `Hands never pass through guns` for the separate clipping fixture; omit the filter for the full suite. The matched FPS fixture uses `/Engine/Maps/Entry?Map=Keel?Mode=Slayer -SBFPShots -SBTestFilter="First-person fixed-view FPS"` with the same resolution and render flags.

No network script, internet hosting, online matchmaking or eight-human testing was performed in this cosmetic pass. Broader weapon animation, material/texture polish and audio timing remain outside the validated scope.
