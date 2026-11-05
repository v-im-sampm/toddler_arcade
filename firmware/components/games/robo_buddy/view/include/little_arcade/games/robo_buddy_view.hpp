#pragma once

#include <cstdint>

#include "little_arcade/games/robo_buddy.hpp"
#include "lvgl.h"

namespace little_arcade::robo_buddy {

class RoboBuddyView {
 public:
  void create(lv_obj_t* parent);
  void sync(const RoboBuddyGame& game, uint32_t now_ms);
  void reset() {
    shown_mood_ = static_cast<Mood>(UINT8_MAX);
    shown_snack_ = static_cast<Snack>(UINT8_MAX);
  }

 private:
  static lv_obj_t* shape(lv_obj_t* parent, int x, int y, int w, int h, int radius, uint32_t fill,
                         uint32_t border, int border_width);
  static void visible(lv_obj_t* object, bool show);
  void sync_face(Mood mood);
  void sync_snack(Snack snack);

  lv_obj_t* body_ = nullptr;
  lv_obj_t* face_panel_ = nullptr;
  lv_obj_t* antenna_ = nullptr;
  lv_obj_t* antenna_tip_ = nullptr;
  lv_point_precise_t antenna_points_[3]{};
  lv_obj_t* left_arm_ = nullptr;
  lv_obj_t* right_arm_ = nullptr;
  lv_obj_t* left_hand_ = nullptr;
  lv_obj_t* right_hand_ = nullptr;
  lv_obj_t* eyes_[2]{};
  lv_obj_t* pupils_[2]{};
  lv_obj_t* closed_eyes_[2]{};
  lv_point_precise_t closed_eye_points_[2][3]{};
  lv_obj_t* heart_parts_[2][3]{};
  lv_obj_t* mouth_ = nullptr;
  lv_point_precise_t mouth_points_[5]{};
  lv_obj_t* tongue_ = nullptr;
  lv_obj_t* cheek_dots_[2]{};
  lv_obj_t* belly_ = nullptr;
  lv_obj_t* belly_dial_ = nullptr;
  lv_obj_t* food_button_ = nullptr;
  lv_obj_t* food_icon_ = nullptr;
  lv_obj_t* food_marks_[3]{};
  lv_obj_t* sleep_button_ = nullptr;
  lv_obj_t* zeds_[2]{};
  lv_obj_t* hello_button_ = nullptr;
  lv_obj_t* hello_icon_ = nullptr;
  Mood shown_mood_ = static_cast<Mood>(UINT8_MAX);
  Snack shown_snack_ = static_cast<Snack>(UINT8_MAX);
};

}  // namespace little_arcade::robo_buddy
