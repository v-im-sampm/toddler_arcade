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
- `components/core/` — header-only shared contracts (`IGame`, `InputEvent`, `GameContext`) and the
  generated `intent.hpp`. No ESP-IDF dependencies, so it is host-testable.
- `components/board/` — generated `board_pins.hpp`. `kPinsVerified` mirrors `config/board_pins.json`;
  TBD pins are `-1` and drivers must refuse to start on them.
- `main/` — `app_main` skeleton.
- `host_test/` — host CTest suite (host only; proves nothing about the device).
- `partitions.csv` — provisional 16 MB layout: 4 MB factory app, 6 MB `model`, ~5.9 MB `assets`.

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

Known follow-ups:
- LVGL uses its builtin 64 KiB pool in internal `.bss` (`CONFIG_LV_USE_BUILTIN_MALLOC`). P02 should decide
  whether to move LVGL allocations to PSRAM once draw-buffer sizing is measured.
- `model` (6 MB) and `assets` partitions are provisional until V01 and A01 measure real content.
- ESP-IDF v6.x exists; v5.5.6 was chosen because esp-sr 2.6.1 declares `idf >=5.0` and this pairing is now
  build-verified. Re-evaluate only with a new spike.
