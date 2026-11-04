# Bubble Aquarium (G10)

Spec: [games/bubble_aquarium.md](../../../../games/bubble_aquarium.md). Status: **scene logic and host tests only**.
There is no LVGL view yet and no assets yet. This scene has not been run on a device.

## Behaviour
- The scene goes INTRO (1.5 s, any input skips it) → PLAY → CELEBRATE (3 s, surprise visitor) → PLAY.
- Play is calm and open-ended. There's no score, no timer and no fail condition. Nothing is ever lost, and `Cue::TRY_AGAIN` is never used.
- Three original fish, one of each `FishType`, swim slowly and bounce off the glass:
  - STRIPEY: a striped oval, 64×48
  - PUFFY: a round, spotty fish, 56×56
  - LONGFIN: a long fish with flowing fins, 80×48

  Each type has a different shape and size, so colour isn't the only way to tell them apart. Fish always stay inside `kTank` (the play area, y ≥ 64).
- **Tap the water** to blow a bubble at that spot. Bubbles rise and disappear at the surface.
- **Tap a bubble** to pop it. Each bubble has a 48×48 hit box.
- **Tap a fish** to make it do a happy wiggle (`happy_ms`).
- **Food shaker** (top bar) drops a pellet at a random x. Pellets sink and rest on the sand. Fish swim towards the nearest pellet and eat it, then show a "yum" glow (`munch_ms`).
- **Bubble button** (top bar) or the voice command AGAIN makes the treasure chest burst out 3 bubbles.
- After every 3 meals, a surprise visitor (TURTLE, SEAHORSE or JELLY) swims across the tank during CELEBRATE. The same visitor never appears twice in a row.
- PLAY and CELEBRATE accept the same input, so taps and voice during the visitor animation are safe and do what the child expects.
- Fixed-size pools mean memory never grows over time:
  - 3 fish
  - up to 8 bubbles (when the pool is full, the oldest bubble is recycled)
  - up to 6 food pellets (when the pool is full, the shaker still wiggles but drops nothing)
- There's no heap allocation. Physics runs in 50 ms sub-steps, with at most 1 s of simulated time per `update()`. All randomness comes from `ctx.rng`.
- After `exit()`, calls to `update()` and `handle()` do nothing. `enter()` starts a fresh tank with no saved profile.

## Layout (320×240)
| Element | Rect | Notes |
|---|---|---|
| Tank / water | `kPlayArea` {0,64,320,176} | primary target |
| Food shaker | {72,0,56,56} | top bar, between Home and Mic |
| Bubble button | {192,0,56,56} | top bar, between Home and Mic |
| Visitor | 72×64 at y=120 | visual only, not a target |

## Input map
| Action | Touch | Voice intent |
|---|---|---|
| Blow a bubble | tap water | — |
| Pop a bubble | tap bubble | — |
| Make a fish wiggle | tap fish | — |
| Feed fish | food shaker button | — (no feed intent exists) |
| Bubble burst | bubble button | AGAIN |
| Home | Home button (SceneManager) | BACK (SceneManager) |

`active_contexts()` returns 0. The scene only uses the Global intent AGAIN, and the `SceneManager` adds Global itself. HELP and NEXT reach the scene but are ignored.

## Cue → asset placeholders (to be declared in `assets/manifest.json`)
| Cue | Variant | Placeholder asset id | Visual counterpart |
|---|---|---|---|
| PROMPT | 0 | `bubble_aquarium.prompt.tap_water` | intro: pointing hand shown over the water |
| AMBIENT | 0 | `bubble_aquarium.ambient.calm_water` | swaying plants, slow fish |
| NOTE | 0–2 | `bubble_aquarium.blub_{0,1,2}` | new bubble at the tap point |
| NOTE | 3 | `bubble_aquarium.pop` | bubble disappears |
| NOTE | 4 | `bubble_aquarium.chest_burst` | chest opens, 3 bubbles, button pulse |
| TAP | 0 | `bubble_aquarium.shaker` | shaker pulse and pellet appears |
| REACTION | fish type | `bubble_aquarium.giggle_{stripey,puffy,longfin}` | fish wiggle |
| SUCCESS | fish type | `bubble_aquarium.munch_{stripey,puffy,longfin}` | fish "yum" glow |
| CELEBRATE | visitor | `bubble_aquarium.visitor_{turtle,seahorse,jelly}` | visitor swims across |

## Tests
`bubble_aquarium_test` (host only) covers:
- layout safety: sizes, spacing, top-bar zone, and no overlap with Home or Mic
- intro flow, and skipping the intro without acting
- 3 distinct fish types
- bubbles: tap, rise, pop and surface
- fish reactions
- feeding
- visitors after 3 meals, with no back-to-back repeats
- bounded pools under heavy tapping, and a 10-minute session with fish staying inside the tank
- AGAIN matching the bubble button
- input during celebration, and no fail state when idle
- harmless stray or out-of-context input
- audio disabled (`NullFeedback`)
- `exit()` mid-celebration followed by a fresh re-enter
- same-seed determinism
- playing through the `SceneManager`: FISH launches the game, GO and RED are filtered out, AGAIN reaches the game, and a Home tap returns to MENU
