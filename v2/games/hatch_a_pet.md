# Hatch-a-Pet: v2 design

| Field | Value |
|---|---|
| Game id | `hatch_a_pet` |
| Template | character-pet |
| World location | Egg Nest |
| Play value | Anticipation, cause-and-effect, yes/no choices, describing patterns |
| Capabilities | music, narration, voice_intents, particles |

**As built (owner direction, 2026):** the shipped Hatch-a-Pet is an exact port of
[`hatch_a_pet.html`](../reference/hatch_a_pet.html). Its scene, egg, creatures, motion and Web Audio score are
reproduced line by line, just like Color Pop:
- model: `firmware/components/games/hatch_a_pet/hatch_a_pet.cpp`;
- painter: `firmware/components/games/hatch_a_pet/view/hatch_a_pet_paint.cpp`, drawn with `ui/ctx2d`;
- score: `firmware/components/audio/hatch_a_pet_score.cpp`;
- verification: host behaviour/score tests and device perf/audio measurements. The golden harness
  (`node golden.mjs hatch_a_pet`) passed 33/33 frames at port time. At the owner's request, frame-by-frame /
  pixel-diff golden analysis is no longer a required check; the harness is kept as an optional tool.

Notable differences from the design below:
- The shelf has 6 slots instead of 8, because the nest occupies the middle. When it is full, the oldest friend leaves.
- The NO/YES choices appear after 3 pet reactions or 5 s (owner playtest: 8 s felt too long).
- The breeze noise bed was replaced by an original, looping lullaby (F major pentatonic, 3/4 at 72 bpm, 20 s loop:
  a soft sine melody with a low pad). It has its own 6-voice pool outside the 8-voice cue limit. It fades in over
  about 1.5 s, ducks to 35% while cues play, stops on Home, and uses no RNG (device and browser match).
- Clipping fix: output trim 2.5 → 1.4 and a 6 ms compressor look-ahead (as Chromium's DynamicsCompressor). A busy
  host render no longer reaches the limiter (peak 0.36 FS, was 0.66 FS with 429 samples limited).
- Performance: the model and painter use float per frame. Hatch-a-Pet paints with a 0.2 px curve flatten tolerance
  (ctx2d default 0.05 is unchanged for Color Pop). The UI loop yields only 1 ms between canvas-game frames.

Where this design and the reference disagree, the reference wins.

## 1. Pitch
Knock on a wobbly egg until it cracks open and a surprise creature tumbles out to say hello.
*Feels like:* opening a present that is happy to see you.

## 2. v1 baseline
- The egg needs 3–5 taps, then a 1.6 s hatch.
- 8 creatures: BLOOP, FLUFFWING, SPROUTLING, ZIGZAG, PUDDLEPUP, GLIMMERBUG, ROLYPOLE, SNORKLE.
- Patterns: SPOTS, STRIPES, ZIGZAGS, STARS.
- A shuffle bag prevents repeats.
- After the hatch, YES (64×64) gives another egg and NO stays with the pet. Voice: YES, NO, AGAIN,
  HELP.

## 3. Experience
### 3.1 Core loop
1. A patterned egg sits in a straw nest, breathing (scale 1.00↔1.02) with an occasional wobble.
2. Each tap is a knock. The egg squashes, a woodblock note climbs the scale, and the crack grows by
   one stage: the stages are hairline, zigzag, a chip pops off, then a peek-eye looks out.
3. On the final tap, the egg shakes in anticipation for 400 ms (rising celesta trill), then the
   shell bursts into two halves plus confetti in the pattern colour. The creature does a "ta-da"
   pose and says its name sound (a nonverbal chirp; the narrator then names it).
4. **Pet time (new in v2):** tap the creature for 3 reactions (tickle-giggle, jump, sneeze-sparkle).
   It follows the finger with its eyes. After 3 reactions or 5 s, two bubbles float up: ✔ another egg
   (a new egg icon) / ✖ stay (a heart icon).
5. ✔ → the creature waves and hops into a "friends shelf" row at the bottom (a session-only memory,
   RAM only, cleared on exit). A new egg rolls in. ✖ → keep petting.

### 3.2 States
`EGG_IDLE → KNOCKING(n/3..5) → PRE_HATCH(0.4 s) → HATCH(1.6 s) → PET → CHOICE → (EGG_IDLE | PET)`

### 3.3 Layout
```
y 0-56     [HOME]                                   [MIC]
y 56-200   nest + egg/creature centred (egg 120×140 visual, 160×176 hit)
y 200-240  friends shelf (up to 8 mini creatures, decorative; tap = tiny wave)
CHOICE: ✔ bubble {72,120,72,72}  ✖ bubble {176,120,72,72}
```

## 4. Characters and animation
| Element | Idle | Press | Success | Hint | Other |
|---|---|---|---|---|---|
| Egg | breathe + wobble every 3–5 s | `squash.press` + crack grows | burst open | wobble harder + glow | the pattern decal matches the creature inside |
| Creature | breathe, blink, look-at | 3 reaction variants | ta-da pose | wave | each has its own silhouette, colour pair, and a chirp at a scale degree |
| Shell halves | — | — | spin/fall with gravity, fade | — | particles `confetti` in pattern shapes |
Device: 8 creatures × 4 poses as baked sprite sheets. Crack stages are overlays.

## 5. Audio
- Identity: F major pentatonic, 92 BPM. Woodblock knocks rise one degree per knock. Celesta trill on
  pre-hatch. Bed: soft nest birds.
- Cues: `knock.n` (degree n), `pre_hatch_trill`, `hatch_pop`, `chirp.<creature>`, `giggle`,
  `choice_appear`, `wave_bye`.
- Narration: "Knock knock!", "Who's inside?", "It's a <name>!", "Another egg?", praise variants.

## 6. Input
| Action | Touch | Voice | Notes |
|---|---|---|---|
| Knock | tap egg | — | Taps during the hatch animation are ignored silently |
| Another egg | tap ✔ | YES | |
| Stay | tap ✖ | NO | |
| Repeat name | tap creature (in PET) | AGAIN | |
| Help | — | HELP | Egg glows + narrator "Tap the egg!" |

## 7. Adaptation and help
Idle 6 s in EGG_IDLE → the egg wobbles + glows. 12 s → a narrator hint. Tap count is randomised
3–5 from the seed. No failure is possible.

## 8. Safety and accessibility
The hatch burst is a scale/particle effect only, with no white flash. Choice bubbles use icons, not
text. The friends shelf is never persisted.

## 9. Budgets
Flash ≤ 1.2 MiB (8 creatures), PSRAM ≤ 900 KiB, particles ≤ 64. Worst frame: HATCH (≈ 45 % dirty).

## 10. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| HP-1 | Egg hatches after the seeded 3–5 knocks; every knock gives visible + audible progress | replay |
| HP-2 | 8 hatches show 8 different creatures (shuffle bag) | replay |
| HP-3 | YES/NO via touch and voice give identical outcomes | replay |
| HP-4 | Taps during HATCH are ignored without sound stacking | replay |
| HP-5 | Friends shelf is cleared on exit/sleep; nothing written to storage | replay + storage audit |
| HP-6 | Device ≥ 30 FPS p95 during HATCH | device bench |

## 11. Later
Creature mixing (pattern × body), a parent-enabled "name your pet" via drawing.

## 12. Open questions
Should the ✔/✖ choice appear automatically, or only after the child taps a "next" egg icon?
