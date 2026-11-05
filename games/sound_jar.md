# Sound Jar
**Pitch:** Catch a sound in a jar, then make it rumbly, squeaky, backwards, echoey, robotic or wobbly.

## Core loop
Tap the red button and make a noise; the jar shows the sound. Tap the five creature pads (big = low, tiny = high) to play it, hold a pad to loop it, and slide along the pads to bend the pitch. Toggle Backwards, Echo, Robot and Wobble at any time. Full design: [`docs/prd/sound_jar.md`](../docs/prd/sound_jar.md).

## Input
Touch: record/stop, jar, four effect toggles, five pitch pads (tap, hold, slide). Voice: AGAIN (replay last sound) and HELP (prompt) only; voice never starts recording, and the launcher tile has no spoken name. Always include a visible Home control and a large touch alternative to every supported voice action. Commands are mapped via `config/commands.json`, not parsed inside game code.

## Scene states
`INTRO -> PLAY -> RECORDING -> CELEBRATE -> PLAY`, plus `PAUSED` and `EXIT`. Never block the UI while audio plays. Resume on re-entry without needing persistent player profile. A built-in sound is in the jar from the start, so every pad works before any recording.

## Visual/audio requirements
320×240 landscape, >=48px primary targets, simple 2D sprites and no rapid flashing. Original colorful artwork, a short spoken prompt, playful synthesized sound effects, positive nonverbal feedback. Speaker silent while recording (no live monitoring). Sampler output passes the existing limiter and volume cap. Asset IDs must be declared in the manifest; no network.

## Privacy
One clip of at most 3 s lives only in RAM. It is never saved, logged or transmitted, and it is zeroed on exit, pause/sleep and parent-gate entry. The parent screen has a Mic toggle (default on); with the mic off or missing, the red button becomes a sparkle that cycles built-in sounds.

## Acceptance criteria
Touch-complete with mic off and with audio off. A too-quiet or too-short take keeps the previous sound and the jar wiggles gently; there is no failure state. Handle repeated taps, Home mid-recording, voice input during animation, audio disabled, and navigation mid-animation. Deterministic seed for test mode; no unbounded allocations per frame.

## Implementation task
`firmware/components/games/sound_jar/` (scene, view, art, tests) on top of `ISampler` / `audio::Sampler`. Shell registration is in `arcade`.
