#include "little_arcade/games/animal_sounds.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::animal_sounds;
using little_arcade::testing::RecordingFeedback;

namespace {

struct Harness {
  Rng rng{42};
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  AnimalSoundsGame game;
  uint32_t t = 0;

  explicit Harness(uint32_t seed = 42) : rng(seed) {}
  void enter_and_play() {
    game.enter(ctx);
    game.update(AnimalSoundsGame::kIntroMs);
    game.update(AnimalSoundsGame::kCallAfterCueMs);  // the call follows the prompt chime
  }
  int wrong_index() const {
    for (std::size_t i = 0; i < game.card_count(); ++i) {
      if (game.card(i).animal != game.target()) return static_cast<int>(i);
    }
    return -1;
  }
  void tap_rect(const Rect& r) {
    game.handle(make_touch_event(static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2),
                                 t += 200));
  }
  void tap_card(int i) { tap_rect(game.card(static_cast<std::size_t>(i)).rect); }
  void say(Intent intent) { game.handle(make_voice_event(intent, t += 200)); }
  void finish_celebration() { game.update(AnimalSoundsGame::kCelebrateMs); }
};

Rect grow(const Rect& a) {
  return Rect{static_cast<int16_t>(a.x - kMinSpacing), static_cast<int16_t>(a.y - kMinSpacing),
              static_cast<int16_t>(a.w + 2 * kMinSpacing), static_cast<int16_t>(a.h + 2 * kMinSpacing)};
}

void test_layout_is_toddler_safe() {
  for (std::size_t i = 0; i < AnimalSoundsGame::kSlotCount; ++i) {
    const Rect& a = AnimalSoundsGame::kSlots[i];
    EXPECT(a.on_screen() && a.large_enough());
    EXPECT(a.y >= kPlayArea.y && a.y + a.h <= kPlayArea.y + kPlayArea.h);
    EXPECT(!a.overlaps(kHomeButton) && !a.overlaps(kMicButton));
    EXPECT(!grow(a).overlaps(AnimalSoundsGame::kSoundButton));
    for (std::size_t j = i + 1; j < AnimalSoundsGame::kSlotCount; ++j) {
      EXPECT(!grow(a).overlaps(AnimalSoundsGame::kSlots[j]));
    }
  }
  const Rect& s = AnimalSoundsGame::kSoundButton;
  EXPECT(s.on_screen() && s.large_enough());
  EXPECT(!grow(s).overlaps(kHomeButton) && !grow(s).overlaps(kMicButton));
  EXPECT(s.x >= 64 && s.x + s.w <= 256);
}

void test_at_least_six_original_animals_with_distinct_clues() {
  EXPECT(kAnimalCount >= 6);
  for (std::size_t i = 0; i < kAnimalCount; ++i) {
    for (std::size_t j = i + 1; j < kAnimalCount; ++j) {
      EXPECT(clue_for(static_cast<Animal>(i)) != clue_for(static_cast<Animal>(j)));
    }
  }
}

void test_intro_then_prompt_and_sound() {
  Harness h;
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(h.game.card_count() == AnimalSoundsGame::kMinCards);
  EXPECT(!h.fb.any(Cue::PROMPT) && !h.fb.any(Cue::NOTE));
  h.game.update(AnimalSoundsGame::kIntroMs - 1);
  EXPECT(h.game.phase() == Phase::INTRO);
  h.game.update(1);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.fb.count_of(Cue::PROMPT) == 1);
  EXPECT(h.fb.last()->cue == Cue::PROMPT && !h.fb.any(Cue::NOTE));  // chime first, never overlapping
  EXPECT(h.game.sound_playing());
  h.game.update(AnimalSoundsGame::kCallAfterCueMs - 1);
  EXPECT(!h.fb.any(Cue::NOTE));
  h.game.update(1);
  EXPECT(h.fb.count_of(Cue::NOTE) == 1);
  EXPECT(h.fb.last()->cue == Cue::NOTE && h.fb.last()->variant == static_cast<uint8_t>(h.game.target()));
  EXPECT(h.game.sound_playing());
  h.game.update(AnimalSoundsGame::kSoundShowMs);
  EXPECT(!h.game.sound_playing() && h.game.phase() == Phase::PLAY);  // no timeout/failure
  h.game.update(60000);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.rounds_completed() == 0);
}

void test_intro_tap_skips_without_answering() {
  Harness h;
  h.game.enter(h.ctx);
  h.tap_card(h.game.target_index());
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.rounds_completed() == 0);
  h.game.update(AnimalSoundsGame::kCallAfterCueMs);
  EXPECT(h.fb.count_of(Cue::NOTE) == 1);
}

void test_round_invariants_progression_and_variety() {
  Harness h;
  h.enter_and_play();
  bool seen[kAnimalCount] = {};
  for (int round = 0; round < 40; ++round) {
    const std::size_t n = h.game.card_count();
    EXPECT(n >= AnimalSoundsGame::kMinCards && n <= AnimalSoundsGame::kMaxCards);
    EXPECT(h.game.target_index() >= 0);
    for (std::size_t i = 0; i < n; ++i) {
      seen[static_cast<std::size_t>(h.game.card(i).animal)] = true;
      for (std::size_t j = i + 1; j < n; ++j) {
        EXPECT(h.game.card(i).animal != h.game.card(j).animal);
        EXPECT(!h.game.card(i).rect.overlaps(h.game.card(j).rect));
      }
    }
    const Animal previous = h.game.target();
    const int chosen = h.game.target_index();
    h.tap_card(chosen);
    EXPECT(h.game.phase() == Phase::CELEBRATE && h.game.celebrating() == chosen);
    h.finish_celebration();
    EXPECT(h.game.phase() == Phase::PLAY);
    EXPECT(h.game.target() != previous);  // no immediate repeat
  }
  EXPECT(h.game.rounds_completed() == 40);
  EXPECT(h.game.card_count() == AnimalSoundsGame::kMaxCards);
  for (bool s : seen) EXPECT(s);  // every animal shows up
}

void test_positions_are_randomized() {
  bool differ = false;
  Harness first(1);
  first.enter_and_play();
  for (uint32_t seed = 2; seed < 20 && !differ; ++seed) {
    Harness other(seed);
    other.enter_and_play();
    for (std::size_t i = 0; i < first.game.card_count(); ++i) {
      if (first.game.card(i).rect.x != other.game.card(i).rect.x ||
          first.game.card(i).rect.y != other.game.card(i).rect.y ||
          first.game.card(i).animal != other.game.card(i).animal) {
        differ = true;
      }
    }
  }
  EXPECT(differ);
}

void test_same_seed_is_deterministic() {
  Harness a(7), b(7);
  a.enter_and_play();
  b.enter_and_play();
  for (int round = 0; round < 6; ++round) {
    EXPECT(a.game.target() == b.game.target());
    EXPECT(a.game.card_count() == b.game.card_count());
    for (std::size_t i = 0; i < a.game.card_count(); ++i) {
      EXPECT(a.game.card(i).animal == b.game.card(i).animal);
      EXPECT(a.game.card(i).rect.x == b.game.card(i).rect.x && a.game.card(i).rect.y == b.game.card(i).rect.y);
    }
    a.tap_card(a.game.target_index());
    b.tap_card(b.game.target_index());
    a.finish_celebration();
    b.finish_celebration();
  }
}

void test_replay_voice_and_touch_equivalent() {
  Harness touch(9), voice(9);
  touch.enter_and_play();
  voice.enter_and_play();
  touch.game.update(AnimalSoundsGame::kSoundShowMs);
  voice.game.update(AnimalSoundsGame::kSoundShowMs);
  const int notes = touch.fb.count_of(Cue::NOTE);
  touch.tap_rect(AnimalSoundsGame::kSoundButton);
  voice.say(Intent::AGAIN);
  EXPECT(touch.fb.count_of(Cue::NOTE) == notes + 1 && voice.fb.count_of(Cue::NOTE) == notes + 1);
  EXPECT(touch.fb.last()->variant == static_cast<uint8_t>(touch.game.target()));
  EXPECT(voice.fb.last()->variant == static_cast<uint8_t>(voice.game.target()));
  EXPECT(touch.game.sound_playing() && voice.game.sound_playing());
  EXPECT(touch.game.hint_active() && voice.game.hint_active());
  voice.say(Intent::HELP);
  EXPECT(voice.fb.count_of(Cue::NOTE) == notes + 2);
  EXPECT(touch.game.rounds_completed() == 0 && voice.game.rounds_completed() == 0);
  EXPECT(touch.game.phase() == Phase::PLAY && voice.game.phase() == Phase::PLAY);
}

void test_wrong_answer_is_gentle_and_retryable() {
  Harness h;
  h.enter_and_play();
  const int notes = h.fb.count_of(Cue::NOTE);
  const int wrong = h.wrong_index();
  h.tap_card(wrong);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.wiggling() == wrong);
  EXPECT(h.game.hint_active());
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 1);
  EXPECT(h.fb.last()->cue == Cue::TRY_AGAIN);  // the call waits for the soft hum
  h.game.update(AnimalSoundsGame::kCallAfterCueMs);
  EXPECT(h.fb.count_of(Cue::NOTE) == notes + 1);  // the sound replays
  h.tap_card(wrong);  // repeated wrong taps stay gentle
  EXPECT(h.game.phase() == Phase::PLAY && h.game.card_count() == AnimalSoundsGame::kMinCards);
  h.game.update(AnimalSoundsGame::kHintMs);
  EXPECT(h.game.wiggling() == -1 && !h.game.hint_active());
  h.tap_card(h.game.target_index());  // still winnable, nothing lost
  EXPECT(h.game.rounds_completed() == 1);
}

void test_repeated_input_during_celebration() {
  Harness h;
  h.enter_and_play();
  const int target = h.game.target_index();
  h.tap_card(target);
  h.tap_card(target);
  h.say(Intent::AGAIN);
  h.tap_card(h.wrong_index());
  h.tap_rect(AnimalSoundsGame::kSoundButton);
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  EXPECT(h.game.rounds_completed() == 1);
  EXPECT(h.fb.count_of(Cue::SUCCESS) == 1 && h.fb.count_of(Cue::TRY_AGAIN) == 0);
  EXPECT(h.fb.last()->cue == Cue::CELEBRATE);
}

void test_empty_tap_and_unrelated_intents_are_harmless() {
  Harness h;
  h.enter_and_play();
  h.game.handle(make_touch_event(108, 150, h.t += 200));  // gap between slots
  h.game.handle(make_touch_event(90, 30, h.t += 200));    // top bar, no control
  h.say(Intent::GO);
  h.say(Intent::YES);
  h.say(Intent::RED);
  h.say(Intent::NEXT);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.rounds_completed() == 0);
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0);
}

void test_audio_disabled_still_playable() {
  Harness h;
  EXPECT(!h.ctx.audio_enabled);
  NullFeedback silent;
  Rng rng(3);
  GameContext quiet(rng, silent);
  AnimalSoundsGame g;
  g.enter(quiet);
  g.update(AnimalSoundsGame::kIntroMs);
  EXPECT(g.phase() == Phase::PLAY && g.card_count() == 2);
  EXPECT(g.sound_playing());  // visual clue on the speaker button stands in for audio
  const Rect& r = g.card(static_cast<std::size_t>(g.target_index())).rect;
  g.handle(make_touch_event(static_cast<int16_t>(r.x + 5), static_cast<int16_t>(r.y + 5), 100));
  EXPECT(g.phase() == Phase::CELEBRATE && g.rounds_completed() == 1);
}

void test_navigation_mid_animation_and_reentry() {
  Harness h;
  h.enter_and_play();
  h.tap_card(h.game.target_index());
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  h.game.exit();  // Home pressed mid-celebration
  EXPECT(!h.game.active());
  const int cues = h.fb.count;
  h.game.update(5000);  // stray calls after exit are harmless
  h.game.handle(make_voice_event(Intent::AGAIN, 1));
  h.game.handle(make_touch_event(160, 100, 2));
  EXPECT(h.fb.count == cues);
  h.game.enter(h.ctx);  // fresh start, no saved profile
  EXPECT(h.game.phase() == Phase::INTRO && h.game.rounds_completed() == 0);
  EXPECT(h.game.card_count() == AnimalSoundsGame::kMinCards && !h.game.sound_playing());
  EXPECT(h.game.celebrating() == -1 && h.game.wiggling() == -1);
}

void test_through_scene_manager() {
  AnimalSoundsGame game;
  GameEntry entries[] = {{GameId::ANIMAL_SOUNDS, Intent::ANIMALS, &game}};
  RecordingFeedback fb;
  SceneManager mgr(entries, 1, fb);
  mgr.handle(make_voice_event(Intent::ANIMALS, 100));
  EXPECT(game.active());
  mgr.update(AnimalSoundsGame::kIntroMs);
  EXPECT(game.phase() == Phase::PLAY);
  const int notes = fb.count_of(Cue::NOTE);
  mgr.handle(make_voice_event(Intent::RED, 300));  // out of context: never reaches game
  mgr.handle(make_voice_event(Intent::GO, 350));
  EXPECT(fb.count_of(Cue::NOTE) == notes && fb.count_of(Cue::TRY_AGAIN) == 0);
  mgr.handle(make_voice_event(Intent::AGAIN, 400));  // Global: reaches game, plays at once
  EXPECT(fb.count_of(Cue::NOTE) == notes + 1);
  mgr.update(AnimalSoundsGame::kCallAfterCueMs);  // the pending first call is not doubled
  EXPECT(fb.count_of(Cue::NOTE) == notes + 1);
  const Rect& r = game.card(static_cast<std::size_t>(game.target_index())).rect;
  mgr.handle(make_touch_event(static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2), 600));
  EXPECT(game.rounds_completed() == 1);
  mgr.handle(make_touch_event(10, 10, 900));  // Home
  EXPECT(!game.active() && mgr.mode() == SceneManager::Mode::MENU);
}

}  // namespace

int main() {
  test_layout_is_toddler_safe();
  test_at_least_six_original_animals_with_distinct_clues();
  test_intro_then_prompt_and_sound();
  test_intro_tap_skips_without_answering();
  test_round_invariants_progression_and_variety();
  test_positions_are_randomized();
  test_same_seed_is_deterministic();
  test_replay_voice_and_touch_equivalent();
  test_wrong_answer_is_gentle_and_retryable();
  test_repeated_input_during_celebration();
  test_empty_tap_and_unrelated_intents_are_harmless();
  test_audio_disabled_still_playable();
  test_navigation_mid_animation_and_reentry();
  test_through_scene_manager();
  return little_arcade::testing::finish("animal sounds tests");
}
