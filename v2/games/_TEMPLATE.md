# <Game name>: v2 game design document

> Copy this template to `games/<game_id>/DESIGN.md` for a new game. Get the design approved before
> implementation. Keep it current. Acceptance criteria IDs are referenced by replays and tickets.

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

## 4. Characters and animation
| Element | Idle | Press | Success | Hint | Other |
|---|---|---|---|---|---|
Use motion tokens from `design/tokens.json`. Note what is baked vs runtime-transformed on the device.

## 5. Audio
- Identity: key, scale, tempo, instruments, ambience, music layers and how engagement moves them.
- Cue table: cue id, trigger, sound description, scale-degree mapping, max length, visual pair.
- Narration table: id, line, variants, when it plays.

## 6. Input
| Action | Touch | Voice intent (context) | Notes |
|---|---|---|---|
Every voice row needs a touch equivalent. State what happens on rapid taps, taps during
animations, and taps on empty space.

## 7. Adaptation and help
Difficulty ramp (from tuning), the hint ladder (idle → visual → spoken → auto-help), and what
happens after repeated misses. No fail state.

## 8. Safety and accessibility
Flash risk, colour-only cues, target sizes, the reduced-motion version, loudness concerns, and privacy
(if mic).

## 9. Budgets (device)
Flash, PSRAM, internal RAM, max particles, max voices, and the worst-frame dirty fraction with which
scene/state produces it.

## 10. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| XX-1 | | replay / unit / device bench / review |

## 11. Later / stretch
Ideas explicitly out of scope for this version.

## 12. Open questions
