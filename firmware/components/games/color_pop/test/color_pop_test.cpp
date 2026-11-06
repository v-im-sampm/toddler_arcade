#include <cmath>
#include <initializer_list>

#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::color_pop;
using little_arcade::testing::RecordingFeedback;

namespace {
constexpr double kDt = 1.0 / 60;

struct Harness {
  Rng rng;
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  ColorPopGame game;
  uint32_t ms = 0;

  explicit Harness(uint32_t seed = 42) : rng(seed) { game.enter(ctx); }
  void run(double seconds) {
    for (int i = 0, n = static_cast<int>(std::lround(seconds / kDt)); i < n; ++i) {
      game.step(kDt);
      game.frame_drawn();
    }
  }
  void to_play() {
    run(1.25);
    run(0.8);  // bubbles finish rising
  }
  int find(bool target) const {
    for (std::size_t i = 0; i < game.bubble_count(); ++i) {
      const Bubble& b = game.bubble(i);
      if ((b.color == game.target()) == target && b.state == BubbleState::FLOAT) return static_cast<int>(i);
    }
    return -1;
  }
  void tap(double x, double y) {
    game.handle(make_touch_event(static_cast<int16_t>(std::lround(x)), static_cast<int16_t>(std::lround(y)), ms += 100));
  }
  void tap_bubble(int i) {
    const Bubble& b = game.bubble(static_cast<std::size_t>(i));
    tap(b.x, b.y);
  }
  void say(Intent intent) { game.handle(make_voice_event(intent, ms += 100)); }
};

Intent intent_for(Color c) {
  switch (c) {
    case Color::RED: return Intent::RED;
    case Color::BLUE: return Intent::BLUE;
    case Color::GREEN: return Intent::GREEN;
    default: return Intent::YELLOW;
  }
}

void test_intro_then_first_round() {
  Harness h;
  EXPECT(h.game.phase() == Phase::INTRO);
  EXPECT(h.fb.count_of(Cue::AMBIENT) == 1);
  EXPECT(h.game.crab().y_off == 40);
  h.run(1.0);
  EXPECT(h.game.phase() == Phase::INTRO && h.game.crab().y_off < 1);
  h.run(0.25);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.has_target());
  EXPECT(h.game.bubble_count() == 2);
  EXPECT(h.game.bubble_target_count() == 2);
  EXPECT(!h.fb.any(Cue::PROMPT));
  h.run(0.3);  // prompt is scheduled 0.25 s after the round starts
  EXPECT(h.fb.count_of(Cue::PROMPT) == 1);
  EXPECT(h.fb.last()->variant == static_cast<uint8_t>(h.game.target()));
  h.run(0.6);
  for (std::size_t i = 0; i < h.game.bubble_count(); ++i) {
    const Bubble& b = h.game.bubble(i);
    EXPECT(b.state == BubbleState::FLOAT);
    EXPECT(std::fabs(b.y - b.by) <= 2.01);
    EXPECT(b.x > 40 && b.x < 280 && b.y > 60 && b.y < 200);
  }
}

void test_intro_skip_and_determinism() {
  Harness h;
  h.tap(160, 120);
  h.game.step(kDt);
  EXPECT(h.game.phase() == Phase::PLAY);

  Harness a(7), b(7), c(8);
  a.to_play();
  b.to_play();
  c.to_play();
  bool same = a.game.bubble_count() == b.game.bubble_count() && a.game.target() == b.game.target();
  for (std::size_t i = 0; same && i < a.game.bubble_count(); ++i) {
    same = a.game.bubble(i).x == b.game.bubble(i).x && a.game.bubble(i).y == b.game.bubble(i).y &&
           a.game.bubble(i).color == b.game.bubble(i).color;
  }
  EXPECT(same);
  bool differs = a.game.target() != c.game.target();
  for (std::size_t i = 0; !differs && i < a.game.bubble_count() && i < c.game.bubble_count(); ++i) {
    differs = a.game.bubble(i).x != c.game.bubble(i).x;
  }
  EXPECT(differs);
}

void test_correct_pop_celebrates_and_continues() {
  Harness h;
  h.to_play();
  const Color target = h.game.target();
  const int t = h.find(true);
  EXPECT(t >= 0);
  h.tap_bubble(t);
  EXPECT(h.game.bubble(static_cast<std::size_t>(t)).state == BubbleState::PRESS);
  h.run(0.1);  // squash, then pop
  EXPECT(h.game.phase() == Phase::CELEBRATE);
  EXPECT(h.game.rounds_completed() == 1);
  EXPECT(h.fb.count_of(Cue::SUCCESS) == 1);
  EXPECT(h.game.particle_count() > 0 && h.game.ring_count() > 0);
  EXPECT(h.game.crab().clap > 0 && h.game.badge().happy > 0);
  for (std::size_t i = 0; i < h.game.bubble_count(); ++i) EXPECT(h.game.bubble(i).state == BubbleState::LEAVE);
  h.run(0.2);
  EXPECT(h.fb.count_of(Cue::CELEBRATE) == 1);
  EXPECT(h.fb.last()->variant == 0);  // small celebration
  h.run(1.3);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.target() != target);  // bag never repeats a colour back-to-back
  EXPECT(h.game.bubble_count() == 2);
}

void test_wrong_pop_is_gentle_and_escalates_help() {
  Harness h;
  h.to_play();
  int w = h.find(false);
  h.tap_bubble(w);
  h.run(0.1);
  EXPECT(h.game.phase() == Phase::PLAY);
  EXPECT(h.game.rounds_completed() == 0);
  EXPECT(h.fb.any(Cue::NOTE));
  EXPECT((h.fb.last()->variant & 0xF0) == cue::kOtherPop);
  EXPECT(!h.fb.any(Cue::TRY_AGAIN));  // no buzzer, no failure state
  EXPECT(h.game.hint() == 1);
  EXPECT(h.game.crab().pointing);
  EXPECT(h.game.bubble_count() == 1);
  h.run(1.0);  // the popped bubble floats back up
  EXPECT(h.game.bubble_count() == 2);
  h.run(0.7);
  w = h.find(false);
  EXPECT(w >= 0);
  h.tap_bubble(w);
  h.run(0.1);
  EXPECT(h.game.hint() == 2);  // helper hand + enlarged target
  const int t = h.find(true);
  EXPECT(t >= 0);
  h.run(0.5);
  EXPECT(h.game.bubble(static_cast<std::size_t>(t)).s > 1.1);
}

void test_idle_hints() {
  Harness h;
  h.to_play();
  const int prompts = h.fb.count_of(Cue::PROMPT);
  h.run(5.4);
  EXPECT(h.game.hint() == 1);  // idle 6 s from round start
  EXPECT(h.fb.count_of(Cue::PROMPT) == prompts);
  h.run(6.5);
  EXPECT(h.fb.count_of(Cue::PROMPT) == prompts + 1);  // spoken once at 12 s
  h.run(10);
  EXPECT(h.fb.count_of(Cue::PROMPT) == prompts + 1);
}

void test_badge_crab_and_water_taps() {
  Harness h;
  h.to_play();
  int prompts = h.fb.count_of(Cue::PROMPT);
  h.tap(160, 30);
  EXPECT(h.fb.count_of(Cue::PROMPT) == prompts + 1);
  EXPECT(h.game.hint() == 0);
  h.run(0.3);
  h.tap(160, 30);  // double tap = help
  EXPECT(h.game.hint() == 1);
  h.tap(30, 220);
  EXPECT(h.fb.last()->cue == Cue::REACTION && h.game.crab().giggle > 0);
  const std::size_t rings = h.game.ring_count();
  h.tap(300, 140);
  if (h.fb.last()->cue == Cue::NOTE) {  // empty water, unless a bubble sits there
    EXPECT(h.fb.last()->variant == cue::kRipple);
    EXPECT(h.game.ring_count() == rings + 1);
  }
}

void test_voice_matches_touch() {
  Harness h;
  h.to_play();
  h.say(Intent::AGAIN);
  EXPECT(h.fb.last()->cue == Cue::PROMPT);
  Color absent = Color::RED;
  for (Color c : {Color::RED, Color::BLUE, Color::GREEN, Color::YELLOW}) {
    bool present = false;
    for (std::size_t i = 0; i < h.game.bubble_count(); ++i) present = present || h.game.bubble(i).color == c;
    if (!present) absent = c;
  }
  h.say(intent_for(absent));
  EXPECT(h.game.hint() >= 1 && h.fb.last()->cue == Cue::PROMPT);
  h.say(intent_for(h.game.target()));
  h.run(0.1);
  EXPECT(h.game.phase() == Phase::CELEBRATE && h.game.rounds_completed() == 1);
}

void test_difficulty_caps_and_long_play() {
  Harness h(99);
  h.to_play();
  std::size_t max_parts = 0, max_bubbles = 0, max_decor = 0;
  for (int round = 0; round < 40; ++round) {
    for (int guard = 0; guard < 600 && h.game.phase() != Phase::PLAY; ++guard) h.run(kDt);
    h.run(0.7);
    const int t = h.find(true);
    EXPECT(t >= 0);
    if (t < 0) break;
    h.tap_bubble(t);
    for (int k = 0; k < 6; ++k) {  // celebration taps make sparkles
      h.run(0.12);
      h.tap(60 + k * 40, 120);
      if (h.game.particle_count() > max_parts) max_parts = h.game.particle_count();
    }
    if (h.game.bubble_count() > max_bubbles) max_bubbles = h.game.bubble_count();
    if (h.game.decor_count() > max_decor) max_decor = h.game.decor_count();
    if (round == 2) {
      h.run(1.0);
      h.run(0.8);
      EXPECT(h.game.bubble_target_count() == 3);  // grows after 3 successes
    }
  }
  EXPECT(h.game.rounds_completed() == 40);
  EXPECT(h.game.bubble_target_count() == 4);
  EXPECT(max_parts <= ColorPopGame::kMaxParticles);
  EXPECT(max_bubbles <= ColorPopGame::kMaxBubbles);
  EXPECT(max_decor <= ColorPopGame::kMaxDecor);
  EXPECT(h.fb.count_of(Cue::CELEBRATE) >= 39);
  int big = 0;
  for (int i = 0; i < h.fb.count && i < RecordingFeedback::kCapacity; ++i) {
    big += h.fb.entries[i].cue == Cue::CELEBRATE && h.fb.entries[i].variant == 1;
  }
  EXPECT(big >= 5);  // every 5th success is the big one
}

void test_mulberry_matches_reference() {
  // mulberry32(7) first outputs, computed with the reference JS function.
  Mulberry32 r(7);
  EXPECT(std::fabs(r.next() - 0.011704753153026104) < 1e-15);
  EXPECT(std::fabs(r.next() - 0.061958257574588060) < 1e-15);
  EXPECT(std::fabs(r.next() - 0.97690763277933002) < 1e-15);
}
}  // namespace

int main() {
  test_intro_then_first_round();
  test_intro_skip_and_determinism();
  test_correct_pop_celebrates_and_continues();
  test_wrong_pop_is_gentle_and_escalates_help();
  test_idle_hints();
  test_badge_crab_and_water_taps();
  test_voice_matches_touch();
  test_difficulty_caps_and_long_play();
  test_mulberry_matches_reference();
  return testing::finish("color_pop_test");
}
