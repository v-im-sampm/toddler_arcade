# Beat Sharks (toddler looper)

Spec: [games/beat_sharks.md](../../../../games/beat_sharks.md). PRD: [docs/prd/beat_sharks.md](../../../../docs/prd/beat_sharks.md).
Mockup: [docs/mockups/beat_sharks.html](../../../../docs/mockups/beat_sharks.html).
Status: **implemented; host and desktop-sim tested**. Not yet played on the physical board.
Host and desktop-simulator results are not device validation.

## Behaviour
- 3 rows × 6 steps. Row 0 = your recorded sound, row 1 = taps (clap/woodblock/shaker),
  row 2 = drums (boom/tom/bongo).
- Tapping a cell cycles **empty → sound 1 → sound 2 → sound 3 → empty**. Each sound is a
  different-sized shark, and the new sound plays once as a preview.
- Row 0 plays the recording at three pitches (rates 49097 / 65536 / 98193, which is −5 / 0 / +7
  semitones). Without a recording or a sampler it uses three built-in boings.
- Play/stop starts and stops the loop. Tempo cycles turtle (500 ms) → walk (375 ms) → rabbit
  (280 ms) per step.
- Record works like Sound Jar: the speaker is silent while recording and a bar fills over the take.
  A take that is too quiet keeps the old sound and the button wiggles.
- Filling every cell triggers a FULL_OCEAN celebration. A new recording triggers NEW_SOUND (stars).
  There are no scores and no failure states.
- Voice: AGAIN (re)starts the loop from step 1, HELP replays the prompt. Voice never starts recording.

## Privacy
Same as Sound Jar: the one clip lives in PSRAM through `ISampler`, and `privacy_wipe` runs on
game exit, pause/sleep and parent-gate entry. The parent **Mic** toggle applies here too.

## Layout (320×240)
| Target | Rect |
|---|---|
| Play | `{60,0,56,56}` |
| Record | `{120,0,56,56}` |
| Tempo | `{180,0,80,56}` |
| Cell (row r, step s) | `{6+52s, 62+60r, 48, 52}` |
| Home / Mic | owned by the SceneManager |

The launcher shows Beat Sharks on page 3, slot 0.

## Memory and size
| Item | Bytes | Where | Evidence |
|---|---|---|---|
| View rasters (9 sharks, bubble, 8 icons, fin, star, tile; ARGB8888) | 169 584 | PSRAM | desktop calculation |
| LVGL pool for the view | 6 568 (64-bit) | LVGL pool | desktop sim (86 840 → 93 408) |
| LVGL heap max_used, sim bench | 103 672 of 114 480 | LVGL pool | desktop sim |
| ESP-IDF image | 1 128 384 (+20 400 vs Sound Jar) | flash | ESP-IDF size report; 73% of 4 MiB app partition free |

## Tests
- Host (`ctest`): `beat_sharks_test` (22 logic tests: cycling, sequencing, tempo, record,
  celebrations, privacy wipe), `beat_sharks_art_test`, and the Beat Sharks checks in
  `audio_synth_test`.
- Desktop sim: `little_arcade_sim --bench --game beat_sharks --seconds 30 --shots DIR`.
  It runs a scripted session (pattern, groove, record, fill the ocean, tempo, stop) and exits
  non-zero unless a sound was caught, the full ocean was reached and the tempo changed twice.
- Device: pending. Still to measure: step timing jitter, tap-to-sound latency, PSRAM/heap
  watermarks, SPL at max volume with all cells full, and whether the 4 px cell gap works on NS2009.

## Rollback
Revert the Beat Sharks commit. To disable it without reverting, remove the `BEAT_SHARKS` entry
from the `Arcade` games table.
