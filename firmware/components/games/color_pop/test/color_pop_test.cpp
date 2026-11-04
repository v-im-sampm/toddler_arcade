#include <initializer_list>

#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::color_pop;
using little_arcade::testing::RecordingFeedback;

namespace {

struct Harness {
  Rng rng{42};
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  ColorPopGame game;
  uint32_t t = 0;

  explicit Harness(uint32_t seed = 42) : rng(seed) {}
  void enter_and_play() {
    game.enter(ctx);
    game.update(ColorPopGame::kIntroMs);
  }
  int target_index() const {
    for (std::size_t i = 0; i < game.bubble_count(); ++i) {
      if (game.bubble(i).color == game.target()) return static_cast<int>(i);
    }
    return -1;
  }
  int wrong_index() const {
    for (std::size_t i = 0; i < game.bubble_count(); ++i) {
      if (game.bubble(i).color != game.target()) return static_cast<int>(i);
    }
    return -1;
  }
  void tap_bubble(int i) {
    const Rect& r = game.bubble(static_cast<std::size_t>(i)).rect;
    game.handle(make_touch_event(static_cast<int16_t>(r.x + r.w / 2),
                                 static_cast<int16_t>(r.y + r.h / 2), t += 200));
  }
  void say(Intent intent) { game.handle(make_voice_event(intent, t += 200)); }
  void finish_celebration() { game.update(ColorPopGame::kCelebrateMs); }
};

Intent intent_for(Color c) {
  switch (c) {
    case Color::RED:
      return Intent::RED;
    case Color::BLUE:
      return Intent::BLUE;
    case Color::GREEN:
      return Intent::GREEN;
    case Color::YELLOW:
      return Intent::YELLOW;
  }
  return Intent::UNKNOWN;
}

void test_layout_is_toddler_safe() {
  for (std::size_t i = 0; i < ColorPopGame::kSlotCount; ++i) {
    const Rect& a = ColorPopGame::kSlots[i];
    EXPECT(a.on_screen() && a.large_enough());
    EXPECT(!a.overlaps(kHomeButton) && !a.overlaps(kMicButton));
    EXPECT(!a.overlaps(ColorPopGame::kTargetBadge));
    for (std::size_t j = i + 1; j < ColorPopGame::kSlotCount; ++j) {
      const Rect& b = ColorPopGame::kSlots[j];
      const Rect grown{static_cast<int16_t>(a.x - kMinSpacing), static_cast<int16_t>(a.y - kMinSpacing),
                       static_cast<int16_t>(a.w + 2 * kMinSpacing), static_cast<int16_t>(a.h + 2 * kMinSpacing)};
      EXPECT(!grown.overlaps(b));
    }
  }
  const Rect& badge = ColorPopGame::kTargetBadge;
  EXPECT(badge.on_screen() && badge.large_enough());
  EXPECT(!badge.overlaps(kHomeButton) && !badge.overlaps(kMicButton));
}

void test_intro_then_prompt() {
  Harness h;
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(h.game.bubble_count() == 2);
  EXPECT(!h.fb.any(Cue::PROMPT));
  h.game.update(ColorPopGame::kIntroMs - 1);
  EXPECT(h.game.phase() == Phase::INTRO);
  h.game.update(1);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.fb.last()->cue == Cue::PROMPT && h.fb.last()->variant == static_cast<uint8_t>(h.game.target()));
}

void test_intro_tap_skips_without_answering() {
  Harness h;
  h.game.enter(h.ctx);
  h.tap_bubble(h.target_index());
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.rounds_completed() == 0);
}

void test_round_invariants_and_progression() {
  Harness h;
  h.enter_and_play();
  for (int round = 0; round < 20; ++round) {
    const std::size_t n = h.game.bubble_count();
    EXPECT(n >= ColorPopGame::kMinBubbles && n <= ColorPopGame::kMaxBubbles);
    EXPECT(h.target_index() >= 0);  // target always on screen
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = i + 1; j < n; ++j) {
        EXPECT(h.game.bubble(i).color != h.game.bubble(j).color);
        EXPECT(!h.game.bubble(i).rect.overlaps(h.game.bubble(j).rect));
      }
    }
    const Color previous = h.game.target();
    h.tap_bubble(h.target_index());
    EXPECT(h.game.phase() == Phase::CELEBRATE);
    h.finish_celebration();
    EXPECT(h.game.phase() == Phase::PLAY);
    EXPECT(h.game.target() != previous);  // no immediate repeat
  }
  EXPECT(h.game.rounds_completed() == 20);
  EXPECT(h.game.bubble_count() == ColorPopGame::kMaxBubbles);
}

void test_positions_are_randomized() {
  bool layouts_differ = false;
  Harness first(1);
  first.enter_and_play();
  for (uint32_t seed = 2; seed < 20 && !layouts_differ; ++seed) {
    Harness other(seed);
    other.enter_and_play();
    for (std::size_t i = 0; i < first.game.bubble_count(); ++i) {
      if (first.game.bubble(i).rect.x != other.game.bubble(i).rect.x ||
          first.game.bubble(i).rect.y != other.game.bubble(i).rect.y) {
        layouts_differ = true;
      }
    }
  }
  EXPECT(layouts_differ);
}

void test_same_seed_is_deterministic() {
  Harness a(7), b(7);
  a.enter_and_play();
  b.enter_and_play();
  for (int round = 0; round < 5; ++round) {
    EXPECT(a.game.target() == b.game.target());
    EXPECT(a.game.bubble_count() == b.game.bubble_count());
    for (std::size_t i = 0; i < a.game.bubble_count(); ++i) {
      EXPECT(a.game.bubble(i).color == b.game.bubble(i).color);
      EXPECT(a.game.bubble(i).rect.x == b.game.bubble(i).rect.x);
    }
    a.tap_bubble(a.target_index());
    b.say(intent_for(b.game.target()));
    a.finish_celebration();
    b.finish_celebration();
  }
}

void test_voice_and_touch_equivalent() {
  Harness touch(9), voice(9);
  touch.enter_and_play();
  voice.enter_and_play();
  touch.tap_bubble(touch.target_index());
  voice.say(intent_for(voice.game.target()));
  EXPECT(touch.game.phase() == Phase::CELEBRATE && voice.game.phase() == Phase::CELEBRATE);
  EXPECT(touch.game.rounds_completed() == 1 && voice.game.rounds_completed() == 1);
  EXPECT(touch.fb.count_of(Cue::SUCCESS) == 1 && voice.fb.count_of(Cue::SUCCESS) == 1);
}

void test_wrong_answer_is_gentle() {
  Harness h;
  h.enter_and_play();
  const int wrong = h.wrong_index();
  h.tap_bubble(wrong);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.wiggling() == wrong);
  EXPECT(h.game.hint_active());
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 1);
  EXPECT(!h.game.bubble(static_cast<std::size_t>(wrong)).popped);
  h.game.update(ColorPopGame::kHintMs);
  EXPECT(h.game.wiggling() == -1 && !h.game.hint_active());
  h.tap_bubble(h.target_index());  // still winnable, nothing lost
  EXPECT(h.game.rounds_completed() == 1);
}

void test_colour_not_on_screen() {
  Harness h;
  h.enter_and_play();  // round 1 shows two colours
  Intent absent = Intent::UNKNOWN;
  for (Color c : {Color::RED, Color::BLUE, Color::GREEN, Color::YELLOW}) {
    bool shown = false;
    for (std::size_t i = 0; i < h.game.bubble_count(); ++i) shown |= h.game.bubble(i).color == c;
    if (!shown) absent = intent_for(c);
  }
  h.say(absent);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.hint_active());
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 1);
}

void test_repeated_input_during_celebration() {
  Harness h;
  h.enter_and_play();
  const int target = h.target_index();
  h.tap_bubble(target);
  h.tap_bubble(target);
  h.say(intent_for(h.game.target()));
  h.tap_bubble(h.wrong_index());
  EXPECT(h.game.rounds_completed() == 1);
  EXPECT(h.fb.count_of(Cue::SUCCESS) == 1 && h.fb.count_of(Cue::TRY_AGAIN) == 0);
}

void test_again_help_and_badge_replay_prompt() {
  Harness h;
  h.enter_and_play();
  const int prompts = h.fb.count_of(Cue::PROMPT);
  h.say(Intent::AGAIN);
  h.say(Intent::HELP);
  const Rect& b = ColorPopGame::kTargetBadge;
  h.game.handle(make_touch_event(static_cast<int16_t>(b.x + 10), static_cast<int16_t>(b.y + 10), h.t += 200));
  EXPECT(h.fb.count_of(Cue::PROMPT) == prompts + 3);
  EXPECT(h.game.hint_active() && h.game.rounds_completed() == 0);
}

void test_empty_tap_and_unrelated_intents_are_harmless() {
  Harness h;
  h.enter_and_play();
  h.game.handle(make_touch_event(310, 100, h.t += 200));  // right margin, no slot
  h.game.handle(make_touch_event(100, 150, h.t += 200));  // gap between slots
  h.say(Intent::GO);
  h.say(Intent::YES);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.rounds_completed() == 0);
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0);
}

void test_audio_disabled_keeps_visual_target() {
  Harness h;
  EXPECT(!h.ctx.audio_enabled);
  h.enter_and_play();
  // Every colour has its own pattern, so the badge/bubbles are readable without colour or audio.
  const Color all[] = {Color::RED, Color::BLUE, Color::GREEN, Color::YELLOW};
  for (std::size_t i = 0; i < kColorCount; ++i) {
    for (std::size_t j = i + 1; j < kColorCount; ++j) EXPECT(pattern_for(all[i]) != pattern_for(all[j]));
  }
  NullFeedback silent;
  Rng rng(3);
  GameContext quiet(rng, silent);
  ColorPopGame g;
  g.enter(quiet);
  g.update(ColorPopGame::kIntroMs);
  EXPECT(g.phase() == Phase::PLAY && g.bubble_count() == 2);
}

void test_navigation_mid_animation_and_reentry() {
  Harness h;
  h.enter_and_play();
  h.tap_bubble(h.target_index());
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  h.game.exit();  // Home pressed mid-celebration
  EXPECT(!h.game.active());
  h.game.update(5000);  // stray update after exit is harmless
  h.game.handle(make_voice_event(Intent::RED, 1));
  h.game.enter(h.ctx);  // fresh start, no saved profile
  EXPECT(h.game.phase() == Phase::INTRO && h.game.rounds_completed() == 0);
}

void test_through_scene_manager() {
  ColorPopGame game;
  GameEntry entries[] = {{GameId::COLOR_POP, Intent::COLORS, &game}};
  RecordingFeedback fb;
  SceneManager mgr(entries, 1, fb);
  mgr.handle(make_voice_event(Intent::COLORS, 100));
  EXPECT(game.active());
  mgr.update(ColorPopGame::kIntroMs);
  mgr.handle(make_voice_event(Intent::GO, 300));  // out of context: never reaches game
  EXPECT(fb.count_of(Cue::TRY_AGAIN) == 0);
  Intent say = Intent::UNKNOWN;
  switch (game.target()) {
    case Color::RED: say = Intent::RED; break;
    case Color::BLUE: say = Intent::BLUE; break;
    case Color::GREEN: say = Intent::GREEN; break;
    case Color::YELLOW: say = Intent::YELLOW; break;
  }
  mgr.handle(make_voice_event(say, 500));
  EXPECT(game.rounds_completed() == 1);
  mgr.handle(make_touch_event(10, 10, 900));  // Home
  EXPECT(!game.active() && mgr.mode() == SceneManager::Mode::MENU);
}

}  // namespace

int main() {
  test_layout_is_toddler_safe();
  test_intro_then_prompt();
  test_intro_tap_skips_without_answering();
  test_round_invariants_and_progression();
  test_positions_are_randomized();
  test_same_seed_is_deterministic();
  test_voice_and_touch_equivalent();
  test_wrong_answer_is_gentle();
  test_colour_not_on_screen();
  test_repeated_input_during_celebration();
  test_again_help_and_badge_replay_prompt();
  test_empty_tap_and_unrelated_intents_are_harmless();
  test_audio_disabled_keeps_visual_target();
  test_navigation_mid_animation_and_reentry();
  test_through_scene_manager();
  return little_arcade::testing::finish("color pop tests");
}
