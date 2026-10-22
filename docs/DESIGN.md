# Shieldbreak: Game Design

**Pitch:** Halo 2's competitive arena feel (recharging shields, the BR duel, power-weapon map control) with
modern Halo Infinite-level polish, and movement where strafing and positioning decide fights.
Easy to pick up, very hard to master. Launch target: **5 maps × 4 modes**.

All designs are original. The classic maps below are referenced for *principles* (flow, sightlines,
weapon placement), never copied geometry or art. See `ART_DIRECTION.md`.

---

## 1. Core pillars
1. **Every fight is a readable duel.** Same spawn weapons, a consistent kill time and accurate hit detection.
   No mechanic that hides the outcome from the shooter.
2. **Movement is defence.** Strafing, jumping and crouching make you genuinely harder to hit, so good
   movement beats a stationary aimer.
3. **The map is the meta.** Power weapons and power-ups sit in neutral, contested spots on timers that are
   never shown on screen. Tracking them is a skill.
4. **Mastery stacks.** Each advanced technique is optional, has a visible tell and has a counter.

## 2. Player
| Stat | Value | Why |
|---|---|---|
| Shield / Health | 70 / 45 | Halo-style shield over health |
| Shield recharge | 4.5 s delay, 2 s refill | Rewards disengaging |
| Walk speed | **660** (up from 600, ~10% faster than Halo 2) | Strafing matters |
| Acceleration | 6000, braking 3200 | Instant direction changes for strafe dodging |
| Jump | ~2.6 m apex, ~1.15 s airtime | Less floaty than Halo 2 |
| BR kill | 4 bursts (~1.4 s) | Long enough for movement to matter, short enough for aim to win |

## 3. Advanced techniques (the skill gap)
| Technique | How | Counter |
|---|---|---|
| **Overcharge Vent** | Tap Reload/Vent while the marker is in the sweet spot during recharge: instant full shield plus a small overshield. A miss restarts the recharge from empty. | Push them while they're recharging |
| **Break Pulse** | Melee within 0.15 s of your shield popping: short-range shockwave that knocks back and strips shields | Keep your distance; it costs a longer recharge |
| **Momentum Carry** | Sprint, crouch to slide, jump out of it: keeps the slide speed in the air; downhill slides speed up; slides chain | Shots get less accurate in the air |
| **Grenade / rocket jumps** | Your own explosions launch you (rockets do 0.4× damage to you) | They cost shields and health |
| **Crouch-jump** | Crouch mid-air to clear higher ledges | — |
| **Radar discipline** | Crouch-walking keeps you off the enemy motion tracker | Allies always show |

**Removed:** Shield Shaping (directional shields). It made damage unreadable.

## 4. Sandbox (deliberately small)
| Item | Slot / source | Role |
|---|---|---|
| **Battle Rifle** | Spawn | 3-round burst; headshot kills an unshielded target; 4-burst kill |
| **Sidearm** | Spawn | Semi-auto precision pistol: fast swap, finishing shots, headshot kills an unshielded target |
| **Sniper Rifle** | Map, usually one per side | 2 body shots kill; a headshot kills through shields |
| **Rocket Launcher** | Map, neutral and most contested | The best weapon: lethal splash, rocket jumps |
| **Rail Lance** | Map, medium power | Hold to charge (~0.8 s, glow + sound tell), release for a one-hit-kill bolt. Released early it does about a third of the damage. 3 charges per pickup. |
| **Aegis Shield** | Map item (equipment), light power | Hold Use Equipment to raise a frontal barrier that blocks bullets and Rail Lance bolts but **not** rockets, grenades or melee. You can't fire while it's raised. 200 hit points or 6 s of uptime per pickup. |
| **Frag grenades** | 2 on spawn | Bounce, 1.1 s fuse after the first bounce |
| **Overshield** | Map power-up (some maps) | +140 overshield that decays |
| **Active Camo** | Map power-up (other maps) | Near-invisible while still; shimmers when moving or shooting; 30 s |

**Removed from play:** SMG and Plasma Rifle (not competitive), and with them Shield Bleed.

## 5. Modes (launch)
| Mode | Rules |
|---|---|
| **Slayer / Team Slayer** | First to 50 kills (team), 25 (FFA) |
| **Capture the Flag** | Flags at each base. Your own flag must be home to score. A dropped flag returns after 15 s or when touched by its own team. Carrier: sidearm only, slower. First to 3. |
| **King of the Hill** | One hill moves every 60 s. Score while only your team is in it. First to 100 s. |
| **Oddball** | One ball in the middle. Score for each second you hold it; the carrier melees only and is visible on the tracker. First to 100 s. |

## 6. Launch maps (original layouts inspired by classic principles)
| Map | Inspired by | Shape | Weapon placement | Power-up | Best modes |
|---|---|---|---|---|---|
| **Keel** | Midship | Symmetric, two bases joined through a raised central hub with low and high routes | Rockets at the hub top; Rail Lance below | Overshield under the hub | CTF, Slayer |
| **Bastion** | Lockout | Asymmetric towers around a central pit, tight vertical angles | Sniper on the tallest tower; rockets in the pit | Camo | KOTH, Oddball, Slayer |
| **Sanctum** | Sanctuary / The Pit | Symmetric open bowl with ruins on two flanks | A sniper per side; rockets low in the middle | Overshield | CTF, Slayer |
| **Creekbed** | Beaver Creek | Small 4v4: two bases across a shallow creek, side caves | Rockets mid-creek; Aegis on a cave side | — | Slayer, CTF |
| **Spire** | Narrows / Construct | Two bases joined by a long central bridge; gravity lifts; height wins | Rockets under the bridge; a sniper per base | Camo | CTF, KOTH |

## 7. Build order
1. Mechanics: remove Shaping, speed tuning, sandbox (Sidearm, Rail Lance, Aegis), with SMG/Plasma out of play.
2. Mode framework plus CTF, KOTH and Oddball.
3. Map framework (code-built layouts) plus the 5 maps as greyboxes.
4. Art pass per `ART_DIRECTION.md`.
