#include <initializer_list>

#include "little_arcade/games/feed_monster.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::feed_monster;
using little_arcade::testing::RecordingFeedback;

namespace {

using G = FeedMonsterGame;

struct Harness {
  Rng rng{42};
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  FeedMonsterGame game;
  uint32_t t = 0;

  explicit Harness(uint32_t seed = 42) : rng(seed) {}
  void enter_and_play() {
    game.enter(ctx);
    game.update(G::kIntroMs);
  }
  void tap(const Rect& r) {
    game.handle(make_touch_event(static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2),
                                 t += 200));
  }
  void tap_food() { tap(G::kFoodBowl); }
  void tap_number(uint8_t n) { tap(G::kNumberButtons[n - 1]); }
  void say(Intent intent) { game.handle(make_voice_event(intent, t += 200)); }
  void feed_all_by_taps() {
    const uint8_t n = game.requested();
    for (uint8_t i = 0; i < n; ++i) tap_food();
  }
  void finish_celebration() { game.update(G::kCelebrateMs); }
};

Intent intent_for(uint8_t n) {
  switch (n) {
    case 1:
      return Intent::ONE;
    case 2:
      return Intent::TWO;
    case 3:
      return Intent::THREE;
    default:
      return Intent::UNKNOWN;
  }
}

Rect grow(const Rect& a) {
  return Rect{static_cast<int16_t>(a.x - kMinSpacing), static_cast<int16_t>(a.y - kMinSpacing),
              static_cast<int16_t>(a.w + 2 * kMinSpacing), static_cast<int16_t>(a.h + 2 * kMinSpacing)};
}

void test_layout_is_toddler_safe() {
  const Rect all[] = {G::kFoodBowl, G::kMonster, G::kNumberButtons[0], G::kNumberButtons[1],
                      G::kNumberButtons[2], G::kRequestBadge};
  const std::size_t n = sizeof(all) / sizeof(all[0]);
  for (std::size_t i = 0; i < n; ++i) {
    const Rect& a = all[i];
    EXPECT(a.on_screen() && a.large_enough());
    EXPECT(!a.overlaps(kHomeButton) && !a.overlaps(kMicButton));
    for (std::size_t j = i + 1; j < n; ++j) EXPECT(!grow(a).overlaps(all[j]));
  }
  // Primary targets sit in the play area; the badge sits in the top bar between Home and Mic.
  for (std::size_t i = 0; i + 1 < n; ++i) EXPECT(all[i].y >= kPlayArea.y);
  EXPECT(G::kRequestBadge.x >= 64 && G::kRequestBadge.x + G::kRequestBadge.w <= 256);
}

void test_intro_then_prompt() {
  Harness h;
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(h.game.fed() == 0);
  EXPECT(!h.fb.any(Cue::PROMPT));
  h.game.update(G::kIntroMs - 1);
  EXPECT(h.game.phase() == Phase::INTRO);
  h.game.update(1);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.fb.last()->cue == Cue::PROMPT && h.fb.last()->variant == h.game.requested());
}

void test_intro_tap_skips_without_feeding() {
  Harness h;
  h.game.enter(h.ctx);
  h.tap_food();
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.fed() == 0 && h.game.rounds_completed() == 0);
}

void test_counts_one_to_three_with_variety() {
  Harness h;
  h.enter_and_play();
  bool seen[4] = {false, false, false, false};
  for (int round = 0; round < 30; ++round) {
    const uint8_t n = h.game.requested();
    EXPECT(n >= G::kMinRequest && n <= G::kMaxRequest);
    if (n >= 1 && n <= 3) seen[n] = true;
    // Visual count grows one dot per snack, and each snack is counted aloud.
    for (uint8_t i = 1; i <= n; ++i) {
      h.fb.clear();
      h.tap_food();
      EXPECT(h.game.fed() == i);
      EXPECT(h.fb.count_of(Cue::SUCCESS) == 1);
      EXPECT(h.fb.entries[1].cue == Cue::SUCCESS && h.fb.entries[1].variant == i);
      EXPECT(h.game.phase() == (i < n ? Phase::PLAY : Phase::CELEBRATE));
    }
    EXPECT(h.fb.any(Cue::CELEBRATE));
    h.finish_celebration();
    EXPECT(h.game.phase() == Phase::PLAY && h.game.fed() == 0);
    EXPECT(h.game.requested() != n);  // no immediate repeat
  }
  EXPECT(seen[1] && seen[2] && seen[3]);
  EXPECT(h.game.rounds_completed() == 30);
}

void test_excess_taps_never_penalised() {
  Harness h;
  h.enter_and_play();
  h.feed_all_by_taps();
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  for (int i = 0; i < 10; ++i) h.tap_food();
  EXPECT(h.game.rounds_completed() == 1);
  EXPECT(h.game.fed() == h.game.requested());  // visual count stays correct
  EXPECT(h.game.extra_snacks() == 10 && h.game.giggling());
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0);
  EXPECT(h.fb.count_of(Cue::CELEBRATE) == 1);
  h.finish_celebration();
  EXPECT(h.game.phase() == Phase::PLAY && h.game.extra_snacks() == 0);
}

void test_voice_and_touch_equivalent() {
  Harness tap_food(9), tap_num(9), voice(9);
  tap_food.enter_and_play();
  tap_num.enter_and_play();
  voice.enter_and_play();
  const uint8_t n = voice.game.requested();
  tap_food.feed_all_by_taps();
  tap_num.tap_number(n);
  voice.say(intent_for(n));
  for (Harness* h : {&tap_food, &tap_num, &voice}) {
    EXPECT(h->game.phase() == Phase::CELEBRATE && h->game.rounds_completed() == 1);
    EXPECT(h->game.fed() == n);
    EXPECT(h->fb.count_of(Cue::SUCCESS) == n);
    EXPECT(h->fb.count_of(Cue::CELEBRATE) == 1);
  }
}

void test_counting_along_by_voice() {
  // Find a seed whose first request is 3, then count ONE, TWO, THREE.
  for (uint32_t seed = 1; seed < 50; ++seed) {
    Harness h(seed);
    h.enter_and_play();
    if (h.game.requested() != 3) continue;
    h.say(Intent::ONE);
    EXPECT(h.game.fed() == 1 && h.game.phase() == Phase::PLAY);
    h.say(Intent::ONE);  // repeated number: no change, count glows
    EXPECT(h.game.fed() == 1 && h.game.hint_active());
    h.say(Intent::TWO);
    EXPECT(h.game.fed() == 2 && h.game.phase() == Phase::PLAY);
    h.say(Intent::THREE);
    EXPECT(h.game.fed() == 3 && h.game.phase() == Phase::CELEBRATE);
    EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0);
    return;
  }
  EXPECT(false);  // no seed produced a request of 3
}

void test_overshoot_number_completes_round() {
  for (uint32_t seed = 1; seed < 50; ++seed) {
    Harness h(seed);
    h.enter_and_play();
    if (h.game.requested() != 1) continue;
    h.say(Intent::THREE);  // more than asked: monster eats its fill, celebrates
    EXPECT(h.game.phase() == Phase::CELEBRATE && h.game.fed() == 1);
    EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0);
    return;
  }
  EXPECT(false);
}

void test_repeated_input_and_voice_during_celebration() {
  Harness h;
  h.enter_and_play();
  h.say(intent_for(h.game.requested()));
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  h.say(Intent::THREE);
  h.say(Intent::ONE);
  h.say(Intent::AGAIN);
  h.tap_number(2);
  h.tap(G::kMonster);
  EXPECT(h.game.rounds_completed() == 1);
  EXPECT(h.fb.count_of(Cue::CELEBRATE) == 1 && h.fb.count_of(Cue::TRY_AGAIN) == 0);
  EXPECT(h.game.phase() == Phase::CELEBRATE);
}

void test_again_help_monster_badge_replay_prompt() {
  Harness h;
  h.enter_and_play();
  const int prompts = h.fb.count_of(Cue::PROMPT);
  h.say(Intent::AGAIN);
  h.say(Intent::HELP);
  h.tap(G::kMonster);
  h.tap(G::kRequestBadge);
  EXPECT(h.fb.count_of(Cue::PROMPT) == prompts + 4);
  EXPECT(h.fb.last()->variant == h.game.requested());
  EXPECT(h.game.hint_active() && h.game.fed() == 0 && h.game.phase() == Phase::PLAY);
}

void test_idle_hint_is_gentle_and_once() {
  Harness h;
  h.enter_and_play();
  const int prompts = h.fb.count_of(Cue::PROMPT);
  h.game.update(G::kIdleHintMs);
  EXPECT(h.fb.count_of(Cue::PROMPT) == prompts + 1 && h.game.hint_active());
  h.game.update(G::kIdleHintMs * 5);
  EXPECT(h.fb.count_of(Cue::PROMPT) == prompts + 1);
  EXPECT(h.game.phase() == Phase::PLAY && h.fb.count_of(Cue::TRY_AGAIN) == 0);
  h.feed_all_by_taps();  // still fully playable
  EXPECT(h.game.rounds_completed() == 1);
}

void test_empty_tap_and_unrelated_intents_are_harmless() {
  Harness h;
  h.enter_and_play();
  h.game.handle(make_touch_event(128, 120, h.t += 200));  // gap between bowl and monster
  h.game.handle(make_touch_event(30, 30, h.t += 200));    // top bar outside badge
  h.say(Intent::GO);
  h.say(Intent::RED);
  h.say(Intent::YES);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.fed() == 0 && h.game.rounds_completed() == 0);
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0);
}

void test_chew_animation_does_not_drop_taps() {
  Harness h;
  h.enter_and_play();
  if (h.game.requested() < 2) {
    h.feed_all_by_taps();
    h.finish_celebration();
  }
  h.tap_food();
  EXPECT(h.game.chewing());
  h.tap_food();  // tap during chew still counts
  EXPECT(h.game.fed() == 2);
  h.game.update(G::kChewMs);
  EXPECT(!h.game.chewing());
}

void test_audio_disabled_still_playable() {
  NullFeedback silent;
  Rng rng(3);
  GameContext quiet(rng, silent);
  EXPECT(!quiet.audio_enabled);
  FeedMonsterGame g;
  g.enter(quiet);
  g.update(G::kIntroMs);
  EXPECT(g.phase() == Phase::PLAY);
  const uint8_t n = g.requested();  // visible via request badge dots
  const Rect& b = G::kFoodBowl;
  for (uint8_t i = 0; i < n; ++i) {
    g.handle(make_touch_event(static_cast<int16_t>(b.x + 5), static_cast<int16_t>(b.y + 5), 100u * (i + 1u)));
    EXPECT(g.fed() == i + 1);  // tummy dots are the visual count
  }
  EXPECT(g.phase() == Phase::CELEBRATE && g.rounds_completed() == 1);
}

void test_navigation_mid_animation_and_reentry() {
  Harness h;
  h.enter_and_play();
  h.feed_all_by_taps();
  EXPECT(h.game.phase() == Phase::CELEBRATE && h.game.chewing());
  h.game.exit();
  EXPECT(!h.game.active() && !h.game.chewing());
  h.game.update(5000);
  h.game.handle(make_voice_event(Intent::ONE, 1));
  h.game.handle(make_touch_event(50, 100, 2));
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO && h.game.rounds_completed() == 0 && h.game.fed() == 0);
  EXPECT(h.game.extra_snacks() == 0);
}

void test_same_seed_is_deterministic() {
  Harness a(7), b(7);
  a.enter_and_play();
  b.enter_and_play();
  for (int round = 0; round < 8; ++round) {
    EXPECT(a.game.requested() == b.game.requested());
    a.feed_all_by_taps();
    b.say(intent_for(b.game.requested()));
    a.finish_celebration();
    b.finish_celebration();
  }
  EXPECT(a.game.rounds_completed() == 8 && b.game.rounds_completed() == 8);
}

void test_through_scene_manager() {
  FeedMonsterGame game;
  GameEntry entries[] = {{GameId::FEED_MONSTER, Intent::COUNTING, &game}};
  RecordingFeedback fb;
  SceneManager mgr(entries, 1, fb);
  EXPECT(game.active_contexts() == static_cast<uint32_t>(Context::Counting));
  mgr.handle(make_voice_event(Intent::COUNTING, 100));
  EXPECT(game.active() && mgr.mode() == SceneManager::Mode::PLAYING);
  mgr.update(G::kIntroMs);
  EXPECT(game.phase() == Phase::PLAY);
  const int before = fb.count;
  mgr.handle(make_voice_event(Intent::GO, 300));   // out of context: never reaches game
  mgr.handle(make_voice_event(Intent::RED, 400));  // out of context
  EXPECT(fb.count == before && game.fed() == 0);
  mgr.handle(make_voice_event(intent_for(game.requested()), 500));
  EXPECT(game.rounds_completed() == 1);
  mgr.handle(make_touch_event(10, 10, 900));  // Home
  EXPECT(!game.active() && mgr.mode() == SceneManager::Mode::MENU);
}

}  // namespace

int main() {
  test_layout_is_toddler_safe();
  test_intro_then_prompt();
  test_intro_tap_skips_without_feeding();
  test_counts_one_to_three_with_variety();
  test_excess_taps_never_penalised();
  test_voice_and_touch_equivalent();
  test_counting_along_by_voice();
  test_overshoot_number_completes_round();
  test_repeated_input_and_voice_during_celebration();
  test_again_help_monster_badge_replay_prompt();
  test_idle_hint_is_gentle_and_once();
  test_empty_tap_and_unrelated_intents_are_harmless();
  test_chew_animation_does_not_drop_taps();
  test_audio_disabled_still_playable();
  test_navigation_mid_animation_and_reentry();
  test_same_seed_is_deterministic();
  test_through_scene_manager();
  return little_arcade::testing::finish("feed monster tests");
}
