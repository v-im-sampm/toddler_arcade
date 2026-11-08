# 01: v1 baseline (what exists, what we learned)

v1 was a **proof-of-concept that ran on the physical board**. Its game, launcher, simulator, and
PixelRoot32 source code have been removed; only shared device-support components still required by
the isolated v2 Bubble Aquarium build remain. Treat everything here as historical input, not as a
constraint on v2's design, except the hardware facts in §3.

## 1. Inventory
| Area | v1 implementation | Location |
|---|---|---|
| Toolchain | ESP-IDF v5.5.6, C++17, LVGL 9.5.0, esp_lvgl_port 2.9.0, esp-sr 2.6.1 (resolved, **not called**) | `../firmware/README.md`, `../firmware/dependencies.lock` |
| Build | Historical v1 image used ESP-IDF in a cached WSL container image (`wslc`) and pinned esptool; the launcher build scripts were removed | Current isolated v2 build: `../native/bubble_aquarium/device/` |
| Core contract | Historical `IGame`/`GameContext` contract; source removed | Former v1 core component |
| Shell | Historical `SceneManager`: launcher with 4 tiles/page, Home/BACK, pause, 80 ms debounce, 10 min sleep, voice context filter; source removed | Former v1 core component |
| Input | Historical `InputEvent { source, intent, x, y, timestamp }`; touch was **press-edge only**, so drag was impossible; source removed | Former v1 core component |
| Audio | 16 kHz mono; a 4-voice procedural `Synth` (sine/triangle/pulse/noise, glides, envelopes, PCM clips); per-game `sound_bank`; a 2-voice `Sampler` (pitch, reverse, echo, ring-mod, vibrato); a `VolumeLimiter` (muted start, levels 0–6 at 4 dB steps, look-ahead limiter, amp-safe ceiling from board JSON) | `../firmware/components/audio/` |
| Visuals | Historical LVGL views and procedural raster art; source removed | Former v1 UI and game view components |
| Voice | Historical policy-only `PushToTalkSession` and `VoiceIntentFilter`; ESP-SR was not integrated; source removed | Former v1 speech component; see `../../docs/voice.md` for the retained v1 policy notes |
| Intents | `config/commands.json` (28 intents, aliases, contexts) → generated `intent.hpp` | `../config/commands.json`, `../tools/gen_firmware_headers.py` |
| Settings | Hidden parent gate (hold Mic corner 5 s + sum challenge), volume 0–6, mic toggle, persisted in NVS | `../firmware/components/settings/` |
| Simulator | Historical SDL2 desktop sim; source removed | Former v1 simulator |
| Tests | Historical v1 component/game test suites; source removed. Current Bubble Aquarium host tests live with its v2 source | `../native/bubble_aquarium/test/` |
| Mockups | Historical root mockups removed; the v2 sketches remain available | `../reference/` |
| Hardware | Yellow carry-handle OpenSCAD enclosure, STEP exports, fit-check PDF | `../hardware/enclosures/` |

### Games (12 designed, 9 in the shell)
| Game | Shell | View | Core idea (v1) |
|---|---|---|---|
| Color Pop | ✅ | ✅ | Pop the requested colour bubble (2→4 bubbles; colour + pattern) |
| Hatch-a-Pet | ✅ | ✅ | Tap egg 3–5×, hatch 1 of 8 original creatures, pet it |
| Rainbow Piano | ✅ | ✅ | 5 shaped keys × 3 instruments, optional follow-the-lights |
| Robo Buddy | ✅ | ✅ | Pet robot "Bolt" with 5 moods, feed/sleep/dance |
| Shape Garage | ✅ | ✅ | Tap-to-place circle/square/triangle onto a robot |
| Who Made That Sound? | ✅ | ✅ | 8 CC0 animal calls; pick the animal card |
| Feed the Monster | ✅ | ✅ | Count 1–3 snacks into a monster |
| Sound Jar | ✅ | ✅ | Record ≤ 3 s, play back on 5 pitch pads + 4 effects |
| Beat Sharks | ✅ | ✅ | 3×6 step sequencer with sharks, one recordable row |
| Rocket Rescue | ❌ | ❌ | Fly between 3 planets, rescue 3 friends, collect stars |
| Silly Faces | ❌ | ❌ | Cycle eyes/mouth to make 4 emotions |
| Bubble Aquarium | ❌ | ❌ | Open-ended fish tank with bubbles, food, visitors |

Detailed per-game facts are in each v2 game doc (`../games/*.md`, "v1 baseline" section).

## 2. Measured device evidence (one board, N16R8)
| Metric | Value | Notes |
|---|---|---|
| App image | 605,065 B | At the H02 point; grew later with more games. Re-measure |
| Internal heap at UI ready | free 243,511 B, min 178,780 B, largest block 128 KiB | |
| PSRAM free | 8,385,768 B | Before game content. PSRAM was essentially unused by v1 |
| UI task stack | 11.7 KB free | |
| Color Pop render + flush | avg 17.3 ms, max 42.1 ms, ≈ 4.9 K px per redraw | 10 s session, partial redraws |
| SPI | 27 MHz in v1; 40 MHz used by the v2 Bubble Aquarium device build (see `../native/bubble_aquarium/device/README.md`); BGR, inverted, landscape via swap+mirror | |
| Audio | Full-duplex I2S0 at 16 kHz; game cue peak ≈ −18.6 dBFS at level 4; no brownout | Owner confirms audible. SPL, current and pop are **unmeasured** |

**Derived constraint (important for v2):** a full 320×240 RGB565 frame is 153,600 B. At 27 MHz SPI
that takes ≈ 45 ms to push, so **full-screen redraw caps at ≈ 22 FPS** before any rendering cost.
Smooth, rich animation on this board therefore depends on:
- dirty-rectangle / partial updates,
- double-buffered DMA overlapped with rendering,
- possibly a validated higher SPI clock.

Treat any design that redraws the whole screen every frame as infeasible until a spike proves
otherwise.

## 3. Hardware facts that carry forward (source: `../config/board_pins.json`)
- **Display:** ST7789V (native 240×320 portrait, used in landscape), SPI. SCLK 14, MOSI 13,
  MISO 12 (via diode), CS 15, DC 21, RST 1, backlight 48 (via an S8050 low-side switch; PWM dimming
  is unvalidated). The tearing-effect (TE) pin is **not known to be routed: treat as TBD**.
- **Touch:** NS2009 resistive, I2C 0x48, SCL 9, SDA 10, PENIRQ 11 (unused in v1). Single point, no
  multi-touch, and pressure is available from the controller but unvalidated.
- **SD:** SDIO 1-bit (CLK 42, CMD 2, D0 41). It is present but unused in v1.
- **Audio (owner-wired, user-reported):** INMP441 mic and MAX98357A amp share BCLK = GPIO4 and
  WS = GPIO5. Mic SD = GPIO6, amp DIN = GPIO7. Amp SD_MODE is hard-wired on (no mute GPIO). GAIN is
  12 dB. The amp runs from the 3.3 V header rail (LD1117 headroom unmeasured).
  - ⇒ **TX and RX share one sample rate.** Raising playback quality above 16 kHz means running the
    bus faster and decimating the mic for speech. That is a spike item.
  - ⇒ **No hardware mute.** Startup-pop avoidance depends on keeping I2S clocks idle until firmware
    is ready.
  - ⇒ The digital ceiling at 12 dB on 3.3 V is ≈ −8 dBFS (`VolumeLimiter::amp_safe_peak`).
- **Free GPIOs on headers:** 4, 5, 6, 7 are all used by audio. No spare header GPIO exists. Any v2
  hardware addition (e.g. a button, haptics, or amp mute) needs a board-level plan and owner approval.
- **Power:** USB-C 5 V → LD1117 3.3 V. The enclosure plans a 1S LiPo, TP4057 charger and 5 V boost,
  which are TBD.

## 4. What worked (keep)
1. **Intent enums from one manifest.** Game code never sees strings. Context filtering in the shell.
2. **Frozen game lifecycle + host-testable pure logic** separated from views. Deterministic RNG.
   Scripted bench runs.
3. **Toddler UX rules:**
   - ≥ 48 px targets.
   - Starter content so nothing is ever empty or silent (Beat Sharks starter beat, Sound Jar built-in boing).
   - Wrong answers wiggle and hint, then auto-help after a second miss.
   - Shuffle bags prevent repeats.
   - Idle hints are visual first.
4. **Audio safety chain:** muted start, ramped volume, look-ahead limiter, amp-aware ceiling,
   int32 mixing.
5. **Privacy design for recording games:** RAM-only, zeroing on every exit path, and a parent toggle.
6. **Owner-evidence discipline:** `verified` flags, evidence tiers, no invented pins.
7. **Sampler DSP** (silence trim, normalise, fades, pitch/reverse/echo/ring/vibrato). It is proven and
   worth porting as-is.

## 5. What limited quality (fix in v2)
| Problem | Effect | v2 direction |
|---|---|---|
| Art is hand-coded procedural paint in C++ headers | Slow to author, flat look, no artist workflow | Real asset pipeline from authoring tools to optimised atlases per platform (`06`) |
| Animation is ad-hoc per view | Inconsistent feel, no squash/stretch or easing language | Shared tween/timeline/state-machine animation system with motion tokens |
| 16 kHz mono 4-voice synth, no music | Thin sound, no musical identity, cues don't harmonise | Mixer with buses, a key/tempo-aware music system, richer instruments, possibly a higher rate (`07`) |
| No spoken prompts | Games rely on motifs; a toddler gets less guidance | Recorded or offline-TTS voice prompts with a consistent narrator persona |
| Press-only touch | No drag, hold, slide (except special-cased in Sound Jar) | Pointer phases: down/move/up/cancel, plus gesture helpers |
| Game registration hard-coded in `Arcade` (layers, includes, enums) | Adding a game edits shell files | Data-driven registry from manifests; codegen |
| Coordinates hard-coded at 320×240 | Can't scale to phone/web | Logical 320×240 design space + resolution-independent rendering + safe areas |
| Desktop sim only, no web/phone | No fast iteration, no demo | Web target as the primary authoring loop |
| Voice never integrated on device | Unknown feasibility | Phase 0 spike with real models and measurements |
| 3 games logic-only | Uneven catalogue | All 12 shipped to the same quality bar |

## 6. Open v1 items that still matter
- H02 is still open: measured 5-point touch calibration, backlight level, PCB revision, touch latency.
- Audio is unmeasured: SPL at the ear, power-on pop, amp current, LD1117 temperature.
- ESP-SR model choice, size and accuracy (V01): never done.
- The 2 h soak, sleep/resume and low-voltage recovery (Q01): never done.
