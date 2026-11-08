# Color Pop: native source-of-truth contract

Source of truth: `v2/reference/quality_bar.html` (the runnable sketch). Design: `v2/games/color_pop.md`.
Where they disagree about implemented behaviour, the native port follows the sketch. The
discrepancies are listed at the end of this file.

## Update model
- Variable step: every frame calls `update(dt)` with `dt = min(0.05, real seconds)`. There is no
  fixed 50 ms step (that is Aquarium's model). Tests and the browser comparison use `dt = 1/64` s,
  which is exactly representable, so the browser and native timelines stay identical.
- `time` and `st.t` accumulate `dt`. All timers are in seconds (double precision in the native core,
  matching JavaScript numbers).

## Randomness
| Stream | Reference | Native | Used for |
|---|---|---|---|
| gameplay | `grng = mulberry32(seed)`, default seed 1234 (`?seed=N`) | `Mulberry32(seed)` | target bag, distractor colours, slot shuffle, slot jitter, wrong-pop note |
| cosmetic | `Math.random` | `Mulberry32(seed ^ 0x9E3779B9)` | bubble phase, particles, crab blink, decor bubbles, celebrate-tap note |

The browser comparison replaces `Math.random` with the same cosmetic Mulberry32 and stubs
`initAudio` (its noise buffer would otherwise consume `Math.random`). The native audio noise uses
its own generator, so sound never perturbs either stream. Gameplay is a pure function of the seed
plus the input timeline. Cosmetic output also depends on the call order, which the port mirrors.

## States and progression
`START → INTRO → PLAY → CELEBRATE → PLAY …`
- **START**: the dim veil and pulsing play button (scale `1 + 0.04·sin 3t`). Any tap calls
  `startGame`: audio ramp-in, INTRO, `crab.yOff = 40`.
- **INTRO**: `yOff = lerp(40, 0, easeOutBack(t/0.6))`. At `t ≥ 1.2` s, `newRound()`. A tap
  (other than Home or mic) sets `t = 99` to skip.
- **newRound**:
  1. `round++` and reset `misses`, `hint`, `idle`, `spoken` and `roundAutoHelp`.
  2. Pick the target from the shuffle bag. It never repeats across a bag boundary: if the new
     bag's last element equals `last`, swap it with element 0.
  3. Shuffle the other colours and take `count − 1` distractors.
  4. Shuffle the slots (i = 5..1).
  5. Bubble *i* rises after `0.12·i` s, with slot jitter `grng·12−6` in x, then y.
  6. Set `badge.sv = −6`. The target motif plays 250 ms later.
- **PLAY**: Idle `> 6` s gives hint 1. Idle `> 12` s replays the motif once and sets `badge.sv = −8`.
- **Target pop**: `successes++`. Every 5th success is a *big* celebration. Every 3rd success raises
  `count` (to a maximum of 4). If this round reached auto-help (hint 2), `autoStreak++`, and two
  such rounds in a row lower `count` (to a minimum of 2). Other bubbles leave, pending respawns are
  dropped, and the game enters **CELEBRATE** for 1.5 s (2.0 s if big). The celebration motif plays
  180 ms later.
- **Wrong pop**: the bubble still pops, with a softer sound. `misses++`. The hint becomes 1, or 2
  from the second miss onwards. The same colour respawns in the same slot after 1.0 s (only while
  in PLAY).
- **Home** `[0,0,56,56]`: clears bubbles, particles and rings and returns to START with
  `crab.yOff = 40`. `successes`, `count`, the bag and `autoStreak` persist. That is the
  reference's actual behaviour.

## Input (pointer down only; no drag anywhere in the reference)
The steps are checked in order:
1. `crab.look = (x−42, y−190)`.
2. START → start.
3. Home rect → home.
4. Mic rect `[264,0,56,56]` → a white ring at (292,28) only. The mic is disabled.
5. INTRO → skip.
6. CELEBRATE: if `t > 0.3`, 6 sparkles plus a random pentatonic note. Otherwise ignore.
7. Badge rect `[132,4,56,56]` → replay the motif, `badge.sv = −8`, and reset idle. A second tap
   within 1 s raises the hint to at least 1.
8. The nearest bubble in `rise` or `float` with distance `< 40`. Apply a 0.08 s debounce per
   bubble, then `press` (squash 0.88 × 1.1424). It pops after 0.08 s.
9. Crab rect `[4,186,76,54]` → giggle 0.5 s and a high plop.
10. Empty space → a white ring (r 3→22, 0.5 s), a decor bubble, a soft high C and an idle reset.

Hit areas: the Home, mic, badge and crab rects above, plus bubbles with radius 40 (diameter 80,
at least 48). Every response is forgiving: nothing is punished, there is no timer and there is no
failure state.

## Motion (per update, in reference order)
- Bubble rise: `easeOutCubic` from y 250 to the slot over 0.6 s. Before its delay the bubble sits
  at y 260 and its spring/breath are skipped.
- Float: `y = by + 2·m·sin(2.1·ph)` and `x = bx + 1.5·m·sin(1.3·ph)`, where `m = 0.3` in reduced
  motion.
- Leave: `y −= dt(60 + 120t)`, `x += 0.4·sin 3ph`, `alpha = 1 − t/0.9`.
- Scale spring: k 380, c 14, 2 substeps. The target is `1.15 + 0.04·sin 7·time` at hint 2.
- Breath: `sx = lerp(sx, 1+br, .25)` and `sy = lerp(sy, 1−br, .25)`, with
  `br = 0.025·sin(ph·2π/2.8)` (×0.4 in reduced motion).
- Wiggle: when the hint rises, rotate `6°·sin((1 − wig/0.36)·6π)` for 0.36 s.
- Glow: `0.5 + 0.5·sin(time·2π/0.9)` on the target at hint ≥ 1.
- Crab:
  - Body: breath, a jump on clap (`−8·sin`), a giggle wobble (`0.12·sin 40g`) and a dance
    (`0.15·sin 10d` rotation and a `6|sin|` hop).
  - Legs: `sin(6·time + i)`.
  - Blink: every 2–6 s for 0.12 s.
  - Right claw: points at the target bubble at hint ≥ 1.
  - Pupils: follow `look`.
- Badge: bob `1.5·sin 2.2t` (×0.3 in reduced motion). Press squash `1 + clamp(−0.012·sv,
  −0.1, 0.12)`. `sv` decays ×0.86 per rendered frame.
- Helper hand at hint 2: at (target.x+14, target.y+30−6|sin 4t|), rotated −0.5 rad.
- Clouds:
  - Positions: `x = ((ox + time·sp·m) mod 400) − 40`.
  - Cloud 1: ox 40, y 22, speed 4.
  - Cloud 2: ox 210, y 38, speed 2.6.
- Shimmer: `y = 67 + 1.2·sin(x/14 + 1.2·time)`.
- Particles: gravity, rotation and life. Alpha is `clamp(1.6·life/max)`. The circle radius is
  `size·(0.5 + 0.5·life/max)`. The cap is 96, and the oldest particle is evicted. Reduced motion
  halves emission counts.
- Rings: radius `lerp(r0, r1, easeOutCubic k)`, alpha `1−k`.
- Decor bubbles: ambient emission while fewer than 4 small ones exist and `frng < 0.8·dt`. They
  rise and wobble, and are removed above y = −20.

## Audio triggers (C major pentatonic, `degFreq(d, o) = 523.25·2^((PENT[d mod 5] + 12(⌊d/5⌋+o))/12)`)
| Event | Sound |
|---|---|
| start | master gain 0 → 0.35 (τ 0.1 s), compressor −14 dB / 10:1, looped waves noise → LP 420 Hz, gain 0.018 ± 0.014 at 0.11 Hz |
| new round (+250 ms), badge tap, idle 12 s | colour motif (RED, BLUE, GREEN or YELLOW; see MOTIFS) |
| target pop | `plop(0,1)` and `marimba(deg(colour), +30 ms, 1)` |
| celebration (+180 ms) | `[0,2,4,5]` with a 90 ms step, or for a big one `[0,1,2,3,4,5,7,10]` with a 110 ms step, velocity 0.8 |
| wrong pop | `plop(0,.55,1.4)` and `marimba(deg(grng·5, 1), +20 ms, .45, .35)` |
| crab tap | `plop(0,.6,1.6)` |
| empty tap | `marimba(deg(0,1), 0, .3, .25)` |
| celebrate tap | `marimba(deg(frng·5, 1), 0, .4, .3)` |

The voice limit is 8 (`voiceOk`). Every sound has a visual counterpart (pop burst, ring, claps,
badge face and squash), so play is complete with the sound off.

## Visual layers (draw order)
1. Static background (sky, sea, sand and sand specks).
2. Clouds.
3. Horizon shimmer.
4. Bubbles in array order, each drawn as glow ring, shadow, gradient body, stroke, iridescent rim,
   glyph and highlights.
5. Crab: legs, arms and claws, body, eye stalks and eyes, pupils, mouth, cheeks.
6. Helper hand.
7. FX: decor bubbles, rings, then particles (sparkles are additive).
8. Badge: glow, cream disc, colour disc, then the glyph or a happy face.
9. Chrome: Home, plus the mic at 45 % opacity.
10. The START veil and play button.

Every bubble colour has a distinct glyph (heart, star, leaf, wave), so colour is never the only cue.

## Discrepancies between the design and the reference (the reference is followed)
| Design (`color_pop.md`) | Reference / native |
|---|---|
| Particle cap ≤ 64 | 96 (`deviceMaxParticles`) |
| Spoken prompts and voice answers (Pinch speaks the colour) | No narration and no mic. The mic icon is disabled and only ripples. The colour motif is the prompt. |
| "Pinch shrugs" on a wrong pop | Pop with a softer sound and a hint escalation. No shrug animation. |
| HINT and AUTO_HELP as separate states | Hint levels 1 and 2 inside PLAY |
| Spoken hint at 12 s idle | Motif replay plus badge squash |
| — | Home does not cancel the pending motif and celebrate `setTimeout`s. Native schedules them as score delays, so they likewise still sound. |

## Native-only deviations (documented, deliberate)
- Decor bubbles are capped at 64 and rings at 16. The reference has no cap. Mashing never exceeds
  these limits in practice, but the cap bounds device memory.
- The reference's D (HUD) and P (web particle cap) keys are not ported. R toggles reduced motion
  on the desktop.
- `badge.sv` decays by `0.86^(60·dt)` per update (frame-rate independent) instead of ×0.86 per
  rendered frame.
- Leaving bubbles fade as one group (image opacity). The Canvas applies `globalAlpha` per
  primitive, so the overlap of shadow and body differs slightly while fading.
- Sprites are placed at integer pixel positions. The Canvas uses sub-pixel positions.
- Voice is optional. `Game::intent()` maps generated `little_arcade::Intent` values (from
  `config/commands.json`) to the same actions as touch:
  - RED, BLUE, GREEN, YELLOW: tap that bubble, or else replay the prompt.
  - AGAIN: badge tap.
  - HELP: raise the hint.

  No mic is opened by this game.
