# v2 agent operating rules

These rules extend the v1 rules in `../AGENTS.md`. Where they conflict, v2 wins inside `v2/` and
the v2 codebase.

## Safety and privacy (absolute)
1. **No network at runtime** on any platform. The web/PWA build may be *served* over HTTPS. After
   install it must run fully offline. Make no runtime fetches beyond its own bundle/cache and use no
   third-party origins.
2. **No telemetry or analytics.** No crash reporters that phone home, no advertising SDKs, no
   accounts, no persistent identifiers, no fingerprinting. Ship third-party SDKs only if the licence
   allows it and they have no network code paths.
3. **Child audio never leaves RAM.** A recording is held for at most the game's documented window
   (v1: ≤ 3 s, one clip per sampler game). It is zeroed on exit, pause, sleep and parent-gate entry.
   It is never logged, written to storage, or sent to a platform speech service. Browser and phone
   OS speech APIs that may upload audio are **forbidden**.
4. **Touch is complete.** Every action reachable by voice is reachable by a large touch target.
5. **No failure, no pressure.** No fail screens, lives, scores that go down, countdown timers, or
   buzzers. No text-reading requirement. Colour is never the only cue.
6. **Photosensitivity:** no more than 3 luminance flashes per second, and no full-screen high-contrast
   strobes or saturated red flashes (WCAG 2.3.1-style rule). This is enforced by an automated check
   (see `docs/11_qa_acceptance.md`).
7. **Hearing safety:** the device starts muted until a parent sets volume. Use a hard digital
   ceiling, a limiter, and ramped volume changes. No sudden loud onsets: each sound's attack and peak
   follow `docs/07_audio_design.md`.
8. Parent-only areas sit behind the hidden gate (hold 5 s + adult challenge). They contain no
   external links, purchases or social features.

## Engineering
9. **Phase 0 first.** No engine code before the market-survey ADRs are approved by the owner.
10. **Single sources of truth, generated everywhere.**
    - Intents come from `config/commands.json` (v2 may extend its schema).
    - Design tokens come from `v2/design/tokens.json`.
    - Game manifests follow `v2/schemas/`.
    - Hardware facts come from `config/board_pins.json`.
    - Generate code for every platform from these. Don't duplicate literals. CI checks for drift.
11. **Evidence tiers.** Label every result as `host`, `web`, `phone-emulator`, `phone-device` or
    `esp32-device`. Never claim a higher tier than you ran. Hardware facts need schematic/datasheet
    citations or bench measurements. Unknown GPIO = `TBD`.
12. **Budgets are gates.** CI fails if any of these exceed budget: flash/app size, PSRAM/internal
    heap at worst-case scene, frame time, or audio CPU. Each PR that touches them reports deltas.
13. **Game isolation.** Each game owns its folder: logic, scenes, assets and tests. Games talk to the
    platform only through the Game SDK. Cross-game or SDK changes need platform review and a version
    bump.
14. **Determinism.** Game logic is deterministic given seed + input timeline, on every platform. Use
    no wall-clock reads, no platform RNG and no float-order-dependent branching in logic.
    Golden replays run in CI.
15. **Containers:** use WSL containers (`wslc`), never Docker.
16. **Licensing and provenance.** All assets are original, CC0/public-domain, or permissively licensed.
    Provenance and hashes go in the asset manifest. No copyrighted characters or likenesses, no
    recognisable commercial audio, and no AI-generated assets unless the tool's licence permits
    commercial use and the provenance records the tool, version and prompt.
17. **Blockers, not workarounds.** If a model, pin, licence or RAM budget fails, write a blocking issue
    and stop. Don't silently relax a requirement.
18. **Commits:** small, focused, with build/test evidence and rollback notes. Stage only your files.
    Follow the owner's instruction on whether to commit or push.
