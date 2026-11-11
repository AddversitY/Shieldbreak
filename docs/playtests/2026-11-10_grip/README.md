# Hand / weapon contact correction — 2026-11-10

Desktop verification on `handoff-2026-11-10`, starting at `9c72567`.

## Scope

- Shared first-/third-person grip placement keeps the knuckles outside the receiver; camera calibration compensates for the complete fitting offset.
- The support palm fits the real underside and the entire wrist-to-knuckle footprint, then moves back along the forend when needed to stay within arm reach. The fit is cached in gun space (20 Hz owner / 10 Hz body), so it follows the gun every rendered frame without repeating all geometry queries.
- The trigger finger uses a forward-bending two-link solution aimed at the blade's front contact, rather than folding backwards through the grip to reach a socket behind the blade. Third-person hands use it too.
- Finger probes follow the distal bone's rotation. Contact curls start open, predict successive joint changes, and search both bend directions only for actual centre-line crossings; a skin-radius touch is a resting contact. Escape candidates use triangle rays; normal closing predictions retain skin-radius sweeps.
- The reload regression now reports an untestable receiver as a failure instead of discarding it. No intersection assertion or threshold was relaxed.
- The grenade-light fixture ignores explosions created before its throw. A lingering explosion from the preceding damage test could finish the observation before the new grenade blinked. Gameplay grenade timing and the `> 0.5` blink threshold are unchanged.

No mesh, texture, material or imported asset was modified.

## Evidence

Baseline: editor binary at `9c72567`, **170 passed / 1 failed**, with the hand-contact failure. Fresh rendered-offscreen observations at 1280x720: **172.8 FPS** in the four-bot Crucible fixture and **153.0 FPS** in the 20-second fixed first-person Keel fixture.

The focused candidate passed **3/3** (filter selection, hand contact, camera check), covering all five first-person weapons, two BR reload samples and the third-person BR. An intermediate full run passed hand contact but failed the grenade-light fixture and averaged **79.2 FPS**. It is retained in `reports/intermediate-gameplay.txt`. A subsequent cached-fit run passed **171/171** at **82.6 FPS**; its fixed-view run passed **9/9** at **86.0 FPS**. Both are retained as `intermediate-cached-*`.

**Performance comparison is inconclusive.** World of Warcraft started at 23:07 local, after the baseline gameplay report (22:42), and was running during final verification. A three-second sample recorded 5.33 CPU-seconds in WoW alongside 8.30 in Unreal (`reports/run-load.txt`). The runs share map, resolution and bot count but differ in concurrent load. Do not attribute the entire FPS difference to this fix or claim that the cache/ray changes recover baseline performance. An idle-desktop before/after recheck remains outstanding; the user's game was left running.

Final verification: editor build **PASS**, full gameplay **171/171**, FP capture suite **9/9**. Raw final FPS: **97.0** four bots, **104.3** fixed first person, subject to the concurrent-load limitation above. `reports/after-gameplay.txt`, `after-fp.txt` and `build.log` are the source evidence. Individual FPS observations are not a statistical benchmark or a claim about packaged performance.

## Reproduce

Build with `Tools\BuildEditor.bat`. For the focused regression, launch the installed editor with the project path and:

```text
-game -windowed -ResX=1280 -ResY=720 -RenderOffscreen -unattended -SBSelfTest -SBNoHelp -SBTestFilter="Hands never pass through guns"
```

The filtered report is `Saved/Logs/SelfTest-Filtered.txt`. Omit `-SBTestFilter` for the complete mechanics suite (`SelfTest.txt`). `-SBGripDebug` adds exact crossing segments, hit coordinates and triangle indices to the game log.

The matching first-person/FPS capture uses `/Engine/Maps/Entry?Map=Keel?Mode=Slayer` and `-SBSelfTest -SBFPShots -SBNoHelp` with the same resolution and render flags. The normal network suites were not rerun: this pass changes cosmetic grip fitting and test isolation, not roster, session capacity or match commands.

## Visual review and limits

`before-after.jpg` and the individual JPEGs preserve the game captures (only resized/compressed for review). Gun framing is retained; the glove placement changes to clear the receiver and magazine. In the sidearm idle capture, the glove is now mostly below the frame; third-person support sits farther back near the magazine. Those holds still need visual polish. This verifies the listed poses, not every frame of every possible action. Cinematic hand animation, gun texture defects, map art, matchmaking, internet hosting and eight-human testing remain outside this pass.
