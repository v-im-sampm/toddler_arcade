# Sound Jar: v2 design

| Field | Value |
|---|---|
| Game id | `sound_jar` |
| Template | music-toy (sampler) |
| World location | Echo Cave |
| Play value | Voice play, cause-and-effect, pitch and effects |
| Capabilities | music, narration, microphone_sampler, pointer_drag, particles |

## 1. Pitch
Say something into the magic jar, then play your own voice high, low, backwards, robot or wobbly.
*Feels like:* a magic echo that plays with you.

## 2. v1 baseline (keep the DSP behaviour)
- Pads at −12, −5, 0, +7, +12 semitones. Hold > 350 ms loops; slide with a 12 px dead zone bends
  pitch.
- Effects: Backwards, Echo (250 ms, 0.45 feedback), Robot (55 Hz ring mod), Wobble (6 Hz,
  ±1.5 semitones).
- Recording: trim, normalise to −6 dBFS, 5 ms fades. A take is too quiet if its sustained level
  < 512 or it lasts < 150 ms.
- Privacy: one clip ≤ 3 s, RAM-only, wiped on exit, pause, sleep and parent-gate entry.

## 3. Experience
### 3.1 Core loop
1. The jar sits open with a glowing lid. Tap the big record button (red circle with a mic icon): the
   jar lid opens, a gentle countdown of 3 dots plays (no beep-pressure), and the child speaks while a
   live waveform swirls into the jar as coloured mist (level-reactive).
2. Recording stops on silence or at 3 s. The jar closes with a "pop", and the mist settles in the
   jar's colour.
3. Too quiet / too short: a friendly "Let's try again, a bit louder!" with the jar wiggling. No
   failure sound.
4. Five pads (shape+colour+size: biggest = lowest) play the clip at their pitch. The jar character's
   mouth/eyes animate with the clip envelope.
5. Hold a pad to loop it; slide up/down to bend. Effect toggles (4 round icons: arrow-back,
   echo-rings, robot head, wavy line). Combining is allowed (cap: 2 at once).
6. A "bin" icon (or a 2 s hold on the jar) empties the jar with a whoosh animation (explicit wipe).

### 3.2 States
`EMPTY → COUNTDOWN → RECORDING → PROCESSING → FULL ⇄ PLAYING`; `TOO_QUIET → EMPTY`.

### 3.3 Layout
```
y 0-56    [HOME]  [effects ×4, 48×48 at 72..248]  [MIC]
y 56-160  jar centre {112,60,96,96}; record button {232,96,72,72}; empty {16,96,56,56}
y 168-240 5 pads (56×64 each, hit 64×72)
```

## 4. Animation
The jar is a character (eyes + mouth); the mist inside uses particles. Pads light and squash; the
pitch bend stretches the pad vertically. Robot effect: the jar gets a robot visor. Backwards: mist
swirls in reverse.

## 5. Audio
- Key follows the pad roots. Ambient bed: cave drip (very quiet).
- DSP chain per voice: resample (pitch) → optional reverse → ring mod / vibrato → echo → bus limiter.
  Must run on all 3 platforms with **bit-identical or tolerance-tested output** (golden audio tests).
- Device: I2S RX and TX share BCLK/WS, so a full-duplex config at one sample rate is needed (v1
  evidence). The amp has no mute pin, so output silence while recording to avoid feedback.

## 6. Input
| Action | Touch | Voice | Notes |
|---|---|---|---|
| Record | record button | — | The Mic button for voice commands is disabled in this game to avoid ambiguity; the shell shows it greyed |
| Play | pad tap/hold/slide | — | |
| Effects | toggles | — | |
| Empty jar | bin | — | |
| Repeat / help | — | AGAIN / HELP (via a parent-enabled setting only) | |

## 7. Privacy (non-negotiable)
- One clip, ≤ 3 s, RAM only, never written to flash/SD/disk/IndexedDB/app storage, never sent
  anywhere.
- Wiped on: exit, pause/background, sleep, parent gate entry, new recording.
- Web/phone: microphone permission prompt is parent-facing. Explain it in the parent area; the game
  remains fully playable with a built-in sample sound if permission is denied ("Jar has a frog
  sound!").
- Manifest `privacy.recording` block must declare all of the above (schema-enforced).

## 8. Acceptance criteria
| ID | Criterion | Verified by |
|---|---|---|
| SJ-1 | Record → play on all 5 pads with correct pitch ratios (±5 cents) | golden audio test |
| SJ-2 | Effects match v1 parameters (echo 250 ms/0.45, ring 55 Hz, wobble 6 Hz ±1.5 st) | golden audio test |
| SJ-3 | Too-quiet / too-short takes trigger the friendly retry | unit + replay |
| SJ-4 | Clip memory zeroed on all 5 wipe triggers; no persistence APIs called | unit + storage audit |
| SJ-5 | Mic denied → built-in sample mode fully playable | replay (web/phone) |
| SJ-6 | No feedback howl on device with speaker at max cap | device bench |
| SJ-7 | Record start latency ≤ 150 ms from tap | device bench |
