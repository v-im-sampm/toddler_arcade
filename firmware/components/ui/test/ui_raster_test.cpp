#include "little_arcade/testing/test_support.hpp"
#include "little_arcade/ui/a8_raster.hpp"
#include "little_arcade/ui/icons.hpp"

using namespace little_arcade;

namespace {
constexpr int kPx = 40;
struct Mask {
  uint8_t a[kPx * kPx];
  uint8_t at(int x, int y) const { return a[y * kPx + x]; }
};
}  // namespace

int main() {
  // Rasterizer: full, empty and half-covered pixels.
  uint8_t px[4 * 4];
  ui::rasterize_a8(px, 4, 4, [](float x, float) { return x < 2.0f; });
  EXPECT(px[0] == 255 && px[1] == 255 && px[2] == 0 && px[3] == 0);
  ui::rasterize_a8(px, 4, 4, [](float x, float) { return x < 1.5f; });
  EXPECT(px[1] == 128);
  ui::rasterize_a8(px, 4, 4, [](float, float) { return false; });
  EXPECT(px[5] == 0);

  const float sq[] = {0, 0, 10, 0, 10, 10, 0, 10};
  EXPECT(ui::in_polygon(5, 5, sq, 4));
  EXPECT(!ui::in_polygon(11, 5, sq, 4));
  EXPECT(ui::seg_dist2(5, 3, 0, 0, 10, 0) == 9.0f);
  EXPECT(ui::seg_dist2(-3, 4, 0, 0, 10, 0) == 25.0f);

  // Navigation icons stay inside their box and keep their holes open.
  Mask home{}, mic{};
  const float s = kPx;
  ui::rasterize_a8(home.a, kPx, kPx, [s](float x, float y) { return ui::home_icon(x, y, s); });
  ui::rasterize_a8(mic.a, kPx, kPx, [s](float x, float y) { return ui::mic_icon(x, y, s); });
  const Mask* icons[] = {&home, &mic};
  for (const Mask* m : icons) {
    EXPECT(m->at(0, 0) == 0 && m->at(kPx - 1, 0) == 0 && m->at(0, kPx - 1) == 0 && m->at(kPx - 1, kPx - 1) == 0);
  }
  EXPECT(home.at(20, 31) == 0);    // door
  EXPECT(home.at(12, 25) == 255);  // wall
  EXPECT(home.at(20, 10) == 255);  // roof
  EXPECT(mic.at(20, 12) == 255);   // capsule
  EXPECT(mic.at(20, 24) < 64);     // gap between capsule and cradle
  EXPECT(mic.at(20, 31) == 255);   // stem

  return testing::finish("ui_raster_test");
}
