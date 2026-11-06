# Rainbow Piano: v2 design

| Field | Value |
|---|---|
| Game id | `rainbow_piano` |
| Template | music-toy |
| World location | Rainbow Bridge |
| Play value | Free musical exploration, shapes, colours, optional follow-the-light |
| Capabilities | music, narration, pointer_drag (glissando), particles |

## 1. Pitch
Five glowing shape-keys on a rainbow bridge sing when touched, and little notes fly into the sky.
*Feels like:* a real instrument that always sounds good.

## 2. v1 baseline
- 5 keys: circle, square, triangle, heart, star. Keys at {4+64i,120,56,112}.
- 3 instruments: bells, flute, drum.
- Optional 5-note follow-light tune. Key glow 300 ms.

## 3. Experience
### 3.1 Core loop
1. Free play. Each key plays its note (C D E G A, pentatonic, so any combination sounds good). The
   key squashes, lights up, and a note-creature (shape + colour) floats up and pops into sparkles.
2. **Glissando (new):** sliding a finger across keys plays each one as it is entered (requires drag
   support; ≤ 1 note per key entry, 60 ms minimum per key).
3. The instrument picker (3–5 round icons at the top) switches timbre: bells, flute, drum, plus new
   marimba and toy piano. A switch plays a demo arpeggio.
4. **Follow the light (optional, a lightbulb icon):** a key glows; tapping it advances. 5 notes make a
   tune, then a rainbow arc sweeps across with a short tune playback. Wrong keys still play their
   notes; the glowing key just keeps glowing.
5. A gentle backing groove (toggle drum icon) at 96 BPM lets taps quantise *visually* (never
   delay audio).

### 3.2 States
`FREE ⇄ FOLLOW(step 0..4) → RAINBOW(2 s) → FREE`

### 3.3 Layout
```
y 0-56     [HOME] [instrument icons 64..256, 48×48 each] [MIC]
y 56-112   sky: note creatures float here; follow-light toggle {136,60,48,48}
y 120-232  5 keys, 56×112 each (hit 64×120, no gaps)
```

## 4. Characters and animation
| Element | Idle | Press | Success | Hint | Other |
|---|---|---|---|---|---|
| Key | slow colour shimmer | `squash.press` + glow 300 ms | — | `glow.hint` (follow mode) | the shape icon bounces with the note |
| Note creature | — | spawn on press, rise 1.2 s with wobble, pop | — | — | ≤ 10 alive; the oldest pops early |
| Rainbow | — | — | sweep arc over 1.2 s with sparkles | — | after a completed tune |

## 5. Audio
- C major pentatonic, keys = C D E G A (octave set by instrument). Low latency is the priority: the
  touch→sound p95 must be ≤ 30 ms on device here (stricter than the global 50 ms).
- Polyphony: 5 voices with release tails. Repeated keys retrigger with a short fade (no clicks).
- No narration in free play beyond the intro ("Play the rainbow!"). Follow mode: "Follow the light!".

## 6. Input
| Action | Touch | Voice | Notes |
|---|---|---|---|
| Play note | tap / slide | — | Multi-press not supported by NS2009 (single touch) |
| Change instrument | tap icon | — | |
| Follow mode on/off | tap bulb | — | |
| Repeat tune | — | AGAIN | Plays the current follow tune |

## 7. Adaptation and help
Follow mode: after 5 s idle the target key bounces. There are no fail states, and the tune always
completes.

## 8. Safety and accessibility
Keys differ by shape and colour. The rainbow sweep has no flashing. The volume follows the global cap,
and the drum instrument is limited by the bus compressor.

## 9. Budgets
Flash ≤ 1 MiB (instrument samples or synth patches), particles ≤ 48, voices ≤ 6.

## 10. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| RP-1 | Every key gives sound + glow; touch→sound ≤ 30 ms p95 on device | device bench |
| RP-2 | Glissando across 5 keys plays 5 notes in order, with no doubles | replay |
| RP-3 | Follow tune completes with any number of wrong presses | replay |
| RP-4 | 20 taps/s for 5 s: no clicks, no voice overflow, no frame drop < 25 FPS | device bench |
| RP-5 | All instruments within loudness targets (doc 07) | audio analyser |

## 11. Later
Record-and-playback of the child's tune (RAM only), more tunes, octave switch.
