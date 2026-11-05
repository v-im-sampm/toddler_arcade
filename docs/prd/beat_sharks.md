# PRD: Beat Sharks (toddler looper / step sequencer)

**Status:** Implemented in v1 (host-tested and simulator-benchmarked; device gameplay not yet
measured). Mockup: [`../mockups/beat_sharks.html`](../mockups/beat_sharks.html). Short game spec:
[`games/beat_sharks.md`](../../games/beat_sharks.md).
Owner decisions: built-in sounds plus **one recordable row** (RAM-only, same rules as Sound
Jar), 3 rows × 6 steps, and **three sounds per row**: tapping a cell cycles empty → sound 1 →
sound 2 → sound 3 → empty.

**Pitch:** Tap the water to drop in sharks. A little fin swims across and every shark it passes
sings: BOOM, tap, and *your own sound*, over and over.

## 1. Why
A loop that keeps going on its own is the most toddler-friendly way to "make music": the child
puts something in, hears it come back again and again, and changes it while it plays. It teaches
patterns, turn-taking with a beat, and that *where* you put something changes *when* you hear it.
Placing a shark is a simple tap, there are no wrong patterns, and the loop never stops on a mistake.
It reuses the synth, the sound bank and the Sound Jar sampler engine with no new audio buffers.

## 2. Users and goals
| Who | Goal |
|---|---|
| Child (~3 y) | Tap squares, hear the loop change, put their own silly noise in the song. No reading, no failing. |
| Parent | Child's voice never leaves RAM and is never saved; the existing Mic setting turns recording off. |
| Developers | Reuse `ISampler`, `IFeedback`/`Synth`; bounded memory; host-testable step clock and pattern logic. |

**Non-goals (v1):** saving or sharing songs, more than one recorded sound, multiple patterns/pages,
per-step pitch or velocity, swing, tempo by number, live mic monitoring, network or file I/O of audio.

## 3. Experience
### 3.1 Screen (320×240 landscape)
```
y 0-56     [HOME] [PLAY/STOP] [ REC ] [ TEMPO  ]  (shell Mic corner 264-320)
y 62-114   [voice][voice][voice][voice][voice][voice]   row 0: your sound (purple sharks with a jar-lid hat)
y 122-174  [ tap ][ tap ][ tap ][ tap ][ tap ][ tap ]   row 1: clap / woodblock / shaker (orange sharks)
y 182-234  [drum ][drum ][drum ][drum ][drum ][drum ]   row 2: boom / tom / bongo (blue sharks)
            step 1  2      3      4      5      6       fin playhead swims along y 234-240
```
| Element | Hit rect (x, y, w, h) | Behavior |
|---|---|---|
| Home | 0, 0, 56, 56 | Exit to launcher (wipes recording, §7). |
| Play/Stop | 60, 0, 56, 56 | Green triangle ↔ square. Stop is gentle: the current sounds finish, the fin parks. |
| Record | 120, 0, 56, 56 | Records a new sound into row 0 (§3.4). With the mic off it becomes a sparkle that cycles built-in sounds (§3.5). |
| Tempo | 180, 0, 80, 56 | Cycles turtle → walking shark → rabbit; each tap plays a short preview tick at the new speed. Stops before the shell's Mic corner (264, 0, 56, 56), which games never receive. |
| Step cells ×18 | x = 6 + 52·c, y = 62 + 60·r, 48×52 | Each tap moves the cell one step along empty → big shark → medium shark → small shark → empty. A new shark plays its sound immediately; going back to empty is silent. |

All targets are ≥ 48 px. Horizontal cell spacing is 4 px, below the preferred 8 px
(`docs/architecture.md`). We accept this because six steps would not fit otherwise. A near-miss only
changes a neighbouring cell, which is harmless and easy to undo. This is checked during H02 touch
calibration (open question 3).

Rows are told apart by **colour + vertical position + shape** (blue drum sharks at the bottom,
orange tap sharks in the middle, purple sharks with a jar-lid hat for your sound), so colour is never
the only cue. Within a row, **shark size** shows which of the three sounds a cell plays: big = lowest,
small = highest. An empty cell shows calm water with a faint bubble; a full cell shows the shark
facing right.

### 3.2 The loop
| Property | v1 value |
|---|---|
| Steps per loop | 6 (each step is an eighth note; one loop = 3 beats) |
| Tempo (step interval) | Turtle 500 ms, Walk 375 ms (default), Rabbit 280 ms, so a loop lasts 3.0 / 2.25 / 1.68 s |
| Row 2 drums | Boom (sine drop ≈ 150 → 50 Hz over 180 ms, a felt "thump", never a bass blast), tom (220 → 110 Hz) and bongo (400 → 260 Hz). |
| Row 1 taps | Clap (60 ms noise burst), woodblock (900 Hz triangle, 70 ms) and shaker (soft 90 ms noise). |
| Row 0 your sound | The sampler clip, one-shot, at three rates: ≈ 0.75× (big), 1× (medium), 1.5× (small). A new hit on this row stops the previous one, so the voice row is mono and long clips never pile up. Without a sampler the row falls back to three synth boings. |
| Polyphony | At most one hit per row per step plus fading tails, inside `Synth::kVoices` = 4 and the sampler's 2 voices. Every synth sound is shorter than the fastest step (280 ms). |
| Playhead | A fin swims along the bottom edge and the current column gets a soft water-coloured tint. Sharks that sound do a 2–3 px hop. There are no luminance flashes and no full-screen effects. At Rabbit the tint changes 3.6 times a second but only moves position, so it stays within the "no rapid flashing" rule. |

**Starter pattern:** big drum sharks (boom) on steps 1 and 4. The grid is never empty on entry, so
a child who does nothing still hears a beat.

### 3.3 Flow and states
`INTRO → PLAY ⇄ RECORDING → CELEBRATE → PLAY`, plus `PAUSED` and `EXIT`. Transport (playing or
stopped) is a sub-state of PLAY.
1. **INTRO** (≤ 3 s, non-blocking, skippable by any tap): a short spoken prompt ("Tap the water
   to add sharks!") while the starter pattern begins to play at the default tempo, under the existing
   volume cap with no loud onset.
2. **PLAY**: cells, tempo and play/stop work at any time, including mid-loop. Edits take effect the
   next time the fin reaches that column. If the fin is within 60 ms of a just-enabled cell, the
   preview is skipped so the child doesn't hear a double hit.
3. **RECORDING** (≤ 1.5 s): see §3.4.
4. **CELEBRATE** (≤ 1.5 s, non-blocking, the loop keeps playing) happens in two cases:
   - after a new sound is accepted, the row-0 sharks sparkle;
   - the first time **every column has at least one shark** ("full ocean"), all sharks do one
     wave. Emptying a column re-arms this.

   There is no score, no goal and no end.
5. **PAUSED** (global PAUSE, parent gate, idle sleep): the transport stops and all voices release.
   On RESUME the game comes back stopped with the pattern kept, and the child presses Play. This
   avoids an unexpected sound on resume.

Navigation is accepted at any time, including mid-recording, which discards the recording.
Repeated taps never queue audio: each tap just moves a cell to its next value, and a preview steals the oldest voice.

### 3.4 Recording into the voice row
1. Tap **Record**. If the loop is playing it pauses and the fin holds its position. Every voice stops,
   the speaker is muted (no feedback howl), and the button shows a red bar filling over 1.5 s with a
   loudness halo. Recording only starts on an explicit tap.
2. Tap again, or reach 1.5 s, to stop. Beat Sharks calls `ISampler::stop_recording()` itself when
   `record_progress_permille() ≥ 500` (half of the engine's 3 s maximum), because short sounds work
   better in a loop. Trim and normalize use the engine's existing rules (Sound Jar PRD §3.3).
3. **Accepted:** row 0 now plays the new sound. If row 0 had no sharks, sharks are placed on steps 1
   and 4 so the sound is heard straight away. CELEBRATE plays, and the loop resumes if it was playing
   before.
4. **Too quiet or too short:** the previous sound stays, the Record button does a gentle wiggle and
   the loop resumes. There is no failure sound and no text.

### 3.5 Mic disabled (parent setting or no mic)
Record becomes a **sparkle** button that cycles the sampler's 3 built-in sounds (boing, quack-like
glide, drum thump) for row 0 and previews each. Everything else behaves the same, so the game is
fully touch-complete with the mic off. Row 0 starts on the built-in boing in both modes.

### 3.6 Voice
Global intents only, so there are **no new entries in `config/commands.json`**:
`BACK` = Home, `HELP` = replay prompt, `AGAIN` = restart the loop from step 1 (and play),
`PAUSE` = stop, `RESUME` = play. Voice never starts recording. The launcher tile has no spoken
name (`Intent::UNKNOWN`), the same as Sound Jar. *Optional, later:* add a `beat` context to `GO`/`STOP`
(speech owner review).

## 4. Functional requirements
| ID | Requirement |
|---|---|
| F1 | 3 × 6 cells, each 0 (empty) or 1–3 (sound); a tap cycles 0 → 1 → 2 → 3 → 0; starter pattern: drum sound 1 on steps 1 and 4. |
| F2 | Cell tap → visual change < 50 ms, audible preview < 100 ms (same audible target as Rainbow Piano). |
| F3 | Step clock driven by `update(dt_ms)` with a remainder carry (no drift). Measured onset jitter ≤ 15 ms p95 on device, otherwise apply F12. |
| F4 | Three tempos (500/375/280 ms per step). Tempo changes apply from the next step and never restart the loop. |
| F5 | Sounds are triggered via `IFeedback::play(Cue::NOTE, row * 3 + sound - 1)` with a Beat Sharks palette in `sound_bank` (variants 0–2 voice-row fallback boings, 3–5 taps, 6–8 drums; `Cue::TAP` variant = tempo tick). With a sampler, row 0 instead calls `ISampler::stop_all()` then `play(rate, false)` at one of three rates. |
| F6 | Record into row 0 via `ISampler`: ≤ 1.5 s, speaker muted, loop paused and resumed afterwards. |
| F7 | Built-in sparkle fallback when `mic_available()` is false. |
| F8 | Output goes through the existing limiter and volume cap; no clipping with all 18 cells on at Rabbit. |
| F9 | `ISampler::wipe()` on EXIT, idle sleep and parent-gate entry. The pattern is not persisted (in-RAM only, reset to the starter pattern on next entry). |
| F10 | Deterministic test mode: injectable clock and PCM fixture. The stretch "Surprise" feature uses a seeded `rng`. |
| F11 | Audio-off mode: the fin, column tint and shark hops still show the beat. |
| F12 | Fallback if F3 fails: move the step clock into the audio task as a small `ISequencer` (sample-accurate step events, pattern passed as an 18-bit mask through the existing SPSC-queue pattern). This is an interface change that needs platform review. |

## 5. Technical design (fits `docs/architecture.md`)
- **Folder:** `firmware/components/games/beat_sharks/` with `beat_sharks.{hpp,cpp}` (logic, no
  LVGL), `view/beat_sharks_view.{hpp,cpp}` (LVGL), header-only `view/include/.../beat_sharks_art.hpp`
  (procedural ARGB rasters), `test/` and `README.md`.
- **Pure logic** (`BeatSharksGame : IGame`) has no LVGL, no audio engine and no heap. It holds:
  - `uint8_t cells[3][6]` (0 = empty, 1–3 = sound);
  - the current step, a millisecond accumulator with remainder carry, tempo index, transport and
    scene state;
  - cell taps (with the 60 ms preview-suppression window) and the record-state transitions.

  It calls `IFeedback`/`ISampler` directly and is fully host-testable with fakes.
- **Audio:** no new engine for v1. Drums use the existing `Synth` via a new `sound_for(BEAT_SHARKS,
  NOTE, v)` palette. Row 0 shares the single `audio::Sampler` clip with Sound Jar, using buffers
  that `main.cpp` already allocates at boot. Neither game keeps the clip across exit (both call
  `wipe()`), so a recording never carries over between games.
- **Input:** only `InputEvent` touch coordinates and intents; no recognition text, no drag.
- **Shell:** register the game ID and launcher tile in `arcade` (integrator review); the shell Mic
  corner already excludes 264,0–320,56.

### 5.1 Memory budget (estimate, to be measured)
| Item | Location | Bytes |
|---|---|---|
| Sampler clip / take / echo buffers | already allocated at boot (Sound Jar) | 0 new |
| Pattern, clock and state | internal | < 64 |
| View rasters (ARGB8888): 9 sharks 46², bubble 46², 8 top-bar icons 40², fin 26×16, star 26², launcher tile 108×68 | PSRAM (`ArgbImage`) | 169 584 (computed from sizes) |
| LVGL objects: 18 single-object cells (shared row styles, shark as background image), 3 buttons, column tint, fin, 3 stars | LVGL pool | 6 568 measured on the 64-bit desktop sim (pool use 86 840 → 93 408 B); smaller on the 32-bit device |
| **Total new** | | **≈ 166 KiB PSRAM, < 7 KB LVGL pool** |

Gate: PSRAM free ≥ 1 MiB and internal heap ≥ 64 KiB with the game running (device measurement).

## 6. Child-safety and audio-safety requirements
- No failure states, no scores, no countdowns; the 1.5 s bar is only a capture indicator.
- BOOM is band-limited and level-capped; tested at max volume with all cells on (no clipping,
  no sustained low-frequency rattle in the enclosure, checked on device).
- Entry starts quietly (prompt, then the starter beat under the cap); RESUME never auto-plays.
- No rapid flashing: only position changes on the beat, no luminance strobes.
- Never punishes "messy" patterns; every pattern including an empty grid is valid. An empty grid just
  shows the fin swimming.

## 7. Privacy (extends the Sound Jar exception; owner sign-off needed)
Same rules as Sound Jar PRD §7, applied to Beat Sharks:
1. At most one clip (≤ 1.5 s here), only in RAM, inside the shared `audio::Sampler`.
2. Never written to flash, NVS, SD, logs, serial or any transport. Logs may contain only aggregate
   peak/RMS values.
3. Zeroed on EXIT, idle sleep, parent-gate entry and recording start.
4. Recording requires an explicit child tap and shows a continuous visible indicator.
5. The existing parent **Mic** setting (NVS key `mic`, default ON) also controls Beat Sharks. Its
   on-screen label is already the generic "Mic ON/OFF", so it needs no change.
6. Doc amendments (done): README, `docs/parental-safety.md` and `docs/voice.md` now say *"Sound
   Jar and Beat Sharks may each hold one ≤ 3 s clip in RAM while open."*

## 8. Acceptance criteria
- Fully playable touch-only with the mic off, and with audio off (visual beat).
- Tap-to-preview < 100 ms; step jitter ≤ 15 ms p95; loop length within ±1 % of nominal over 60 s
  (device measurement, not the simulator).
- No clipping at max volume with all 18 cells on at Rabbit tempo (full-scale fixture + device SPL).
- Sample buffer zeroed after EXIT, idle sleep and parent gate (host test + on-device check).
- Handles:
  - repeated rapid taps on one cell, and tempo taps mid-step;
  - Home mid-recording, recording while the loop plays, and too-quiet recordings;
  - mic loss mid-session, and voice intents during CELEBRATE;
  - PAUSE/RESUME while playing (resumes stopped).
- No per-frame allocations; PSRAM, internal heap and LVGL-pool deltas recorded.
- Host, emulator and physical-device results reported separately (AGENTS rule 8).

## 9. Tests
**Host (pure C++):**
- Step clock with exact step counts over a long run: no drift, and the remainder carries across
  uneven `dt`.
- Tempo change mid-step; toggle semantics and the preview-suppression window.
- Starter pattern; the "full ocean" trigger fires once and re-arms.
- Recording state machine: pause/resume of the transport, the 1.5 s auto-stop, ACCEPTED vs
  TOO_QUIET, auto-placing steps 1 and 4 when row 0 is empty, and Home mid-record cancels.
- Mic-off sparkle cycling.
- `wipe()` is called on exit and pause.
- The sound bank palette stays under `kMaxSoundMs` and `kMaxToneLevel`.

**Device:** onset jitter (log step timestamps, or record the speaker), latency, heap/PSRAM
watermarks, SPL at max volume with all cells on, and a feedback check with the speaker near the mic.

## 10. Milestones
1. **M0** Owner review of this PRD, the mockup and the §7 extension.
2. **M1** `BeatSharksLogic` + host tests, and the sound-bank palette + bank tests.
3. **M2** Scene, view and simulator; device jitter measurement (decide on F12).
4. **M3** Record/sparkle wiring through `ISampler`; parent Mic label.
5. **M4** Launcher registration, `games/beat_sharks.md`, validator game count 11 → 12, and the §7
   doc amendments.

## 11. Open questions
1. Is 6 steps (a 3-beat loop) musical enough, or should a later version offer a second page/8 steps
   with smaller cells? (Owner chose 3 × 6 for v1.)
2. Should the voice row play the clip at different pitches per step (melody)? This is deferred as
   a stretch goal: "pitch hats".
3. Is the 4 px horizontal cell gap reliable on the NS2009 resistive panel? Check during H02.
4. Stretch ideas, behind evaluation:
   - a "Surprise" button for a seeded random pattern;
   - drag-to-paint cells;
   - a "clear the ocean" hold gesture (v1 has no clear button: emptying is done by tapping, and
     Home → re-enter restores the starter pattern).

## Rollback
Docs-only at M0: delete `docs/prd/beat_sharks.md` and `docs/mockups/beat_sharks.html`. Later
milestones are self-contained in the game folder plus the sound-bank palette and shell registration;
revert with `git revert <sha>`.
