#include "little_arcade/games/bubble_aquarium.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::bubble_aquarium;
using little_arcade::testing::RecordingFeedback;
using Game = BubbleAquariumGame;

namespace {

bool spaced(const Rect& a, const Rect& b) {
  const Rect grown{static_cast<int16_t>(a.x - kMinSpacing), static_cast<int16_t>(a.y - kMinSpacing),
                   static_cast<int16_t>(a.w + 2 * kMinSpacing), static_cast<int16_t>(a.h + 2 * kMinSpacing)};
  return !grown.overlaps(b);
}

bool inside(const Rect& inner, const Rect& outer) {
  return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w &&
         inner.y + inner.h <= outer.y + outer.h;
}

int note_count(const RecordingFeedback& fb, uint8_t variant) {
  int n = 0;
  for (int i = 0; i < fb.count && i < RecordingFeedback::kCapacity; ++i) {
    n += (fb.entries[i].cue == Cue::NOTE && fb.entries[i].variant == variant) ? 1 : 0;
  }
  return n;
}

struct Harness {
  Rng rng{42};
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  Game game;
  uint32_t t = 0;

  explicit Harness(uint32_t seed = 42) : rng(seed) {}
  void enter_and_play() {
    game.enter(ctx);
    game.update(Game::kIntroMs);
  }
  void tap(int16_t x, int16_t y) { game.handle(make_touch_event(x, y, t += 200)); }
  void tap_rect(const Rect& r) {
    tap(static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2));
  }
  void say(Intent intent) { game.handle(make_voice_event(intent, t += 200)); }
  void run(uint32_t ms, uint32_t frame = 33) {
    for (uint32_t done = 0; done < ms; done += frame) game.update(frame);
  }
  // A water point that is not on a fish and not on a bubble.
  bool water_point(int16_t& out_x, int16_t& out_y) const {
    // Scan bottom-up so a new bubble has room to rise before reaching the surface.
    for (int16_t y = kScreenHeight - 8; y > Game::kTank.y; y = static_cast<int16_t>(y - 6)) {
      for (int16_t x = 4; x < kScreenWidth; x = static_cast<int16_t>(x + 6)) {
        bool clear = true;
        for (std::size_t i = 0; i < Game::kFishCount; ++i) clear &= !game.fish(i).rect.contains(x, y);
        for (std::size_t i = 0; i < Game::kMaxBubbles; ++i) {
          const Bubble& b = game.bubble(i);
          if (!b.active) continue;
          const int dx = x - b.x;
          const int dy = y - b.y;
          clear &= !(dx >= -Game::kBubbleHit - 2 && dx <= Game::kBubbleHit + 2 &&
                     dy >= -Game::kBubbleHit - 2 && dy <= Game::kBubbleHit + 2);
        }
        if (clear) {
          out_x = x;
          out_y = y;
          return true;
        }
      }
    }
    return false;
  }
  // Drops food until a visitor arrives. Returns false if it never does.
  bool feed_until_visitor() {
    for (uint32_t i = 0; i < Game::kFeedsPerVisitor; ++i) tap_rect(Game::kFoodButton);
    for (int frames = 0; frames < 900; ++frames) {
      game.update(33);
      if (game.phase() == Phase::CELEBRATE) return true;
    }
    return false;
  }
};

void test_layout_is_toddler_safe() {
  const Rect controls[] = {Game::kFoodButton, Game::kBubbleButton};
  for (const Rect& r : controls) {
    EXPECT(r.on_screen() && r.large_enough());
    EXPECT(!r.overlaps(kHomeButton) && !r.overlaps(kMicButton));
    EXPECT(spaced(r, kHomeButton) && spaced(r, kMicButton));
    EXPECT(r.x >= 64 && r.x + r.w <= 256);  // optional top-bar zone
    EXPECT(spaced(r, Game::kTank));
  }
  EXPECT(spaced(Game::kFoodButton, Game::kBubbleButton));
  // The water (primary target) is the whole play area.
  EXPECT(Game::kTank.on_screen() && Game::kTank.large_enough());
  EXPECT(Game::kTank.y >= kPlayArea.y);
  EXPECT(!Game::kTank.overlaps(kHomeButton) && !Game::kTank.overlaps(kMicButton));
  for (std::size_t i = 0; i < kFishTypeCount; ++i) {
    EXPECT(Game::kFishW[i] >= kMinTarget && Game::kFishH[i] >= kMinTarget);
  }
  EXPECT(2 * Game::kBubbleHit >= kMinTarget);
  EXPECT(Game::kVisitorY >= Game::kTank.y && Game::kVisitorY + Game::kVisitorH <= kScreenHeight);
}

void test_intro_then_play() {
  Harness h;
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(!h.fb.any(Cue::PROMPT));
  h.game.update(Game::kIntroMs - 1);
  EXPECT(h.game.phase() == Phase::INTRO);
  h.game.update(1);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.fb.count_of(Cue::PROMPT) == 1 && h.fb.count_of(Cue::AMBIENT) == 1);
}

void test_intro_tap_skips_without_acting() {
  Harness h;
  h.game.enter(h.ctx);
  h.tap(160, 150);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.active_bubbles() == 0);
  EXPECT(!h.fb.any(Cue::NOTE));
}

void test_three_distinct_fish_types() {
  Harness h;
  h.enter_and_play();
  bool seen[kFishTypeCount] = {};
  for (std::size_t i = 0; i < Game::kFishCount; ++i) {
    const Fish& f = h.game.fish(i);
    seen[static_cast<std::size_t>(f.type)] = true;
    EXPECT(inside(f.rect, Game::kTank) && f.rect.large_enough());
  }
  EXPECT(seen[0] && seen[1] && seen[2]);
  // Shape (size/aspect) differs per type, so colour is not the only clue.
  for (std::size_t i = 0; i < kFishTypeCount; ++i) {
    for (std::size_t j = i + 1; j < kFishTypeCount; ++j) {
      EXPECT(Game::kFishW[i] != Game::kFishW[j] || Game::kFishH[i] != Game::kFishH[j]);
    }
  }
}

void test_tap_water_makes_bubble_that_rises_and_pops() {
  Harness h;
  h.enter_and_play();
  int16_t x = 0, y = 0;
  EXPECT(h.water_point(x, y));
  h.tap(x, y);
  EXPECT(h.game.active_bubbles() == 1);
  EXPECT(h.fb.last()->cue == Cue::NOTE && h.fb.last()->variant < 3);
  // Tapping the bubble again (nothing has moved yet) pops it.
  h.tap(x, y);
  EXPECT(h.game.active_bubbles() == 0);
  EXPECT(note_count(h.fb, 3) == 1);
  // A new bubble rises, then disappears at the surface.
  h.tap(x, y);
  EXPECT(h.game.active_bubbles() == 1);
  int idx = -1;
  for (std::size_t i = 0; i < Game::kMaxBubbles; ++i) {
    if (h.game.bubble(i).active) idx = static_cast<int>(i);
  }
  const int16_t start_y = h.game.bubble(static_cast<std::size_t>(idx)).y;
  h.game.update(500);
  EXPECT(h.game.bubble(static_cast<std::size_t>(idx)).active);
  EXPECT(h.game.bubble(static_cast<std::size_t>(idx)).y < start_y);  // rises
  h.run(6000);
  EXPECT(h.game.active_bubbles() == 0);
}

void test_tap_fish_reacts() {
  Harness h;
  h.enter_and_play();
  for (std::size_t i = 0; i < Game::kFishCount; ++i) {
    h.game.update(Game::kHappyMs);  // clear previous wiggles
    // Tap a point owned only by this fish.
    const Rect r = h.game.fish(i).rect;
    bool tapped = false;
    for (int16_t yy = r.y; yy < r.y + r.h && !tapped; yy = static_cast<int16_t>(yy + 4)) {
      for (int16_t xx = r.x; xx < r.x + r.w && !tapped; xx = static_cast<int16_t>(xx + 4)) {
        bool only = true;
        for (std::size_t j = 0; j < Game::kFishCount; ++j) only &= j == i || !h.game.fish(j).rect.contains(xx, yy);
        if (!only) continue;
        h.tap(xx, yy);
        tapped = true;
      }
    }
    if (!tapped) continue;  // fully hidden behind another fish this frame
    EXPECT(h.game.fish(i).happy_ms == Game::kHappyMs);
    EXPECT(h.fb.last()->cue == Cue::REACTION &&
           h.fb.last()->variant == static_cast<uint8_t>(h.game.fish(i).type));
  }
  EXPECT(h.game.active_bubbles() == 0);  // fish taps don't blow bubbles
}

void test_feeding_fish() {
  Harness h;
  h.enter_and_play();
  h.tap_rect(Game::kFoodButton);
  EXPECT(h.game.active_food() == 1 && h.game.food_button_pulsing());
  EXPECT(h.fb.last()->cue == Cue::TAP);
  h.run(15000);
  EXPECT(h.game.active_food() == 0);
  EXPECT(h.game.meals_eaten() == 1);
  EXPECT(h.fb.count_of(Cue::SUCCESS) == 1);
  uint32_t total = 0;
  for (std::size_t i = 0; i < Game::kFishCount; ++i) total += h.game.fish(i).meals;
  EXPECT(total == 1);
  EXPECT(h.game.phase() == Phase::PLAY);  // one meal is not yet a visitor
}

void test_surprise_visitor_after_meals() {
  Harness h;
  h.enter_and_play();
  EXPECT(h.feed_until_visitor());
  EXPECT(h.game.visitors_seen() == 1 && h.game.visitor_visible());
  EXPECT(h.fb.count_of(Cue::CELEBRATE) == 1);
  EXPECT(h.fb.last()->cue == Cue::CELEBRATE &&
         h.fb.last()->variant == static_cast<uint8_t>(h.game.visitor()));
  const int16_t x0 = h.game.visitor_rect().x;
  h.game.update(Game::kCelebrateMs / 2);
  EXPECT(h.game.visitor_rect().x > x0);  // swims across
  h.game.update(Game::kCelebrateMs / 2);
  EXPECT(h.game.phase() == Phase::PLAY && !h.game.visitor_visible());
  // Visitors never repeat back-to-back.
  Visitor previous = h.game.visitor();
  for (int i = 0; i < 6; ++i) {
    EXPECT(h.feed_until_visitor());
    EXPECT(h.game.visitor() != previous);
    previous = h.game.visitor();
    h.game.update(Game::kCelebrateMs);
  }
  EXPECT(h.game.visitors_seen() == 7);
  EXPECT(h.game.meals_eaten() == 21);
}

void test_pools_are_bounded() {
  Harness h;
  h.enter_and_play();
  // Toddler mash: many taps, food and bubble bursts with little time between.
  for (int i = 0; i < 400; ++i) {
    const int16_t x = static_cast<int16_t>(h.rng.below(kScreenWidth));
    const int16_t y = static_cast<int16_t>(h.rng.below(kScreenHeight));
    h.tap(x, y);
    if (i % 3 == 0) h.tap_rect(Game::kFoodButton);
    if (i % 5 == 0) h.say(Intent::AGAIN);
    if (i % 7 == 0) h.tap_rect(Game::kBubbleButton);
    h.game.update(16);
    EXPECT(h.game.active_bubbles() <= Game::kMaxBubbles);
    EXPECT(h.game.active_food() <= Game::kMaxFood);
  }
  // Long calm session: everything stays in the tank.
  for (int i = 0; i < 18000; ++i) {
    h.game.update(33);
    if (i % 600 == 0) h.tap_rect(Game::kFoodButton);
    for (std::size_t f = 0; f < Game::kFishCount; ++f) EXPECT(inside(h.game.fish(f).rect, Game::kTank));
  }
  EXPECT(h.game.active_bubbles() <= Game::kMaxBubbles);
  static_assert(Game::kMaxBubbles == 8 && Game::kMaxFood == 6, "fixed pools");
  static_assert(sizeof(Game) < 1024, "scene state is small and fixed-size");
}

void test_voice_again_matches_bubble_button() {
  Harness touch(9), voice(9);
  touch.enter_and_play();
  voice.enter_and_play();
  touch.tap_rect(Game::kBubbleButton);
  voice.say(Intent::AGAIN);
  EXPECT(touch.game.active_bubbles() == Game::kBurstBubbles);
  EXPECT(voice.game.active_bubbles() == Game::kBurstBubbles);
  EXPECT(touch.game.bubble_button_pulsing() && voice.game.bubble_button_pulsing());
  EXPECT(note_count(touch.fb, 4) == 1 && note_count(voice.fb, 4) == 1);
  for (std::size_t i = 0; i < Game::kMaxBubbles; ++i) {
    EXPECT(touch.game.bubble(i).active == voice.game.bubble(i).active);
    EXPECT(touch.game.bubble(i).x == voice.game.bubble(i).x && touch.game.bubble(i).y == voice.game.bubble(i).y);
  }
}

void test_input_during_celebration_and_no_fail() {
  Harness h;
  h.enter_and_play();
  EXPECT(h.feed_until_visitor());
  const uint32_t seen = h.game.visitors_seen();
  h.say(Intent::AGAIN);
  h.say(Intent::AGAIN);
  h.say(Intent::HELP);  // unused global intents are harmless
  h.say(Intent::NEXT);
  h.tap_rect(Game::kBubbleButton);
  h.tap(160, 200);
  h.tap(160, 200);
  EXPECT(h.game.phase() == Phase::CELEBRATE && h.game.visitors_seen() == seen);
  EXPECT(h.game.active_bubbles() > 0);
  h.game.update(Game::kCelebrateMs);
  EXPECT(h.game.phase() == Phase::PLAY);
  // Leaving the child alone for ten minutes never fails or nags.
  h.run(10u * 60u * 1000u, 100);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(!h.fb.any(Cue::TRY_AGAIN));
}

void test_empty_and_unrelated_input_is_harmless() {
  Harness h;
  h.enter_and_play();
  const int before = h.fb.count;
  h.tap(160, 30);  // top bar gap between the two buttons
  h.tap(30, 30);   // Home area (owned by the SceneManager)
  h.say(Intent::GO);
  h.say(Intent::RED);
  h.say(Intent::YES);
  EXPECT(h.fb.count == before);
  EXPECT(h.game.active_bubbles() == 0 && h.game.active_food() == 0);
  EXPECT(h.game.phase() == Phase::PLAY);
}

void test_audio_disabled_is_playable() {
  NullFeedback silent;
  Rng rng(3);
  GameContext quiet(rng, silent);
  EXPECT(!quiet.audio_enabled);
  Game g;
  g.enter(quiet);
  g.update(Game::kIntroMs);
  EXPECT(g.phase() == Phase::PLAY);
  g.handle(make_touch_event(Game::kBubbleButton.x + 10, Game::kBubbleButton.y + 10, 100));
  EXPECT(g.active_bubbles() == Game::kBurstBubbles && g.bubble_button_pulsing());  // visual cue
  for (int i = 0; i < 3; ++i) {
    g.handle(make_touch_event(Game::kFoodButton.x + 10, Game::kFoodButton.y + 10, 300u + 200u * static_cast<uint32_t>(i)));
  }
  for (int i = 0; i < 900 && g.phase() != Phase::CELEBRATE; ++i) g.update(33);
  EXPECT(g.phase() == Phase::CELEBRATE && g.visitor_visible());
  EXPECT(g.meals_eaten() == 3);
}

void test_navigation_mid_animation_and_reentry() {
  Harness h;
  h.enter_and_play();
  h.say(Intent::AGAIN);
  EXPECT(h.feed_until_visitor());
  h.tap_rect(Game::kFoodButton);
  h.game.exit();  // Home pressed mid-celebration
  EXPECT(!h.game.active());
  EXPECT(h.game.active_bubbles() == 0 && h.game.active_food() == 0);
  const int cues = h.fb.count;
  h.game.update(5000);  // stray calls after exit are harmless
  h.game.handle(make_voice_event(Intent::AGAIN, 1));
  h.game.handle(make_touch_event(160, 150, 2));
  EXPECT(h.fb.count == cues && h.game.active_bubbles() == 0);
  h.game.enter(h.ctx);  // fresh start, no saved profile
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(h.game.meals_eaten() == 0 && h.game.visitors_seen() == 0);
  EXPECT(h.game.active_bubbles() == 0 && h.game.active_food() == 0);
  h.game.update(Game::kIntroMs);
  EXPECT(h.game.phase() == Phase::PLAY);
}

void test_same_seed_is_deterministic() {
  Harness a(7), b(7);
  a.enter_and_play();
  b.enter_and_play();
  for (int i = 0; i < 300; ++i) {
    const int16_t x = static_cast<int16_t>(20 + (i * 37) % 280);
    const int16_t y = static_cast<int16_t>(70 + (i * 53) % 160);
    a.tap(x, y);
    b.tap(x, y);
    if (i % 20 == 0) {
      a.tap_rect(Game::kFoodButton);
      b.tap_rect(Game::kFoodButton);
    }
    a.game.update(100);
    b.game.update(100);
  }
  EXPECT(a.game.phase() == b.game.phase());
  EXPECT(a.game.meals_eaten() == b.game.meals_eaten() && a.game.visitors_seen() == b.game.visitors_seen());
  EXPECT(a.game.visitor() == b.game.visitor());
  for (std::size_t i = 0; i < Game::kFishCount; ++i) {
    EXPECT(a.game.fish(i).rect.x == b.game.fish(i).rect.x && a.game.fish(i).rect.y == b.game.fish(i).rect.y);
  }
  for (std::size_t i = 0; i < Game::kMaxBubbles; ++i) {
    EXPECT(a.game.bubble(i).active == b.game.bubble(i).active && a.game.bubble(i).y == b.game.bubble(i).y);
  }
  EXPECT(a.fb.count == b.fb.count);
  // A different seed gives a different starting tank.
  Harness c(8);
  c.enter_and_play();
  Harness d(7);
  d.enter_and_play();
  bool differs = false;
  for (std::size_t i = 0; i < Game::kFishCount; ++i) differs |= c.game.fish(i).rect.x != d.game.fish(i).rect.x;
  EXPECT(differs);
}

void test_through_scene_manager() {
  Game game;
  GameEntry entries[] = {{GameId::BUBBLE_AQUARIUM, Intent::FISH, &game}};
  RecordingFeedback fb;
  SceneManager mgr(entries, 1, fb);
  mgr.handle(make_voice_event(Intent::FISH, 100));
  EXPECT(game.active() && mgr.mode() == SceneManager::Mode::PLAYING);
  mgr.update(Game::kIntroMs);
  EXPECT(game.phase() == Phase::PLAY);
  const int cues = fb.count;
  mgr.handle(make_voice_event(Intent::GO, 300));   // out of context: never reaches game
  mgr.handle(make_voice_event(Intent::RED, 350));  // out of context
  EXPECT(fb.count == cues && game.active_bubbles() == 0);
  mgr.handle(make_voice_event(Intent::AGAIN, 400));  // Global intent reaches the game
  EXPECT(game.active_bubbles() == Game::kBurstBubbles);
  mgr.handle(make_touch_event(Game::kFoodButton.x + 20, Game::kFoodButton.y + 20, 600));
  EXPECT(game.active_food() == 1);
  mgr.handle(make_touch_event(10, 10, 900));  // Home
  EXPECT(!game.active() && mgr.mode() == SceneManager::Mode::MENU);
}

}  // namespace

int main() {
  test_layout_is_toddler_safe();
  test_intro_then_play();
  test_intro_tap_skips_without_acting();
  test_three_distinct_fish_types();
  test_tap_water_makes_bubble_that_rises_and_pops();
  test_tap_fish_reacts();
  test_feeding_fish();
  test_surprise_visitor_after_meals();
  test_pools_are_bounded();
  test_voice_again_matches_bubble_button();
  test_input_during_celebration_and_no_fail();
  test_empty_and_unrelated_input_is_harmless();
  test_audio_disabled_is_playable();
  test_navigation_mid_animation_and_reentry();
  test_same_seed_is_deterministic();
  test_through_scene_manager();
  return little_arcade::testing::finish("bubble aquarium tests");
}
