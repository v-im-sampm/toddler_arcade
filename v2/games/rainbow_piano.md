# Rainbow Piano: v2 design

| Field | Value |
|---|---|
| Game id | `rainbow_piano` |
| Template | music-toy |
| World location | Rainbow Bridge |
| Play value | Free musical exploration, shapes, colours, optional follow-the-light |
| Capabilities | music, narration, pointer_drag (glissando), particles |

**Reference sketch (v2 mockup, awaiting owner review):** [`rainbow_piano.html`](../reference/rainbow_piano.html) is the
runnable browser reference for this design: scene, motion and Web Audio score. Once the owner approves
it, the device port follows it line by line ([06 §4.5](../docs/06_visual_motion_design.md)).
Notable differences from the design below:
- Instrument picker: bells, flute, drum and marimba fill the top band x 64..256 exactly (4 × 48 px). Toy piano, the default, sits on the rainbow's left cloud foot {4,58,56,58}, because five 48 px icons do not fit in 64..256.
- The groove toggle is a metronome on the right cloud foot {260,58,56,58}, not a drum, so it is not confused with the drum instrument. It is off by default and has its own 6-voice pool. The visual quantise is note creatures popping on the beat.
- Follow mode: entering plays the "Follow the light!" cue, then the light previews the whole tune. Tapping the bulb off and on keeps the tune and step and replays the preview; this is the touch path for AGAIN.
- Follow progress: each correct note lights one rainbow band. Tunes are 6 original 5-note phrases from a seeded shuffle bag.
- Help: follow mode bounces the target after 5 s, plays the spoken cue at 12 s, and shows a helper hand after 2 wrong keys or the spoken cue. Free play, which the design does not specify, gets a silent glow wave at 6 s and "Play the rainbow!" at 12 s.
- "Spoken" prompts are marimba motif stand-ins until the voice pack exists.
- Glissando: key entries are queued and played ≥ 60 ms apart; fast slides fill in skipped keys. A 3 px hysteresis stops chatter at key edges.
- Voices: key notes have their own 5-voice pool, one per key; a retrigger or steal fades in about 20 ms. UI cues keep the shared 8-voice limit, and the groove has its own 6, so the total can exceed §9's 6.
- Key note tails of 0.4–1.2 s exceed the 150 ms tap-sound rule by design (§5 release tails).
- Note creatures can be tapped (pop plus a soft note). Particles are capped at 48 per §9.
- Keys still play during the 2 s RAINBOW celebration (no input block).

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
