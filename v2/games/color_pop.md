# Color Pop: v2 design (GOLDEN GAME)

| Field | Value |
|---|---|
| Game id | `color_pop` |
| Template | tap-choice |
| World location | Bubble Beach |
| Play value | Colour names, listening, cause-and-effect |
| Capabilities | music, narration, voice_intents, particles |
| Manifest example | [`../schemas/examples/color_pop.manifest.json`](../schemas/examples/color_pop.manifest.json), tuning [`color_pop.tuning.json`](../schemas/examples/color_pop.tuning.json) |

Color Pop is the **golden game**. It is built first, to the full quality bar on all three
platforms, and it proves the engine, pipeline, SDK, shell integration and tests. Every pattern used
here becomes an SDK preset or template.

## 1. Pitch
Shiny bubbles float up from the sand. A friendly crab, **Pinch**, asks for a colour. Pop the right
one and it bursts into sparkles and a happy note. *Feels like:* popping real bubbles that sing.

## 2. v1 baseline
- `INTRO` 1.2 s → `PLAY` → `CELEBRATE` 1.5 s → new round.
- 2 bubbles, growing to 4. Six fixed 72×72 slots. Colours RED/BLUE/GREEN/YELLOW, with no target
  repeated consecutively.
- Patterns: heart, wave, leaf, star.
- A wrong tap wiggles. The target badge glows and replays the prompt.
- Voice: RED, BLUE, GREEN, YELLOW, AGAIN, HELP.
- Synth motifs per colour (red "bum-BUM", blue falling bells, green rising plucks, yellow twinkle).
  Success is `bubble_pop`; celebration is `celebrate_arpeggio`.

## 3. Experience
### 3.1 Core loop
1. Bubbles (2–4) rise from the sand with a wobble and settle into a gentle float. Each has a colour
   and a pattern glyph inside.
2. Pinch pops up from the sand holding a little flag in the target colour and pattern. The
   narrator says "Pop the **blue** bubble!". A colour motif plays. The prompt badge (top centre)
   shows the colour + pattern.
3. The child taps:
   - **Target:** a pre-pop squash, then a burst (particles in the bubble colour + sparkles), a note
     at the colour's scale degree, Pinch claps, and a short narrator praise. Remaining bubbles drift
     away happily. → next round.
   - **Other bubble:** that bubble *also* pops harmlessly with a softer plink, because toddlers love
     popping. The target begins `glow.hint`, and Pinch points at it. The bubble respawns after 1 s
     so the choice set is preserved. (v2 change: no "wrong" wiggle-only moments. Every tap is fun,
     while the hint steers.)
   - **Empty water/sand:** a touch ripple plus a tiny bubble rising from the finger.
4. Every 5 successes: a **big celebration**. A wave of bubbles rises, Pinch dances, and the music
   sting plays.

### 3.2 States
`INTRO → PROMPT → PLAY ⇄ HINT → AUTO_HELP → CELEBRATE → (BIG_CELEBRATE) → PROMPT`
- INTRO ≤ 1.5 s, skippable with a tap.
- PROMPT is not blocking: taps are accepted while the narrator speaks.
- HINT: after 6 s idle or one non-target pop.
- AUTO_HELP: after 2 non-target pops. The target grows to 1.15× and pulses, and a helper hand bobs.
- CELEBRATE input block ≤ 300 ms, after which taps make extra sparkles.

### 3.3 Layout
```
y 0-56    [HOME]        [ prompt badge 132,4,56,56 ]         [MIC]
y 64-200  bubble slots (6 positions, 72×72 visual, 80×80 hit) — 2..4 used
y 168-240 sand strip; Pinch the crab peeks at x 8-72 (decorative, tap = giggle)
```
Slot positions are jittered by ±6 units per round (seeded) so the layout feels organic.

## 4. Characters and animation
| Element | Idle | Press | Success | Hint | Other |
|---|---|---|---|---|---|
| Bubble | float bob (tokens.idle), iridescent highlight slides slowly | `squash.press` | pre-pop stretch 80 ms → burst (particles `color_pop.pop` + `sparkle_burst`), ring wave | `glow.hint` + slight scale pulse | spawn: rise from the sand with `ease.out` + wobble spring |
| Pinch (crab) | breathe, eye blink, claw tap every 4–7 s | giggle wiggle | clap claws + jump with `squash.land` | points a claw toward the target | look-at follows the last touch |
| Prompt badge | soft bob | press | flashes **once** (≤ 1 per round) to a happy face | pulses with the hint | — |
| Background | sky gradient (dithered on device), 2 small clouds drifting, 3–4 tiny ambient bubbles | — | — | — | — |
Device: bubbles are baked sprites (base + highlight overlay). Squash and float are runtime scale and
translate. Pinch is a baked sprite sheet at 12–15 fps. The background is a static cached layer.

The former v1 implementation used a separate pixel engine and its code has
been removed. This v2 design and [`quality_bar.html`](../reference/quality_bar.html)
define the target experience; the v2 runtime, renderer, and audio stack remain
open pending the approved platform decisions. Where this section and the
reference disagree on game behavior, the reference wins.

## 5. Audio
- Identity: C major pentatonic, 104 BPM. Marimba + bubble plinks + shaker. Bed = waves. Layers rise
  with streaks of play and fall when idle.
- Cues:
  | Cue | Trigger | Sound | Pitch | Visual pair |
  |---|---|---|---|---|
  | `prompt_motif.<color>` | prompt | v1 motifs re-orchestrated on marimba | colour motif | badge bob |
  | `pop.target` | target pop | wet "plop" + marimba note | degree per colour (tuning) | burst |
  | `pop.other` | other pop | softer plink | random degree (seeded) | small burst |
  | `ripple` | empty tap | tiny water blip | degree 0 high | ripple |
  | `celebrate` / `celebrate_big` | success / every 5 | sting quantised to the next 1/8 | — | particles |
- Narration: see the manifest (`vo_intro`, `vo_find_<color>` + variants, `vo_hint`, `vo_praise` ×6).

## 6. Input
| Action | Touch | Voice (context) | Notes |
|---|---|---|---|
| Choose colour | tap bubble | RED/BLUE/GREEN/YELLOW (colors) | A voice answer pops the matching bubble with the same animation as touch; if that colour isn't on screen, Pinch shrugs kindly and the target glows |
| Repeat prompt | tap badge | AGAIN (global) | |
| Help | tap badge twice within 1 s | HELP (global) | Jumps to the hint |
| Rapid taps | each tap pops at most one bubble; 80 ms debounce per bubble | | |

## 7. Adaptation and help
- 2 bubbles to start. +1 after 3 successes (max 4). Ease back to the previous count after 2 auto-helps
  in a row.
- The target colour is never repeated consecutively and comes from a shuffle bag. Pattern glyphs are
  always shown.

## 8. Safety and accessibility
- The big celebration has many particles but no luminance flash (the analyser must pass).
- Colours + patterns pass the CVD check.
- Reduced motion: fewer particles, no wave.

## 9. Budgets (device)
Flash ≤ 900 KiB, PSRAM ≤ 600 KiB, particles ≤ 64, voices ≤ 8. Worst frame: BIG_CELEBRATE (≈ 35 %
dirty).

## 10. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| CP-1 | Touch-only play through 10 rounds; the count ramps 2→4 | replay (device bench) |
| CP-2 | Target never repeats consecutively; positions randomised by seed; identical replay output on all platforms | replay ×3 platforms |
| CP-3 | Non-target pop → hint; 2 → auto-help; no negative sound | replay |
| CP-4 | Voice answers behave exactly like taps; out-of-set colour handled kindly | replay (intent injection) + voice eval |
| CP-5 | Device: animated region ≥ 30 FPS p95; touch→sound ≤ 50 ms p95 | device bench |
| CP-6 | Fully playable with audio off (all cues have a visual) | replay with audio off + review |
| CP-7 | Mashing (10 taps/s for 5 s) never blocks, crashes or stacks sounds beyond the voice limit | replay |
| CP-8 | Home mid-celebration exits within the transition time; re-entry starts cleanly | replay |
| CP-9 | Flash/target/colour-cue analysers pass | CI |

## 11. Later
More colours (orange, purple) unlocked by parent setting; "two of the same colour" counting variant.

## 12. Open questions
Crab vs. seal vs. octopus for the host character (art review).
