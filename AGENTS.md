# Agent operating rules

1. Read README and docs before edits. Prefer small PRs and link acceptance criteria.
2. Hardware facts require evidence: record manufacturer schematic/example path, revision and test outcome. Unknown GPIO = `TBD`, never guessed.
3. Keep `config/commands.json` the single source of truth for intents. Generate C++ mapping from it; don't duplicate string literals.
4. Games must be touch-complete without voice, noncompetitive, forgiving, no punitive failure, no text reading required.
5. No network dependencies at runtime; avoid telemetry, child voice storage, camera, cloud APIs, and persistent identifiers.
6. Shared APIs defined in `docs/architecture.md`; each game owns its own source folder. No cross-game edits without review.
7. Every PR supplies build/test evidence, heap/PSRAM deltas where relevant, and rollback instructions.
8. Never claim hardware validation from a desktop simulation. Tests must distinguish host, emulator, and physical device.
9. Protect speaker output: conservative default gain, volume cap, no unexpected loud startup sounds.
10. If model compatibility, pins or RAM fail, create a blocking issue rather than silently changing requirements.
11. Run containers with WSL containers (`wslc`), never Docker. Use `tools/idf_wslc_build.ps1` for ESP-IDF builds.
12. When a game or feature is done (build and host tests pass), commit it to `main` and push `main` to its remote. Stage only the files you changed; never commit unrelated user edits, secrets, or build outputs. If no remote is configured, commit and report that the push was skipped.
