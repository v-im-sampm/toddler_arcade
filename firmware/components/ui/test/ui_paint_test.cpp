#include <cstdint>

#include "little_arcade/testing/test_support.hpp"
#include "little_arcade/ui/launcher_art.hpp"
#include "little_arcade/ui/paint.hpp"

using namespace little_arcade;

namespace {
uint32_t alpha(uint32_t p) { return p >> 24; }
uint32_t red(uint32_t p) { return (p >> 16) & 0xFF; }
uint32_t blue(uint32_t p) { return p & 0xFF; }
}  // namespace

int main() {
  // blend_over: opaque source replaces, transparent destination takes the source.
  EXPECT(ui::blend_over(0xFF00FF00u, 0xFF0000u, 255) == 0xFFFF0000u);
  EXPECT(ui::blend_over(0x00000000u, 0x0000FFu, 128) == 0x800000FFu);
  EXPECT(ui::blend_over(0x12345678u, 0xFFFFFFu, 0) == 0x12345678u);
  // Half red over opaque blue: opaque, roughly half of each channel.
  const uint32_t mix = ui::blend_over(0xFF0000FFu, 0xFF0000u, 128);
  EXPECT(alpha(mix) == 255);
  EXPECT(red(mix) >= 126 && red(mix) <= 130);
  EXPECT(blue(mix) >= 125 && blue(mix) <= 129);
  // Half over half: alpha = 0.5 + 0.5 * 0.5.
  EXPECT(alpha(ui::blend_over(0x80FFFFFFu, 0x000000u, 128)) >= 190);

  uint32_t buf[16 * 16];
  ui::Canvas c{buf, 16, 16};
  c.clear();
  EXPECT(ui::count_opaque(c) == 0);

  // A filled circle covers its centre fully, leaves the corners empty and
  // antialiases its edge.
  ui::fill_circle(c, 8, 8, 5.5f, 0x112233);
  EXPECT(c.at(8, 8) == 0xFF112233u);
  EXPECT(c.at(0, 0) == 0);
  bool partial = false;
  for (int x = 0; x < 16; ++x) partial |= alpha(c.at(x, 8)) > 0 && alpha(c.at(x, 8)) < 255;
  EXPECT(partial);

  // The paint box clamps to the canvas: painting off-canvas is safe.
  ui::fill_circle(c, -20, -20, 50, 0xFFFFFF);
  ui::fill_circle(c, 100, 100, 5, 0xFFFFFF);
  EXPECT(c.at(0, 0) == 0xFFFFFFFFu);

  // Stroke rings leave the middle empty.
  c.clear();
  ui::stroke_circle(c, 8, 8, 5, 2, 0x000000);
  EXPECT(alpha(c.at(8, 8)) == 0);
  EXPECT(alpha(c.at(13, 8)) > 200);

  // Rotated ellipse: wide along x at rot = 0, tall at rot = 90 degrees.
  EXPECT(ui::in_ellipse(9, 0, 0, 0, 10, 3));
  EXPECT(!ui::in_ellipse(0, 9, 0, 0, 10, 3));
  EXPECT(ui::in_ellipse(0, 9, 0, 0, 10, 3, 1.5707963f));

  // Arc covers only its angle range (here the lower half: 0..pi, y down).
  EXPECT(ui::in_arc(0, 10, 0, 0, 10, 1, 0.0f, 3.1415926f));
  EXPECT(!ui::in_arc(0, -10, 0, 0, 10, 1, 0.0f, 3.1415926f));
  EXPECT(!ui::in_arc(0, 0, 0, 0, 10, 1, 0.0f, 3.1415926f));

  // Path: a square from lines fills its inside; strokes are round-capped.
  ui::Path sq;
  sq.move(2, 2).line(14, 2).line(14, 14).line(2, 14);
  EXPECT(sq.n == 4);
  EXPECT(sq.filled(8, 8) && !sq.filled(15, 8));
  EXPECT(sq.stroked(8, 2.5f, 1.0f) && !sq.stroked(8, 8, 1.0f));
  EXPECT(!sq.stroked(2, 8, 1.0f) && sq.stroked(2, 8, 1.0f, true));  // closing edge only when closed
  // Curves end exactly at their end point and the capacity is respected.
  ui::Path cv;
  cv.move(0, 0).quad(5, 10, 10, 0).cubic(12, 5, 18, 5, 20, 0);
  EXPECT(cv.pts[2 * (cv.n - 1)] == 20.0f && cv.pts[2 * (cv.n - 1) + 1] == 0.0f);
  ui::Path big;
  big.move(0, 0);
  for (int i = 0; i < 200; ++i) big.line(static_cast<float>(i), 0);
  EXPECT(big.n == ui::Path::kMax);

  // Star and heart helpers cover their centres.
  c.clear();
  ui::fill_star(c, 8, 8, 7, 3, 0xFFFFFF);
  EXPECT(alpha(c.at(8, 8)) == 255 && alpha(c.at(0, 15)) == 0);
  EXPECT(ui::in_heart(10, 10, 10, 10, 10) && !ui::in_heart(10, 0, 10, 10, 10));

  // Launcher logo: rainbow bands outer-to-inner, below the arc stays clear.
  static uint32_t logo_px[ui::kLogoW * ui::kLogoH];
  ui::Canvas logo{logo_px, ui::kLogoW, ui::kLogoH};
  logo.clear();
  ui::paint_logo(logo);
  EXPECT((logo.at(40, 38 - 34) & 0xFFFFFFu) == ui::palette::kRed);
  EXPECT((logo.at(40, 38 - 13) & 0xFFFFFFu) == ui::palette::kBlue);
  EXPECT(alpha(logo.at(40, 38 - 3)) == 0);  // hollow centre
  EXPECT(alpha(logo.at(6, 40)) == 255);     // cloud
  EXPECT(ui::next_arrow_icon(10, 10, 20) && !ui::next_arrow_icon(2, 2, 20));

  return testing::finish("ui_paint_test");
}
