# Desktop simulator (LVGL + SDL2)

Runs the real `ColorPopGame` logic and `ColorPopView` LVGL view on a desktop PC.
It is for visual iteration and render-cost comparison only. **Results from this
simulator are host-desktop measurements, not hardware validation** (AGENTS.md rule 8).

Pinned dependencies (fetched by CMake, verified by SHA256, not vendored):

| Dependency | Version | Notes |
|---|---|---|
| LVGL | v9.5.0 (85aa60d) | same version as `firmware/main/idf_component.yml` |
| SDL2 | 2.32.10 | static, desktop window/mouse only |

`lv_conf.h` mirrors the device intent: RGB565, 33 ms refresh period and a 128 KB LVGL heap.

## Build (Windows, Visual Studio 2022)

```powershell
cmake -S firmware/sim -B build-sim -G "Visual Studio 17 2022" -A x64
cmake --build build-sim --config Release
```

On other platforms, use any CMake generator with a C++17 compiler.

## Interactive

```powershell
build-sim\Release\little_arcade_sim.exe [--zoom 3] [--seed N]
```

| Input | Meaning |
|---|---|
| Mouse click | touch |
| `R` `B` `G` `Y` | voice intent RED / BLUE / GREEN / YELLOW |
| `A` / `H` | voice intent AGAIN / HELP |
| Home button | restart the scene (SceneManager is not simulated) |
| Mic button | logs only; push-to-talk is simulated by the keys above |
| `Esc` | quit |

Audio cues are printed to the console (`[audio] PROMPT "green!"`) instead of being played.

## Headless benchmark

```powershell
build-sim\Release\little_arcade_sim.exe --bench --seconds 60 --seed 1234 [--frame-ms 33] [--shots DIR]
```

The benchmark uses a deterministic simulated clock and a scripted player. In each
round the player taps a wrong bubble, sometimes taps the badge (AGAIN), and then
answers by touch or by voice. LVGL renders into two partial RGB565 buffers
(320×24 lines each, as planned for the device). The flush callback copies them
into a frame, which `--shots` saves as BMPs.

The report covers:

- render time per frame that actually redrew (avg, p50, p95, max);
- logic and view-sync time;
- redrawn pixels per frame;
- LVGL heap high-water mark.

It also prints a `JSON {...}` line, which the engine comparison under
`experiments/engine_compare/` is intended to reuse. Exit code 0 means at least one round completed.
