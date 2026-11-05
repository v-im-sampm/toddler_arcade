#pragma once
#include <cstdint>

#include "little_arcade/games/feed_monster.hpp"
#include "little_arcade/ui/argb_image.hpp"
#include "lvgl.h"

namespace little_arcade::feed_monster {

class FeedMonsterView {
 public:
  void create(lv_obj_t* parent);
  void sync(const FeedMonsterGame& game, uint32_t now_ms);
  void reset() { initialised_ = false; }

 private:
  static constexpr int kSparkles = 7;

  ui::ArgbImage monster_img_;
  ui::ArgbImage bowl_img_;
  ui::ArgbImage snack_img_;
  lv_obj_t* monster_ = nullptr;
  lv_obj_t* bowl_ = nullptr;
  lv_obj_t* snack_ = nullptr;
  lv_obj_t* badge_ = nullptr;
  lv_obj_t* request_snacks_[FeedMonsterGame::kMaxRequest]{};
  lv_obj_t* tummy_dots_[FeedMonsterGame::kMaxRequest]{};
  lv_obj_t* number_cards_[FeedMonsterGame::kNumberButtonCount]{};
  lv_obj_t* number_snacks_[FeedMonsterGame::kNumberButtonCount][FeedMonsterGame::kMaxRequest]{};
  lv_obj_t* hint_ring_ = nullptr;
  lv_obj_t* sparkles_[kSparkles]{};
  Phase last_phase_ = Phase::INTRO;
  uint32_t phase_start_ms_ = 0;
  uint32_t flight_start_ms_ = 0;
  uint8_t last_fed_ = 0;
  uint8_t shown_fed_ = 0xFF;
  uint8_t shown_request_ = 0;
  int32_t shown_monster_y_ = -1;
  int32_t shown_rotation_ = -1;
  bool initialised_ = false;
  bool flight_active_ = false;
};

}  // namespace little_arcade::feed_monster
