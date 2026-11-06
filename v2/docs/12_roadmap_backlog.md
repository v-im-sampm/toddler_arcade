# 12: Roadmap and backlog

Milestones map to gates in `11_qa_acceptance.md`. Tickets list dependencies. Anything without a
dependency can run in parallel. Owner = specialist agent in `../agents/`.

## Milestones
| M | Name | Gate | Outcome |
|---|---|---|---|
| M0 | Decide | G0 | Survey, spikes, ADRs approved |
| M1 | Engine + PAL | G1 | Renderer, audio, input, assets on device + web |
| M2 | Shell | G2 | Launcher, parent area, settings, transitions, sleep, privacy wipe on 3 platforms |
| M3 | Golden game | G3 | Color Pop at full quality everywhere; pipeline proven end-to-end |
| M4 | Authoring kit | G4 | Templates, dev server, replays, budgets, docs |
| M5 | Catalogue | G5 | Remaining 11 games |
| M6 | Voice | G6 | Offline voice on all platforms |
| M7 | Harden + ship | G7–G9 | Safety, soak, compliance, release |
M6 can start after M1, in parallel with M3–M5.

## Tickets
| ID | Owner | Depends | Deliverable |
|---|---|---|---|
| **R01** | research | — | Desk survey + `decisions/survey_matrix.md` + licence register draft |
| R02 | platform | R01 | Spikes 4.1, 4.2, 4.9 harnesses + owner-run results |
| R03 | graphics | R01 | Spikes 4.3 (vector/skeletal vs baked) + 4.6 toy game per short-listed stack |
| R04 | audio | R01 | Spike 4.5 + ADR-012 rate decision (with owner measurements) |
| R05 | speech | R01 | Spikes 4.7, 4.8 |
| R06 | platform | R01 | Spike 4.4 (scripting VM), if short-listed |
| R07 | hardware | R02, R04 | Spike 4.10 power/thermal + H02 leftovers (5-point calibration, PCB revision, TE pin check) |
| **R08** | research | R02–R07 | ADR-001…012 written; owner approval |
| P01 | platform | R08 | Monorepo skeleton, codegen (intents, tokens, manifests, pins) with `--check`, CI, `wslc` build image |
| P02 | platform | P01 | Device PAL: ST7789 DMA flush (chosen clock), NS2009 with filtering + calibration, I2S full-duplex at the chosen rate, NVS, asset partition access |
| P03 | platform | P01 | Web PAL: canvas/WebGL host, Pointer Events, AudioWorklet, IndexedDB, service worker |
| P04 | graphics | P02, P03 | Renderer: retained scene graph, dirty regions, sprites/atlases, 9-slice, tint, additive, debug overlay |
| P05 | graphics | P04 | Animation runtime: tweens/tokens, timelines, springs, clip player (baked and/or live per ADR-004), character state machines |
| P06 | graphics | P04 | Particle system (data-defined, pooled, budgets) |
| P07 | audio | P02, P03 | Audio engine: buses, ducking, synth (port v1 + new voices), sampler (port v1 DSP), limiter/ceiling (port v1), scale-aware cues, music sequencer/stems, deterministic offline render |
| P08 | platform | P04 | Input: pointer phases, gesture recognisers, shell pre-emption, debounce, intents |
| P09 | platform | P01 | Asset pipeline: sources → per-platform packs, atlases, dithering, audio normalisation, provenance + budget checks |
| P10 | platform | P04–P09 | Game SDK v0.1 + reference docs + import-boundary lint |
| S01 | platform | P10 | Shell: island launcher, Home, transitions, parent gate/settings, sleep, session timer, privacy wipe tests, About/licences |
| S02 | platform | S01 | Phone packaging per ADR-009 (Android + iOS debug builds), permission audit |
| ART-01 | art | R08 | Character bible (Bolt, Munch, creatures, Finn, narrator helper), palette proof on the real panel |
| ART-02 | art | ART-01, P09 | Launcher island + shared UI kit (Home, Mic, buttons, ripples, hint hand) |
| AUD-01 | audio | R08 | Toy Box instrument kit + launcher theme + shared stingers |
| AUD-02 | audio | AUD-01, P09 | Narrator persona + pipeline (recording/TTS), shared praise/hint lines |
| **G01** | games | S01, ART-02, AUD-02 | **Color Pop (golden game)** on all 3 platforms + device evidence |
| T01 | qa | G01 | Safety analysers (flash, targets, colour-cue, loudness), golden render/audio infra, device bench runner |
| K01 | platform | G01, T01 | Authoring kit: templates (5), `new-game`, dev server with device-fidelity mode, replay recorder |
| K02 | qa | K01 | Fresh-agent dry run: build a throwaway sample game from docs only (gate G4) |
| G02 | games | K01 | Hatch-a-Pet |
| G03 | games | K01 | Rainbow Piano |
| G04 | games | K01 | Robo Buddy |
| G05 | games | K01 | Shape Garage |
| G06 | games | K01 | Animal Sounds |
| G07 | games | K01 | Rocket Rescue |
| G08 | games | K01 | Silly Faces |
| G09 | games | K01 | Feed the Monster |
| G10 | games | K01 | Bubble Aquarium |
| G11 | games | K01 | Sound Jar |
| G12 | games | K01 | Beat Sharks |
| V01 | speech | P07, R05 | Device voice: ESP-SR integration, AFE/cooldown, intent codegen → grammar |
| V02 | speech | P03, R05 | Web/phone offline recogniser integration, lazy model load |
| V03 | qa | V01, V02 | Voice evaluation protocol (`08` §5) and report |
| Q01 | qa | G02–G12 | Soak, sleep/wake, power-cycle, low-voltage (device); web/phone soak |
| Q02 | qa | Q01, V03 | Compliance checklist, SPL report, privacy network audit, release bundle |
| REL | platform | Q02 | v2.0.0 release artifacts + rollback docs |

Every ticket includes:
- file ownership,
- acceptance criteria IDs (from `04` or a game doc),
- tests to run,
- the expected evidence tier,
- budget deltas,
- rollback notes.

## Suggested new games after v2.0 (validate with toddler play-tests first)
| Idea | Template | Hook |
|---|---|---|
| Peekaboo Garden | tap-choice | Animals hide behind bushes; tap to find; object permanence |
| Bath Time Boats | free-play | Push boats, splash, rubber duck squeaks; light drag optional |
| Dance Party with Bolt | music-toy | Tap a move; Bolt and friends dance to the beat; copy-me |
| Sticker Scene | free-play | Tap stickers into a scene (tap-to-place); seasonal packs |
| Little Chef | character-pet | Stir, pour, sprinkle (taps); Munch tastes; no wrong recipes |
| Day & Night | free-play | Toggle sun/moon; animals go to sleep; calm bedtime game |
