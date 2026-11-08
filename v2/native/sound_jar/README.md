# Sound Jar native (LVGL 9 desktop + ESP32-S3)

Native, sprite-based port of [`v2/reference/sound_jar.html`](../../reference/sound_jar.html)
(design: [`v2/games/sound_jar.md`](../../games/sound_jar.md)). [GAME_SPEC.md](GAME_SPEC.md) describes how the game
must behave: update model, states, timers, seeded takes, hit areas, audio cues and visuals. It also lists where the
design and the runnable reference disagree; in those cases the port follows the reference.

| Folder | Contents |
|---|---|
| `src/game/` | Portable, deterministic gameplay (no LVGL/SDL/IDF); the variable-`dt` `frame()` model of the reference |
| `src/audio/` | Port of the reference Web Audio score (built-in sample takes, effects) as an `ExternalScore` |
| `src/ui/` | LVGL 9 sprite view, `sound_jar.pack` loader, render-cost profiler |
| `src/platform/desktop/` | SDL2 window, mouse-as-touch (including drag), desktop audio, offscreen checks |
| `device/` | Standalone ESP-IDF 5.5.6 project for the ESP32-S3 board (LVGL 9.5.0) |
| `tools/` | Canvas artwork exporter, seeded browser-vs-LVGL comparison, scenario scripts |

## Desktop (host)

```powershell
npm ci --prefix tools\quality_bar_golden                      # headless Edge driver used by the exporter/compare
cmake -S v2\native -B build-v2-native -G "Visual Studio 17 2022" -A x64
cmake --build build-v2-native --config Release --target sound_jar_native sound_jar_game_test sound_jar_score_test
ctest --test-dir build-v2-native -C Release -R sound_jar --output-on-failure
.\build-v2-native\sound_jar\Release\sound_jar_native.exe --zoom 2          # window; mouse = touch
node v2\native\sound_jar\tools\visual_compare.mjs                         # seeded browser vs LVGL, 14 scenarios
```

The exporter (`tools/export_assets.mjs`) runs the reference's own Canvas drawing functions in headless Edge at build
time. It writes `assets/generated/sound_jar.pack`: 1,886 sprites in RGB565, RGB565A8 or A8, 2,286,899 B in total.
It also writes [assets/manifest.json](assets/manifest.json), which records the source function, crop, origin, frame
timing, format and bytes of every sprite. The finished game needs no browser.

The game also runs offscreen with `--script tools\scenarios\NN.txt` combined with one of:
- `--screenshot F.bmp`
- `--dump`
- `--verify-redraw [--direct]`: compares each incremental frame against a full render
- `--dirty-stats`
- `--profile`: per-layer full-screen render cost

Other options are `--seed N`, `--mute`, `--reduced` and `--check`. Use `--window --script F --exit-after N --screenshot F.bmp` to run the real SDL window and save
its framebuffer.

## Device (ESP32-S3, built with wslc, never Docker)

```powershell
pwsh tools\idf_wslc_build.ps1 -Prepare                     # once: image little-arcade/idf:5.5.6
pwsh v2\native\sound_jar\device\build.ps1                  # -> device\build\sound_jar_sprite.bin + sound_jar.pack
pwsh v2\native\sound_jar\device\flash.ps1 -Port COM16      # checks ESP32-S3 / 16 MB first; 460800 baud
.venv\Scripts\python.exe -m serial.tools.miniterm COM16 115200   # monitor
```

Flash layout:
- bootloader at 0x0;
- partition table at 0x8000;
- app at 0x10000 (4 MiB factory);
- `sound_jar.pack` at the `assets` partition, 0xA10000 (0x5F0000).

At boot the firmware checks the pack signature and refuses to run with a stale pack. The pack is memory-mapped from flash and used in place.

`build.ps1 -Bench` (and `flash.ps1 -Bench`) builds a profiling image into `device/build-bench/`. It plays the
reference `autoTarget()` on its own (record → pads, holds and slides → effects) and once logs a per-layer render
profile.

`device/sdkconfig.defaults` holds this game's overrides to the shared `firmware/sdkconfig.defaults`: the FreeRTOS LVGL OS layer and 2 software draw units.
Delete `device/sdkconfig` after changing either defaults file.

### Microphone and privacy (device)

The device records the child's real voice from the INMP441 mic:
- data GPIO6, sharing BCLK GPIO4 and WS GPIO5 with the speaker;
- the board facts are in `firmware/components/board/include/little_arcade/board_pins.hpp` (`kMicDataIn`).

It records only when **both** of these hold:
- the mic hardware started;
- the parent mic switch `settings::load_mic_enabled()` is on (default on).

Otherwise every recording uses the built-in voices, exactly like the desktop and the reference. The boot log line
`[sound_jar] mic switch=... hardware=... -> recordings use ...` shows which mode is active.

Recording with the mic (native, user-requested after physical testing):
- Tapping the mic opens the lid and records at once. There is no countdown, wire or lid-pop sound, and the jar fills
  as you talk.
- Recording stops 0.5 s after you stop talking, on a second tap, or at 3 s. Playback is trimmed to the sound.
- If a take is too quiet, the jar stays empty and silent and the helper hand points at the mic until the next try.
- Built-in recordings (desktop, no mic, or parent switch off) keep the reference's countdown.

- Input gain: a fixed ×16 (+24 dB) on the device (`LiveTake::set_input_gain`). Raw INMP441 speech was below the
  reference's LOUD threshold.
- Silence is judged relative to the room: 10× the take's quietest 20 ms frame, between ACTIVE and LOUD. The first
  100 ms after the tap is discarded.
- The clip is at most 3 s, 16 kHz int16, 96,000 B in PSRAM. It is never written to flash, sent anywhere or logged.
- It is zeroed on Home, the bin, a new recording, a too-quiet take and any processing failure.
- The speaker is silenced while capturing.
- Serial logs print only aggregates every 10 s: `[mic] live=... state=... takes=... jar_live=... start_latency=...
  last_peak=... last_floor=... dBFS | capture avg/max`. Peak and floor are one number per take, used for calibration.

Rules and wipe triggers are in GAME_SPEC.md § Microphone.

Shared-API change (AGENTS rule 6, needs review): `firmware/components/audio` gains an optional
`I2sAudioOptions::mic_sink` (`MicSink` interface, `mic_sink.hpp`). It defaults to `nullptr`, so other firmware is
unchanged.

### Rollback

- Exact restore of the image that was on COM16 before this work (`little_arcade_launcher`, `e01bc23-dirty`). Run this from the
  backup folder; it holds `boot_pt_0x0.bin`, `app_0x10000.bin` and `assets_0xA10000.bin`, read back with esptool
  before the first flash:
  `.venv\Scripts\python.exe -m esptool --chip esp32s3 -p COM16 -b 460800 write-flash 0x0 boot_pt_0x0.bin 0x10000 app_0x10000.bin 0xA10000 assets_0xA10000.bin`
- Or rebuild and flash another firmware with its own script, for example `v2\native\color_pop\device\flash.ps1`.
- Source rollback: `git revert <this commit>` removes `v2/native/sound_jar/`, the `add_subdirectory(sound_jar)` line
  and the `.gitignore` entries. No other game or the launcher is modified.
- Microphone rollback: `git revert` the microphone commit. That restores the built-in-only game (`36ec287`) and removes
  the optional `MicSink` hook from `firmware/components/audio`. Then rebuild and run `device\flash.ps1 -Port COM16`.
  To keep the code but turn live recording off, switch the parent mic setting off (`settings::load_mic_enabled()`).

## Verification evidence

| Check | Result | Evidence |
|---|---|---|
| Game rules and edge cases (`sound_jar_game_test`) | pass | host |
| Score render / cue checks (`sound_jar_score_test`) | pass | host |
| Offscreen LVGL interaction and render check (`sound_jar_offscreen_check`) | pass | host |
| `--verify-redraw` on all 14 scenarios, partial and direct modes | 0 stale pixels | host |
| Seeded browser vs LVGL, 14 scenarios | all pass; MAE 3.1–4.8, >32 px-delta share 0.55–2.12% (thresholds MAE ≤ 5.5, ≤ 3.0%; rationale and negative controls in `tools/visual_compare.mjs`) | browser + host |
| SDL window run, framebuffer screenshot | renders correctly | host |
| Flash, boot, pack mapped, no errors (COM16: ESP32-S3 QFN56 rev v0.2, 16 MB flash, 8 MB PSRAM) | pass | physical device (serial log only) |
| Microphone take (`sound_jar_live_test`): capture, trim, normalise, end rules against a −55/−45 dBFS room profile, lead-in discard, wipes, too-quiet hush, direct flow vs built-in countdown, parity with the mic off | pass | host |
| `Game::advance` keeps wall-clock pace in ≤ 50 ms ticks (`sound_jar_game_test`) | pass | host |
| Mic boot: `mic switch=on hardware=yes -> recordings use the microphone` | pass | physical device (serial) |
| Own voice recorded and played back on pads and jar; no howl | pass: user-confirmed; takes peaked at −18 to −23 dBFS, `jar_live=1` | physical device (user + serial) |
| Recording starts on the tap; jar fills while talking | pass: user-confirmed; start latency 13–22 ms | physical device (user + serial) |
| Auto-stop about 0.5 s after speech; leading/trailing silence trimmed | pass: user-confirmed (10× floor rule; floor −49 to −56 dBFS) | physical device (user + serial) |
| Silent take: jar empty and quiet, hand points at mic | pass: user-confirmed | physical device (user) |
| Animations at natural speed (wall-clock `advance`) | pass: user-confirmed | physical device (user) |

The serial log does not show what the screen looks like or how loud the sound is. The physical-device rows marked
"user" were checked by the user playing on the board. Sound levels have not been calibrated with a meter, and the
mic has not been tested with a child.

### Measured budgets (physical device, serial logs)

| Item | Measured | Budget ([04_requirements.md](../../docs/04_requirements.md)) |
|---|---|---|
| App binary | 1,152,816 B with the mic (1,145,648 B before) in the 4 MiB factory partition (72.5% free) | PF-10 ≥ 10% headroom: met |
| Sprite pack | 2,286,899 B in the 0x5F0000 assets partition (63.3% free) | PF-10: met |
| Static internal RAM (DIRAM) | 276,995 / 341,760 B | — |
| Free internal heap, steady state | 68,975 B with the mic (full-duplex RX DMA buffers); 80,575 B before | PF-7 ≥ 64 KiB: met at steady state |
| Free internal heap, lowest ever | 40,192 B (brief dip during boot, display/audio/touch init, before the view is created) | PF-7: **not met at the boot dip** |
| Free PSRAM | 8,167,680 B (the 96,000 B clip included) | PF-8 ≥ 1 MiB: met |
| Mic capture cost | avg 55–60 µs per 16 ms block; up to ~10 ms once per take (zeroing the 96 KB PSRAM clip on arm/wipe) | — |
| Audio score load | 0.1% idle; 12.5–13.1% of one core in bench FULL play, worst block 4.2 ms, 0 drops | PF-9 ≤ 25%: met |
| Frame rate, START (idle) | p95 interval 8.2–8.4 fps; render p50 91 ms at 33% dirty | **PF-1 ≥ 30 fps p95: not met** |
| Frame rate, bench play (RECORDING / FULL) | 6.0 fps / 3.2–4.0 fps p95; render p50 ≈ 98 / 165–190 ms | **PF-1: not met (blocker)** |
| Frame rate, interactive play with wall-clock `advance` | interval p50 130–160 ms, p95 4.7–6.9 fps; dirty p50 55–69% | **PF-1: not met**; animation speed now correct |

## Known gaps

- **PF-1 frame rate is a measured blocker.** These measurements are from the physical device:
  - A full-screen render takes about 188 ms in FULL play.
  - The per-layer profile (bench) attributes 41 ms to the FX custom-draw pass (expanding rings drawn with full-circle `lv_draw_arc`, plus particles) and 13 ms to the overlay pass.
  - About 65 alpha-blended, mostly transformed sprite layers make up the rest, roughly 4–10 ms each.
  - Most sprites breathe, bob or fade every frame, as in the reference. At this frame rate 77–85% of the screen is dirty every frame in FULL.

  Changes already tried on the device:

  | Change | Effect |
  |---|---|
  | Batch view invalidation | `sync` 24–28 → 7–14 ms; no more invalid-area overflow into full-screen redraws; START 7.1 → 8.3 fps |
  | 2 draw units | small gain |
  | Pack in PSRAM instead of flash | no gain; reverted |
  | `LV_ATTRIBUTE_FAST_MEM_USE_IRAM` | does not fit: DRAM overflow of 36,696 B |

  Reaching 30 fps needs about 6× less render work. That needs a fidelity or design decision. Options:
  - pre-rendered ring sprites;
  - caching static layers into a pre-composited background;
  - fewer continuously animated sprites on the device;
  - a lower device animation rate.
- PF-7 is not met for the brief internal-heap dip at boot (40.2 KB). Steady state is ≈69 KB with the mic.
- **Animation play rate (fixed).** Below 20 fps a single 50 ms-capped tick per frame slowed every animation;
  the device now ticks in ≤ 50 ms steps up to the wall clock (`Game::advance`). Motion is correct in time, but it is
  steppier and slightly lower in frame rate (more of the screen changes per frame). That is PF-1 above.
- Microphone: the input gain (×16) and the 10× noise-floor rule were tuned in one room on one board. In a room
  whose quiet floor is above −56 dBFS (LOUD / 10), "sounding" becomes LOUD, so soft word tails may be trimmed. It
  has not been tested with a child's voice. The `MicSink` hook in the shared audio component needs review (AGENTS rule 6).
- The audio score is a port of the reference's Web Audio graph. It is not bit-identical; see GAME_SPEC.md.
  Sound levels on the device have not been calibrated.
- Sprite animation is limited to the exported frames: the jar mist level uses 33 steps and the swirl 16 phases.
  Layering matches drawScene() except where GAME_SPEC.md says otherwise.
- The touch controller init logs an `i2c.master` pull-up warning. It comes from the board, not this game.
