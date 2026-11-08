# Sound Jar: native source-of-truth contract

Source of truth: the runnable reference [`v2/reference/sound_jar.html`](../../reference/sound_jar.html)
(sha256 `bfc358f5…a165`, recorded in `assets/manifest.json`). The design is
[`v2/games/sound_jar.md`](../../games/sound_jar.md). Where they disagree, the native port follows the reference; see
"Discrepancies" below.

## Update model
- The reference `frame()` calls `update(dt)` once per animation frame with `dt = min(0.05, real dt)`; there is
  **no fixed step**. Native gameplay (`Game::tick(dt)`) takes the same variable `dt`. The desktop window passes
  real time capped at 50 ms; scripted runs, tests and browser comparisons use exactly 1/64 s steps on both sides.
- The device renders at about 7–10 fps, below the reference's 20 fps clamp. A single 50 ms-capped tick per frame ran
  every animation at about half speed (user-reported on the device). The device therefore calls `Game::advance(elapsed)`,
  which ticks in steps of at most 50 ms until game time catches up with the wall clock. A stall is cut to 0.25 s
  (`kMaxCatchUp`). Each step keeps the reference's dt cap. The frame rate itself is unchanged (PF-1).
- `musicTick()` (the background melody and cave drips) runs inside `tick`. The drip visuals follow `st.amb`
  on a 14 s loop (`kDrips`) whether or not audio exists.
- Springs (`k = 380`, `c = 14`) and all easing follow the reference formulas.

## States
`START → INTRO (1.3 s, pads wake 0.35 + 0.1·i s) → EMPTY`
`EMPTY/FULL/TOO_QUIET --record--> COUNTDOWN (1.35 s, 3 silent dots lit at 0, 0.45 and 0.9 s) → RECORDING`
`RECORDING → PROCESSING (0.5 s, lid shuts) → FULL` (cheer, then the jar plays the new take once after 0.45 s)
`RECORDING → TOO_QUIET (voiced < 0.15 s) → EMPTY after 1.4 s`
- Recording ends when 0.5 s pass after the last active input (level ≥ 0.002), at 3 s, or when the record button
  is tapped again after 0.3 s.
- "Voiced" time accumulates while the level ≥ 512/32768.
- PLAYING is derived: a live voice in EMPTY or FULL.
- `can_play()` is true in EMPTY, FULL and TOO_QUIET.
- Home (top-left 56×56) returns to START from any state. It stops all voices, clears hint and idle state and
  resets tries and actions.
- Bin aborts a recording and pours the jar back to the frog sound (EMPTY).

## Takes and randomness
- **Built-in takes (reference, desktop, and device fallback).** Each recording replays one of four built-in voices synthesised by the reference
  `renderTake()`, exported at build time as 16 kHz int16 PCM plus 20 ms level frames
  (`assets/generated/takes_data.cpp`, 107,202 B). FROG is the starting jar sound.
- **Live microphone take (device only; see "Microphone" below).** When the mic is present and the parent mic
  switch is on, the recording is captured from the INMP441 instead. The take rotation still advances, so
  colours and the whisper slot stay the reference's. Only the samples and levels come from the mic.
- Gameplay RNG `grng = mulberry32(seed)`: when the order is empty, it Fisher-Yates-shuffles `[1,2,3]` and
  builds `order = {WHISPER, b0, b1, b2}`, popped from the back. Takes 1–3 are therefore the shuffled real
  voices and every 4th take is the whisper, which is too quiet.
- Cosmetic RNG `frng = mulberry32(seed ^ 0x9E3779B9)` covers particles, blinks and sparkles. Taps never
  perturb the take order. Particle emission consumes `frng` in the reference order: angle, speed, life, colour,
  size, rotation, spin.

## Input (320×240 logical; mouse == touch)
| Target | Hit rect | Action |
|---|---|---|
| Home | 0,0 56×56 | `home()` |
| Mic (disabled) | 264,0 56×56 | white ring only; voice commands are disabled in this game |
| Effects ×4 (BACK, ECHO, ROBOT, WOBBLE) | centre x 82/134/186/238, y 6, 48×48 | toggle; at most 2 on, a 3rd bumps the oldest; previews the jar after 0.18 s |
| Record | 232,96 72×72 | start the countdown (a live mic take records at once; see § Microphone); tap again after 0.3 s while recording to stop early |
| Bin | 16,96 56×56 | empty the jar (whoosh), abort a recording |
| Jar | 112,60 96×96 | play the jar sound (always plays when `can_play`) |
| Pads ×5 (−12, −5, 0, +7, +12 st) | x 64·i, y 168, 64×72 | tap plays a one-shot; hold ≥ 350 ms loops; vertical slide past a 12 px dead zone bends 1 st per 8 px (max ±5) |
| Anywhere else | — | sparkle, ring and a soft tap note |

- In START, any tap starts the game. A tap during INTRO skips it.
- Each pressable object has an 80 ms debounce.
- Loops stop on release; one-shots play out.
- At most 4 live voices: the oldest is stolen.
- Voice intents (`config/commands.json` enums only):
  - AGAIN = tap the jar;
  - HELP = the hint path;
  - START = start the game;
  - BACK = Home.
  
  No intent records audio.

## Forgiving help and accessibility
- After 6 s idle the helper hand points at `suggest()`: the record button, then a fixed tour of pads and
  effects. After 12 s the jar hums ("hm-hm?").
- Too quiet is nonverbal: the jar wiggles with raised brows, hums, and yellow "louder" arcs ripple from the
  record button. There is no failure sound and no score.
- Reduced motion (`R` on desktop) scales wiggles and hops down, halves the mist wave amplitude and calms the
  lid lean. Gameplay is identical.
- With sound off, every cue keeps a visual: rings, particles, mouth and pad sing animation, loop dots, louder
  arcs and the lid pop.

## Audio triggers (reference function → `Cue`)
- `tapNote`, `padNote`, `lidPop`, `lidShut`, `softShut`, `tada`, `hmm`, `hint`, `fxOn`, `fxOff`, `fxBump`,
  `wake`, `whoosh`.
- A live recording plays no `lidPop` (see § Microphone).
- Sampler voices: start, loop, bend and release.
- Shared FX bus: echo 250 ms with 0.45 feedback, 55 Hz ring mod, 6 Hz wobble at ±1.5 st, and reverse per voice.
- The melody and drips duck while voices play.
- `voiceOk()` keeps the game silent while RECORDING.
- The master gain ramps 0 → 0.35 on the first start; the firmware volume cap applies on top. There is no sound
  before the first tap.

## Visuals
- All artwork is exported from the reference's own Canvas drawing functions (`tools/export_assets.mjs`):
  1,886 sprites in a 2,286,899 B pack.
  - The 320×240 static background is RGB565 and dithered.
  - Characters, buttons and props are RGB565A8.
  - Mist, swirl, rings, motes and arcs are A8 masks recoloured at runtime.
- Animated parts are separate layers with LVGL transforms: rotation, scale and opacity.
- Continuous shapes are quantised into frames:
  - mist wave: 8 phases;
  - mist level: 33 steps;
  - swirl: 17×16;
  - glint: 8;
  - jar mouth: 16;
  - pad mouths: 8;
  - FX icons: 9–16 frames.
  
  See `assets/manifest.json` → `frame_timing`.

## Discrepancies (reference wins)
These are documented in the design's own "Notable differences" list:
- Sampled built-in voices replace the microphone.
- Effects are centred at x 82..238, not 72..248.
- The retry is nonverbal.
- The countdown dots are silent.
- The lid leans on the jar while recording.
- A second tap stops the recording early.
- The jar plays a new take once by itself.
- There is no 2 s jar-hold to empty; the bin only.
- There is no prompt badge.
- PLAYING is derived.
- The FX bus is shared (≤ 4 voices).
- Pad visuals shrink with pitch.
- Mist motes are computed shapes.

The reference has no microphone. The design (§ Privacy, SJ-3 to SJ-7) asks for one, so the device adds a live
recorder on top of the reference rules. This is a deliberate, documented addition beyond the runnable reference.

## Microphone (device; `src/game/live_take.{hpp,cpp}`)
- **Gate.** Live mode is used only if the mic hardware started **and** `settings::load_mic_enabled()` is on
  (the parent switch; default on). It is decided when a recording starts. Otherwise the built-in takes are used
  unchanged (SJ-5). The desktop always uses built-in takes, and so do bench builds, to keep frame windows comparable.
  The top-right mic (voice commands) stays disabled. Intents never record.
- **Recording flow (user-requested after physical testing; live takes only).** Tapping the mic opens the lid and
  arms capture at once. The flow:
  - Phase goes straight to RECORDING: no countdown dots, no ribbon/wire, and no `lidPop` (capture mutes the speaker,
    which would chop it, and the mic would hear it).
  - The jar's mist rises with `voiced` while you talk.
  - Built-in takes (desktop, no mic, or parent switch off) keep the reference's 1.35 s countdown, so browser parity
    (`tools/visual_compare.mjs`) is unchanged.
  - An earlier iteration had a "listening" blip and a red ring; the user found the countdown plus the animations
    unclear and asked for this direct flow instead.
- **Capture.** 16 kHz mono int16 from the shared full-duplex I2S driver through the `MicSink` hook:
  - A DC blocker (pole 0.995) runs continuously, removing the INMP441 offset.
  - The device then applies a fixed ×16 (+24 dB) input gain. The reference's thresholds assume browser AGC levels,
    and the first physical test showed raw INMP441 speech landing below LOUD.
  - The first 100 ms after arm (`kDiscardSamples`: the speaker tail and the tap itself) is never kept. It only
    measures the room.
  - The clip is at most 3 s (48,000 samples, 96,000 B in PSRAM).
- **End rules** (captured sample time, i.e. wall clock):
  - the 3 s cap;
  - 0.5 s with no *sounding* frame;
  - a second tap after 0.3 s.

  A frame is sounding when its 20 ms RMS ≥ `clamp(10 × floor, ACTIVE 0.002, LOUD 512/32768)`, where `floor` is the
  quietest frame of this recording. The reference's absolute ACTIVE does not work on the device: the room noise at
  ×16 measured −55 dBFS with frames up to −45 dBFS, above ACTIVE (−54 dBFS), so takes always ran to 3 s.
  - A 3× ratio still failed physically.
  - 10× passed: user-confirmed auto-stop and trim.

  `voiced` is the seconds of frames with RMS ≥ LOUD (absolute, as in the reference). Under 0.15 s the take is too quiet.
- **Too quiet (user-requested; live takes only).** The friendly `hmm` retry still plays, then:
  - the jar stays empty (it is not refilled with the frog), and the jar, pads and AGAIN make no voice;
  - the helper hand stays on the mic button (`hint = 2`, wiggle every 1.6 s) until the next recording;
  - the bin or Home return to the reference's starting jar (the built-in frog).

  The game still starts with the reference's frog jar.
- **Processing** follows the reference `renderTake()`:
  - trim to the sounding frames, from one frame before to two frames after, so word tails survive;
  - normalise the peak to 0.5 (gain ≤ 32);
  - 5 ms fades;
  - `lev` = raw frame RMS, `playLev` = processed RMS.

  If nothing survives the trim, the take is treated as too quiet. If capture is not acknowledged within 1.5 s, the
  built-in take is used.
- **Speaker silent while capturing (SJ-6).** The score ramps its output to 0 over 5 ms while capturing, so the
  mic does not hear the speaker. The Arm request re-asserts the mute, because a new recording queues Wipe then
  Arm in the same audio block.
- **Wipes (SJ-4).** These volatile-zero the whole buffer on the audio thread and bump a generation counter that kills any voice still playing the
  clip:
  - Home/exit;
  - the bin;
  - a new recording;
  - too quiet;
  - a processing failure.

  Pause/background, sleep and parent-gate entry do not exist in this single-game target (N/A). Nothing is persisted.
  Only aggregates appear in the serial log: take count, start latency, the loudest and quietest frame level per take,
  and capture cost.
- **SJ-7 latency** is measured from the tap (arm) to the first captured block: 13–22 ms on the device.

## Native-only differences
- The reference's `ended` flag is not mirrored. The duck gate uses `started_` where the reference tests `ac`.
- Frames are quantised as listed above.
- Some layering is approximated:
  - halo z-order;
  - loop dots are drawn after all pads;
  - ribbon alpha overlap;
  - bin lid hinge pivot;
  - scaled lit-dot stroke.
- The audio score is a 16 kHz C++ re-implementation (`src/audio/sound_jar_score.cpp`) with linear-interpolation
  resampling, 16-sample control blocks and one-pole envelope curves. It is not bit-identical to Web Audio.
