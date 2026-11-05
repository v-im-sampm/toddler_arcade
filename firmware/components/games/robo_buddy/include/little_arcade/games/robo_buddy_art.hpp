#pragma once

#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::robo_buddy::art {

inline constexpr int kTileW = 108;
inline constexpr int kTileH = 68;
inline constexpr uint32_t kRobot = 0x43C6BE;
inline constexpr uint32_t kRobotShade = 0x279A99;
inline constexpr uint32_t kRobotEdge = 0x246C78;
inline constexpr uint32_t kFace = 0xE9FCFF;
inline constexpr uint32_t kAntenna = 0xF5C84C;

inline void paint_tile_art(ui::Canvas& canvas) {
  using namespace ui;
  namespace pal = ui::palette;
  constexpr float cx = 54.0f;
  fill_ellipse(canvas, cx, 40, 23, 24, 0, kRobot);
  stroke_ellipse(canvas, cx, 40, 23, 24, 0, 3, kRobotEdge);
  fill_ellipse(canvas, cx, 42, 17, 15, 0, kFace);
  fill_circle(canvas, 48, 39, 3.2f, pal::kInk);
  fill_circle(canvas, 60, 39, 3.2f, pal::kInk);
  fill_circle(canvas, 49, 38, 1.1f, pal::kWhite);
  fill_circle(canvas, 61, 38, 1.1f, pal::kWhite);
  Path smile;
  smile.move(48, 47).quad(54, 53, 60, 47, 8);
  stroke_path(canvas, smile, 2.4f, pal::kInk);
  stroke_path(canvas, Path{}.move(cx, 17).line(cx, 12).line(cx + 6, 8), 3, kRobotEdge);
  fill_circle(canvas, cx + 7, 8, 3.5f, kAntenna);
  fill_circle(canvas, 43, 61, 3, kRobotShade);
  fill_circle(canvas, 65, 61, 3, kRobotShade);
}

}  // namespace little_arcade::robo_buddy::art
