# Silly Faces (G08)

Spec: [games/silly_faces.md](../../../../games/silly_faces.md). Status: **host only. This folder has scene logic and host tests only.**
There is no LVGL view yet and there are no assets yet. It has not run on a device. Test results come from desktop host builds and are not device validation.

## Behaviour
- **States:** INTRO (1.2 s; any input skips it and doesn't change the face) → PLAY → CELEBRATE (1.5 s) → PLAY. The SceneManager handles PAUSED and EXIT. Each entry starts fresh, with no saved profile.
- **Emotions:** There are four: HAPPY, SLEEPY, SURPRISED and SAD. Each eye style and each mouth style belongs to one emotion. Tapping the eyes or the mouth moves that part to the next style, and it wraps around after four taps.
- **Matching:** When the eyes and mouth match, the monster acts out that feeling (CELEBRATE) and a sticker for that emotion lights up (`emotions_made()`). The first time all four stickers are lit, there is one bigger celebration.
- **No grading:** Every face is fine. If the child asks the monster to show a mixed face, it gives a happy giggle. The game never plays TRY_AGAIN, has no failure state and never counts anything against the child. SAD and SLEEPY are acted out just as warmly as HAPPY.
- **Mix:** This button switches to a different mixed face, which gives the child something to fix. **New monster** changes the body colour and the topper, and also switches to a mixed face.
- **Idle hint:** After 8 s without input, the eyes and mouth glow. This hint has no sound, no penalty and no time limit.
- **Input during CELEBRATE:** It's ignored, so repeated taps and voice input during the animation are safe.
- **Original design:** The monster is a round blob body in one of 4 colours. It has one of 3 toppers (antennae, little horns or a fuzzy tuft) and swappable cartoon eyes and mouth. It doesn't use licensed characters, and there is no camera.

## Layout (320×240)
| Target | Rect | Purpose |
|---|---|---|
| `kEyesZone` | {88, 68, 144, 64} | cycle eyes |
| `kMouthZone` | {88, 144, 144, 64} | cycle mouth |
| `kShowButton` | {8, 72, 64, 64} | "show me!" (YES) |
| `kMixButton` | {8, 152, 64, 64} | mix it up (NO) |
| `kNewMonsterButton` | {248, 72, 64, 64} | new monster (NEXT) |
| `kPromptBadge` | {132, 4, 56, 56} | top bar; replay prompt (AGAIN/HELP) |

All targets are at least 48×48 and sit at least 8 px apart. None of them overlap Home {0,0,56,56} or Mic {264,0,56,56}.

## Input map
`active_contexts()` = `Context::General`. The SceneManager adds Global.
| Action | Touch | Voice intent |
|---|---|---|
| Change eyes | tap eyes | — |
| Change mouth | tap mouth | — |
| Act out current face | Show button | YES |
| Mix up the face | Mix button | NO |
| New monster | New-monster button | NEXT |
| Hear prompt again + glow hint | prompt badge | AGAIN, HELP |
| Home | Home button (SceneManager) | BACK (SceneManager) |

## Cue → asset placeholders (to be declared in `assets/manifest.json`)
| Cue | variant | Placeholder asset id | Visual counterpart |
|---|---|---|---|
| PROMPT | 0 | `silly_faces.prompt.make_a_face` | eyes + mouth glow (`hint_active`) |
| TAP | 0 eyes / 1 mouth / 2 new monster / 3 mix | `common.tap`, `silly_faces.boing`, `silly_faces.pop_in`, `silly_faces.shuffle` | part bounce (`bouncing`) |
| REACTION | 0..3 emotion | `silly_faces.react.{happy,sleepy,surprised,sad}` | act-out animation (`showing`) |
| REACTION | 4 (`kSillyReaction`) | `silly_faces.react.giggle` | giggle wobble (`showing_silly`) |
| CELEBRATE | 0 / 1 (all four) | `common.celebrate`, `silly_faces.all_faces` | sparkles + sticker row |

## Tests
`silly_faces_test` (host only) covers:
- layout safety for every target
- intro flow and the skip that doesn't change the face
- cycling through all 4 emotions, and making each emotion
- the one-time all-four celebration
- no grading: mixed faces giggle, and there's never a TRY_AGAIN
- mix and new monster always give a different face or colour
- voice/touch equivalence for YES, NO, NEXT, AGAIN and HELP
- repeated taps and voice input during celebration
- harmless empty taps and unrelated intents
- the idle hint is visual only
- playing with audio disabled (NullFeedback)
- exit() mid-animation, then a fresh re-entry
- the same seed always gives the same result
- playing through the SceneManager:
  - FACES launches the game
  - GO and RED are filtered out
  - a Home tap returns to MENU
