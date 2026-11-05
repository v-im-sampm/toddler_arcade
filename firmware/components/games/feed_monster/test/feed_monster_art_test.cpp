#include <vector>

#include "little_arcade/games/feed_monster_art.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;

namespace {

struct Image {
  std::vector<uint32_t> pixels;
  ui::Canvas canvas;

  Image(int w, int h) : pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0), canvas{pixels.data(), w, h} {}

  bool border_clear() const {
    for (int x = 0; x < canvas.w; ++x) {
      if (canvas.at(x, 0) || canvas.at(x, canvas.h - 1)) return false;
    }
    for (int y = 0; y < canvas.h; ++y) {
      if (canvas.at(0, y) || canvas.at(canvas.w - 1, y)) return false;
    }
    return true;
  }
};

}  // namespace

int main() {
  using namespace feed_monster::art;
  Image monster(kMonsterW, kMonsterH);
  paint_monster_image(monster.canvas);
  EXPECT(monster.border_clear());
  EXPECT((monster.canvas.at(kMonsterW / 2, 70) >> 24) == 255);
  EXPECT(ui::count_opaque(monster.canvas) > 7000);

  Image bowl(kBowlW, kBowlH);
  paint_bowl_image(bowl.canvas);
  EXPECT(bowl.border_clear());
  EXPECT(ui::count_opaque(bowl.canvas) > 3000);

  Image snack(kSnackW, kSnackH);
  paint_snack_image(snack.canvas);
  EXPECT(snack.border_clear());
  EXPECT(ui::count_opaque(snack.canvas) > 150);

  Image tile(kTileW, kTileH);
  paint_tile_art(tile.canvas);
  EXPECT(tile.border_clear());
  EXPECT(ui::count_opaque(tile.canvas) > 1200);
  return testing::finish("feed_monster_art_test");
}
