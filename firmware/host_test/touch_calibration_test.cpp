#include "little_arcade/testing/test_support.hpp"
#include "little_arcade/touch_calibration.hpp"

using namespace little_arcade::board;

namespace {

void test_pressed_threshold() {
  EXPECT(!ns2009_pressed(0));
  EXPECT(!ns2009_pressed(150));
  EXPECT(ns2009_pressed(151));
  EXPECT(!ns2009_pressed(kNs2009Invalid));
}

void test_axis_clamps() {
  EXPECT(scale_axis(0, 200, 3850, 240) == 0);
  EXPECT(scale_axis(200, 200, 3850, 240) == 0);
  EXPECT(scale_axis(3850, 200, 3850, 240) == 239);
  EXPECT(scale_axis(4095, 200, 3850, 240) == 239);
  EXPECT(scale_axis(2025, 200, 3850, 240) == 119);
}

void test_portrait_matches_upstream() {
  constexpr Orientation portrait{false, false, false};
  const ScreenPoint a = ns2009_to_screen(200, 400, {}, portrait);
  EXPECT(a.x == 0 && a.y == 0);
  const ScreenPoint b = ns2009_to_screen(3850, 3920, {}, portrait);
  EXPECT(b.x == 239 && b.y == 319);
}

void test_landscape_stays_on_screen() {
  const ScreenPoint corners[] = {
      ns2009_to_screen(0, 0), ns2009_to_screen(4095, 0), ns2009_to_screen(0, 4095), ns2009_to_screen(4095, 4095)};
  for (const ScreenPoint& p : corners) {
    EXPECT(p.x >= 0 && p.x < 320);
    EXPECT(p.y >= 0 && p.y < 240);
  }
  // swap_xy: raw Y drives screen x; mirror_x flips raw X onto screen y.
  const ScreenPoint p = ns2009_to_screen(200, 3920);
  EXPECT(p.x == 319 && p.y == 239);
}

}  // namespace

int main() {
  test_pressed_threshold();
  test_axis_clamps();
  test_portrait_matches_upstream();
  test_landscape_stays_on_screen();
  return little_arcade::testing::finish("touch calibration tests");
}
