# Vendored PixelRoot32

Color Pop runs on the PixelRoot32 2D engine. The engine sources are copied
here so the build has no network fetch and no Arduino or PlatformIO dependency.

| Part | Upstream | Version | Commit | License |
|---|---|---|---|---|
| `engine/` | https://github.com/PixelRoot32-Game-Engine/PixelRoot32-Game-Engine | v1.14.1 | `18a360bf` | MIT (`engine/LICENSE`) |
| `apu/` | https://github.com/gperez88/PixelRoot32-APU | v2.3.0 | `2a6e8a45` | MIT (`apu/LICENSE`) |

## What is copied

- `engine/include/**`: every upstream header, unmodified.
- `engine/src/**`: the subset listed in `CMakeLists.txt` (`LA_PR32_ENGINE_SRCS`), unmodified:
  - core: Engine, Scene, SceneManager
  - graphics: Renderer, Color, DisplayConfig, fonts, DirtyGrid, SpanTable, TextLayout, TileAnimation and TransitionEffect (Engine.cpp references it even with transitions disabled)
  - input: touch dispatcher, queue, manager and state machine
  - audio: AudioEngine, DefaultAudioScheduler, MusicPlayer
  - platform logging and capabilities, GameplayEventBus
- `apu/`: `ApuCore.cpp` and its headers, unmodified.

None of these upstream files is modified. Everything Little-Arcade-specific is
in `port/` or in `components/games/color_pop/pr32/`.

## Port shims (`port/`, ours)

- `Arduino.h` and `arduino_shim.cpp`: `millis()`/`micros()` driven by
  `little_arcade::pr32_port::set_now_ms()`, so engine time is the UI loop's
  clock. Also provides the few other Arduino symbols the subset uses.
- `SPI.h`, `drivers/esp32/TFT_eSPI_Drawer.h`: empty stand-ins for headers that are
  included but not used. No display driver is built.
- `msvc_compat.h`: GCC builtins for the MSVC host build. It is force-included
  only into the engine library.

## Build configuration

`LA_PR32_DEFS` in `CMakeLists.txt` turns modules on and off:

- On: audio, touch, and 4bpp sprites. 4bpp sprites must stay on, because without
  `PIXELROOT32_ENABLE_4BPP_SPRITES` `drawSprite(Sprite4bpp)` compiles to nothing.
- Off: physics, UI, particles, camera effects, transitions, tilemap cache and the
  TFT_eSPI/DAC/I2S drivers.

The arcade supplies these parts itself:

- **Draw surface:** `FrameSurface`, a 320x240 RGB565 surface in PSRAM. LVGL
  shows its frame through one `lv_image`.
- **Touch input:** the touch feed into `TouchEventDispatcher`.
- **Audio:** pulled from `AudioEngine::generateSamples()` by AudioFeedback's
  scene-audio hook on the audio task, so it passes through the existing volume
  ramp and VolumeLimiter.

Engine warnings are silenced (`-w` / `/w`), and the engine headers are SYSTEM
includes on the host.

## Updating

1. Clone the new tags of both repositories.
2. Copy `include/**` and the listed `src/` files over `engine/`, and copy the APU
   files over `apu/`. Do not edit them.
3. Update the table above.
4. Run the host tests (`color_pop_pr32_test` covers rendering, touch and APU
   output) and `pwsh tools/idf_wslc_build.ps1`, and record the flash and PSRAM
   deltas.

Rollback: revert the commit that added this directory. That commit also removes
Color Pop's runtime under `components/games/color_pop/pr32/`.
