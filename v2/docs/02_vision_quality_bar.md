# 02: Vision and quality bar

## Vision
> A pocket-sized toy that feels **alive**. Every touch is answered instantly with a squish, a sparkle
> and a sound that fits the music. Characters breathe, blink and react. A three-year-old can play
> every game without reading, without failing, and without an adult. A parent trusts it completely:
> nothing is recorded, nothing is sent, nothing is sold.

v2 is three things:
1. **A toy:** twelve polished games with a shared world, characters, and sound.
2. **A platform:** an engine plus Game SDK. A new game is a folder with a manifest, logic, scenes,
   assets and tests. It runs the same on device, web and phone.
3. **A pipeline:** artists and designers author in real tools. The pipeline produces optimised,
   budget-checked assets for every target automatically.

The runnable [quality-bar sketch](../reference/quality_bar.html) is the visual and interaction
reference for this document. It demonstrates the intended level of response and polish; it is not
production code, a required art style, or evidence of device performance. Every game applies the
shared standards below through its own original characters, setting, and mechanics.

## Design pillars
| Pillar | Means | Does not mean |
|---|---|---|
| **Alive** | Idle motion everywhere (breathing, blinking, swaying), anticipation and follow-through on every action | Constant busy motion. Calm by default, energetic on response |
| **Instant** | Visual response in the same frame the touch is read, sound in ≤ 50 ms | Waiting for an animation to finish before accepting input |
| **Musical** | All cues in one key and tempo per scene, quantised where it helps, so random tapping sounds like music | Loud jingles or long fanfares |
| **Forgiving** | Every tap does something nice; wrong answers become hints; auto-help after repeated misses | Fail states, timers, scores, lives |
| **Readable** | Big silhouettes, high contrast, colour + shape + position cues, one focal point at a time | Clutter, small details, text |
| **Calm-safe** | Soft palette, no strobing, gentle volume curve, bedtime-friendly sleep | Over-stimulation ("casino" effects) |
| **One world** | Shared cast, palette, typeface-free iconography, and sound palette across games | 12 unrelated art styles |

## Concrete quality bar (each item must be demonstrable)
### Visual
- Characters are illustrated with consistent line weight, soft shading (2–3 tones + highlight), and
  a coloured outline (no pure-black lines). Style reference: **original** chunky, rounded "toy"
  shapes. **No** likeness of existing IP.
- Each interactive object has at least: idle loop, press (squash), release (stretch + overshoot),
  success, and hint. Characters also have blink, look-at-touch, happy, surprised and sleepy.
- Backgrounds have 2–3 parallax layers with slow ambient motion (clouds, bubbles, stars) drawn in
  dirty regions only.
- Particles (sparkle, confetti, bubble, star, note) come from a shared, budgeted particle system.
- Transitions between scenes are themed and ≤ 500 ms (iris, wipe, bubble-pop), with no full-white flash.
- On the device, the animated region reaches ≥ 30 FPS, and full-screen transitions keep ≥ 20 FPS
  without visible tearing artefacts that a child would notice. Validate on the board.
- On web and phone, the same scenes run at 60 FPS and the same art is re-exported at higher density
  (2×/3×). Identical layout in logical units.

### Audio
- Each game has a **musical identity**: key, tempo, 2–4 instrument timbres, and an ambient bed.
  Interaction sounds are pitched to the scene's scale (pentatonic by default), so mashing sounds
  pleasant.
- An adaptive music layer (stems or procedural) rises gently with engagement and settles when idle.
- A narrator voice gives short spoken prompts ("Pop the blue bubble!") with a consistent warm
  persona, recorded or generated offline at build time. It is localisable later.
- Mixing: buses for music / sfx / voice / ambience. Ducking under voice, a limiter, and loudness
  normalisation of all assets.
- No clicks or pops: every sound has an envelope, and starts/stops are de-clicked.

### Interaction
- Touch phases: down, move, up and cancel, with hold, drag and slide helpers. **Drag is never
  required.** Tap alternatives always exist.
- Touch on screen shows an immediate soft "touch ripple" under the finger, even on a miss.
- Voice: push-to-talk with a friendly listening animation. A recognised intent animates the matching
  on-screen element, so the child links words to targets.

### Platform
- Adding a game requires **zero edits** to shell code: manifest-driven registration.
- Live preview in a browser with hot reload of logic, tuning values and assets.
- Headless deterministic behavior replays in CI for every game. Golden image/audio comparisons
  may be used as optional diagnostics, but frame-by-frame analysis is not a ship requirement.
- Budgets (flash, RAM, frame time, audio CPU) are checked per game.

### Per-game quality gate
Every game's design and implementation must make these observable, not just describe them:

| Check | Required evidence |
|---|---|
| Touch feels immediate | First visual acknowledgement is visible within the `PF-3` budget; touch ripple or an equally clear local response appears even on empty space. |
| Every interaction is alive | Each primary object has a calm idle state and distinct press/release, success, and hint responses. Effects never delay the next input beyond the documented, ≤300 ms exception. |
| Help is kind | The idle → visual → optional spoken → auto-help ladder is specified and replay-tested. A non-target action stays playful and is never penalized or scored negatively. |
| Sound belongs to the game | The game declares its musical identity; frequent sounds use its scale, are enveloped and capped, and have a visual counterpart. Audio-off play remains complete. |
| Device budget is real | Particle/voice/entity pools have explicit caps; worst-frame area and memory budgets are named and tested. A preview is not device evidence. |
| Motion is safe and adjustable | Reduced-motion behavior and luminance/flash checks are specified and tested. Ambient motion remains slower and quieter than interaction feedback. |

Use the [game design template](../games/_TEMPLATE.md) to record these choices and the
[authoring workflow](09_game_sdk_authoring.md) to turn each check into acceptance criteria and
replay evidence. A game is not quality-bar complete until its criteria pass on the required evidence
tiers in [QA acceptance](11_qa_acceptance.md).

## Anti-goals
- No open-world, no progression grind, no collectibles that create compulsion loops, no streaks.
- No in-app purchases, no ads, no external links, no social or multiplayer networking.
- No user-generated content leaves the device.
- No "learning metrics" or profiles of the child.
