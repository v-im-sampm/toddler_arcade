# Sound Jar (toddler sampler)

Spec: [games/sound_jar.md](../../../../games/sound_jar.md). PRD: [docs/prd/sound_jar.md](../../../../docs/prd/sound_jar.md).
Mockup: [docs/mockups/sound_jar.html](../../../../docs/mockups/sound_jar.html).
Status: **implemented; host and desktop-sim tested**. Not yet played on the physical board. The
INMP441 mic wiring is owner-reported, not bench-verified (`board::audio::kWiringBenchVerified = false`).
Host and desktop-simulator results are not device validation.

## Behaviour
- INTRO (1.2 s, spoken prompt; any input skips it and still acts) → PLAY ⇄ RECORDING (≤ 3 s) →
  CELEBRATE (1.5 s, stars, plays the new sound once) → PLAY.
- A built-in "boing" is in the jar from the start, so every pad works before any recording.
- Record: tap the red dot, make a noise, tap the stop square (or wait 3 s). The speaker is silent
  while recording. A fill bar shows time and the button's outline grows with loudness.
- The engine trims leading and trailing silence, normalizes the kept sound to −6 dBFS and adds
  5 ms fades. The trim works on 5 ms windows:
  - Sound must stay above a threshold for 15 ms, so screen-tap pops and clicks around the sound are
    cut too.
  - The threshold is 1/10 of the loudest sustained part, raised to 2× the quietest window in a
    noisy room.
  - It keeps 20 ms before the sound, 80 ms after it, and any pauses inside it.
  A take that is too quiet (no sustained sound ≥ 512) or too short (< 150 ms) keeps the old sound
  and the jar wiggles. A take with only a click counts as too quiet. There is no failure sound and
  no text.
- Pads: big → tiny creature = −12, −5, 0, +7, +12 semitones. Tap = one shot. Hold > 350 ms = loop
  until lifted. Hold and slide sideways (12 px dead zone) = continuous pitch bend, shown by a dot.
- Effects (toggles, stackable): Backwards, Echo, Robot, Wobble. Each tap previews the sound.
- No mic (hardware missing, audio failed, or parent turned it off): the red dot becomes a sparkle
  that cycles three built-in sounds (boing, quack, drum). Without any sampler, pads fall back to
  `Cue::NOTE` tones so the game still responds.
- Voice: AGAIN replays the last pad with the current effects, HELP replays the prompt. Voice never
  starts recording. The launcher tile has no spoken name.

## Privacy
- One clip lives in PSRAM. It is never written to flash/NVS, logged or sent anywhere, and never
  reaches ESP-SR.
- The raw take buffer is zeroed as soon as it has been processed.
- `wipe()` zeroes everything on game exit, pause/sleep and parent-gate entry
  (`Arcade::tick` → `SoundJarGame::privacy_wipe`).
- The parent screen has a **Mic** toggle (NVS key `mic`, default on). It is applied through
  `Sampler::set_mic_allowed`.

## Layout (320×240)
| Target | Rect |
|---|---|
| Record | `{60,0,64,56}` |
| Jar (waveform + playhead, tap = play) | `{128,0,132,56}` |
| Effects | `{4+80i, 64, 72, 52}`, i = 0..3 |
| Pads | `{4+64i, 124, 56, 112}`, i = 0..4 |
| Home / Mic | owned by the SceneManager (`kHomeButton`, `kMicButton`) |

The launcher shows Sound Jar on page 2, slot 4.

## Architecture
- `little_arcade::ISampler` (core) is all the game sees.
- `audio::Sampler` (`components/audio/sampler.cpp`) is the engine:
  - UI-task calls push into a lock-free SPSC queue.
  - `capture()`/`mix()` run on the audio task, with no heap and no floats.
  - `AudioFeedback::render()` mixes the sampler before the existing limiter and volume cap.
- `main.cpp` allocates the three buffers in PSRAM and attaches the sampler to the audio task, the
  feedback mixer and the arcade.
- `Arcade::pointer` forwards raw touch to `SoundJarGame::pointer` for hold and slide.

## Memory and size
| Item | Bytes | Where | Evidence |
|---|---|---|---|
| Clip + raw take + echo buffers | 200 000 | PSRAM | desktop calculation |
| View rasters (5 critters 50², 7 icons 36², star 28², jar 112×34; ARGB8888) | 104 656 | PSRAM | desktop calculation |
| `libsound_jar.a` | 13 404 flash | flash | ESP-IDF size report |
| LVGL heap max_used, sim bench | 96 912 vs 93 760 (Robo Buddy) | LVGL pool | desktop sim |
| LVGL heap at boot (menu) | used 52 948, max 60 392 of 123 236 | LVGL pool | physical device, boot only |
| Internal heap after audio start | free 140 107, min 75 376 | internal | physical device, boot only |

The ESP-IDF image with Sound Jar is **1,107,984 bytes**, with 74% of the 4 MiB app partition free.
With the default 64 KB LVGL pool the device hung on its first frame: the 24 KB draw layer could not
be allocated, so the pool is now 128 KB (`CONFIG_LV_MEM_SIZE_KILOBYTES`). DIRAM use is 235,331 of
341,760 bytes (68.86%) for the whole firmware. The device numbers above are from a clean boot to the
menu only; no Sound Jar gameplay has been measured on the device yet.

## Tests
- Host (`ctest`):
  - `audio_sampler_test`: resampling, reverse, echo/robot/wobble, limiter, trim/normalize
    (pops around the sound, steady room noise, click-only take, pauses inside the sound),
    too-quiet path, wipe, queue/epoch.
  - `sound_jar_test`: state machine, Home mid-record, hold/loop, bend, mic off, privacy wipe.
  - `sound_jar_art_test`
  - `parent_gate_test`: the mic toggle.
- Desktop sim: `little_arcade_sim --bench --game sound_jar --seconds 16 --shots DIR`.
  - Scripted session: records a synthetic tone (not a microphone), plays a pad, toggles Backwards,
    Echo and Robot, then holds and slides from the big pad to the tiny one.
  - Exits non-zero unless a sound was caught, the effects ended up Echo+Robot and the bend
    reached the last pad.
- Interactive sim: `little_arcade_sim` uses the PC microphone for Sound Jar (`--no-mic` turns it off).
- Device: pending.
  - tap-to-sound and record-start latency
  - PSRAM/heap watermarks
  - SPL at max volume with +12 st + Echo + Robot
  - feedback check with the speaker near the mic
  - NS2009 slide usability

## Rollback
Revert the Sound Jar commit. The game folder, sampler engine, shell/parent-gate/main/sim wiring
and docs all ship together. To disable it without reverting, remove the `SOUND_JAR` entry from the
`Arcade` games table.
