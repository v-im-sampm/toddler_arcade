# Feed the Monster (G09)

Spec: [games/feed_monster.md](../../../../games/feed_monster.md). Status: **scene logic + host tests only**.
There's no LVGL view yet and no assets yet. This code hasn't run on a device. Host tests don't count as device validation.

## Behaviour
- INTRO (1.2 s, any input skips without feeding) → PLAY → CELEBRATE (1.8 s) → PLAY with a new request.
- A friendly original monster asks for **1–3 snacks**. The request is chosen through `ctx.rng` (deterministic per seed) and never repeats back-to-back, so 1, 2 and 3 all appear.
- Visual count works with audio off:
  - the top-bar request badge shows N dots
  - the monster's tummy lights one dot per snack eaten
- Each snack plays a `SUCCESS` cue with variant = running count, so the voice counts "one… two… three" aloud.
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
| Number buttons 1/2/3 (dots) | `{8,176,96,56}` `{112,176,96,56}` `{216,176,96,56}` | count along to N |
| Request badge (top bar) | `{132,4,56,56}` | shows N dots; tap replays request |

## Input map (contexts: `Counting`; SceneManager adds `Global`)
| Action | Touch | Voice intent |
|---|---|---|
| Feed one snack | tap food bowl | — |
| Count along to N | tap number button N | ONE / TWO / THREE |
| Hear/see request again + hint | tap monster or request badge | AGAIN, HELP |
| Home | Home button (SceneManager) | BACK (SceneManager) |

## Cue → asset placeholders (to be declared in `assets/manifest.json`)
| Cue | variant | Placeholder asset id |
|---|---|---|
| PROMPT | requested count 1–3 | `feed_monster.prompt.{one,two,three}` ("Feed me N!") |
| TAP | 0 | `common.tap` |
| SUCCESS | running count 1–3 | `feed_monster.count.{one,two,three}` + munch |
| CELEBRATE | 0 | `common.celebrate` |
| REACTION | 0 | `feed_monster.giggle` |

## Tests
`feed_monster_test` (host only) covers:
- layout safety
- intro timing and intro-skip without feeding
- counts 1–3 with variety, visual and spoken count, and no back-to-back repeat
- excess taps never penalised
- voice/food/number-button equivalence
- counting along by voice
- overshoot number
- input and voice during celebration
- AGAIN/HELP/monster/badge replay
- one-shot idle hint
- harmless empty taps and unrelated intents
- chew animation not dropping taps
- audio disabled (`NullFeedback`)
- exit mid-animation and fresh re-entry
- same-seed determinism
- playing through the `SceneManager`: launch with `COUNTING`, out-of-context GO/RED filtered, Home returns to MENU
