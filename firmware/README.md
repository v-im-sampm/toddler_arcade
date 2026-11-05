# Little Arcade firmware (ESP-IDF)

**Status: H02 display + touch bring-up flashed and run on one MaTouch Lite board (N16R8 module).**
`app_main` starts the ST7789 display, the NS2009 touch controller and an LVGL UI task. It does not drive
audio yet; that waits on H03 (see `docs/hardware.md`).

## Pinned toolchain and dependencies (P01 spike)

| Item | Version | Where pinned |
| --- | --- | --- |
| ESP-IDF | v5.5.6 (container `espressif/idf:v5.5.6`) | `main/idf_component.yml`, `tools/idf_wslc_build.ps1` |
| LVGL | 9.5.0 | `main/idf_component.yml` |
| esp_lvgl_port | 2.9.0 | `main/idf_component.yml` |
| esp-sr | 2.6.1 (resolved, not yet called; V01 decides) | `main/idf_component.yml` |

`dependencies.lock` records the exact resolved graph. Change versions only with a new build + size report.

## Layout

- `components/core/`: shared contracts with no ESP-IDF dependencies, so it is host-testable.
  - `IGame` and `GameContext`. These are FROZEN for game work.
  - `InputEvent`, `Rng`, `IFeedback` cues, and layout constants in `geometry.hpp`.
  - The generated `intent.hpp`.
  - `SceneManager`, which handles launcher paging, Home/BACK, pause, debounce, idle sleep and voice context filtering.
- `components/speech/`: push-to-talk session timing and `VoiceIntentFilter`. Pure logic; ESP-SR is not called yet.
- `components/audio/`: `VolumeLimiter`, which provides muted startup, a parent cap, ramping and a hard peak limit. Pure logic; there is no I2S driver yet.
- `components/settings/`: `ParentGate`, the hidden parent settings gate (pure logic, host-tested), plus an NVS store on the device that persists only the volume level (namespace `la_settings`, key `volume`).
- `components/games/<name>/`: one component per game, holding scene logic, a README and host tests.
  - The IDF project and the host tests discover these folders automatically.
  - Each folder is owned by its game; don't edit across games without review.
- `components/board/`: generated `board_pins.hpp`.
  - `kPinsVerified` mirrors `config/board_pins.json`.
  - TBD pins are `-1`, and drivers must refuse to start on them.
- `components/board/`: also `board_io` (ST7789 over SPI, NS2009 over I2C) and `touch_calibration.hpp` (raw-to-screen mapping, host-tested).
- `main/`: `app_main` and the LVGL UI task (display flush, touch indev, perf log). Speech and audio are not linked yet; the full wiring is I01.
- `host_test/`: host CTest suite. Host only; it proves nothing about the device.
- `partitions.csv`: provisional 16 MB layout with a 4 MB factory app, 6 MB `model` and about 5.9 MB `assets`.

Generated headers come from `config/*.json`; never edit them by hand:

```
python tools/gen_firmware_headers.py          # regenerate
python tools/gen_firmware_headers.py --check  # CI drift check (also run by tools/validate_specs.py)
```

## Build

ESP-IDF build in the pinned container via WSL containers (`wslc`; do not use Docker), with no local IDF install needed, plus size reports and GCC host tests:

```
pwsh tools/idf_wslc_build.ps1          # add -Clean for a from-scratch build
```

Host tests with a local compiler:

```
cmake -S firmware/host_test -B build-host
cmake --build build-host --config Debug
ctest --test-dir build-host -C Debug --output-on-failure
```

To build a single game in isolation, pass `-DLA_ONLY_GAME=<folder>`, e.g. `-DLA_ONLY_GAME=color_pop`.
Flash with esptool from `firmware/build` (the board used for H02 enumerates as a USB serial port, e.g. `COM15`):

```
python -m esptool --chip esp32s3 -p COM15 -b 921600 --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m 0x0 bootloader/bootloader.bin \
  0x8000 partition_table/partition-table.bin 0x10000 little_arcade.bin 0x410000 srmodels/srmodels.bin
```

`CONFIG_SPIRAM_MODE_OCT` assumes an N16R8 module; the H02 board reported 16 MB flash and 8 MB octal PSRAM.

## Sound and parent settings

Sound is **muted from the factory** and stays muted until a parent opts in (`docs/acceptance.md` G4, `docs/parental-safety.md`):

1. Hold the Mic corner (top-right) for 5 seconds. A short tap on the Mic corner does nothing to the games.
2. Answer the sum by tapping the right number. A wrong answer, or 30 s with no touch, closes the screen.
3. Pick a volume from 0 (off) to 6 (the parent cap). Each non-zero tap plays a short preview chime.
4. Tap ✓.

The level is saved in NVS and applied after the first frame on later boots, ramping up over 300 ms, so there is no startup sound. To restore the factory default, erase NVS:

```
python -m esptool --chip esp32s3 -p COM15 erase_region 0x9000 0x6000
```

## H02 device evidence (one physical board)

Image: `little_arcade.bin` 605,065 B, built with `tools/idf_wslc_build.ps1`. It had zero warnings and 17/17 host tests passed.
It was flashed over USB at 921600 baud and all esptool hashes were verified. The boot log showed:

- `esp_psram: Found 8MB PSRAM device` and `flash=16777216 bytes`.
- `ST7789 up: SPI 27000000 Hz, BGR, inverted, swap=1 mirror=1,0`.
- `NS2009 probe at 0x48: ESP_OK`.
- Memory at UI ready:
  - Internal heap free 243,511 B, minimum 178,780 B, largest block 131,072 B.
  - PSRAM free 8,385,768 B.
  - `lv_mem` max_used 13.9 KB.
  - UI task stack 11.7 KB free.
- Color Pop gameplay over 10 s:
  - 134 redraws and 4 rounds.
  - Render plus flush averaged 17.3 ms (max 42.1 ms), with about 4.9 K px per redraw.

The owner played Color Pop on the board and reported the picture correct and touch accurate.

Still not verified:

- A measured 5-point touch calibration. The current values are x 200–3850, y 400–3920, Z > 150.
- Backlight level.
- The PCB revision.
- Touch-to-pixel latency.

**Rollback:** reflash a previous `little_arcade.bin` at 0x10000, or restore the full factory dump with `write_flash 0x0 <backup>.bin`.

## P01 build evidence (container build only — not run on hardware)

Environment: `espressif/idf:v5.5.6` (digest `sha256:bf72fe7a15c2…`), xtensa-esp-elf GCC 14.2.0. These numbers were recorded
before the switch to WSL containers; rebuilds now use `tools/idf_wslc_build.ps1` with the same image.
Clean build reproducible; zero compiler warnings.

| Measure | Value |
| --- | --- |
| `little_arcade.bin` | 353,104 B (0x56350); 92% of 4 MB factory partition free |
| Bootloader | 0x5590 B; 33% free |
| Static DIRAM | 128,795 B used / 341,760 B (37.7%); `.bss` 68,456 B |
| Flash code / rodata | 219,322 B / 56,652 B |
| `srmodels.bin` | 4 B — esp-sr default selects **no** WakeNet/MultiNet model; model size unmeasured (V01) |
| Host tests | GCC (container) and MSVC `/W4 /WX`: 1/1 passed |

Runtime heap/PSRAM numbers do not exist yet: `app_main` logs them, but the image has never run on a board.

### Host-ahead re-check (after P02-core / V02-logic / P03-logic)

- The container build is clean, with zero warnings.
- `little_arcade.bin` is 352,985 B. It's essentially unchanged because `main` doesn't link the new components yet.
- The container GCC host run passed 5/5 suites: core_contract, scene_manager, voice_policy, volume and color_pop.
- MSVC `/W4 /WX /permissive-` passes the same suites.

These are build and logic results only. Footprint deltas for scenes and views have to be measured once I01 links them.

### All ten game scenes (G01–G10, host-ahead)

- The container build is clean, with zero warnings. All 10 game components compile for the ESP32-S3 with xtensa GCC: `libcolor_pop.a` … `libbubble_aquarium.a`, 45–105 KB archives including debug info.
- `little_arcade.bin` is unchanged at 352,985 B because `main` doesn't link any game until I01.
- Host suites: 14/14 pass with the container GCC, and with MSVC `/W4 /WX /permissive-` in Debug and Release.
- Gaps each game README flags, where voice falls back to touch only:
  - There are no intents for animal names, shapes, planets, feed, sleep or pet.
  - Shape Garage drag isn't possible, because `InputEvent` has no press/move/release phases.
  - Any new intent must be added to `config/commands.json` and reviewed. It also grows the V01 grammar.

Known follow-ups:

- LVGL uses its builtin 64 KiB pool in internal `.bss` (`CONFIG_LV_USE_BUILTIN_MALLOC`). P02 should decide
  whether to move LVGL allocations to PSRAM once draw-buffer sizing is measured.
- `model` (6 MB) and `assets` partitions are provisional until V01 and A01 measure real content.
- ESP-IDF v6.x exists; v5.5.6 was chosen because esp-sr 2.6.1 declares `idf >=5.0` and this pairing is now
  build-verified. Re-evaluate only with a new spike.
