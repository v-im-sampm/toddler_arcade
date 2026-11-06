# Robo Buddy: v2 design

| Field | Value |
|---|---|
| Game id | `robo_buddy` |
| Template | character-pet |
| World location | Bolt's Workshop |
| Play value | Emotions, caring routines, cause-and-effect |
| Capabilities | music, narration, voice_intents, particles |

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
