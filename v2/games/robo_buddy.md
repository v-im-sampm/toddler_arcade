# Robo Buddy: v2 design

| Field | Value |
|---|---|
| Game id | `robo_buddy` |
| Template | character-pet |
| World location | Bolt's Workshop |
| Play value | Emotions, caring routines, cause-and-effect |
| Capabilities | music, narration, voice_intents, particles |

**Reference sketch (v2 mockup, awaiting owner review):** [`robo_buddy.html`](../reference/robo_buddy.html) is the
runnable browser reference for this design: scene, motion and Web Audio score. Once the owner approves
it, the device port follows it line by line ([06 §4.5](../docs/06_visual_motion_design.md)).
Notable differences from the design below:
- Narration and voice intents are not sketched (Mic shown disabled). Each mood "line" is an in-key FM/kalimba motif, and the mood badge in the top band stands in for AGAIN (tap: replays the last mood's motif and pose, not counted) and HELP (double tap: food, hello and moon glow one by one with rising pings). YES/NO are the chest buttons only.
- Food hit box is {8,92,72,68}, which keeps an 8 px gap from the moon. Taps on Bolt outside the head/belly boxes count as the nearer part (y < 134 → head).
- The dance is 8 key poses stepped on eighth notes and tweened, not a 12 fps flipbook. It is a non-blocking 4-bar overlay (16 beats, 9.6 s) that starts on the next beat at least 0.6 s away and ends with a tada. Reactions during it play but don't count, and the moon ends it. The disco is three soft light pools behind Bolt whose hue drifts 36°/s (one cycle per 10 s), with no flashing. Bolt's LED face and antenna bulb follow the same hue.
- SLEEPY (moon, or the 20 s doze) does not count toward the dance. Taps inside the 0.8 s wake guard only make him stir. While he sleeps the workshop dims, stars twinkle and he snores softly; after 8 s a soft glow over his head hints "tap to wake".
- "Want to dance?" appears once 3 reactions are counted and Bolt is free: his two chest lamps pop out as 48 px ✔/✖ buttons (green/red, 8 px apart). Taps within the 300 ms input block are swallowed. YES starts the dance. NO nods and keeps counting, and the question isn't asked again until after the next dance.
- Hint ladder: at 8 s idle Bolt waves; 1.6 s later he glances at a prop (or the ✔ button), which glows with `glow.hint`; at 20 s he dozes.
- Additions: an intro roll-in with a wave (1.5 s, skippable); an original 8-bar workshop tune and a quiet hum under the motifs; the hello hand on a springy wall coil; and a chest battery meter. YUMMY fills the meter, each dance dips it one segment (never below 1) and sleep refills it; it has no consequence.
- LED faces are 8 base faces plus chew/yawn/sing variants. The eyes look left, right and up only, never down onto the mouth. Particles are capped at 48 (§9).

## 1. Pitch
Bolt, a round little robot, reacts to tickles, snacks, naps and hellos with big expressive feelings,
and sometimes breaks into a dance. *Feels like:* a pet robot friend who adores you.

## 2. v1 baseline
- Moods: CALM, HAPPY, SILLY, YUMMY, SLEEPY. Every 5th reaction is a dance.
- Mood relaxes to CALM after 4 s. Wake guard 0.8 s.
- Targets: head, belly, food, sleep, hello. Voice: YES, NO, AGAIN, HELP.

## 3. Experience
### 3.1 Core loop
1. Bolt idles: breathing antenna bob, blinking LED eyes, looks at the last touch, and makes small
   servo-hum fidgets.
2. Touch targets:
   - Head → HAPPY: eyes become arcs, antenna spins, and it beeps a happy melody.
   - Belly → SILLY: tickle giggle, wobble, and gear sparkles.
   - Food (oil-can / battery-cookie prop) → YUMMY: munch, and the battery meter fills.
   - Sleep (moon prop) → SLEEPY: eyes droop, Zzz particles, and a lullaby motif. Stays asleep until
     touched (wake guard 0.8 s), then does a stretch-yawn.
   - Hello (wave hand prop) → Bolt waves back, says a "beep-boop hello" chirp, and the narrator
     says "Hi friend!".
3. Every 5th reaction: Bolt dances to a 4-bar groove, with disco lights done as gentle colour shifts
   and no flashing.
4. The mood decays to CALM after 4 s (except SLEEPY).

### 3.2 States
Mood FSM: `CALM → {HAPPY|SILLY|YUMMY|SLEEPY|HELLO} → CALM` + `DANCE` overlay every 5th reaction.

### 3.3 Layout
```
y 0-56    [HOME]                                     [MIC]
y 56-232  Bolt centred {96,56,128,176}: head hit {104,60,112,72}, belly hit {108,136,104,72}
          props: food {8,96,72,72}, sleep {8,168,72,64}, hello {240,96,72,72}
```

## 4. Characters and animation
| Element | Idle | Press | Success | Hint | Other |
|---|---|---|---|---|---|
| Bolt | breathe, blink, look-at, fidget | squash per target | mood pose + face swap | glance at a prop | dance cycle 8 poses at 12 fps |
| Props | gentle bob | press | — | `glow.hint` | — |
| Face | LED-matrix-style expressions (8 faces) | | | | runtime-swappable overlay |

## 5. Audio
- G major pentatonic, 100 BPM. FM blips, kalimba, soft workshop hum.
- Cues per mood: a 3–5 note melody per mood (HAPPY rising, SILLY bouncing, YUMMY "nom" chords,
  SLEEPY descending lullaby), dance groove, wake stretch.
- Narration: "Bolt is happy!", "Bolt is sleepy… shh", "Let's dance!". Emotion words are the
  learning goal.

## 6. Input
| Action | Touch | Voice | Notes |
|---|---|---|---|
| Interact | tap target | — | |
| "Want to dance?" prompt (after 3 reactions) | tap Bolt's ✔ / ✖ chest buttons | YES / NO | Optional prompt |
| Repeat | — | AGAIN | Repeats the last mood line |
| Help | — | HELP | Props glow in sequence |

## 7. Adaptation and help
Idle 8 s → Bolt waves at the child. Idle 20 s → Bolt dozes (SLEEPY), and any tap wakes it.

## 8. Safety and accessibility
Disco lights are a hue shift at ≤ 1 Hz with luminance change ≤ 10 %. No scary faces (SAD is not
used here).

## 9. Budgets
Flash ≤ 1 MiB, particles ≤ 48. Worst frame: DANCE (≈ 50 % dirty; use region-limited lights).

## 10. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| RB-1 | Each of the 5 targets yields its mood within 100 ms of touch | replay + device bench |
| RB-2 | Every 5th reaction triggers a dance | replay |
| RB-3 | Mood relaxes to CALM after 4 s; SLEEPY persists until a touch; wake guard 0.8 s respected | replay |
| RB-4 | YES/NO voice equals touch | replay |
| RB-5 | Dance passes the flash analyser | CI |
