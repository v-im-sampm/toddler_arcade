// Host tests for Color Pop on the vendored PixelRoot32 engine: the engine's
// frame loop draws the game, its touch pipeline reaches the game, and its APU
// plays the score. Host runs only; nothing here claims device validation.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "color_pop_sprites.hpp"
#include "core/Engine.h"
#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/games/color_pop_stage.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::color_pop;
using little_arcade::testing::RecordingFeedback;

namespace {
constexpr int W = 320, H = 240;

uint32_t rgb_of(uint16_t c) {
  const uint32_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
  return ((r * 255 / 31) << 16) | ((g * 255 / 63) << 8) | (b * 255 / 31);
}
int dist(uint32_t a, uint32_t b) {
  int d = 0;
  for (int s = 0; s <= 16; s += 8) d += std::abs(static_cast<int>((a >> s) & 0xFF) - static_cast<int>((b >> s) & 0xFF));
  return d;
}

struct Harness {
  Rng rng{7};
  RecordingFeedback fb;
  GameContext ctx{rng, fb};
  ColorPopGame game;
  ColorPopStage& stage = ColorPopStage::instance();
  uint32_t ms = 1000;
  int touches = 0;

  Harness() {
    EXPECT(ColorPopStage::init_runtime());
    game.enter(ctx);
    EXPECT(stage.begin(game));
    stage.set_touch_handler(
        [](void* user, int16_t x, int16_t y, uint32_t now) {
          auto* h = static_cast<Harness*>(user);
          ++h->touches;
          h->game.handle(make_touch_event(x, y, now));
        },
        this);
  }
  ~Harness() {
    stage.end();
    stage.set_touch_handler(nullptr, nullptr);
    game.exit();
  }
  void frames(double seconds) {
    for (int i = 0, n = static_cast<int>(std::lround(seconds * 60)); i < n; ++i) {
      game.step(1.0 / 60);
      stage.frame(ms += 16);
    }
  }
  uint32_t px(int x, int y) const { return rgb_of(stage.framebuffer()[y * W + x]); }
  int float_bubble(bool target) const {
    for (std::size_t i = 0; i < game.bubble_count(); ++i) {
      const Bubble& b = game.bubble(i);
      if ((b.color == game.target()) == target && b.state == BubbleState::FLOAT) return static_cast<int>(i);
    }
    return -1;
  }
};

void test_first_frame_is_full_and_painted() {
  Harness h;
  h.stage.frame(h.ms);
  EXPECT(h.stage.ready());
  EXPECT(h.stage.dirty_count() == 1);
  const DirtyRect r = h.stage.dirty()[0];
  EXPECT(r.x == 0 && r.y == 0 && r.w == W && r.h == H);
  EXPECT(dist(h.px(160, 4), 0xBFE6FF) < 40);   // sky
  EXPECT(dist(h.px(300, 150), 0x8ECDF7) < 60);  // sea
  EXPECT(dist(h.px(200, 236), 0xF3DDB0) < 90);  // sand (speckles allowed)
  EXPECT(dist(h.px(10, 25), 0xFFF3D6) < 20);    // Home key cream
  EXPECT(dist(h.px(28, 33), 0x3B3550) < 20);    // Home key house
}

void test_bubbles_carry_their_colour() {
  Harness h;
  h.frames(2.1);
  EXPECT(h.game.phase() == Phase::PLAY);
  int checked = 0;
  for (std::size_t i = 0; i < h.game.bubble_count(); ++i) {
    const Bubble& b = h.game.bubble(i);
    if (b.state != BubbleState::FLOAT) continue;
    const uint32_t p = h.px(static_cast<int>(std::lround(b.x)), static_cast<int>(std::lround(b.y)) + 22);
    Color best = Color::RED;
    int best_d = 1 << 30;
    for (Color c : {Color::RED, Color::BLUE, Color::GREEN, Color::YELLOW}) {
      const Swatch s = swatch(c);
      const int d = std::min(dist(p, s.base), dist(p, s.shadow));
      if (d < best_d) best_d = d, best = c;
    }
    EXPECT(best == b.color);
    ++checked;
  }
  EXPECT(checked >= 2);
}

void test_incremental_frames_match_a_full_redraw() {
  Harness h;
  h.frames(3.0);
  uint32_t area = 0;
  for (int i = 0; i < h.stage.dirty_count(); ++i) {
    const DirtyRect r = h.stage.dirty()[i];
    EXPECT(r.x >= 0 && r.y >= 0 && r.w > 0 && r.h > 0 && r.x + r.w <= W && r.y + r.h <= H);
    area += static_cast<uint32_t>(r.w * r.h);
  }
  EXPECT(h.stage.dirty_count() > 0);
  EXPECT(area < W * H);
  std::printf("color_pop_pr32: steady frame dirty area %u px in %d rects, avg %u px\n", area, h.stage.dirty_count(),
              h.stage.dirty_px_avg());
  std::vector<uint16_t> incremental(h.stage.framebuffer(), h.stage.framebuffer() + W * H);
  // Same game state, fresh surface: everything is painted from scratch.
  h.stage.end();
  EXPECT(h.stage.begin(h.game));
  h.stage.frame(h.ms);
  EXPECT(std::memcmp(incremental.data(), h.stage.framebuffer(), sizeof(uint16_t) * W * H) == 0);
}

void test_touch_reaches_the_game_through_the_engine() {
  Harness h;
  h.frames(2.1);
  const int i = h.float_bubble(true);
  EXPECT(i >= 0);
  if (i < 0) return;
  const Bubble& b = h.game.bubble(static_cast<std::size_t>(i));
  const int16_t x = static_cast<int16_t>(std::lround(b.x)), y = static_cast<int16_t>(std::lround(b.y));
  const auto before = h.fb.count_of(Cue::SUCCESS);
  h.stage.touch(true, x, y, h.ms);
  h.stage.frame(h.ms += 16);
  EXPECT(h.touches == 1);
  EXPECT(h.game.bubble(static_cast<std::size_t>(i)).state == BubbleState::PRESS);
  h.frames(0.1);  // still held: no second press; squash, then pop
  EXPECT(h.touches == 1);
  EXPECT(h.fb.count_of(Cue::SUCCESS) == before + 1);
  h.stage.touch(false, x, y, h.ms);
  h.stage.frame(h.ms += 16);
  h.stage.touch(true, 300, 150, h.ms);  // open water
  h.stage.frame(h.ms += 16);
  h.stage.touch(false, 300, 150, h.ms);
  EXPECT(h.touches == 2);
}

int32_t peak(const std::vector<int32_t>& v, std::size_t from = 0) {
  int32_t m = 0;
  for (std::size_t i = from; i < v.size(); ++i) m = std::max(m, std::abs(v[i]));
  return m;
}
std::vector<int32_t> render(audio::ISceneAudio& a, float seconds) {
  std::vector<int32_t> out(static_cast<std::size_t>(seconds * 16000), 0);
  for (std::size_t i = 0; i < out.size(); i += 256) a.render_add(out.data() + i, std::min<std::size_t>(256, out.size() - i));
  return out;
}

void test_apu_score() {
  EXPECT(ColorPopStage::init_runtime());
  audio::ISceneAudio& a = ColorPopStage::scene_audio();
  EXPECT(peak(render(a, 0.2f)) == 0);  // idle is silent
  EXPECT(!a.cue(Cue::TAP, 0));         // not ours: AudioFeedback's tone bank plays it
  EXPECT(!a.cue(Cue::NOTE, 0x40));
  EXPECT(a.cue(Cue::SUCCESS, static_cast<uint8_t>(Color::BLUE)));
  const std::vector<int32_t> hit = render(a, 0.6f);
  const int32_t p = peak(hit);
  std::printf("color_pop_pr32: SUCCESS peak %d (%.2f FS)\n", p, p / 32768.0);
  EXPECT(p > 4000 && p < 26000);
  EXPECT(peak(render(a, 0.5f)) == 0);  // the notes end
  for (uint8_t c = 0; c < 4; ++c) {
    EXPECT(a.cue(Cue::PROMPT, c));
    EXPECT(peak(render(a, 0.3f)) > 2000);
    render(a, 0.8f);
  }
  EXPECT(a.cue(Cue::CELEBRATE, 1));
  const int32_t big = peak(render(a, 0.5f));
  std::printf("color_pop_pr32: CELEBRATE peak %d (%.2f FS)\n", big, big / 32768.0);
  EXPECT(big > 4000 && big < 32767);
  a.stop();
  render(a, 0.02f);                    // the stop commands drain
  EXPECT(peak(render(a, 0.5f)) == 0);  // nothing scheduled survives a stop
}

void report_sizes() {
  std::printf("color_pop_pr32: sizeof(Engine) %zu B, baked sprites %zu B\n", sizeof(pixelroot32::core::Engine),
              sprites::kBakedBytes);
}
}  // namespace

int main() {
  report_sizes();
  test_first_frame_is_full_and_painted();
  test_bubbles_carry_their_colour();
  test_incremental_frames_match_a_full_redraw();
  test_touch_reaches_the_game_through_the_engine();
  test_apu_score();
  return testing::finish("color_pop_pr32_test");
}
