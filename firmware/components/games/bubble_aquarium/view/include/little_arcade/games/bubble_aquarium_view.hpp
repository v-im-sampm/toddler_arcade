#pragma once

#include "little_arcade/games/bubble_aquarium.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/argb_image.hpp"
#include "little_arcade/games/bubble_aquarium_art.hpp"
#include "lvgl.h"

namespace little_arcade::bubble_aquarium {

class BubbleAquariumView {
 public:
  void create(lv_obj_t* parent);
  void sync(const BubbleAquariumGame& game, uint32_t now_ms);

 private:
  ui::ArgbImage fish_art_[kFishTypeCount]{};
  ui::ArgbImage bubble_art_{};
  ui::ArgbImage food_art_{};
  ui::ArgbImage visitor_art_[kVisitorCount]{};
  lv_obj_t* fish_[kFishTypeCount]{};
  lv_obj_t* bubbles_[BubbleAquariumGame::kMaxBubbles]{};
  lv_obj_t* food_[BubbleAquariumGame::kMaxFood]{};
  lv_obj_t* visitor_ = nullptr;
  lv_obj_t* food_button_ = nullptr;
  lv_obj_t* bubble_button_ = nullptr;
  lv_obj_t* chest_ = nullptr;
  bool initialized_ = false;
  int shown_phase_ = -1;
};

}  // namespace little_arcade::bubble_aquarium
