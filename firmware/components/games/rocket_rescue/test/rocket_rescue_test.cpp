#include "little_arcade/games/rocket_rescue.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::rocket_rescue;
using little_arcade::testing::RecordingFeedback;

namespace {

using G = RocketRescueGame;

struct Harness {
  Rng rng{42};
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  RocketRescueGame game;
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
  void tap_planet(int i) { tap(G::kPlanetRects[i]); }
  void say(Intent intent) { game.handle(make_voice_event(intent, t += 200)); }
  void fly(uint32_t ms) {
    for (uint32_t done = 0; done < ms; done += 50) game.update(50);
  }
  void finish_trip() { fly(G::kTripMs); }
  void finish_celebration() { game.update(G::kCelebrateMs); }
};

bool spaced(const Rect& a, const Rect& b) {
  const Rect grown{static_cast<int16_t>(a.x - kMinSpacing), static_cast<int16_t>(a.y - kMinSpacing),
                   static_cast<int16_t>(a.w + 2 * kMinSpacing), static_cast<int16_t>(a.h + 2 * kMinSpacing)};
  return !grown.overlaps(b);
}

void test_layout_is_toddler_safe() {
  constexpr std::size_t kN = kPlanetCount + G::kStarSlotCount + 3;
  Rect all[kN]{};
  std::size_t n = 0;
  for (const Rect& r : G::kPlanetRects) all[n++] = r;
  for (const Rect& r : G::kStarSlots) all[n++] = r;
  all[n++] = G::kGoButton;
  all[n++] = G::kStopButton;
  all[n++] = G::kPromptBadge;
  EXPECT(n == kN);
  for (std::size_t i = 0; i < kN; ++i) {
    EXPECT(all[i].on_screen() && all[i].large_enough());
    EXPECT(!all[i].overlaps(kHomeButton) && !all[i].overlaps(kMicButton));
    for (std::size_t j = i + 1; j < kN; ++j) EXPECT(spaced(all[i], all[j]));
  }
  // Primary targets live in the play area; the optional badge sits in the top bar between Home and Mic.
  for (std::size_t i = 0; i + 1 < kN; ++i) EXPECT(all[i].y >= kPlayArea.y);
  EXPECT(G::kPromptBadge.x >= 64 && G::kPromptBadge.x + G::kPromptBadge.w <= 256);
  // GO / STOP are extra large for small hands.
  EXPECT(G::kGoButton.w >= 2 * kMinTarget && G::kStopButton.w >= 2 * kMinTarget);
  EXPECT(kPlayArea.contains(G::kPadPoint.x, G::kPadPoint.y));
}

void test_three_distinct_planets() {
  Harness h;
  h.game.enter(h.ctx);
  EXPECT(kPlanetCount == 3);
  for (std::size_t i = 0; i < kPlanetCount; ++i) {
    EXPECT(!h.game.planet(i).rescued);
    for (std::size_t j = i + 1; j < kPlanetCount; ++j) {
      EXPECT(h.game.planet(i).look != h.game.planet(j).look);
      EXPECT(h.game.planet(i).buddy != h.game.planet(j).buddy);
    }
  }
  EXPECT(h.game.rocket_state() == RocketState::DOCKED && h.game.docked_at() == -1);
  const Point p = h.game.rocket_position();
  EXPECT(p.x == G::kPadPoint.x && p.y == G::kPadPoint.y);
}

void test_intro_then_prompt() {
  Harness h;
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(!h.fb.any(Cue::PROMPT));
  h.game.update(G::kIntroMs - 1);
  EXPECT(h.game.phase() == Phase::INTRO);
  h.game.update(1);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.fb.last()->cue == Cue::PROMPT && h.fb.last()->variant == static_cast<uint8_t>(h.game.suggested()));
}

void test_intro_input_skips_without_launching() {
  Harness h;
  h.game.enter(h.ctx);
  h.say(Intent::GO);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.rocket_state() == RocketState::DOCKED);
  Harness t;
  t.game.enter(t.ctx);
  t.tap_planet(0);
  EXPECT(t.game.phase() == Phase::PLAY && t.game.rocket_state() == RocketState::DOCKED);
}

void test_go_rescues_all_three_with_gentle_motion() {
  Harness h;
  h.enter_and_play();
  for (int rescue = 0; rescue < 3; ++rescue) {
    const int target = h.game.suggested();
    EXPECT(target >= 0 && !h.game.planet(static_cast<std::size_t>(target)).rescued);
    h.tap(G::kGoButton);
    EXPECT(h.game.rocket_state() == RocketState::FLYING && h.game.destination() == target);
    Point prev = h.game.rocket_position();
    int max_step = 0;
    while (h.game.rocket_state() == RocketState::FLYING) {
      h.game.update(33);  // ~30 fps
      const Point p = h.game.rocket_position();
      const int dx = p.x > prev.x ? p.x - prev.x : prev.x - p.x;
      const int dy = p.y > prev.y ? p.y - prev.y : prev.y - p.y;
      if (dx + dy > max_step) max_step = dx + dy;
      prev = p;
    }
    EXPECT(max_step <= 6);  // gentle glide, no jumps
    EXPECT(h.game.docked_at() == target && h.game.phase() == Phase::CELEBRATE);
    EXPECT(h.game.planet(static_cast<std::size_t>(target)).rescued);
    h.finish_celebration();
    EXPECT(h.game.phase() == Phase::PLAY);
  }
  EXPECT(h.game.rescues_total() == 3 && h.game.missions_completed() == 1);
  EXPECT(h.fb.count_of(Cue::SUCCESS) == 3 && h.fb.count_of(Cue::CELEBRATE) == 3);
  // New mission: friends wait again and the suggested planet isn't the one we're on.
  EXPECT(h.game.rescued_count() == 0);
  EXPECT(h.game.suggested() >= 0 && h.game.suggested() != h.game.docked_at());
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0);
}

void test_no_timed_failure() {
  Harness h;
  h.enter_and_play();
  // Idle on the pad for an hour: nothing happens.
  for (int i = 0; i < 3600; ++i) h.game.update(1000);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.rocket_state() == RocketState::DOCKED);
  h.say(Intent::GO);
  h.fly(G::kTripMs / 2);
  h.say(Intent::STOP);
  const uint32_t progress = h.game.trip_progress_permille();
  EXPECT(h.game.rocket_state() == RocketState::HOVERING && progress > 0 && progress < 1000);
  for (int i = 0; i < 3600; ++i) h.game.update(1000);  // hover forever: never crashes or drifts
  EXPECT(h.game.rocket_state() == RocketState::HOVERING);
  EXPECT(h.game.trip_progress_permille() == progress);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.rescues_total() == 0);
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0);
  h.say(Intent::GO);
  h.finish_trip();
  EXPECT(h.game.phase() == Phase::CELEBRATE && h.game.rescues_total() == 1);
}

void test_stop_and_go_voice_touch_equivalent() {
  Harness touch(9), voice(9);
  touch.enter_and_play();
  voice.enter_and_play();
  touch.tap(G::kGoButton);
  voice.say(Intent::GO);
  touch.fly(1000);
  voice.fly(1000);
  touch.tap(G::kStopButton);
  voice.say(Intent::STOP);
  EXPECT(touch.game.rocket_state() == RocketState::HOVERING && voice.game.rocket_state() == RocketState::HOVERING);
  EXPECT(touch.game.trip_progress_permille() == voice.game.trip_progress_permille());
  touch.tap(G::kGoButton);
  voice.say(Intent::GO);
  touch.finish_trip();
  voice.finish_trip();
  EXPECT(touch.game.phase() == Phase::CELEBRATE && voice.game.phase() == Phase::CELEBRATE);
  EXPECT(touch.game.docked_at() == voice.game.docked_at());
  EXPECT(touch.fb.count_of(Cue::SUCCESS) == 1 && voice.fb.count_of(Cue::SUCCESS) == 1);
  EXPECT(touch.fb.count_of(Cue::NOTE) == voice.fb.count_of(Cue::NOTE));
}

void test_tap_planet_flies_and_retargets_smoothly() {
  Harness h;
  h.enter_and_play();
  h.tap_planet(0);
  EXPECT(h.game.rocket_state() == RocketState::FLYING && h.game.destination() == 0);
  h.fly(1000);
  const Point before = h.game.rocket_position();
  h.tap_planet(2);  // change of mind mid-flight
  const Point after = h.game.rocket_position();
  EXPECT(before.x == after.x && before.y == after.y);  // no teleport
  EXPECT(h.game.destination() == 2 && h.game.trip_progress_permille() == 0);
  h.finish_trip();
  EXPECT(h.game.docked_at() == 2 && h.game.planet(2).rescued);
  h.finish_celebration();
  // Tapping the planet we're on: the friend waves; no flight.
  const int reactions = h.fb.count_of(Cue::REACTION);
  h.tap_planet(2);
  EXPECT(h.game.rocket_state() == RocketState::DOCKED && h.game.waving() == 2);
  EXPECT(h.fb.count_of(Cue::REACTION) == reactions + 1);
  // Revisiting a rescued planet is a friendly visit, not a second rescue.
  h.tap_planet(0);
  h.finish_trip();
  h.finish_celebration();
  h.tap_planet(2);
  h.finish_trip();
  EXPECT(h.game.phase() == Phase::PLAY && h.game.docked_at() == 2 && h.game.waving() == 2);
  EXPECT(h.game.rescues_total() == 2);
}

void test_hover_then_tap_same_planet_resumes() {
  Harness h;
  h.enter_and_play();
  h.tap_planet(1);
  h.fly(500);
  h.say(Intent::STOP);
  const uint32_t progress = h.game.trip_progress_permille();
  h.tap_planet(1);
  EXPECT(h.game.rocket_state() == RocketState::FLYING && h.game.trip_progress_permille() == progress);
}

void test_stars_collect_and_respawn() {
  Harness h;
  h.enter_and_play();
  for (std::size_t i = 0; i < G::kStarCount; ++i) {
    EXPECT(h.game.star(i).visible);
    for (std::size_t j = i + 1; j < G::kStarCount; ++j) EXPECT(h.game.star(i).slot != h.game.star(j).slot);
  }
  for (int round = 0; round < 30; ++round) {
    const std::size_t i = static_cast<std::size_t>(round) % G::kStarCount;
    const uint8_t old_slot = h.game.star(i).slot;
    const Rect r = h.game.star_rect(i);
    h.tap(r);
    EXPECT(!h.game.star(i).visible);
    h.tap(r);  // repeated tap on the empty spot is harmless
    h.game.update(G::kStarRespawnMs);
    EXPECT(h.game.star(i).visible && h.game.star(i).slot != old_slot);
    for (std::size_t a = 0; a < G::kStarCount; ++a) {
      for (std::size_t b = a + 1; b < G::kStarCount; ++b) EXPECT(h.game.star(a).slot != h.game.star(b).slot);
    }
  }
  EXPECT(h.game.stars_collected() == 30);
  EXPECT(h.fb.count_of(Cue::NOTE) == 30);
  EXPECT(h.game.rocket_state() == RocketState::DOCKED);  // stars never launch the rocket
}

void test_repeated_input_and_voice_during_animation() {
  Harness h;
  h.enter_and_play();
  h.say(Intent::GO);
  h.fly(1000);
  const uint32_t progress = h.game.trip_progress_permille();
  for (int i = 0; i < 5; ++i) {
    h.tap(G::kGoButton);
    h.say(Intent::GO);
  }
  EXPECT(h.game.rocket_state() == RocketState::FLYING && h.game.trip_progress_permille() == progress);
  EXPECT(h.fb.count_of(Cue::NOTE) == 1);  // single launch sound
  h.say(Intent::STOP);
  h.say(Intent::STOP);
  h.tap(G::kStopButton);
  EXPECT(h.fb.count_of(Cue::NOTE) == 2);
  h.say(Intent::GO);
  h.finish_trip();
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  // Everything during the celebration is ignored.
  h.say(Intent::GO);
  h.say(Intent::AGAIN);
  h.tap(G::kGoButton);
  for (int i = 0; i < static_cast<int>(kPlanetCount); ++i) h.tap_planet(i);
  EXPECT(h.game.rocket_state() == RocketState::DOCKED && h.game.rescues_total() == 1);
  EXPECT(h.fb.count_of(Cue::SUCCESS) == 1 && h.fb.count_of(Cue::TRY_AGAIN) == 0);
  h.finish_celebration();
  EXPECT(h.game.phase() == Phase::PLAY);
}

void test_again_help_and_badge_replay_prompt() {
  Harness h;
  h.enter_and_play();
  const int prompts = h.fb.count_of(Cue::PROMPT);
  h.say(Intent::AGAIN);
  h.say(Intent::HELP);
  h.tap(G::kPromptBadge);
  EXPECT(h.fb.count_of(Cue::PROMPT) == prompts + 3);
  EXPECT(h.fb.last()->variant == static_cast<uint8_t>(h.game.suggested()));
  EXPECT(h.game.hint_active() && h.game.rocket_state() == RocketState::DOCKED);
  h.game.update(G::kHintMs);
  EXPECT(!h.game.hint_active());
}

void test_empty_tap_and_unrelated_intents_are_harmless() {
  Harness h;
  h.enter_and_play();
  h.game.handle(make_touch_event(160, 210, h.t += 200));  // launch pad (drawn only)
  h.game.handle(make_touch_event(100, 90, h.t += 200));   // gap between planets
  h.game.handle(make_touch_event(-1, -1, h.t += 200));
  h.say(Intent::STOP);  // nothing to stop
  h.say(Intent::RED);
  h.say(Intent::YES);
  h.say(Intent::UNKNOWN);
  EXPECT(h.game.phase() == Phase::PLAY && h.game.rocket_state() == RocketState::DOCKED);
  EXPECT(h.fb.count_of(Cue::TRY_AGAIN) == 0 && h.fb.count_of(Cue::NOTE) == 0);
  EXPECT(h.game.active_contexts() == static_cast<uint32_t>(Context::Space));
}

void test_audio_disabled_touch_only_mission() {
  NullFeedback silent;
  Rng rng(3);
  GameContext quiet(rng, silent);
  EXPECT(!quiet.audio_enabled && !quiet.voice_enabled);
  RocketRescueGame g;
  g.enter(quiet);
  g.update(G::kIntroMs);
  uint32_t t = 0;
  for (int rescue = 0; rescue < 3; ++rescue) {
    const int target = g.suggested();  // drawn on the prompt badge + hint glow
    EXPECT(target >= 0);
    const Rect& r = G::kPlanetRects[target];
    g.handle(make_touch_event(static_cast<int16_t>(r.x + 10), static_cast<int16_t>(r.y + 10), t += 200));
    for (uint32_t ms = 0; ms < G::kTripMs; ms += 100) g.update(100);
    EXPECT(g.phase() == Phase::CELEBRATE);
    g.update(G::kCelebrateMs);
  }
  EXPECT(g.missions_completed() == 1 && g.phase() == Phase::PLAY);
}

void test_navigation_mid_animation_and_reentry() {
  Harness h;
  h.enter_and_play();
  h.say(Intent::GO);
  h.fly(1000);
  h.game.exit();  // Home pressed mid-flight
  EXPECT(!h.game.active());
  h.game.update(10000);
  h.game.handle(make_voice_event(Intent::GO, 1));
  h.game.handle(make_touch_event(40, 210, 2));
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO && h.game.rocket_state() == RocketState::DOCKED);
  EXPECT(h.game.docked_at() == -1 && h.game.rescues_total() == 0 && h.game.stars_collected() == 0);

  h.game.update(G::kIntroMs);
  h.say(Intent::GO);
  h.finish_trip();
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  h.game.exit();  // Home pressed mid-celebration
  h.game.update(G::kCelebrateMs);
  h.game.enter(h.ctx);
  EXPECT(h.game.phase() == Phase::INTRO && h.game.rescued_count() == 0 && h.game.missions_completed() == 0);
}

void test_same_seed_is_deterministic() {
  Harness a(7), b(7);
  a.enter_and_play();
  b.enter_and_play();
  for (int step = 0; step < 6; ++step) {
    EXPECT(a.game.suggested() == b.game.suggested());
    for (std::size_t i = 0; i < kPlanetCount; ++i) {
      EXPECT(a.game.planet(i).look == b.game.planet(i).look);
      EXPECT(a.game.planet(i).buddy == b.game.planet(i).buddy);
    }
    for (std::size_t i = 0; i < G::kStarCount; ++i) EXPECT(a.game.star(i).slot == b.game.star(i).slot);
    a.tap(a.game.star_rect(0));
    b.tap(b.game.star_rect(0));
    a.tap(G::kGoButton);
    b.say(Intent::GO);
    a.finish_trip();
    b.finish_trip();
    a.finish_celebration();
    b.finish_celebration();
  }
  EXPECT(a.game.missions_completed() == 2 && b.game.missions_completed() == 2);
}

void test_seeds_vary_the_mission() {
  bool differs = false;
  Harness first(1);
  first.enter_and_play();
  for (uint32_t seed = 2; seed < 20 && !differs; ++seed) {
    Harness other(seed);
    other.enter_and_play();
    differs = other.game.suggested() != first.game.suggested() ||
              other.game.planet(0).look != first.game.planet(0).look ||
              other.game.planet(0).buddy != first.game.planet(0).buddy;
  }
  EXPECT(differs);
}

void test_through_scene_manager() {
  RocketRescueGame game;
  GameEntry entries[] = {{GameId::ROCKET_RESCUE, Intent::SPACE, &game}};
  RecordingFeedback fb;
  SceneManager mgr(entries, 1, fb);
  mgr.handle(make_voice_event(Intent::SPACE, 100));
  EXPECT(game.active() && mgr.mode() == SceneManager::Mode::PLAYING);
  EXPECT((mgr.active_contexts() & static_cast<uint32_t>(Context::Space)) != 0);
  mgr.update(G::kIntroMs);
  EXPECT(game.phase() == Phase::PLAY);
  const int prompts = fb.count_of(Cue::PROMPT);
  mgr.handle(make_voice_event(Intent::YELLOW, 200));  // out of context: never reaches game
  mgr.handle(make_voice_event(Intent::THREE, 250));
  EXPECT(game.rocket_state() == RocketState::DOCKED && fb.count_of(Cue::PROMPT) == prompts);
  mgr.handle(make_voice_event(Intent::AGAIN, 260));  // global intent reaches the game
  EXPECT(fb.count_of(Cue::PROMPT) == prompts + 1);
  mgr.handle(make_voice_event(Intent::GO, 300));
  EXPECT(game.rocket_state() == RocketState::FLYING);
  mgr.update(500);
  mgr.handle(make_voice_event(Intent::STOP, 400));
  EXPECT(game.rocket_state() == RocketState::HOVERING);
  const Rect& go = G::kGoButton;
  mgr.handle(make_touch_event(static_cast<int16_t>(go.x + 5), static_cast<int16_t>(go.y + 5), 600));
  EXPECT(game.rocket_state() == RocketState::FLYING);
  mgr.update(G::kTripMs);
  EXPECT(game.rescues_total() == 1);
  mgr.handle(make_touch_event(10, 10, 900));  // Home
  EXPECT(!game.active() && mgr.mode() == SceneManager::Mode::MENU);
}

}  // namespace

int main() {
  test_layout_is_toddler_safe();
  test_three_distinct_planets();
  test_intro_then_prompt();
  test_intro_input_skips_without_launching();
  test_go_rescues_all_three_with_gentle_motion();
  test_no_timed_failure();
  test_stop_and_go_voice_touch_equivalent();
  test_tap_planet_flies_and_retargets_smoothly();
  test_hover_then_tap_same_planet_resumes();
  test_stars_collect_and_respawn();
  test_repeated_input_and_voice_during_animation();
  test_again_help_and_badge_replay_prompt();
  test_empty_tap_and_unrelated_intents_are_harmless();
  test_audio_disabled_touch_only_mission();
  test_navigation_mid_animation_and_reentry();
  test_same_seed_is_deterministic();
  test_seeds_vary_the_mission();
  test_through_scene_manager();
  return little_arcade::testing::finish("rocket rescue tests");
}
