# Beat Sharks
**Pitch:** Fill a little ocean grid with sharks and a fin swims along playing their sounds in a loop. Tap again for a different sound.

## Core loop
The loop is always playing. Three rows (top = your sound, middle = taps, bottom = drums) × six columns. Tapping a cell cycles it: empty bubble → big shark → medium shark → small shark → empty. Each size is a different sound in that row (bigger = lower), so a toddler can explore by just tapping. A fin glides along the bottom and every shark in its column sounds. Fill every column for a happy wave. Full design: [`docs/prd/beat_sharks.md`](../docs/prd/beat_sharks.md).

## Input
Touch: 18 grid cells (48×52 px), Play/Stop, Record (or a sparkle that cycles built-in sounds when the mic is off or missing) and Tempo (turtle → walk → rabbit). Voice: AGAIN (restart the loop from the start) and HELP (prompt) only; voice never records, and the launcher tile has no spoken name. Always include a visible Home control and a large touch alternative to every supported voice action. Commands are mapped via `config/commands.json`, not parsed inside game code.

## Scene states
`INTRO -> PLAY -> RECORDING -> CELEBRATE -> PLAY`, plus `PAUSED` and `EXIT`. Never block the UI while audio plays. A starter beat (two big drum sharks) plays from the first second, so the screen is never silent or empty. Pausing (sleep, Home) stops the loop and it stays stopped on resume.

## Visual/audio requirements
320×240 landscape, >=48px primary targets, simple 2D sprites, no rapid flashing (the playing column is a soft tint that moves position only). Original shark artwork; nine short synthesized sounds (fallback boings, clap/woodblock/shaker, boom/tom/bongo) each shorter than the fastest step. The top row plays the one recorded clip at three pitches through the shared sampler. Output passes the existing limiter and volume cap.

## Privacy
Same rules as Sound Jar: one clip of at most 3 s lives only in RAM, never saved, logged or transmitted, and zeroed on exit, pause/sleep and parent-gate entry. The parent Mic toggle applies; with the mic off or missing, the record button becomes a sparkle that cycles built-in sounds.

## Acceptance criteria
Touch-complete with mic off and with audio off. No wrong patterns, scores or failure states; a too-quiet take keeps the previous sound and the record button wiggles gently. Handle rapid taps (a tap is never doubled by the fin arriving at that column), Home mid-recording, long frame stalls (the clock skips ahead without a burst of sounds) and voice input during the intro. Deterministic for tests; no per-frame allocations.

## Implementation task
`firmware/components/games/beat_sharks/` (logic, LVGL view, header-only art, host tests) on top of `ISampler` / `audio::Sampler`. Sounds are `sound_bank` NOTE variants `row * 3 + sound - 1`. Shell registration is in `arcade`; `little_arcade_sim --bench --game beat_sharks` runs a scripted session.
