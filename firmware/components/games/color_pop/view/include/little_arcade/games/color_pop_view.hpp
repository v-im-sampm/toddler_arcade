#pragma once
#include <cstdint>

#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/ui/argb_image.hpp"
#include "little_arcade/ui/fx.hpp"
#include "little_arcade/ui/motion.hpp"
#include "lvgl.h"

namespace little_arcade::color_pop {

// Procedural Color Pop renderer in the quality-bar beach style. Only game
// logic owns input; this view paints cached sprites and bounded shared effects.
class ColorPopView {
 public:
  void create(lv_obj_t* parent);
  void sync(const ColorPopGame& game, uint32_t now_ms);

 private:
  struct BubbleCache {
    int color = -1;
    int scale_x = -1;
    int scale_y = -1;
    int rotation = -1;
    int16_t x = -1;
    int16_t y = -1;
    bool visible = false;
  };

  void paint_bubble(ui::ArgbImage& image, Color color);
  void paint_badge(ui::ArgbImage& image, const ui::tok::Hue* hue, bool happy);
  void paint_crab(ui::Canvas& canvas);
  void paint_pattern(ui::Canvas& canvas, Pattern pattern, float cx, float cy, float size, uint32_t color);
  void sync_badge(const ColorPopGame& game);
  void sync_bubbles(const ColorPopGame& game, uint32_t now_ms);
  void sync_effects(const ColorPopGame& game, uint32_t now_ms);

  ui::ArgbImage bubble_art_[kColorCount]{};
  ui::ArgbImage badge_art_[kColorCount + 2]{};
  ui::ArgbImage crab_art_{};
  ui::ArgbImage hand_art_{};
  lv_obj_t* bubbles_[ColorPopGame::kMaxBubbles]{};
  lv_obj_t* shadows_[ColorPopGame::kMaxBubbles]{};
  BubbleCache cache_[ColorPopGame::kMaxBubbles];
  lv_obj_t* badge_ = nullptr;
  lv_obj_t* badge_question_ = nullptr;
  lv_obj_t* hint_ring_ = nullptr;
  lv_obj_t* helper_hand_ = nullptr;
  lv_obj_t* crab_ = nullptr;
  lv_obj_t* intro_play_ = nullptr;
  ui::FxLayer fx_;
  ui::motion::Spring crab_spring_{};
  Phase last_phase_ = Phase::INTRO;
  uint32_t last_round_ = 0;
  uint32_t last_hint_ms_ = 0;
  uint32_t wiggle_start_ms_ = 0;
  int last_wiggle_ = -1;
  int badge_variant_ = -1;
  uint32_t last_ms_ = 0;
  bool have_time_ = false;
};

}  // namespace little_arcade::color_pop
