// Deterministic host tests for Color Pop's reference rules (GAME_SPEC.md). Always-on checks
// (not assert) so Release builds test too. Fixed dt = 1/64 s (exact in binary floating point).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "game/color_pop.hpp"

using namespace color_pop;
using little_arcade::Intent;

static int g_failures = 0;
#define CHECK(cond)                                                              \
  do {                                                                           \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } \
  } while (0)

namespace {
constexpr double kDt = 1.0 / 64;

struct Log : Sound {
  std::vector<std::string> events;
  int starts = 0;
  void start() override { ++starts; events.push_back("start"); }
  void marimba(double f, double w, double v, double d) override {
    char s[96]; std::snprintf(s, sizeof s, "marimba %.2f %.2f %.2f %.2f", f, w, v, d); events.push_back(s);
  }
  void plop(double w, double v, double h) override {
    char s[64]; std::snprintf(s, sizeof s, "plop %.2f %.2f %.2f", w, v, h); events.push_back(s);
  }
  void motif(Color c, double d) override {
    char s[48]; std::snprintf(s, sizeof s, "motif %d %.2f", static_cast<int>(c), d); events.push_back(s);
  }
  void celebrate(bool big, double d) override {
    char s[48]; std::snprintf(s, sizeof s, "celebrate %d %.2f", big ? 1 : 0, d); events.push_back(s);
  }
  int count(const char* prefix) const {
    int n = 0;
    for (const auto& e : events) n += e.rfind(prefix, 0) == 0 ? 1 : 0;
    return n;
  }
};

void run(Game& g, double seconds) {
  const int n = static_cast<int>(std::lround(seconds / kDt));
  for (int i = 0; i < n; ++i) g.tick(kDt);
}
// START -> tap -> intro (1.2 s) -> first round, bubbles risen.
void to_play(Game& g) {
  g.down(160, 120);
  run(g, 1.25);
  run(g, 0.9);
}
const Bubble* find(const Game& g, bool target) {
  for (const Bubble& b : g.bubbles())
    if ((b.color == g.target()) == target && b.state == BubbleState::Float) return &b;
  return nullptr;
}
// Tap the target and let the celebration finish (next round starts).
void pop_target(Game& g) {
  const Bubble* b = find(g, true);
  CHECK(b != nullptr);
  if (!b) return;
  g.down(b->x, b->y);
  run(g, 2.1);
  run(g, 0.9);
}
uint64_t state_hash(const Game& g) {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](double v) {
    const long long q = std::llround(v * 1e6);
    h ^= static_cast<uint64_t>(q); h *= 1099511628211ull;
  };
  mix(g.round()); mix(g.successes()); mix(g.count()); mix(static_cast<int>(g.target())); mix(g.hint());
  for (const Bubble& b : g.bubbles()) { mix(b.x); mix(b.y); mix(b.s); mix(b.ph); mix(static_cast<int>(b.color)); }
  for (const Particle& p : g.particles()) { mix(p.x); mix(p.y); mix(p.size); }
  for (const Decor& d : g.decor()) { mix(d.x); mix(d.y); }
  mix(g.crab().blink);
  return h;
}
void scripted(Game& g) {
  to_play(g);
  const Bubble* w = find(g, false);
  if (w) g.down(w->x, w->y);
  run(g, 1.5);
  pop_target(g);
  g.down(250, 210);
  run(g, 0.5);
}
}  // namespace

static void test_start_and_intro() {
  Log log;
  Game g(log);
  CHECK(g.phase() == Phase::Start);
  run(g, 2);
  CHECK(log.events.empty());  // no sound before the first tap
  g.down(10, 10);             // any tap starts, even on Home
  CHECK(g.phase() == Phase::Intro);
  CHECK(log.starts == 1);
  run(g, 1.0);
  CHECK(g.phase() == Phase::Intro);
  run(g, 0.25);
  CHECK(g.phase() == Phase::Play);
  CHECK(g.round() == 1);
  CHECK(g.bubbles().size() == 2);
  CHECK(log.count("motif") == 1);
  CHECK(log.events.back().find("0.25") != std::string::npos);

  Game skip(log);
  skip.down(160, 120);
  skip.down(200, 200);  // intro skippable: st.t = 99
  skip.tick(kDt);
  CHECK(skip.phase() == Phase::Play);
}

static void test_determinism() {
  Log a_log, b_log, c_log;
  Game a(a_log, 99), b(b_log, 99), c(c_log, 100);
  scripted(a); scripted(b); scripted(c);
  CHECK(state_hash(a) == state_hash(b));
  CHECK(a_log.events == b_log.events);
  CHECK(state_hash(a) != state_hash(c));
}

static void test_reference_seed_1234() {
  // Values reproduced from the reference: mulberry32(1234) gameplay stream.
  Log log;
  Game g(log, 1234);
  to_play(g);
  CHECK(g.round() == 1);
  CHECK(g.count() == 2);
  for (const Bubble& b : g.bubbles()) {
    CHECK(b.slot >= 0 && b.slot < 6);
    CHECK(std::fabs(b.bx - kSlots[b.slot][0]) <= 6);
    CHECK(std::fabs(b.by - kSlots[b.slot][1]) <= 6);
  }
}

static void test_no_repeat_and_bag() {
  for (uint32_t seed = 1; seed <= 40; ++seed) {
    Log log;
    Game g(log, seed);
    to_play(g);
    Color prev = g.target();
    int seen[4] = {};
    for (int r = 0; r < 12; ++r) {
      ++seen[static_cast<int>(g.target())];
      pop_target(g);
      CHECK(g.phase() == Phase::Play);
      CHECK(g.target() != prev);
      prev = g.target();
    }
    for (int k = 0; k < 4; ++k) CHECK(seen[k] == 3);  // 12 rounds = 3 full shuffle bags
  }
}

static void test_count_ramp_and_big_celebration() {
  Log log;
  Game g(log, 5);
  to_play(g);
  CHECK(g.count() == 2);
  for (int i = 0; i < 3; ++i) pop_target(g);
  CHECK(g.successes() == 3);
  CHECK(g.count() == 3);
  CHECK(g.bubbles().size() == 3);
  pop_target(g);
  // Fifth success: big celebration lasts 2.0 s and dances.
  const Bubble* b = find(g, true);
  CHECK(b != nullptr);
  log.events.clear();
  g.down(b->x, b->y);
  run(g, 0.1);
  CHECK(g.phase() == Phase::Celebrate);
  CHECK(g.successes() == 5);
  CHECK(g.crab().dance > 1.8);
  CHECK(g.decor().size() >= 20);
  CHECK(log.count("celebrate 1 0.18") == 1);
  run(g, 1.6);
  CHECK(g.phase() == Phase::Celebrate);  // a small one would have ended at 1.5 s
  run(g, 0.4);
  CHECK(g.phase() == Phase::Play);
  run(g, 0.9);
  for (int i = 0; i < 6; ++i) pop_target(g);
  CHECK(g.count() == 4);  // capped
}

static void test_wrong_tap_hints_and_respawn() {
  Log log;
  Game g(log, 11);
  to_play(g);
  const Bubble* w = find(g, false);
  CHECK(w != nullptr);
  const Color wrong = w->color;
  log.events.clear();
  g.down(w->x, w->y);
  run(g, 0.125);  // press lasts 0.08 s -> pop on the 6th 1/64 s tick; pointing updates next tick
  CHECK(g.misses() == 1);
  CHECK(g.hint() == 1);
  CHECK(log.count("plop 0.00 0.55 1.40") == 1);
  CHECK(g.pending_respawns() == 1);
  CHECK(g.crab().pointing);
  bool present = false;
  for (const Bubble& b : g.bubbles()) present |= b.color == wrong;
  CHECK(!present);
  run(g, 0.75);
  present = false;
  for (const Bubble& b : g.bubbles()) present |= b.color == wrong;
  CHECK(!present);  // not before 1.0 s after the pop
  run(g, 0.25);
  present = false;
  for (const Bubble& b : g.bubbles()) present |= b.color == wrong;
  CHECK(present);  // respawned after 1.0 s
  run(g, 0.7);
  w = find(g, false);
  CHECK(w != nullptr);
  g.down(w->x, w->y);
  run(g, 0.1);
  CHECK(g.misses() == 2);
  CHECK(g.hint() == 2);
  CHECK(g.round_auto_help());
  CHECK(g.helper_bubble() >= 0);
  run(g, 0.5);
  const Bubble* t = find(g, true);
  CHECK(t && t->s > 1.1);  // hint 2 enlarges the target
}

static void test_idle_hints() {
  Log log;
  Game g(log, 21);
  to_play(g);
  log.events.clear();
  CHECK(g.hint() == 0);
  run(g, 5.0);  // idle counts from round start (~0.95 s ago)
  CHECK(g.hint() == 0);
  run(g, 0.2);
  CHECK(g.hint() == 1);
  CHECK(log.count("motif") == 0);
  run(g, 6.0);
  CHECK(log.count("motif") == 1);  // spoken hint once at 12 s
  CHECK(g.hint() == 1);
  run(g, 20);
  CHECK(log.count("motif") == 1);
}

static void test_auto_help_decrement() {
  Log log;
  Game g(log, 3);
  to_play(g);
  for (int i = 0; i < 3; ++i) pop_target(g);
  CHECK(g.count() == 3);
  for (int k = 0; k < 2; ++k) {
    for (int m = 0; m < 2; ++m) {  // two misses -> auto help for this round
      const Bubble* w = find(g, false);
      CHECK(w != nullptr);
      if (w) g.down(w->x, w->y);
      run(g, 1.6);
    }
    CHECK(g.round_auto_help());
    pop_target(g);
  }
  // successes = 5; 2 consecutive auto-helped rounds -> count back to 2.
  CHECK(g.count() == 2);
  CHECK(g.auto_streak() == 0);
}

static void test_debounce_and_mashing() {
  Log log;
  Game g(log, 8);
  to_play(g);
  const Bubble* t = find(g, true);
  CHECK(t != nullptr);
  const double x = t->x, y = t->y;
  g.down(x, y);
  g.down(x, y);  // within 0.08 s and already pressing: ignored
  run(g, 0.1);
  CHECK(g.successes() == 1);
  CHECK(g.phase() == Phase::Celebrate);
  for (int i = 0; i < 400; ++i) {  // mash during the celebration
    g.down(40 + (i * 37) % 240, 60 + (i * 53) % 150);
    g.tick(kDt / 4);
    CHECK(static_cast<int>(g.particles().size()) <= kMaxParticles);
  }
  CHECK(static_cast<int>(g.particles().size()) == kMaxParticles);
  CHECK(static_cast<int>(g.rings().size()) <= kMaxRings);
  CHECK(static_cast<int>(g.decor().size()) <= kMaxDecor);
  CHECK(g.successes() == 1);
}

static void test_celebrate_input_block() {
  Log log;
  Game g(log, 8);
  to_play(g);
  const Bubble* t = find(g, true);
  g.down(t->x, t->y);
  run(g, 0.1);
  const std::size_t before = g.particles().size();
  log.events.clear();
  g.down(200, 200);  // st.t <= 0.3: blocked
  CHECK(log.events.empty());
  CHECK(g.particles().size() == before);
  run(g, 0.4);  // celebrate began ~0.09 s after the tap (press duration)
  g.down(200, 200);
  CHECK(log.count("marimba") == 1);
}

static void test_home_and_chrome() {
  Log log;
  Game g(log, 4);
  to_play(g);
  const Bubble* t = find(g, true);
  g.down(t->x, t->y);
  run(g, 0.3);
  CHECK(g.phase() == Phase::Celebrate);
  g.down(20, 20);  // Home mid-celebration
  CHECK(g.phase() == Phase::Start);
  CHECK(g.bubbles().empty());
  CHECK(g.particles().empty());
  CHECK(g.rings().empty());
  run(g, 3);
  CHECK(g.phase() == Phase::Start);  // celebration timer no longer fires
  g.down(160, 120);
  CHECK(log.starts == 1);  // audio context started only once
  run(g, 1.25);
  CHECK(g.phase() == Phase::Play);
  CHECK(g.round() == 2);

  const int round = g.round();
  g.down(300, 20);  // disabled mic: ring only
  CHECK(g.phase() == Phase::Play);
  CHECK(g.round() == round);
  CHECK(!g.rings().empty());
}

static void test_badge_and_background_taps() {
  Log log;
  Game g(log, 6);
  to_play(g);
  log.events.clear();
  g.down(160, 30);
  CHECK(log.count("motif") == 1);
  CHECK(g.hint() == 0);
  run(g, 0.5);
  g.down(160, 30);  // double tap within 1 s = help
  CHECK(g.hint() == 1);
  log.events.clear();
  g.down(40, 220);  // crab giggles
  CHECK(g.crab().giggle > 0.4);
  CHECK(log.count("plop 0.00 0.60 1.60") == 1);
  const std::size_t decor = g.decor().size();
  g.down(300, 225);  // empty water: ripple + decor + soft note, never a penalty
  CHECK(g.decor().size() == decor + 1);
  CHECK(log.count("marimba") == 1);
  CHECK(g.misses() == 0);
}

static void test_intents_match_touch() {
  for (uint32_t seed = 1; seed <= 12; ++seed) {
    Log tl, il;
    Game touch(tl, seed), voice(il, seed);
    to_play(touch);
    voice.intent(Intent::START);
    run(voice, 1.25);
    run(voice, 0.9);
    CHECK(state_hash(touch) == state_hash(voice));
    const Bubble* t = find(touch, true);
    touch.down(t->x, t->y);
    switch (voice.target()) {
      case Color::Red: voice.intent(Intent::RED); break;
      case Color::Blue: voice.intent(Intent::BLUE); break;
      case Color::Green: voice.intent(Intent::GREEN); break;
      case Color::Yellow: voice.intent(Intent::YELLOW); break;
    }
    run(touch, 0.5); run(voice, 0.5);
    CHECK(touch.successes() == 1 && voice.successes() == 1);
    CHECK(tl.events == il.events);
    CHECK(state_hash(touch) == state_hash(voice));
  }
  Log log;
  Game g(log, 2);
  to_play(g);
  log.events.clear();
  g.intent(Intent::AGAIN);
  CHECK(log.count("motif") == 1);
  g.intent(Intent::HELP);
  CHECK(g.hint() == 1);
  g.intent(Intent::HELP);
  CHECK(g.hint() == 2);
  g.intent(Intent::BACK);
  CHECK(g.phase() == Phase::Start);
}

static void test_reduced_motion() {
  Log a_log, b_log;
  Game a(a_log, 9), b(b_log, 9);
  b.set_reduced(true);
  to_play(a); to_play(b);
  const Bubble* ta = find(a, true);
  const Bubble* tb = find(b, true);
  a.down(ta->x, ta->y); b.down(tb->x, tb->y);
  run(a, 0.1); run(b, 0.1);
  CHECK(a.particles().size() == 30);  // 14 + 16
  CHECK(b.particles().size() == 15);  // round(7) + round(8)
  CHECK(a_log.events == b_log.events);  // sound cues are unchanged by reduced motion
}

int main() {
  test_start_and_intro();
  test_determinism();
  test_reference_seed_1234();
  test_no_repeat_and_bag();
  test_count_ramp_and_big_celebration();
  test_wrong_tap_hints_and_respawn();
  test_idle_hints();
  test_auto_help_decrement();
  test_debounce_and_mashing();
  test_celebrate_input_block();
  test_home_and_chrome();
  test_badge_and_background_taps();
  test_intents_match_touch();
  test_reduced_motion();
  if (g_failures) { std::printf("color_pop_game_test: %d failure(s)\n", g_failures); return 1; }
  std::printf("color_pop_game_test: all checks passed\n");
  return 0;
}
