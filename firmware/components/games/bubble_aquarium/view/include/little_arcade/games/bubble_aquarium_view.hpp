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
  ui::ArgbImage plant_art_{};
  ui::ArgbImage moon_art_{};
  ui::ArgbImage sun_art_{};
  lv_obj_t* fish_[kFishTypeCount]{};
  lv_obj_t* bubbles_[BubbleAquariumGame::kMaxBubbles]{};
  lv_obj_t* food_[BubbleAquariumGame::kMaxFood]{};
  lv_obj_t* plants_[2]{};
  lv_obj_t* visitor_ = nullptr;
  lv_obj_t* food_button_ = nullptr;
  lv_obj_t* bedtime_button_ = nullptr;
  lv_obj_t* bubble_button_ = nullptr;
  lv_obj_t* chest_ = nullptr;
  lv_obj_t* chest_lid_ = nullptr;
  lv_obj_t* moon_icon_ = nullptr;
  lv_obj_t* sun_icon_ = nullptr;
  lv_obj_t* dim_overlay_ = nullptr;
  lv_obj_t* hint_hand_ = nullptr;
};

}  // namespace little_arcade::bubble_aquarium
