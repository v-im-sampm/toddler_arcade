# Retained ESP-IDF support components

The former v1 firmware image, launcher, game implementations, simulator, and
host-test harness have been removed. This directory now contains only the
shared ESP-IDF support used by the current v2 Bubble Aquarium device build:

- `components/board/`: ST7789 display and NS2009 touch drivers, using generated
  board configuration.
- `components/audio/`: capped synth, amp-safe volume limiting, audio feedback,
  I2S output, and reusable audio utilities.
- `components/core/`: shared contracts required by the retained audio support.
- `components/settings/`: parent volume persistence.
- `partitions.csv` and `sdkconfig.defaults`: device build settings consumed by
  `v2/native/bubble_aquarium/device/`.

The device build entry point is
[`v2/native/bubble_aquarium/device/build.ps1`](../v2/native/bubble_aquarium/device/build.ps1).
Build and flash evidence is documented in that game's device README; hardware
facts and measured v1 lessons are summarized in
[`v2/docs/01_v1_baseline.md`](../v2/docs/01_v1_baseline.md).

Generated headers come from the root `config/*.json` files. Regenerate or
check them with:

```powershell
python tools/gen_firmware_headers.py
python tools/gen_firmware_headers.py --check
```
