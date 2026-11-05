# Integration, build and agent orchestration
## Sequence
1. Hardware agent verifies board schematic, pins, display/touch, I2S and power.
2. Platform agent pins ESP-IDF/LVGL versions, creates real `firmware/` ESP-IDF project, driver abstraction, launcher, assets and test harness.
3. Voice agent runs standalone ESP-SR compatibility spike in parallel with game spec/asset work; then integrates intent queue.
4. Games agents build scenes in separate folders against frozen `IGame` interface. Start Color Pop, Hatch-a-Pet, Rainbow Piano; then remaining seven.
5. QA agent tests host and hardware, size, safety, and stability; release gate.

## Build and deployment workflow

Use the supported Windows workflow in [firmware/README.md](../firmware/README.md); do not install ESP-IDF
locally or use Docker. Prerequisites are PowerShell 7, WSL with the `wslc` CLI, Git, and Python on the
Windows host. The first call prepares the versioned ESP-IDF image; subsequent calls reuse it, while
build artifacts and managed components persist in the mounted `firmware/` directory:

```powershell
pwsh tools/idf_wslc_build.ps1 -Prepare
pwsh tools/idf_wslc_build.ps1
```

The firmware build runs generated-header checks, ESP-IDF compilation, size reporting, and host C++
tests. For a clean rebuild, use `-Clean`; to refresh the cached build image, use `-RebuildImage`.

The desktop simulator is a separate host build (Visual Studio 2022 and CMake on Windows):

```powershell
cmake -S firmware/sim -B build-sim -G "Visual Studio 17 2022" -A x64
cmake --build build-sim --config Release
build-sim\Release\little_arcade_sim.exe
```

The simulator may fetch its pinned LVGL and SDL2 dependencies during the first CMake configure.
See [firmware/sim/README.md](../firmware/sim/README.md) for controls and benchmark options. Simulator
results are host-only and are not hardware validation.

To deploy, build the firmware, confirm the board and COM port, then run
`pwsh tools/flash_firmware.ps1 -Port COM15` (replace the port as appropriate). The script creates a
local `.venv` and installs the pinned Windows esptool dependency once; it flashes and verifies the
bootloader, partition table, app, and model image. Open a serial monitor and manually check boot,
display, touch, and audio. A successful flash is not a substitute for those physical checks. The
current four-byte `srmodels.bin` contains no selected speech model.

## Merge policy
When a game or feature is done, its firmware build and host tests pass, and any device flash has been reported honestly, commit the work to `main` and push it:

```powershell
git add <files you changed>
git commit -m "<area>: <summary>"
git push origin main
```

Stage only your own changes; leave unrelated user edits, build output, and `.venv/` uncommitted. If `main` has no remote, commit locally and report that the push was skipped.

Platform agent owns `firmware/components/core`, `board`, `ui` and build system. Voice agent owns `speech`. Each game agent owns its scene directory and tests. One integrator owns registry/manifest changes. No agent edits the hardware mapping without hardware-agent approval.

## Agent handoff prompts
Copy `backlog/issues.md` tickets into GitHub issues. Assign `.github/agents/*.agent.md` as role guidance. Require each PR to link issue, include exact tested environment, unit/device results, memory impact, and follow-up risks.
