# 11: QA, acceptance gates and evidence

## 1. Evidence tiers (label every result)
`host` · `web` (name browser + version) · `phone-emulator` · `phone-device` (model + OS) ·
`esp32-device` (board revision, module, firmware commit). A result proves only its own tier.

## 2. Gates
| Gate | Name | Pass condition |
|---|---|---|
| G0 | Decisions | All Phase 0 ADRs approved; licence register complete; spike results recorded |
| G1 | Engine on device | Spikes 4.1/4.2/4.5 targets met with the production engine; touch calibration (5-point) stored; startup silent |
| G2 | Shell | Launcher, Home, parent gate, settings, sleep, session timer, transitions on all 3 platforms; privacy-wipe tests pass |
| G3 | Golden game | Color Pop meets every criterion in `games/color_pop.md` on all 3 platforms with device evidence |
| G4 | Authoring kit | A new sample game is scaffolded → previewed → replay-tested → flashed to the device by following docs only (dry run by a fresh agent) |
| G5 | Catalogue | All 12 games pass their acceptance criteria + DoD (`09` §5) |
| G6 | Voice | Device + web + phone recognition meets `08` §5 targets (adult); disable path verified; zero-network audit |
| G7 | Safety and compliance | Flash/luminance lint green for all replays; SPL measured ≤ limits; store-policy checklist complete; privacy audit (network capture shows zero requests after install) |
| G8 | Reliability | 2 h device soak cycling all games (heap min stable); 1 h web/phone soak; sleep/wake ×100; power-cycle mid-game ×50; low-voltage brownout recovery (device) |
| G9 | Release | Artifacts, notes, rollback, known limitations |

## 3. Test pyramid
1. **Unit:** pure rules, DSP blocks (from v1: limiter, sampler trims, synth envelopes), codegen,
   and the manifest validators.
2. **Replay/integration:** every game has replays per acceptance criterion. Each asserts state
   checkpoints, emitted cues, and that no input is ignored longer than 300 ms outside documented
   windows. Include rapid input, target and empty-space taps, at least one success, each rung of the
   hint ladder, and an audio-off/reduced-motion replay.
3. **Golden render/audio:** per platform renderer; perceptual tolerance; reviewed diffs.
4. **Safety analysers** (automated, CI):
   - **Flash:** per frame, compute relative luminance over sliding windows. Fail if more than 3
     general or red flashes per second exceed the WCAG 2.3.1 area/contrast thresholds (implement per
     the WCAG "general flash and red flash thresholds" definition; cite it).
   - **Targets:** every interactive node's hit area ≥ 48×48 logical units and spacing ≥ 8 (allow
     documented exceptions such as the Beat Sharks 4-unit column gap, with a reason).
   - **Colour-only cues:** answer sets must differ by shape/pattern as well as hue (manifest flag +
     a CVD-simulated image diff).
   - **Loudness:** asset normalisation report; onset guard check on golden audio.
   - **Quality contract:** per-screen review confirms idle, press, release, success and hint states;
     every touch has a same-frame visual acknowledgement; every audio cue has a visual counterpart;
     input remains available during nonessential animation; particle/voice caps hold during mashing.
     The quality-bar sketch is a reference for feel, not a golden image to copy.
   - **Visual craft:** reviewed 320×240 captures for every game's idle, interaction, hint and
     celebration states. Reject placeholder-flat scenes, clipped focal art, missing highlights or
     shadows, and inconsistent chrome. Include a Home/re-entry capture to expose stale view state.
5. **Device bench:** replays on the board. Collect FPS p50/p95/p99, the worst-frame dirty area,
   particle count, heap/PSRAM minima, audio CPU, underruns, and touch→visual/sound latency. Report
   both normal and reduced-motion results; never present a browser preview as device evidence.
6. **Exploratory + toddler play-test:**
   - Parent-observed sessions; notes only, **no recordings of the child**.
   - Questions: Where did they hesitate? What made them laugh? Did they ever look confused or upset?
   - Changes are tracked as design tickets.

## 4. Latency measurement method (device)
- **Touch → sound:** a contact microphone on the screen plus the speaker output (or the board's own
  mic in a lab build) recorded together. Measure the tap transient to the output onset over ≥ 30
  taps.
- **Touch → visual:** a high-speed phone video (240 FPS) of a finger tap, counting frames to the
  first pixel change. Alternatively use a debug GPIO toggle on touch-down and on SPI flush of the
  responding region, read by a logic analyser.

## 5. Release evidence bundle
Build logs, the partition map, size reports per game, the budget report, device metrics tables,
analyser reports, SPL/current/pop measurements, a short video per game per platform, the licence
register, and known limitations. Golden diffs may be included as optional diagnostics, but are not
required release evidence.
