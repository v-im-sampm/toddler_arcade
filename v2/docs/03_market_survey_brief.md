# 03: Phase 0 market survey and technical spikes (MANDATORY FIRST PHASE)

**Goal:** choose the v2 technology stack from evidence, not habit. The owner chose **not to
pre-decide** the runtime. You must survey the current market, short-list, prototype on the real
ESP32-S3 board, and record decisions as ADRs (`../decisions/`).

**Deadline discipline:** keep Phase 0 time-boxed. Recommend 1–2 weeks of agent time plus owner
flash sessions. Prefer breadth in desk research, then depth only on the short-list.

---

## 1. Decisions to make (one ADR each)
| ADR | Decision | Must cover |
|---|---|---|
| ADR-001 | **Portability strategy and language(s)** | How one game source runs on ESP32-S3, web and phone |
| ADR-002 | **Device graphics stack** | Renderer, UI toolkit (if any), flush strategy, frame pacing on ST7789/SPI |
| ADR-003 | **Web/phone graphics stack** | Canvas2D / WebGL2 / WebGPU / native; DPI scaling; same scene description |
| ADR-004 | **Animation authoring and runtime** | Tool(s) artists use, runtime format, device playback cost |
| ADR-005 | **Game logic extensibility** | Compiled-in vs scripted vs data-driven; hot reload; sandboxing |
| ADR-006 | **Audio engine** | Mixer, synth, sampler, music system, DSP portability, sample rate on device |
| ADR-007 | **Offline voice per platform** | Device KWS/command model; web/phone offline model; licences |
| ADR-008 | **Asset pipeline and formats** | Image/atlas/compression per target; audio codec; build tooling |
| ADR-009 | **Phone and web packaging** | PWA vs wrapper (e.g. Capacitor/Tauri) vs native; store compliance |
| ADR-010 | **Build, CI and test infrastructure** | Monorepo layout, codegen, golden tests, containers (`wslc`) |
| ADR-011 | **Device storage and update** | Partition layout, asset partition/SD, OTA or not (offline), integrity |
| ADR-012 | **Device audio clocking** | I2S rate with shared BCLK/WS; decimation for speech; quality vs CPU |

ADRs 001, 002, 005 and 006 are coupled. Evaluate them as **end-to-end stacks** (§3), not in isolation.

## 2. Evaluation criteria (weights are suggestions; justify changes)
| Criterion | Weight | How to evidence |
|---|---|---|
| Runs well on **ESP32-S3 + 8 MB PSRAM + SPI 320×240** | 25 | On-device spike numbers (§4) |
| Single-source portability (device / web / phone) | 15 | Same game file builds/runs on all; % shared code |
| Visual/animation ceiling (vector, skeletal, particles, blend, AA) | 12 | Spike scenes; screenshots/video |
| Audio quality and latency | 10 | Measured latency; render CPU %; spectral check |
| Authoring speed for new games (hot reload, tools, docs) | 10 | Time to build the "toy game" spike (§4.6) |
| Licence fit (closed-source kids product, store distribution, no attribution traps) | 8 | Licence text cited; copyleft implications analysed |
| Maturity / maintenance / community / vendor risk | 8 | Release cadence, issues, bus factor, funding |
| Determinism and testability (headless, replays, golden images) | 7 | Demonstrated in spike |
| Memory/flash footprint | 5 | Measured binary + RAM deltas |

Score each short-listed stack 1–5 per criterion with a cited reason. Show the weighted total, and
explain any veto (e.g. licence incompatibility or a device FPS fail).

## 3. Candidate leads to investigate (verify everything; this is not an endorsement)
These are **starting points** from general knowledge. Their current status, licence and
capabilities **must be verified** from primary sources, and you should search for newer
alternatives.

### 3.1 Portability / runtime (ADR-001, ADR-005)
| Lead | Idea | Questions to answer |
|---|---|---|
| **A. Portable C/C++ core + platform backends** (v1 style, evolved) | Engine and games in C++; ESP-IDF on device; Emscripten→WASM for web; the web build wrapped or a native build for phones | Rendering abstraction cost; how much v1 code ports; developer speed vs scripting |
| **B. Rust core** (e.g. esp-rs / `esp-idf-hal` on S3, `embedded-graphics`, WASM for web) | Memory safety, good WASM story | Maturity of the S3 PSRAM/LCD/I2S/ESP-SR bindings; ESP-SR FFI; team familiarity |
| **C. Cross-platform UI toolkit** that targets MCUs + web + mobile (e.g. **Slint**, **LVGL** on all targets) | One declarative UI language | Licence terms (e.g. Slint's licence options), animation/particle ceiling, game-loop fit, WASM/mobile support |
| **D. Embedded scripting VM for game logic** (e.g. **Lua**, **Berry**, **Wren**, **MicroPython**, **JerryScript/Elk** JS, **WebAssembly runtimes** such as WAMR/wasm3 on ESP32) | Engine native per platform; games as scripts/bytecode so the same game file runs everywhere with hot reload | VM speed and RAM on S3; GC pauses vs 33 ms frames; determinism; sandboxing; debugging |
| **E. Data-driven games** (state machines + tweens + rules in JSON/YAML, minimal code) | Most games here are simple state machines | Expressiveness ceiling; combined with D for escape hatches? |
| **F. Web-first (TypeScript) with a device transpile/port** | Fastest web/phone authoring | Realistic device path? Likely only via D (JS VM) or WASM AOT. Prove or reject |
| **G. Existing small game frameworks** with MCU ports (search: "ESP32 game engine", e.g. projects built on `TFT_eSPI`/`LovyanGFX`, Arduboy-style, "Pico" fantasy consoles) | Re-use | Licence, quality ceiling, portability to web/phone |

A **hybrid** is likely the best fit and should be evaluated explicitly. Example shape: a native
engine in C++ or Rust handles rendering, audio and input; each game's logic is a **WASM module or
script** plus **data** (manifest, timelines, tuning); the engine is compiled per platform.

### 3.2 Device graphics (ADR-002)
- **LVGL 9.x** (v1 uses 9.5). Investigate the newest version's features: its vector graphics and
  Lottie (ThorVG) support, the draw-unit architecture, and ESP32-S3 SIMD-accelerated blending if any.
  Check whether LVGL suits *game* scenes or only UI chrome.
- **Custom tile/sprite compositor:** dirty rectangles, RGB565 + A8 alpha, line buffers, and
  double-buffered SPI DMA overlapped with rendering on the second core.
- **ESP32-S3 acceleration:** Xtensa **PIE/SIMD** instructions, **ESP-DSP**, and Espressif component
  registry libraries for image decode or JPEG (`esp_new_jpeg`?), plus any 2D/PPA availability (the
  PPA is on ESP32-P4, **not** S3; confirm).
- **ThorVG** directly (vector + Lottie). Measure the cost at 320×240 on S3.
- **Panel limits:** max reliable SPI clock for this ST7789 board (27 MHz in v1; test 40/60/80 MHz
  for signal integrity). TE pin availability (TBD; check the schematic). Partial-window updates via
  CASET/RASET.
- Fonts: games avoid text, but the parent UI needs some.

### 3.3 Web/phone graphics (ADR-003)
- Canvas2D vs **WebGL2** vs **WebGPU** (check current iOS Safari support).
- If the core is compiled: **SDL3** / **sokol** / **raylib**-style platform layers with Emscripten.
- Pixel-art vs smooth vector: decide whether the device look (320×240) upscales crisply
  (nearest/integer scale with an art-directed frame) or re-renders at native density. This has a
  big impact on the asset pipeline.

### 3.4 Animation authoring (ADR-004)
| Lead | Notes to verify |
|---|---|
| **Rive** (state machines, runtime in C++) | Can the C++ runtime render via a software rasteriser on S3 within budget? Licence of runtime and editor; file-format stability |
| **Lottie** via ThorVG / rlottie / LVGL | Authoring in After Effects/Bodymovin or alternatives; CPU cost on S3; probably pre-rasterise for the device |
| **Spine / DragonBones / Spriter** skeletal | Spine runtime licence needs a Spine editor licence; check terms. DragonBones status |
| **Aseprite / sprite-sheet** frame animation | Simple, deterministic, cheap on device; flash cost |
| **In-engine tweens + timelines** (code/data) | Always needed for UI motion; define in tokens |
| **Hybrid bake** | Author in a vector/skeletal tool → build step bakes device sprite sheets (RGB565/A8, palettised) while web/phone play the vector/skeletal source live |

### 3.5 Audio (ADR-006, ADR-012)
- Keep and port v1's synth/sampler/limiter (known good), or adopt a library:
  - **miniaudio** (backends), **SoLoud** (game audio), **libpd** (Pure Data; likely heavy),
  - **Faust** (DSP language that compiles to C++ for ESP32 and to WASM for the web, which gives
    single-source DSP),
  - **Web Audio + AudioWorklet** on web,
  - **Oboe/AAudio** on Android and **AVAudioEngine** on iOS if native.
- Music: procedural/generative (scale-locked sequencer) vs pre-rendered stems with adaptive mixing.
  Weigh flash cost against quality.
- Codecs on device: IMA-ADPCM, **Opus** (`esp-libopus`? verify), or raw PCM in PSRAM. Measure
  decode CPU.
- Device rate: measure CPU and quality for 16 / 22.05 / 24 / 32 kHz, with mic decimation to
  16 kHz for speech (the I2S clocks are shared).

### 3.6 Offline voice (ADR-007), see also `08_voice.md`
- **Device:** Espressif **ESP-SR**. Check:
  - the current WakeNet/MultiNet versions, English command models, and how custom commands are
    defined (phoneme vs text input; check MultiNet 6/7 docs);
  - the AFE (AEC/NS/VAD) and whether its reference-channel needs can be met with shared I2S;
  - model sizes, RAM/PSRAM, CPU, and licence terms.
- **Web/phone (must be on-device, no cloud):** candidates include
  - **sherpa-onnx** (keyword spotting/ASR, WASM + Android + iOS),
  - **Vosk** (WASM builds; grammar-restricted recognition),
  - **whisper.cpp** (WASM; probably too heavy for snappy commands),
  - **Picovoice Rhino/Porcupine** (commercial licence terms; check the free-tier rules for
    distributed products),
  - OS on-device recognisers: Android `SpeechRecognizer` with on-device mode, iOS
    `SFSpeechRecognizer` with `requiresOnDeviceRecognition`. These are **only acceptable if
    guaranteed on-device**. Verify per OS version.
- The Web Speech API (`SpeechRecognition`) in Chromium sends audio to a server. **Do not use it.**
  Verify the current behaviour and document it.

### 3.7 Packaging (ADR-009)
- **PWA** (service worker, offline cache, install prompt): iOS limitations (audio unlock, storage
  eviction, fullscreen, orientation lock).
- **Capacitor**, **Tauri 2 mobile**, or fully native shells: store acceptance for kids categories
  (Apple Kids Category, Google Play Families policy), and SDKs that must be excluded.
- Guided Access / screen pinning guidance for parents (Android app pinning, iOS Guided Access).

## 4. Mandatory spikes (on the real ESP32-S3 board unless marked)
Each spike has a checked-in harness, a fixed scripted input, and a results table. Use evidence tier
`esp32-device` only for numbers from the board.

| # | Spike | Pass criteria |
|---|---|---|
| 4.1 | **Flush ceiling:** fill/stream full frames and partial windows at 27/40/60/80 MHz; DMA double-buffering; render on core 1 while flushing | Report µs per KB and stable max clock with no artefacts. Full-frame FPS and ≥ 30 FPS for a 160×120 animated region |
| 4.2 | **Sprite compositor:** 1 parallax background + 12 alpha sprites (64×64, RGB565+A8) + 60 particles, dirty-rect only | ≥ 30 FPS p95 in the animated region; frame time p99 < 40 ms; internal heap free ≥ 64 KiB |
| 4.3 | **Vector/skeletal runtime on device:** one 128×128 character animation via each short-listed runtime (e.g. Rive/ThorVG-Lottie) vs a baked sprite sheet of the same animation | FPS, CPU %, RAM, flash for each. Decide live vs baked |
| 4.4 | **Scripting VM:** run the same small game logic (Color Pop rules) in each short-listed VM at 30 Hz | Logic tick p99 < 2 ms; no GC pause > 4 ms; heap bounded over 30 min |
| 4.5 | **Audio:** 8-voice mixer + 1 music stem/sequencer + limiter at 16 and 32 kHz (mic decimated to 16 kHz); touch→sound latency | Audio task CPU < 25 % of one core; no underruns in 30 min; p95 touch→sound ≤ 50 ms (measure with mic or scope) |
| 4.6 | **Toy game in each short-listed stack** (`web` + `esp32-device`): a "tap the bouncing star" game with tween, particles, a scale-locked sound, a hint and a replay test | The same game source runs on both. Record the authoring hours and the friction points |
| 4.7 | **ESP-SR command recognition** with the commands in `config/commands.json` (menu + colours + numbers) | Flash and RAM footprint; quiet-room adult intent accuracy on a recorded test set (deleted after scoring, with consent); latency p95 ≤ 1.5 s after utterance end; coexistence with 4.5 at the chosen I2S rate |
| 4.8 | **Web/phone offline KWS** (tier `web` and `phone-device`) for the same command set | Works in airplane mode; bundle size; latency; licence check |
| 4.9 | **Storage:** asset partition read throughput (SPI flash memory-mapped vs littlefs vs SD) for streaming atlases and audio | MB/s and worst-case stall; choose loading strategy |
| 4.10 | **Power/thermal smoke** (owner bench): current at idle/animated/loud at the chosen SPI and audio rates | Recorded mA; LD1117 temperature after 15 min; no brownout |

If a spike fails its pass criteria, record the failure honestly. Propose a design change (e.g. bake
instead of live vector) or raise a blocking issue. **Do not lower the bar silently.**

## 5. Deliverables
1. `decisions/ADR-0xx-*.md` for every row in §1, using `decisions/ADR_TEMPLATE.md`.
2. `decisions/survey_matrix.md`: candidates × criteria with weighted scores and citations
   (URL + access date + version).
3. `decisions/spike_results.md`: harness location, commit, tier, raw numbers, photos/video links
   provided by the owner.
4. `decisions/licence_register.md`: every dependency, its licence, obligations, and compatibility
   with closed distribution and app stores.
5. An updated `docs/05_architecture.md` §"Chosen stack" that binds the abstract architecture to the
   chosen tech.
6. A summary to the owner listing **the decisions requiring approval** and any requirement that
   cannot be met.
