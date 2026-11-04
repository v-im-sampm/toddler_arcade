# Color Pop (G01)

Spec: [games/color_pop.md](../../../../games/color_pop.md). Status: **scene logic + host tests only**.
There is no LVGL view yet, no assets yet (A01), and it has not run on a device.

## Behaviour
- INTRO (1.2 s, any input skips) → PLAY → CELEBRATE (1.5 s) → PLAY with a new round.
- Each round shows 2 bubbles, growing to 4. Colours are distinct, positions are shuffled over 6 fixed 72×72 slots, and the target never repeats back-to-back.
- Each colour has its own pattern (`pattern_for`): RED heart, BLUE wave, GREEN leaf, YELLOW star. The target badge at the top centre shows the requested colour and pattern, so the game works with audio off.
- A wrong bubble wiggles, the target hint glows and the prompt replays. Nothing is lost, and there's no failure state.
- Input during CELEBRATE is ignored, which makes repeated taps and voice-during-animation safe.

## Input map
| Action | Touch | Voice intent |
|---|---|---|
| Choose colour | tap bubble | RED / BLUE / GREEN / YELLOW |
| Hear request again + hint | tap target badge | AGAIN, HELP |
| Home | Home button (SceneManager) | BACK (SceneManager) |

## Cue → asset placeholders (to be declared in `assets/manifest.json` by A01)
| Cue | variant | Placeholder asset id |
|---|---|---|
| PROMPT | colour index | `color_pop.prompt.{red,blue,green,yellow}` |
| TAP | 0 | `common.tap` |
| SUCCESS | colour index | `color_pop.pop` |
| CELEBRATE | 0 | `common.celebrate` |
| TRY_AGAIN | 0 | `common.try_again_soft` |

## Tests
`color_pop_test` (host only) covers:
- layout size and spacing
- intro and prompt behaviour
- round invariants and growth
- randomised and deterministic layouts
- voice/touch equivalence
- gentle wrong answers
- input during celebration
- audio disabled
- Home pressed mid-animation
- playing through the `SceneManager`
