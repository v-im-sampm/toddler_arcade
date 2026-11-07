# Standalone ESP32-S3 Bubble Aquarium sprite build

This is a **single-game device image**. Flashing it replaces the currently
installed launcher firmware until that firmware is reflashed. The device port
uses the original Canvas-exported sprite frames, not the older procedural
Aquarium implementation in `firmware/components/games/bubble_aquarium/`.

The [desktop game logic](../src/game/aquarium.cpp) and
[LVGL image scene](../src/ui/aquarium_view.cpp) are compiled unchanged except
for an ESP-IDF asset reader. The board's existing ST7789 and NS2009 drivers
provide display and continuous touch events. LVGL renders 320x240 RGB565
using two 320x24 DMA buffers; source artwork is BGRA8888 in PSRAM. The build
script uses **WSL containers (`wslc`)** and pinned ESP-IDF 5.5.6/LVGL 9.5.0:

```powershell
pwsh v2\native\bubble_aquarium\device\build.ps1
pwsh v2\native\bubble_aquarium\device\flash.ps1 -Port COM16
& .\.venv\Scripts\python.exe -m serial.tools.miniterm COM16 115200
```

Exit miniterm with Ctrl+]. Flash writes bootloader at 0x0, partition table
at 0x8000, app at 0x10000 and **asset image at 0xA10000**. It does not erase
NVS: the existing parent's saved volume is reused (0 = mute). The local
audio backend uses the existing capped synth and amp-safe ceiling, with no
mic capture. The desktop's original 70/60 BPM music is not yet on device.

`tools/pack_device_assets.mjs` selects every other phase from the full
Canvas export (120 unique frames, 3.71 MiB), below the 5.94 MiB assets
partition. Aliased frames still provide moving tails, visitors, seaweed,
light rays and surface motion, but at half their desktop frame count. The
build loads 3.71 MiB of source pixels into PSRAM once at boot; it fails
loudly on missing partitions, allocation failure, or invalid assets. Before
flashing, verify the host tests and visual comparison in the parent README.
Serial logs report actual device heap/PSRAM, frames/10s and UI stack space;
these measurements are mandatory before describing this as validated on a
physical board. Board pin and calibration evidence remain in
`firmware/README.md` and `docs/hardware.md`.

## COM16 deployment evidence (host date 2026-11-06)

- ESP-IDF 5.5.6 build completed; the factory app is **940,224 bytes** of
  4,194,304 bytes, and the sprite image is **3,891,040 bytes** of
  6,225,920 bytes. All four flash segments passed esptool hash verification.
- COM16 reported an ESP32-S3 with 16 MB flash and 8 MB PSRAM. After the
  assets loaded, free internal heap was **138,263 bytes** and free PSRAM was
  **4,479,624 bytes**. At the 10-second boot log, internal heap was 124,095
  bytes, PSRAM 4,479,112 bytes, and UI task stack headroom 20,352 bytes.
- The game reached `ready` and logged ~99 start-screen redraws per ten
  seconds without a reported watchdog reset, exception or allocation error.
  This verifies boot and memory, **not** screen appearance, sound quality,
  touch gameplay, FPS while feeding, or calibrated SPL. Those still need a
  person to play on the board; a desktop screenshot cannot establish them.

Rollback: rebuild and flash the original launcher using
`pwsh tools/idf_wslc_build.ps1` and
`pwsh tools/flash_firmware.ps1 -Port COM16`. The original flash script
does not touch the sprites partition; this is harmless to the launcher.
