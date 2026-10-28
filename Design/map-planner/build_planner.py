#!/usr/bin/env python3
"""Export the authored Shieldbreak geometry into an offline overhead scale planner.

This exporter reads and validates map authoring data. It never regenerates the
runtime C++ maps or the PNG previews produced by shieldbreak_maps.py.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import runpy
import sys
from typing import Any, Sequence


HERE = Path(__file__).resolve().parent
MAP_SOURCE = HERE.parent / "maps" / "shieldbreak_maps.py"
TEMPLATE = HERE / "planner.template.html"
FRAGMENT_OUTPUT = HERE / "scale-planner.fragment.html"
STANDALONE_OUTPUT = HERE / "index.html"
PLACEHOLDER = "__MAP_DATA_JSON__"


def metres(value: float) -> float:
    """Authoring centimetres become deterministic metre coordinates, to 3 dp."""
    result = round(float(value) / 100.0, 3)
    return 0.0 if result == 0.0 else result


def point(values: Sequence[float]) -> list[float]:
    if len(values) != 3:
        raise ValueError("Expected an XYZ point in the map authoring data.")
    return [metres(value) for value in values]


def load_scenes() -> dict[str, dict[str, Any]]:
    """Validate every authored map before exporting the five launch maps."""
    authored = runpy.run_path(str(MAP_SOURCE))
    maps = authored["MAPS"]
    validate = authored["validate"]
    problems = []
    for level in maps:
        problems.extend(f"{level.id}: {problem}" for problem in validate(level))
    if problems:
        raise ValueError("Map validation failed; nothing written:\n  " + "\n  ".join(problems))

    scenes = {}
    for level in maps:
        if level.id == "Crucible":
            continue
        if level.id in scenes:
            raise ValueError(f"Duplicate authored map ID: {level.id}")
        scenes[level.id] = {
            "id": level.id,
            "span": [metres(2 * level.half_x), metres(2 * level.half_y)],
            "floor": metres(level.floor_z),
            "blocks": [
                {"a": point(a), "b": point(b), "kind": kind}
                for a, b, kind in level.blocks
            ],
            "ramps": [
                {"a": point(a), "b": point(b), "width": metres(width), "kind": kind}
                for a, b, width, kind, _thickness in level.ramps
            ],
            "lifts": [{"a": point(a), "b": point(b)} for a, b in level.lifts],
            "spawns": [point(position) for position in level.spawns],
            "weapons": [
                {"kind": kind, "p": point(position)}
                for kind, position, _respawn in level.weapons
            ],
            "powerups": [
                {"kind": kind, "p": point(position)}
                for kind, position, _respawn in level.powerups
            ],
            "flags": [point(position) for position in level.flags],
            "hills": [point(position) for position in level.hills],
            "ball": point(level.ball),
        }
    if not scenes:
        raise ValueError("The authoring source contains no launch maps.")
    return scenes


def embedded_json(scenes: dict[str, dict[str, Any]]) -> str:
    """Keep JSON valid inside an inline script even if authored labels change."""
    payload = json.dumps(scenes, ensure_ascii=True, allow_nan=False, separators=(",", ":"))
    return payload.replace("<", "\\u003c").replace(">", "\\u003e").replace("&", "\\u0026")


def strip_host_send_blocks(fragment: str) -> str:
    """Remove explicitly marked host-only message sending from offline output."""
    result = []
    in_host_block = False
    for line in fragment.splitlines(keepends=True):
        marker = line.strip()
        if re.fullmatch(r"//\s*HOST_SEND_BEGIN", marker):
            if in_host_block:
                raise ValueError("Nested HOST_SEND blocks are not supported.")
            in_host_block = True
        elif re.fullmatch(r"//\s*HOST_SEND_END", marker):
            if not in_host_block:
                raise ValueError("HOST_SEND_END has no matching HOST_SEND_BEGIN.")
            in_host_block = False
        elif not in_host_block:
            result.append(line)
    if in_host_block:
        raise ValueError("HOST_SEND_BEGIN has no matching HOST_SEND_END.")
    return "".join(result)


OFFLINE_STYLE = """
:root {
  color-scheme: light;
  --background: #f7f8fa; --foreground: #17212d;
  --muted: #e9edf2; --muted-foreground: #526175; --border: #cbd3de;
  --primary: #1769aa; --primary-foreground: #ffffff; --input: #ffffff;
  --accent: #e0eef9; --accent-foreground: #123f61; --destructive: #b42332;
  --red: #bb3b4b; --blue: #286fbe; --orange: #b86219; --green: #26835c;
  --viz-series-1: #2876a8; --viz-series-2: #a86827; --viz-series-3: #6f60a3;
  --viz-series-4: #328368; --viz-series-5: #a84d66; --viz-series-6: #55728a;
}
@media (prefers-color-scheme: dark) {
  :root {
    color-scheme: dark;
    --background: #111923; --foreground: #edf2f8;
    --muted: #202d3d; --muted-foreground: #adbacb; --border: #3b4a60;
    --primary: #79b8eb; --primary-foreground: #102131; --input: #192534;
    --accent: #263e54; --accent-foreground: #dbedfc; --destructive: #ff8795;
    --red: #ef8190; --blue: #79b9f4; --orange: #eab378; --green: #79c9a2;
    --viz-series-1: #79b9df; --viz-series-2: #d4ad78; --viz-series-3: #baabeb;
    --viz-series-4: #86c7af; --viz-series-5: #dd94ac; --viz-series-6: #a1bbd0;
  }
}
* { box-sizing: border-box; }
body {
  margin: 0; padding: clamp(12px, 3vw, 32px);
  background: var(--background); color: var(--foreground);
  font: 15px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif;
}
.planner-page { max-width: 1440px; margin: 0 auto; }
button, input, select, textarea { font: inherit; }
button, select, input[type="checkbox"] { cursor: pointer; }
button:disabled { cursor: default; opacity: .55; }
.btn {
  display: inline-flex; align-items: center; justify-content: center; gap: 6px;
  padding: 8px 12px; border: 1px solid var(--border); border-radius: 8px;
  background: var(--muted); color: var(--foreground); text-decoration: none;
}
.btn:hover:not(:disabled) { background: var(--accent); color: var(--accent-foreground); }
.btn-primary { border-color: var(--primary); background: var(--primary); color: var(--primary-foreground); }
.form-control, .form-select {
  display: block; width: 100%; min-width: 0; padding: 8px 10px;
  border: 1px solid var(--border); border-radius: 7px;
  background: var(--input); color: var(--foreground);
}
.form-range { width: 100%; margin: 8px 0; accent-color: var(--primary); }
.form-label { display: block; margin-bottom: 5px; font-weight: 500; }
.form-check { display: flex; align-items: center; gap: 7px; }
.form-check-input { width: 1rem; height: 1rem; margin: 0; accent-color: var(--primary); }
.form-check-label { cursor: pointer; }
.viz-row { display: flex; align-items: center; flex-wrap: wrap; gap: 10px; }
.viz-controls { display: flex; align-items: end; flex-wrap: wrap; gap: 12px; }
.viz-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 220px), 1fr)); gap: 12px; }
.btn[aria-pressed="true"] { background: var(--primary); color: var(--primary-foreground); border-color: var(--primary); }
.text-small { font-size: .875rem; }
.text-muted { color: var(--muted-foreground); }
.sr-only {
  position: absolute; width: 1px; height: 1px; padding: 0; margin: -1px;
  overflow: hidden; clip: rect(0, 0, 0, 0); white-space: nowrap; border: 0;
}
:focus-visible { outline: 2px solid var(--primary); outline-offset: 3px; }
""".strip()


OFFLINE_STATE_SHIM = """
<script>
// Offline fallback: state is local to this page; no host messaging is available.
(function () {
  const api = window.openai || (window.openai = {});
  const stateKey = "shieldbreak-map-scale-v1";
  if (typeof api.widgetState === "undefined") {
    try { api.widgetState = JSON.parse(localStorage.getItem(stateKey) || "{}"); }
    catch { api.widgetState = {}; }
  }
  if (typeof api.setWidgetState !== "function") {
    api.setWidgetState = function (state) {
      api.widgetState = state;
      try { localStorage.setItem(stateKey, JSON.stringify(state)); } catch {}
      return Promise.resolve();
    };
  }
})();
</script>
""".strip()


def standalone_document(fragment: str) -> str:
    offline_fragment = strip_host_send_blocks(fragment).replace(
        ">Send map direction</button>", ">Show direction brief</button>"
    )
    return (
        "<!doctype html>\n<html lang=\"en\">\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "<title>Shieldbreak map scale planner</title>\n"
        f"<style>\n{OFFLINE_STYLE}\n</style>\n</head>\n<body>\n"
        f"{OFFLINE_STATE_SHIM}\n<main class=\"planner-page\">\n"
        f"{offline_fragment}</main>\n</body>\n</html>\n"
    )


def make_outputs(inline_output: Path | None = None) -> dict[Path, str]:
    # Source validation happens before reading the template or writing any output.
    scenes = load_scenes()
    template = TEMPLATE.read_text(encoding="utf-8")
    if template.count(PLACEHOLDER) != 1:
        raise ValueError(f"{TEMPLATE.name} must contain exactly one {PLACEHOLDER} placeholder.")
    fragment = template.replace(PLACEHOLDER, embedded_json(scenes)).rstrip() + "\n"
    outputs = {FRAGMENT_OUTPUT: fragment, STANDALONE_OUTPUT: standalone_document(fragment)}
    if inline_output is not None:
        target = inline_output.expanduser().resolve()
        if target.suffix.lower() != ".html":
            raise ValueError("--inline-output must name an .html file.")
        if target in (TEMPLATE, STANDALONE_OUTPUT):
            raise ValueError("--inline-output cannot overwrite the template or standalone index.")
        outputs[target] = fragment
    return outputs


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Validate maps and fail if generated output is missing or stale; write nothing.")
    parser.add_argument("--inline-output", type=Path, metavar="PATH", help="Also write (or check) a host fragment at an explicit .html path.")
    args = parser.parse_args(argv)
    try:
        outputs = make_outputs(args.inline_output)
        if args.check:
            stale = [path for path, expected in outputs.items()
                     if not path.is_file() or path.read_text(encoding="utf-8") != expected]
            if stale:
                for path in stale:
                    print(f"missing or stale: {path}", file=sys.stderr)
                print("Regenerate with build_planner.py before committing the preview.", file=sys.stderr)
                return 1
            print(f"Planner outputs are current ({len(outputs)} files); authored maps validate.")
            return 0
        for path, content in outputs.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open("w", encoding="utf-8", newline="\n") as output:
                output.write(content)
            print(f"wrote {path}")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Planner export failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
