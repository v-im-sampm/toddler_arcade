#pragma once

#include <cstdint>

#include "little_arcade/games/rainbow_piano.hpp"
#include "little_arcade/ui/argb_image.hpp"
#include "lvgl.h"

namespace little_arcade::rainbow_piano {

class RainbowPianoView {
 public:
  void create(lv_obj_t* parent);
  void sync(const RainbowPianoGame& game);
  void reset() { initialised_ = false; }

 private:
  ui::ArgbImage key_icons_[kKeyCount];
  ui::ArgbImage instrument_icons_[kInstrumentCount];
  ui::ArgbImage banner_;
  ui::ArgbImage celebrate_icon_;
  lv_obj_t* keys_[kKeyCount]{};
  lv_obj_t* key_icons_obj_[kKeyCount]{};
  lv_obj_t* follow_markers_[kKeyCount]{};
  lv_obj_t* instruments_[kInstrumentCount]{};
  lv_obj_t* celebrate_stars_[3]{};
  lv_obj_t* banner_obj_ = nullptr;
  int shown_follow_ = -2;
  int shown_instrument_ = -1;
  int shown_phase_ = -1;
  bool shown_key_lit_[kKeyCount]{};
  bool initialised_ = false;
};

}  // namespace little_arcade::rainbow_piano
