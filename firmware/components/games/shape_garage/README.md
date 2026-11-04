# Shape Garage (G05)

Spec: [games/shape_garage.md](../../../../games/shape_garage.md). Status: **scene logic + host tests only**.
There is no LVGL view yet, no assets yet, and it has not run on a device.

## Behaviour
- INTRO (1.2 s, any input skips it and doesn't count as a move) → PLAY → CELEBRATE (1.8 s robot dance) → PLAY with a new robot.
- The robot's chest has 3 shaped slots: CIRCLE, SQUARE and TRIANGLE. The tray at the bottom holds the 3 matching parts. Slot order and tray order are shuffled each round using `ctx.rng`, so the same seed always gives the same layout. The robot colour style cycles through 3 looks.
- **Tap-to-place:** tap a part to lift it (it rises and appears on the robot head), then tap a slot. Dragging isn't supported or needed.
- **Slot first also works:** tapping an empty slot lifts its matching part and makes the slot glow, so a second tap on that slot places the part.
- **Forgiving matching:**
  - Each slot accepts taps up to 6 px outside its drawn edge.
  - A wrong slot makes the part wiggle and the correct slot glow, then replays the shape prompt. The child keeps holding the part, and nothing is lost.
  - On a second miss with the same part, it slides into its own slot by itself.
- Tapping the lifted part again puts it back. Tapping a filled slot only plays the tap sound.
- Filling all 3 slots repairs the robot, which leads to CELEBRATE. Input is ignored during CELEBRATE, so repeated taps or voice commands can't double-count.
- There's no failure state, no fail timer and no score.

## Input map
Context: `General` (YES, NO). The SceneManager adds `Global`, which brings in AGAIN, HELP and NEXT. BACK, PAUSE and RESUME are handled by the SceneManager.

| Action | Touch | Voice intent |
|---|---|---|
| Lift a part | tap part in tray (or tap an empty slot) | YES with nothing held lifts the first part still in the tray |
| Place the lifted part | tap its slot | YES places it in its matching slot |
| Put the part back | tap the lifted part again | NO |
| Hear the prompt again + hint glow | tap robot head (`kRobotHead`) | AGAIN, HELP |
| Home | Home button (SceneManager) | BACK (SceneManager) |

## Layout (320×240)
| Target | Rect(s) |
|---|---|
| Robot chest slots | `{48,72,64,64}` `{128,72,64,64}` `{208,72,64,64}`; hit area grows by 6 px on each side |
| Tray parts | `{48,164,64,64}` `{128,164,64,64}` `{208,164,64,64}` |
| Robot head (top bar, optional) | `{132,4,56,56}` |

## Visual counterparts (audio may be off)
| Accessor | Shows |
|---|---|
| `selected()` | lifted part, also drawn on the robot head |
| `wiggling()` | gentle "not there" wiggle |
| `hint_slot()`, `hint_piece()` | soft glow |
| `snapping()` | part sliding into its slot |
| `filled_count()` | robot eyes light up, one per filled slot |
| `phase() == CELEBRATE` | robot dance (no rapid flashing) |

## Cue → asset placeholders (to be declared in `assets/manifest.json`)
| Cue | variant | Placeholder asset id |
|---|---|---|
| PROMPT | 0 (`kPromptFixRobot`) | `shape_garage.prompt.fix_robot` |
| PROMPT | 1 + shape (`prompt_for`) | `shape_garage.prompt.{circle,square,triangle}` |
| TAP | 0 | `common.tap` |
| SUCCESS | shape index | `shape_garage.clunk.{circle,square,triangle}` |
| TRY_AGAIN | 0 | `common.try_again_soft` |
| CELEBRATE | 0 | `shape_garage.robot_dance` |

Sprites (original art, not created yet): `shape_garage.robot.{0,1,2}`, `shape_garage.part.{circle,square,triangle}`, `shape_garage.slot.{circle,square,triangle}`.

## Tests
`shape_garage_test` (host only) covers:
- layout safety, including the forgiving hit areas
- the intro and its skip
- 3 distinct shapes every round
- tap-to-place
- forgiving taps on slot edges
- slot-first placement
- a gentle wrong answer followed by the assist
- putting a part back by touch and by voice
- voice/touch equivalence, including YES matching the tap sequence
- repeated input during celebration
- repeated taps during play
- voice during the snap animation
- AGAIN, HELP and robot-head prompt replay
- harmless empty taps and unrelated intents
- play with audio disabled (`NullFeedback`)
- exiting mid-animation, then re-entering fresh
- same seed gives the same result
- layouts vary across seeds
- playing through the `SceneManager` (SHAPES launch, out-of-context intents filtered, Home)

## Not covered
- **Optional drag:** `InputEvent` has no press, move or release phases, so dragging can't be told apart from a tap. Tap-to-place covers everything the spec requires.
- **PAUSED and EXIT:** these are owned by the SceneManager.
