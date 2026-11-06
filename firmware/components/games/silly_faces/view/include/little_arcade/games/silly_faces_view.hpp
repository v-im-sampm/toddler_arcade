#pragma once

#include "little_arcade/games/silly_faces.hpp"
#include "little_arcade/games/silly_faces_art.hpp"
#include "little_arcade/ui/argb_image.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "lvgl.h"

namespace little_arcade::silly_faces {

class SillyFacesView {
 public:
  void create(lv_obj_t* parent);
  void sync(const SillyFacesGame& game, uint32_t now_ms);

 private:
  ui::ArgbImage face_art_{};
  lv_obj_t* face_ = nullptr;
  lv_obj_t* eyes_zone_ = nullptr;
  lv_obj_t* mouth_zone_ = nullptr;
  lv_obj_t* show_button_ = nullptr;
  lv_obj_t* mix_button_ = nullptr;
  lv_obj_t* next_button_ = nullptr;
  lv_obj_t* prompt_button_ = nullptr;
  lv_obj_t* stickers_[kEmotionCount]{};
  int shown_face_ = -1;
  int shown_hint_ = -1;
  bool initialized_ = false;
};

}  // namespace little_arcade::silly_faces
