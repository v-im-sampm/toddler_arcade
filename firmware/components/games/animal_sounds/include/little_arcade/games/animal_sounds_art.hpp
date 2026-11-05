#pragma once
#include <algorithm>
#include <cstdint>

#include "little_arcade/games/animal_sounds.hpp"
#include "little_arcade/ui/paint.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::animal_sounds::art {

namespace pal = ui::palette;

inline constexpr int kAnimalW = 80;
inline constexpr int kAnimalH = 64;
inline constexpr int kClueSize = 20;
inline constexpr int kTileW = 108;
inline constexpr int kTileH = 68;

struct Brush {
  ui::Canvas& canvas;
  float cx;
  float cy;
  float scale;

  float x(float v) const { return cx + v * scale; }
  float y(float v) const { return cy + v * scale; }
  void circle(float px, float py, float r, uint32_t color) const {
    ui::fill_circle(canvas, x(px), y(py), r * scale, color);
    ui::stroke_circle(canvas, x(px), y(py), r * scale, scale, pal::kInk);
  }
  void ellipse(float px, float py, float rx, float ry, uint32_t color, float rot = 0.0f) const {
    ui::blob(canvas, x(px), y(py), rx * scale, ry * scale, rot, color, pal::kInk, scale * 1.2f);
  }
  void dot(float px, float py, float r, uint32_t color) const {
    ui::fill_circle(canvas, x(px), y(py), r * scale, color);
  }
  void triangle(float ax, float ay, float bx, float by, float cx_, float cy_, uint32_t color) const {
    ui::Path p;
    p.move(x(ax), y(ay)).line(x(bx), y(by)).line(x(cx_), y(cy_));
    ui::fill_path(canvas, p, color);
    ui::stroke_path(canvas, p, scale, pal::kInk, true);
  }
  void line(float ax, float ay, float bx, float by, float width = 1.5f) const {
    ui::Path p;
    p.move(x(ax), y(ay)).line(x(bx), y(by));
    ui::stroke_path(canvas, p, width * scale, pal::kInk);
  }
  void eyes(float px, float py, float gap = 7.0f) const {
    for (const float side : {-1.0f, 1.0f}) {
      eye(px + side * gap, py);
    }
  }
  void eye(float px, float py) const {
    ui::blob(canvas, x(px), y(py), 3.0f * scale, 3.8f * scale, 0, pal::kWhite, pal::kInk, scale);
    dot(px + 0.5f, py + 0.6f, 1.6f, pal::kInk);
    dot(px + 1.0f, py - 0.7f, 0.7f, pal::kWhite);
  }
  void cheeks(float px, float py, float gap = 10.0f) const {
    dot(px - gap, py, 2.0f, 0xF5A0A8);
    dot(px + gap, py, 2.0f, 0xF5A0A8);
  }
  void smile(float px, float py, float radius = 4.0f) const {
    ui::Path p;
    p.arc(x(px), y(py), radius * scale, 0.2f, 2.94f, 10);
    ui::stroke_path(canvas, p, scale, pal::kInk);
  }
};

inline void paint_animal_at(ui::Canvas& c, Animal animal, float cx, float cy, float scale) {
  const Brush b{c, cx, cy, scale};
  static constexpr uint32_t kHalos[] = {0xDDF4FF, 0xFFF1B8, 0xF1E8FF, 0xFFE2EA,
                                         0xFFE9D4, 0xFFF0C9, 0xDDF5D8, 0xE9DFFF};
  ui::fill_circle(c, cx, cy + 1.0f * scale, 30.0f * scale, kHalos[static_cast<std::size_t>(animal)]);
  ui::stroke_circle(c, cx, cy + 1.0f * scale, 30.0f * scale, 1.2f * scale, pal::kWhite);
  ui::fill_star(c, cx - 21.0f * scale, cy - 19.0f * scale, 3.0f * scale, 1.4f * scale, pal::kWhite);
  ui::fill_circle(c, cx + 21.0f * scale, cy - 18.0f * scale, 1.7f * scale, pal::kWhite);
  switch (animal) {
    case Animal::COW:
      b.ellipse(-15, 20, 4, 7, pal::kShell);
      b.ellipse(-5, 21, 4, 7, pal::kShell);
      b.ellipse(6, 21, 4, 7, pal::kShell);
      b.ellipse(16, 20, 4, 7, pal::kShell);
      b.ellipse(0, 5, 23, 16, pal::kWhite);
      b.ellipse(-3, -8, 16, 15, pal::kWhite);
      b.ellipse(-17, -11, 5, 8, pal::kWhite, -0.25f);
      b.ellipse(11, -11, 5, 8, pal::kWhite, 0.25f);
      b.ellipse(-9, -22, 3, 4, pal::kYellow);
      b.ellipse(7, -22, 3, 4, pal::kYellow);
      b.ellipse(-12, 4, 6, 5, pal::kInk);
      b.ellipse(12, 0, 5, 6, pal::kInk);
      b.ellipse(0, 1, 10, 7, pal::kPink);
      b.eyes(-3, -10, 7);
      b.dot(-3, 1, 1.1f, 0x9D5968);
      b.dot(3, 1, 1.1f, 0x9D5968);
      b.smile(0, 5, 4);
      b.cheeks(0, -2, 12);
      break;
    case Animal::DUCK:
      b.ellipse(-2, 22, 7, 3, pal::kOrange);
      b.ellipse(9, 22, 7, 3, pal::kOrange);
      b.ellipse(-2, 8, 21, 14, pal::kYellow);
      b.ellipse(6, -8, 12, 12, pal::kYellow);
      b.ellipse(-4, 5, 11, 7, 0xF9C647, -0.2f);
      b.ellipse(-1, -21, 3, 6, pal::kYellow, -0.35f);
      b.triangle(16, -9, 29, -5, 16, -2, pal::kOrange);
      b.eye(8, -10);
      b.cheeks(7, -3, 8);
      break;
    case Animal::SHEEP:
      b.ellipse(0, 6, 22, 17, pal::kWhite);
      for (int i = 0; i < 6; ++i) {
        const float px = -15.0f + static_cast<float>(i % 3) * 15.0f;
        const float py = (i < 3 ? -9.0f : 1.0f);
        ui::fill_circle(c, b.x(px), b.y(py), 8.0f * scale, pal::kWhite);
        ui::stroke_circle(c, b.x(px), b.y(py), 8.0f * scale, scale, pal::kInk);
      }
      b.ellipse(0, 0, 10, 13, pal::kShell);
      b.ellipse(-12, -1, 5, 3, pal::kShell, -0.2f);
      b.ellipse(12, -1, 5, 3, pal::kShell, 0.2f);
      b.eyes(0, -4, 4);
      b.smile(0, 2, 3);
      b.cheeks(0, 0, 8);
      b.ellipse(-9, 23, 4, 3, 0x7D6E68);
      b.ellipse(9, 23, 4, 3, 0x7D6E68);
      break;
    case Animal::PIG:
      b.ellipse(-12, 22, 6, 4, 0xE96E91);
      b.ellipse(12, 22, 6, 4, 0xE96E91);
      b.ellipse(0, 3, 21, 19, pal::kPink);
      b.triangle(-17, -7, -17, -25, -3, -15, 0xF58BA5);
      b.triangle(3, -15, 17, -25, 17, -6, 0xF58BA5);
      b.ellipse(-12, -7, 4, 8, 0xD95F83);
      b.ellipse(12, -7, 4, 8, 0xD95F83);
      b.ellipse(0, 8, 10, 7, 0xFFA8BC);
      b.eyes(0, -5, 7);
      b.dot(-4, 8, 1.5f, 0x9D4964);
      b.dot(4, 8, 1.5f, 0x9D4964);
      b.smile(0, 15, 3);
      b.cheeks(0, 1, 14);
      break;
    case Animal::DOG:
      b.ellipse(0, 4, 20, 19, pal::kOrange);
      b.ellipse(-17, -3, 7, 13, 0xB86F46, -0.25f);
      b.ellipse(17, -3, 7, 13, 0xB86F46, 0.25f);
      b.ellipse(0, 6, 11, 9, 0xFFD9B0);
      b.eyes(0, -3, 7);
      b.dot(0, 5, 3.0f, pal::kInk);
      b.line(0, 7, 0, 10);
      b.smile(0, 9, 4);
      b.ellipse(1, 13, 4, 5, 0xF27B82);
      b.cheeks(0, 2, 12);
      b.line(-14, 12, 14, 12, 3);
      b.triangle(-3, 13, 3, 13, 0, 19, pal::kYellow);
      break;
    case Animal::CAT:
      b.ellipse(0, 4, 20, 19, 0xFFD66B);
      b.triangle(-18, -5, -17, -24, -3, -13, 0xFFD66B);
      b.triangle(3, -13, 17, -24, 18, -5, 0xFFD66B);
      b.triangle(-14, -7, -13, -19, -5, -12, pal::kPink);
      b.triangle(5, -12, 13, -19, 14, -7, pal::kPink);
      b.ellipse(0, 7, 8, 6, pal::kShell);
      b.eyes(0, -3, 7);
      b.triangle(-2, 4, 2, 4, 0, 7, pal::kPink);
      b.line(-5, 8, -20, 5);
      b.line(-5, 10, -20, 12);
      b.line(5, 8, 20, 5);
      b.line(5, 10, 20, 12);
      b.cheeks(0, 4, 12);
      break;
    case Animal::FROG:
      b.ellipse(0, 8, 22, 16, pal::kGreen);
      b.ellipse(-11, -7, 9, 10, 0x73D86F);
      b.ellipse(11, -7, 9, 10, 0x73D86F);
      ui::fill_circle(c, b.x(-11), b.y(-8), 2.0f * scale, pal::kInk);
      ui::fill_circle(c, b.x(11), b.y(-8), 2.0f * scale, pal::kInk);
      b.smile(0, 7, 10);
      b.dot(-7, 12, 2.0f, 0x4BA962);
      b.dot(7, 12, 2.0f, 0x4BA962);
      b.cheeks(0, 5, 15);
      break;
    case Animal::OWL:
      b.ellipse(0, 4, 20, 22, pal::kPurple);
      b.triangle(-16, -10, -19, -25, -5, -15, pal::kPurple);
      b.triangle(5, -15, 19, -25, 16, -10, pal::kPurple);
      b.ellipse(-10, 0, 8, 10, 0xD7B3F0);
      b.ellipse(10, 0, 8, 10, 0xD7B3F0);
      b.circle(-10, -1, 4, pal::kWhite);
      b.circle(10, -1, 4, pal::kWhite);
      ui::fill_circle(c, b.x(-10), b.y(-1), 1.7f * scale, pal::kInk);
      ui::fill_circle(c, b.x(10), b.y(-1), 1.7f * scale, pal::kInk);
      b.triangle(-4, 5, 4, 5, 0, 12, pal::kYellow);
      for (int i = 0; i < 3; ++i) {
        b.ellipse(-17.0f + static_cast<float>(i) * 5.0f, 8, 2, 4, 0xD7B3F0, -0.35f);
        b.ellipse(7.0f + static_cast<float>(i) * 5.0f, 8, 2, 4, 0xD7B3F0, 0.35f);
      }
      b.cheeks(0, 8, 17);
      break;
  }
}

inline void paint_animal(ui::Canvas& c, Animal animal) {
  const float scale = std::min(static_cast<float>(c.w) / kAnimalW, static_cast<float>(c.h) / kAnimalH);
  paint_animal_at(c, animal, static_cast<float>(c.w) * 0.5f, static_cast<float>(c.h) * 0.52f, scale);
}

inline void paint_clue(ui::Canvas& c, Clue clue) {
  const float s = std::min(static_cast<float>(c.w), static_cast<float>(c.h)) / 20.0f;
  const float cx = static_cast<float>(c.w) * 0.5f, cy = static_cast<float>(c.h) * 0.5f;
  const auto x = [=](float v) { return cx + v * s; };
  const auto y = [=](float v) { return cy + v * s; };
  ui::fill_circle(c, cx, cy, 9.0f * s, 0xFFF3BA);
  ui::stroke_circle(c, cx, cy, 9.0f * s, s, pal::kInk);
  switch (clue) {
    case Clue::HOOFPRINT:
      ui::fill_circle(c, x(-2), y(-2), 1.5f * s, pal::kInk);
      ui::fill_circle(c, x(2), y(-2), 1.5f * s, pal::kInk);
      ui::blob(c, x(0), y(3), 3.2f * s, 2.2f * s, 0, pal::kInk, pal::kInk, s);
      break;
    case Clue::WEBBED_FOOT: {
      ui::Path web;
      web.move(x(-3), y(-2)).line(x(-2), y(3)).line(x(2), y(3)).line(x(3), y(-2));
      ui::fill_path(c, web, pal::kInk);
      ui::stroke_path(c, web, 1.2f * s, pal::kInk);
      ui::fill_circle(c, cx, y(4), 2.0f * s, pal::kInk);
      break;
    }
    case Clue::WOOL_CLOUD:
      for (const float px : {-3.5f, 0.0f, 3.5f}) ui::fill_circle(c, x(px), cy, 2.7f * s, pal::kWhite);
      ui::stroke_ellipse(c, cx, cy, 6.0f * s, 3.8f * s, 0, s, pal::kInk);
      break;
    case Clue::CURLY_TAIL: {
      ui::Path curl;
      curl.arc(cx, cy, 4.0f * s, 0.2f, 5.5f, 18);
      ui::stroke_path(c, curl, 1.7f * s, pal::kInk);
      ui::fill_circle(c, x(4), y(-3), 1.2f * s, pal::kInk);
      break;
    }
    case Clue::BONE:
      ui::stroke_ellipse(c, cx, cy, 4.2f * s, 1.7f * s, 0, 2.4f * s, pal::kInk);
      ui::fill_circle(c, x(-5), y(-2), 2.1f * s, pal::kInk);
      ui::fill_circle(c, x(-5), y(2), 2.1f * s, pal::kInk);
      ui::fill_circle(c, x(5), y(-2), 2.1f * s, pal::kInk);
      ui::fill_circle(c, x(5), y(2), 2.1f * s, pal::kInk);
      break;
    case Clue::WHISKERS:
      ui::stroke_ellipse(c, cx, y(2), 2.0f * s, 1.7f * s, 0, s, pal::kInk);
      for (const float side : {-1.0f, 1.0f}) {
        ui::Path whiskers;
        whiskers.line(x(side * 2), y(1)).line(x(side * 7), y(-1));
        whiskers.line(x(side * 2), y(3)).line(x(side * 7), y(4));
        ui::stroke_path(c, whiskers, s, pal::kInk);
      }
      break;
    case Clue::LILY_PAD:
      ui::fill_circle(c, cx, cy, 6.0f * s, pal::kGreen);
      ui::stroke_circle(c, cx, cy, 6.0f * s, s, pal::kInk);
      ui::fill_path(c, ui::Path{}.move(cx, cy).line(x(6), y(-3)).line(x(6), y(3)), 0xFFF3BA);
      break;
    case Clue::FEATHER: {
      ui::Path feather;
      feather.move(cx, y(7)).cubic(x(-5), y(2), x(4), y(-5), cx, y(-7));
      feather.cubic(x(7), y(-4), x(4), y(4), cx, y(7));
      ui::fill_path(c, feather, 0xD7B3F0);
      ui::stroke_path(c, feather, s, pal::kInk, true);
      ui::Path shaft;
      shaft.move(cx, y(7)).line(cx, y(-6));
      ui::stroke_path(c, shaft, s, pal::kInk);
      break;
    }
  }
}

inline void paint_tile_art(ui::Canvas& c) {
  paint_animal_at(c, Animal::COW, 34.0f, 37.0f, 0.52f);
  paint_animal_at(c, Animal::DUCK, 77.0f, 38.0f, 0.52f);
  for (int i = 0; i < 3; ++i) {
    ui::fill_circle(c, 24.0f + static_cast<float>(i) * 10.0f, 10.0f + static_cast<float>(i % 2) * 3.0f, 2.0f,
                    pal::kYellow);
  }
  ui::fill_circle(c, 91.0f, 56.0f, 2.5f, pal::kPink);
}

}  // namespace little_arcade::animal_sounds::art
