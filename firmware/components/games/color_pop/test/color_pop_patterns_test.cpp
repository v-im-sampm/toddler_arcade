#include "little_arcade/games/color_pop_art.hpp"
#include "little_arcade/games/color_pop_patterns.hpp"
#include "little_arcade/testing/test_support.hpp"
#include "little_arcade/ui/a8_raster.hpp"

using namespace little_arcade;
using namespace little_arcade::color_pop;

namespace {
constexpr int kPx = 40;

struct Mask {
  uint8_t a[kPx * kPx];
  uint8_t at(int x, int y) const { return a[y * kPx + x]; }
  int coverage() const {
    int sum = 0;
    for (uint8_t v : a) sum += v;
    return sum / 255;
  }
};

Mask raster(Pattern p) {
  Mask m{};
  ui::rasterize_a8(m.a, kPx, kPx, [p](float x, float y) { return pattern_inside(p, x, y, kPx); });
  return m;
}

bool corners_clear(const Mask& m) {
  return m.at(0, 0) == 0 && m.at(kPx - 1, 0) == 0 && m.at(0, kPx - 1) == 0 && m.at(kPx - 1, kPx - 1) == 0;
}
}  // namespace

int main() {
  const Pattern all[] = {Pattern::HEART, Pattern::WAVE, Pattern::LEAF, Pattern::STAR};
  int cover[4];
  for (int i = 0; i < 4; ++i) {
    const Mask m = raster(all[i]);
    cover[i] = m.coverage();
    EXPECT(corners_clear(m));
    // Each clue is clearly visible but leaves the bubble colour showing.
    EXPECT(cover[i] > kPx * kPx / 10);
    EXPECT(cover[i] < kPx * kPx * 7 / 10);
  }
  // Shapes are distinct from each other (not colour alone).
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      const Mask a = raster(all[i]), b = raster(all[j]);
      int diff = 0;
      for (int k = 0; k < kPx * kPx; ++k) diff += (a.a[k] > 127) != (b.a[k] > 127) ? 1 : 0;
      EXPECT(diff > kPx * kPx / 20);
    }
  }

  const Mask heart = raster(Pattern::HEART);
  EXPECT(heart.at(20, 20) == 255);
  EXPECT(heart.at(20, 39) == 0);  // below the point
  EXPECT(heart.at(6, 34) == 0);   // beside the point
  const Mask star = raster(Pattern::STAR);
  EXPECT(star.at(20, 21) == 255);
  const Mask leaf = raster(Pattern::LEAF);
  EXPECT(leaf.at(20, 20) < 128);  // transparent vein through the centre
  EXPECT(leaf.at(15, 20) > 200);  // body beside the vein
  const Mask wave = raster(Pattern::WAVE);
  EXPECT(wave.at(20, 20) == 0);  // gap between the two strokes

  // Launcher tile: three coloured bubbles that fit the tile picture.
  static uint32_t tile_px[kTileW * kTileH];
  ui::Canvas tile{tile_px, kTileW, kTileH};
  tile.clear();
  paint_tile_art(tile);
  EXPECT((tile.at(22 - 10, 42) & 0xFFFFFFu) == ui::palette::kRed);
  EXPECT((tile.at(86 - 10, 42) & 0xFFFFFFu) == ui::palette::kYellow);
  EXPECT((tile.at(54 + 10, 26) & 0xFFFFFFu) == ui::palette::kBlue);
  int clue = 0;  // blue bubble's white wave clue (shine is outside this box)
  for (int y = 20; y < 34; ++y) {
    for (int x = 48; x < 62; ++x) clue += tile.at(x, y) == (0xFF000000u | ui::palette::kWhite);
  }
  EXPECT(clue > 5);
  EXPECT((tile.at(0, 0) >> 24) == 0 && (tile.at(kTileW - 1, kTileH - 1) >> 24) == 0);

  return testing::finish("color_pop_patterns_test");
}
