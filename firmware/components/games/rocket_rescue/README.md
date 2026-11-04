# Rocket Rescue (G07)

Spec: [games/rocket_rescue.md](../../../../games/rocket_rescue.md). Status: **scene logic + host tests only**.
There is no LVGL view yet and no assets yet. Cue ids below are placeholders. The game has not run on a device.

## Behaviour
- INTRO (1.5 s; any input skips it and does not launch) → PLAY → CELEBRATE (1.8 s) → PLAY. `PAUSED`/`EXIT` come from the `SceneManager` (it stops calling `update()` while paused; Home calls `exit()`).
- There are **3 planets**. Each has its own surface pattern (`Look`: RINGED / SPOTTED / STRIPED), so you can tell them apart without colour, and each has an original friend waiting (MOON_PUP / STAR_BUNNY / COMET_KITTY). Looks are shuffled once per entry. Friends and the suggested visiting order are shuffled again for each mission.
- The rocket starts on the launch pad. **GO** flies to the suggested planet, which is shown on the prompt badge. Tapping a planet flies there directly. Each trip is a gentle 2.5 s glide that takes the same time over any distance, so the rocket moves at most a few px per frame.
- **STOP** makes the rocket hover in place. It stays there with no time limit until GO, or until the child taps the same planet again. Tapping another planet mid-flight turns the rocket smoothly from where it is, with no teleport.
- Reaching a planet whose friend is waiting rescues the friend (SUCCESS + CELEBRATE). Once all 3 are rescued there is a bigger celebration, and a new mission starts with fresh friends. The first suggested planet is never the one the rocket is sitting on.
- Visiting a planet that is already rescued, or tapping the planet the rocket is on, makes the friend wave (REACTION). That's all; it doesn't start another rescue.
- 3 collectible stars sit in 5 fixed slots. Tapping one makes it twinkle away, adds to a running count and brings it back in a different free slot after 0.9 s. Stars never affect the rocket.
- **There is no timed failure.** Nothing crashes, runs out of fuel, times out or is lost. Input during CELEBRATE is ignored, which makes repeated taps and voice-during-animation safe. Repeated GO or STOP presses while already flying or hovering do nothing.

## Layout (320×240)
| Target | Rect (x,y,w,h) |
|---|---|
| Planets 0/1/2 | (16,68,56,56) (132,68,56,56) (248,68,56,56) |
| Star slots ×5 | y=132, 48×48, x=16/80/144/208/272 |
| GO button | (16,188,96,48) |
| STOP button | (208,188,96,48) |
| Prompt badge (top bar, optional) | (132,4,56,56) |
| Launch pad (drawn only) | point (160,212) |

## Input map (`Context::Space`)
| Action | Touch | Voice intent |
|---|---|---|
| Launch / keep going | GO button, or tap a planet | GO |
| Hover | STOP button | STOP |
| Fly to a specific planet | tap planet | — (touch only) |
| Collect star | tap star | — (touch only) |
| Hear prompt again + hint glow | tap prompt badge | AGAIN, HELP |
| Home | Home button (SceneManager) | BACK (SceneManager) |

## Cue → asset placeholders (to be declared in `assets/manifest.json`)
| Cue | variant | Placeholder asset id | Visual counterpart |
|---|---|---|---|
| PROMPT | planet index | `rocket_rescue.prompt.planet{0,1,2}` | prompt badge shows planet look + friend; planet glows while hint is active |
| TAP | 0 | `common.tap` | button/planet press-down |
| NOTE | 0 twinkle / 1 launch / 2 hover | `rocket_rescue.star_twinkle`, `rocket_rescue.launch`, `rocket_rescue.hover` | star sparkle; rocket flame; rocket bobbing in place |
| SUCCESS | friend index | `rocket_rescue.rescue.{moon_pup,star_bunny,comet_kitty}` | friend hops into the rocket |
| CELEBRATE | 0 rescue / 1 mission | `common.celebrate`, `rocket_rescue.mission_complete` | slow confetti / stars (no rapid flashing) |
| REACTION | friend index | `rocket_rescue.wave.{moon_pup,star_bunny,comet_kitty}` | friend waves (`waving()`) |

## Tests
`rocket_rescue_test` (host only) covers:
- layout safety: sizes, spacing, Home/Mic clearance, play area and top-bar band
- three distinct planets
- intro and prompt behaviour, and an intro skip that doesn't launch
- a full 3-planet mission with gentle motion, followed by a new mission
- no timed failure: an hour idle and an hour hovering
- GO/STOP touch and voice give the same result
- tap-to-fly, smooth mid-flight turns, friend waves and revisits
- tapping the hover planet resumes the flight
- stars: collection, respawn and no shared slots
- repeated input and voice during flight and celebration
- AGAIN, HELP and the badge
- harmless empty taps and unrelated intents
- a full touch-only mission with audio disabled (`NullFeedback`)
- `exit()` mid-flight and mid-celebration, then a fresh re-entry
- the same seed always plays the same way, and different seeds give different missions
- playing through the `SceneManager` (`Intent::SPACE` launch, out-of-context intents filtered, Home tap)
