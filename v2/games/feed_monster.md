# Feed the Monster: v2 design

| Field | Value |
|---|---|
| Game id | `feed_monster` |
| Template | custom (drag/tap counting) |
| World location | Snack Shack |
| Play value | Counting 1–3 (later 1–5), one-to-one correspondence |
| Capabilities | music, narration, voice_intents, pointer_drag, particles |

## 1. Pitch
Munch, a big fluffy monster, wants exactly *two* berries. Feed it and watch it chomp!
*Feels like:* feeding a silly pet.

## 2. v1 baseline
- Asks for 1–3 snacks; extra snacks are harmless.
- Chew 350 ms, giggle 500 ms, idle reminder 6 s.
- Layout: bowl {8,72,112,96}, monster {136,72,176,96}, number buttons y=176.
- Voice: ONE, TWO, THREE, AGAIN, HELP.

## 3. Experience
### 3.1 Core loop
1. Munch holds up N fingers and shows N dots in a thought bubble. The narrator: "I want **two**
   berries!"
2. The child gives snacks by dragging them from the bowl into Munch's mouth, or by tapping a snack
   (it hops into the mouth). Each snack: chomp + crunch note at a rising degree, and a counter dot in
   the bubble fills while the narrator counts ("One!", "Two!").
3. At N: Munch rubs its belly, a happy burp of sparkles appears, and it dances. Next request.
4. Extra snacks (> N): Munch giggles "Too many! Hee hee". There's no penalty, and the round still
   completes.
5. Number buttons (1/2/3 dot cards) remain as an alternative way to answer "how many?". Tapping the
   right card feeds that many automatically, with counting.

### 3.2 States
`ASK → FEEDING(k/N) → FULL → CELEBRATE → ASK`

### 3.3 Layout
```
y 0-56    [HOME]  [thought bubble with dots 120,4,80,56]  [MIC]
y 64-168  bowl {8,72,112,96} (3–5 snacks) | Munch {136,72,176,96} mouth hit {176,88,96,64}
y 176-232 dot cards 1/2/3 (each 72×56)
```

## 4. Characters and animation
Munch: idle breathe + eye follow + tongue lick, open-mouth anticipation when a snack is dragged near,
chomp (3 frames + `squash.land`), giggle, belly rub, dance. Snack variety: berries, carrots,
cookies (original designs). Visual counting dots always mirror the number.

## 5. Audio
- E♭ major pentatonic, 100 BPM. Tuba-ish pluck bass, crunches pitched to the count (degree 0, 2, 4).
  Kitchen bed.
- Narration numbers are the learning goal: clear, with stress on the number.

## 6. Input
| Action | Touch | Voice | Notes |
|---|---|---|---|
| Feed one | drag or tap snack | — | |
| Answer count | tap dot card | ONE / TWO / THREE | Card auto-feeds with counting |
| Repeat | tap thought bubble | AGAIN | |
| Help | — | HELP | Munch counts on fingers while the snacks glow |

## 7. Adaptation
1–3 at start, up to 5 after mastery (parent setting may cap). The idle reminder at 6 s is Munch
pointing at the bowl.

## 8. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| FM-1 | Each request satisfiable by drag, tap and dot card | replay ×3 |
| FM-2 | Count narration matches snacks given; extra snacks harmless | replay |
| FM-3 | Voice ONE/TWO/THREE equivalent to dot cards | replay |
| FM-4 | Chomp sync: sound within 30 ms of the jaw-close frame | device review |
| FM-5 | Bowl always refills, so the child can never run out | replay |
