# Shieldbreak: Design Pillars

**Easy to pick up:** a Halo 2/3 sandbox. Two weapons, grenades, melee, recharging shields. A new player can ignore every advanced mechanic and still play well.

**Nearly impossible to master:** each advanced mechanic is built on the shield system the player already understands. Each one rewards timing, positioning, and reading the opponent, and each has a real cost when misused.

## Core (Halo baseline)

- Shields absorb damage, then recharge after a delay. Health sits underneath.
- Two-weapon carry, map weapon pickups, frag and plasma-style grenades.
- Strong melee, headshot rule on unshielded targets.
- Floaty jumps, no ADS slowdown on most weapons, no loadouts. Everyone spawns equal.

## Advanced mechanics (the mastery layer)

Each mechanic has one input, a visible tell for opponents, and a cost.

### 1. Shield Shaping (hold `Shape`)
Concentrate your shield into a frontal arc. Frontal damage is reduced by about 40%, but hits from behind or the sides land at about 1.5× and the recharge delay is longer.
- *Easy:* hold it when you are winning a 1v1 head-on.
- *Mastery:* release it the instant a second enemy flanks you. Bait opponents into shooting a shaped shield, and read the visible glow on enemies' shields.

### 2. Break Pulse (melee within ~0.15 s of your shield popping)
If you melee right as your shield breaks, the collapsing shield is released as a short-range shockwave. It knocks back and staggers enemies and strips part of their shields.
- *Easy:* it occasionally happens by accident and feels great.
- *Mastery:* frame-tight timing, deliberately "letting" your shield break at the right range, and pulsing enemies off ledges.
- *Cost:* the next shield recharge takes +2 s.

### 3. Overcharge Vent (tap `Vent` during recharge)
While your shield is recharging, a moving marker crosses the shield bar. Tap inside the sweet spot to finish recharging instantly *plus* gain a small overshield. Miss, and the recharge restarts from zero.
- *Easy:* ignore it. Normal recharge is always safe.
- *Mastery:* hitting it while under pressure, and deciding when the risk is worth it.

### 4. Momentum Carry (sprint → crouch-slide → jump)
Sliding preserves sprint speed. Jumping out of a slide carries that velocity into the air. Shooting while airborne after a carry has slightly more bloom.
- *Easy:* sprint and jump like normal.
- *Mastery:* chaining slides down slopes, using grenade jumps, and rotating across the map faster than anyone else.

### 5. Shield Bleed (weapon property)
Some weapons, mainly plasma-style ones, do bonus shield damage and *extend the target's recharge delay*. Ballistic weapons do bonus health damage.
- *Easy:* "shoot with anything."
- *Mastery:* the Halo noob-combo, made deeper. Strip with plasma, swap, then headshot, while timing your swaps against the enemy's recharge.

## Design rules for adding mechanics

1. Every mechanic must be optional for a new player to succeed in a casual match.
2. Every mechanic must have an audiovisual tell that opponents can read and counter.
3. Every mechanic must have a cost or failure state, so there is no strictly-better option.
4. Every number lives in a data asset (`UShieldTuningData`, weapon DataTables), never hard-coded, so playtesting can iterate fast.
