# Bubble Aquarium: v2 design

| Field | Value |
|---|---|
| Game id | `bubble_aquarium` |
| Template | free-play (calm) |
| World location | Reef |
| Play value | Calm-down toy, caring, cause-and-effect |
| Capabilities | music, narration, particles, pointer_drag |

**Reference sketch (v2 mockup, awaiting owner review):** [`bubble_aquarium.html`](../reference/bubble_aquarium.html) is the
runnable browser reference for this design: scene, motion and Web Audio score. Once the owner approves
it, the device port follows it line by line ([06 §4.5](../docs/06_visual_motion_design.md)).
Notable differences from the design below:
- Bedtime toggle is a moon/sun button at {132,0,56,56}, between the shaker {72,0,56,56} and the bubble button {192,0,56,56}; the B key is the parent shortcut.
- No 3 s celebrate phase: each meal gets a gentle chime plus a 1.6 s happy loop (a wiggle in place under reduced motion; at bedtime the fish just dozes off again).
- Hint ladder: after 6 s idle the shaker glows and wiggles; at 12 s a soft chime stands in for the "Shake the food!" narration, with a helper hand. No hints at bedtime.
- Visitors come in turn (TURTLE → SEAHORSE → JELLY) rather than at random, drawn as an overlay in front of the fish, which wander below the visitor's path; tapping a visitor makes it wave and plays a soft bell.
- Bubbles come from the treasure chest (bubble button or chest tap; the chest also burps 2 every 9-14 s). At 8 bubbles the oldest pops quietly. Bubble radius is 18-22 px, hit radius r + 6.
- Dragging across the shaker drops 1 pellet per 26 px (6 pellets max). Dragging on the glass or a fish makes the nearest fish follow the finger.
- At bedtime, food wakes the nearest fish; it eats, then goes back to sleep.
- Caustics are simplified to soft light rays plus slow light loops on the sand. A quiet "reef wash" (filtered noise) plays under the music.
- The hint timer and INTRO length count simulation time (50 ms steps), so replays match exactly at any frame rate.

## 1. Pitch
A peaceful fish tank. Shake food in, blow bubbles, and watch the fish swim over. Sometimes a special
visitor drops by. *Feels like:* a living aquarium at bedtime.

## 2. v1 baseline (logic only, no view)
- Intro 1.5 s, celebrate 3 s. Fish: STRIPEY, PUFFY, LONGFIN.
- Up to 8 bubbles and 6 pellets. Food shaker {72,0}, bubble button {192,0}.
- Every 3 meals brings a visitor (TURTLE, SEAHORSE or JELLY). Uses 50 ms physics substeps.

## 3. Experience
### 3.1 Core loop
1. Fish wander with boids-lite steering (wander + avoid walls + gentle schooling). Seaweed sways,
   caustic light moves slowly, and bubbles rise from a treasure chest.
2. Food shaker: tap (or shake by dragging) to drop up to 6 pellets that sink with drift. The nearest
   fish turns, swims over and gulps (chomp + plink). After a meal a fish does a happy loop.
3. Bubble button: a stream of bubbles (≤ 8). Tapping bubbles pops them with soft notes.
4. Tap the glass: the closest fish turns to look and swims toward the touch (curious, never
   startled).
5. Every 3 meals, a visitor glides through slowly (TURTLE/SEAHORSE/JELLY) with its own lullaby
   phrase.
6. **Bedtime mode (new; parent or moon icon):** the lights dim, the music slows to 60 BPM, and the
   fish go to sleep. It's a wind-down activity that doesn't end the session automatically.

### 3.2 States
`INTRO → SWIM` (continuous), with `VISITOR` overlay and `BEDTIME` variant.

### 3.3 Layout
```
y 0-56    [HOME] [food shaker 72,0,56,56] [bubble 192,0,56,56] [MIC]
y 56-240  tank (full width); fish hit areas 64×48 minimum
```

## 4. Characters and animation
Fish: tail-wag swim cycle (procedural sine on tail sprite), turning squash, gulp, happy loop, sleep
(slow float + Zzz). Visitors: slow and large. Device: fish are small sprites with runtime transforms.
The background tank is a static layer, and caustics are a cheap palette/overlay trick or omitted on
device (decide in the graphics ADR).

## 5. Audio
- E major pentatonic, 70 BPM. Soft pads, bubble plinks. Reef bed. The calmest game: −3 dB music
  relative to others, no stings, and celebrations are gentle chimes.

## 6. Input
| Action | Touch | Voice | Notes |
|---|---|---|---|
| Feed | tap/drag shaker | — | Max 6 pellets; further taps make the shaker jiggle only |
| Bubbles | tap button / tap bubbles | — | |
| Attract fish | tap glass | — | |
| Repeat / help | — | AGAIN / HELP | Narrator suggests "Shake the food!" |

## 7. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| BA-1 | Simulation deterministic with 50 ms substeps (identical replay across platforms) | replay ×3 |
| BA-2 | Pellet and bubble caps hold under mashing | replay |
| BA-3 | Visitor appears every 3 meals | replay |
| BA-4 | Bedtime mode reduces brightness/tempo; no stings | replay + review |
| BA-5 | ≥ 30 FPS with 3 fish + 8 bubbles + 6 pellets on device; dirty area measured | device bench |
