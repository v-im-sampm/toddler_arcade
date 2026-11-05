#pragma once

#include <cstdint>

#include "little_arcade/games/sound_jar.hpp"
#include "little_arcade/ui/argb_image.hpp"
#include "lvgl.h"

namespace little_arcade::sound_jar {

class SoundJarView {
 public:
  void create(lv_obj_t* parent);
  void sync(const SoundJarGame& game);
  void reset() { initialised_ = false; }

 private:
  void repaint_wave(const SoundJarGame& game, bool reversed);

  ui::ArgbImage critter_icons_[kPadCount];
  ui::ArgbImage effect_icons_[kEffectCount];
  ui::ArgbImage record_icon_;
  ui::ArgbImage stop_icon_;
  ui::ArgbImage sparkle_icon_;
  ui::ArgbImage star_icon_;
  ui::ArgbImage wave_;
  lv_obj_t* record_ = nullptr;
  lv_obj_t* record_dot_ = nullptr;
  lv_obj_t* stop_square_ = nullptr;
  lv_obj_t* sparkle_ = nullptr;
  lv_obj_t* jar_ = nullptr;
  lv_obj_t* wave_obj_ = nullptr;
  lv_obj_t* playhead_ = nullptr;
  lv_obj_t* fill_ = nullptr;
  lv_obj_t* effects_[kEffectCount]{};
  lv_obj_t* pads_[kPadCount]{};
  lv_obj_t* bend_marker_ = nullptr;
  lv_obj_t* stars_[3]{};
  uint32_t shown_generation_ = 0;
  int shown_reversed_ = -1;
  int shown_phase_ = -1;
  int shown_sparkle_ = -1;
  int shown_effects_ = -1;
  int shown_level_ = -1;
  int shown_fill_ = -1;
  int shown_playhead_ = -2;
  int shown_bend_ = -2;
  int shown_jar_dx_ = 0;
  int shown_jar_lit_ = -1;
  bool shown_pad_lit_[kPadCount]{};
  bool initialised_ = false;
};

}  // namespace little_arcade::sound_jar
