#include "little_arcade/settings/parent_gate.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using little_arcade::settings::ParentGate;

namespace {

constexpr int16_t hx(const Rect& r) { return static_cast<int16_t>(r.x + r.w / 2); }
constexpr int16_t hy(const Rect& r) { return static_cast<int16_t>(r.y + r.h / 2); }

// Press-and-release tap; returns whether the press edge was forwarded.
bool tap(ParentGate& g, const Rect& r, uint32_t& now) {
  const bool fwd = g.pointer(true, hx(r), hy(r), now);
  now += 50;
  g.pointer(false, 0, 0, now);
  now += 50;
  return fwd;
}

// Holds the hotspot for `ms`, sampling every 20 ms like the UI loop.
void hold_hotspot(ParentGate& g, uint32_t ms, uint32_t& now) {
  const uint32_t end = now + ms;
  for (; now <= end; now += 20) g.pointer(true, hx(ParentGate::kHotspot), hy(ParentGate::kHotspot), now);
  g.pointer(false, 0, 0, now);
  now += 20;
}

int correct_index(const ParentGate& g) {
  for (int i = 0; i < ParentGate::kChoiceCount; ++i) {
    if (g.choice(i) == g.sum_a() + g.sum_b()) return i;
  }
  return -1;
}

void open_settings(ParentGate& g, uint32_t& now) {
  hold_hotspot(g, ParentGate::kHoldMs + 40, now);
  tap(g, ParentGate::kChoiceRects[correct_index(g)], now);
}

void test_fresh_board_is_muted_and_closed() {
  ParentGate g(1, 0);
  EXPECT(g.level() == 0);
  EXPECT(!g.open());
  ParentGate clamped(1, 200);
  EXPECT(clamped.level() == ParentGate::kMaxLevel);
}

void test_normal_taps_pass_through_once() {
  ParentGate g(1, 0);
  uint32_t now = 0;
  EXPECT(g.pointer(true, 100, 100, now));
  EXPECT(!g.pointer(true, 100, 100, now + 20));  // held: no repeat edge
  g.pointer(false, 0, 0, now + 40);
  EXPECT(!g.open());
}

void test_hotspot_never_reaches_games() {
  ParentGate g(1, 0);
  uint32_t now = 0;
  EXPECT(!tap(g, ParentGate::kHotspot, now));
  EXPECT(!g.open());
}

void test_short_hold_or_sliding_off_does_not_open() {
  ParentGate g(1, 0);
  uint32_t now = 0;
  hold_hotspot(g, ParentGate::kHoldMs - 200, now);
  EXPECT(!g.open());
  // Start on the hotspot, slide off before 5 s, keep holding.
  g.pointer(true, hx(ParentGate::kHotspot), hy(ParentGate::kHotspot), now);
  g.pointer(true, 100, 100, now + 1000);
  g.pointer(true, 100, 100, now + 6000);
  EXPECT(!g.open());
}

void test_hold_opens_challenge_with_three_distinct_answers() {
  for (uint32_t seed = 1; seed < 200; ++seed) {
    ParentGate g(seed, 0);
    uint32_t now = 0;
    hold_hotspot(g, ParentGate::kHoldMs + 40, now);
    EXPECT(g.state() == ParentGate::State::CHALLENGE);
    EXPECT(g.sum_a() >= 2 && g.sum_a() <= 7 && g.sum_b() >= 2 && g.sum_b() <= 7);
    EXPECT(correct_index(g) >= 0);
    EXPECT(g.choice(0) != g.choice(1) && g.choice(1) != g.choice(2) && g.choice(0) != g.choice(2));
  }
}

void test_wrong_answer_just_closes() {
  ParentGate g(7, 0);
  uint32_t now = 0;
  hold_hotspot(g, ParentGate::kHoldMs + 40, now);
  const int wrong = (correct_index(g) + 1) % ParentGate::kChoiceCount;
  tap(g, ParentGate::kChoiceRects[wrong], now);
  EXPECT(!g.open());
  EXPECT(g.level() == 0);
}

void test_parent_sets_level_and_it_is_reported_once() {
  ParentGate g(3, 0);
  uint32_t now = 0;
  open_settings(g, now);
  EXPECT(g.state() == ParentGate::State::SETTINGS);
  tap(g, ParentGate::level_rect(4), now);
  EXPECT(g.level() == 4);
  EXPECT(g.take_preview());
  EXPECT(!g.take_preview());
  EXPECT(!g.take_level_changed());  // only reported on close
  tap(g, ParentGate::kDoneRect, now);
  EXPECT(!g.open());
  EXPECT(g.take_level_changed());
  EXPECT(!g.take_level_changed());
  // Back to muted is also a change; tapping "off" gives no preview.
  open_settings(g, now);
  tap(g, ParentGate::level_rect(0), now);
  EXPECT(!g.take_preview());
  tap(g, ParentGate::kDoneRect, now);
  EXPECT(g.level() == 0 && g.take_level_changed());
}

void test_idle_settings_close_after_timeout() {
  ParentGate g(5, 2);
  uint32_t now = 0;
  open_settings(g, now);
  const uint32_t pressed_at = now;
  tap(g, ParentGate::level_rect(5), now);
  g.pointer(false, 0, 0, pressed_at + ParentGate::kIdleCloseMs - 10);
  EXPECT(g.open());
  g.pointer(false, 0, 0, pressed_at + ParentGate::kIdleCloseMs + 10);
  EXPECT(!g.open());
  EXPECT(g.level() == 5 && g.take_level_changed());
}

void test_level_buttons_fit_screen() {
  for (int l = 0; l < ParentGate::kLevelCount; ++l) EXPECT(ParentGate::level_rect(l).on_screen());
  for (int i = 0; i < ParentGate::kChoiceCount; ++i) EXPECT(ParentGate::kChoiceRects[i].on_screen());
  EXPECT(ParentGate::kDoneRect.on_screen());
}

}  // namespace

int main() {
  test_fresh_board_is_muted_and_closed();
  test_normal_taps_pass_through_once();
  test_hotspot_never_reaches_games();
  test_short_hold_or_sliding_off_does_not_open();
  test_hold_opens_challenge_with_three_distinct_answers();
  test_wrong_answer_just_closes();
  test_parent_sets_level_and_it_is_reported_once();
  test_idle_settings_close_after_timeout();
  test_level_buttons_fit_screen();
  return little_arcade::testing::finish("parent gate tests");
}
