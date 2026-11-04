# Who Made That Sound?
**Pitch:** Identify animal sounds.

## Core loop
Play a sound; choose animal among 2–4 illustrated cards.

## Input
Touch: animal choice; Voice: AGAIN BACK. Always include a visible Home control and a large touch alternative to every supported voice action. Commands are mapped via `config/commands.json`, not parsed inside game code.

## Scene states
`INTRO -> PLAY -> CELEBRATE -> PLAY`, plus `PAUSED` and `EXIT`. Never block the UI while audio plays. Resume on re-entry without needing persistent player profile.

## Visual/audio requirements
320×240 landscape, >=48px primary targets, simple 2D sprites and no rapid flashing. Original colorful artwork, a short spoken prompt, playful synthesized sound effects, positive nonverbal feedback. Asset IDs must be declared in the manifest; no network.

## Acceptance criteria
At least 6 animals; replay sound; nonpunitive retries. Handle repeated taps, voice input during animation, audio disabled, and navigation mid-animation. Deterministic seed for test mode; no unbounded allocations per frame.

## Implementation task
Create `firmware/components/games/animal_sounds/` scene source, headers, CMake registration, scene unit tests, and asset placeholders after core interface is finalized. Do not change global registry without integrator review.
