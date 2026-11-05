# Offline predefined voice commands
## Scope
Fixed commands only, not free-form dictation or conversations. Intended ESP-SR WakeNet/MultiNet on ESP32-S3 after compatibility validation. Verify supported language/acoustic model, model licensing, current Espressif APIs and command grammar. **Do not assume arbitrary custom words or toddler voices are supported by an off-the-shelf model.**

## Activation
Phase A: push-to-talk via a large microphone icon (touch), so no wake-word false activations. Phase B optional wake phrase after measurements; never require wake phrase for games. Visual listening state and short audible cue. Stop listening after 4 s silence or 8 s maximum; never retain captured audio. Do not transmit audio.

## Intents
`config/commands.json` defines stable intent IDs, spoken aliases and contexts. First release: menu navigation (start/back/next/again/help), game answers (colors, numbers 1–3, yes/no), actions (go/stop/pause/resume), and optional game names. Don't enable irrelevant grammar outside a scene. Prompt the user to speak once; no always-on background capture until validated.

## Recognition policy
- Voice input is supplementary; touch can accomplish everything.
- Use calibrated confidence and noise thresholds; if uncertain, ask again or show touch choices. Never infer an answer from low confidence.
- Context restricts valid commands. Example: Color Pop accepts RED/BLUE/GREEN/YELLOW, AGAIN, BACK; Rocket accepts GO/STOP/BACK.
- 750 ms dedupe for repeated same command; 1.5 s cooldown after speaker playback if no AEC; tune by bench tests.
- Do not upload child's voice; no stored utterances or transcripts. No child speech is persisted or transmitted. Sound Jar and Beat Sharks may each hold one ≤ 3 s clip in RAM while open (never routed to ESP-SR, never logged or saved).
- Speech recognition in noisy rooms or with children's voices is experimental: test with adult and child speech only with guardian permission and without retaining recordings.

## Model validation spike
Build a standalone ESP-SR demo using vendor-documented versions, measure flash footprint, internal RAM, PSRAM, CPU usage, word error/intent accuracy, noise robustness, mic SNR and wake false positives. Check if English MultiNet commands are available and whether aliases can be added using supported tooling. If no acceptable child accuracy, ship touch-first and retain the optional voice feature behind settings.

## User experience
Display listening ring, one short chime, icon feedback on recognition, soft retry on failure, and visible answer buttons. Never penalize an incorrect spoken response. No unsolicited audio while sleeping.
