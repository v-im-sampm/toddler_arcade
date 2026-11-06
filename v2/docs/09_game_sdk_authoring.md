# 09: Game SDK and authoring workflow

Goal: **a new game in days.** A designer/developer (or an LLM) scaffolds a game, sees it live in a
browser with hot reload, tunes it, adds assets, writes replay tests, checks budgets, and runs it on
the device. They never touch shell code.

## 1. Anatomy of a game
```
games/<game_id>/
  manifest.json          # validated by schemas/game.manifest.schema.json
  DESIGN.md              # copied from v2/games/<game_id>.md and kept current
  logic/                 # game rules + state machine (language per ADR-001/005)
  scenes/                # scene/layout descriptions (data), node names, hit areas
  tuning.json            # all gameplay numbers (counts, timings, difficulty ramp) — hot-reloadable
  assets-src/            # editable sources (vector/layered/animation/audio project files)
  assets/                # generated per-platform packs (git-ignored or LFS; per ADR-008)
  narration.csv          # prompt_id, text, variant, notes (input to recording/TTS)
  tests/
    rules.test.*         # unit tests of pure rules
    replays/*.replay     # seed + timed input script + expected checkpoints
```
- **Logic** holds pure rules: state, input → state transitions, and requests for effects (animate X,
  cue Y). It is unit-testable without rendering.
- **Scenes** are declarative: nodes, layers, anchors, hit areas, animation clip bindings and particle
  emitters. They are editable in a scene preview tool or by hand.
- **Tuning** holds every magic number from v1 READMEs (e.g. "egg needs 3–5 taps", "celebrate 1.8 s").
  It lives in data, not code.
- **Quality evidence** is part of the design, not a polish pass at the end. Use
  [`games/_TEMPLATE.md`](../games/_TEMPLATE.md) and the
  [quality-bar sketch](../reference/quality_bar.html) to specify responsive feedback, calm idle
  animation, a kind hint ladder, scale-safe audio, reduced motion, and measurable budgets.

## 2. Scaffolding
`tools/new-game <game_id> --template <tap-choice|free-play|music-toy|sequencer|character-pet>`
creates the folder, a manifest with placeholders, a state machine stub, placeholder assets
(coloured shapes with the correct IDs and sizes), a narration sheet, and a passing replay test.

Templates map to v1 archetypes:
| Template | v1 examples | Provides |
|---|---|---|
| `tap-choice` | Color Pop, Animal Sounds, Feed the Monster, Shape Garage | prompt → 2–4 options → hint ladder → celebrate; difficulty ramp 2→4 options |
| `free-play` | Bubble Aquarium, Rocket Rescue | ambient sim, pooled entities, discovery events |
| `music-toy` | Rainbow Piano, Sound Jar | scale-locked pads, instrument switch, optional follow-me |
| `sequencer` | Beat Sharks | music-clock grid, cell cycling, playhead |
| `character-pet` | Robo Buddy, Hatch-a-Pet, Silly Faces | character state machine, moods, look-at, reactions |

## 3. Live preview loop (web)
- `tools/dev <game_id>` starts a local dev server (localhost only), opens the game in the
  **web PAL** with:
  - hot reload of `tuning.json`, scenes and assets (and logic if the runtime allows; ADR-005);
  - a **device-fidelity mode**: 320×240, 30 FPS cap, RGB565 quantise + dither preview, 16 kHz (or
    the chosen device rate) audio, mono, and a speaker EQ simulation;
  - an overlay showing dirty rects, frame time, particle count, the active audio voices, and the
    state machine with its current state;
  - input injection: click = touch, keys = intents, and record/replay of an input timeline into a
    `.replay` file;
  - a **budget panel**: estimated device flash/PSRAM for the game's current assets.
- Device-fidelity mode is a *preview*. It is never evidence for device performance (evidence tiers).

## 4. Testing
| Layer | Tool | Runs where |
|---|---|---|
| Rules unit tests | language-native test runner | host CI |
| Replays | Headless engine runs a seed + input script at fixed step; asserts state checkpoints; captures frames at marked times | host CI (all games, every PR) |
| Golden images | Frames from replays compared perceptually (e.g. SSIM ≥ threshold) per platform renderer | host CI |
| Golden audio | Render audio for replays; compare loudness/spectral fingerprints; check onsets ≤ limits | host CI |
| Safety lint | Flash/luminance analyser over replay frames; target size/spacing checker over scene hit areas; colour-only-cue checker for answer sets | host CI |
| Quality replay | Cover target and empty-space taps, success, repeated misses, idle hint, auto-help, rapid input, and reduced motion; assert immediate acknowledgement, cue pairing, and no input block over 300 ms outside a documented window | host CI + review |
| Budget | Asset pack sizes vs manifest budgets; code size per game (device build) | CI device build |
| Device bench | Same replays on the board via a serial/USB command; logs FPS, frame-time histogram, particle count, heap/PSRAM minima, audio CPU | owner-run, evidence tier `esp32-device` |

## 5. Definition of done for a game
- [ ] Manifest validates; capabilities minimal
- [ ] All acceptance criteria in its design doc pass (replays reference criterion IDs)
- [ ] Touch-complete with voice off; playable with audio off; playable with mic off (if relevant)
- [ ] Per-screen review in `docs/06` passes: idle/press/release/success/hint states, touch feedback on misses, and reduced-motion behavior
- [ ] Quality replay covers the hint ladder, rapid taps, empty taps, and cue/visual pairing; animation never blocks input beyond its documented window
- [ ] Safety lint, golden tests and budgets green
- [ ] Narration recorded/rendered, loudness-normalised, provenance recorded
- [ ] Web + phone + device builds run it. Device evidence attached: FPS p95, worst frame, heap/PSRAM
      minima, touch→sound latency
- [ ] Design review sign-off (visual checklist in `06` §7) and a toddler play-test note (parent
      observation, no recordings of the child)

## 6. SDK versioning and governance
- Semver. The SDK API surface is documented and generated (reference docs from source).
- Breaking changes need: an ADR, a migration note, and updates to all games in the same PR series.
- Games may not reach into engine internals. Lint/CI enforce import boundaries.

## 7. Prompting an LLM to make a new game (template)
> Create a new Little Arcade v2 game `<id>` using `tools/new-game --template <t>`. Follow
> `v2/games/_TEMPLATE.md` to write `DESIGN.md` first and get it approved. Implement logic and
> scenes against the SDK only, put every number in `tuning.json`, declare assets in the manifest
> with budgets, add narration lines, and write replays covering each acceptance criterion, including
> immediate touch feedback, empty-space taps, the complete hint ladder, reduced motion, and rapid
> input. Use `reference/quality_bar.html` as a feel reference, not as production code or a source of
> copied art. Meet the definition of done in `docs/09` §5. Do not edit shell, engine or other games.
