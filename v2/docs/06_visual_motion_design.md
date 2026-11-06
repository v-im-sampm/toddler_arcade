# 06: Visual and motion design system

Numeric values live in [`../design/tokens.json`](../design/tokens.json). This document explains them.
Generated constants are the only way code should consume them.

The runnable [`../reference/quality_bar.html`](../reference/quality_bar.html) is a feel reference,
not production code. Match its immediacy, calm idle life, tactile feedback, coherent sound, and
readable help; do not copy its character, scene, or implementation.

## 1. Art direction: "Toy Box Island"
All games live in one original world, **Toy Box Island**: a soft, rounded, sunny place made of
felt, wood and jelly. Each game is a spot on the island:
- Color Pop: Bubble Beach
- Beat Sharks: Shark Lagoon
- Rocket Rescue: Star Hill
- Robo Buddy and Shape Garage: Bolt's Workshop
- Feed the Monster: Snack Shack
- Animal Sounds: Farmyard
- Hatch-a-Pet: Egg Nest
- Rainbow Piano: Rainbow Bridge
- Silly Faces: Mirror Pond
- Bubble Aquarium: Reef
- Sound Jar: Echo Cave

The launcher can be an illustrated island map, with tiles as places, or a carousel. Decide in design
review.

**Shape language:**
- Circles and rounded rectangles dominate.
- The minimum corner radius is 25 % of the shorter side for props.
- Characters are 2–3 heads tall with big eyes (≥ 18 % of the face width) and a simple mouth.
- No sharp points except deliberate stars.
- No realistic textures. Use flat fills, one shadow tone, one highlight tone, and an optional soft
  gradient on large shapes.

**Line:** a coloured outline, darker than the fill (fill × 0.55 lightness). Weight scales with
object size: 2 px at 48 px tall on the 320×240 stage, scaling with density on web/phone. Never use
pure black (#000) lines.

**Cast (original, reusable across games):**
| Character | Role | Notes |
|---|---|---|
| **Bolt** | Friendly robot (v1 Robo Buddy) | Rounded TV-head, antenna bulb shows mood colour. Avoid any resemblance to existing film robots |
| **Munch** | Hungry monster (v1 Feed the Monster) | Round, fuzzy-edged, one big mouth, tiny horns |
| **Pip & friends** | Creatures (v1 Hatch-a-Pet 8 creatures) | Bloop, Fluffwing, Sproutling, Zigzag, Puddlepup, Glimmerbug, Rolypole, Snorkle |
| **Finn** | Shark band leader | Rounded, smiling, no teeth shown |
| **Narrator** | Off-screen voice | Warm, upbeat, unhurried. Optionally appears as a small helper icon (a speaking star) when prompting |

A **character bible** (turnarounds, expression sheet, colour callouts) is an early deliverable
(ticket ART-01). Every character needs:
- neutral, happy, surprised and sleepy expressions,
- a blink,
- a look-left/right/at-touch pose,
- a "listening" pose for voice.

## 2. Colour
- The palette in `tokens.json → color` has 12 hues × 3 tones (shadow/base/light), plus neutrals and
  semantic roles: background sky, ground, interactive highlight, and hint glow.
- **Answer colours** (red, blue, green, yellow) are tuned for distinguishability under protanopia,
  deuteranopia and tritanopia (simulate in CI). Each pairs with a **pattern and shape**:
  - red ♥ heart
  - blue ~ wave
  - green 🍃 leaf
  - yellow ★ star
  This is v1 parity.
- **Device colour:** RGB565 quantises gradients. Use ordered dithering (4×4 Bayer) at export for
  gradients, and avoid large subtle gradients in device art. Test on the real panel, since IPS
  gamma and BGR/inversion settings differ from desktop.
- Brightness: avoid full-screen pure white. The background maximum luminance token keeps the
  screen comfortable at night (target: background L* ≤ 92).

## 3. Layout
- Logical stage: 320×240 units, 4:3, origin top-left.
- **Safe chrome:** Home (0,0,56,56), Mic (264,0,56,56). The play area is y ≥ 64 by default; games
  may use the 64–264 top band for in-game controls (v1 parity).
- Wider screens: the stage is centred and scaled to fit height. Side bleed shows a background
  extension (layer `bleed`) and never shows interactive elements.
- Targets: ≥ 48 units (prefer 56–72), spacing ≥ 8. Hit areas may extend up to 6 units beyond the
  visual for forgiveness (v1 Shape Garage).
- Composition: one focal element at a time. The prompt target is centred or highlighted, and the
  answer options sit in the lower 60 % of the screen where small hands rest.

## 4. Motion language
### 4.1 Principles (for a 3-year-old)
1. **Anticipation → action → follow-through.** For example: press squash (60 ms) → release stretch
   + overshoot (220 ms) → settle.
2. **Everything breathes.** Idle characters use a 2.4–3.2 s breathing scale (±2–3 %) and blink
   every 2–6 s at random from a shuffle bag. Props bob ±2 units.
3. **Respond where the finger is.** A touch ripple appears at the contact point, and characters
   look toward the touch.
4. **Calm motion amplitude.** No screen shake by default (opt-in "big celebration" may shake ≤ 2
   units for ≤ 300 ms, and it is disabled under reduced motion). No spinning faster than 1 rev/s.
5. **Readable timing.** Important reactions last ≥ 400 ms so a toddler sees them. Celebrations run
   1.2–2.0 s and are skippable by tapping (never block input longer than 300 ms).
6. **Feedback is complete.** Every primary object defines idle, press, release, success, and hint
   states. Every touch is acknowledged at its location, including a tap on empty space. The input
   response is drawn immediately; longer effects continue without holding the input path.
7. **Hints reduce effort, not agency.** State the idle visual hint, optional spoken reminder, and
   auto-help timing in the game tuning. Keep the requested target identifiable without relying on
   sound, reading, or hue alone.
8. **Reduced motion is a designed mode.** Keep state changes and target cues clear while reducing
   ambient movement, particle count, and large celebrations according to the safety tokens.

### 4.2 Motion tokens (see `tokens.json → motion`)
| Token | Value | Use |
|---|---|---|
| `dur.instant` | 60 ms | press squash |
| `dur.quick` | 140 ms | hover/highlight, small UI |
| `dur.base` | 220 ms | release, pop-in |
| `dur.gentle` | 400 ms | entrances, hints |
| `dur.scene` | 450 ms | scene transitions |
| `dur.celebrate` | 1500 ms | standard celebration |
| `ease.out` | cubic-bezier(0.22, 1, 0.36, 1) | entrances |
| `ease.in` | cubic-bezier(0.64, 0, 0.78, 0) | exits |
| `ease.inOut` | cubic-bezier(0.65, 0, 0.35, 1) | moves |
| `ease.backOut` | cubic-bezier(0.34, 1.56, 0.64, 1) | pop / overshoot |
| `spring.bouncy` | stiffness 380, damping 14, mass 1 | jelly reactions |
| `spring.soft` | stiffness 160, damping 18, mass 1 | follow / look-at |
| `squash.press` | scale (1.12, 0.88) | press |
| `squash.land` | scale (1.18, 0.82) → (0.94, 1.06) → 1 | landing |
| `wiggle.hint` | rotate ±6°, 3 cycles, 360 ms | gentle "not that one" |
| `glow.hint` | 0→1→0 alpha pulse, 900 ms period (≈1.1 Hz) | hint target (well below the 3 Hz flash limit) |

### 4.3 Standard reactions (SDK presets, so all games feel related)
| Preset | Composition |
|---|---|
| `react.tap` | ripple at touch + press squash + sfx `tap` pitched to scale degree |
| `react.correct` | backOut pop + sparkle burst (12–20 particles) + sfx `success` + narrator praise (shuffle bag) |
| `react.notThis` | `wiggle.hint` + soft sfx `boop` + the target begins `glow.hint` |
| `react.autoHelp` | the target scales to 1.15 and pulses; a helper arrow/hand bobs towards it |
| `celebrate.small` | confetti (≤ 40 particles), character happy, 1.2 s |
| `celebrate.big` | stars + music sting + character dance, 2.0 s, skippable |
| `transition.bubble` | circular iris from the tapped tile, 450 ms; no white frame |

### 4.4 Particles
Use data-defined emitters with pooled particles. Device cap: 96 live particles total, 48 per emitter
(tune in spike 4.2). Shapes: circle, star, heart, note, bubble ring, confetti rect. Fields:
- rate/burst, lifetime range, velocity cone, gravity, drag;
- spin, scale-over-life curve, colour ramp (palette indices), additive flag.

Each game documents its worst-case concurrent emitters and uses a hard cap; reaching the cap must
degrade gracefully (for example, skip the oldest cosmetic emission), never allocate in the frame
path or delay input.

### 4.5 Shared firmware visual kit
For ESP-IDF/LVGL games, use the shared renderer primitives in
`firmware/components/ui/include/little_arcade/ui/`: `tokens.hpp` for palette and motion values,
`shade.hpp` for shaded original art, `motion.hpp` for bounded transforms, `particles.hpp` and
`fx.hpp` for pooled effects, and `scene_art.hpp` for the shared world backdrops. A game may add
game-specific art and reactions, but must not replace these with flat placeholder circles or a
single solid-color screen.

Every rendered scene must have a recognizable setting with at least two tonal layers, a focal
character or object with a darker colored outline, highlight and shadow, and the same cream/ink
chrome language. Answer objects remain readable without color alone. Effects are additive: keep
the view responsive, reuse the fixed pools, and paint static art at scene creation rather than in
the frame loop. The current firmware particle layer caps a scene at 48 live particles; changing that
cap requires device-budget evidence.

**The device must match the browser reference exactly.** When a game has a reference sketch (Color Pop:
[`quality_bar.html`](../reference/quality_bar.html)), the firmware is a line-by-line port of it,
not a reinterpretation. The device and the desktop simulator run the same code and must look and
sound identical. Workflow (proven on Color Pop):
1. **Port the sketch's canvas drawing** to `ui/ctx2d` (a canvas-2D-compatible software renderer:
   paths, arcs, gradients, alpha, `globalCompositeOperation`, ordered dither to RGB565). Keep the
   painter (`<game>_paint.cpp`) in the same order and with the same constants as the JS `draw()`.
2. **Port the model** (`<game>.cpp`) with the same timings, easing and seeded randomness. Use the
   game logic's own PRNG, never `Math.random`-style drift.
3. **Port the Web Audio score** (`audio/<game>_score.cpp`) node-for-node: oscillator types,
   envelopes, filters, compressor and master gain.
4. **Golden diff:** add a scripted replay to `tools/quality_bar_golden` (Edge captures the
   reference; the host binary renders the same frames). Every frame must pass the thresholds in
   `golden.mjs` (mean |diff| ≤ 1.5 and ≤ 1 % of pixels off by more than 32; Color Pop reaches
   PSNR 42–46 dB) before the game ships. Open
   `out/report.html` for the side-by-side.
5. **Owner review on the panel.** The golden diff proves the code; only the owner on real hardware
   signs off the look, motion and sound.

**Device performance rules** (ESP32-S3, measured on Color Pop):
- Use `float` only in per-frame and per-sample code. The FPU is single-precision; `double` math
  is emulated in software and was about 400× slower than on the host.
- Redraw only dirty tiles (`ctx2d` tile cache), and use `second_core_worker()` so both cores raster.
- Audio scores must render a 16 ms block in well under 16 ms (Color Pop takes about 2 ms). Run
  compressors and envelopes at block rate, precompute phase increments, and use no per-sample
  `pow`/`log`/divides. Underruns sound "digitized".
- Check the `perf` and `[audio]` serial logs on the device for every game before review.

Simulator screenshots are host evidence only; real-panel quality and performance
still require an on-device review.

## 5. Rendering strategy on the device (constraints from `01_v1_baseline.md` §2)
- A full-frame push at 40 MHz SPI (validated on one board) is ≈ 31 ms (≈ 45 ms at the v1 27 MHz), so
  **design scenes so that most frames touch < 35 % of the screen**:
  - static or slow backgrounds are cached;
  - ambient motion comes from small sprites (clouds, bubbles), not full-layer scrolling;
  - parallax scrolling happens only during transitions.
- Transitions that must touch the full screen (iris, wipe) are designed to look intentional at
  20 FPS, using motion-blur-free shapes.
- Prefer **baked frames** for complex character animation on the device. Use **runtime transforms**
  (scale/rotate/translate) for squash/stretch and bob. The pipeline can pre-bake rotated variants if
  the spike shows rotation is too expensive.
- Each game's "worst frame" is listed in its design doc and tested by a golden-replay benchmark on
  the device.

## 6. Asset production
- Source of truth: vector or layered files (tool chosen in ADR-004) stored under
  `games/<id>/assets-src/` with an export script. No hand-written pixel arrays in code (a v1 lesson).
- Exports: device 1× (320×240 stage), web/phone 2× and 3×.
- Every asset has a provenance record (author/tool/licence/date/hash) per
  `schemas/asset.provenance.schema.json`.
- AI image generation is allowed only if ADR-008 approves a tool whose licence permits commercial use
  and whose output a human artist edits for consistency. Record the provenance, and never prompt
  for an existing IP style or character.

## 7. Review checklist (per screen)
- [ ] One focal point; silhouettes readable as a 32 px thumbnail
- [ ] Colour + shape/pattern cues; passes CVD simulation
- [ ] Every primary object has idle, press, release, success and hint states; each state is reachable and visibly distinct
- [ ] Touch ripple or equivalent immediate acknowledgement appears on target and empty-space taps
- [ ] Idle visual/spoken hints and auto-help are timed, gentle, and replay-testable
- [ ] Sound cues are scale-safe, bounded, and paired with visible feedback; audio-off play is complete
- [ ] Reduced-motion mode preserves all essential cues and stays within the motion safety tokens
- [ ] No flashes > 3 Hz; no large red flash; luminance check passes
- [ ] Particle/entity/voice caps and device worst-frame dirty area are recorded and within budget
- [ ] 320×240 screenshots show the intended depth, palette and focal hierarchy in idle, interaction,
      hint and celebration states; screenshots are reviewed for clipping and visual flatness
- [ ] Re-entry recreates the scene without stale view state, leaked LVGL objects or a blank backdrop
- [ ] Looks right on the real panel (owner photo) and at 3× on phone
