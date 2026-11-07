#include "little_arcade/games/bubble_aquarium.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::bubble_aquarium;
using little_arcade::testing::RecordingFeedback;
using Game = BubbleAquariumGame;

namespace {

void advance_intro(Game& game) {
  game.update(Game::kMaxPhysicsMs);
  game.update(Game::kIntroMs - Game::kMaxPhysicsMs);
}

bool inside(const Rect& inner, const Rect& outer) {
  return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w &&
         inner.y + inner.h <= outer.y + outer.h;
}

bool spaced(const Rect& a, const Rect& b) {
  const Rect grown{static_cast<int16_t>(a.x - kMinSpacing), static_cast<int16_t>(a.y - kMinSpacing),
                   static_cast<int16_t>(a.w + 2 * kMinSpacing), static_cast<int16_t>(a.h + 2 * kMinSpacing)};
  return !grown.overlaps(b);
}

struct Harness {
  Rng rng;
  RecordingFeedback fb;
  GameContext ctx;
  Game game;
  uint32_t time_ms = 0;

  explicit Harness(uint32_t seed = 42) : rng(seed), ctx(rng, fb) {}

  void enter_and_play() {
    game.enter(ctx);
    advance_intro(game);
  }
  void tap(int16_t x, int16_t y) { game.handle(make_touch_event(x, y, time_ms += 200)); }
  void tap_rect(const Rect& rect) {
    tap(static_cast<int16_t>(rect.x + rect.w / 2), static_cast<int16_t>(rect.y + rect.h / 2));
  }
  void say(Intent intent) { game.handle(make_voice_event(intent, time_ms += 200)); }
  void run(uint32_t ms, uint32_t frame_ms = 33) {
    for (uint32_t elapsed = 0; elapsed < ms; elapsed += frame_ms) game.update(frame_ms);
  }
  bool find_clear_water(int16_t& x, int16_t& y) const {
    for (int16_t py = 68; py < kScreenHeight; py = static_cast<int16_t>(py + 4)) {
      for (int16_t px = 4; px < kScreenWidth; px = static_cast<int16_t>(px + 4)) {
        bool clear = true;
        for (std::size_t i = 0; i < Game::kFishCount; ++i) clear &= !game.fish(i).rect.contains(px, py);
        for (std::size_t i = 0; i < Game::kMaxBubbles; ++i) {
          const Bubble& bubble = game.bubble(i);
          if (!bubble.active) continue;
          const int dx = px - bubble.x;
          const int dy = py - bubble.y;
          clear &= dx * dx + dy * dy > (bubble.size + 6) * (bubble.size + 6);
        }
        if (clear && !game.visitor_rect().contains(px, py)) {
          x = px;
          y = py;
          return true;
        }
      }
    }
    return false;
  }
  bool feed_until_visitor() {
    const uint32_t previous_visitors = game.visitors_seen();
    for (std::size_t i = 0; i < Game::kFeedsPerVisitor; ++i) tap_rect(Game::kFoodButton);
    for (int frame = 0; frame < 1400; ++frame) {
      game.update(33);
      if (game.visitors_seen() > previous_visitors) return true;
    }
    return false;
  }
};

void test_layout_and_intro() {
  const Rect controls[] = {Game::kFoodButton, Game::kBedtimeButton, Game::kBubbleButton};
  for (const Rect& rect : controls) {
    EXPECT(rect.on_screen() && rect.large_enough());
    EXPECT(!rect.overlaps(kHomeButton) && !rect.overlaps(kMicButton));
    EXPECT(spaced(rect, kHomeButton) && spaced(rect, kMicButton));
    EXPECT(rect.y == 0);
  }
  EXPECT(spaced(Game::kFoodButton, Game::kBedtimeButton));
  EXPECT(spaced(Game::kBedtimeButton, Game::kBubbleButton));
  EXPECT(Game::kTank.on_screen() && Game::kTank.large_enough());
  EXPECT(Game::kTank.y == 64);
  EXPECT(Game::kBubbleHit * 2 >= kMinTarget);
  EXPECT(Game::kIntroMs == 1500);

  Harness h;
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO && !h.fb.any(Cue::PROMPT));
  h.game.update(Game::kMaxPhysicsMs);
  h.game.update(Game::kIntroMs - Game::kMaxPhysicsMs - 1);
  EXPECT(h.game.phase() == Phase::INTRO);
  h.game.update(1);
  EXPECT(h.game.phase() == Phase::SWIM);
  EXPECT(h.fb.count_of(Cue::PROMPT) == 1 && h.fb.count_of(Cue::AMBIENT) == 1);
  h.game.exit();

  Harness skip;
  skip.game.enter(skip.ctx);
  skip.tap(160, 140);
  EXPECT(skip.game.phase() == Phase::SWIM);
  EXPECT(skip.game.active_food() == 0 && skip.game.active_bubbles() == 0);
}

void test_fish_roam_and_touch_interactions() {
  Harness h;
  h.enter_and_play();
  for (std::size_t i = 0; i < Game::kFishCount; ++i) {
    EXPECT(h.game.fish(i).type == static_cast<FishType>(i));
    EXPECT(h.game.fish(i).rect.large_enough());
    EXPECT(inside(h.game.fish(i).rect, Game::kTank));
  }
  h.run(3000);
  for (std::size_t i = 0; i < Game::kFishCount; ++i) EXPECT(inside(h.game.fish(i).rect, Game::kTank));

  const Rect fish_rect = h.game.fish(0).rect;
  h.tap(static_cast<int16_t>(fish_rect.x + fish_rect.w / 2),
        static_cast<int16_t>(fish_rect.y + fish_rect.h / 2));
  EXPECT(h.game.fish(0).happy_ms > 0 || h.game.fish(0).mode == FishMode::HELLO);
  EXPECT(h.fb.last()->cue == Cue::REACTION);

  int16_t x = 0, y = 0;
  EXPECT(h.find_clear_water(x, y));
  h.tap(x, y);
  bool following = false;
  for (std::size_t i = 0; i < Game::kFishCount; ++i) following |= h.game.fish(i).mode == FishMode::FOLLOW;
  EXPECT(following);
}

void test_food_and_visitors() {
  Harness h;
  h.enter_and_play();
  h.tap_rect(Game::kFoodButton);
  EXPECT(h.game.active_food() == 1 && h.game.food_button_pulsing());
  EXPECT(h.game.food(0).x >= Game::kSpoutX - 8 && h.game.food(0).x <= Game::kSpoutX + 8);
  EXPECT(h.fb.last()->cue == Cue::TAP);
  EXPECT(h.feed_until_visitor());
  EXPECT(h.game.meals_eaten() == 3 && h.game.visitors_seen() == 1);
  EXPECT(h.game.visitor_visible() && h.game.phase() == Phase::SWIM);
  const int16_t x0 = h.game.visitor_rect().x;
  h.game.update(500);
  EXPECT(h.game.visitor_rect().x > x0);
  h.run(11000);
  EXPECT(!h.game.visitor_visible());

  for (uint8_t expected = 1; expected < kVisitorCount; ++expected) {
    EXPECT(h.feed_until_visitor());
    EXPECT(static_cast<uint8_t>(h.game.visitor()) == expected);
    EXPECT(h.game.visitors_seen() == static_cast<uint32_t>(expected + 1));
    h.run(11000);
  }
}

void test_bubbles_and_caps() {
  Harness h;
  h.enter_and_play();
  h.tap_rect(Game::kBubbleButton);
  EXPECT(h.game.bubble_button_pulsing());
  EXPECT(h.fb.last()->cue == Cue::NOTE && h.fb.last()->variant == 4);
  h.run(1500);
  EXPECT(h.game.active_bubbles() > 0 && h.game.active_bubbles() <= Game::kMaxBubbles);
  int index = -1;
  for (std::size_t i = 0; i < Game::kMaxBubbles; ++i) {
    if (h.game.bubble(i).active) {
      index = static_cast<int>(i);
      break;
    }
  }
  EXPECT(index >= 0);
  const Bubble& bubble = h.game.bubble(static_cast<std::size_t>(index));
  const int16_t x = bubble.x;
  const int16_t y = bubble.y;
  h.tap(x, y);
  EXPECT(h.game.active_bubbles() < Game::kMaxBubbles);
  EXPECT(h.fb.last()->cue == Cue::NOTE && h.fb.last()->variant == 3);

  for (int i = 0; i < 80; ++i) {
    h.tap_rect(Game::kBubbleButton);
    h.game.update(50);
    EXPECT(h.game.active_bubbles() <= Game::kMaxBubbles);
  }
  for (int i = 0; i < 20; ++i) h.tap_rect(Game::kFoodButton);
  EXPECT(h.game.active_food() <= Game::kMaxFood);
  static_assert(Game::kMaxBubbles == 8 && Game::kMaxFood == 6, "fixed pools");
  static_assert(sizeof(Game) < 2048, "scene state is bounded");
}

void test_bedtime_hints_and_voice() {
  Harness h;
  h.enter_and_play();
  h.tap_rect(Game::kBedtimeButton);
  EXPECT(h.game.bedtime());
  EXPECT(h.game.bedtime_button_pulsing());
  for (std::size_t i = 0; i < Game::kFishCount; ++i) EXPECT(h.game.fish(i).mode == FishMode::SLEEP);
  h.run(3000);
  EXPECT(h.game.bedtime_dim() > 0);
  EXPECT(h.game.hint_level() == 0);
  h.tap_rect(Game::kFoodButton);
  EXPECT(h.game.active_food() == 1);
  h.run(12000);
  EXPECT(h.game.meals_eaten() >= 1);
  EXPECT(h.game.fish(0).mode == FishMode::SLEEP || h.game.fish(1).mode == FishMode::SLEEP ||
         h.game.fish(2).mode == FishMode::SLEEP);
  h.tap_rect(Game::kBedtimeButton);
  EXPECT(!h.game.bedtime());
  h.run(Game::kHintVisualMs);
  EXPECT(h.game.hint_level() >= 1);
  h.run(Game::kHintSpokenMs);
  EXPECT(h.game.hint_level() == 2);
  h.say(Intent::AGAIN);
  EXPECT(h.fb.last()->cue == Cue::PROMPT);
  h.say(Intent::HELP);
  EXPECT(h.fb.last()->cue == Cue::PROMPT);
}

void test_touch_during_visitors_and_long_idle() {
  Harness h;
  h.enter_and_play();
  EXPECT(h.feed_until_visitor());
  const uint32_t visitors = h.game.visitors_seen();
  const Rect visitor = h.game.visitor_rect();
  h.tap(static_cast<int16_t>(visitor.x + visitor.w / 2), static_cast<int16_t>(visitor.y + visitor.h / 2));
  EXPECT(h.fb.last()->cue == Cue::REACTION);
  h.say(Intent::NEXT);
  EXPECT(h.game.visitors_seen() == visitors);
  h.run(10u * 60u * 1000u, 100);
  EXPECT(h.game.phase() == Phase::SWIM);
  EXPECT(!h.fb.any(Cue::TRY_AGAIN));
}

void test_audio_disabled_exit_and_reentry() {
  NullFeedback silent;
  Rng rng(3);
  GameContext quiet(rng, silent);
  Game game;
  game.enter(quiet);
  advance_intro(game);
  game.handle(make_touch_event(Game::kBubbleButton.x + 10, Game::kBubbleButton.y + 10, 100));
  EXPECT(game.active() && game.bubble_button_pulsing());
  game.exit();
  EXPECT(!game.active() && game.active_food() == 0 && game.active_bubbles() == 0);
  game.update(5000);
  game.handle(make_voice_event(Intent::AGAIN, 1));
  EXPECT(game.active_bubbles() == 0);
  game.enter(quiet);
  EXPECT(game.phase() == Phase::INTRO && game.meals_eaten() == 0 && game.visitors_seen() == 0);
}

void test_seed_replay_and_scene_manager() {
  Harness a(7), b(7);
  a.enter_and_play();
  b.enter_and_play();
  for (int i = 0; i < 300; ++i) {
    const int16_t x = static_cast<int16_t>(40 + (i * 37) % 240);
    const int16_t y = static_cast<int16_t>(70 + (i * 53) % 160);
    a.tap(x, y);
    b.tap(x, y);
    if (i % 20 == 0) {
      a.tap_rect(Game::kFoodButton);
      b.tap_rect(Game::kFoodButton);
    }
    if (i % 17 == 0) {
      a.tap_rect(Game::kBedtimeButton);
      b.tap_rect(Game::kBedtimeButton);
    }
    a.game.update(100);
    b.game.update(100);
  }
  EXPECT(a.game.meals_eaten() == b.game.meals_eaten());
  EXPECT(a.game.visitors_seen() == b.game.visitors_seen());
  EXPECT(a.game.visitor() == b.game.visitor());
  EXPECT(a.game.bedtime() == b.game.bedtime());
  for (std::size_t i = 0; i < Game::kFishCount; ++i) {
    EXPECT(a.game.fish(i).rect.x == b.game.fish(i).rect.x);
    EXPECT(a.game.fish(i).rect.y == b.game.fish(i).rect.y);
    EXPECT(a.game.fish(i).mode == b.game.fish(i).mode);
  }
  for (std::size_t i = 0; i < Game::kMaxBubbles; ++i) {
    EXPECT(a.game.bubble(i).active == b.game.bubble(i).active);
    EXPECT(a.game.bubble(i).x == b.game.bubble(i).x && a.game.bubble(i).y == b.game.bubble(i).y);
  }

  Game game;
  GameEntry entries[] = {{GameId::BUBBLE_AQUARIUM, Intent::FISH, &game}};
  RecordingFeedback fb;
  SceneManager manager(entries, 1, fb);
  manager.handle(make_voice_event(Intent::FISH, 100));
  EXPECT(game.active() && manager.mode() == SceneManager::Mode::PLAYING);
  manager.update(Game::kMaxPhysicsMs);
  manager.update(Game::kIntroMs - Game::kMaxPhysicsMs);
  const int cue_count = fb.count;
  manager.handle(make_voice_event(Intent::GO, 300));
  manager.handle(make_voice_event(Intent::AGAIN, 400));
  EXPECT(fb.count == cue_count + 1 && fb.last()->cue == Cue::PROMPT);
  manager.handle(make_touch_event(10, 10, 900));
  EXPECT(!game.active() && manager.mode() == SceneManager::Mode::MENU);
}

}  // namespace

int main() {
  test_layout_and_intro();
  test_fish_roam_and_touch_interactions();
  test_food_and_visitors();
  test_bubbles_and_caps();
  test_bedtime_hints_and_voice();
  test_touch_during_visitors_and_long_idle();
  test_audio_disabled_exit_and_reentry();
  test_seed_replay_and_scene_manager();
  return little_arcade::testing::finish("bubble aquarium v2 prototype tests");
}
