# Rocket Rescue: v2 design

| Field | Value |
|---|---|
| Game id | `rocket_rescue` |
| Template | custom (go/stop guidance) |
| World location | Star Hill (sky above it) |
| Play value | Start/stop control, planets, helping friends |
| Capabilities | music, narration, voice_intents, particles |

## 1. Pitch
Fly a little rocket between planets to pick up lost space friends, using big GO and STOP buttons.
*Feels like:* being the pilot.

## 2. v1 baseline (logic only, no view)
- 3 planets (RINGED, SPOTTED, STRIPED) and 3 friends (MOON_PUP, STAR_BUNNY, COMET_KITTY).
- Glide 2.5 s. Stars respawn after 0.9 s. GO and STOP buttons.
- Voice: GO, STOP, AGAIN, HELP.

## 3. Experience
### 3.1 Core loop
1. A friend waves from one planet ("Help! Come get me!"), and its planet glows.
2. The child taps a planet to set course (the rocket turns toward it), then GO: flames, a whoosh, and
   a 2.5 s glide along a curved path with a star trail. Collectible stars along the path chime when
   touched by the rocket (they respawn after 0.9 s).
3. STOP at any time: the rocket brakes with a wobble and hovers. GO resumes.
4. Arrival at the right planet: landing bounce, the friend hops in and cheers, and the rocket returns
   home to the dock, where the friend joins the dock party.
5. Arriving at a different planet: a local friendly alien waves and gives a star, and the planet with
   the friend glows brighter (hint). There's no failure.
6. All 3 rescued: a fireworks-style party (no flashing; slow blooming sparkles), then a new set.

### 3.2 States
`ASK → AIM → FLYING ⇄ STOPPED → LANDED → (RETURN) → ASK`; `PARTY` after 3.

### 3.3 Layout
```
y 0-56    [HOME]                                    [MIC]
y 56-176  space: 3 planets (each ≥ 64×64 hit), rocket, home dock left
y 176-240 [GO {48,180,96,56} green circle ▶]   [STOP {176,180,96,56} red octagon ■]
```
GO/STOP use colour + distinct shapes + icons.

## 4. Characters and animation
Rocket: idle hover bob, flame flicker (3 frames), lean into turns, brake wobble. Friends: wave,
cheer, ride. Planets: slow rotation (ring shimmer, spots drift).

## 5. Audio
- B♭ Lydian, 84 BPM. Pads + glockenspiel; a whoosh with pitch following speed; star chimes climb the
  scale in sequence; a brake "fwump".
- Narration: "Help!", "Go, rocket, go!", "Stop!", "You found <friend>!".

## 6. Input
| Action | Touch | Voice | Notes |
|---|---|---|---|
| Choose destination | tap planet | — | |
| Go | GO button | GO | With no destination set, the rocket goes to the friend's planet (auto-help) |
| Stop | STOP button | STOP | |
| Repeat | — | AGAIN | Friend calls again |
| Help | — | HELP | Path preview dots toward the friend |

## 7. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| RR-1 | Rescue all 3 friends touch-only; GO with no target still works | replay |
| RR-2 | STOP halts within 300 ms; GO resumes on the same path | replay |
| RR-3 | Voice GO/STOP equivalent to buttons, including mid-flight | replay |
| RR-4 | Party passes the flash analyser | CI |
| RR-5 | ≥ 30 FPS during flight on device (rocket + trail dirty regions) | device bench |
