#include <cstdint>
#include <vector>

#include "little_arcade/games/robo_buddy_art.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;

int main() {
  using namespace little_arcade::robo_buddy;
  std::vector<uint32_t> pixels(art::kTileW * art::kTileH);
  ui::Canvas canvas{pixels.data(), art::kTileW, art::kTileH};
  little_arcade::robo_buddy::art::paint_tile_art(canvas);
  EXPECT(ui::count_opaque(canvas) > 700);
  bool robot_color_seen = false;
  for (uint32_t pixel : pixels) {
    robot_color_seen = robot_color_seen || (pixel & 0xFFFFFFu) == art::kRobot;
  }
  EXPECT(robot_color_seen);
  EXPECT((canvas.at(0, 0) >> 24) == 0);
  return testing::finish("robo_buddy_art_test");
}
