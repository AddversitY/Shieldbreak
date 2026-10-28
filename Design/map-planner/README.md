# Shieldbreak overhead scale planner

## Draw your own layout

Open `index.html` in a browser. Choose a map, set its length and width, and
select a drawing tool: route, structure, cover, gap, lift, base or label.
Hide reference geometry for a blank design. Move existing marks by dragging;
the mark list also supports deletion and precise one-metre arrow moves.
Sketches resize with the footprint and save in this browser. Up to 40 marks
are shared across all five maps. Use **Show direction brief** to copy a plan
back into the development chat, including coordinates and notes.

A photographed paper sketch also works: mark bases/routes and one known
distance, or provide the desired overall length and width. The development
pass translates those decisions into playable geometry; this planner edits
the design only.

This is a design preview of the five launch maps, exported from the actual blocks,
ramps, lifts, spawns, pickups and objectives in
`Design/maps/shieldbreak_maps.py`. Crucible remains the test arena and is excluded
from the planner. All authored maps, including Crucible, must validate before the
exporter writes anything.

Scale choices in the planner **do not change the playable game**. They do not
rewrite map authoring data, generate runtime C++, import assets, or verify that
an enlarged layout plays well. Art direction should follow the chosen space and
reworked routes rather than lock the new maps to the old footprints.

## Area versus linear scale

Doubling both map dimensions gives **four times the area**, not twice the area.
For example, a 72 × 44 m footprint is 3,168 m²; at twice both dimensions it becomes
144 × 88 m and 12,672 m².

- **2× area** means about **1.414×** each horizontal dimension.
- **3× area** means about **1.732×** each horizontal dimension.
- **4× area** means **2×** each horizontal dimension.
- **2–4× each horizontal dimension** means **4–16× area**.

The planner makes the chosen interpretation explicit. A geometric overhead
preview is a starting point for design: longer travel times, sightlines, spawn
safety, cover spacing, objective routes and vertical access need a separate
playable-map pass. In particular, spreading horizontal ramp runs while retaining
their heights changes ramp angles and jump relationships. Compare route flow
before committing architecture or art assets to the enlarged space.

## Generate and check

Python 3.9 or newer is sufficient; the exporter uses only the standard library.
Run from the repository root:

```powershell
python Design/map-planner/build_planner.py
python Design/map-planner/build_planner.py --check
```

The first command reads `planner.template.html`, injects compact metre-based map
data, and writes these files beside the exporter:

- `scale-planner.fragment.html`: host-styled inline visualization fragment.
- `index.html`: standalone offline page with light/dark utility styles and local
  state fallbacks. Open this file directly in a browser; no server or downloads
  are required.

The check command writes nothing and returns a nonzero exit code if maps fail
validation, a generated file is missing, or an output is stale. After authoring
data or template changes, regenerate and rerun the check. This does not run the
map source's C++/PNG generation entry point.

To also export the inline fragment to an explicitly chosen HTML path:

```powershell
python Design/map-planner/build_planner.py --inline-output C:/path/to/preview.html
python Design/map-planner/build_planner.py --check --inline-output C:/path/to/preview.html
```

`--check --inline-output` checks that optional copy too. The copy must have an
`.html` suffix and cannot overwrite the source template or the standalone index.

## Data and template contract

`__MAP_DATA_JSON__` must occur exactly once in the template. The JSON is an object
keyed by map ID. Every position and distance is converted from Unreal centimetres
to **metres**, rounded to three decimal places:

```text
{
  Keel: {
    id, span: [X length, Y width], floor,
    blocks: [{a: [x,y,z], b: [x,y,z], kind}],
    ramps: [{a: [x,y,z], b: [x,y,z], width, kind}],
    lifts: [{a: [x,y,z], b: [x,y,z]}],
    spawns: [[x,y,z]],
    weapons: [{kind, p: [x,y,z]}],
    powerups: [{kind, p: [x,y,z]}],
    flags: [[x,y,z]], hills: [[x,y,z]], ball: [x,y,z]
  }, ...
}
```

Coordinates retain the game's Z-up convention and authored red-side/blue-side
placement. The template owns how the designer's selected scale is presented;
the exported baseline geometry remains unchanged.

The host fragment uses inherited theme variables and the utility classes listed
in the exporter's offline style sheet. Host-only message-sending code must sit
between `// HOST_SEND_BEGIN` and `// HOST_SEND_END` lines. Those blocks remain in
the inline fragment and are removed from `index.html`; the template's offline
fallback can still show or copy a design brief. Standalone output provides
guarded `window.openai.widgetState` and `window.openai.setWidgetState` fallbacks and makes no
host-message calls. No gameplay or build validation is implied by planner export.
