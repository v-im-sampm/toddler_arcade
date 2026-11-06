# 05: Target architecture (runtime-agnostic)

This document defines **responsibilities, contracts and data flow**. The concrete language, engine
and libraries are bound in §9 *after* Phase 0 ADRs are approved. Interfaces below are written in
neutral pseudo-IDL. Implement them idiomatically in the chosen stack.

## 1. Layering
```
┌────────────────────────────────────────────────────────────────────┐
│ Games (one folder each): manifest + logic + scenes + assets + tests │  ← portable, deterministic
├────────────────────────────────────────────────────────────────────┤
│ Game SDK (versioned API surface)                                   │  ← the only thing games see
│  scene graph · animation · particles · audio cues/music · input    │
│  intents · rng/shuffle · timers · hints · celebrate · capabilities │
├────────────────────────────────────────────────────────────────────┤
│ Shell: launcher · navigation · parent gate · settings · sleep ·    │
│        session timer · transitions · voice UI · privacy wipe        │
├────────────────────────────────────────────────────────────────────┤
│ Engine services                                                     │
│  Renderer (compositor, dirty rects) · Animation runtime ·           │
│  Audio engine (mixer, synth, sampler, music, limiter) ·             │
│  Input (pointer phases, gestures) · Voice (KWS → intents) ·         │
│  Asset manager (IDs, variants, streaming) · Clock/scheduler ·       │
│  Storage (settings only) · Logging (local only)                     │
├────────────────────────────────────────────────────────────────────┤
│ Platform Abstraction Layer (PAL): display · touch · audio I/O ·     │
│  mic · persistence · time · threads · asset storage · power         │
├───────────────┬──────────────────────┬─────────────────────────────┤
│ ESP32-S3 PAL  │ Web PAL (WASM/JS)     │ Phone PAL (wrapper/native)  │
│ ST7789 SPI DMA│ Canvas/WebGL/WebGPU   │ via web PAL or native       │
│ NS2009 I2C    │ Pointer Events        │                             │
│ I2S full-dup. │ AudioWorklet          │                             │
│ NVS, flash/SD │ IndexedDB (settings)  │                             │
└───────────────┴──────────────────────┴─────────────────────────────┘
```
Rules:
- Games depend only on the SDK.
- The SDK depends on engine services.
- Only the PAL touches the platform.
- The shell is itself built on the SDK where practical. It dogfoods the SDK.

## 2. Threading and timing model
- **Fixed-step simulation** at 60 Hz logical ticks (16.67 ms) with an accumulator. On the device
  the renderer may present at 30 Hz. Logic still advances in fixed steps, so behaviour is identical
  across platforms. Large stalls are clamped: at most N catch-up steps, after which time skips
  (v1 Beat Sharks rule: "no burst of sounds").
- **Device (two cores), proposed for validation in Phase 0:**
  - Core 0: audio task (highest app priority, fixed block size such as 5–10 ms), voice/AFE task, and
    touch sampling.
  - Core 1: game tick + render.
  - SPI flush is DMA-driven, double-buffered, and overlapped with rendering of the next band.
- **Cross-thread communication** uses lock-free single-producer/single-consumer queues only:
  - input → logic,
  - logic → audio commands,
  - voice → intents.
  Nothing blocks the render or audio thread.
- **Audio clock is the music master clock.** Musical events (beats, quantised notes) are scheduled
  in sample time. The game reads the audio clock to sync visuals, with a latency-compensation
  offset per platform.

## 3. Game contract
### 3.1 Manifest (data, validated by `schemas/game.manifest.schema.json`)
Declares:
- id, display assets (tile icon + idle animation), and the launcher voice intent;
- min SDK version and capabilities;
- voice contexts/intents used;
- the asset list with per-platform budgets;
- audio identity (key, scale, tempo, instruments);
- the scene list, a deterministic test seed, and the replay scripts.

### 3.2 Lifecycle (pseudo-IDL)
```
interface Game {
  // Allocate everything here (pools sized from manifest). May stream assets asynchronously.
  load(ctx: GameContext) -> LoadHandle
  enter(ctx: GameContext, resume: Option<Snapshot>)
  tick(dt_fixed: Duration)                 // logic only, deterministic
  on_input(e: InputEvent)                  // pointer phases or intent, already routed
  on_pause(reason: PauseReason)            // Home, sleep, parent gate, OS background
  on_resume()
  exit() -> Option<Snapshot>               // tiny optional state for "resume on re-entry"
  unload()
}
```
Rendering is **retained**: games mutate scene-graph nodes and start animations; the engine renders.
Games never issue draw calls per frame. This is what lets the device renderer compute dirty regions
and lets the web renderer run at 60 FPS from the same scene.

### 3.3 GameContext (services)
```
ctx.scene      : SceneGraph     // nodes, layers, hit-testing
ctx.anim       : Animator       // tweens, timelines, springs, clip players, state machines
ctx.fx         : Particles      // data-defined emitters
ctx.audio      : AudioApi       // cue(id, {pitch_degree?, pan?, gain?}), music.set_state(s), prompt(id)
ctx.sampler    : SamplerApi?    // only if manifest declares "microphone"; RAM-only, privacy-wiped
ctx.input      : InputApi       // gesture recognisers, focus/hint pointing
ctx.rng        : Rng            // seeded per entry (replayable)
ctx.time       : GameClock      // game time + audio-beat time
ctx.hints      : HintScheduler  // idle→visual→spoken→auto-help ladder
ctx.celebrate  : Celebrate      // standard small/big celebrations (particles + sting + narrator)
ctx.settings   : ReadOnlySettings  // audio_on, voice_on, mic_on, reduced_motion
ctx.log        : Log            // local only, stripped/limited in release
```

### 3.4 Input
```
InputEvent =
  | Pointer { phase: Down|Move|Up|Cancel, x, y, t, pressure? }     // logical 320×240 coords
  | Gesture { kind: Tap|Hold|HoldEnd|Drag|Slide, target: NodeId?, x, y, t }
  | Intent  { intent: IntentId, confidence, source: Voice|Keyboard|Test, t }
```
- The shell consumes Home, Mic and parent-gate gestures **before** games.
- Intents are pre-filtered to the scene's active contexts.
- Debounce defaults to 80 ms for taps, configurable per node.
- Resistive touch noise: apply median and hysteresis in the PAL. Gestures use dead zones (v1 Sound
  Jar slide: 12 px).

### 3.5 Scene states
Every game uses an explicit state machine. It is visualisable and testable. The SDK provides a
`StateMachine` helper with enter/exit hooks and timed transitions. Canonical states remain
`INTRO → PLAY ⇄ (game-specific) → CELEBRATE → PLAY`, plus shell-owned `PAUSED`/`EXIT`.

## 4. Renderer requirements (both device and high-res)
- Retained scene graph → display list per frame → **dirty-region computation** (union of changed
  nodes' old/new bounds, merged to ≤ K rectangles) → band rasteriser → flush.
- Pixel formats: device RGB565 framebuffer bands; sprites RGB565+A8 or indexed (palette) + A8;
  premultiplied alpha.
- Features: affine transforms (scale/rotate for squash/stretch; rotated sprites may use pre-rotated
  frames on device if too costly), tint/colour-matrix (for highlight/glow), additive blend for
  sparkles, 9-slice, clip.
- Background layers can be marked static (cached) so only moving sprites invalidate.
- `reduced_motion` setting: halves parallax and particle counts, and removes screen shake.
- Debug overlay (dev builds): dirty rects, FPS, frame time histogram, heap/PSRAM, audio CPU.

## 5. Audio engine requirements
- Graph: voices → per-bus (music, sfx, voice, ambience, sampler) gain/duck → master EQ (speaker
  compensation for the 1 W driver, device only) → look-ahead limiter → ceiling → output.
- Voices: synth (v1 waves + FM/wavetable/plucked-string options), sample playback (one-shot, loop,
  pitch), sampler (v1 effects), music sequencer/stem player.
- Musical context: per-scene `key`, `scale`, `tempo`. `cue(..., pitch_degree)` maps to the scale.
  Optional quantise-to-grid for rhythm games.
- Voice-over (narrator) playback with priority and ducking. Prompt queueing with "latest wins" for
  repeated prompts.
- Deterministic rendering for golden-audio tests (seeded noise).
- See `07_audio_design.md` for the sound design rules.

## 6. Asset system
- All assets are referenced by **stable IDs** generated from manifests (no string paths in game logic).
- The pipeline (ADR-008) compiles sources into per-platform packs:
  - device: RGB565/A8/indexed atlases, ADPCM/Opus/PCM, baked animations, aligned for memory-mapped reads;
  - web/phone: WebP/AVIF/PNG at 2×–3×, Opus/AAC/OGG, vector/skeletal sources if live.
- Packs have content hashes. The device verifies them at boot (or lazily) and falls back to a safe
  placeholder visual and silence if one is corrupt.
- Each game declares a **budget**: flash, PSRAM-resident, and peak streaming. The pipeline fails the
  build on overrun.

## 7. Shell and privacy wipe
- The shell owns navigation, transitions, the parent gate, settings, sleep, the session timer and
  the voice UI.
- On any of exit / pause / sleep / parent-gate / OS-background, the shell calls
  `sampler.wipe()` (zero RAM) **before** handing control to anything else. Unit tests cover every
  path (v1 parity).

## 8. Code generation (single sources of truth)
| Source | Generates |
|---|---|
| `config/commands.json` (v2 schema) | Intent enums/IDs for every language, voice grammar files per recogniser, docs table |
| `v2/design/tokens.json` | Colour palette constants, easing curves, durations, layout constants, audio levels |
| `games/*/manifest.json` | Game registry, asset ID enums, capability checks, budget gates |
| `config/board_pins.json` | Device PAL pin header (as v1) |
CI runs every generator in `--check` mode to detect drift.

## 9. Chosen stack (fill after ADR approval)
| Layer | Choice | ADR |
|---|---|---|
| Language(s) / runtime | _TBD_ | ADR-001/005 |
| Device renderer | _TBD_ | ADR-002 |
| Web/phone renderer | _TBD_ | ADR-003 |
| Animation | _TBD_ | ADR-004 |
| Audio | _TBD_ | ADR-006/012 |
| Voice | _TBD_ | ADR-007 |
| Assets | _TBD_ | ADR-008 |
| Packaging | _TBD_ | ADR-009 |
| Build/CI | _TBD_ | ADR-010 |
| Storage/update | _TBD_ | ADR-011 |

## 10. Proposed repository layout (adapt after ADRs)
```
v2/
  engine/            # core services (renderer, audio, input, assets, anim) — portable
  pal/esp32s3/       # ESP-IDF project, drivers, partition table
  pal/web/           # web host, service worker, AudioWorklet
  pal/phone/         # wrapper or native shells
  sdk/               # public Game SDK headers/bindings + docs
  shell/             # launcher, parent area, transitions
  games/<id>/        # manifest.json, logic, scenes, assets-src/, tests/, replays/
  tools/             # codegen, asset pipeline, replay runner, budget checker, flash scripts
  design/            # tokens, style guide sources
  tests/             # cross-game golden suites
```
