# Hatch-a-Pet native contract

Portable C++ model for [`../../reference/hatch_a_pet.html`](../../reference/hatch_a_pet.html).
`Model::state()` supplies phase, egg identity/pattern/crack progress, pet reaction and transforms,
shell timing, choice state, touch feedback, and the session-only friends shelf to a renderer.
`pointer_down`, `pointer_up`, `tick(seconds)`, and `take_cues()` are the host/view boundary; the
model has no LVGL, audio, platform, wall-clock, or storage dependency. `pop_cue()` provides an
allocation-free cue drain; its bounded queue retains the newest 32 cues. Gameplay uses the reference
mulberry32 stream and shuffle bag; cosmetic timing uses an independently seeded stream.

## Reference rules retained

- Tap anywhere to start. Intro lands at 0.7 s, ends at 1.2 s, and may be skipped. Egg hit box is
  `{84,64,152,150}`; 80 ms knock/pet debounce; seeded hatch need is 3–5 knocks.
- Reference `frame()` advances variable `dt = min(0.05, elapsed_seconds)` with two half-step
  spring integrations (`k=380`, `c=14`); host replay uses 16 ms ticks, never Aquarium's
  50 ms step. EGG visual/spoken hints begin at 6/12 s. Cosmetic wobble repeats after 3–5 s,
  independently of the gameplay RNG used for egg identity, pattern, and knock count.
- Final knock enters a 0.4 s pre-hatch; hatch lasts 1.6 s. During PRE_HATCH, HATCH, BYE, and ROLL,
  gameplay touches stay locked. Every pointer-down still updates `TouchFeedback` immediately so
  the view can acknowledge it without queuing a gameplay cue.
- Pet reactions cycle giggle, jump, sneeze. Choices appear after three reactions or 5 s; touch and
  `Command::Yes/No` share the same 300 ms choice-input gate and decision handlers.
- YES waves the pet to the six-slot shelf, evicts the oldest when full, then rolls in the next egg.
  BYE and ROLL each last 0.9 s. NO keeps the current pet. Home clears the shelf and resets the
  shuffle bag, not the RNG stream.
- Home occupies `{0,0,56,56}`, disabled mic `{264,0,56,56}`, prompt badge `{132,4,56,56}`.
  PET hit radius is 52; YES/NO centres `(272,168)` / `(48,168)` use radius 38, enabled after
  300 ms. Shelf friend hit radius is 16. The reference listens on pointerdown, not drag.
- Cue names are stable identifiers returned with optional variants (`knock` number or creature
  index). The renderer/audio adapter owns synthesis and particles.

## Artwork, motion, and audio

The original Canvas functions yield flash-mapped RGB565 background and RGB565A8 shell, nest,
eight creatures in four face moods, chrome, badge and choice layers. A8 decal masks recolor to
the seeded creature. Shell halves rotate/fall and fade independently; the egg rolls, breathes and
squashes, the pet springs and jumps, shelf friends bob, clouds drift, choices bob, and masked
particles animate in a bounded 64-object cosmetic pool. `tools/export_assets.mjs` records the
original source calls, crop, origin, format, dimensions, hash and byte estimates in
`assets/manifest.json`; its PNGs are generated artifacts. Neither native target needs JavaScript
or a browser at runtime.

Host and device reuse the repository's original Hatch-a-Pet score and limiter, including its
F-major-pentatonic lullaby. Sound begins only after the start touch; device volume comes from
parent settings (default muted). The shared device feedback layer adds a short scene-change tone
on entry/Home, unlike the browser. Device mic remains disabled. Blinking/look-at eye details,
flower sway and dandelion fluff are not yet fully animated in the native layer set; shell graphics,
expressions, patterns and primary motion are captured from the original code. This is a known
fidelity gap, not an assertion of exact visual equivalence.

## Documented differences from the v2 design prose

The runnable reference wins: choice bubbles sit at `(48,168)` and `(272,168)` with radius 38,
not the prose layout; the shelf has six slots, not eight; and the choice appears after three pet
reactions or five seconds. The reference itself has no voice recognizer hooks; `Command` exposes the
specified HELP/AGAIN/YES/NO adapter without changing touch behavior. No recognition service is
enabled in this isolated target; touch is complete. The design's 64-particle budget is stricter
than the reference's `deviceMaxParticles=96`; native caps at 64. The reference's 8-voice score
pool and compressor remain outside gameplay logic in the reused platform score.

## Host verification

`test/model_test.cpp` is a standalone host test. Compile it with `model.cpp` and C++17; it exercises
seeded shuffle/no-repeat behavior, phase timing, debounce, empty/locked touches, idle help, choice
routes, shelf/home behavior, and cosmetic/gameplay RNG separation. This is host-only evidence, not
device validation. `tools/visual_compare.mjs` compares nine seeded browser/LVGL scenes against
measured game-specific mean RGB differences of 5–12 (of 255), and 0.5–8% of pixels with >40
channel error, depending on scene. The YES rolling-egg scene remains the largest deviation.
These are **web/host** captures, not physical display/touch/audio measurements.
