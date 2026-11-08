# Sound Jar native (LVGL 9 desktop + ESP32-S3)

Native, sprite-based port of [`v2/reference/sound_jar.html`](../../reference/sound_jar.html)
(design: [`v2/games/sound_jar.md`](../../games/sound_jar.md)). [GAME_SPEC.md](GAME_SPEC.md) describes how the game
must behave: update model, states, timers, seeded takes, hit areas, audio cues and visuals. It also lists where the
design and the runnable reference disagree; in those cases the port follows the reference.

| Folder | Contents |
|---|---|
| `src/game/` | Portable, deterministic gameplay (no LVGL/SDL/IDF); the variable-`dt` `frame()` model of the reference |
| `src/audio/` | Port of the reference Web Audio score (built-in sample takes, effects) as an `ExternalScore` |
| `src/ui/` | LVGL 9 sprite view, `sound_jar.pack` loader, render-cost profiler |
| `src/platform/desktop/` | SDL2 window, mouse-as-touch (including drag), desktop audio, offscreen checks |
| `device/` | Standalone ESP-IDF 5.5.6 project for the ESP32-S3 board (LVGL 9.5.0) |
| `tools/` | Canvas artwork exporter, seeded browser-vs-LVGL comparison, scenario scripts |

## Desktop (host)

```powershell
npm ci --prefix tools\quality_bar_golden                      # headless Edge driver used by the exporter/compare
cmake -S v2\native -B build-v2-native -G "Visual Studio 17 2022" -A x64
cmake --build build-v2-native --config Release --target sound_jar_native sound_jar_game_test sound_jar_score_test
ctest --test-dir build-v2-native -C Release -R sound_jar --output-on-failure
.\build-v2-native\sound_jar\Release\sound_jar_native.exe --zoom 2          # window; mouse = touch
node v2\native\sound_jar\tools\visual_compare.mjs                         # seeded browser vs LVGL, 14 scenarios
```

The exporter (`tools/export_assets.mjs`) runs the reference's own Canvas drawing functions in headless Edge at build
time. It writes `assets/generated/sound_jar.pack`: 1,886 sprites in RGB565, RGB565A8 or A8, 2,286,899 B in total.
It also writes [assets/manifest.json](assets/manifest.json), which records the source function, crop, origin, frame
timing, format and bytes of every sprite. The finished game needs no browser.

The game also runs offscreen with `--script tools\scenarios\NN.txt` combined with one of:
- `--screenshot F.bmp`
- `--dump`
- `--verify-redraw [--direct]`: compares each incremental frame against a full render
- `--dirty-stats`
- `--profile`: per-layer full-screen render cost

Other options are `--seed N`, `--mute`, `--reduced` and `--check`. Use `--window --script F --exit-after N --screenshot F.bmp` to run the real SDL window and save
its framebuffer.

## Device (ESP32-S3, built with wslc, never Docker)

```powershell
pwsh tools\idf_wslc_build.ps1 -Prepare                     # once: image little-arcade/idf:5.5.6
pwsh v2\native\sound_jar\device\build.ps1                  # -> device\build\sound_jar_sprite.bin + sound_jar.pack
pwsh v2\native\sound_jar\device\flash.ps1 -Port COM16      # checks ESP32-S3 / 16 MB first; 460800 baud
.venv\Scripts\python.exe -m serial.tools.miniterm COM16 115200   # monitor
```

Flash layout:
- bootloader at 0x0;
- partition table at 0x8000;
- app at 0x10000 (4 MiB factory);
- `sound_jar.pack` at the `assets` partition, 0xA10000 (0x5F0000).

At boot the firmware checks the pack signature and refuses to run with a stale pack. The pack is memory-mapped from flash and used in place.

`build.ps1 -Bench` (and `flash.ps1 -Bench`) builds a profiling image into `device/build-bench/`. It plays the
reference `autoTarget()` on its own (record → pads, holds and slides → effects) and once logs a per-layer render
profile.

`device/sdkconfig.defaults` holds this game's overrides to the shared `firmware/sdkconfig.defaults`: the FreeRTOS LVGL OS layer and 2 software draw units.
Delete `device/sdkconfig` after changing either defaults file.

### Rollback

- Exact restore of the image that was on COM16 before this work (`little_arcade_launcher`, `e01bc23-dirty`). Run this from the
  backup folder; it holds `boot_pt_0x0.bin`, `app_0x10000.bin` and `assets_0xA10000.bin`, read back with esptool
  before the first flash:
  `.venv\Scripts\python.exe -m esptool --chip esp32s3 -p COM16 -b 460800 write-flash 0x0 boot_pt_0x0.bin 0x10000 app_0x10000.bin 0xA10000 assets_0xA10000.bin`
- Or rebuild and flash another firmware with its own script, for example `v2\native\color_pop\device\flash.ps1`.
- Source rollback: `git revert <this commit>` removes `v2/native/sound_jar/`, the `add_subdirectory(sound_jar)` line
  and the `.gitignore` entries. No other game or the launcher is modified.

## Verification evidence

| Check | Result | Evidence |
|---|---|---|
| Game rules and edge cases (`sound_jar_game_test`) | pass | host |
| Score render / cue checks (`sound_jar_score_test`) | pass | host |
| Offscreen LVGL interaction and render check (`sound_jar_offscreen_check`) | pass | host |
| `--verify-redraw` on all 14 scenarios, partial and direct modes | 0 stale pixels | host |
| Seeded browser vs LVGL, 14 scenarios | all pass; MAE 3.1–4.8, >32 px-delta share 0.55–2.12% (thresholds MAE ≤ 5.5, ≤ 3.0%; rationale and negative controls in `tools/visual_compare.mjs`) | browser + host |
| SDL window run, framebuffer screenshot | renders correctly | host |
| Flash, boot, pack mapped, no errors (COM16: ESP32-S3 QFN56 rev v0.2, 16 MB flash, 8 MB PSRAM) | pass | physical device (serial log only) |

The serial log does not show what the screen looks like or how loud the sound is, and it does not prove touch
gameplay. Those have **not** been checked on the physical device.

### Measured budgets (physical device, serial logs)

| Item | Measured | Budget ([04_requirements.md](../../docs/04_requirements.md)) |
|---|---|---|
| App binary | 1,145,648 B in the 4 MiB factory partition (72.7% free) | PF-10 ≥ 10% headroom: met |
| Sprite pack | 2,286,899 B in the 0x5F0000 assets partition (63.3% free) | PF-10: met |
| Static internal RAM (DIRAM) | 276,915 / 341,760 B | — |
| Free internal heap, steady state | 80,575 B (START), ≈79,000 B (bench FULL play) | PF-7 ≥ 64 KiB: met at steady state |
| Free internal heap, lowest ever | 39,828 B (brief dip during boot, display/audio/touch init, before the view is created) | PF-7: **not met at the boot dip** |
| Free PSRAM | 8,270,564 B | PF-8 ≥ 1 MiB: met |
| Audio score load | 0.1% idle; 12.5–13.1% of one core in bench FULL play, worst block 4.2 ms, 0 drops | PF-9 ≤ 25%: met |
| Frame rate, START (idle) | p95 interval 8.2–8.4 fps; render p50 91 ms at 33% dirty | **PF-1 ≥ 30 fps p95: not met** |
| Frame rate, bench play (RECORDING / FULL) | 6.0 fps / 3.2–4.0 fps p95; render p50 ≈ 98 / 165–190 ms | **PF-1: not met (blocker)** |

## Known gaps

- **PF-1 frame rate is a measured blocker.** These measurements are from the physical device:
  - A full-screen render takes about 188 ms in FULL play.
  - The per-layer profile (bench) attributes 41 ms to the FX custom-draw pass (expanding rings drawn with full-circle `lv_draw_arc`, plus particles) and 13 ms to the overlay pass.
  - About 65 alpha-blended, mostly transformed sprite layers make up the rest, roughly 4–10 ms each.
  - Most sprites breathe, bob or fade every frame, as in the reference. At this frame rate 77–85% of the screen is dirty every frame in FULL.

  Changes already tried on the device:

  | Change | Effect |
  |---|---|
  | Batch view invalidation | `sync` 24–28 → 7–14 ms; no more invalid-area overflow into full-screen redraws; START 7.1 → 8.3 fps |
  | 2 draw units | small gain |
  | Pack in PSRAM instead of flash | no gain; reverted |
  | `LV_ATTRIBUTE_FAST_MEM_USE_IRAM` | does not fit: DRAM overflow of 36,696 B |

  Reaching 30 fps needs about 6× less render work. That needs a fidelity or design decision. Options:
  - pre-rendered ring sprites;
  - caching static layers into a pre-composited background;
  - fewer continuously animated sprites on the device;
  - a lower device animation rate.
- PF-7 is not met for the brief internal-heap dip at boot (39.8 KB). Steady state is ≈80 KB.
- No microphone, as in the runnable reference: each recording replays one of four built-in voices exported from
  the reference `renderTake()`. The mic icon is only artwork. There is no mic dependency, voice storage or ESP-SR,
  so SJ-4, SJ-5 and SJ-7 do not apply yet (see GAME_SPEC.md).
- The audio score is a port of the reference's Web Audio graph. It is not bit-identical; see GAME_SPEC.md.
  Sound levels on the device have not been calibrated.
- Sprite animation is limited to the exported frames: the jar mist level uses 33 steps and the swirl 16 phases.
  Layering matches drawScene() except where GAME_SPEC.md says otherwise.
- The touch controller init logs an `i2c.master` pull-up warning. It comes from the board, not this game.
