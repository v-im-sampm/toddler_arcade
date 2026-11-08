# Little Arcade launcher (Color Pop + Bubble Aquarium)

A two-tile main menu that starts either native game and brings the child back with the
game's own Home button. It is an isolated project: neither `../color_pop/` nor
`../bubble_aquarium/` is modified; the launcher compiles their portable game, view and score
sources and owns only the shell, menu and platform glue.

## Behaviour

| Item | Rule |
| --- | --- |
| Menu | Left tile = Color Pop (coral frame, crab + three bubbles), right tile = Bubble Aquarium (teal frame, clown fish + bubbles). Thumbnails and actors are the games' exported sprites, no text. |
| Hit areas | Whole screen halves (x < 160 → Color Pop), so a toddler can't miss. The tapped tile drops 4 px for 0.18 s, then the game starts. |
| Start | The menu tap counts as each game's Play tap: Color Pop goes straight into its intro, Aquarium starts swimming. |
| Home | Top-left 56×56 (both games' Home rect). In any game phase it resets that game and returns to the menu. |
| Guard | 0.35 s after every scene change taps are ignored so one tap can't fall through two screens. |
| Touch | Touch-only, voice not used (no mic probe). Drags are forwarded to Aquarium only when the touch started inside the game. |
| Audio | Color Pop's score runs through `GatedScore` (80 ms linear fade, silent and not ticked outside Color Pop). Aquarium cues use its device cue mapping on both desktop and device; the menu plays a soft note on tile press and tap on Home. Parent volume and the amp-safe limiter are unchanged. |

Rules are in `src/launcher/shell.{hpp,cpp}` and are host-tested in `test/launcher_test.cpp`.

### Differences from the v2 design docs

* The v2 docs sketch an "island launcher"; that is still a design-review item. This is the
  simplest touch-complete menu for the two finished games.
* Desktop Aquarium audio uses the device cue synth (`AquariumCues`), not the standalone desktop
  toy synth, so the two platforms sound the same.

## Desktop (Windows, LVGL 9.5 + SDL2)

```powershell
cmake -S v2\native -B build-v2-native
cmake --build build-v2-native --config Release --target launcher_native launcher_test
ctest --test-dir build-v2-native -C Release -R launcher
.\build-v2-native\launcher\Release\launcher_native.exe            # window, mouse = touch
.\build-v2-native\launcher\Release\launcher_native.exe --check --screenshots build-v2-native\launcher\shots
```

Options: `--zoom 1..4`, `--seed N`, `--mute`, `--exit-after S`, `--pack FILE`,
`--aquarium-assets DIR` (defaults are copied next to the exe by the build).

`--check` runs offscreen: menu → Color Pop (play) → Home → Aquarium (shaker drag feeds) → Home,
twice, verifying the returning menu is pixel-identical, and writes `launcher_menu*.bmp`,
`launcher_color_pop.bmp`, `launcher_aquarium.bmp`.

## Device (ESP32-S3, 16 MB flash, 8 MB PSRAM)

```powershell
pwsh v2\native\launcher\device\build.ps1           # both exporters + wslc ESP-IDF 5.5.6 build (never Docker)
pwsh v2\native\launcher\device\flash.ps1 -Port COM16  # refuses non-ESP32-S3 / non-16MB targets
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 -p COM16 read-mac   # optional port sanity check
```

Monitor: any 115200-baud serial terminal on the port (e.g. `python -m serial.tools.miniterm COM16 115200`);
the app logs `[boot]`, `[scene]`, `[frames]` and `[mem:*]` lines with tag `launcher`.

Partition layout (`device/partitions.csv`):

| Name | Offset | Size | Contents |
| --- | --- | --- | --- |
| factory | 0x10000 | 4 MiB | `little_arcade_launcher.bin` |
| cp_assets | 0x410000 | 1 MiB | `color_pop.pack` (memory-mapped, checked against `pack_signature.hpp`) |
| assets | 0xA10000 | 5.9 MiB | Aquarium `sprite_assets.bin` (copied to PSRAM by its view) |

The old `model` partition is dropped: no voice model is used.

### Measured (physical device, COM16, ESP32-S3 rev v0.2)

| Metric | Value |
| --- | --- |
| App image | 1,006,416 B (24 % of the 4 MiB factory partition) |
| Assets | color_pop.pack 571,920 B; sprite_assets.bin 3,891,040 B |
| Static DIRAM | 242,355 B of 341,760 B |
| Boot | menu visible 316 ms after the UI task starts; both games ready at 884 ms |
| Heap after audio start | internal 108,707 B free (min 43,904); PSRAM 4,113,784 B free |
| Menu render | p50 10.3 ms, p95 ≈ 20 ms per frame (interval p95 includes idle frames with nothing to redraw) |
| Aquarium render | p50 82–94 ms, p95 ≈ 102 ms (≈ 10 fps); same display path as the standalone Aquarium image |

Serial evidence shows boot, menu, and scene changes Menu → Color Pop / Aquarium → Menu on the
device. Those transitions came from physical taps not made by the agent; screen appearance,
calibrated loudness and touch feel are **not** verified by the logs.

## Known gaps

* Aquarium renders at about 10 fps on the device (its full-screen BGRA sprites); Color Pop's
  CP-5 frame-rate gap is documented in `../color_pop/README.md`. The launcher does not change
  either game's renderer.
* There is no parental settings screen in the menu; volume is the saved parent level.

## Rollback

Reflash a single-game image, which rewrites its own partition table:

```powershell
pwsh v2\native\color_pop\device\build.ps1;        pwsh v2\native\color_pop\device\flash.ps1 -Port COM16
pwsh v2\native\bubble_aquarium\device\build.ps1;  pwsh v2\native\bubble_aquarium\device\flash.ps1 -Port COM16
```

To remove the launcher from the repo, revert its commit (it only adds `v2/native/launcher/`,
one `add_subdirectory(launcher)` line and `.gitignore` entries).
