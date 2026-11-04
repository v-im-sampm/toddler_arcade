# Little Arcade firmware (ESP-IDF)

**Status: P01 build skeleton. Compiles; has not been flashed or run on a physical board.**
The app boots (expected), logs build/board/memory facts and idles. It does not drive any GPIO:
display/touch bring-up waits on H02 and audio on H03 (see `docs/hardware.md`).

## Pinned toolchain and dependencies (P01 spike)
| Item | Version | Where pinned |
|---|---|---|
| ESP-IDF | v5.5.6 (container `espressif/idf:v5.5.6`) | `main/idf_component.yml`, `tools/idf_docker_build.ps1` |
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
- `components/games/<name>/`: one component per game, holding scene logic, a README and host tests.
  - The IDF project and the host tests discover these folders automatically.
  - Each folder is owned by its game; don't edit across games without review.
- `components/board/`: generated `board_pins.hpp`.
  - `kPinsVerified` mirrors `config/board_pins.json`.
  - TBD pins are `-1`, and drivers must refuse to start on them.
- `main/`: the `app_main` skeleton. It does not yet link core, speech, audio or games. That wiring is I01, and is blocked on H02/H03.
- `host_test/`: host CTest suite. Host only; it proves nothing about the device.
- `partitions.csv`: provisional 16 MB layout with a 4 MB factory app, 6 MB `model` and about 5.9 MB `assets`.

Generated headers come from `config/*.json`; never edit them by hand:
```
python tools/gen_firmware_headers.py          # regenerate
python tools/gen_firmware_headers.py --check  # CI drift check (also run by tools/validate_specs.py)
```

## Build
ESP-IDF build in the pinned container (no local IDF install needed), plus size reports and GCC host tests:
```
pwsh tools/idf_docker_build.ps1          # add -Clean for a from-scratch build
```
Host tests with a local compiler:
```
cmake -S firmware/host_test -B build-host
cmake --build build-host --config Debug
ctest --test-dir build-host -C Debug --output-on-failure
```
To build a single game in isolation, pass `-DLA_ONLY_GAME=<folder>`, e.g. `-DLA_ONLY_GAME=color_pop`.
Flashing (`idf.py -p PORT flash monitor`) is untested. Do not flash until H02 confirms the module's
flash/PSRAM variant; `CONFIG_SPIRAM_MODE_OCT` assumes an N16R8 module.

## P01 build evidence (container build only — not run on hardware)
Environment: `espressif/idf:v5.5.6` (digest `sha256:bf72fe7a15c2…`), xtensa-esp-elf GCC 14.2.0, Docker on Windows.
Clean build reproducible; zero compiler warnings.

| Measure | Value |
|---|---|
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

Known follow-ups:
- LVGL uses its builtin 64 KiB pool in internal `.bss` (`CONFIG_LV_USE_BUILTIN_MALLOC`). P02 should decide
  whether to move LVGL allocations to PSRAM once draw-buffer sizing is measured.
- `model` (6 MB) and `assets` partitions are provisional until V01 and A01 measure real content.
- ESP-IDF v6.x exists; v5.5.6 was chosen because esp-sr 2.6.1 declares `idf >=5.0` and this pairing is now
  build-verified. Re-evaluate only with a new spike.
