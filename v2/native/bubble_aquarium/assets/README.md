# Canvas sprite export

The [source sketch](../../../reference/bubble_aquarium.html) contains **all**
original artwork and no imported images. [export_assets.mjs](../tools/export_assets.mjs)
uses its actual drawing functions inside headless Edge to produce the
[generated/](generated/) PNG previews and equally sized BGRA8888 pixel files.
The [manifest](generated/manifest.json) records every asset's ID, original
stage-space crop bounds, dimensions and uncompressed bytes.

Backdrop, full-screen start overlay and chrome are static. Fish are exported
in six tail/fin positions per face (left/right) and mood
(normal/happy/sleep); visitors, back/front seaweed, surface, rays, caustics
and buttons each have six frames. The chest has four lid angles. Bubbles,
pellets, the helper hand and fish shadows are transparent sprites. The LVGL
view layers and chooses frames at runtime, but never invokes JavaScript or
repaints Canvas paths.

BGRA8888 is little-endian LVGL ARGB8888 image memory order. PNGs are for
inspection; the game loads raw frames, validates their lengths, and retains
them in desktop RAM to avoid per-frame PNG decode cost. For an embedded
version, keep PNG previews as provenance, convert to indexed/RGB565+alpha
or other measured flash format, and cache only live frames. Pixel-level
fidelity and flash/PSRAM usage must be measured **on physical hardware**;
this build supplies no ESP32 firmware or device measurements.
