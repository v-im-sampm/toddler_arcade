# Hatch-a-Pet (G02)

Spec: [games/hatch_a_pet.md](../../../../games/hatch_a_pet.md). Status: **scene logic + host tests only**.
There's no LVGL view yet and no assets yet. Every asset id below is a placeholder. The game has not run on a device.

## Behaviour
- **INTRO** lasts 1.2 s, and any input skips it. Skipping doesn't count as a crack.
- **EGG** (the spec's PLAY state): a big patterned egg sits in the middle. It needs 3–5 taps to crack, picked at random per egg. Every tap in the play area adds a crack, so there are no "missed" taps. The egg wobbles on each tap and the crack lines grow.
- **HATCH** (the spec's CELEBRATE state): the shell bursts and the creature appears over 1.6 s. Input is ignored during this animation, which makes repeated taps and voice input safe.
- **PET** (back to PLAY): the creature waves hello. The prompt badge shows the "another egg?" question as a picture. The child can pet the creature as often as they like. They can also keep it (NO) or ask for a new egg (YES), which returns to EGG.
- The game has no failure state, no timers that end play, no score and no text.
- Creatures come from a shuffle bag, so all 8 appear before any repeat and the same creature never hatches twice in a row. The egg pattern is random. All randomness comes from `ctx.rng`, so the same seed gives the same sequence.
- `exit()` clears the active context, so later `update()`/`handle()` calls do nothing. `enter()` starts fresh with no saved profile. The game uses fixed members only, with no heap allocation.

### Original creatures (8; the spec requires at least 6)
| Id | Description |
|---|---|
| BLOOP | round teal blob with one curly antenna |
| FLUFFWING | peach puffball with two tiny wings |
| SPROUTLING | green bean body with a leaf on top |
| ZIGZAG | purple lizard-ish critter with a zigzag tail |
| PUDDLEPUP | blue droplet puppy with floppy ears |
| GLIMMERBUG | yellow beetle with a dotted shell |
| ROLYPOLE | orange striped ball that rolls |
| SNORKLE | pink critter with a long curly snout |

Egg patterns: SPOTS, STRIPES, ZIGZAGS, STARS.

## Layout (320×240)
| Target | Rect {x,y,w,h} | Shown in |
|---|---|---|
| Egg | {104, 68, 112, 136} | INTRO / EGG / HATCH |
| Creature | {96, 72, 128, 128} | PET |
| NO / "stay" button | {16, 136, 64, 64} | PET |
| YES / "another egg" button | {240, 136, 64, 64} | PET |
| Prompt badge (optional top bar) | {132, 4, 56, 56} | EGG / PET |

Every target is at least 48×48 and at least 8 px from the others. None of them overlaps Home {0,0,56,56} or Mic {264,0,56,56}.

## Input map (`active_contexts()` = `Context::General`)
| Action | Touch | Voice intent |
|---|---|---|
| Crack the egg | tap egg (or anywhere in the play area) | YES (EGG) |
| Another egg | YES button | YES, AGAIN, NEXT (PET) |
| Stay with this pet | NO button | NO (PET) |
| Pet the creature | tap creature (or anywhere in the play area) | NO (PET) |
| Hear the prompt again + hint glow | tap prompt badge | AGAIN (EGG), HELP |
| Skip intro | any tap | any routed intent |
| Home | Home button (SceneManager) | BACK (SceneManager) |

In the EGG phase, NO and NEXT do nothing; the egg just waits.

## Cue → asset placeholders (to be declared in `assets/manifest.json`)
| Cue | variant | Placeholder asset id | Visual counterpart |
|---|---|---|---|
| PROMPT | 0 = tap the egg, 1 = another egg? | `hatch_a_pet.prompt.{tap_egg,another_egg}` | badge picture + hint glow |
| TAP | 0 | `common.tap` | egg wobble |
| NOTE | crack number 1–5 | `hatch_a_pet.crack.{1..5}` (rising) | crack lines grow |
| SUCCESS | creature index | `hatch_a_pet.hatch` | shell burst |
| CELEBRATE | 0 | `common.celebrate` | gentle sparkles (no flashing) |
| REACTION | creature index | `hatch_a_pet.creature.{bloop,…,snorkle}` | happy bounce |

## Tests
`hatch_a_pet_test` (host only) covers:
- layout safety and spacing
- intro timing and prompt
- intro skip without cracking
- 3–5 taps per egg, with every tap progressing (all values seen)
- cue variants
- at least 6 creatures, with all 8 seen per bag and no back-to-back repeats
- same-seed determinism and seed variety
- voice/touch equivalence for YES, NO, AGAIN and the badge
- petting, plus AGAIN/NEXT/HELP in PET
- harmless NO and unrelated input in EGG
- repeated taps and voice input during HATCH
- audio disabled (`NullFeedback`)
- Home pressed mid-hatch, then a fresh re-entry
- playing through the `SceneManager` (EGGS launches the game, GO is filtered out, Home at (10,10) returns to the menu)
