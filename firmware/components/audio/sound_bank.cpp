#include "little_arcade/audio/sound_bank.hpp"

#include "little_arcade/scene_manager.hpp"

namespace little_arcade::audio {
namespace {

// C-major pentatonic, C5 upward: any mix of these sounds consonant.
constexpr uint16_t kPenta[] = {523, 587, 659, 784, 880, 1047, 1175, 1319, 1568, 1760};
constexpr uint8_t kPentaCount = sizeof(kPenta) / sizeof(kPenta[0]);

constexpr Tone tone(Wave w, uint16_t f0, uint16_t f1, uint16_t ms, uint8_t level, uint8_t attack = 4,
                    uint8_t release = 20) {
  return Tone{w, f0, f1, ms, level, attack, release};
}
constexpr Tone rest(uint16_t ms) { return Tone{Wave::SINE, 0, 0, ms, 0, 0, 0}; }
constexpr Tone pluck(uint16_t hz, uint16_t ms, uint8_t level = 150) {
  return tone(Wave::TRIANGLE, hz, hz, ms, level, 3, static_cast<uint8_t>(ms > 120 ? 100 : ms * 2 / 3));
}
constexpr Tone bell(uint16_t hz, uint16_t ms, uint8_t level = 150) {
  return tone(Wave::SINE, hz, hz, ms, level, 4, static_cast<uint8_t>(ms > 150 ? 140 : ms * 3 / 4));
}

// ---- shared defaults ----------------------------------------------------

Sound tap_blip() { return Sound{}.add(tone(Wave::TRIANGLE, 950, 760, 40, 110, 2, 28)); }

Sound gentle_hmm() {  // TRY_AGAIN: soft, falling, never a buzzer
  return Sound{}.add(tone(Wave::SINE, 494, 440, 140, 120, 15, 60)).add(tone(Wave::SINE, 440, 392, 200, 110, 10, 120));
}

Sound sparkle_success(uint8_t variant) {
  const uint8_t k = variant % 3;
  return Sound{}.add(bell(kPenta[3 + k], 90)).add(bell(kPenta[5 + k], 200));
}

Sound celebrate_arpeggio() {
  return Sound{}
      .add(pluck(523, 90, 160))
      .add(pluck(659, 90, 160))
      .add(pluck(784, 90, 160))
      .add(pluck(1047, 120, 170))
      .add(rest(30))
      .add(bell(1319, 110, 130))
      .add(bell(1568, 320, 140));
}

Sound default_prompt() { return Sound{}.add(bell(659, 150, 140)).add(bell(784, 240, 140)); }

Sound default_note(uint8_t variant) { return Sound{}.add(pluck(kPenta[variant % kPentaCount], 220, 150)); }

Sound default_reaction(uint8_t variant) {
  const uint16_t base = static_cast<uint16_t>(500 + 70 * (variant % 8));
  return Sound{}
      .add(tone(Wave::SINE, base, static_cast<uint16_t>(base * 3 / 2), 110, 140, 8, 40))
      .add(tone(Wave::SINE, static_cast<uint16_t>(base * 5 / 4), static_cast<uint16_t>(base * 7 / 4), 140, 140, 8, 80));
}

Sound default_sound(Cue cue, uint8_t variant) {
  switch (cue) {
    case Cue::TAP: return tap_blip();
    case Cue::PROMPT: return default_prompt();
    case Cue::SUCCESS: return sparkle_success(variant);
    case Cue::TRY_AGAIN: return gentle_hmm();
    case Cue::CELEBRATE: return celebrate_arpeggio();
    case Cue::NOTE: return default_note(variant);
    case Cue::REACTION: return default_reaction(variant);
    case Cue::AMBIENT: break;  // no background loop yet: silence is calmer
  }
  return Sound{};
}

// ---- Color Pop ------------------------------------------------------------
// Each colour gets its own short motif so the prompt is learnable without
// speech (spoken prompts are a later asset task). Order: RED, BLUE, GREEN, YELLOW.

Sound color_prompt(uint8_t color) {
  Sound s;
  switch (color % 4) {
    case 0:  // red: warm low "bum-BUM"
      s.add(pluck(392, 130, 160)).add(rest(20)).add(pluck(523, 260, 170));
      break;
    case 1:  // blue: gentle falling
      s.add(bell(784, 150)).add(bell(659, 150)).add(bell(523, 260));
      break;
    case 2:  // green: hopping up
      s.add(pluck(523, 110)).add(pluck(659, 110)).add(pluck(784, 240));
      break;
    default:  // yellow: bright twinkle
      s.add(bell(1047, 100, 130)).add(bell(1319, 100, 130)).add(bell(1047, 100, 130)).add(bell(1568, 240, 130));
      break;
  }
  return s;
}

Sound bubble_pop(uint8_t color) {
  const uint16_t lo = static_cast<uint16_t>(520 + 90 * (color % 4));
  return Sound{}
      .add(tone(Wave::NOISE, 7000, 7000, 18, 90, 1, 12))
      .add(tone(Wave::SINE, lo, static_cast<uint16_t>(lo * 5 / 2), 90, 170, 2, 50))
      .add(bell(kPenta[4 + (color % 4)], 160, 120));
}

Sound color_pop_sound(Cue cue, uint8_t variant) {
  switch (cue) {
    case Cue::PROMPT: return color_prompt(variant);
    case Cue::SUCCESS: return bubble_pop(variant);
    case Cue::CELEBRATE: {  // fired together with SUCCESS: let the pop land first
      Sound s;
      s.add(rest(240));
      const Sound arp = celebrate_arpeggio();
      for (uint8_t i = 0; i < arp.count; ++i) s.add(arp.tones[i]);
      return s;
    }
    default: return default_sound(cue, variant);
  }
}

// ---- Rainbow Piano --------------------------------------------------------

constexpr uint16_t kPianoNotes[] = {523, 587, 659, 784, 880};  // C-major pentatonic

Sound piano_note(uint8_t variant) {
  const uint8_t key = variant % 5;
  const uint8_t instrument = variant / 5;
  const uint16_t hz = kPianoNotes[key];
  switch (instrument) {
    case 0:  // soft bell with a quiet, high shimmer
      return Sound{}.add(bell(hz, 220, 145)).add(bell(static_cast<uint16_t>(hz * 2), 70, 55));
    case 1:  // gentle sustained flute-like sine
      return Sound{}.add(tone(Wave::SINE, hz, static_cast<uint16_t>(hz + 12), 240, 110, 22, 100));
    default: {  // small pitched drum, descending softly rather than a loud thump
      const uint16_t low = static_cast<uint16_t>(hz / 4);
      return Sound{}.add(tone(Wave::TRIANGLE, low, static_cast<uint16_t>(low * 2 / 3), 115, 115, 3, 70))
          .add(tone(Wave::NOISE, 3200, 3200, 14, 45, 1, 10));
    }
  }
}

Sound piano_prompt() {
  return Sound{}.add(bell(659, 90, 100)).add(bell(784, 90, 105)).add(bell(880, 180, 110));
}

Sound piano_select(uint8_t instrument) {
  const uint16_t hz = static_cast<uint16_t>(659 + (instrument % 3) * 131);
  return Sound{}.add(tone(instrument == 2 ? Wave::TRIANGLE : Wave::SINE, hz, hz, 90, 85, 6, 55));
}

Sound piano_celebrate() {
  return Sound{}.add(bell(523, 90, 120)).add(bell(659, 90, 125)).add(bell(784, 100, 130)).add(bell(1047, 260, 135));
}

Sound rainbow_piano_sound(Cue cue, uint8_t variant) {
  switch (cue) {
    case Cue::PROMPT: return piano_prompt();
    case Cue::NOTE: return piano_note(variant % 15);
    case Cue::TAP: return piano_select(variant);
    case Cue::CELEBRATE: return piano_celebrate();
    default: return default_sound(cue, variant);
  }
}

// ---- Hatch-a-Pet ----------------------------------------------------------

Sound egg_prompt(uint8_t prompt) {
  if (prompt == 0) {  // TAP_THE_EGG: "knock knock"
    return Sound{}.add(pluck(330, 70, 170)).add(rest(90)).add(pluck(330, 70, 170)).add(rest(60)).add(bell(659, 200, 120));
  }
  // ANOTHER_EGG: curious rising "ooh?"
  return Sound{}.add(tone(Wave::SINE, 392, 523, 180, 140, 20, 40)).add(bell(659, 260, 130));
}

Sound egg_crack(uint8_t crack) {  // crack 1..N rises in pitch toward hatching
  const uint8_t k = crack > 0 ? static_cast<uint8_t>(crack - 1) : 0;
  return Sound{}
      .add(tone(Wave::NOISE, 3500, 3500, 22, 110, 1, 15))
      .add(pluck(kPenta[k % kPentaCount], 90, 160));
}

Sound hatch_fanfare(uint8_t creature) {
  const uint8_t k = creature % 5;
  return Sound{}
      .add(tone(Wave::SINE, 400, 1200, 160, 130, 10, 40))
      .add(rest(20))
      .add(bell(kPenta[k], 110, 150))
      .add(bell(kPenta[k + 2], 110, 150))
      .add(bell(kPenta[k + 4], 300, 150));
}

// Eight original creature "voices": BLOOP, FLUFFWING, SPROUTLING, ... in
// Creature enum order. Simple chirps/boings; no recognizable characters.
Sound creature_voice(uint8_t creature) {
  Sound s;
  switch (creature % 8) {
    case 0:  // bloop: bubbly low-high wobble
      s.add(tone(Wave::SINE, 300, 520, 90, 160, 6, 30)).add(tone(Wave::SINE, 360, 620, 110, 150, 6, 60));
      break;
    case 1:  // fluffwing: tiny fluttering tweets
      s.add(tone(Wave::SINE, 1400, 1900, 60, 120, 4, 30)).add(rest(30)).add(tone(Wave::SINE, 1500, 2100, 70, 120, 4, 40));
      break;
    case 2:  // sproutling: soft rising "hee"
      s.add(tone(Wave::TRIANGLE, 600, 900, 200, 140, 20, 80));
      break;
    case 3:  // boing
      s.add(tone(Wave::TRIANGLE, 220, 660, 80, 160, 3, 20)).add(tone(Wave::TRIANGLE, 660, 330, 160, 140, 3, 90));
      break;
    case 4:  // purr-ish low rumble
      s.add(tone(Wave::PULSE, 110, 120, 160, 70, 20, 60)).add(tone(Wave::PULSE, 120, 100, 160, 60, 10, 80));
      break;
    case 5:  // giggle: three quick hops
      s.add(pluck(880, 60)).add(pluck(988, 60)).add(pluck(1175, 110));
      break;
    case 6:  // sleepy yawn: slow falling
      s.add(tone(Wave::SINE, 700, 380, 360, 130, 40, 160));
      break;
    default:  // happy squeak
      s.add(tone(Wave::SINE, 1000, 1500, 90, 130, 5, 30)).add(tone(Wave::SINE, 1500, 1200, 90, 120, 5, 50));
      break;
  }
  return s;
}

Sound hatch_a_pet_sound(Cue cue, uint8_t variant) {
  switch (cue) {
    case Cue::PROMPT: return egg_prompt(variant);
    case Cue::NOTE: return egg_crack(variant);
    case Cue::SUCCESS: return hatch_fanfare(variant);
    case Cue::REACTION: return creature_voice(variant);
    case Cue::CELEBRATE:  // fired with the hatch fanfare: a soft sparkle after it
      return Sound{}.add(rest(720)).add(bell(1568, 90, 110)).add(bell(1319, 90, 110)).add(bell(2093, 260, 110));
    case Cue::TAP: return Sound{}.add(tone(Wave::TRIANGLE, 420, 380, 35, 120, 2, 25));  // soft "tok"
    default: return default_sound(cue, variant);
  }
}

Sound robo_prompt(uint8_t prompt) {
  if (prompt == 1) {
    return Sound{}.add(tone(Wave::SINE, 392, 523, 220, 95, 18, 110)).add(bell(659, 180, 95));
  }
  return Sound{}.add(pluck(523, 100, 105)).add(rest(35)).add(pluck(659, 100, 110)).add(bell(784, 190, 110));
}

Sound robo_reaction(uint8_t mood) {
  switch (mood) {
    case 1: return Sound{}.add(tone(Wave::SINE, 650, 980, 100, 105, 5, 35)).add(bell(1175, 120, 95));
    case 2: return Sound{}.add(pluck(784, 55, 100)).add(pluck(988, 55, 100)).add(bell(1175, 130, 95));
    case 3: return Sound{}.add(tone(Wave::TRIANGLE, 420, 560, 90, 90, 4, 45)).add(bell(659, 140, 90));
    case 4: return Sound{}.add(tone(Wave::SINE, 480, 340, 300, 75, 35, 150));
    default: return Sound{}.add(tone(Wave::SINE, 440, 780, 170, 95, 18, 80)).add(bell(1047, 120, 85));
  }
}

Sound robo_buddy_sound(Cue cue, uint8_t variant) {
  switch (cue) {
    case Cue::PROMPT: return robo_prompt(variant);
    case Cue::REACTION: return robo_reaction(variant);
    case Cue::NOTE: return Sound{}.add(pluck(kPenta[variant % 3], 90, 90));
    case Cue::CELEBRATE: return Sound{}.add(bell(659, 90, 95)).add(bell(784, 90, 100)).add(bell(1047, 180, 100));
    case Cue::AMBIENT: return Sound{}.add(tone(Wave::SINE, 320, 280, 220, 45, 50, 140));
    case Cue::TAP: return Sound{}.add(tone(Wave::TRIANGLE, 700, 620, 35, 85, 2, 24));
    default: return default_sound(cue, variant);
  }
}

Sound shape_prompt(uint8_t prompt) {
  if (prompt == 0) {
    return Sound{}.add(pluck(523, 90, 105)).add(rest(35)).add(pluck(659, 90, 110)).add(bell(784, 200, 105));
  }
  const uint16_t hz[] = {523, 659, 784};
  const uint16_t note = hz[(prompt - 1) % 3];
  return Sound{}.add(bell(note, 120, 110)).add(rest(35)).add(pluck(static_cast<uint16_t>(note * 3 / 2), 170, 105));
}

Sound shape_clunk(uint8_t shape) {
  constexpr uint16_t bases[] = {392, 440, 523};
  const uint16_t hz = bases[shape % 3];
  return Sound{}.add(tone(Wave::TRIANGLE, hz, static_cast<uint16_t>(hz * 3 / 2), 65, 95, 3, 35))
      .add(bell(static_cast<uint16_t>(hz * 2), 100, 75));
}

Sound shape_dance() {
  return Sound{}.add(pluck(523, 75, 95)).add(pluck(659, 75, 100)).add(pluck(784, 90, 100)).add(bell(1047, 210, 95));
}

Sound shape_garage_sound(Cue cue, uint8_t variant) {
  switch (cue) {
    case Cue::PROMPT: return shape_prompt(variant);
    case Cue::SUCCESS: return shape_clunk(variant);
    case Cue::CELEBRATE: return shape_dance();
    case Cue::TAP: return Sound{}.add(tone(Wave::TRIANGLE, 659, 523, 35, 85, 2, 24));
    default: return default_sound(cue, variant);
  }
}

// Animal Sounds calls are synthesized from gentle glides and short pauses.
// They are original sound effects, not samples or speech recordings.
Sound animal_call(uint8_t animal) {
  switch (animal % 8) {
    case 0:  // cow: warm, low two-part moo
      return Sound{}.add(tone(Wave::SINE, 150, 112, 220, 125, 18, 60))
          .add(rest(35))
          .add(tone(Wave::SINE, 178, 132, 235, 120, 16, 70));
    case 1:  // duck: three bright, soft quacks
      return Sound{}.add(tone(Wave::TRIANGLE, 920, 680, 95, 105, 5, 35))
          .add(rest(35))
          .add(tone(Wave::TRIANGLE, 880, 660, 105, 100, 5, 40))
          .add(rest(30))
          .add(tone(Wave::TRIANGLE, 980, 740, 100, 105, 5, 40));
    case 2:  // sheep: a mellow rising and falling baa
      return Sound{}.add(tone(Wave::SINE, 430, 560, 170, 115, 16, 55))
          .add(tone(Wave::SINE, 560, 385, 205, 110, 12, 75));
    case 3:  // pig: two round, nasal oinks
      return Sound{}.add(tone(Wave::TRIANGLE, 390, 345, 115, 105, 8, 40))
          .add(rest(40))
          .add(tone(Wave::TRIANGLE, 470, 370, 135, 105, 8, 55));
    case 4:  // dog: a friendly pair of soft woofs
      return Sound{}.add(tone(Wave::SINE, 360, 540, 95, 115, 5, 35))
          .add(rest(45))
          .add(tone(Wave::SINE, 420, 680, 105, 110, 5, 40))
          .add(tone(Wave::SINE, 680, 510, 100, 100, 7, 45));
    case 5:  // cat: two gentle, rounded mew glides
      return Sound{}.add(tone(Wave::SINE, 610, 840, 135, 110, 12, 40))
          .add(tone(Wave::SINE, 840, 520, 195, 110, 10, 70));
    case 6:  // frog: three short, low croaks
      return Sound{}.add(tone(Wave::TRIANGLE, 510, 405, 80, 105, 5, 30))
          .add(rest(35))
          .add(tone(Wave::TRIANGLE, 485, 390, 85, 100, 5, 35))
          .add(rest(35))
          .add(tone(Wave::TRIANGLE, 455, 365, 100, 100, 5, 40));
    default:  // owl: two soft, rounded hoots
      return Sound{}.add(tone(Wave::SINE, 610, 550, 210, 110, 18, 65))
          .add(rest(60))
          .add(tone(Wave::SINE, 590, 500, 245, 105, 18, 80));
  }
}

Sound animal_sounds_sound(Cue cue, uint8_t variant) {
  switch (cue) {
    case Cue::PROMPT: return Sound{}.add(bell(659, 120, 105)).add(rest(55)).add(bell(784, 200, 105));
    case Cue::NOTE: return animal_call(variant);
    default: return default_sound(cue, variant);
  }
}

}  // namespace

Sound sound_for(uint8_t scene, Cue cue, uint8_t variant) {
  switch (scene) {
    case static_cast<uint8_t>(GameId::COLOR_POP): return color_pop_sound(cue, variant);
    case static_cast<uint8_t>(GameId::ANIMAL_SOUNDS): return animal_sounds_sound(cue, variant);
    case static_cast<uint8_t>(GameId::HATCH_A_PET): return hatch_a_pet_sound(cue, variant);
    case static_cast<uint8_t>(GameId::RAINBOW_PIANO): return rainbow_piano_sound(cue, variant);
    case static_cast<uint8_t>(GameId::ROBO_BUDDY): return robo_buddy_sound(cue, variant);
    case static_cast<uint8_t>(GameId::SHAPE_GARAGE): return shape_garage_sound(cue, variant);
    default: return default_sound(cue, variant);
  }
}

Sound scene_change_sound(uint8_t from_scene, uint8_t to_scene) {
  if (from_scene == to_scene) return Sound{};
  if (to_scene == IFeedback::kSceneMenu) {  // back home: soft descending "whoop"
    return Sound{}.add(tone(Wave::SINE, 880, 440, 180, 120, 10, 80));
  }
  // launching a game: rising "whoosh" + ding
  return Sound{}.add(tone(Wave::SINE, 330, 990, 160, 120, 20, 30)).add(bell(1319, 220, 120));
}

}  // namespace little_arcade::audio
