# Map scale direction

## Implemented size pass (2026-10-27)

The user repeated that the playable maps must be enlarged. The first runtime
pass now doubles both horizontal dimensions: **4× original ground area**. The
table below retains original footprints and comparisons for design history.
The planner now reads the enlarged runtime baseline. Crucible is unchanged.
Heights and movement remain fixed; future sketch-led route revisions are still
open. See `MAPS.md` for current dimensions and `COORDINATION.md` for actual tests.

## Earlier size discussion

On 2026-10-27 the user said all five launch maps feel too small and should be
**2–4 times larger**. They want overhead layouts and room to direct the size
and creative direction. This document records candidate footprints and design
guidance; **the alternative candidates below were initially discussion proposals**.
The overhead planner is a conceptual preview, not a playable build or a promise
that every enlarged route will work without further design.

Do not change runtime geometry, collisions, item locations, spawns or navigation
from this document alone. Resolve scale before detailed environment art.
Shieldbreak remains **4v4**, with the current movement and jump mechanics.

The user's creative answers are now recorded: use **a mix of settings, with a
different mood for each map**, draw from the **whole supplied reference collage**,
and lead the art pass with **Spire**. Individual material palettes, environment
themes and size targets below remain proposals for discussion.

## What “larger” means

For a current horizontal footprint `L × W`:

- **2× area:** multiply both horizontal dimensions by exactly `sqrt(2)`
  (approximately `1.4142135623730951`).
- **4× area:** multiply both horizontal dimensions by exactly `2`.
- **2× length and 2× width** already means **4× area**.
- **4× length and 4× width** would mean **16× area**, a substantially different
  interpretation of the user's request that needs an explicit choice.

These footprint factors describe the outer size, not an instruction to scale
every mesh, gap, ramp or pickup radius. Height is a separate design variable;
the horizontal examples below retain existing Z values as their starting point.
The planner can also explore different length and width factors independently.

## Candidate footprints

Dimensions are **X length × Y width in metres**, excluding the thickness of
perimeter walls. Current sizes come from the authored half-extents in
`Design/maps/shieldbreak_maps.py`; displayed candidates are rounded to 0.1 m.

| Map | Current | 2× area, each axis ×√2 | 4× area, each axis ×2 |
|---|---:|---:|---:|
| Keel | 72 × 44 | 101.8 × 62.2 | 144 × 88 |
| Bastion | 60 × 60 | 84.9 × 84.9 | 120 × 120 |
| Sanctum | 76 × 52 | 107.5 × 73.5 | 152 × 104 |
| Creekbed | 56 × 36 | 79.2 × 50.9 | 112 × 72 |
| Spire | 84 × 32 | 118.8 × 45.3 | 168 × 64 |

Crucible remains the unchanged test arena. Its established self-test positions
are not part of this launch-map expansion proposal.

## Expand space without enlarging the player

Redistribute combat spaces and the routes between them. Keep a useful distinction
between a room where a fight happens and a connector where a player rotates.
Larger footprints should provide more options and anticipation, not simply longer
empty walks or additional isolated corners for eight players to search.

Keep local interactions at human scale: doors, corridor clearances, waist-high
cover, steps, ramp widths, jump ledges and weapon pads. Preserve their dimensions
until a specific playtest calls for a change. In particular, do not automatically
double cover height, jump rise, lift travel, objective trigger size or weapon
respawn intervals. A decoration must not imply cover where shots pass through it,
or an opening where the collision box still blocks a player.

At the current base walk speed of 6.6 m/s, a straight unobstructed end-to-end
crossing becomes √2 or 2 times as long under the two candidates. Actual encounter
times also depend on routes, spawn selection and visibility. Measure those in a
playable blockout before deciding whether combat pacing improved.

## Per-map expansion guidance

**Keel:** Keep the upper deck and the playable space beneath it as the central
encounter. Add room between the bases, lane cover and ramp approaches, with enough
cover to rotate between them. Do not turn the larger floor into an unobstructed
rifle shooting gallery. Preserve clear upper/lower routes and the readable
relationship between rockets above and Overshield below. Start by retaining the
16 × 12 m deck, its four piers and its headroom, then decide deliberately whether
the central fight itself needs more space.

**Bastion:** Expand the circulation around the pit and between the asymmetric
towers and archive room. Retain distinct high and low fights; simply pushing
every landmark farther away can make the sniper tower dominate the map. Keep
the pit accessible from several directions and avoid creating a long, exposed
bridge with no useful alternate rotation. Preserve the existing tower jumps,
room entrances and intermediate steps as local movement challenges.

**Sanctum:** Add approach depth and flanking space around the courtyard rather
than making the middle a huge empty bowl. Preserve the 2.8 m rocket-plinth climb
as an intentional crouch/Power Jump interaction. Keep routes from each base to
the centre comparable, with interruption to excessive sniper sightlines. Extra
width should offer useful flank choices without making the flag routes so remote
that defenders cannot rotate between them.

**Creekbed:** Give the banks and approaches more room while keeping the map's
close-range identity. A 4× area candidate is 112 × 72 m, so it should be tested
particularly carefully before calling it a small, fast arena. Keep hut interiors,
doorways and roof access at their present scale. The caves currently have only
2.4 m of clearance at the banks: do not spend that headroom on hanging details.
Maintain a clear creek crossing and central rocket landmark rather than adding
deep water, visual clutter or new movement penalties during a size pass.

**Spire:** Explore added lower-lane width and deeper base approaches before
automatically stretching the already-long bridge. Its current bridge spans
56 m; simple uniform footprint scaling would make that about 79.2 or 112 m.
Those are long exposed routes for 4v4. Preserve clear lower rotations, meaningful
mid platforms and the four lift routes; re-evaluate each lift's takeoff, arc,
landing and vulnerability if its endpoints move. Bridge width, staggered cover
and base sniper visibility need separate decisions from overall map length.

## Art identity after the scale decision

A provisional shared identity is climate-control infrastructure built from
weathered mineral/ceramic shells around exposed service cores. Use original
silhouettes and restrained amber diagnostic markings, with each map getting a
different setting:

| Map | Original setting candidate |
|---|---|
| Keel | Coastal pressure-engine drydock, salt-stained ceramic and slate metal |
| Bastion | Basalt seismic observatory and archive, bronze service equipment |
| Sanctum | Abandoned floodgarden conservatory, pale ceramic and terracotta |
| Creekbed | Rust-enamel sluice stations in a wet limestone gully |
| Spire | High-altitude barometric exchange above a cloud sea |

These are creative proposals, not asset selections. Avoid specific reference-game
landmarks, logos, faction architecture and recognisable prop silhouettes.
Keep team colours and player silhouettes readable against neutral combat surfaces.
Place dense foliage, machinery and strong atmospheric detail outside active
sightlines. Reserve the most conspicuous local glows for gameplay information,
including pickups and lifts; keep purely decorative lights visually subordinate.

The first art slice follows the user's choice of **Spire**: one base/bridge
threshold, a lower-route section, a lift landing and the distant landmark seen
from them. A proposed setting is a storm-harvesting exchange above a broken
mountain caldera, using grounded service machinery and original layered structural
fins. Resolve the bridge's exposed length, lower-route width and base spacing in
the greybox first, then test material readability and above/below combat in that
small slice before expanding the kit across all five maps.

## Phase plan and acceptance evidence

1. Use overhead previews to choose a map's length, width, retained landmarks and
   intended pace. Record the user's dimensions and route decisions before geometry
   edits. A 2× area trial is a useful first comparison with the 4× area candidate;
   neither is an approved default.
2. Author one expanded greybox in the generator. Keep its gameplay-scale pieces
   deliberate and preserve point symmetry where the map currently uses it. Run
   the map validator, regenerate C++ and previews, and check surface support,
   headroom and reachable connections.
3. Playtest the expanded map with eight participants/bots as appropriate. Measure
   time to the first fight, spawn-to-centre and flag-return travel, idle search
   time, effective cover, pickups, objective rotations and bot reachability.
   Report bot tests separately from human matches.
4. Check local network joins and map travel, spawn fairness, lift landings and
   performance. A larger floor can increase navigation work and visible scenery;
   measure these costs rather than increasing player or bot count to fill space.
5. Accept or revise the greybox, then create the small art slice. Compare it with
   the accepted collision layout before expanding the kit across the map. Keep
   render detail, texture memory and effects viable on the lower-memory laptop
   as well as the desktop.

The original proposal was design-only. The later playable size pass is
recorded at the top of this document.
