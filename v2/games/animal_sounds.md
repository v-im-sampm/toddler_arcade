# Animal Sounds: v2 design

| Field | Value |
|---|---|
| Game id | `animal_sounds` |
| Template | tap-choice |
| World location | Farmyard |
| Play value | Listening, animal names and sounds |
| Capabilities | music, narration, voice_intents, particles |

**Reference sketch (v2 mockup, awaiting owner review):** [`animal_sounds.html`](../reference/animal_sounds.html) is the
runnable browser reference for this design: scene, motion and Web Audio score. Once the owner approves
it, the device port follows it line by line ([06 §4.5](../docs/06_visual_motion_design.md)).
Notable differences from the design below:
- Animal calls are original synthesized voices (sine partials, glides, LFO, filtered noise), not CC0 recordings, so there is no provenance record.
- No narrator: "Who says that?", "It's the <animal>!", the parade line and "Listen…" are stood in for by the call replay, the badge portrait of the found animal and the badge bounce.
- AGAIN/HELP voice intents are absent (mic disabled in the sketch); tapping the horn badge is the replay.
- Audio-off mode is simulated with the M key (master mute); the badge then shows the hidden animal's silhouette.
- Each animal has a fixed hiding place (barn half door: cow, pig, dog; pond: duck, frog; bush: sheep, cat, owl) rather than barn doors only.
- Spot count ramps 2 → 3 → 4 on unaided finds and steps back one after two auto-helped rounds in a row; 2-spot rounds never pair similar-sounding animals.
- A pick hushes any call in progress, so only one animal calls at a time; auto-help also replays the mystery call with the hidden animal's mouth synced.
- Taps during the celebration and parade are acknowledged (ring, sparkle, note) but do not skip them.
- The parade shows the last four animals found, hopping across in about 2 s.

## 1. Pitch
Hear an animal call, then find who made it. The animal pops out of its barn door and does a happy
dance. *Feels like:* peekaboo with farm friends.

## 2. v1 baseline
- Intro 1.5 s, celebrate 1.8 s.
- 8 animals (COW, DUCK, SHEEP, PIG, DOG, CAT, FROG, OWL), each a CC0 PCM call.
- Cards grow from 2 to 4 across 6 slots of 96×80. Replay button at {132,4,56,56}.
- Voice: AGAIN, HELP. A wrong tap wiggles and hints.

## 3. Experience
### 3.1 Core loop
1. 2–4 barn doors / bushes / ponds (habitat per animal) each hide an animal. Animals peek partially
   (ears/tail visible) so the choice is visual, not memory.
2. A mystery call plays and the speaker-horn button wiggles in time with it. The narrator says "Who
   says that?" (no name, so the child must listen).
3. Tap:
   - **Right animal:** it pops out fully, repeats the call with a big mouth animation, does a dance,
     and the narrator names it ("Moo! It's the cow!").
   - **Other animal:** it pops out too, does its own call and a friendly wave, then hides again.
     The right one starts peeking more (hint). That's still fun and teaches another sound.
4. After 2 others: the right animal peeks fully and wiggles (auto-help).
5. Every 4 successes: a "farm parade". The found animals march across with their calls in rhythm.

### 3.2 States
`INTRO → CALL → PLAY ⇄ HINT → AUTO_HELP → CELEBRATE → (PARADE) → CALL`

### 3.3 Layout
```
y 0-56    [HOME]     [replay horn 132,4,56,56]      [MIC]
y 64-232  hiding spots (6 slots, 96×80 each), 2..4 used
```

## 4. Characters and animation
Each animal has: peek, pop-out, call (mouth open/close synced to the call's amplitude envelope),
dance, wave, hide. Stylised and original (no recognisable licensed farm characters).

## 5. Audio
- A major pentatonic, 90 BPM. Ukulele + whistle music. The farmyard bed contains **no animal calls**.
- Calls: v1 CC0 recordings re-mastered (denoised, loudness-normalised per doc 07), or newly
  sourced recordings, both with provenance records. Music ducks −10 dB under calls.
- Narration: "Who says that?", "It's the <animal>!" for 8 animals, parade line, hint "Listen…".

## 6. Input
| Action | Touch | Voice | Notes |
|---|---|---|---|
| Choose | tap spot | — | Animal names as voice intents are a stretch goal (needs new intents + eval) |
| Replay call | tap horn | AGAIN | |
| Help | — | HELP | |

## 7. Adaptation and help
2 → 4 spots over rounds. A shuffle bag of animals. Easier mixes first (cow vs duck), and similar
sounds (cat vs owl) only later.

## 8. Accessibility
Fully playable with audio off? **No.** Listening is the point. With audio off, the game switches to
"match the picture": the badge shows the animal silhouette. It must stay solvable (doc 11 rule).

## 9. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| AS-1 | 10 rounds touch-only; spots 2→4; no animal repeated consecutively | replay |
| AS-2 | Every call meets loudness and duration (≤ 2.5 s) targets with provenance on file | analyser + provenance CI |
| AS-3 | Wrong pick plays that animal's call + hint, never a negative cue | replay |
| AS-4 | Audio-off mode is solvable via silhouettes | replay audio off |
| AS-5 | Mouth animation in sync with call within 50 ms | review on device |
