#include <cstdint>
#include <vector>

#include "little_arcade/games/beat_sharks.hpp"
#include "little_arcade/games/beat_sharks_art.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::beat_sharks;

namespace {

std::vector<uint32_t> raster(int w, int h, void (*painter)(ui::Canvas&)) {
  std::vector<uint32_t> px(static_cast<std::size_t>(w * h));
  ui::Canvas c{px.data(), w, h};
  c.clear();
  painter(c);
  return px;
}

void test_sharks_shrink_from_low_to_high_and_fit() {
  constexpr int S = art::kSharkSize;
  std::vector<std::vector<uint32_t>> all;
  for (std::size_t row = 0; row < kRows; ++row) {
    int previous = 1 << 30;
    for (uint8_t sound = 1; sound <= kSoundsPerRow; ++sound) {
      std::vector<uint32_t> px(S * S);
      ui::Canvas c{px.data(), S, S};
      c.clear();
      art::paint_shark(c, row, sound);
      const int area = ui::count_opaque(c);
      EXPECT(area > 80);
      EXPECT(area < previous);  // size, not just colour, tells the sounds apart
      previous = area;
      // Nothing touches the canvas edge (no clipped tails or fins).
      for (int i = 0; i < S; ++i) {
        EXPECT((c.at(i, 0) >> 24) == 0 && (c.at(i, S - 1) >> 24) == 0);
        EXPECT((c.at(0, i) >> 24) == 0 && (c.at(S - 1, i) >> 24) == 0);
      }
      all.push_back(px);
    }
  }
  for (std::size_t i = 0; i < all.size(); ++i)
    for (std::size_t j = i + 1; j < all.size(); ++j) EXPECT(all[i] != all[j]);
}

void test_shark_fits_cell() {
  EXPECT(art::kSharkSize <= BeatSharksGame::kCellW && art::kSharkSize <= BeatSharksGame::kCellH);
}

void test_buttons_and_tempo_icons_are_distinct() {
  constexpr int S = art::kIconSize;
  std::vector<std::vector<uint32_t>> masks;
  void (*painters[])(ui::Canvas&) = {art::paint_play,   art::paint_stop,   art::paint_record_dot, art::paint_record_stop,
                                     art::paint_sparkle, art::paint_turtle, art::paint_walk_shark, art::paint_rabbit};
  for (auto* paint : painters) {
    auto px = raster(S, S, paint);
    ui::Canvas c{px.data(), S, S};
    EXPECT(ui::count_opaque(c) > 100);
    EXPECT((px[0] >> 24) == 0);
    masks.push_back(px);
  }
  for (std::size_t i = 0; i < masks.size(); ++i)
    for (std::size_t j = i + 1; j < masks.size(); ++j) EXPECT(masks[i] != masks[j]);
  std::vector<uint32_t> px(S * S);
  ui::Canvas c{px.data(), S, S};
  for (std::size_t t = 0; t < kTempoCount; ++t) {
    c.clear();
    art::paint_tempo_icon(c, t);
    EXPECT(px == masks[5 + t]);
  }
}

void test_bubble_fin_and_tile_render() {
  auto bubble = raster(art::kSharkSize, art::kSharkSize, art::paint_bubble);
  ui::Canvas b{bubble.data(), art::kSharkSize, art::kSharkSize};
  const int bubble_area = ui::count_opaque(b);
  EXPECT(bubble_area > 20 && bubble_area < 200);  // faint, much smaller than any shark
  auto fin = raster(art::kFinW, art::kFinH, art::paint_fin);
  ui::Canvas f{fin.data(), art::kFinW, art::kFinH};
  EXPECT(ui::count_opaque(f) > 60);
  auto tile = raster(art::kTileW, art::kTileH, art::paint_tile_art);
  ui::Canvas t{tile.data(), art::kTileW, art::kTileH};
  EXPECT(ui::count_opaque(t) > 1000);
  EXPECT((t.at(0, 0) >> 24) == 0);
  EXPECT((t.at(art::kTileW - 1, art::kTileH - 1) >> 24) == 0);
}

}  // namespace

int main() {
  test_sharks_shrink_from_low_to_high_and_fit();
  test_shark_fits_cell();
  test_buttons_and_tempo_icons_are_distinct();
  test_bubble_fin_and_tile_render();
  return testing::finish("beat_sharks_art_test");
}
