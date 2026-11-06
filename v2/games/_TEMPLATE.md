# <Game name>: v2 game design document

> Copy this template to `games/<game_id>/DESIGN.md` for a new game. Get the design approved before
> implementation. Keep it current. Acceptance criteria IDs are referenced by replays and tickets.
> Use [`../reference/quality_bar.html`](../reference/quality_bar.html) as a feel reference only;
> create original art and game-specific interactions. A description such as "polished" is not
> sufficient: specify measurable responses and prove them with acceptance criteria.

| Field | Value |
|---|---|
| Game id | `snake_case_id` |
| Template | tap-choice / free-play / music-toy / sequencer / character-pet / custom |
| World location | Place on Toy Box Island |
| Learning / play value | e.g. colours, counting, cause-and-effect, rhythm, emotions |
| Session length | Typical 2–5 min; no end state required |
| Capabilities | music, narration, voice_intents, microphone_sampler, pointer_drag, particles |

## 1. Pitch
One sentence a parent understands. One sentence describing what the child *feels*.

## 2. v1 baseline (if any)
Facts from v1 (states, counts, timings, sounds), so behaviour that worked is preserved.

## 3. Experience
### 3.1 Core loop
Numbered steps. What starts it, what the child does, how the game responds, and how it repeats.
### 3.2 States
`INTRO → … → CELEBRATE → …` with entry/exit conditions and timings (link to `tuning.json` keys).
### 3.3 Layout (320×240 logical units)
ASCII sketch + a table of nodes with rect, role, and min size. Respect the Home (0,0,56,56) and Mic
(264,0,56,56) chrome.

For each primary target, record its visible bounds, hit bounds, spacing, state-dependent visibility,
and touch/voice alternative. Show how answer sets remain distinguishable without colour or sound.

### 3.4 Visual quality and renderer
Describe the setting, focal point, palette and material cues. Each scene needs a layered, colored
backdrop; each primary object needs a colored outline, highlight and shadow; controls use the shared
cream/ink chrome. Name the idle, touch, success and hint responses that a screenshot reviewer should
be able to see. Use the ESP-IDF/LVGL shared visual kit rather than inventing per-game token or
particle systems; add a new world or emitter only with a documented reason and measured budget.

## 4. Characters and animation
| Element | Idle | Press | Release | Success | Hint | Other |
|---|---|---|---|---|---|
Use motion tokens from `design/tokens.json`. Note what is baked vs runtime-transformed on the device.
Describe anticipation/follow-through and the reduced-motion version. Ambient motion must stay calmer
than interaction feedback, use bounded dirty regions, and respect the particle/entity caps.

## 5. Audio
- Identity: key, scale, tempo, instruments, ambience, music layers and how engagement moves them.
- Cue table: cue id, trigger, sound description, scale-degree mapping, envelope/maximum length,
  voice cap, and visual pair. Every frequent cue is pleasant under repeated taps.
- Narration table: id, line, variants, when it plays.
State the audio-off behavior; narration, music and microphone are never required to complete play.

## 6. Input
| Action | Touch | Voice intent (context) | Notes |
|---|---|---|---|
Every voice row needs a touch equivalent. State what happens on rapid taps, taps during
animations, and taps on empty space.

## 7. Adaptation and help
Difficulty ramp (from tuning), exact idle → visual hint → optional spoken hint → auto-help timing,
and what happens after repeated misses. No fail state or negative feedback. Taps remain accepted
during nonessential animation; any input-blocking window is named and stays within 300 ms.

## 8. Safety and accessibility
Flash risk, colour-only cues, target sizes, the reduced-motion version, loudness concerns, and privacy
(if mic). Include immediate touch acknowledgement on targets and empty space, an audio-off
playthrough, and checks against `docs/06_visual_motion_design.md` and `docs/11_qa_acceptance.md`.

## 9. Budgets (device)
Flash, PSRAM, internal RAM, max particles, max voices, and the worst-frame dirty fraction with which
scene/state produces it.

## 10. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| XX-1 | | replay / unit / device bench / review |

At minimum, criteria cover: first visual response to touch, empty-space touch, idle/press/release/
success/hint states, a complete help ladder, touch-only and audio-off completion, reduced motion,
rapid taps/capped effects, and the game's worst-case visual/audio/memory budgets. Include reviewed
320×240 captures of the menu, idle, interaction, hint and celebration states plus Home/re-entry.
Link each criterion to a replay or an explicit review/bench method; do not claim device evidence
from a preview.

## 11. Later / stretch
Ideas explicitly out of scope for this version.

## 12. Open questions
