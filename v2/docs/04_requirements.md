# 04: Requirements

IDs are stable: reference them in tickets, tests and ADRs. **MUST** is release-blocking. **SHOULD**
needs an ADR to waive.

## 1. Platforms
| ID | Requirement |
|---|---|
| PL-1 | MUST run on the v1 ESP32-S3 board (see `01_v1_baseline.md` §3) with no hardware changes. Optional additions need owner approval |
| PL-2 | MUST run in current Chrome, Edge, Firefox and Safari (desktop + mobile) as an installable PWA that works offline after first load |
| PL-3 | MUST ship to Android and iOS as installable apps (packaging per ADR-009) that need no network permission |
| PL-4 | MUST play every game from the **same game sources** on all platforms; platform differences only in backends and asset density |
| PL-5 | SHOULD support landscape only. Phones lock landscape; portrait shows a friendly "turn me" animation (no text) |
| PL-6 | MUST present a 4:3 logical stage of 320×240 units. Wider screens get art-directed side extensions (background bleed), never stretched gameplay. Touch targets scale with the stage |

## 2. Shell
| ID | Requirement |
|---|---|
| SH-1 | Launcher shows games as large animated tiles (each tile has an idle animation and a sound on press). Paging or a smooth horizontal carousel that works with taps (arrows); swipe optional |
| SH-2 | Persistent Home button (top-left) in every game; returns to launcher in ≤ 1 tap + transition (≤ 500 ms) |
| SH-3 | Push-to-talk mic button (top-right) when voice is enabled; hidden when disabled |
| SH-4 | Parent gate: hold a corner for 5 s with a visible progress fill, then an adult challenge. Settings: volume (0–N, muted default), voice on/off, mic-for-recording on/off, session timer (off/10/20/30 min) with a gentle "bedtime" wind-down, game visibility toggles, touch calibration (device), about/licences |
| SH-5 | Sleep after inactivity (default 10 min): a dim, calm sleeping animation; wake on tap; no sound on wake unless the child interacts |
| SH-6 | Boot to launcher ≤ 3 s on device (target), with no audio until the first user interaction and the saved volume ramp |
| SH-7 | Games register from manifests; adding or removing a game requires no shell code edits |
| SH-8 | Session timer wind-down: music slows, the narrator says goodnight, and the screen goes to sleep. Never abrupt |

## 3. Game SDK
| ID | Requirement |
|---|---|
| SDK-1 | Lifecycle: load → enter → (update/render, input, pause/resume)* → exit → unload; deterministic given seed + input timeline |
| SDK-2 | Input: pointer down/move/up/cancel with position and time; tap/hold/drag/slide recognisers; intents (voice or keyboard) filtered by scene context |
| SDK-3 | Scene graph: sprites, groups, transforms (pos/scale/rotate/anchor), opacity, tint, z-order, clip rects, nine-slice, hit areas (shape-based, with padding) |
| SDK-4 | Animation: tweens with token easings, timelines/sequences, spring physics, state-machine-driven character animations, and baked/vector clips per ADR-004 |
| SDK-5 | Particles: budgeted emitters defined in data (shape, colour ramp, lifetime, gravity, spin), pooled |
| SDK-6 | Audio: play cue by ID, music state changes, scale-quantised notes, sampler access (permissioned), voice prompts by ID; never blocks |
| SDK-7 | Assets: referenced by ID only; declared in the game manifest; per-platform variants resolved by the pipeline |
| SDK-8 | Services: seeded RNG, shuffle bags, timers in game time, hint scheduler (idle → visual hint → spoken hint → auto-help), and a celebration helper |
| SDK-9 | No heap allocation in the frame path after enter (device); pools sized from the manifest |
| SDK-10 | Capabilities declared in the manifest (`microphone`, `music`, `voice_intents`, …). The shell denies anything undeclared |
| SDK-11 | Versioned API (semver). Games state the minimum SDK version |

## 4. Performance budgets (verify/tune in Phase 0; ADR may revise with evidence)
| ID | Metric | ESP32-S3 | Web/phone |
|---|---|---|---|
| PF-1 | Animated-region frame rate | ≥ 30 FPS p95 | 60 FPS p95 on reference phones (define in ADR-009) |
| PF-2 | Full-screen transition frame rate | ≥ 20 FPS | 60 FPS |
| PF-3 | Touch → first visual response | p95 ≤ 60 ms | p95 ≤ 50 ms |
| PF-4 | Touch → sound onset | p95 ≤ 50 ms | p95 ≤ 50 ms (best effort; document browser/OS floor) |
| PF-5 | Voice: end of utterance → feedback | p95 ≤ 1.5 s | p95 ≤ 1.0 s |
| PF-6 | Game launch (tap tile → interactive) | ≤ 1.0 s | ≤ 0.7 s |
| PF-7 | Free internal heap at worst case | ≥ 64 KiB | n/a |
| PF-8 | Free PSRAM at worst case | ≥ 1 MiB | n/a |
| PF-9 | Audio task CPU | ≤ 25 % of one core | n/a |
| PF-10 | Per-game flash (code + assets) | budgeted per game in manifest; total fits the ADR-011 partition map with ≥ 10 % headroom | Web bundle ≤ 25 MB total, per-game lazy load ≤ 3 MB (target) |
| PF-11 | Soak | 2 h cycling all games, no crash, no leak (heap min stable ± 1 KiB) | 1 h, no leak |

## 5. Audio
| ID | Requirement |
|---|---|
| AU-1 | Device starts muted; parent-set level persists; ramp ≥ 300 ms on start/change |
| AU-2 | Hard digital ceiling derived from the amp gain/supply (v1 `amp_safe_peak`); look-ahead limiter on master |
| AU-3 | Buses: music, sfx, voice, ambience, sampler → master. Voice ducks music/ambience by 6–10 dB |
| AU-4 | All assets loudness-normalised at build time (targets in `07_audio_design.md`) |
| AU-5 | Every cue has a visual counterpart; the game is fully playable with audio off |
| AU-6 | No onset louder than the token `audio.max_onset_dbfs` relative to the bus; no click on start/stop |
| AU-7 | Phone/web respect the OS volume and silent switch where applicable; audio unlock on first touch is invisible to the child |

## 6. Voice (see `08_voice.md`)
| ID | Requirement |
|---|---|
| VO-1 | Offline-only recognition on all platforms; no audio retained |
| VO-2 | Push-to-talk default; listening ≤ 4 s silence / 8 s max |
| VO-3 | Context-filtered intents from `config/commands.json` (v2 schema) |
| VO-4 | Confidence threshold, 750 ms dedupe, playback cooldown or echo cancellation |
| VO-5 | Can be disabled by the parent; the mic button hides when disabled |

## 7. Accessibility and child UX
| ID | Requirement |
|---|---|
| UX-1 | Primary targets ≥ 48×48 logical units (≥ 56 preferred), spacing ≥ 8 |
| UX-2 | ≤ 4 simultaneous choices for questions; 2 at the start, growing with success |
| UX-3 | No text required; icons and a narrator. The parent UI may use text |
| UX-4 | Colour never the only cue (shape/pattern/position also) and checked for colour-vision deficiency |
| UX-5 | Flash safety: ≤ 3 flashes/s, no large saturated-red flashes; automated luminance check |
| UX-6 | Wrong input → gentle wiggle + hint. Second miss → stronger hint. Third → auto-help |
| UX-7 | Every scene responds to every tap (even "empty" taps make a ripple/sound) |
| UX-8 | Left-handed friendly: no essential controls only on one side where avoidable |

## 8. Privacy, safety, legal (see `13_safety_privacy_compliance.md`)
| ID | Requirement |
|---|---|
| PR-1 | No network permission requested on phone; web makes no runtime network requests after install (CSP `connect-src 'self'`) |
| PR-2 | No analytics, ads, accounts, IDs, crash upload |
| PR-3 | Recording games: RAM-only, ≤ 3 s, zeroed on all exit paths; mic permission requested only when the parent enables it |
| PR-4 | Licence register complete; an About screen lists open-source notices (parent area) |
