// Replays tools/quality_bar_golden/script.txt through the Color Pop model and
// painter (the same code the device runs) and writes the captured frames as
// PPM, so golden.mjs can diff them against quality_bar.html rendered in Edge.
//
//   color_pop_reference_render [script] [out_dir]
//
// With no out_dir (ctest) it still replays the whole script and checks the
// renderer invariants: no display-list overflow, and the tile-cached
// incremental render equals a full render bit for bit.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/games/color_pop_paint.hpp"
#include "little_arcade/testing/test_support.hpp"

#ifndef LA_QB_SCRIPT
#define LA_QB_SCRIPT "script.txt"
#endif

using namespace little_arcade;
using namespace little_arcade::color_pop;
namespace c2d = little_arcade::ui::c2d;

namespace {
constexpr int W = 320, H = 240;

struct Cmd {
  std::string op;
  int frame = 0;
  double x = 0, y = 0;
};

bool load_script(const char* path, std::vector<Cmd>& cmds, uint32_t& gseed, uint32_t& cseed, int& end) {
  FILE* f = std::fopen(path, "r");
  if (!f) return false;
  char line[256];
  while (std::fgets(line, sizeof line, f)) {
    char op[32] = {};
    double a = 0, b = 0, c = 0;
    const int n = std::sscanf(line, "%31s %lf %lf %lf", op, &a, &b, &c);
    if (n < 1 || op[0] == '#') continue;
    const std::string s = op;
    if (s == "seed" && n >= 3) {
      gseed = static_cast<uint32_t>(a);
      cseed = static_cast<uint32_t>(b);
    } else if (s == "end" && n >= 2) {
      end = static_cast<int>(a);
    } else if (n >= 2) {
      cmds.push_back(Cmd{s, static_cast<int>(a), b, c});
    }
  }
  std::fclose(f);
  return true;
}

void write_ppm(const std::string& path, const uint32_t* px) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  std::fprintf(f, "P6\n%d %d\n255\n", W, H);
  std::vector<uint8_t> row(W * 3);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const uint32_t p = px[y * W + x];
      row[x * 3] = static_cast<uint8_t>(p >> 16);
      row[x * 3 + 1] = static_cast<uint8_t>(p >> 8);
      row[x * 3 + 2] = static_cast<uint8_t>(p);
    }
    std::fwrite(row.data(), 1, row.size(), f);
  }
  std::fclose(f);
}

uint32_t expand565(uint16_t v) {
  const uint32_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
  return ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
}

const Bubble* find_bubble(const ColorPopGame& g, bool target) {
  for (std::size_t i = 0; i < g.bubble_count(); ++i) {
    const Bubble& b = g.bubble(i);
    if (target ? (b.color == g.target() && (b.state == BubbleState::FLOAT || b.state == BubbleState::RISE))
               : (b.color != g.target() && b.state == BubbleState::FLOAT)) {
      return &b;
    }
  }
  return nullptr;
}
}  // namespace

int main(int argc, char** argv) {
  const char* script = argc > 1 ? argv[1] : LA_QB_SCRIPT;
  const std::string out_dir = argc > 2 ? argv[2] : "";
  std::vector<Cmd> cmds;
  uint32_t gseed = 1234, cseed = 99;
  int end = 0;
  EXPECT(load_script(script, cmds, gseed, cseed, end));
  if (cmds.empty()) return testing::finish("color_pop_reference_render");

  c2d::DisplayList list;
  EXPECT(list.allocate(display_list_limits()));
  c2d::Context ctx(list);

  std::vector<uint32_t> bg(W * H), front(W * H), full(W * H), shown(W * H);
  std::vector<uint16_t> front565(W * H);
  ctx.reset();
  paint_background(ctx);
  c2d::Target bt;
  bt.out32 = bg.data();
  c2d::Renderer::render(list, bt, nullptr);

  Rng rng(1);
  NullFeedback fb;
  GameContext gc{rng, fb};
  ColorPopGame game;
  game.enter(gc);
  game.seed(gseed, cseed);

  c2d::TileCache cache;
  uint32_t max_items = 0, max_points = 0, max_contours = 0;
  double total_ms = 0, worst_ms = 0, paint_ms = 0;
  uint64_t sum_points = 0;
  int tiles = 0;
  int worst_frame = 0, mismatches = 0, captured = 0;
  for (int frame = 1; frame <= end; ++frame) {
    for (const Cmd& c : cmds) {
      if (c.frame != frame) continue;
      if (c.op == "tap") {
        game.pointer_down(c.x, c.y);
      } else if (c.op == "tap_target" || c.op == "tap_other") {
        if (const Bubble* b = find_bubble(game, c.op == "tap_target")) game.pointer_down(b->x, b->y);
      }
    }
    game.step(1.0 / 60);
    const auto t0 = std::chrono::steady_clock::now();
    ctx.reset();
    paint_frame(ctx, game, false);
    paint_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    sum_points += list.point_count();
    c2d::Target t;
    t.background = bg.data();
    t.out32 = front.data();
    t.out565 = front565.data();
    tiles += c2d::Renderer::render(list, t, &cache).tiles_drawn;
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    total_ms += ms;
    if (ms > worst_ms) {
      worst_ms = ms;
      worst_frame = frame;
    }
    game.frame_drawn();
    max_items = std::max(max_items, list.item_count());
    max_points = std::max(max_points, list.point_count());
    max_contours = std::max(max_contours, list.contour_total());
    EXPECT(list.overflows() == 0);

    bool capture = false;
    for (const Cmd& c : cmds) capture = capture || (c.op == "capture" && c.frame == frame);
    if (capture) {
      ++captured;
      c2d::Target ft;  // full, uncached render must match the incremental one
      ft.background = bg.data();
      ft.out32 = full.data();
      c2d::Renderer::render(list, ft, nullptr);
      if (std::memcmp(full.data(), front.data(), full.size() * 4) != 0) ++mismatches;
      if (!out_dir.empty()) {
        char name[64];
        std::snprintf(name, sizeof name, "/cpp_%04d.ppm", frame);
        write_ppm(out_dir + name, front.data());
        for (int i = 0; i < W * H; ++i) shown[i] = expand565(front565[i]);
        std::snprintf(name, sizeof name, "/cpp565_%04d.ppm", frame);
        write_ppm(out_dir + name, shown.data());
      }
    }
  }
  EXPECT(mismatches == 0);
  EXPECT(captured > 0);
  std::printf("frames %d captured %d | peak items %u points %u contours %u (limits %u/%u/%u) | list %zu B\n", end,
              captured, max_items, max_points, max_contours, list.limits().items, list.limits().points,
              list.limits().contours, list.bytes());
  std::printf("avg points %.0f, tiles/frame %.1f | host paint avg %.3f ms, raster avg %.3f ms\n",
              static_cast<double>(sum_points) / end, static_cast<double>(tiles) / end, paint_ms / end,
              (total_ms - paint_ms) / end);
  std::printf("host paint+render avg %.3f ms, worst %.3f ms (frame %d) | rounds %u\n", total_ms / end, worst_ms,
              worst_frame, game.rounds_completed());
  return testing::finish("color_pop_reference_render");
}
