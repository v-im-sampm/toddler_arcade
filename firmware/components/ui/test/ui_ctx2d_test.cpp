#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

#include "little_arcade/testing/test_support.hpp"
#include "little_arcade/ui/ctx2d.hpp"

using namespace little_arcade::ui::c2d;

namespace {
constexpr int W = 320, H = 240;
constexpr uint8_t kBayerRef[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

struct Frame {
  std::vector<uint32_t> px = std::vector<uint32_t>(W * H, 0);
  uint32_t at(int x, int y) const { return px[y * W + x] & 0xFFFFFFu; }
  int r(int x, int y) const { return static_cast<int>((at(x, y) >> 16) & 0xFF); }
  long coverage_red() const {
    long s = 0;
    for (uint32_t p : px) s += (p >> 16) & 0xFF;
    return s;
  }
};

RenderStats draw(DisplayList& list, Frame& f, TileCache* cache = nullptr, const uint32_t* bg = nullptr) {
  Target t;
  t.background = bg;
  t.out32 = f.px.data();
  return Renderer::render(list, t, cache);
}

void test_rect_and_circle_area() {
  DisplayList list;
  EXPECT(list.allocate());
  Context ctx(list);
  ctx.reset();
  ctx.set_fill(rgb(0xFF0000));
  ctx.fill_rect(10.5, 20, 40, 30);  // half-covered columns at both ends
  Frame f;
  draw(list, f);
  EXPECT(f.at(30, 35) == 0xFF0000);
  EXPECT(f.r(10, 35) >= 126 && f.r(10, 35) <= 129);
  EXPECT(f.at(9, 35) == 0 && f.at(51, 35) == 0);
  EXPECT(std::labs(f.coverage_red() / 255 - 40 * 30) <= 2);

  ctx.reset();
  ctx.set_fill(rgb(0xFF0000));
  ctx.begin_path();
  ctx.arc(160, 120, 30, 0, 7);
  ctx.fill();
  Frame c;
  draw(list, c);
  const double area = 3.14159265358979 * 30 * 30;
  EXPECT(std::fabs(c.coverage_red() / 255.0 - area) < area * 0.01);
  EXPECT(c.at(160, 120) == 0xFF0000 && c.at(160 + 32, 120) == 0);
  EXPECT(list.overflows() == 0);
}

void test_stroke_width_and_alpha() {
  DisplayList list;
  EXPECT(list.allocate());
  Context ctx(list);
  ctx.reset();
  ctx.set_stroke(rgb(0xFF0000));
  ctx.set_line_width(4);
  ctx.begin_path();
  ctx.move_to(20, 100);
  ctx.line_to(220, 100);
  ctx.stroke();
  Frame f;
  draw(list, f);
  EXPECT(std::labs(f.coverage_red() / 255 - 200 * 4) <= 4);  // butt caps
  EXPECT(f.at(100, 98) == 0xFF0000 && f.at(100, 101) == 0xFF0000 && f.at(100, 102) == 0);

  ctx.reset();
  ctx.set_line_cap(Cap::ROUND);
  ctx.set_stroke(rgb(0xFF0000));
  ctx.set_line_width(4);
  ctx.begin_path();
  ctx.move_to(20, 100);
  ctx.line_to(220, 100);
  ctx.stroke();
  Frame g;
  draw(list, g);
  const double expect_area = 200 * 4 + 3.14159265 * 4;  // + two half-discs
  EXPECT(std::fabs(g.coverage_red() / 255.0 - expect_area) < 6);

  ctx.reset();  // overlapping joins in one stroke never double-blend
  ctx.set_stroke(rgb(0xFF0000));
  ctx.set_global_alpha(0.5);
  ctx.set_line_width(6);
  ctx.begin_path();
  ctx.move_to(40, 40);
  ctx.line_to(120, 40);
  ctx.line_to(80, 90);
  ctx.close_path();
  ctx.stroke();
  Frame h;
  draw(list, h);
  int max_r = 0;
  for (int i = 0; i < W * H; ++i) max_r = std::max(max_r, static_cast<int>((h.px[i] >> 16) & 0xFF));
  EXPECT(max_r >= 126 && max_r <= 129);
}

void test_transform_gradient_and_lighter() {
  DisplayList list;
  EXPECT(list.allocate());
  Context ctx(list);
  ctx.reset();
  ctx.save();
  ctx.translate(100, 100);
  ctx.scale(2, 1);
  ctx.set_fill(rgb(0x00FF00));
  ctx.fill_rect(0, 0, 10, 10);
  ctx.restore();
  const int g = ctx.create_linear_gradient(0, 0, 0, 100);
  ctx.add_color_stop(g, 0, rgb(0x000000));
  ctx.add_color_stop(g, 1, rgb(0xFF0000));
  ctx.set_fill_gradient(g);
  ctx.fill_rect(200, 0, 20, 100);
  ctx.set_composite(Op::LIGHTER);
  ctx.set_fill(rgb(0x0000FF));
  ctx.fill_rect(200, 0, 20, 100);
  Frame f;
  draw(list, f);
  EXPECT(f.at(119, 105) == 0x00FF00 && f.at(121, 105) == 0);
  EXPECT(std::abs(f.r(210, 50) - 128) <= 3);
  EXPECT((f.at(210, 50) & 0xFF) == 0xFF);  // 'lighter' added blue
}

void test_tile_cache_and_dither() {
  DisplayList list;
  EXPECT(list.allocate());
  Context ctx(list);
  TileCache cache;
  std::vector<uint32_t> bg(W * H, 0x204060);
  Frame f;
  ctx.reset();
  ctx.set_fill(rgb(0xFFFFFF));
  ctx.fill_rect(5, 5, 10, 10);
  RenderStats s = draw(list, f, &cache, bg.data());
  EXPECT(s.tiles_drawn == TileCache::kCols * TileCache::kRows);
  EXPECT(f.at(300, 200) == 0x204060);
  ctx.reset();
  ctx.set_fill(rgb(0xFFFFFF));
  ctx.fill_rect(5, 5, 10, 10);
  s = draw(list, f, &cache, bg.data());
  EXPECT(s.tiles_drawn == 0 && s.rect_count == 0);
  ctx.reset();
  ctx.set_fill(rgb(0xFFFFFF));
  ctx.fill_rect(6, 5, 10, 10);  // moved inside tile (0,0) only
  s = draw(list, f, &cache, bg.data());
  EXPECT(s.tiles_drawn == 1 && s.rect_count == 1);
  EXPECT(s.rects[0][0] == 0 && s.rects[0][1] == 0 && s.rects[0][2] == kTile && s.rects[0][3] == kTile);
  ctx.reset();  // removing the item redraws only that tile too
  s = draw(list, f, &cache, bg.data());
  EXPECT(s.tiles_drawn == 1 && f.at(8, 8) == 0x204060);

  // Pure 565-representable colours survive the dither exactly.
  EXPECT(to565(0xFFFFFF, 3, 1) == 0xFFFF);
  EXPECT(to565(0x000000, 0, 0) == 0x0000);
  // A mid-step colour averages to its true value over the 4x4 matrix.
  long sum = 0;
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x) sum += (to565(0x0C0000, x, y) >> 11) & 0x1F;
  EXPECT(std::fabs(sum / 16.0 * 8 - 12) <= 1.0);
  // The divide-free quantizer matches plain integer division for every input.
  for (uint32_t v = 0; v < 256; ++v) {
    for (int y = 0; y < 4; ++y) {
      for (int x = 0; x < 4; ++x) {
        const uint32_t p = to565((v << 16) | (v << 8) | v, x, y);
        const uint32_t d = (static_cast<uint32_t>(kBayerRef[y][x]) * 255u + 8u) / 16u;
        EXPECT((p >> 11) == (v * 31 + d) / 255 && ((p >> 5) & 0x3F) == (v * 63 + d) / 255 &&
               (p & 0x1F) == (v * 31 + d) / 255);
      }
    }
  }
}

// Runs the job on a std::thread, standing in for the device's second core.
class ThreadWorker final : public Worker {
 public:
  bool start(void (*fn)(void*), void* arg) override {
    thread_ = std::thread(fn, arg);
    ++starts;
    return true;
  }
  void join() override { thread_.join(); }
  int starts = 0;

 private:
  std::thread thread_;
};

void paint_busy_scene(Context& ctx, int frame) {
  ctx.reset();
  for (int i = 0; i < 24; ++i) {
    const double x = 20 + (i * 37 + frame * 3) % 290, y = 20 + (i * 53 + frame * 2) % 200;
    const int g = ctx.create_radial_gradient(x - 6, y - 6, 2, x, y, 22);
    ctx.add_color_stop(g, 0, rgb(0xFFFFFF, 0.9f));
    ctx.add_color_stop(g, 1, rgb(0x3080F0, 0.6f));
    ctx.set_fill_gradient(g);
    ctx.begin_path();
    ctx.arc(x, y, 22, 0, 6.2831853);
    ctx.fill();
    ctx.set_stroke(rgb(0x3B3550));
    ctx.set_line_width(2.5);
    ctx.stroke();
    if (i % 4 == 0) ctx.set_composite(Op::LIGHTER);
    ctx.set_fill(rgb(0xFFE066, 0.5f));
    ctx.fill_rect(x - 4, y + 10, 30, 6);
    ctx.set_composite(Op::SOURCE_OVER);
  }
}

void test_worker_matches_serial() {
  DisplayList list;
  EXPECT(list.allocate());
  Context ctx(list);
  std::vector<uint32_t> bg(W * H);
  for (int i = 0; i < W * H; ++i) bg[i] = 0x102030u + static_cast<uint32_t>(i % W);
  TileCache serial_cache, threaded_cache;
  std::vector<uint32_t> a32(W * H), b32(W * H);
  std::vector<uint16_t> a565(W * H), b565(W * H);
  ThreadWorker worker;
  for (int frame = 0; frame < 6; ++frame) {
    paint_busy_scene(ctx, frame);
    Target ta;
    ta.background = bg.data();
    ta.out32 = a32.data();
    ta.out565 = a565.data();
    Target tb = ta;
    tb.out32 = b32.data();
    tb.out565 = b565.data();
    const RenderStats sa = Renderer::render(list, ta, &serial_cache);
    const RenderStats sb = Renderer::render(list, tb, &threaded_cache, &worker);
    EXPECT(sa.tiles_drawn == sb.tiles_drawn && sa.rect_count == sb.rect_count);
    EXPECT(a32 == b32);
    EXPECT(a565 == b565);
  }
  EXPECT(worker.starts == 6);
}
}  // namespace

int main() {
  test_rect_and_circle_area();
  test_stroke_width_and_alpha();
  test_transform_gradient_and_lighter();
  test_tile_cache_and_dither();
  test_worker_matches_serial();
  return little_arcade::testing::finish("ui_ctx2d_test");
}
