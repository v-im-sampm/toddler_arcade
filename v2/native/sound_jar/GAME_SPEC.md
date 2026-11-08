# Sound Jar: native source-of-truth contract

Source of truth: the runnable reference [`v2/reference/sound_jar.html`](../../reference/sound_jar.html)
(sha256 `bfc358f5…a165`, recorded in `assets/manifest.json`). The design is
[`v2/games/sound_jar.md`](../../games/sound_jar.md). Where they disagree, the native port follows the reference; see
"Discrepancies" below.

## Update model
- The reference `frame()` calls `update(dt)` once per animation frame with `dt = min(0.05, real dt)`; there is
  **no fixed step**. Native gameplay (`Game::tick(dt)`) takes the same variable `dt`. The desktop window passes
  real time capped at 50 ms; scripted runs, tests and browser comparisons use exactly 1/64 s steps on both sides.
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
- **No microphone.** Each recording replays one of four built-in voices synthesised by the reference
  `renderTake()`, exported at build time as 16 kHz int16 PCM plus 20 ms level frames
  (`assets/generated/takes_data.cpp`, 107,202 B). FROG is the starting jar sound.
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
| Record | 232,96 72×72 | start the countdown; tap again after 0.3 s while recording to stop early |
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

The native port therefore has no microphone path. It does not add a mic dependency, store voice, or use
ESP-SR, so SJ-4, SJ-5 and SJ-7 do not apply until a real recorder exists.

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
