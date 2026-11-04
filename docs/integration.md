# Integration, build and agent orchestration
## Sequence
1. Hardware agent verifies board schematic, pins, display/touch, I2S and power.
2. Platform agent pins ESP-IDF/LVGL versions, creates real `firmware/` ESP-IDF project, driver abstraction, launcher, assets and test harness.
3. Voice agent runs standalone ESP-SR compatibility spike in parallel with game spec/asset work; then integrates intent queue.
4. Games agents build scenes in separate folders against frozen `IGame` interface. Start Color Pop, Hatch-a-Pet, Rainbow Piano; then remaining seven.
5. QA agent tests host and hardware, size, safety, and stability; release gate.

## Local prerequisites
Install ESP-IDF toolchain compatible with chosen ESP-SR, board USB serial driver, and dependencies. Run `idf.py set-target esp32s3`, `idf.py build`, `idf.py -p PORT flash monitor` **only after platform agent creates real ESP-IDF source and validates wiring**. This handoff pack is not itself flashable.

## Merge policy
Platform agent owns `firmware/components/core`, `board`, `ui` and build system. Voice agent owns `speech`. Each game agent owns its scene directory and tests. One integrator owns registry/manifest changes. No agent edits the hardware mapping without hardware-agent approval.

## Agent handoff prompts
Copy `backlog/issues.md` tickets into GitHub issues. Assign `.github/agents/*.agent.md` as role guidance. Require each PR to link issue, include exact tested environment, unit/device results, memory impact, and follow-up risks.
