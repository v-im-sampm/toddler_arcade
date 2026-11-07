# Beat Sharks: v2 design

| Field | Value |
|---|---|
| Game id | `beat_sharks` |
| Template | sequencer |
| World location | Shark Lagoon |
| Play value | Rhythm, patterns, music making, voice play |
| Capabilities | music, narration, voice_intents, microphone_sampler, particles |

**Reference sketch (v2 mockup, awaiting owner review):** [`beat_sharks.html`](../reference/beat_sharks.html) is the
runnable browser reference for this design: scene, motion and Web Audio score. Once the owner approves
it, the device port follows it line by line ([06 §4.5](../docs/06_visual_motion_design.md)).
Notable differences from the design below:
- Timing: a browser has no audio-thread sequencer, so a deterministic model step clock is the master and each step is scheduled into Web Audio 0.12 s ahead (re-anchored on > 30 ms drift, late steps dropped) instead of ≥ 2 steps ahead on the audio thread; on the device the audio task owns the clock and the visuals follow it.
- Rec is simulated (no microphone in sketches): three silent countdown dots, a listening animation (≤ 1.5 s, or a second Rec tap ends it), then the take "captures" the next of three original synthesized voices (bah, boop, dee; seeded order) for the voice row, with the NEW_SOUND reward.
- During a take the speaker stays silent (countdown and listening): grid taps still toggle sharks, without a preview.
- If the voice row is empty when a take lands, two medium singing sharks are added on steps 1 and 4 so the new voice is heard at once.
- Clear is a 1 s hold on Play: a bubble fills around the button, then pops with a gentle whoosh; the press itself still toggles play/pause immediately.
- Resuming play finishes the interrupted step instead of restarting from step 1.
- Finn surfaces only for the intro (bobbing to the beat) and during takes, so the 18-cell grid stays readable.
- Tempo icon: turtle / walking crab / rabbit plus 1-3 level bars, so tempo is never shown by the animal alone.
- Lane icons are small cream badges overlaid at the left edge of each lane (note, clapping hands, drum).
- No narration or voice intents (no speech in sketches, Mic disabled): the 12 s hint is the HELP ghost pattern (ghost fins plus helper hand) with a neutral two-note chime.

## 1. Pitch
Three rows of friendly sharks jump out of the lagoon in time with a beat the child builds by
tapping. *Feels like:* conducting a splashy shark band.

## 2. v1 baseline
- Grid of 3 rows × 6 steps. Each cell cycles empty → big → medium → small → empty.
- Rows: recorded voice at rates 0.75/1/1.5×; taps (clap, woodblock, shaker); drums (boom, tom,
  bongo).
- Tempo steps 500/375/280 ms (eighth notes at 60/80/107 BPM). Starter beat: booms on steps 1 and 4.
  Fin playhead.
- Controls: Play {60,0,56,56}, Record {120,0,56,56}, Tempo {180,0,80,56}. Cells at
  {6+52c, 62+60r, 48, 52}.
- Rewards: FULL_OCEAN and NEW_SOUND. Voice: AGAIN, HELP.
- Recording privacy identical to Sound Jar.

## 3. Experience
### 3.1 Core loop
1. The lagoon has 3 lanes. Each cell is a water spot. Tap a spot: a shark fin appears (big), then
   medium, then small, then empty. Size = sound variant (pitch/timbre). The sound plays immediately
   on tap (preview), even when stopped.
2. Play (on by default with the starter beat): a cute fish playhead swims across the steps. On each
   step, the active sharks leap out of the water (jump height by size), splash particles fly, and the
   sound plays, sample-accurately on the audio clock. Visual leaps are scheduled from the audio
   clock, not the frame clock.
3. Tempo button: turtle / walker / rabbit icons cycle 60/80/107 BPM.
4. Record (row 1 voice): the same flow as Sound Jar (≤ 3 s, RAM-only). The voice row's sharks then
   "sing" the child's voice at 0.75/1/1.5× rates. Without a recording, row 1 uses a built-in "doo"
   sound.
5. Rewards: FULL_OCEAN (every cell used at least once in a session) brings a whale that splashes a
   rainbow. NEW_SOUND (first recording) brings a confetti splash.
6. A "clear" bubble (with a 1 s hold, to avoid accidents) resets to the starter beat.

### 3.2 States
`STOPPED ⇄ PLAYING` × `{IDLE, RECORDING}` (playback pauses while recording to avoid feedback).

### 3.3 Layout
```
y 0-56    [HOME] [Play 60,0,56,56] [Rec 120,0,56,56] [Tempo 180,0,80,56] [MIC]
y 62-234  3×6 grid, cells {6+52c, 62+60r, 48, 52}; lane icons left-overlaid
```
The cells are below the 56-unit minimum width at 48. v2 must either keep 48×52 with a 52×60 hit area
(no gaps), or move to 5 steps. Record this as a design-review decision (open question).

## 4. Animation
Shark leaps follow an arc with squash on exit/entry, sized by variant. The splash particle count
scales with size. The fish playhead bobs. The lagoon has gentle waves (a static layer plus a small
animated strip). Worst frame: all 18 sharks jumping on one step → budget test.

## 5. Audio
- C major pentatonic. Drums row: boom/tom/bongo. Taps row: clap/woodblock/shaker. Voice row: the
  recorded clip.
- Scheduling: a look-ahead scheduler (≥ 2 steps ahead) on the audio thread. Jitter ≤ 1 ms on all
  platforms. Swing is not used.
- Voice limit: 18 potential hits on one step; mix bus + per-row voice stealing (keep the newest).

## 6. Input
| Action | Touch | Voice | Notes |
|---|---|---|---|
| Toggle cell | tap | — | |
| Play/stop | Play | — | |
| Tempo | Tempo | — | |
| Record | Rec | — | Mic command button disabled while recording |
| Repeat / help | — | AGAIN / HELP | AGAIN replays the narrator's last hint; HELP shows a ghost pattern to copy |

## 7. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| BS-1 | Starter beat plays booms on steps 1 & 4 at 80 BPM on entry | replay + audio capture |
| BS-2 | Step timing jitter ≤ 1 ms (audio render), visual leap within ±1 frame of the sound | golden audio + device bench |
| BS-3 | Cell cycling exactly matches v1 (empty→big→medium→small→empty) | unit |
| BS-4 | All 18 cells active at 107 BPM: no dropouts, ≥ 25 FPS on device | device bench |
| BS-5 | Recording privacy identical to SJ-4; built-in "doo" when no mic | unit + storage audit |
| BS-6 | FULL_OCEAN and NEW_SOUND rewards fire once per session | replay |
