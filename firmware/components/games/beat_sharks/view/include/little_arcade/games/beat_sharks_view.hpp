#pragma once

#include <cstdint>

#include "little_arcade/games/beat_sharks.hpp"
#include "little_arcade/ui/argb_image.hpp"
#include "lvgl.h"

namespace little_arcade::beat_sharks {

class BeatSharksView {
 public:
  void create(lv_obj_t* parent);
  void sync(const BeatSharksGame& game);
  void reset() { initialised_ = false; }

 private:
  ui::ArgbImage sharks_[kRows][kSoundsPerRow];
  ui::ArgbImage bubble_;
  ui::ArgbImage play_icon_, stop_icon_;
  ui::ArgbImage record_icon_, record_stop_icon_, sparkle_icon_;
  ui::ArgbImage tempo_icons_[kTempoCount];
  ui::ArgbImage fin_icon_;
  ui::ArgbImage star_icon_;

  lv_obj_t* play_ = nullptr;
  lv_obj_t* play_img_ = nullptr;
  lv_obj_t* record_ = nullptr;
  lv_obj_t* record_img_ = nullptr;
  lv_obj_t* record_fill_ = nullptr;
  lv_obj_t* tempo_ = nullptr;
  lv_obj_t* tempo_img_ = nullptr;
  lv_obj_t* column_ = nullptr;
  // Each cell is one object: a shared per-row style plus the shark (or bubble)
  // as its background image, to keep the LVGL pool cost low.
  lv_style_t cell_styles_[kRows]{};
  lv_obj_t* cells_[kRows][kSteps]{};
  lv_obj_t* fin_ = nullptr;
  lv_obj_t* stars_[3]{};

  int shown_cell_[kRows][kSteps]{};
  int shown_dy_[kRows][kSteps]{};
  int shown_playing_ = -1;
  int shown_record_mode_ = -1;
  int shown_record_dx_ = 0;
  int shown_record_lit_ = -1;
  int shown_level_ = -1;
  int shown_fill_ = -1;
  int shown_tempo_ = -1;
  int shown_column_ = -2;
  int shown_fin_x_ = -1;
  int shown_stars_ = -1;
  bool initialised_ = false;
};

}  // namespace little_arcade::beat_sharks
