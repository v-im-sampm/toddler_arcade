// Host tests for the launcher's portable rules: menu selection, launch timing, Home intercept,
// the scene-change guard, and the score fade gate.
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "launcher/gated_score.hpp"
#include "launcher/shell.hpp"

using launcher::Event;
using launcher::Scene;
using launcher::Shell;

namespace {
int failures = 0;
#define CHECK(cond)                                                     \
  do {                                                                  \
    if (!(cond)) {                                                      \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                       \
    }                                                                   \
  } while (0)

void run(Shell& s, double seconds) {
  for (double t = 0; t < seconds; t += 1.0 / 64) s.tick(1.0 / 64);
}

Event launch_after_press(Shell& s) {
  Event e = Event::None;
  for (int i = 0; i < 64 && e != Event::Launch; ++i) e = s.tick(1.0 / 64);
  return e;
}

void test_left_half_launches_color_pop() {
  Shell s;
  CHECK(s.scene() == Scene::Menu);
  CHECK(s.down(30, 200) == Event::Pressed);
  CHECK(s.pressed() == 0);
  CHECK(s.tick(0.1) == Event::None);  // visible press first
  CHECK(s.scene() == Scene::Menu);
  CHECK(s.tick(0.1) == Event::Launch);
  CHECK(s.scene() == Scene::ColorPop);
  CHECK(s.pressed() == -1);
}

void test_right_half_launches_aquarium_anywhere() {
  for (double y : {0.0, 20.0, 120.0, 239.0}) {
    Shell s;
    CHECK(s.down(160, y) == Event::Pressed);  // x == 160 is the right half
    CHECK(launch_after_press(s) == Event::Launch);
    CHECK(s.scene() == Scene::Aquarium);
  }
}

void test_second_tap_while_pressing_does_not_switch() {
  Shell s;
  CHECK(s.down(40, 120) == Event::Pressed);
  CHECK(s.down(300, 120) == Event::Ignored);
  CHECK(launch_after_press(s) == Event::Launch);
  CHECK(s.scene() == Scene::ColorPop);
}

void test_guard_then_forward_then_home() {
  Shell s;
  s.down(300, 120);
  launch_after_press(s);
  CHECK(s.scene() == Scene::Aquarium);
  CHECK(s.down(10, 10) == Event::Ignored);  // guard: the launching press cannot also hit Home
  run(s, launcher::kGuardSeconds + 0.02);
  CHECK(s.down(200, 150) == Event::Forward);
  CHECK(s.down(56, 10) == Event::Forward);   // just right of Home belongs to the game
  CHECK(s.down(10, 56) == Event::Forward);   // just below Home belongs to the game
  CHECK(s.down(55.9, 55.9) == Event::Home);
  CHECK(s.scene() == Scene::Menu);
  CHECK(s.down(300, 120) == Event::Ignored);  // guard after Home too
  run(s, launcher::kGuardSeconds + 0.02);
  CHECK(s.down(300, 120) == Event::Pressed);
}

void test_round_trip_both_games() {
  Shell s;
  for (int pass = 0; pass < 3; ++pass)
    for (int tile = 0; tile < 2; ++tile) {
      run(s, launcher::kGuardSeconds + 0.02);
      CHECK(s.down(tile ? 250 : 70, 120) == Event::Pressed);
      CHECK(launch_after_press(s) == Event::Launch);
      CHECK(s.scene() == launcher::kTileGame[tile]);
      run(s, launcher::kGuardSeconds + 0.02);
      CHECK(s.down(28, 28) == Event::Home);
      CHECK(s.scene() == Scene::Menu);
    }
}

void test_negative_dt_is_ignored() {
  Shell s;
  s.down(30, 30);
  CHECK(s.tick(-5) == Event::None);
  CHECK(s.scene() == Scene::Menu);
  CHECK(s.time() == 0);
}

struct Constant final : little_arcade::audio::ExternalScore {
  int calls = 0;
  void render_add(int32_t* out, std::size_t n) override {
    ++calls;
    for (std::size_t i = 0; i < n; ++i) out[i] += 1000;
  }
};

void test_gate_fades_and_parks_score() {
  Constant inner;
  launcher::GatedScore gate(inner);
  std::vector<int32_t> buf(256, 0);
  gate.render_add(buf.data(), buf.size());
  CHECK(inner.calls == 0);  // closed and silent: the score is paused, not rendered
  CHECK(buf[255] == 0);

  gate.set_open(true);
  int32_t prev = -1;
  bool monotonic = true;
  for (int block = 0; block < 6; ++block) {  // 1536 samples > 1280-sample fade
    std::fill(buf.begin(), buf.end(), 0);
    gate.render_add(buf.data(), buf.size());
    for (int32_t v : buf) { monotonic &= v >= prev; prev = v; }
  }
  CHECK(monotonic);
  CHECK(buf.front() == 1000 && buf.back() == 1000);
  CHECK(gate.gain() == launcher::GatedScore::kFadeSamples);

  gate.set_open(false);
  prev = 1001;
  monotonic = true;
  for (int block = 0; block < 6; ++block) {
    std::fill(buf.begin(), buf.end(), 0);
    gate.render_add(buf.data(), buf.size());
    for (int32_t v : buf) { monotonic &= v <= prev; prev = v; }
  }
  CHECK(monotonic);
  CHECK(buf.back() == 0 && gate.gain() == 0);
  const int calls = inner.calls;
  gate.render_add(buf.data(), buf.size());
  CHECK(inner.calls == calls);
}
}  // namespace

int main() {
  test_left_half_launches_color_pop();
  test_right_half_launches_aquarium_anywhere();
  test_second_tap_while_pressing_does_not_switch();
  test_guard_then_forward_then_home();
  test_round_trip_both_games();
  test_negative_dt_is_ignored();
  test_gate_fades_and_parks_score();
  if (failures) {
    std::fprintf(stderr, "launcher_test: %d failure(s)\n", failures);
    return EXIT_FAILURE;
  }
  std::puts("launcher_test: all passed");
  return EXIT_SUCCESS;
}
