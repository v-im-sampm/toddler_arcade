# Robo Buddy (G04)

Spec: [games/robo_buddy.md](../../../../games/robo_buddy.md). Status: **scene logic, original LVGL view, host tests, and launcher integration implemented**. It has not been validated on physical hardware.

## Character
"Bolt" is an original robot design. It has a round teal body, one bendy antenna, two big round screen-eyes, a belly dial and stubby spring arms. It isn't based on any existing character. The view and launcher tile are procedural and use no external assets.

## Behaviour
- **States:** INTRO (1.5 s, the robot powers on and waves; any input skips it) → PLAY → CELEBRATE (reaction, 1.2 s, or a dance, 2 s) → PLAY. There is also an in-game **SLEEP** nap state: PLAY → SLEEP → (wake) CELEBRATE → PLAY. The SceneManager handles PAUSED and EXIT. This nap has nothing to do with the system sleep.
- **Moods:** each mood has a distinct face shape, so you can tell them apart without colour or audio:
  - CALM: round eyes and a small smile.
  - HAPPY: crescent eyes and the antenna bobs. Pet the head.
  - SILLY: a wink, a tongue and a wobble. Tickle the belly.
  - YUMMY: heart eyes and munching. Feed a snack.
  - SLEEPY: closed eyes and floating Z bubbles. Tap the sleep button.

  After 4 s with no input in PLAY, the mood relaxes back to CALM.
- **Reactions:** each reaction picks one of 3 poses with `ctx.rng`. Repeating the same action never repeats the same pose back-to-back. Every 5th reaction becomes a longer dance with a celebrate cue.
- **Food button:** shows a snack (bolt cookie, battery bar or gear gummy). After each feed it changes to a different snack.
- **Sleep and wake:**
  - The sleep button starts a nap. During the first 0.8 s yawn, input is ignored, so a double tap doesn't wake the robot straight away.
  - After that, any tap on the robot, the food button or the sleep button wakes it with a stretch. YES, NO and AGAIN wake it too.
  - Taps on empty space don't wake it. The hello button shows the "tap me to wake me" hint.
  - The nap never times out and nothing is lost.
- **Forgiving play:** there are no failure states and no TRY_AGAIN cue. Empty taps and unrelated intents do nothing. Input during a reaction is ignored, so repeated taps and voice during an animation are safe.

## Layout (320×240)
| Target | Rect (x, y, w, h) |
|---|---|
| Head (pet) | 104, 64, 112, 72 |
| Belly (tickle) | 104, 144, 112, 88 |
| Food button | 12, 128, 80, 80 |
| Sleep/wake button | 228, 128, 80, 80 |
| Hello button (top bar) | 132, 0, 56, 56 |

## Input map (`active_contexts()` = General; SceneManager adds Global)
| Action | Touch | Voice intent |
|---|---|---|
| Pet (HAPPY) | tap head | YES |
| Tickle (SILLY) | tap belly | NO |
| Feed (YUMMY) | tap food button | — |
| Sleep | tap sleep button | — (no SLEEP intent exists) |
| Wake | tap robot, food or sleep button | YES, NO, AGAIN |
| Repeat last action | tap the same part again | AGAIN |
| Hint and replay prompt | tap hello button | HELP |
| Home | Home button (SceneManager) | BACK (SceneManager) |

## Cue → asset mapping
| Cue | Variant | Asset id | Visual counterpart |
|---|---|---|---|
| PROMPT | 0 | `robo_buddy.prompt.play` | action buttons outline during a hint |
| PROMPT | 1 | `robo_buddy.prompt.wake` | sleep controls outline during a hint |
| TAP | 0 | `common.tap` | tap sound; reaction follows an accepted action |
| REACTION | 1 / 2 / 3 / 4 | `robo_buddy.chirp.{happy,silly,yummy,sleepy}` | mood face |
| REACTION | 5 | `robo_buddy.chirp.wake` | wake/stretch reaction |
| NOTE | pose 0–2 | `robo_buddy.beep.{0,1,2}` | pose variation |
| CELEBRATE | 0 | `common.celebrate` | dance |
| AMBIENT | 0 | `robo_buddy.snore` | floating Z bubbles |

Sound cues are procedural and remain subject to the shared muted-by-default volume cap. Spoken prompt recordings are not included yet. Asset IDs and procedural sources are listed in [assets/manifest.json](../../../../assets/manifest.json).

## Tests
`robo_buddy_test` (host only) covers:
- **Layout:** size, spacing, Home/Mic clearance, play area and top-bar band.
- **Intro:** the prompt, and the intro skip.
- **Moods:** three or more distinct moods.
- **Sleep and wake:** doze guard, empty taps, hint, wake by touch and by voice.
- **Mood relax:** the mood returns to CALM when idle.
- **Voice and touch equivalence:** YES/NO/AGAIN against head, belly and repeat taps.
- **Hello and HELP:** the hello button behaves the same as HELP.
- **Variety:** poses vary and the snack changes.
- **Dance:** every 5th reaction.
- **Input safety:** repeated taps and voice during a reaction, empty taps and unrelated intents.
- **Audio disabled:** play still works with `NullFeedback`.
- **Lifecycle:** exit mid-reaction and while asleep, then re-enter fresh.
- **Determinism:** the same seed gives the same results.
- **SceneManager:** launch with ROBOT, out-of-context voice is filtered, Home at (10,10).

`robo_buddy_art_test` verifies the original launcher tile art. The desktop simulator builds and benchmarks the LVGL view; its output is host-only and is not hardware validation.
