# Rainbow Piano (G03)

Spec: [games/rainbow_piano.md](../../../../games/rainbow_piano.md). Status: **scene logic, LVGL view, procedural sound bank and launcher tile implemented**.
The [browser mockup](../../../../docs/mockups/rainbow_piano.html) is the visual reference. The view and game are wired into the shared Arcade and desktop simulator. The original sound cues are synthesized in the sound bank; no recorded samples are used. This has not been run on a physical device. The real <100 ms touch-to-sound path has not been measured on hardware.

## Behaviour
- INTRO (1.2 s, then a short synthesized prompt motif) → PLAY → CELEBRATE (1.5 s) → PLAY with a new tune.
- Any input during INTRO skips the intro and still does what it was meant to do. A key tap plays its note immediately, and the prompt is skipped so it doesn't talk over the child.
- **Free play:** there are five big rainbow keys (red→purple, low→high), and each key also has its own shape (circle, square, triangle, heart, star). Every key press calls `Cue::NOTE` synchronously inside `handle()` in **every** phase, with no deferral and no lockout. The key glows for 300 ms as the visual counterpart. Pressing a key again restarts the glow, so it doesn't flash.
- **Three instruments:** bells, flute and drum, picked with three touch buttons. The selected one is highlighted. `NOTE` variant = `instrument * 5 + key` (15 distinct sounds) and each has a different procedural timbre.
- **Follow-light (optional, never required):** a soft light marks the next key of a random 5-note tune, and consecutive notes always differ. Pressing the lit key moves the light along. Finishing the tune triggers CELEBRATE, and then a new tune starts. Other keys still just play their notes, so there's no wrong answer, no TRY_AGAIN, and no timer.
- Taps during CELEBRATE still play notes and can still change the instrument. They don't count toward a tune, so repeated taps or voice can't trigger a second celebration.
- `exit()` clears all state, so stray `update()`/`handle()` calls do nothing. `enter()` always starts fresh: bells, INTRO, a new tune.

## Layout (320×240, all ≥48 px, ≥8 px apart, all in play area y ≥ 64)
| Target | Rect(s) |
|---|---|
| Instrument buttons (bells, flute, drum) | `{60,64,56,48}`, `{132,64,56,48}`, `{204,64,56,48}` |
| Keys 0–4 | `{4+64i, 120, 56, 112}` |

## Input map
| Action | Touch | Voice intent |
|---|---|---|
| Play a note | tap key | — |
| Play the last note again (or the lit key if none yet) | tap that key | AGAIN |
| Change instrument | tap instrument button | — |
| Home | Home button (SceneManager) | BACK (SceneManager) |

`active_contexts()` returns `0`. The only voice action the game uses is AGAIN, which is Global and is added by the SceneManager. HELP, colour and other intents are ignored.

## Cue → sound bank and manifest
| Cue | variant | Placeholder asset id | Visual counterpart |
|---|---|---|---|
| PROMPT | 0 | `rainbow_piano.prompt.play` | rainbow header |
| NOTE | `inst*5+key` (0–14) | `rainbow_piano.{bells,flute,drum}.{0..4}` | key glow (`key_lit`) |
| TAP | instrument index | `rainbow_piano.select.{bells,flute,drum}` | selected instrument highlight |
| CELEBRATE | 0 | `common.celebrate` | three non-flashing stars; follow-light pauses |

These are original procedural sounds registered in [`assets/manifest.json`](../../../../assets/manifest.json) and implemented in `audio/sound_bank.cpp`.

## Validation status
- Host suite: 22/22 tests pass, including scene logic, key/instrument art and sound-bank safety.
- Desktop simulator: Release build passes; a 20-second seeded run completed five tunes. Peak LVGL heap was 54,768 bytes on the host.
- The simulator is desktop-only and does not establish hardware latency, acoustic safety or device memory headroom.
- Device build and physical latency/audio measurements are still pending.

## Tests
`rainbow_piano_test` (host only) covers:
- layout safety for every target rect
- the intro and prompt
- a key tap during the intro playing immediately
- every key emitting NOTE synchronously, plus the glow
- 3 instruments producing 15 distinct variants
- follow-light completion and celebration
- off-light keys never being punished
- repeated taps and voice during celebration
- AGAIN matching touch
- empty taps and unrelated intents doing nothing
- audio disabled (NullFeedback), plus distinct key shapes
- exit mid-celebration followed by a fresh re-entry
- same-seed determinism and per-seed variety
- play through the SceneManager: MUSIC launches the game, an out-of-context RED is filtered out, AGAIN reaches the game, and Home returns to MENU
