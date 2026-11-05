#include <cstdint>
#include <vector>

#include "little_arcade/games/rainbow_piano_art.hpp"
#include "little_arcade/games/rainbow_piano.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::rainbow_piano;

namespace {

void test_key_shapes_are_visible_and_distinct() {
  std::vector<uint32_t> pixels(art::kIconSize * art::kIconSize);
  std::vector<std::vector<uint32_t>> masks(kKeyCount);
  for (std::size_t i = 0; i < kKeyCount; ++i) {
    ui::Canvas c{pixels.data(), art::kIconSize, art::kIconSize};
    c.clear();
    art::paint_key_shape(c, i, ui::palette::kWhite);
    EXPECT(ui::count_opaque(c) > 80);
    EXPECT((c.at(art::kIconSize / 2, art::kIconSize / 2) >> 24) != 0);
    masks[i] = pixels;
  }
  for (std::size_t i = 0; i < kKeyCount; ++i)
    for (std::size_t j = i + 1; j < kKeyCount; ++j) EXPECT(masks[i] != masks[j]);
}

void test_all_instrument_icons_and_launcher_tile_render() {
  std::vector<uint32_t> icon_pixels(art::kIconSize * art::kIconSize);
  for (std::size_t i = 0; i < kInstrumentCount; ++i) {
    ui::Canvas c{icon_pixels.data(), art::kIconSize, art::kIconSize};
    c.clear();
    art::paint_instrument_icon(c, i);
    EXPECT(ui::count_opaque(c) > 40);
  }

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
  test_key_shapes_are_visible_and_distinct();
  test_all_instrument_icons_and_launcher_tile_render();
  return testing::finish("rainbow_piano_art_test");
}
