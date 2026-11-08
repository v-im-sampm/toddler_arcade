# Hatch-a-Pet native spike

This is an isolated ESP32-S3 device image and LVGL 9/SDL2 desktop game; it does not replace the
launcher or modify another game's runtime. [GAME_SPEC.md](GAME_SPEC.md) records the source-of-truth
rules, reference discrepancies and known fidelity gaps. Source art is captured at build time from
[`../../reference/hatch_a_pet.html`](../../reference/hatch_a_pet.html), not recreated by hand.
The installed LVGL is 9.5.0, ESP-IDF is 5.5.6; device pins and drivers come only from the
repository's documented `firmware/components/board` component.

## Host build / deterministic and visual tests (PowerShell from repo root)

```powershell
cmake -S v2\native -B build-v2-native -G "Visual Studio 17 2022" -A x64
cmake --build build-v2-native --config Release --target hatch_a_pet_native hatch_a_pet_game_test
ctest --test-dir build-v2-native -C Release -R hatch_a_pet --output-on-failure
node v2\native\hatch_a_pet\tools\visual_compare.mjs
.\build-v2-native\hatch_a_pet\Release\hatch_a_pet_native.exe --zoom 2
.\build-v2-native\hatch_a_pet\Release\hatch_a_pet_native.exe --check --scene choice --screenshot choice.bmp
```

`--mute`, `--reduced` and `--seed N` are also supported. Mouse-down is touch, and mouse release
ends the contact. No reference drag actions exist. The executable loads its copied `hatch_a_pet.pack`
from its own directory; no browser, JS, or HTML is used at runtime. The exporter itself requires
Node.js, Microsoft Edge and the repository's existing `playwright-core` installation.

## Device build, verification, flash, monitor (PowerShell from repo root)

```powershell
pwsh v2\native\hatch_a_pet\device\build.ps1
# Verify COM16 belongs to the intended 16 MB ESP32-S3 before any write.
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 -p COM16 flash-id
# If and only if the identity and build are verified:
pwsh v2\native\hatch_a_pet\device\flash.ps1 -Port COM16
.\.venv\Scripts\python.exe -m serial.tools.miniterm COM16 115200
```

The flash script verifies the chip and 16 MB flash, checks the factory/assets partition bounds,
then writes bootloader, partition table, app and the matching signed sprite pack together. The
firmware refuses a missing or mismatched asset signature. The build uses `wslc` and never Docker.
The isolated app is not integrated into the multi-game launcher. If COM16 cannot be verified,
**do not flash**, and do not infer physical display, speaker, or touch behavior from host screenshots.

## Rollback

Before an authorized device flash, save the existing 16 MB image (private backup, never commit it):

```powershell
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 -p COM16 read-flash 0x0 0x1000000 previous-board.bin
```

To restore it, after verifying the same board/port:

```powershell
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 -p COM16 `
  write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x0 previous-board.bin
```

`previous-board.bin` may contain settings or child data from previous images: keep it private,
delete it when no longer needed, and never upload or commit it. The game itself has no storage,
network, voice recording, or telemetry. The mic drawing is disabled by the reference.

## Evidence and budget gates

The signed sprite pack and app sizes are printed by their respective build steps. Asset budget
is ≤1.2 MiB (pack); sprite PSRAM is zero (flash-mapped); the design's ≤900 KiB PSRAM, device
30 FPS p95 hatch, heap minimum, audio CPU and speaker SPL require physical-device measurements.
A successful boot log alone cannot establish screen appearance, calibrated audio, or playable touch.
Do not treat host/offscreen metrics as device evidence.

## Current validation (host / web / cross-build / ESP32-device boot)

The owner accepted Hatch-a-Pet as tested and done on 2026-11-08. The evidence below retains
its original tiers; owner acceptance does not turn unrecorded physical interactions into
measured touch, display, hatch-frame or acoustic results.

- Host: all three Hatch CTest targets pass (deterministic rules, offscreen LVGL touch/render,
  nine-scene seeded browser comparison). The Edge reference and LVGL captures are in
  `assets/generated/browser_*.png` and `assets/generated/lvgl_*.png`; differences are bounded
  in `tools/visual_compare.mjs`. A GUI LVGL window was opened; Windows PrintWindow captured a
  black SDL GPU surface, so only the offscreen LVGL screenshot is visual evidence.
- Cross-build: ESP-IDF 5.5.6 / LVGL 9.5.0 image built via `wslc`; app 954,736 bytes,
  signed sprite pack 1,208,368 bytes (1.152 MiB, under 1.2 MiB asset budget),
  combined app + pack 2,163,104 bytes. The app partition has about 77% free. Sprite
  storage is flash-mapped, not copied into PSRAM. Link-time DIRAM usage and app-section
  detail are in `device/build/size.txt`, but this is not runtime free-heap/PSRAM evidence.
- ESP32-device (2026-11-08, ESP32-S3 rev v0.2, 16 MB flash / 8 MB PSRAM, COM16):
  `esptool flash-id` verified identity; `device/flash.ps1 -Port COM16` wrote bootloader,
  partition table, 954,736-byte app and 1,208,368-byte matching pack, each with a verified
  write hash. A serial capture after reset showed IDF 5.5.6 boot, `pack=1208368 bytes
  sprites=94`, ST7789 initialization, NS2009 probe `ESP_OK`, and audio I2S initialization.
  At first frame, internal free/min heap was 156,527/91,796 bytes, PSRAM free/min was
  8,380,004/8,380,004 bytes. After ten seconds idle, internal free/min was 142,655/77,924
  bytes and PSRAM free/min 8,379,492/8,379,492 bytes. Idle frame-window maximum render
  was 13,431 us and maximum interval 21 ms; audio reported render maximum 283 us with
  zero cues and zero output peak. These are boot/idle measurements **only**: there is no
  measured hatch p95 FPS, sound calibration/SPL, observed screen appearance or physically
  exercised touch gameplay. The 1.2 MiB game-art budget is met by the sprite pack alone;
  app plus artwork exceeds 1.2 MiB if that budget was intended as total program footprint.
