# Keel redesign: what Midship teaches us

Status: research and proposal only (2026-11-06). Maps come last, so nothing here is built yet. When map work resumes,
Keel is the first map to take to AAA, and this is its brief.

The rule stands: we study how Halo 2's Midship works and why it lasted, then build our own layout and art. No
Halo geometry, layout, assets or textures are copied or traced, and there is no ring in the sky.

## Sources and limits

Read: the Halopedia and Halo Fandom pages for Midship and its remakes (Heretic in Halo 3, Truth in Halo 5), PC
Gamer's history of Halo 2's maps, Prima's Truth guide, a Bungie.org strategy write-up and Kotaku's top maps list.
Overhead images could not be downloaded from the cloud (the image host blocks it), so the layout notes below come
from those texts and general knowledge of the map. Exact sizes, lift counts and spawn timers are not published and
are not claimed here.

## How Midship works

**It is small and dense on purpose.** Bungie planned Halo 2's maps on a small / medium / large grid, each with a
primary gametype. Midship is the small one, built for 2 to 8 players, duels and team Slayer. Every room is a few
seconds from every other room, so fights never stop and a lost fight is never far from the next one.

**It is one loop around a centre.** Players described it as "one huge wavy donut": a sloped path runs round the
outside through side rooms, and a platform crosses the middle at the top. There are no dead ends. Every room has
two or more exits, so you can always rotate, and you can always be flanked.

**One focal point everyone can see.** "Top mid", the platform across the middle (it floated in Halo 2), is seen
from both bases and both ramps. Whoever holds it sees the whole map, and is also seen by the whole map. That is
the core trade: height gives information and angles, exposure takes them back.

**Bases look into the middle.** Each team's base sits at one end, raised, with openings over the centre. The best
players jump straight from the base onto top mid instead of taking the ramp. Halo 5's remake keeps that jump and
adds a power tower that covers top mid and both bases.

**Near-symmetric, with identity.** Halopedia calls it symmetrical but "not exactly": the odd parts (the carbine
side and the pink side) are the same distance from both bases, so it stays fair while each side looks and feels
different. That is also what makes callouts work: "Carbine", "Top mid", "Pink", "Planet side", "Moon side" each
name a place you can picture.

**Power is placed to start fights.** The energy sword was the map's last power weapon and sits where the close
fights happen. Grenades and plasma pistols are early pickups near the bases. In the remakes the sword spawns where
both teams see it, and Halo 5's top mid has barriers that block fire, break, and come back.

**A high skill ceiling hidden in the geometry.** Climbable beams on the centre structure, ledges to jump to,
quiet spots to ambush from. A new player walks the ramps; an expert never does. This is the "easy to pick up,
impossible to master" idea in map form.

**Spectacle outside, clarity inside.** It is the inside of an alien warship: curved purple metal, glowing light
strips, and windows onto a planet and moon. The detail is in the shapes (every room has a different silhouette)
and the view out, not in clutter on the floor, so play spaces stay readable.

## Where Keel stands today

Keel (`Design/maps/shieldbreak_maps.py`, `keel()`) already takes Midship as its reference: an enclosed warship,
two hangars facing a reactor hall, rockets on the core at 6.5 m with the Overshield underneath, a cargo hold flank
and an engine deck flank. Measured: 136 x 88 m, flag-to-flag routes of 14.2 / 15.7 / 17.1 s sprint, 217 standing
lines over 60 m before the hull ribs cut them to 135.

What it is missing, judged against the principles above:

- **Too big and too long for this style.** 136 m end to end makes it a medium map; long walks between fights.
- **Corridors, not a loop.** The flanks are separated lanes behind full-height walls; you choose a lane and commit,
  instead of circling a centre you can always see.
- **The centre is not a stage.** The core is a block reached by bridges and lifts; it is not the dramatic,
  visible-from-everywhere place top mid is.
- **No signature skill routes.** No base-to-centre jump, no climbable beams, no hidden ledges.
- **No identity per area.** Grey boxes with orange ramps; nothing to name or picture, and no view outside.

## Proposal: Keel, rebuilt

Premise kept: a derelict warship of our own faction (gunmetal hull, hazard orange, amber lamps). The centre and
the layout are new.

### Shape

A compact loop, roughly **104 x 72 m** (to confirm in blockout), point-symmetric like now (red at -X, blue at +X),
three play heights: **trench 0 m, decks 3.5 m, high 7 m**, with a tall vault (about 18 m) over the middle so the
centre feels huge and the rest stays tight. Target flag-to-flag about 11 to 13 s sprint, checked with
`Design/maps/analyze.py`, and every room with at least two exits.

### The centre: the Cradle

A drydock trench runs across the middle of the ship. Hanging over it on two crane arms is a **cargo pod** at
7 m. This is our focal point, not a floating platform: a hanging pod, crane arms and a trench below.

- **Pod top (7 m):** the Rockets. Seen from both team windows, both crane arms and both flanks. Low cover only.
- **Trench (0 m):** the Overshield, under the pod. Safe from above only while you stay under the pod.
- **Crane arms:** narrow beams from each side up to the pod. Walkable for anyone; the expert route.

### The bases: the Bridges

Each team has a command bridge at its end, at 3.5 m, with a long armoured **window ledge** looking into the Cradle.
Exits: a ramp down into the trench, a side door to each flank, and a gravity lift up from the back to a 7 m
overwatch. From the window ledge a crouch-jump or Power Jump reaches the crane arm, and from there the pod. The
flag sits on the bridge, defended from inside, out of the centre's direct line.

### The flanks: one long, one close

Equidistant from both bases (point mirror), so each team has one of each on its side of the map:

- **Breach (north):** a torn hull section sealed by a force field, open to space: a gas giant and a debris field
  outside, no ring. A curved catwalk at 7 m with the longest lines on the map (about 40 m, never 60). The Rail
  Lance is here.
- **Galleries (south):** engine galleries at 0 and 3.5 m, pipes, pistons, tight corners, a slide gap under a pipe
  run. Close range only. Camo is here.

Both flanks join the trench at the Cradle and the bridges at their ends, which closes the loop.

### Skill ceiling built in

- Window ledge to crane arm to pod (crouch-jump, then a beam walk).
- Climbable crane beams onto the pod from the trench.
- Slide gap in the Galleries that skips a corner.
- Power Jump from the Breach catwalk onto the far crane arm.
- Each one is visible and learnable, never a hidden exploit, and each has a counter angle.

### Callouts

Red Bridge, Blue Bridge, Window, Cradle, Pod, Trench, Crane One, Crane Two, Breach, Galleries, Pipes, Overwatch.
Every one gets its own silhouette and light colour so a new player learns them in one match.

### Art direction

Original shapes, scanned and modelled detail, performance first:

- Ribbed gunmetal hull with heavy rounded frames (our faction's shape language: chunky, industrial, angled, not
  Covenant curves and not Forerunner panels).
- Contrast lighting: warm amber work lamps against cold blue light from the Breach and the space outside.
- The Cradle is the hero set: crane arms, cables, the pod with hazard stripes, steam from the trench, volumetric
  shafts from the vault.
- Spectacle through the windows and the Breach (planet, debris, distant ships), which costs little because it is
  far away and simple.
- Modular kit: shared materials, instanced ribs and pipes, LODs, measured FPS before and after every step and no
  regression against the current Keel.

## Build order when maps resume

1. Blockout in `shieldbreak_maps.py` (shape, heights, items, routes); run `--check` and `analyze.py`.
2. Desktop playtest with bots and an FPS run; adjust sizes and lines.
3. Hero set first (the Cradle), then bridges, flanks, view out.
4. Lighting and sound pass, FPS check, screenshots for Vincent.
