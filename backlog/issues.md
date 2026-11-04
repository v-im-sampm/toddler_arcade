# GitHub issue backlog (copy into issues; dependencies matter)
| ID | Owner | Depends on | Deliverable |
|---|---|---|---|
| H01 | hardware | — | Exact schematic/revision pin map and GPIO conflict matrix |
| H02 | hardware | H01 | ST7789 + NS2009 bring-up, touch calibration, photos/logs |
| H03 | hardware | H01 | External I2S mic + MAX98357A wired and validated, gain limits |
| P01 | platform | H01 | ESP-IDF dependency/partition spike, actual build skeleton |
| P02 | platform | P01,H02 | LVGL UI task, launcher, game interface, navigation |
| P03 | platform | P01,H03 | Audio effects engine, voice prompt playback, safe volume |
| V01 | speech | H03,P01 | ESP-SR offline grammar/model compatibility and size benchmark |
| V02 | speech | V01,P02 | Push-to-talk intent queue, context filter, thresholds, fallback |
| A01 | assets | P02 | Asset generation/export pipeline, provenance, manifests |
| G01 | games | P02,P03,A01 | Color Pop |
| G02 | games | P02,P03,A01 | Hatch-a-Pet |
| G03 | games | P02,P03,A01 | Rainbow Piano |
| G04 | games | P02,P03,A01 | Robo Buddy |
| G05 | games | P02,P03,A01 | Shape Garage |
| G06 | games | P02,P03,A01 | Animal Sounds |
| G07 | games | P02,P03,A01 | Rocket Rescue |
| G08 | games | P02,P03,A01 | Silly Faces |
| G09 | games | P02,P03,A01 | Feed the Monster |
| G10 | games | P02,P03,A01 | Bubble Aquarium |
| I01 | integration | V02,G01-G10 | Registry, contextual grammar, build/size integration |
| Q01 | QA | I01 | Device validation, 2h soak, memory/latency report |
| R01 | integration | Q01 | Versioned release firmware, wiring guide, known limitations |

## Status (2026-11-04)
| ID | Status | Evidence / blocker |
|---|---|---|
| H01 | Done — source review only | `config/board_pins.json` (`verified: false`), `docs/hardware.md` conflict matrix, `tools/board_pins.py` gate. Physical PCB revision not observed. |
| P01 | Done — container build only | `firmware/` ESP-IDF v5.5.6 skeleton, `dependencies.lock`, size report in `firmware/README.md`. Never flashed. |
| H02 | **BLOCKED** | Needs a physical MaTouch Lite board: confirm module variant (flash/PSRAM mode), PCB revision, display rotation, backlight polarity, NS2009 address/axes, 5-point calibration. No board was connected to the build machine. |
| H03 | **BLOCKED** | Needs physical mic + MAX98357A + speaker. H01 found only GPIO4–7 accessible (J1/J2) and 3V3-only headers: a shared-clock full-duplex I2S layout uses all four, leaving no amp-enable GPIO. Requires a hardware decision (hard-wired SD_MODE, sacrificing SD/another bus, or an I/O expander) plus current/thermal measurement of the LD1117. |
| P02, P03, V01 → all later tickets | Blocked by H02/H03 per dependency table | — |

### Host-ahead work (logic only; NOT hardware-validated, tickets remain open)
The owner chose to continue host-testable logic while H02/H03 wait on hardware. None of this closes a ticket.
| ID | Host-ahead part done | Still open |
|---|---|---|
| P02 | Frozen `IGame`/`GameContext`, layout constants, `SceneManager` (launcher paging, Home/BACK, pause, debounce, sleep, voice context filter), host tests | LVGL UI task + views, display/touch drivers (needs H02), measured touch latency |
| P03 | `VolumeLimiter`: muted startup, parent cap, ramp, hard peak limit; host tests | I2S driver, mixer, prompt playback, acoustic level measurement (needs H03) |
| V02 | `PushToTalkSession` (4 s silence / 8 s max) and `VoiceIntentFilter` (confidence, context, dedupe, playback cooldown); host tests | ESP-SR integration and threshold calibration (needs V01/H03) |
| G01–G10 | All 10 scenes: logic + host tests (14/14 suites pass on MSVC and container GCC; each game component compiles for ESP32-S3). See each `firmware/components/games/*/README.md` | LVGL views, assets (A01), device playthrough (Q01); decide whether to add game-specific voice intents (animals, shapes, planets, feed/sleep) |

## Each issue must include
Exact file ownership, acceptance criteria from the matching spec, target branch, tests to run, expected artifacts and measurement evidence. Do not claim done without physical board testing when hardware is involved.
