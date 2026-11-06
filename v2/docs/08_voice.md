# 08: Offline voice commands (all platforms)

Voice is a **supplement**. Every game is complete by touch. Voice must be offline, retain no audio,
and never leave the device. Recognition text never reaches game code; games receive **intent IDs**
only.

## 1. Intent manifest v2
`config/commands.json` (v1) remains the single source. v2 extends the schema in a
backward-compatible way:
```jsonc
{
  "version": 2,
  "activation": "push_to_talk",
  "locales": ["en-US"],
  "intents": [
    {
      "id": 8, "name": "RED",
      "contexts": ["colors"],
      "phrases": { "en-US": ["red"] },
      "min_confidence": { "default": 0.6 },          // tuned per recogniser in ADR-007
      "touch_equivalent": "the red/heart bubble"       // documentation: proves touch completeness
    }
  ]
}
```
New intents proposed for v2 games (add only if the recogniser tests well; otherwise touch-only):
`MORE`, `BIG`, `SMALL`, `FAST`, `SLOW`, `HELLO`, `SLEEP`/`NIGHT_NIGHT`, `DANCE`, animal names
(`COW`, `DUCK`, …), shape names (`CIRCLE`, `SQUARE`, `TRIANGLE`), and emotion names (`HAPPY`, `SAD`,
`SLEEPY`, `SURPRISED`). Each needs a context, a touch equivalent, and a recogniser test result.
Generators emit:
- the intent enum for each language,
- grammar/keyword files per recogniser,
- the voice docs table,
- a test-set prompt sheet for evaluators.

## 2. Interaction model
1. The child taps the **Mic** button (top-right). A friendly "ears" animation and a soft rising
   two-note chime play. The music ducks.
2. Listen for up to 8 s, ending after 4 s of silence or a recognised intent (v1 `PushToTalkSession`).
3. On a recognised intent above threshold within the current context:
   - the matching on-screen element pulses (this links the word to the touch target),
   - then the action happens exactly as if tapped.
4. Low confidence or out of context: the ears tilt with a gentle "hmm?" sound, and the touch options
   glow. Never guess.
5. A 750 ms dedupe applies. Use a playback cooldown or AEC per spike 4.7.
- Wake word (always listening) is **out of scope** unless Phase 0 shows false triggers ≤ 1 / 30 min
  on representative audio. Even then it is parent-opt-in and off by default.

## 3. Per-platform engines (decide in ADR-007; verify all claims)
| Platform | Requirement | Leads |
|---|---|---|
| ESP32-S3 | Fits flash/PSRAM with games; ≤ 1.5 s p95; works with the shared-clock I2S | ESP-SR MultiNet (English command models; check current version and custom-command method), AFE (NS/VAD/AEC) |
| Web | WASM, offline, ≤ ~20 MB model, lazy-loaded only when the parent enables voice | sherpa-onnx KWS, Vosk grammar mode; verify licences and sizes |
| Android/iOS | On-device only, guaranteed; no network permission | Same WASM/native lib as web, or OS on-device recognisers only if verifiably offline |
**Forbidden:** the browser Web Speech API or any recogniser that may send audio to a server.

## 4. Privacy
- Audio frames live only in ring buffers for the duration of a listening session and are zeroed on
  session end.
- No transcripts, logs of recognised words, or audio dumps in release builds. Debug builds may log
  intent IDs and confidence only.
- Model files are bundled. Nothing is downloaded at runtime.
- The mic permission on phone/web is requested only after the parent enables voice or recording in
  the parent area, with a parent-facing explanation screen.

## 5. Evaluation protocol
- Test set: adult speakers (≥ 6, mixed accents) × each intent × quiet / TV-noise / kitchen-noise ×
  20 cm / 60 cm. Child speakers only with guardian consent; recordings are scored and **deleted**,
  and only aggregate numbers are kept.
- Metrics:
  - intent accuracy per context,
  - false accept on out-of-context and non-speech audio,
  - latency from the end of the utterance,
  - CPU, RAM and flash use.
- Targets: adults ≥ 90 % in a quiet room. Child results are reported without a pass threshold before
  the pilot (v1 policy).
