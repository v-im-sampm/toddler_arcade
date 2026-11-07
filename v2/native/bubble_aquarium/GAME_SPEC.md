# Bubble Aquarium: browser reference contract

Source of truth: [bubble_aquarium.html](../../reference/bubble_aquarium.html). This is
a calm, endless, noncompetitive aquarium; there is no score target, loss, or ending.
Meals and visits are counters, not points.

- **Stage:** 320 x 240 landscape. Intro screen has a large play button; touching
  anywhere starts a 1.5 s skippable fish greeting. The Home tile at (0,0,56,56)
  returns to the start screen. Mic at (264,0,56,56) is disabled.
- **Animals:** orange Stripey, pink Puffy, yellow/lilac Longfin wander in separate
  lanes and look at touches. Touching a fish makes it greet or peek if asleep.
  Touching empty glass attracts the nearest available fish. Dragging a fish or
  across the glass makes that fish follow; a 12 px dead zone avoids false drags.
- **Feeding:** tap shaker (72,0,56,56) to drop one pellet at (100,50), or
  drag it for another per 26 px traveled (at most once per 120 ms). Maximum six
  pellets. Pellets fall, splash at y=62, sink to y=211; nearest free fish claims
  each, swims to it, gulps, then makes a 1.6 s happy loop. In bedtime it eats,
  then returns to sleep. A meal increments the internal counter.
- **Bubbles:** bubble button (192,0,56,56) or chest at (190,186,60,48)
  releases five bubbles at 300 ms intervals; the chest also releases two every
  9-14 s (14-19 s in bedtime). Maximum eight; the oldest is recycled quietly.
  Bubbles start at y=198, grow to radius 18-22, bob upward, pop at the surface
  or when touched (radius + 6 hit area). Fish respond to pops.
- **Guests:** every third gulp brings one slowly crossing turtle, seahorse,
  or jellyfish, in that order. Additional visits queue (max two). Guests are
  drawn in front of fish; tapping one makes it wave and chime.
- **Bedtime:** moon tile (132,0,56,56) or B toggles dimmed lights over 2 s,
  fish sleeping in their bed spots, and music changing from 70 to 60 BPM.
  Tap again to wake them. After 6 s of daytime inactivity the shaker is
  highlighted, and at 12 s a hint chimes; no hints at bedtime.
- **Simulation:** seeded deterministic gameplay at 50 ms steps; independent
  frame updates interpolate visual feedback. Home resets the world. The
  browser has separate cosmetic randomness for particles and blink timing.
- **Visual/audio:** original procedural pastel sky, blue water gradient,
  sand, coral, seaweed, treasure chest, expressive fish, guests, translucent
  bubbles and cream buttons. Web Audio uses quiet E-major pentatonic lullaby,
  filtered reef wash, additive chimes, shaker noise and gentle pitch glides.
  No external images, fonts, sound files, storage or network assets exist.
  D/R/P/B are browser debug/reduced-motion/particle-cap/bedtime shortcuts.

The desktop port keeps the same touch hotspots and primary state transitions.
Known departures are listed in [README.md](README.md).
