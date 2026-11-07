# Bubble Aquarium (v2 sketch prototype on the v1 firmware)

Reference: [v2/games/bubble_aquarium.md](../../../../v2/games/bubble_aquarium.md) and
[v2/reference/bubble_aquarium.html](../../../../v2/reference/bubble_aquarium.html). This is an
interim implementation in the existing ESP-IDF/C++/LVGL firmware, not the approved v2 engine or
cross-platform port.

## Behaviour
- The 1.5 s intro enters open-ended swim; any input skips the intro without also acting.
- Three original fish wander in bounded lanes. Touching a fish earns a gentle reaction; tapping the
  water attracts the nearest available fish.
- The food shaker drops at most six pellets. Fish approach and eat them; there is no score or fail
  state. Every third meal brings the next visitor in the fixed TURTLE → SEAHORSE → JELLY sequence.
- The bubble button and treasure chest start a gentle stream. There are at most eight bubbles;
  tapping a bubble pops it quietly, and the oldest is recycled when the pool is full.
- The moon button toggles bedtime. Fish sleep, food wakes the nearest fish and it returns to sleep
  after eating, visual hints stop, and a gradual dim overlay is shown.
- The three top controls are offset slightly from the browser sketch to keep 8 px hit-area spacing
  while retaining 56×56 targets.
- After six idle seconds the food button glows; after twelve seconds a soft prompt cue and helper
  hand suggest feeding. AGAIN and HELP request the same prompt.
- Simulation uses deterministic 50 ms steps and the context RNG. All entity pools are fixed-size.
  Scene exit clears transient state; re-entry starts a fresh aquarium.

## Interim-port limitations
- The frozen v1 touch API sends press edges only, so shaker/fish dragging and continuous finger
  following are not available; large tap targets remain complete for play.
- The v1 feedback API has no game-owned score bus or tempo control. This port uses existing
  semantic cues and does not provide the sketch's E-major lullaby, reef-wash noise, or 70/60 BPM
  tempo change.
- Device FPS, dirty-area, heap/PSRAM deltas, and audio measurements have not been recorded by this
  source change. A successful firmware build or flash is not physical playthrough evidence.

## Host tests
`bubble_aquarium_test` covers toddler-safe control geometry, intro/skip behaviour, fish movement and
touch, feeding, the visitor sequence, bounded pools, bubble pop/burst, bedtime, idle hints, voice
help, audio-disabled play, exit/re-entry, seeded replay, and SceneManager navigation.
`bubble_aquarium_art_test` checks the procedural fish, visitor, bubble, food, plant, and bedtime
icons for non-empty, distinct artwork.
