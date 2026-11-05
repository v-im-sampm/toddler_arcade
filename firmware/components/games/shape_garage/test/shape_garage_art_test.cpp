#include <cstdint>
#include <vector>

#include "little_arcade/games/shape_garage_art.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::shape_garage;

namespace {
struct Image {
  std::vector<uint32_t> px;
  ui::Canvas c;
  Image(int w, int h) : px(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0u), c{px.data(), w, h} {}
  bool border_clear() const {
    for (int x = 0; x < c.w; ++x) {
      if ((c.at(x, 0) >> 24) || (c.at(x, c.h - 1) >> 24)) return false;
    }
    for (int y = 0; y < c.h; ++y) {
      if ((c.at(0, y) >> 24) || (c.at(c.w - 1, y) >> 24)) return false;
    }
    return true;
  }
};
}  // namespace

int main() {
  using namespace art;
  EXPECT(shape_inside(Shape::CIRCLE, 32, 32, 32, 32, 40));
  EXPECT(!shape_inside(Shape::CIRCLE, 8, 8, 32, 32, 40));
  EXPECT(shape_inside(Shape::SQUARE, 14, 14, 32, 32, 36));
  EXPECT(!shape_inside(Shape::SQUARE, 10, 32, 32, 32, 36));
  EXPECT(shape_inside(Shape::TRIANGLE, 32, 17, 32, 32, 40));
  EXPECT(!shape_inside(Shape::TRIANGLE, 10, 10, 32, 32, 40));

  Image head(kHeadImgW, kHeadImgH);
  paint_head_icon(head.c);
  EXPECT(head.border_clear());
  EXPECT(ui::count_opaque(head.c) > 400);

  std::vector<std::vector<uint32_t>> variants;
  for (uint8_t style = 0; style < ShapeGarageGame::kRobotStyles; ++style) {
    Image robot(kRobotImgW, kRobotImgH);
    paint_robot(robot.c, style);
    EXPECT(robot.border_clear());
    EXPECT(ui::count_opaque(robot.c) > 18000);
    for (const auto& seen : variants) EXPECT(seen != robot.px);
    variants.push_back(robot.px);
  }

  std::vector<std::vector<uint32_t>> pieces;
  for (std::size_t i = 0; i < kShapeCount; ++i) {
    for (const bool slot : {false, true}) {
      Image icon(kShapeImgW, kShapeImgH);
      paint_shape_icon(icon.c, static_cast<Shape>(i), slot);
      EXPECT(icon.border_clear());
      EXPECT(ui::count_opaque(icon.c) > 900);
      if (!slot) {
        for (const auto& seen : pieces) EXPECT(seen != icon.px);
        pieces.push_back(icon.px);
      }
    }
  }

  Image tile(kTileW, kTileH);
  paint_tile_art(tile.c);
  EXPECT(tile.border_clear());
  EXPECT(ui::count_opaque(tile.c) > 2500);
  return testing::finish("shape_garage_art_test");
}
