# Bubble Aquarium native desktop (LVGL 9 + SDL2)

This is a **Windows desktop application**, not firmware, web rendering, or
hardware validation. It builds from the v2 native CMake project with pinned
LVGL **v9.5.0** and SDL2 **2.32.10** (CMake download hashes are specified in
[`../CMakeLists.txt`](../CMakeLists.txt)). All v2 gameplay lives in
`src/game/` without SDL, LVGL, or ESP-IDF dependencies. `src/ui/` loads
the original Canvas artwork exported as PNG plus native BGRA8888 image data
and presents it with LVGL 9 image objects and frame selection;
`src/platform/desktop/` owns SDL mouse, clock, window, audio and BMP output.
No ESP32 drivers or toolchain are involved.

From the repository root in **PowerShell** with Visual Studio 2022 C++ tools:

```powershell
npm ci --prefix tools\quality_bar_golden
cmake -S v2\native -B build-v2-native -G "Visual Studio 17 2022" -A x64
cmake --build build-v2-native --config Release --target bubble_aquarium_native aquarium_game_test
.\build-v2-native\bubble_aquarium\Release\bubble_aquarium_native.exe --zoom 2
```

Node.js and Microsoft Edge are required **at build time only** to export the
original browser artwork. CMake downloads pinned LVGL/SDL2 on a clean machine;
the executable itself uses neither Node.js nor Edge.

The `--zoom 2` window is 640 x 480 physical pixels; pointer coordinates remain
320 x 240 logical pixels. `--zoom 1..4`, `--seed N`, and `--mute` are optional.
Click the central play button; click/drag the shaker to feed fish, the bubble
button/chest to release bubbles, bubbles to pop, fish or empty glass to attract
them, and the moon/sun to toggle bedtime. Home restarts. `B` toggles bedtime,
`R` reduces the happy loop, `Esc` exits. Audio is capped at a low gain, starts
silent until play is clicked, and is never recorded or saved. The mic icon is
disabled as in the browser sketch.

Host tests and an offscreen LVGL screenshot (no display necessary):

```powershell
.\build-v2-native\bubble_aquarium\Release\aquarium_game_test.exe
.\build-v2-native\bubble_aquarium\Release\bubble_aquarium_native.exe --check --screenshot .\build-v2-native\bubble_aquarium\aquarium_lvgl.bmp
```

The second command verifies an intro, touch-fed meal, bubble burst and bedtime
and writes the rendered 320 x 240 RGB565 screen to a BMP. Optional GUI checks
require an interactive desktop; the headless check does not prove a window can
open. This desktop result does not measure ESP32 flash/PSRAM. The portable core
uses fixed-capacity arrays (3 fish, 6 pellets, 8 bubbles); desktop sprite
buffers and framebuffer are allocated in host memory.

## Asset pipeline and known differences

The reference has **no imported assets**: all artwork is Canvas 2D and audio
is synthesized by Web Audio. `tools/export_assets.mjs` **runs the original
Canvas drawing functions in headless Edge during the build**; it exports a
static backdrop and 227 separate transparent sprite frames to
`assets/generated/` as PNG previews plus raw BGRA8888. Fish have six tail
positions, left/right, normal/happy/sleep expressions; guests, seaweed,
light rays, surface, caustics, chest and buttons have separate frames. Build
copies generated assets beside the exe; **no browser or JS runs at game
runtime**. No artwork is manually redrawn, nor compiled as a C array.
From the repository root, to regenerate independently:

```powershell
node v2\native\bubble_aquarium\tools\export_assets.mjs
node v2\native\bubble_aquarium\tools\visual_compare.mjs
```

The visual script drives an equivalent seeded intro/feed/bubbles/bedtime
scenario in the browser and the LVGL exe, writes side-by-side previews and a
JSON diff to `build-v2-native\bubble_aquarium\comparison\`, and fails above
14 mean RGB absolute error or 18% pixels differing by more than 32. The
current host result is 9.9 / 11.78% after RGB565 quantization. The original
procedural fish geometry, gradients, facial expressions and coral are
preserved. The current sprite implementation does not animate individual
eye gaze, blink, micro-particles, fish turning squash, or every intermediate
chest lid position or the moon/sun crossfade; those remain differences from
browser Canvas. The desktop sound backend
plays conservative pentatonic sine cues and an 8-note 70/60 BPM bed, **not**
the browser's richer multivoice/noise/filtered reef audio graph. There is no
spoken hint or sound storage. Reduced-motion currently removes the happy loop,
but does not disable every small sprite bob. The raw desktop BGRA frame set is
approximately 7 MB; an embedded port should quantize and stream selected frames
from flash, not preload all frames into PSRAM.

Acceptance evidence: host assertions cover seeded replay, food/bubble caps,
three-meal visitor, bedtime, reset; the offscreen check covers LVGL rendering.
Rollback is removing the Bubble Aquarium subdirectory from
[`../CMakeLists.txt`](../CMakeLists.txt) and this isolated directory.
