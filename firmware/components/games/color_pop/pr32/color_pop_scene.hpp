#pragma once
#include <cstdint>

#include "core/Scene.h"
#include "frame_surface.hpp"
#include "little_arcade/games/color_pop.hpp"

namespace little_arcade::color_pop {
class ColorPopStage;
}

namespace little_arcade::color_pop::pr32 {

// Color Pop as a PixelRoot32 scene. It draws the game's public state with the
// engine Renderer: baked 4bpp sprites (tools/gen_color_pop_sprites.py) recoloured
// through a per-draw palette slot, plus engine primitives for the moving
// parts. Translucency is pre-mixed against the backdrop colour at that row.
// update() is empty on purpose: ColorPopGame is stepped by the arcade's
// SceneManager (Home, sleep and the parent gate stay shared with every game).
class ColorPopScene final : public pixelroot32::core::Scene {
 public:
  void attach(ColorPopGame* game, FrameSurface* surface, ColorPopStage* stage) {
    game_ = game;
    surface_ = surface;
    stage_ = stage;
    background_ready_ = false;
  }

  void update(unsigned long) override {}
  void draw(pixelroot32::graphics::Renderer& renderer) override;
  void processTouchEvents(pixelroot32::input::TouchEvent* events, uint8_t count) override;

 private:
  void draw_background(pixelroot32::graphics::Renderer& r);

  ColorPopGame* game_ = nullptr;
  FrameSurface* surface_ = nullptr;
  ColorPopStage* stage_ = nullptr;
  bool background_ready_ = false;
};

}  // namespace little_arcade::color_pop::pr32
