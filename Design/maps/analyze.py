#!/usr/bin/env python3
"""Competitive layout metrics for the Shieldbreak maps (estimates from geometry, not playtest results).

    python3 Design/maps/analyze.py [MapId ...] [--png]

Uses the bot walk graph from shieldbreak_maps.walk_graph (50 cm grid, step-ups, ramps, drops, lifts) plus player-only
jump links, and line-of-sight tests against the blocks (ramps are thin and ignored). Speeds come from the movement
component: sprint 950 cm/s, walk 660; a Power Jump costs ~0.5 s of charge. Everything here is an estimate: it ignores
acceleration, ramp slowdown, strafing and other players.
"""

import heapq
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import shieldbreak_maps as S  # noqa: E402

SPRINT = 950.0
EYE = 160.0
JUMP_UP = 200.0        # a standing jump reliably lands on a ledge this high (apex ~258)
POWER_UP = 330.0       # a full Power Jump (apex ~387)
POWER_COST = 0.5       # seconds of charge
BR_OPTIMAL = 4000.0    # Battle Rifle: base spread 0.25 deg ~ 17 cm at 40 m (head-sized); beyond it, headshots get luck-based


def segment_blocked(idx, a, b, ignore_small=False):
    """True if the segment a-b passes through a block (slab test). Uses the index cells along the segment."""
    m = idx.m
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    length = math.hypot(dx, dy)
    steps = max(1, int(length / (idx.cell * 0.5)))
    seen = set()
    for k in range(steps + 1):
        x, y = a[0] + dx * k / steps, a[1] + dy * k / steps
        for c in ((int(math.floor(x / idx.cell)), int(math.floor(y / idx.cell))),):
            for i in idx.cells.get(c, ()):
                if i in seen:
                    continue
                seen.add(i)
                mn, mx, _ = m.blocks[i]
                t0, t1 = 0.0, 1.0
                ok = True
                for o, d, lo, hi in ((a[0], dx, mn[0], mx[0]), (a[1], dy, mn[1], mx[1]), (a[2], dz, mn[2], mx[2])):
                    if abs(d) < 1e-9:
                        if not (lo < o < hi):
                            ok = False
                            break
                    else:
                        u, v = (lo - o) / d, (hi - o) / d
                        t0, t1 = max(t0, min(u, v)), min(t1, max(u, v))
                        if t0 >= t1:
                            ok = False
                            break
                if ok:
                    return True
    return False


def add_jump_links(g):
    """Player-only links: jump up onto a ledge (<= JUMP_UP) or Power Jump (<= POWER_UP), from 1-3 cells away."""
    jumps = [[] for _ in g.nodes]
    for n, (i, j, z, _) in enumerate(g.nodes):
        for di in range(-3, 4):
            for dj in range(-3, 4):
                if (di, dj) == (0, 0) or abs(di) + abs(dj) > 4:
                    continue
                for k, (z2, _) in enumerate(g.grid.get((i + di, j + dj), ())):
                    dz = z2 - z
                    if S.STEP_UP < dz <= POWER_UP:
                        t = g.node_of[(i + di, j + dj, k)]
                        a, b = g.pos(n), g.pos(t)
                        # head clearance over the arc: nothing solid between take-off (raised) and landing
                        if segment_blocked(g.idx, (a[0], a[1], max(a[2], b[2]) + 120), (b[0], b[1], b[2] + 120)):
                            continue
                        jumps[n].append((t, dz > JUMP_UP))
    g.jumps = jumps
    return g


def dijkstra(g, sources, jumps=False, penalty=None):
    dist = [math.inf] * len(g.nodes)
    prev = [-1] * len(g.nodes)
    heap = []
    for s in sources:
        dist[s] = 0.0
        heap.append((0.0, s))
    heapq.heapify(heap)
    while heap:
        d, n = heapq.heappop(heap)
        if d > dist[n]:
            continue
        a = g.pos(n)
        cand = [(t, False) for t in g.fwd[n]]
        if jumps:
            cand += g.jumps[n]
        for t, power in cand:
            b = g.pos(t)
            h = math.hypot(b[0] - a[0], b[1] - a[1])
            fall = max(0.0, a[2] - b[2])
            cost = (h if h < 300 else h * 0.6) / SPRINT  # lifts: long hop at ~1.6x speed
            cost += math.sqrt(2 * fall / 1568.0) * 0.5 if fall > 100 else 0.0
            cost += POWER_COST if power else 0.0
            if penalty:
                cost *= penalty[t]
            if d + cost < dist[t]:
                dist[t] = d + cost
                prev[t] = n
                heapq.heappush(heap, (d + cost, t))
    return dist, prev


def path_to(prev, t):
    out = []
    while t != -1:
        out.append(t)
        t = prev[t]
    return out[::-1]


def team_spawns(m):
    red = [s for s in m.spawns if s[0] < 0]
    blue = [s for s in m.spawns if s[0] > 0]
    return red, blue


def anchors(m):
    out = []
    for k, p, _ in m.weapons:
        out.append((k, p))
    for k, p, _ in m.powerups:
        out.append((k, p))
    for t, p in enumerate(m.flags):
        if p:
            out.append(("red flag" if t == 0 else "blue flag", p))
    if m.hills:
        for h_i, h in enumerate(m.hills):
            out.append((f"hill {h_i + 1}", (h[0], h[1], h[2] - 150)))
    return out


def sample_points(g, step_cells=5):
    """Standable network points on a coarser grid (step_cells * 50 cm): the highest per column."""
    out = []
    for (i, j), st in g.grid.items():
        if i % step_cells or j % step_cells:
            continue
        for k, (z, _) in enumerate(st):
            out.append(g.node_of[(i, j, k)])
    return out


def visible_from(g, target, pts, max_range=6000.0):
    t = (target[0], target[1], target[2] + EYE - 70)
    n = 0
    out = []
    for p in pts:
        a = g.pos(p)
        e = (a[0], a[1], a[2] + EYE)
        d = math.dist(e, t)
        if d > max_range or d < 100:
            continue
        if not segment_blocked(g.idx, e, t):
            out.append(p)
    return out


def analyze(m, png=False):
    g = S.walk_graph(m)
    add_jump_links(g)
    errors, stats = S.reachability(m, g)
    red, blue = team_spawns(m)
    rs = [g.nearest(s) for s in red]
    bs = [g.nearest(s) for s in blue]
    dr, pr = dijkstra(g, rs)
    db, pb = dijkstra(g, bs)
    drj, _ = dijkstra(g, rs, jumps=True)
    dbj, _ = dijkstra(g, bs, jumps=True)
    lines = []
    o = S.openness(m)
    lines.append(f"## {m.name}  ({2 * m.half_x / 100:.0f} x {2 * m.half_y / 100:.0f} m)")
    lv = ", ".join(f"{k / 100:.1f}-{(k + 250) / 100:.1f} m: {v:.0f}" for k, v in sorted(stats["levels"].items()))
    lines.append(f"- walkable network {stats['network_m2']:.0f} m2 of {stats['standable_m2']:.0f} m2 standable; by height ({lv} m2)")
    lines.append(f"- exposed floor {o['exposed'] * 100:.0f}%, mean view {o['sight_mean']:.0f} m, p90 {o['sight_p90']:.0f} m")
    if errors:
        lines.append(f"- reachability issues: {len(errors)}")
        for e in errors[:6]:
            lines.append(f"  - {e}")
    pts = sample_points(g)
    # Opening times (closest spawn of each team; sprinting; walk-only vs with jumps).
    lines.append("- opening times from the nearest spawn, sprinting (walk routes / with jumps), red vs blue:")
    for name, p in anchors(m):
        n = g.nearest((p[0], p[1], p[2] - 70))
        if n is None:
            continue
        a, b, aj, bj = dr[n], db[n], drj[n], dbj[n]
        f = lambda v: "unreachable" if v == math.inf else f"{v:.1f}s"
        lines.append(f"  - {name:12s} red {f(a)} / {f(aj)}   blue {f(b)} / {f(bj)}")
    # Spawn exposure: enemy-half standable points with eye LOS to the spawn within 50 m.
    lines.append("- spawn exposure (enemy-half points that can see the spawn within 50 m):")
    expo = []
    for s in m.spawns:
        enemy = [p for p in pts if (g.pos(p)[0] > 0) == (s[0] < 0)]
        vis = visible_from(g, (s[0], s[1], s[2] + 70), enemy, 5000)
        expo.append((len(vis), s))
    expo.sort(reverse=True)
    lines.append("  - " + ", ".join(f"{tuple(int(v) for v in s)}: {c}" for c, s in expo[:6]))
    # Power item exposure.
    lines.append("- pickup exposure (standable points that see it within 60 m: total / red half / blue half):")
    for name, p in anchors(m):
        if "hill" in name or "flag" in name:
            continue
        vis = visible_from(g, p, pts, 6000)
        r = sum(1 for v in vis if g.pos(v)[0] < 0)
        lines.append(f"  - {name:12s} {len(vis)} / {r} / {len(vis) - r}")
    # Long sightlines.
    longs = 0
    total = 0
    for p in pts[::2]:
        a = g.pos(p)
        e = (a[0], a[1], a[2] + EYE)
        for k in range(8):
            ang = k * math.pi / 4
            far = (e[0] + math.cos(ang) * 6000, e[1] + math.sin(ang) * 6000, e[2])
            total += 1
            if not segment_blocked(g.idx, e, far):
                longs += 1
    lines.append(f"- share of eye-level rays clear past 60 m: {longs / max(1, total) * 100:.0f}%")
    # Longest standing-to-standing sightlines (10 m sample): where a sniper or a BR at the edge of its range rules.
    coarse = sample_points(g, 20)
    long_pairs = []
    for ai in range(len(coarse)):
        a = g.pos(coarse[ai])
        ea = (a[0], a[1], a[2] + EYE)
        for bi in range(ai + 1, len(coarse)):
            b = g.pos(coarse[bi])
            eb = (b[0], b[1], b[2] + EYE)
            d = math.dist(ea, eb)
            if d >= 6000 and not segment_blocked(g.idx, ea, eb):
                long_pairs.append((d, a, b))
    long_pairs.sort(reverse=True)
    lines.append(f"- standing sightlines over 60 m (10 m sample): {len(long_pairs)}; longest: "
                 + "; ".join(f"{d / 100:.0f} m ({a[0]:.0f},{a[1]:.0f},{a[2]:.0f})-({b[0]:.0f},{b[1]:.0f},{b[2]:.0f})"
                             for d, a, b in long_pairs[:4]))
    g.long_pairs = long_pairs
    # Routes between the flags (or spawn groups): three diverse routes.
    src = g.nearest((m.flags[0][0], m.flags[0][1], m.flags[0][2] - 90))
    dst = g.nearest((m.flags[1][0], m.flags[1][1], m.flags[1][2] - 90))
    penalty = [1.0] * len(g.nodes)
    routes = []
    for r in range(3):
        d, prev = dijkstra(g, [src], penalty=penalty)
        if d[dst] == math.inf:
            break
        path = path_to(prev, dst)
        secs = sum(math.dist(g.pos(path[i])[:2], g.pos(path[i + 1])[:2]) for i in range(len(path) - 1)) / SPRINT
        hi = max(g.pos(n)[2] for n in path)
        routes.append((secs, hi, path))
        for n in path:
            i, j, _, _ = g.nodes[n]
            for di in range(-12, 13):
                for dj in range(-12, 13):
                    for k in range(len(g.grid.get((i + di, j + dj), ()))):
                        penalty[g.node_of[(i + di, j + dj, k)]] = 4.0
    lines.append("- flag-to-flag routes (each one avoids the earlier ones by 6 m): "
                 + "; ".join(f"{secs:.1f}s, peak {hi / 100:.1f} m" for secs, hi, _ in routes))
    if png:
        render_routes(m, g, routes)
    return "\n".join(lines)


def render_routes(m, g, routes):
    from PIL import Image, ImageDraw
    path = os.path.join(S.PNG_DIR, f"{m.id}.png")
    tmp = os.path.join(os.environ.get("ANALYZE_OUT", "/tmp"), f"{m.id}_routes.png")
    S.render_png(m, tmp)
    img = Image.open(tmp).convert("RGB")
    d = ImageDraw.Draw(img)
    scale, pad = 0.12, 60
    px = lambda x, y: (pad + (x + m.half_x + 100) * scale, pad + 70 + (m.half_y + 100 - y) * scale)
    colors = [(255, 255, 255), (255, 170, 0), (0, 255, 120)]
    for (secs, hi, p), c in zip(routes, colors):
        d.line([px(*g.pos(n)[:2]) for n in p[::4]], fill=c, width=3)
    img.save(tmp)
    return tmp


def main():
    ids = [a for a in sys.argv[1:] if not a.startswith("--")]
    for m in S.MAPS:
        if m.id == "Crucible" or (ids and m.id not in ids):
            continue
        print(analyze(m, png="--png" in sys.argv))
        print()


if __name__ == "__main__":
    main()
