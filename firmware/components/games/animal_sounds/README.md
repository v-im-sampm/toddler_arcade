# Who Made That Sound? / Animal Sounds (G06)

Spec: [games/animal_sounds.md](../../../../games/animal_sounds.md). The host-testable scene, LVGL view,
original vector illustrations, recorded animal-call bank (CC0 clips), and launcher tile are implemented.
Status: **done** (accepted by the owner after on-device play-testing on 2026-11-04).
Formal hardware validation is still pending: no measured PSRAM/minimum-heap figures, speaker level checks or test logs exist yet.
Host and desktop-simulator results are not device validation.

## Behaviour
- INTRO (1.5 s, any input skips without counting as an answer) → PLAY → CELEBRATE (1.8 s) → PLAY with a new round.
- PLAY starts with a short question motif. About 450 ms later (`kCallAfterCueMs`, so the motif can't mask it) the target animal's recorded call plays. After a wrong answer, the call is also delayed after the soft try-again hum. Replays from the speaker button, AGAIN or HELP are immediate. The scene never blocks while audio plays. `sound_playing()` is only a visual cue timer. Spoken prompt recordings/TTS are not included.
- There are 8 original cartoon animals: cow, duck, sheep, pig, dog, cat, frog and owl. Each round shows 2 cards, growing to 4. Cards always show distinct animals. Positions are shuffled over 6 fixed 96×80 slots. The target never repeats back-to-back. Card count never shrinks as a penalty.
- Each animal has an original illustrated portrait and a unique visual clue (`clue_for`): hoofprint, webbed foot, wool cloud, curly tail, bone, whiskers, lily pad and feather. The clue shows on the speaker button while the sound plays, so the game remains usable with audio off.
- A wrong card does a gentle wiggle. The correct card glows as a hint, and the sound replays. There are no timers, no lost progress and no failure state.
- Input during CELEBRATE is ignored, which makes repeated taps and voice-during-animation safe. `exit()` clears all state. Later `update()`/`handle()` calls do nothing, and `enter()` starts fresh.

## Layout (320×240)
| Target | Rect |
|---|---|
| Card slots | `{8,68,96,80} {112,68,96,80} {216,68,96,80} {8,156,96,80} {112,156,96,80} {216,156,96,80}` |
| Speaker / replay button | `{132,4,56,56}` (top bar, between x=64 and x=256) |
| Home / Mic | owned by the SceneManager (`kHomeButton`, `kMicButton`) |

The arcade launcher adds Animal Sounds on page 2. Image assets are painted once at view creation and
held in ARGB8888 buffers: 8 × 80 × 64 animal portraits, 8 × 20 × 20 clue icons, and one 108 × 68
tile, approximately **206,016 bytes (201.2 KiB)** total. This is a desktop calculation; actual PSRAM
availability and minimum heap remain to be measured on the board.

The ESP-IDF integration build produced an **822,032-byte app binary** (`0xC8B10`), with 80% of
the 4 MiB app partition free. The whole-firmware linker report used 166,071 of 341,760 bytes of
DIRAM (48.59%), including 91,752 bytes of `.bss`; these are not G06-only deltas. After the
recorded calls were added, the app binary is **1,084,640 bytes** (`0x108CE0`, +262,608 bytes,
about 74% of the app partition free). The clips live in flash `.rodata` and add no RAM. No physical-board
PSRAM or minimum-heap measurement has been made.

## Input map
| Action | Touch | Voice intent |
|---|---|---|
| Choose animal | tap card | — (no animal-name intents exist in `config/commands.json`) |
| Replay sound + hint | tap speaker button | AGAIN, HELP (Global) |
| Home | Home button (SceneManager) | BACK (SceneManager) |

`active_contexts()` returns `0` because no animal context exists. The SceneManager adds Global.

## Cue → assets (`assets/manifest.json`)
| Cue | variant | Asset id |
|---|---|---|
| PROMPT | 0 | `animal_sounds.prompt.question_motif` (nonverbal) |
| NOTE | animal index | `animal_sounds.sound.{cow,duck,sheep,pig,dog,cat,frog,owl}` (recorded CC0 clip, see below) |
| TAP | 0 | `common.tap` |
| SUCCESS | animal index | `animal_sounds.happy.{animal}` |
| CELEBRATE | 0 | `common.celebrate` |
| TRY_AGAIN | 0 | `common.try_again_soft` |

Visual counterparts: PROMPT/NOTE → speaker sound-waves + clue; TAP/SUCCESS/CELEBRATE → card bounce (`celebrating()`); TRY_AGAIN → wiggle (`wiggling()`) + target glow (`hint_active()`).

## Animal call recordings
The first on-device test showed that synthesized calls were not recognizable as their animals. They were replaced with short real recordings. Every clip is **CC0 1.0 (public domain)** by Joseph Sardin, [BigSoundBank.com](https://bigsoundbank.com/licenses.html). Provenance is recorded in [sources.json](../../../../assets/audio/animal_sounds/sources.json): source page, download URL, SHA-256 and trim window. The pig is a human mouth imitation of a grunting pig, because no CC0 real-pig recording was found. No copyrighted character audio is used.

- `python tools/make_animal_clips.py [--update-hashes]` (needs ffmpeg) downloads the sources at dev time. It verifies their hashes, trims the call, band-limits it to 150 Hz–7 kHz for the small speaker, normalises it to a 0.88 FS peak, fades the edges, and writes 16 kHz mono 16-bit WAVs to `assets/audio/animal_sounds/`.
- At configure time, `firmware/components/audio/animal_clips.cmake` turns the WAVs into a flash-resident `int16_t` table. Nothing is decoded or fetched at runtime.
- The synth plays clips with 3 ms edge ramps at `kAnimalCallLevel` (190/255) through the normal volume limiter.
- The eight clips are 0.65–1.15 s each, with 254,150 bytes of PCM in flash.

## Tests
`animal_sounds_test` (host only) covers:
- layout safety: size, on-screen, play area, spacing, and no Home/Mic overlap
- at least 6 animals with distinct clues
- intro, prompt and sound sequence; no timeout
- an intro tap skips without answering
- round invariants, growth from 2 to 4 cards, no repeat target, and every animal appears
- randomised and deterministic layouts
- replay by touch, AGAIN and HELP is equivalent
- gentle, retryable wrong answers
- repeated input during celebration
- empty taps and unrelated intents are harmless
- audio disabled (NullFeedback)
- `exit()` mid-celebration and a fresh re-entry
- playing through the `SceneManager`: ANIMALS launch, out-of-context intents filtered, AGAIN reaching the game, and Home returning to MENU
- `animal_sounds_art_test` checks that all eight portraits and clues paint distinct, non-empty images and that launcher tile art is present.
- `audio_synth_test` checks that all eight animal calls use distinct clips within `kMaxSoundMs`. It checks that every clip is 0.3–1.2 s, peaks ≤ 30000, is audible (RMS > 1000) and starts and ends near silence. It also checks that clip playback ramps its edges.
