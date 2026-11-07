# Hatch-a-Pet (G02)

Spec: [v2/games/hatch_a_pet.md](../../../../v2/games/hatch_a_pet.md). Status: **exact port of the
owner-approved browser sketch [`v2/reference/hatch_a_pet.html`](../../../../v2/reference/hatch_a_pet.html)**,
built the same way as Color Pop (`../color_pop`). Where this README and the sketch disagree, the sketch wins.

## Layout of the component
| Part | File | Ported from (sketch) |
|---|---|---|
| Model (deterministic state machine) | `hatch_a_pet.cpp`, `include/little_arcade/games/hatch_a_pet.hpp` | `update(dt)`, tap handlers, `grng` = `mulberry32(seed)` |
| Painter (line-by-line, on `ui/ctx2d`) | `view/hatch_a_pet_paint.cpp`, `view/include/.../hatch_a_pet_paint.hpp` | `drawScene()`, `drawStart()` and every `draw*` helper |
| LVGL view (canvas, tile cache, dual-core raster) | `view/hatch_a_pet_view.cpp`, `view/include/.../hatch_a_pet_view.hpp` | — (mirrors `ColorPopView`) |
| Menu tile art | `view/include/.../hatch_a_pet_art.hpp` | — |
| Audio score | `components/audio/hatch_a_pet_score.cpp` (`HatchAPetScore`) | the Web Audio graph (marimba/glide voices, lullaby, compressor) |

Gameplay randomness uses `mulberry32(seed)`, bit-exact with the sketch. Cosmetic randomness
(confetti, fluff, blinks) uses a separate, seedable stream.

## Behaviour (from the sketch)
- **START → INTRO** (egg drops in over 1.2 s) **→ EGG**. On the device `enter()` starts play directly, so the
  START overlay is only drawn by the reference render.
- **EGG:** each knock squashes the egg and grows the crack. An egg needs 3–5 knocks. A chip shows at ≥ 0.5 crack
  progress, and a peeking eye at ≥ 0.75.
- **PRE_HATCH** (0.4 s shake + trill) **→ HATCH** (1.6 s): the halves fly apart, confetti falls and the creature pops out.
- **PET:** each tap plays the next of 3 reactions (giggle, jump, sneeze). The NO/YES choices appear after 3 reactions
  or 5 s.
  - **NO** (keep playing): hearts appear and the choices come back later.
  - **YES:** **BYE** (0.9 s hop to the next shelf slot) **→ ROLL** (0.9 s, a new egg rolls in) **→ EGG**.
- **Shelf:** 6 slots (the nest occupies the middle). When it is full, the oldest friend leaves and the others glide to their new slots.
- **Badge:** tapping it adds a ring and sparkles. In EGG the egg wiggles and you hear "knock knock"; in PET the creature looks up, giggles and chirps its name.
- **Hint ladder:** after 6 s idle a glow appears; after 12 s a pointing hand.
- There is no failure, score, timer, text or competition. Taps are debounced, and the particle, ring and fluff counts are capped (96, 32 and 8).
- Squash convention: q > 0 stretches (sx = 1 − q, sy = 1 + q).

Creatures (8, from a shuffle bag): BLOOP, FLUFFWING, SPROUTLING, ZIGZAG, PUDDLEPUP, GLIMMERBUG, ROLYPOLE,
SNORKLE. Egg patterns: SPOTS, STRIPES, ZIGZAGS, STARS.

## Hit areas (320×240)
| Target | Rect / circle |
|---|---|
| Egg | `kEggHit` {84, 64, 152, 150} |
| Creature | circle r 52 at (160, 150) |
| NO / YES choices | circles r 38 at (48, 168) / (272, 168) |
| Badge | `kPromptBadge` {132, 4, 56, 56} |

Home and the mic key are painted by the game (as in the sketch); the shared chrome is hidden on this layer.
SceneManager still owns Home and Mic.

## Input map (`Context::General`; touch is complete without voice)
| Action | Touch | Voice intent |
|---|---|---|
| Knock the egg | tap egg | YES, NO or NEXT (EGG) |
| Pet / next reaction | tap creature | YES, NO or NEXT (PET, choices hidden) |
| Keep this pet | NO choice | NO (choices shown) |
| Another egg | YES choice | YES or NEXT (choices shown) |
| Badge (repeat prompt) | tap badge | AGAIN, HELP |
| Skip intro | — | any of the above |

## Audio (`HatchAPetScore`, 16 kHz)
The score uses sine voices with a linear attack and exponential decay, plus exponential glides. Other details:
- at most 8 cue voices;
- an original looping lullaby (F major pentatonic, 72 bpm, 3/4, 20 s loop). It has its own 6-voice pool, a 1.5 s fade-in, ducks to 0.35 under cues, stops on Home, and uses no RNG;
- a block-rate compressor (16 samples, 6 ms look-ahead), output trim 1.4 (was 2.5, which overdrove the speaker), and a master gain that ramps in to 0.35.

The sketch's `setTimeout` audio (for example "knock knock" 200 ms after the badge tap) is scheduled inside the score,
not in game state. Cue mapping is documented in `hatch_a_pet.hpp` (`cue::`).

## Tests and verification
- `hatch_a_pet_test` (host) covers:
  - intro, the START overlay, and knocks/hatch over many seeds;
  - debounce;
  - the reaction cycle, the choices timeout, NO and YES → shelf → ROLL;
  - the full-shelf shift and the shuffle bag;
  - the badge, idle hints, and voice ≡ touch;
  - determinism, the particle cap and SceneManager launch/Home.
- `audio_hatch_a_pet_score_test` (host) covers the score's pitches, envelopes, voice limit, scheduling and level.
- `hatch_a_pet_reference_render` + `tools/quality_bar_golden`: `node golden.mjs hatch_a_pet` steps the sketch and
  the firmware painter in lockstep (script `script_hatch_a_pet.txt`, 33 captures). It compares PNGs, and each frame must
  have mean |d| ≤ 1.5 and ≤ 1% of pixels > 32. This is host evidence only, not device validation. At the owner's
  request this frame-by-frame comparison is optional and no longer a required gate.
- Device perf (COM16, bench auto-play, 10 s windows): 104–126 redraws/10 s, paint 17–21 ms, raster 36–45 ms,
  flush 23–28 ms, `[audio] render_max` ≤ 3.5 ms (was 76–106 redraws, paint 17–31 ms, raster 29–49 ms).