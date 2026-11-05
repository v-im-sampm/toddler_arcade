# Desktop simulator (LVGL + SDL2)

Runs the full `Arcade` shell (`firmware/components/arcade`) on a desktop PC: the
`SceneManager` launcher, Home, Color Pop, Hatch-a-Pet, Rainbow Piano and Robo Buddy, with the real game logic
and the real LVGL views shared with the device firmware. It is for visual
iteration and render-cost comparison only. **Results from this simulator are
host-desktop measurements, not hardware validation** (AGENTS.md rule 8).

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
| Mouse click | touch (launcher tiles, Home, game areas) |
| `R` `B` `G` `Y` | voice intent RED / BLUE / GREEN / YELLOW |
| `A` / `H` | voice intent AGAIN / HELP |
| `J` / `N` / `X` | voice intent YES / NO / NEXT |
| `Backspace` | voice intent BACK (to the launcher) |
| `C` / `E` / `M` / `T` | voice intent COLORS / EGGS / MUSIC / ROBOT (launch from the menu) |
| Mic button (tap) | does nothing yet; push-to-talk is simulated by the keys above |
| Mic button (hold 5 s) | opens the hidden parent gate (a bar under the Mic fills after 1 s): answer the sum, pick volume 0–6, tap ✓ |
| `Esc` | quit |

Audio cues are printed to the console (`[audio] PROMPT 0`). Like the device, sound **starts muted** until a parent opts in through the parent gate; after that, cues play through the PC speakers with the same synth and sound bank as the device (SDL audio, 16 kHz mono). `--volume N` (0–6) stands in for a level a parent already saved; the sim does not persist settings. `--mute` disables sim audio entirely. Bench mode stays silent.

## Headless benchmark

```powershell
build-sim\Release\little_arcade_sim.exe --bench --game color_pop|hatch_a_pet|rainbow_piano|robo_buddy --seconds 60 --seed 1234 [--frame-ms 33] [--shots DIR]
```

The benchmark uses a deterministic simulated clock. It starts on the launcher,
taps the game's tile at 600 ms, and then a scripted player plays through the
`SceneManager`:

- **Color Pop**: each round taps a wrong bubble, sometimes taps the badge
  (AGAIN), then answers by touch or by voice.
- **Hatch-a-Pet**: taps the egg (badge replay on alternate eggs, one crack by
  voice YES), pets the creature twice, presses NO, then YES by touch or voice.
- **Rainbow Piano**: follows the optional light by tapping the lit key until a
  tune completes. Any key still plays freely; there is no wrong-key response.
- **Robo Buddy**: pets, tickles, feeds, then puts Bolt to sleep and wakes him.

LVGL renders into two partial RGB565 buffers (320×24 lines each, as on the
device). The flush callback copies them into a frame, which `--shots` saves as
BMPs named `<game>_NN_<moment>.bmp`, including `00_menu`.

The report covers:

- render time per frame that actually redrew (avg, p50, p95, max);
- logic and view-sync time (includes Hatch-a-Pet art rasterisation);
- redrawn pixels per frame;
- LVGL heap high-water mark.

It also prints a `JSON {...}` line, which the engine comparison under
`experiments/engine_compare/` is intended to reuse. Exit code 0 means at least one round (Color Pop), one hatch (Hatch-a-Pet), or
one tune (Rainbow Piano) or one reaction (Robo Buddy) completed.
