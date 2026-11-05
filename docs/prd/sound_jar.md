# PRD: Sound Jar (toddler sampler)

**Status:** Built (v1). Spec: [`../../games/sound_jar.md`](../../games/sound_jar.md). Mockup:
[`../mockups/sound_jar.html`](../mockups/sound_jar.html). Owner decisions: RAM-only recording
exception approved, parent mic setting defaults **ON**, no spoken launcher name (tap the tile).

**Pitch:** Catch a sound in a jar, then make it squeaky, rumbly, backwards, echoey, robotic or
wobbly — by tapping big colorful buttons.

## 1. Why
Toddlers love hearing their own voice played back changed. A sampler is pure cause-and-effect:
every tap makes a sound, there is no right answer, and it naturally teaches high/low, fast/slow,
forwards/backwards. It reuses hardware we already have (INMP441 mic, MAX98357A speaker) and the
existing 16 kHz audio path.

## 2. Users and goals
| Who | Goal |
|---|---|
| Child (~3 y) | Record a silly noise, hear it change instantly, repeat endlessly. No reading, no failing. |
| Parent | Confidence that the child's voice never leaves RAM, is never saved, and can be disabled. |
| Developers | Reuse `audio`/`Synth`, bounded memory, host-testable DSP. |

**Non-goals (v1):** saving/sharing recordings, multiple stored sample slots, sequencer/song mode,
live mic monitoring through the speaker, any network or file I/O of audio.

## 3. Experience
### 3.1 Screen (320×240 landscape, all targets ≥ 48 px)
```
y 0-56     [HOME] [ REC ] [ JAR: waveform + playhead ] [MIC]
y 64-116   [ BACKWARDS ] [ ECHO ] [ ROBOT ] [ WOBBLE ]        (toggles)
y 124-236  [ BIG ] [ big ] [ mid ] [ small ] [ tiny ]          (pitch pads, low -> high)
```
| Element | Hit rect (x, y, w, h) | Behavior |
|---|---|---|
| Home | 0, 0, 56, 56 | Exit to launcher (wipes sample, §7). |
| Record | 60, 0, 64, 56 | Tap to start; tap again or 3 s limit to stop. A fill bar grows with time, halo grows with loudness. |
| Jar | 128, 0, 132, 56 | Shows the sample waveform with a moving playhead; tap plays at normal pitch with active effects. Stops short of the shell's Mic corner (264, 0, 56, 56), which games never receive. |
| Effect toggles ×4 | x = 4 + 80·i, y 64, 72×52 | Toggle on/off; each tap also previews. Effects stack. |
| Pitch pads ×5 | x = 4 + 64·i, y 124, 56×112 | Tap = one-shot at that pitch. Hold > 350 ms = loop while held. |

Pitch pads show the same friendly blob creature in five sizes: **big creature = low voice, tiny
creature = high voice**. Colors follow the rainbow, and size/position carry the meaning so color
is never the only cue.

### 3.2 Sound tricks
| Trick | Control | v1 behavior |
|---|---|---|
| Pitch (chipmunk / giant) | Pitch pads | Resample at −12, −5, 0, +7, +12 semitones (ratio 0.5–2.0). Speed changes with pitch — that's the fun. |
| Pitch bend (slide) | Hold a pad and slide sideways | Bonus. After a 12 px dead zone the pitch glides continuously between pad pitches; a dot marks the position. Never required (architecture: no precise drag). |
| Loop | Hold a pad | Sample repeats while held; release fades out in 40 ms. |
| Backwards | Toggle | Plays end → start. Jar waveform flips. |
| Echo | Toggle | 250 ms feedback delay, feedback 0.45, tail ≤ 1.2 s. |
| Robot | Toggle | Ring modulation with a 55 Hz sine carrier. |
| Wobble | Toggle | Vibrato: 6 Hz LFO, ±1.5 semitones on the playback rate. |

**Stretch (post-v1, behind evaluation):** Stutter ("b-b-b-baby"), 2–3 jar slots, a "Surprise"
button that randomizes effects with a deterministic seed, tap-to-record rhythm loop.

### 3.3 Flow and states
`INTRO → PLAY ⇄ RECORDING → CELEBRATE → PLAY`, plus `PAUSED` and `EXIT`.
1. **INTRO**: short spoken prompt ("Tap the red button and make a silly sound!") over a built-in
   *boing* already in the jar, so every pad works before any recording.
2. **RECORDING** (≤ 3 s): speaker muted (no feedback howl), all voices stopped, push-to-talk
   disabled. Red ring and loudness halo animate. Starts only on an explicit tap.
3. **Process** (< 50 ms): trim leading/trailing silence (keep 20 ms pre-roll / 80 ms tail),
   normalize peak to −6 dBFS, 5 ms fade in/out.
   - Sound is detected in 5 ms windows and must last at least 15 ms, so clicks and screen-tap
     pops are trimmed.
   - The threshold is the loudest sustained level / 10, raised to 2× the room-noise floor.
4. **CELEBRATE** (≤ 1.5 s, non-blocking): jar sparkles and plays the new sound once.
5. **Too quiet / too short** (no sustained sound ≥ 512 ≈ −36 dBFS, or < 150 ms): keep the
   previous sound, jar does a gentle wiggle. No failure sound, no text.

Repeated taps never queue up: at most 2 sample voices (plus one fading out); a new tap steals the
oldest. Navigation is accepted at any time, including mid-recording (recording is discarded).
A loop that is released early still finishes its current pass instead of cutting off.

### 3.4 Mic disabled (parent setting or no mic)
Record button becomes a **sparkle** button that cycles three built-in, in-project synthesized
sounds (boing, quack-like pulse glide, drum thump). All pads and effects behave identically, so
the game is fully touch-complete and works with the mic off.

### 3.5 Voice
Global intents only — **no new entries in `config/commands.json`**:
`BACK` = Home, `AGAIN` = replay last pad + effects, `HELP` = replay prompt, `PAUSE`/`RESUME`.
Voice commands are never used to start recording (the command itself would be recorded).
The launcher tile has no spoken name (`Intent::UNKNOWN`): children open Sound Jar by tapping it.
Push-to-talk is not wired at runtime yet; when it is, it and Record must be mutually exclusive
owners of I2S RX.

## 4. Functional requirements
| ID | Requirement |
|---|---|
| F1 | Record mono 16 kHz int16, max 3.0 s (48 000 samples) into a single preallocated buffer. |
| F2 | Recording starts < 100 ms after tap; visible indicator for the entire capture. |
| F3 | Pad/jar/toggle tap → audible start < 100 ms (same target as Rainbow Piano). |
| F4 | Pitch via fixed-point linear-interpolated resampling (Q16.16 step); no floats in `render()`. |
| F5 | Backwards, Echo, Robot, Wobble each independently toggleable and stackable. |
| F6 | Hold-to-loop and optional slide bend on pads. |
| F7 | Built-in fallback sounds when mic disabled/unavailable/permission off. |
| F8 | Output passes through the existing volume cap; soft limiter after Echo; no clipping. |
| F9 | Sample wiped (zeroed) on EXIT, idle sleep, and parent-gate entry (§7). |
| F10 | Deterministic test mode: injectable PCM fixture replaces mic; seeded RNG for stretch features. |

## 5. Technical design (as built; fits `docs/architecture.md`)
- **Interface:** games see only `little_arcade::ISampler` (`core/include/little_arcade/sampler.hpp`):
  record start/stop/cancel, play/bend/end_loop/release, effect mask, built-ins, `wipe()`, and
  read-only progress/level/thumbnail/playhead atomics for the view.
- **Engine:** `audio::Sampler` (`components/audio/sampler.cpp`) borrows three buffers allocated
  once at boot in PSRAM by `main.cpp` (games never allocate them). UI-task calls only push into a
  16-entry lock-free SPSC command queue; the audio task owns every buffer. `wipe()` bumps an epoch
  so queued commands from before a wipe are discarded.
- **Capture:** the existing I2S RX path (`start_i2s_audio`, shared BCLK/WS) feeds
  `Sampler::capture()` on the audio task. Captured PCM is never routed to ESP-SR, logs or storage.
- **Playback:** `AudioFeedback::render()` mixes the sampler voices before the existing limiter and
  volume cap. Q16.16 linear-interpolated resampling, reverse, echo (250 ms, 0.45 feedback),
  ring-mod robot (55 Hz), vibrato wobble (6 Hz, ±1.5 st), soft knee; no floats or heap in `mix()`.
  The speaker is silenced while recording.
- **Bend:** the shell forwards raw pointer state (`Arcade::pointer` → `SoundJarGame::pointer`); the
  game sends a new rate only when it changes and the audio task slews it over 10 ms.
- **Game code** receives `InputEvent` intents/touch only; no recognition text.

### 5.1 Memory budget
| Item | Location | Bytes |
|---|---|---|
| Clip buffer 3 s × 16 kHz × int16 | PSRAM | 96 000 |
| Raw take buffer (zeroed after processing) | PSRAM | 96 000 |
| Echo delay line 250 ms | PSRAM | 8 000 |
| Waveform thumbnail (64 columns) | internal | 64 |
| View rasters: 5 critters 50², 7 icons 36², star 28², jar 112×34 (ARGB8888) | PSRAM (`ArgbImage`) | 104 656 |
| Built-in fallback sounds | synthesized into the clip at runtime | 0 |
| **Total new** | | **≈ 305 KB PSRAM, < 1 KiB internal** (LVGL heap max_used +3 KB in the sim bench) |

Gate: PSRAM free ≥ 1 MiB and internal heap ≥ 64 KiB with the sampler active (device measurement
pending; see `firmware/components/games/sound_jar/README.md`).

## 6. Child-safety and audio-safety requirements
- No failure states, timers that pressure, or punishments; the 3 s ring is a capture indicator.
- Speaker muted while recording; no live mic monitoring (prevents howling feedback).
- +12 semitone output and Echo stay under the existing volume cap; limiter verified with a
  full-scale fixture. No sudden loud sounds on enter.
- No rapid flashing: playhead and halo animate gently, no full-screen strobes.

## 7. Privacy (policy exception — requires owner sign-off)
Current docs forbid recording (`README.md` Boundaries; `docs/parental-safety.md`; `docs/voice.md`).
This app needs a narrowly scoped exception:
1. Audio lives **only in RAM**, single slot, overwritten by each new recording.
2. **Never** written to flash, NVS, SD, logs, serial, or any transport; logs may contain only
   aggregate peak/RMS (same rule as the H03 mic probe). Review rule: the `audio::Sampler` buffers
   may not be passed to any file/NVS/UART/network API.
3. Wiped (zeroed, not just freed) on EXIT, idle sleep, parent-gate entry, and recording start.
4. Recording requires an explicit child tap and shows a continuous visible indicator.
5. Parent setting **"Mic"** in the parent screen (stored in NVS key `mic`). Owner decision:
   default **ON**. When off, §3.4 built-in sounds are used. The clip is also wiped whenever the
   parent gate opens.
6. README, `parental-safety.md` and `voice.md` amended to: *"No child speech is persisted or
   transmitted. Sound Jar may hold one ≤ 3 s clip in RAM while open."*

## 8. Acceptance criteria
- All targets ≥ 48 px; game fully playable with mic disabled and with audio-off (visual-only).
- Tap-to-sound < 100 ms; record-start < 100 ms (measured on device, not simulator).
- No clipping at max volume with +12 st, Echo and Robot all on (full-scale fixture).
- Sample buffer verified zeroed after EXIT/idle/parent gate (host test + on-device check).
- Handles: repeated taps, Home mid-recording, voice intent during CELEBRATE, mic loss,
  silence-only recording, 3 s cap, toggling all effects while a loop is held.
- No per-frame allocations; measured PSRAM/internal heap deltas recorded.
- Host, emulator and physical-device results reported separately (AGENTS rule 8).

## 9. Tests
**Host (pure C++):** resampler ratio/length (0.5× doubles length, 2× halves), reverse is exact
mirror, echo tail bounded and decays, robot carrier frequency, wobble LFO bounds, limiter never
exceeds int16, trim/normalize on fixtures (silence, click, voice-like chirp), too-quiet path keeps
previous sample, `wipe()` zeroes buffer, state machine (Home mid-record, 3 s cap, PTT exclusion).
**Device:** latency, heap/PSRAM watermarks, SPL at max volume, feedback check with speaker near mic.

## 10. Milestones
1. ~~**M0** Owner decision on §7 + mockup review.~~ Done.
2. ~~**M1** Host DSP library + unit tests~~ (`audio_sampler_test`). Done.
3. **M2** Capture through I2S RX — wired; device latency/memory measurements pending hardware.
4. ~~**M3** `firmware/components/games/sound_jar/` scene + simulator; built-in fallback sounds.~~ Done.
5. ~~**M4** Launcher registration, `games/sound_jar.md`, validator count, §7 doc amendments.~~ Done.

## 11. Open questions
1. ~~Approve the RAM-only exception / mic default?~~ Approved, default ON.
2. Is 3 s enough? (Each extra second costs 64 KB PSRAM: clip + raw take.)
3. ~~Launcher voice name?~~ None; tap the tile.
4. Is the slide bend usable on the NS2009 resistive panel? Check during H02 calibration.

## Rollback
Revert the Sound Jar commit (`git revert <sha>`). It is self-contained: the game folder, the
sampler engine and tests, the shell/parent-gate/main/sim wiring and these docs.
