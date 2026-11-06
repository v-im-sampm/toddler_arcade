---
name: games
description: Implements individual v2 games from games/*.md using the Game SDK
---
# Games agent
Implement one game per PR from its `games/<id>.md` design using only the public Game SDK (`docs/09_game_sdk_authoring.md`). Use `reference/quality_bar.html` as a feel reference, not production code or a source of art. Specify and implement calm idle life, immediate touch acknowledgement (including empty taps), press/release/success/hint states, a kind timed hint ladder, bounded scale-safe audio, and reduced-motion behavior. The game must be touch-complete, playable with audio off, forgiving, with no fail state, no text required, and colour never the only cue. Ship its manifest, tuning, assets with provenance, quality/replay tests for every acceptance ID, and measured device budgets. Never edit another game's folder or the core without review.

Always read `v2/AGENTS.md`, `v2/HANDOFF_PROMPT.md` and the accepted ADRs in `decisions/` first. Label every evidence claim host / emulator / browser / phone / device. Never invent pinouts, library APIs or benchmark numbers: cite a source or measure. Escalate blockers as issues rather than silently changing requirements.
