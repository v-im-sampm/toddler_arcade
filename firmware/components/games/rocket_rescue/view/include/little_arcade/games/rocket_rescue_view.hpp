#pragma once

#include "little_arcade/games/rocket_rescue.hpp"
#include "little_arcade/games/rocket_rescue_art.hpp"
#include "little_arcade/ui/argb_image.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "lvgl.h"

namespace little_arcade::rocket_rescue {

class RocketRescueView {
 public:
  void create(lv_obj_t* parent);
  void sync(const RocketRescueGame& game, uint32_t now_ms);

 private:
  ui::ArgbImage planets_[kPlanetCount]{};
  ui::ArgbImage rocket_art_{};
  ui::ArgbImage star_art_{};
  lv_obj_t* planet_[kPlanetCount]{};
  lv_obj_t* stars_[RocketRescueGame::kStarCount]{};
  lv_obj_t* rocket_ = nullptr;
  lv_obj_t* prompt_ = nullptr;
  lv_obj_t* prompt_icon_ = nullptr;
  lv_obj_t* prompt_question_ = nullptr;
  lv_obj_t* go_ = nullptr;
  lv_obj_t* stop_ = nullptr;
  int shown_looks_[kPlanetCount] = {-1, -1, -1};
  bool initialized_ = false;
};

}  // namespace little_arcade::rocket_rescue
