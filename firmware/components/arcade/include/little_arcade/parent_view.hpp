#pragma once
#include "little_arcade/settings/parent_gate.hpp"
#include "lvgl.h"

namespace little_arcade {

// Adult-only overlay for ParentGate: the sum challenge and the volume levels
// (0 = off .. 6). Uses digits/text because only a parent sees it. Built once,
// hidden while the gate is closed; drawn above every scene layer.
class ParentView {
 public:
  void create(lv_obj_t* screen);
  void sync(const settings::ParentGate& gate);

 private:
  lv_obj_t* button(lv_obj_t* parent, const Rect& r, lv_obj_t** label_out);

  lv_obj_t* root_ = nullptr;
  lv_obj_t* challenge_ = nullptr;
  lv_obj_t* settings_ = nullptr;
  lv_obj_t* question_ = nullptr;
  lv_obj_t* choice_labels_[settings::ParentGate::kChoiceCount]{};
  lv_obj_t* level_buttons_[settings::ParentGate::kLevelCount]{};
  settings::ParentGate::State shown_ = settings::ParentGate::State::CLOSED;
  uint8_t shown_a_ = 0, shown_b_ = 0;
  int shown_level_ = -1;
};

}  // namespace little_arcade
