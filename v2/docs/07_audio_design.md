# 07: Audio design system

## 1. Sonic identity
- **Feel:** warm, playful, acoustic-toy timbres: marimba, kalimba, glockenspiel, ukulele pluck,
  soft synth pads, felt-hammer piano, hand percussion (shaker, woodblock, bongo), and whistles.
  Nothing harsh, distorted or alarm-like. No buzzers.
- **Everything is in tune:** each scene declares `key`, `scale` and `tempo`. Interaction sounds use
  scale degrees, not fixed pitches, so a toddler mashing buttons hears a melody. Default scale:
  major pentatonic. Sleepy or calm scenes may use Lydian or major with soft voicings.
- **One family across games:** a shared **"Toy Box" instrument kit** plus per-game signature
  instruments. The launcher theme motif (4–6 notes) is re-orchestrated in each game's intro sting.

| Game | Key / tempo (start points) | Signature timbres | Ambient bed |
|---|---|---|---|
| Color Pop | C major pent., 104 BPM | bubble plinks, marimba | gentle waves |
| Hatch-a-Pet | F major pent., 92 BPM | woodblock knocks, celesta | nest birds, soft breeze |
| Rainbow Piano | C major pent. (keys = C D E G A) | bells / flute / drum + new: marimba, toy piano | none (instrument focus) |
| Robo Buddy | G major pent., 100 BPM | FM blips, kalimba | workshop hum (very soft) |
| Shape Garage | D major pent., 96 BPM | clunks tuned to scale, ratchet clicks | workshop |
| Animal Sounds | A major pent., 90 BPM | ukulele, whistles | farmyard (no animal calls in the bed, to avoid confusion) |
| Feed the Monster | E♭ major pent., 100 BPM | tuba-ish pluck bass, crunches | kitchen |
| Sound Jar | key follows the pad roots (−12, −5, 0, +7, +12 semitones) | the child's sound | cave drip |
| Beat Sharks | C major pent., 60 / 80 / 107 BPM (turtle / walk / rabbit; v1 eighth-note steps) | drums + taps + recorded voice | lagoon |
| Rocket Rescue | B♭ Lydian, 84 BPM | pads, glock, whooshes | space shimmer |
| Silly Faces | G major pent., 110 BPM | boings, slide whistle, kazoo-like FM | pond |
| Bubble Aquarium | E major pent., 70 BPM | soft pads, bubble plinks | reef |

(Beat Sharks v1 step intervals of 500/375/280 ms equal eighth notes at 60/80/107 BPM. Keep them
toddler-slow, and derive step times from the music clock instead of hard-coded milliseconds.)

## 2. Music system
- **Adaptive layers** per game: `bed` (ambience), `low` (pad/bass), `mid` (rhythm), `high`
  (melody/sparkle).
  - Engagement (recent interactions) fades layers in over 2–4 bars.
  - Idle fades them back to `bed` + `low`.
- **Stingers** quantised to the next beat (or the next 1/8 for responsiveness ≤ 120 ms) for success
  and celebration.
- **Implementation (choose in ADR-006):**
  - (a) a procedural sequencer driving the synth/sample voices: tiny flash, fully key-aware;
  - (b) pre-rendered stems: higher fidelity, more flash, harder to re-key;
  - (c) hybrid, with procedural rhythm/melody over short looped pad samples.
- On the device, the music must fit the audio CPU budget (PF-9). Music may be thinner on the device
  (fewer layers) than on web/phone, but it must use the same composition data.

## 3. Interaction sound rules
| Rule | Detail |
|---|---|
| Latency | Touch → onset ≤ 50 ms (PF-4). Don't quantise direct-touch sounds (feels laggy); quantise only stingers/loops |
| Envelope | Attack ≥ 3 ms (v1 `kMinEdgeMs`), release ≥ 10 ms; no clicks on interrupt (fast 5 ms fade) |
| Variation | Every frequent sound has ≥ 3 variants or ±1 scale-degree / ±3 % pitch jitter (seeded) to avoid "machine-gun" repetition |
| Polyphony | Per-cue voice limits + stealing (oldest/quietest); the same cue retriggered within 40 ms is merged |
| Length | Taps ≤ 150 ms; success ≤ 600 ms; celebration sting ≤ 2 s |
| Meaning | Correct = rising interval; hint = soft neutral two-note (never a descending "wrong" trope); new thing = sparkle |
| Visual pair | Every cue has a visual counterpart (AU-5) |

## 4. Narrator (spoken prompts)
- Persona: one warm, friendly adult voice, slightly slow (≈ 140 words/min), clear consonants, and
  smiling tone. English (en-US) first. The structure is localisable (IDs, not text in code).
- Script per game (see each game doc's "Narration" table). Keep each prompt ≤ 2 s. Use praise
  variants (≥ 6, shuffle-bag) and hint variants (≥ 3).
- Source options (ADR-006/008):
  - (a) a recorded voice actor with a written licence/release covering the product;
  - (b) **offline TTS at build time**, never at runtime on the device, with a voice model whose
    licence allows commercial redistribution (verify per voice: many open TTS voices carry
    restrictive or non-commercial dataset licences).
  Record provenance either way.
- Processing chain at build: high-pass 80 Hz, de-ess, gentle compression, normalise to the voice
  bus target, trim, 5 ms fades, and resample to the device rate.

## 5. Mixing, loudness and safety
| Bus | Build-time normalisation target (start point; tune on device) | Runtime |
|---|---|---|
| Voice (narrator) | −18 LUFS-I (short clips: −16 LUFS-S max) | Ducks music/ambience by 8 dB, 60 ms attack, 300 ms release |
| SFX | Peak −9 dBFS, short-term −20 LUFS-S typical | Per-cue gain, voice limits |
| Music | −24 LUFS-I | Adaptive layers |
| Ambience | −30 LUFS-I | Very low, filtered |
| Sampler | Normalised to −6 dBFS peak (v1), then bus gain −6 dB | Privacy-wiped |
- **Master:** a speaker-compensation EQ (device only: high-pass ≈ 200–300 Hz to protect the 1 W
  driver and save headroom; measure the response) → look-ahead limiter (v1) → ceiling from
  `amp_safe_peak(amp_gain_db, amp_supply_mv)`.
- **Volume:** parent levels 0–6 (v1: 4 dB steps, 6 = unity). Default 0 (muted) on the device.
  Ramp ≥ 300 ms.
- **Onset guard:** no sound may start more than `audio.max_onset_jump_db` (token, start 12 dB) above
  the current master short-term level within 50 ms. The limiter look-ahead enforces this.
- **Acoustic verification (device):** measure the A-weighted SPL at 25 cm and 50 cm at max volume
  with pink noise and with the loudest game. Check the result against applicable toy-safety acoustic
  limits (research the current **EN 71-1** and **ASTM F963** acoustic clauses and cite them in
  ADR-012). Owner bench task.
- **Phone/web:** respect OS volume. Never auto-raise. Start web audio only after the first touch
  (browser autoplay rules) with a ramp.

## 6. Device audio specifics
- The I2S bus is shared by the mic and amp (one BCLK/WS), so they have one sample rate. Options
  (ADR-012):
  - stay at 16 kHz with good anti-aliased synthesis;
  - or run 32 kHz (or 24/48) with a polyphase decimator for the mic → 16 kHz for speech and sampler.
  Measure CPU and quality.
- **No amp mute pin:** keep I2S clocks off until the audio engine is running and has written
  silence. Fade in. Measure the power-on pop (owner bench).
- **Echo:** the mic is near the speaker. Speech recognition either uses an AFE with AEC (needs a
  reference signal: the engine's output buffer) or listens only during quiet windows (v1: 1.5 s
  cooldown). Spike 4.7 decides.
- Use mono output (one 1 W speaker). Stereo assets are mixed down at build time.

## 7. Deliverables checklist (AUD tickets)
- [ ] Toy Box instrument kit (synth patches and/or samples) with licence/provenance
- [ ] Per-game music data + stingers + ambience
- [ ] Narrator script, recordings/TTS renders, and processing script
- [ ] Loudness normaliser in the asset pipeline with CI report
- [ ] Golden-audio tests (render N seconds of a seeded replay → compare spectral/loudness fingerprints, not bytes)
- [ ] Device SPL/pop/current report (owner bench)
