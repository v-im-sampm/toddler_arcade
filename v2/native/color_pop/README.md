# Color Pop native (LVGL 9 desktop + ESP32-S3)

Native port of the Color Pop game in [`v2/reference/quality_bar.html`](../../reference/quality_bar.html)
(design: [`v2/games/color_pop.md`](../../games/color_pop.md)). The behavioural contract and the
documented design-vs-reference discrepancies are in [GAME_SPEC.md](GAME_SPEC.md).

| Folder | Contents |
|---|---|
| `src/game/` | Portable, deterministic gameplay (no LVGL/SDL/IDF) |
| `src/audio/` | Port of the reference Web Audio score as an `ExternalScore` (mixed by the shared `AudioFeedback`) |
| `src/ui/` | LVGL 9 sprite view and the `color_pop.pack` loader |
| `src/platform/desktop/` | SDL2 window, mouse-as-touch, desktop audio, offscreen checks |
| `device/` | Standalone ESP-IDF 5.5.6 project for the ESP32-S3 board |
| `tools/` | Canvas artwork exporter, seeded browser-vs-LVGL comparison, scenario scripts |

## Desktop

```powershell
npm ci --prefix tools\quality_bar_golden
cmake -S v2\native -B build-v2-native -G "Visual Studio 17 2022" -A x64
cmake --build build-v2-native --config Release --target color_pop_native color_pop_game_test color_pop_score_test
ctest --test-dir build-v2-native -C Release -R color_pop
.\build-v2-native\color_pop\Release\color_pop_native.exe --zoom 2
node v2\native\color_pop\tools\visual_compare.mjs            # seeded browser vs LVGL (also --seed 77)
```

The asset exporter (`tools/export_assets.mjs`) runs the original Canvas drawing functions in headless
Edge at build time and writes `assets/generated/color_pop.pack` (RGB565A8 sprites, 571,920 B, 216
sprites) plus `assets/manifest.json`. Nothing browser-related runs at game time. Options:
`--seed N`, `--mute`, `--reduced`, `--check`, `--screenshot F.bmp`, `--script F.txt`, `--dump`,
`--verify-redraw`, `--dirty-stats`.

## Device (ESP32-S3, built with wslc, never Docker)

```powershell
pwsh tools\idf_wslc_build.ps1 -Prepare                     # once: image little-arcade/idf:5.5.6
pwsh v2\native\color_pop\device\build.ps1                  # -> device\build\color_pop_sprite.bin + color_pop.pack
pwsh v2\native\color_pop\device\flash.ps1 -Port COM16      # checks ESP32-S3/16 MB first, 460800 baud
```

`build.ps1 -Bench` builds an auto-tapping profiling firmware into `device/build-bench/`
(`flash.ps1 -Bench`). Layout: app at 0x10000, `color_pop.pack` at the `assets` partition (0xA10000).
To monitor: `.venv\Scripts\python.exe -m serial.tools.miniterm COM16 115200`.

Rollback: reflash another firmware with its own `flash.ps1` (for example
`v2\native\bubble_aquarium\device\flash.ps1`), or write a saved full-flash backup with
`.venv\Scripts\python.exe -m esptool --chip esp32s3 -p COM16 -b 460800 write-flash 0x0 <backup.bin>`.

### Measured budgets (physical device, serial logs)

| Item | Measured | Budget |
|---|---|---|
| App binary | 967,840 B | — |
| Sprite pack (flash mmap) | 571,920 B | 900 KiB flash |
| PSRAM in use | ≈16–30 KB | 600 KiB |
| Internal heap free | ≈131–149 KB, minimum ≈65–70 KB | — |
| Audio render | 1.6–2.2 ms per 16 ms block, 0 drops | — |
| Frame interval p95 | idle ≈27 fps; play/celebration 8–15 fps | **CP-5 ≥30 fps: not met** |

## Known gaps

- CP-5 frame rate is not met on the device: a full-screen redraw takes ≈50 ms (the background alone takes ≈33 ms over SPI).
- Host `--verify-redraw` finds 1–4 stale pixels at the crab-leg row (y≈222) in a small share of frames in partial render mode. The cause is unexplained.
- The user tested the device manually. Sound levels have not been calibrated with measurements.
