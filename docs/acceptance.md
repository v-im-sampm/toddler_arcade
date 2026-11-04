# Acceptance and QA plan
## Gates
G0 Documentation: exact board revision, GPIO mapping, software dependency versions and licenses documented.
G1 Hardware: display 320×240, touch calibration corners/center, backlight, I2S mic and amp verified separately.
G2 Platform: stable home launcher, all games selectable, touch-only full playthrough, consistent back/home.
G3 Voice: context-filtered command recognition; voice can be disabled; microphone indicator; no network calls.
G4 Assets: original/licensed assets only; offline loading; audio levels safe; startup muted until configured.
G5 Reliability: 2 h soak cycling games, no crashes/leaks, touch responsiveness measured, sleep/resume works.

## Suggested quantitative targets (not yet measured)
- Touch feedback p95 < 100 ms; animations >= 20 FPS under speech load.
- Spoken command feedback p95 < 1.5 s after utterance end in quiet environment; log actual distribution.
- >= 90% adult command-intent accuracy in quiet room on test set; child results separately reported, no guaranteed threshold before pilot.
- Voice false triggers <= 1 per 30 minutes of representative audio (if always-listening enabled).
- Internal heap free >= 64 KiB and PSRAM free >= 1 MiB after worst-case scene and model init, subject to revision.
- No game failure blocks progression; any scene can return home in <= 2 actions.
- Zero audio persistence, zero telemetry, zero network dependency in release 1.

## Test matrix
Host: manifest validation, command routing, scene lifecycle, memory ownership, RNG seeds, boundary conditions.
Device: rotation, touch corners, SD presence/absence, low voltage/reset recovery, speaker gain, mic interference, cold boot, game transitions, memory watermarks, 2 h soak.
Voice: adult and child speakers, quiet/noisy, near/far, synonyms, low-confidence, double recognition, playback echo, command outside context.
Accessibility: no reading requirement, visual plus audio cues, large hit areas, no punishment, color not sole distinguishing feature.

## Release evidence
Attach build logs, partition map, size report, runtime heap/PSRAM minima, measured latency, video of each game, audio tests, board wiring diagram, and known limitations.
