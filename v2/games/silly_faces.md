# Silly Faces: v2 design

| Field | Value |
|---|---|
| Game id | `silly_faces` |
| Template | custom (mix-and-match) |
| World location | Mirror Pond |
| Play value | Emotions vocabulary, creativity, laughter |
| Capabilities | music, narration, voice_intents, particles |

## 1. Pitch
A friendly pond monster looks up from the water. Tap its eyes and mouth to make silly and feeling
faces, and collect a sticker for each feeling. *Feels like:* making faces in a mirror.

## 2. v1 baseline (logic only, no view)
- Eyes and mouth each cycle through 4 styles. Emotions: HAPPY, SLEEPY, SURPRISED, SAD.
- A sticker per emotion found, with a big celebration when all 4 are found. Idle hint after 8 s.
- Buttons: Show, Mix, New Monster.

## 3. Experience
### 3.1 Core loop
1. The monster's face fills the centre. Tap the eyes to cycle eye styles, and tap the mouth to cycle
   mouths. Each change has a "boing" + squash, and the face does a small reaction.
2. When the eyes + mouth combination matches a known emotion, the monster *acts it out* (e.g.
   SURPRISED: jump + "Oh!" + hair stands up). The narrator names the feeling ("Surprised!"), and a
   sticker flies to the sticker row.
3. Non-emotion combinations are just silly: random goofy sounds, which are always rewarding.
4. **Show me** button (mirror icon): the narrator asks for a feeling ("Can you make a sleepy face?"),
   and the target sticker slot glows. The parts that need changing wiggle after 8 s.
5. **Mix** (dice icon): random face with a spin animation. **New monster** (egg icon): a different
   monster body/colour (4 bodies).
6. All 4 stickers: a big celebration, a pond splash and a dance, and the stickers reset.

### 3.2 States
`FREE ⇄ SHOW_ME(target) → FOUND → FREE`; `ALL_FOUND` celebration.

### 3.3 Layout
```
y 0-56    [HOME]  [sticker row 4×40 at 80..240]   [MIC]
y 56-184  face: eyes hit {72,64,176,56}, mouth hit {96,128,128,56}
y 184-240 [Show 24,184,64,56] [Mix 128,184,64,56] [New 232,184,64,56]
```

## 4. Characters and animation
SAD is shown gently (droopy eyes, small wobble lip, a single slow tear sparkle) and recovers to a
smile after 2 s with a narrator line "It's OK to feel sad. Hug!". Never scary. Each emotion has a
3–5 beat acted reaction. Faces are runtime part composition (eyes, mouth, brows) on device.

## 5. Audio
- G major pentatonic, 110 BPM. Boings, slide whistle, kazoo-like FM. Pond bed.
- Each emotion has a musical colour: HAPPY bright major, SLEEPY slow descending, SURPRISED upward
  slide, SAD soft minor-ish descent resolving to major.

## 6. Input
| Action | Touch | Voice | Notes |
|---|---|---|---|
| Change eyes / mouth | tap part | — | |
| Show me | button | — | |
| Accept a suggestion "Want a new monster?" | ✔ | YES | |
| Decline | ✖ | NO | |
| Repeat | — | AGAIN | |
| Help | — | HELP | Required parts glow |

## 7. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| SF-1 | All 4 emotions reachable touch-only; each names the feeling and awards a sticker once | replay |
| SF-2 | SAD reaction resolves to comfort within 2.5 s | replay + review |
| SF-3 | Show-me idle hint at 8 s wiggles exactly the parts that need changing | replay |
| SF-4 | Stickers reset after ALL_FOUND and on exit (RAM only) | replay |
| SF-5 | Faces readable at device size (review screenshots at 1×) | review |
