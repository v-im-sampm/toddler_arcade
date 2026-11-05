# Feed the Monster (G09)

Spec: [games/feed_monster.md](../../../../games/feed_monster.md). The scene logic, LVGL view, procedural art,
launcher tile and synthesized sound bank are implemented. The browser mockup is
[docs/mockups/feed_monster.html](../../../../docs/mockups/feed_monster.html). This code has not been validated
on a device; host tests and the desktop simulator are not device validation.

## Behaviour
- INTRO (1.2 s, any input skips without feeding) → PLAY → CELEBRATE (1.8 s) → PLAY with a new request.
- A friendly original monster asks for **1–3 snacks**. The request is chosen through `ctx.rng` (deterministic per seed) and never repeats back-to-back, so 1, 2 and 3 all appear.
- Visual count works with audio off:
  - the top-bar request bubble shows N snack pictures
  - only N tummy dots are shown, and one fills per snack eaten
  - the matching count button is highlighted with the same snack pictures
- Each snack plays a `SUCCESS` cue with variant = running count; the sound bank uses a distinct soft count motif
  and munch sound. Spoken prompts are not included in this implementation.
- When the tummy count reaches the request, the monster celebrates.
- Extra taps are never penalised:
  - Food taps during CELEBRATE make the monster giggle (`REACTION`, `extra_snacks()`). The count stays correct.
  - Saying or tapping a number larger than the request just completes the round.
  - Saying or tapping a number at or below the current count makes the tummy dots glow. Nothing else changes.
- After 6 s idle in PLAY, one gentle reminder replays the request. There's no timeout and no failure state.
- Short chew (350 ms) and giggle (500 ms) animations never block input. Voice input and number buttons are ignored during CELEBRATE.

## Layout (320×240, all ≥48 px, ≥8 px apart, clear of Home/Mic)
| Target | Rect | Action |
|---|---|---|
| Food bowl | `{8,72,112,96}` | feed one snack |
| Monster | `{136,72,176,96}` | replay request |
| Number buttons 1/2/3 (snacks) | `{8,176,96,56}` `{112,176,96,56}` `{216,176,96,56}` | count along to N |
| Request bubble (top bar) | `{104,4,112,56}` | shows N snack pictures; tap replays request |

## Input map (contexts: `Counting`; SceneManager adds `Global`)
| Action | Touch | Voice intent |
|---|---|---|
| Feed one snack | tap food bowl | — |
| Count along to N | tap number button N | ONE / TWO / THREE |
| Hear/see request again + hint | tap monster or request bubble | AGAIN, HELP |
| Home | Home button (SceneManager) | BACK (SceneManager) |

## Cue → asset IDs (declared in `assets/manifest.json`)
| Cue | variant | Placeholder asset id |
|---|---|---|
| PROMPT | requested count 1–3 | `feed_monster.prompt.{one,two,three}` (count motif) |
| TAP | 0 | `common.tap` |
| SUCCESS | running count 1–3 | `feed_monster.count.{one,two,three}` + munch |
| CELEBRATE | 0 | `common.celebrate` |
| REACTION | 0 | `feed_monster.giggle` |

## Tests
`feed_monster_test` and `feed_monster_art_test` (host only) cover:
- layout safety
- intro timing and intro-skip without feeding
- counts 1–3 with variety, visual and distinct sound cues, and no back-to-back repeat
- excess taps never penalised
- voice/food/number-button equivalence
- counting along by voice
- overshoot number
- input and voice during celebration
- AGAIN/HELP/monster/bubble replay
- one-shot idle hint
- harmless empty taps and unrelated intents
- chew animation not dropping taps
- audio disabled (`NullFeedback`)
- exit mid-animation and fresh re-entry
- same-seed determinism
- playing through the `SceneManager`: launch with `COUNTING`, out-of-context GO/RED filtered, Home returns to MENU
