# Little Arcade v2

The active product specification and game designs are in [`v2/README.md`](v2/README.md).
Start with [`AGENTS.md`](AGENTS.md), then follow the reading order in the v2 README.

## Current code

- [`v2/native/`](v2/native/) contains the experimental native Bubble Aquarium host and ESP32-S3
  implementation.
- [`firmware/components/`](firmware/components/) retains only shared ESP32-S3 support used by the
  v2 device build: board I/O, audio, settings, and their core contracts.
- The previous v1 launcher, game implementations, simulator, host-test harness, and PixelRoot32
  engine have been removed. The twelve v2 game specifications remain in [`v2/games/`](v2/games/).
- Root-level `docs/`, `games/`, and `backlog/` files that remain are historical v1 references; use
  their v2 counterparts as the active specifications.
- Historical v1 measurements and lessons are summarized in
  [`v2/docs/01_v1_baseline.md`](v2/docs/01_v1_baseline.md). Hardware evidence and configuration
  remain in [`config/`](config/) and [`docs/hardware.md`](docs/hardware.md).

## Host build and tests

With CMake, a C++17 compiler, Node.js, and Microsoft Edge installed:

```powershell
cmake -S v2/native -B build-v2-native -G "Visual Studio 17 2022" -A x64
cmake --build build-v2-native --config Release --target bubble_aquarium_native aquarium_game_test
ctest --test-dir build-v2-native -C Release --output-on-failure
```

The CMake project downloads pinned LVGL and SDL2 for development. The game executable does not use
Node.js or a browser at runtime.

## ESP32-S3 build

Build the standalone Bubble Aquarium device image using WSL containers (`wslc`):

```powershell
pwsh v2\native\bubble_aquarium\device\build.ps1
```

See [`v2/native/bubble_aquarium/device/README.md`](v2/native/bubble_aquarium/device/README.md) for
device scope, flashing instructions, and the limits of the recorded hardware evidence.
