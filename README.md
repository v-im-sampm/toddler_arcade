# Little Arcade — Copilot Agent Handoff

**Status: prototype firmware runs on the physical board.** Display/touch playthrough is confirmed; the owner has wired the external I2S mic and speaker and confirmed audible audio (H03 functional validation). H02 calibration/board details and quantitative audio power/acoustic measurements remain open. Target: Makerfabs MaTouch Lite ESP32-S3 2.4-inch ST7789 320×240 / NS2009 resistive touch, 16 MB flash, 8 MB PSRAM. Ten offline toddler games plus **predefined offline voice commands**.

## Start here
1. Read `AGENTS.md`, `docs/architecture.md`, `docs/hardware.md`, `docs/voice.md`, `docs/acceptance.md`.
2. Run hardware discovery **before assigning pins or soldering**. Record validated GPIOs in `config/board_pins.example.json` and produce a checked-in board config.
3. In GitHub Copilot coding agent, assign issues from `backlog/issues.md` in dependency order; copy the prompts from `.github/agents/` for specialist agents.
4. Follow `docs/integration.md` to build and flash once actual board drivers are implemented.

## Boundaries
- Never invent display, touch, mic or amplifier pins. Makerfabs examples/schematic are authoritative and must be inspected.
- Audio hardware is **not included** in the board specification. The owner-wired external INMP441/MAX98357A mic and speaker are functionally confirmed audible on the prototype; wiring pins remain owner-reported, and calibrated SPL, power-on pop, current and regulator thermal measurements are not recorded. See `docs/hardware.md`.
- Espressif ESP-SR WakeNet/MultiNet is the intended offline stack. Verify **current model language support, model sizes, licensing, ESP-IDF compatibility and toddler voice accuracy** before choosing a model. Some commands may need constrained phrasing or custom models; fallback to touch is mandatory.
- No microphone streaming, account, ads, analytics, or Wi-Fi needed in release 1. No child speech is persisted or transmitted. Sound Jar may hold one ≤ 3 s clip in RAM while open (parent Mic toggle, default on; see `docs/prd/sound_jar.md` §7).
- All source assets must be original, permissively licensed, or generated in-project with recorded provenance.

## Contents
- `.github/copilot-instructions.md`: repository-wide Copilot guardrails
- `.github/agents/*.agent.md`: hardware, platform, speech, games, QA agents
- `docs/`: specifications, contracts, milestones, QA, child safety, asset pipeline
- `games/*.md`: 10 individual game specifications
- `config/commands.json`: canonical voice intent map
- `config/board_pins.example.json`: unresolved hardware wiring template
- `backlog/issues.md`: sequential and parallelizable tickets
- `firmware/`: ESP-IDF prototype firmware, host-testable game/settings logic, and desktop simulator
- `tools/validate_specs.py`: validates command manifest and all ten game specs

## Definition of done
One ESP-IDF image boots, calibrates touch, plays audio, recognizes supported offline commands in quiet conditions, runs all eleven games without network access, recovers from errors, and passes on-device memory/latency/child-safety checks. See `docs/acceptance.md`.
