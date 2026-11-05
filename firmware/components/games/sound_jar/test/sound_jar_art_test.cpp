#include <cstdint>
#include <vector>

#include "little_arcade/games/sound_jar.hpp"
#include "little_arcade/games/sound_jar_art.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::sound_jar;

namespace {

void test_critters_shrink_from_low_to_high() {
  std::vector<uint32_t> pixels(art::kCritterSize * art::kCritterSize);
  int previous = 1 << 30;
  for (std::size_t i = 0; i < kPadCount; ++i) {
    ui::Canvas c{pixels.data(), art::kCritterSize, art::kCritterSize};
    c.clear();
    art::paint_critter(c, i);
    const int area = ui::count_opaque(c);
    EXPECT(area > 100);
    EXPECT(area < previous);  // size, not just colour, tells the pads apart
    previous = area;
    EXPECT((c.at(0, 0) >> 24) == 0);
  }
}

void test_effect_and_record_icons_are_distinct() {
  std::vector<uint32_t> pixels(art::kIconSize * art::kIconSize);
  std::vector<std::vector<uint32_t>> masks;
  for (std::size_t i = 0; i < kEffectCount; ++i) {
    ui::Canvas c{pixels.data(), art::kIconSize, art::kIconSize};
    c.clear();
    art::paint_effect_icon(c, i);
    EXPECT(ui::count_opaque(c) > 60);
    masks.push_back(pixels);
  }
  void (*painters[])(ui::Canvas&) = {art::paint_record_dot, art::paint_stop_square, art::paint_sparkle};
  for (auto* paint : painters) {
    ui::Canvas c{pixels.data(), art::kIconSize, art::kIconSize};
    c.clear();
    paint(c);
    EXPECT(ui::count_opaque(c) > 100);
    masks.push_back(pixels);
  }
  for (std::size_t i = 0; i < masks.size(); ++i)
    for (std::size_t j = i + 1; j < masks.size(); ++j) EXPECT(masks[i] != masks[j]);
}

void test_wave_reflects_thumbnail_and_reverse() {
  constexpr int kW = 64, kH = 20;
  std::vector<uint32_t> fwd(kW * kH), rev(kW * kH);
  uint8_t columns[ISampler::kThumbColumns]{};
  for (std::size_t i = 0; i < ISampler::kThumbColumns; ++i) columns[i] = static_cast<uint8_t>(i * 4);
  ui::Canvas a{fwd.data(), kW, kH};
  art::paint_wave(a, columns, ISampler::kThumbColumns, false, art::kWaveColor);
  ui::Canvas b{rev.data(), kW, kH};
  art::paint_wave(b, columns, ISampler::kThumbColumns, true, art::kWaveColor);
  // Quiet on the left, loud on the right; reversed is the mirror image.
  EXPECT((a.at(0, 2) >> 24) == 0);
  EXPECT((a.at(kW - 1, 2) >> 24) != 0);
  EXPECT((b.at(0, 2) >> 24) != 0);
  EXPECT((b.at(kW - 1, 2) >> 24) == 0);
  // Silence still draws a centre line.
  uint8_t silent[ISampler::kThumbColumns]{};
  art::paint_wave(a, silent, ISampler::kThumbColumns, false, art::kWaveColor);
  EXPECT(ui::count_opaque(a) >= kW);
}

void test_launcher_tile_renders() {
  std::vector<uint32_t> tile_pixels(art::kTileW * art::kTileH);
  ui::Canvas tile{tile_pixels.data(), art::kTileW, art::kTileH};
  tile.clear();
  art::paint_tile_art(tile);
  EXPECT(ui::count_opaque(tile) > 1000);
  EXPECT((tile.at(0, 0) >> 24) == 0);
  EXPECT((tile.at(art::kTileW - 1, art::kTileH - 1) >> 24) == 0);
}

}  // namespace

int main() {
  test_critters_shrink_from_low_to_high();
  test_effect_and_record_icons_are_distinct();
  test_wave_reflects_thumbnail_and_reverse();
  test_launcher_tile_renders();
  return testing::finish("sound_jar_art_test");
}
