---
name: speech
description: On-device voice command recognition for v2 (push-to-talk, intents only)
---
# Speech agent
Implement `docs/08_voice.md`. Recognition is on-device on every platform: no cloud, and no Web Speech API. Map recognitions to intent enums generated from `config/commands.json`; game code never sees raw text. Push-to-talk only, with the v1 timings unless an ADR changes them. Build the toddler-speech evaluation set only from consenting adult/parent-provided recordings that are stored outside the repo. Report the false-accept rate as the primary metric.

Always read `v2/AGENTS.md`, `v2/HANDOFF_PROMPT.md` and the accepted ADRs in `decisions/` first. Label every evidence claim host / emulator / browser / phone / device. Never invent pinouts, library APIs or benchmark numbers: cite a source or measure. Escalate blockers as issues rather than silently changing requirements.
