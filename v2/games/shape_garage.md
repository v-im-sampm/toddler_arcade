# Shape Garage: v2 design

| Field | Value |
|---|---|
| Game id | `shape_garage` |
| Template | custom (drag-to-slot) |
| World location | Bolt's Workshop |
| Play value | Shape matching, fine motor (drag), building |
| Capabilities | music, narration, voice_intents, pointer_drag, particles |

**Reference sketch (v2 mockup, awaiting owner review):** [`shape_garage.html`](../reference/shape_garage.html) is the
runnable browser reference for this design: scene, motion and Web Audio score. Once the owner approves
it, the device port follows it line by line ([06 §4.5](../docs/06_visual_motion_design.md)).
Notable differences from the design below:
- Voice intents (YES/NO/AGAIN/HELP) and the 10 s "Should I help?" offer are not sketched (Mic shown disabled); help is touch-only: glow + hop at 6 s idle, prompt motif + helper-hand demo at 12 s, auto-glide on the 2nd miss.
- Narration is stood in for by the badge (it shows the next piece's shape; tap it to replay) and a two-note prompt motif pitched by that shape.
- Adds a 4th shape, a yellow STAR (clunk degree 3, from §10), so a 4-socket vehicle has four distinct shapes.
- ROLL_IN (0.8 s roll + pieces popping into the tray) is skippable with a tap; COMPLETE is a 1.5 s wake/rev/horn beat before the 1.8 s DRIVE_OFF.
- Vehicles roll in from the left and leave to the right; the truck's last socket is its cab door, the boat rides a wheeled trailer, the rocket lifts slightly as it leaves.
- Sleep is closed eyes + rising bubbles (no "Z"); one eye peeks when a piece clicks in and the smile grows as sockets fill.
- Socket count steps 2 → 3 → 4 per finished vehicle and back one after a round that needed auto-help.
- Snap radius 28 units (+8 per miss, max 44) with a gentle magnetic pull inside 1.5× that; drag follow is a stiff critically damped spring (no overshoot).
- Tray order is shuffled so it never mirrors the socket order; tray pieces also breathe slightly between their 5 s wiggles.
- Wrong socket plays a soft rising boing + the piece's note; vehicle taps snore (asleep) or honk (awake), so taps during COMPLETE/DRIVE_OFF still answer.
- Wheel spin is capped at 1 rev/s; particle cap 32 (P toggles the web cap for comparison).

## 1. Pitch
Drag chunky shape pieces into a toy car or rocket. When it's built, it zooms off!
*Feels like:* building with real toy blocks.

## 2. v1 baseline
- Shapes: circle, square, triangle. Tap a part, then tap a slot (no drag in v1).
- A wrong slot wiggles and the correct slot glows. A second miss auto-snaps the part.
- Hit area has 6 px slop. Voice: YES, NO, AGAIN, HELP.

## 3. Experience
### 3.1 Core loop
1. A vehicle silhouette (car, truck, rocket, boat; rotating per round) shows 2–4 shaped sockets.
   Matching pieces sit in a parts tray.
2. **Drag (v2 primary):** the piece lifts (scale 1.1, shadow) and follows the finger with slight lag
   (spring). Dropping within the snap radius (≥ 24 units) of the right socket gives a magnetic snap,
   a ratchet click and a sparkle.
3. **Tap-tap (kept as a complete alternative):** tap a piece (it lifts and glows), then tap a socket.
4. Wrong socket: the piece bounces back to the tray along an arc, and the correct socket glows
   (no negative sound). On the 2nd miss for that piece it auto-glides into place.
5. All pieces placed: the vehicle wakes up (eyes open), revs, and drives off-screen with a dust trail
   and a horn tune. A new vehicle rolls in.

### 3.2 States
`ROLL_IN → BUILD(pieces) → COMPLETE → DRIVE_OFF(1.8 s) → ROLL_IN`

### 3.3 Layout
```
y 0-56    [HOME]                                   [MIC]
y 56-168  vehicle with sockets (each ≥ 56×56)
y 176-240 parts tray (pieces 56×56, hit 64×64)
```

## 4. Characters and animation
| Element | Idle | Press | Success | Hint | Other |
|---|---|---|---|---|---|
| Piece | tiny wiggle every 5 s | lift scale 1.1 + shadow | snap with `squash.land` | bounce | drag follow: critically damped spring |
| Socket | dashed outline breathes | — | fills, glow pulse | `glow.hint` | |
| Vehicle | eyes closed (sleeping) | — | wake, rev bounce, drive off | — | dust particles |

## 5. Audio
- D major pentatonic, 96 BPM. Clunks tuned to scale degrees by shape (circle=0, square=2,
  triangle=4), ratchet clicks, horn tune on completion. Workshop bed.
- Narration: "Find the circle!", "Click!", "It's ready! Vroom!".

## 6. Input
| Action | Touch | Voice | Notes |
|---|---|---|---|
| Place piece | drag OR tap-tap | — | Both always available |
| Confirm auto-help | — | YES | "Should I help?" offered after 10 s idle |
| Decline | — | NO | |
| Repeat | — | AGAIN | |
| Help | — | HELP | The next piece glides halfway, then waits |

Drag requires continuous touch tracking (PF/touch requirement in doc 04). If device drag quality is
poor, tap-tap remains fully playable.

## 7. Adaptation and help
2 sockets → 3 → 4 over rounds. Snap radius is generous (tuning) and grows after misses.

## 8. Budgets
Flash ≤ 1 MiB, particles ≤ 32. Drag must keep ≥ 30 FPS on device (dirty rect = piece + shadow +
previous position).

## 9. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| SG-1 | Vehicle completes via drag only and via tap-tap only | replay ×2 |
| SG-2 | Wrong drop returns the piece + hint; 2nd miss auto-places | replay |
| SG-3 | Drag latency (finger→piece) ≤ 50 ms p95 on device | device bench |
| SG-4 | Drop near the screen edge or outside never loses a piece | replay |
| SG-5 | Voice YES/NO/HELP equals touch equivalents | replay |

## 10. Later
More shapes (star, heart, half-circle), colour-matching mode, build-your-own free mode.
