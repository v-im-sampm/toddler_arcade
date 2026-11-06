# Shape Garage: v2 design

| Field | Value |
|---|---|
| Game id | `shape_garage` |
| Template | custom (drag-to-slot) |
| World location | Bolt's Workshop |
| Play value | Shape matching, fine motor (drag), building |
| Capabilities | music, narration, voice_intents, pointer_drag, particles |

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
