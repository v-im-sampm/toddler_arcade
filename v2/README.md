# Little Arcade v2: handoff package

This folder is a self-contained brief for an LLM (or a team) building **Little Arcade v2**. v2 is an
offline toddler arcade with high-quality audio, visuals and animation. It runs from one codebase on:

| Target | Role |
|---|---|
| **ESP32-S3 device** (Makerfabs MaTouch Lite 2.4", 320×240) | Primary product. This is the tightest budget, so it sets the design floor. |
| **Web** (offline-capable PWA) | Fastest authoring loop, demos, and a parent "try it" target. |
| **Phone** (Android + iOS) | Installed kids app, built from the web or native build. |

The v1 proof-of-concept is in the parent repository (`../firmware`, `../docs`, `../games`). Read
it as a **reference**. Its behaviour, safety rules and measured hardware facts carry forward. Its code
structure does not have to.

## How to use this package
1. Give the LLM **this whole `v2/` folder plus read access to the v1 repo**.
2. Paste [`HANDOFF_PROMPT.md`](HANDOFF_PROMPT.md) as the first instruction.
3. The LLM must complete **Phase 0, the market survey** ([`docs/03_market_survey_brief.md`](docs/03_market_survey_brief.md)).
   Its output is Architecture Decision Records in [`decisions/`](decisions/). The survey runs before
   any engine code is written, and it includes on-device spikes.
4. After you approve the ADRs, implementation follows [`docs/12_roadmap_backlog.md`](docs/12_roadmap_backlog.md).

## Reading order
| # | Document | Purpose |
|---|---|---|
| 0 | [AGENTS.md](AGENTS.md) | Non-negotiable operating rules for any agent |
| 1 | [docs/01_v1_baseline.md](docs/01_v1_baseline.md) | What v1 is, measured facts, lessons, what to keep/drop |
| 2 | [docs/02_vision_quality_bar.md](docs/02_vision_quality_bar.md) | Product vision and the "much higher quality" bar, made concrete |
| 3 | [docs/03_market_survey_brief.md](docs/03_market_survey_brief.md) | **Phase 0**: research + spikes to choose runtime, renderer, audio, voice, packaging |
| 4 | [docs/04_requirements.md](docs/04_requirements.md) | Functional / non-functional requirements and per-platform budgets |
| 5 | [docs/05_architecture.md](docs/05_architecture.md) | Runtime-agnostic target architecture and contracts |
| 6 | [docs/06_visual_motion_design.md](docs/06_visual_motion_design.md) | Art direction, motion language, rendering strategy for a 320×240 SPI panel |
| 7 | [docs/07_audio_design.md](docs/07_audio_design.md) | Sonic identity, adaptive music, mixer, loudness safety |
| 8 | [docs/08_voice.md](docs/08_voice.md) | Offline voice intents across device, web and phone |
| 9 | [docs/09_game_sdk_authoring.md](docs/09_game_sdk_authoring.md) | How a new game is created, previewed, tested and shipped |
| 10 | [docs/10_platforms_deployment.md](docs/10_platforms_deployment.md) | Build, flash, host, store-ship, update |
| 11 | [docs/11_qa_acceptance.md](docs/11_qa_acceptance.md) | Gates, test pyramid, golden images/audio, device evidence |
| 12 | [docs/12_roadmap_backlog.md](docs/12_roadmap_backlog.md) | Milestones and dependency-ordered tickets |
| 13 | [docs/13_safety_privacy_compliance.md](docs/13_safety_privacy_compliance.md) | Child safety, privacy, kids-store policy |
| — | [games/](games/) | One v2 game design document per game, plus a template for new games |
| — | [schemas/](schemas/) | JSON Schemas for game and asset manifests, with a worked example |
| — | [design/tokens.json](design/tokens.json) | Design tokens for colour, motion, layout and audio, shared by every platform |
| — | [reference/quality_bar.html](reference/quality_bar.html) | Runnable browser sketch of the intended feel (motion, particles, harmonised audio) |
| — | [agents/](agents/) | Specialist agent prompts for v2 roles |
| — | [decisions/](decisions/) | ADR template and the list of decisions Phase 0 must produce |

## Fixed decisions (owner-approved, do not reopen)
- **Device hardware stays the same:** ESP32-S3 (N16R8: 16 MB flash, 8 MB octal PSRAM). It uses a
  320×240 ST7789 over SPI, NS2009 resistive single-touch, and an external INMP441 mic plus MAX98357A
  amp sharing I2S clocks. The facts are in [`docs/01_v1_baseline.md`](docs/01_v1_baseline.md) §3 and
  `../config/board_pins.json`.
- **Offline voice commands stay in scope** on every platform. Touch is always a complete alternative.
- **The runtime/engine/language strategy is NOT pre-decided.** The next LLM must choose it through the
  evidence-based market survey.
- All twelve v1 games come forward and are redesigned. New games must be cheap to add.
- Child safety and privacy rules from v1 still apply in full, and they now extend to web and phone.

## Status of this package
These are documents and assets only. No v2 code exists yet. Every external fact about third-party
libraries in these docs is a **lead to verify**, not a verified claim. The survey must confirm
current versions, licences and performance with citations.
