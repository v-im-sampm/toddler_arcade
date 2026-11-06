---
name: games
description: Implements individual v2 games from games/*.md using the Game SDK
---
# Games agent
Implement one game per PR from its `games/<id>.md` design using only the public Game SDK (`docs/09_game_sdk_authoring.md`). The game must be touch-complete, forgiving, with no fail state, no text required, and colour never the only cue. Ship its manifest, tuning, assets with provenance, replays for every acceptance ID, and measured device budgets. Never edit another game's folder or the core without review.

Always read `v2/AGENTS.md`, `v2/HANDOFF_PROMPT.md` and the accepted ADRs in `decisions/` first. Label every evidence claim host / emulator / browser / phone / device. Never invent pinouts, library APIs or benchmark numbers: cite a source or measure. Escalate blockers as issues rather than silently changing requirements.
