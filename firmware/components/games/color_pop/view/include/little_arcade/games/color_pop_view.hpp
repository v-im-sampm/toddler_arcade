#pragma once
#include <cstdint>

#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "lvgl.h"

namespace little_arcade::color_pop {

// LVGL renderer for ColorPopGame (layout: docs/mockups/color_pop.html).
// create() builds every object once. sync() reads the game's accessors and
// only touches properties that changed since the previous frame, so a still
// scene invalidates nothing. No heap use after create(). Objects are not
// clickable: the owner routes touches to the game as InputEvents.
class ColorPopView {
 public:
  static constexpr int kPatternPx = 40;

  void create(lv_obj_t* parent);
  void sync(const ColorPopGame& game, uint32_t now_ms);
  // Restart animation timing (called when the game is re-entered).
  void reset() { initialised_ = false; }

 private:
  struct BubbleObjs {
    lv_obj_t* shadow = nullptr;
    lv_obj_t* body = nullptr;
    lv_obj_t* icon = nullptr;
  };
  // Last values applied to LVGL, used to skip redundant invalidations.
  struct BubbleCache {
    int color = -1;
    int16_t x = -1;
    int16_t y = -1;
    int16_t dx = 0;
    int16_t glow_pad = -1;
    bool visible = true;
  };
  static constexpr int kSparkles = 12;

  void sync_badge(const ColorPopGame& game, uint32_t now_ms);
  void sync_bubble(std::size_t i, const ColorPopGame& game, uint32_t now_ms);
  void sync_sparkles(const ColorPopGame& game, uint32_t now_ms);

  ui::A8Image<kPatternPx, kPatternPx> patterns_[kColorCount]{};
  BubbleObjs bubbles_[ColorPopGame::kMaxBubbles];
  BubbleCache cache_[ColorPopGame::kMaxBubbles];
  lv_obj_t* badge_ = nullptr;
  lv_obj_t* badge_fill_ = nullptr;
  lv_obj_t* badge_icon_ = nullptr;
  lv_obj_t* badge_question_ = nullptr;
  lv_obj_t* ring_ = nullptr;
  lv_obj_t* sparkles_[kSparkles]{};

  Phase last_phase_ = Phase::INTRO;
  uint32_t phase_start_ms_ = 0;
  int last_wiggle_ = -1;
  uint32_t wiggle_start_ms_ = 0;
  int badge_color_ = -2;
  int16_t badge_pad_ = -1;
  bool sparkles_visible_ = true;
  bool initialised_ = false;
};

}  // namespace little_arcade::color_pop
