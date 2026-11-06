#include <cstdint>
#include <vector>

#include "little_arcade/testing/test_support.hpp"
#include "little_arcade/ui/motion.hpp"
#include "little_arcade/ui/particles.hpp"
#include "little_arcade/ui/shade.hpp"

using namespace little_arcade;
using namespace little_arcade::ui;

namespace {
uint32_t ch(uint32_t p, int shift) { return (p >> shift) & 0xFFu; }
}  // namespace

int main() {
  // mix / ramp3: endpoints and ordering of the 3-tone ramp.
  EXPECT(mix(0x000000, 0xFFFFFF, 0.0f) == 0x000000u);
  EXPECT(mix(0x000000, 0xFFFFFF, 1.0f) == 0xFFFFFFu);
  EXPECT(ramp3(tok::kRed, 0.0f) == tok::kRed.light);
  EXPECT(ramp3(tok::kRed, 0.55f) == tok::kRed.base);
  EXPECT(ramp3(tok::kRed, 1.0f) == tok::kRed.shadow);

  std::vector<uint32_t> px(64 * 64, 0);
  Canvas c{px.data(), 64, 64};

  // Shaded ball: lit upper-left is lighter than the lower-right, edge is outlined.
  shaded_ball(c, 32, 32, 26, tok::kBlue);
  const uint32_t lit = c.at(24, 22), dark = c.at(46, 46);
  EXPECT((lit >> 24) == 255 && (dark >> 24) == 255);
  EXPECT(ch(lit, 0) + ch(lit, 8) + ch(lit, 16) > ch(dark, 0) + ch(dark, 8) + ch(dark, 16));
  EXPECT((c.at(1, 1) >> 24) == 0);  // corners stay transparent
  gloss(c, 32, 32, 26);
  const uint32_t g = c.at(22, 20);  // inside the gloss oval: brighter than before
  EXPECT(ch(g, 16) + ch(g, 8) + ch(g, 0) >= ch(lit, 16) + ch(lit, 8) + ch(lit, 0));

  // Vertical gradient: top and bottom rows take the end colours.
  c.clear();
  fill_vgradient(c, 0, 0, 64, 64, 0x000000, 0xFF0000);
  EXPECT(c.at(10, 0) == 0xFF000000u);
  EXPECT(ch(c.at(10, 63), 16) == 255);
  EXPECT(ch(c.at(10, 32), 16) > 100 && ch(c.at(10, 32), 16) < 160);

  // Dithered RGB565: flat white stays white, flat black stays black, and a
  // mid-grey dithers between neighbouring levels only.
  std::vector<uint16_t> out(64 * 64);
  c.clear();
  fill_rect(c, 0, 0, 64, 64, 0xFFFFFF);
  to_rgb565_dithered(c, out.data());
  EXPECT(out[0] == 0xFFFF && out[64 * 64 - 1] == 0xFFFF);
  fill_rect(c, 0, 0, 64, 64, 0x000000);
  to_rgb565_dithered(c, out.data());
  EXPECT(out[0] == 0x0000);
  fill_rect(c, 0, 0, 64, 64, 0x848484);
  to_rgb565_dithered(c, out.data());
  for (int i = 0; i < 16; ++i) {
    const uint32_t r5 = out[i] >> 11;
    EXPECT(r5 == 16 || r5 == 17);
  }

  // Rounded rect: corners cut, centre inside.
  EXPECT(!in_round_rect(0.5f, 0.5f, 0, 0, 40, 40, 10));
  EXPECT(in_round_rect(20, 20, 0, 0, 40, 40, 10));

  // Spring: a press kick overshoots then settles on target.
  motion::Spring s;
  s.kick(-3.0f);
  float lowest = 1.0f;
  for (int i = 0; i < 120; ++i) {
    s.step(1.0f / 30.0f, 1.0f);
    lowest = s.x < lowest ? s.x : lowest;
  }
  EXPECT(lowest < 0.95f);
  EXPECT(s.settled(1.0f, 0.01f));
  // Large frame gaps stay stable (dt is clamped).
  s.kick(5.0f);
  for (int i = 0; i < 50; ++i) s.step(0.5f, 1.0f);
  EXPECT(s.x > 0.9f && s.x < 1.1f);

  // Easing and safety-related motion helpers.
  EXPECT(motion::ease_out_back(0.0f) == 0.0f);
  EXPECT(motion::ease_out_back(1.0f) > 0.999f && motion::ease_out_back(1.0f) < 1.001f);
  EXPECT(motion::ease_out_back(0.7f) > 1.0f);  // overshoot
  EXPECT(motion::wiggle_deg(tok::kWiggleMs) == 0.0f);
  for (uint32_t t = 0; t < 2000; t += 7) {
    const float gl = motion::glow(t);
    EXPECT(gl >= 0.0f && gl <= 1.0f);
    const float w = motion::wiggle_deg(t);
    EXPECT(w <= tok::kWiggleDeg && w >= -tok::kWiggleDeg);
  }
  EXPECT(tok::kGlowPeriodMs >= 334);  // <= 3 flashes per second

  // Particles: never more than capacity live; oldest is recycled; reduced
  // motion halves emission; everything dies after its life.
  FxSim<tok::kFxMaxParticles> sim(3);
  EXPECT(sim.emit(10, 10, 30, fx::sparkle()) == 30);
  EXPECT(sim.emit(10, 10, 40, fx::confetti()) == 40);
  EXPECT(sim.live() == tok::kFxMaxParticles);
  EXPECT(tok::kFxMaxParticles <= 96);
  sim.clear();
  sim.set_reduced_motion(true);
  EXPECT(sim.emit(0, 0, 16, fx::sparkle()) == 8);
  EXPECT(sim.live() == 8);
  for (int i = 0; i < 100; ++i) sim.step(0.05f);
  EXPECT(sim.live() == 0);
  // Rising bubbles move up.
  sim.set_reduced_motion(false);
  sim.emit(100, 200, 1, fx::bubbles());
  const float y0 = sim.at(0).y;
  sim.step(0.3f);
  EXPECT(sim.at(0).y < y0);

  return little_arcade::testing::finish("ui_quality_kit_test");
}
