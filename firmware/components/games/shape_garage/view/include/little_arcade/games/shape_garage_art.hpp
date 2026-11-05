#pragma once
#include "little_arcade/games/shape_garage.hpp"
#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::shape_garage::art {

namespace pal = ui::palette;

inline constexpr int kRobotImgW = 256;
inline constexpr int kRobotImgH = 128;
inline constexpr int kShapeImgW = 64;
inline constexpr int kShapeImgH = 64;
inline constexpr int kHeadImgW = 48;
inline constexpr int kHeadImgH = 48;
inline constexpr int kTileW = 108;
inline constexpr int kTileH = 68;

inline constexpr uint32_t kShapeColors[kShapeCount] = {pal::kRed, pal::kBlue, pal::kYellow};
inline constexpr uint32_t kRobotColors[ShapeGarageGame::kRobotStyles] = {0x43C6BE, 0xF3A47C, 0x9B83D1};
inline constexpr uint32_t kRobotEdges[ShapeGarageGame::kRobotStyles] = {0x246C78, 0xA85B48, 0x604A91};

inline uint32_t shape_rgb(Shape shape) {
  return kShapeColors[static_cast<std::size_t>(shape)];
}

inline uint32_t robot_rgb(uint8_t style) {
  return kRobotColors[style % ShapeGarageGame::kRobotStyles];
}

inline uint32_t robot_edge_rgb(uint8_t style) {
  return kRobotEdges[style % ShapeGarageGame::kRobotStyles];
}

inline bool shape_inside(Shape kind, float x, float y, float cx, float cy, float size) {
  const float h = size * 0.5f;
  switch (kind) {
    case Shape::CIRCLE: return ui::in_circle(x, y, cx, cy, h);
    case Shape::SQUARE: return x >= cx - h && x <= cx + h && y >= cy - h && y <= cy + h;
    case Shape::TRIANGLE: {
      const float points[] = {cx, cy - h, cx + h, cy + h, cx - h, cy + h};
      return ui::in_polygon(x, y, points, 3);
    }
  }
  return false;
}

inline void paint_shape(ui::Canvas& canvas, Shape kind, float cx, float cy, float size, uint32_t fill, uint32_t edge,
                        float edge_width = 3.0f) {
  const float half = size * 0.5f + edge_width;
  ui::paint(canvas, cx - half, cy - half, cx + half, cy + half, edge, 255,
            [=](float x, float y) { return shape_inside(kind, x, y, cx, cy, size + edge_width * 2); });
  ui::paint(canvas, cx - size * 0.5f, cy - size * 0.5f, cx + size * 0.5f, cy + size * 0.5f, fill, 255,
            [=](float x, float y) { return shape_inside(kind, x, y, cx, cy, size - edge_width * 2); });
  ui::fill_ellipse(canvas, cx - size * 0.16f, cy - size * 0.2f, size * 0.09f, size * 0.06f, -0.5f, pal::kWhite, 190);
}

inline bool rounded_rect(float x, float y, float x0, float y0, float x1, float y1, float radius) {
  const float cx = x < x0 + radius ? x0 + radius : (x > x1 - radius ? x1 - radius : x);
  const float cy = y < y0 + radius ? y0 + radius : (y > y1 - radius ? y1 - radius : y);
  return ui::in_circle(x, y, cx, cy, radius);
}

inline void paint_round_rect(ui::Canvas& canvas, float x0, float y0, float x1, float y1, float radius,
                             uint32_t fill, uint32_t edge, float edge_width = 3.0f) {
  ui::paint(canvas, x0, y0, x1, y1, edge, 255, [=](float x, float y) {
    return rounded_rect(x, y, x0, y0, x1, y1, radius);
  });
  ui::paint(canvas, x0 + edge_width, y0 + edge_width, x1 - edge_width, y1 - edge_width, fill, 255, [=](float x, float y) {
    return rounded_rect(x, y, x0 + edge_width, y0 + edge_width, x1 - edge_width, y1 - edge_width,
                        radius > edge_width ? radius - edge_width : 0.0f);
  });
}

inline void paint_robot(ui::Canvas& canvas, uint8_t style) {
  const uint32_t body = robot_rgb(style), edge = robot_edge_rgb(style);
  paint_round_rect(canvas, 1, 42, 40, 84, 15, body, edge, 3);
  paint_round_rect(canvas, 216, 42, 255, 84, 15, body, edge, 3);
  paint_round_rect(canvas, 16, 8, 240, 125, 29, body, edge, 4);
  paint_round_rect(canvas, 28, 22, 228, 113, 22, style % 3 == 1 ? 0xFFD8C5 : 0xB9F0E7, edge, 2);
  ui::fill_circle(canvas, 20, 61, 5, pal::kYellow);
  ui::stroke_circle(canvas, 20, 61, 5, 1.5f, edge);
  ui::fill_circle(canvas, 236, 61, 5, pal::kYellow);
  ui::stroke_circle(canvas, 236, 61, 5, 1.5f, edge);
  ui::fill_circle(canvas, 43, 102, 4, pal::kWhite);
  ui::fill_circle(canvas, 213, 102, 4, pal::kWhite);
  ui::fill_ellipse(canvas, 128, 119, 48, 5, 0, edge, 100);
}

inline void paint_shape_icon(ui::Canvas& canvas, Shape kind, bool slot) {
  const uint32_t fill = slot ? 0x537F8E : shape_rgb(kind);
  const uint32_t edge = slot ? 0xD9F7F2 : pal::kInk;
  paint_shape(canvas, kind, kShapeImgW * 0.5f, kShapeImgH * 0.5f, slot ? 44.0f : 42.0f, fill, edge, 3.0f);
}

inline void paint_head_icon(ui::Canvas& canvas) {
  const uint32_t edge = robot_edge_rgb(0);
  paint_round_rect(canvas, 6, 8, 42, 39, 11, 0xE9FCFF, edge, 3);
  ui::fill_circle(canvas, 17, 21, 4, pal::kWhite);
  ui::fill_circle(canvas, 31, 21, 4, pal::kWhite);
  ui::fill_circle(canvas, 18, 22, 2, pal::kInk);
  ui::fill_circle(canvas, 32, 22, 2, pal::kInk);
  ui::Path smile;
  smile.move(18, 27).quad(24, 34, 30, 27);
  ui::stroke_path(canvas, smile, 2.5f, pal::kInk);
  ui::Path antenna;
  antenna.move(24, 8).line(24, 6).line(28, 6);
  ui::stroke_path(canvas, antenna, 2.5f, edge);
  ui::fill_circle(canvas, 30, 7, 2.5f, pal::kYellow);
  ui::stroke_circle(canvas, 30, 7, 2.5f, 1.2f, edge);
}

inline void paint_tile_art(ui::Canvas& canvas) {
  paint_round_rect(canvas, 6, 7, 102, 63, 16, 0x43C6BE, 0x246C78, 3);
  paint_round_rect(canvas, 23, 12, 72, 58, 12, 0xE9FCFF, 0x246C78, 2);
  paint_head_icon(canvas);
  paint_shape(canvas, Shape::CIRCLE, 68, 34, 17, pal::kRed, pal::kInk, 1.5f);
  paint_shape(canvas, Shape::SQUARE, 82, 43, 17, pal::kBlue, pal::kInk, 1.5f);
  paint_shape(canvas, Shape::TRIANGLE, 94, 29, 16, pal::kYellow, pal::kInk, 1.5f);
}

}  // namespace little_arcade::shape_garage::art
