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

## Each issue must include
Exact file ownership, acceptance criteria from the matching spec, target branch, tests to run, expected artifacts and measurement evidence. Do not claim done without physical board testing when hardware is involved.
