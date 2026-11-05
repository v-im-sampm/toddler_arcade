#pragma once
#include <cstdint>

#include "little_arcade/games/animal_sounds.hpp"
#include "little_arcade/ui/argb_image.hpp"
#include "lvgl.h"

namespace little_arcade::animal_sounds {

class AnimalSoundsView {
 public:
  void create(lv_obj_t* parent);
  void sync(const AnimalSoundsGame& game, uint32_t now_ms);
  void reset() { initialised_ = false; }

 private:
  struct CardNode {
    lv_obj_t* shadow = nullptr;
    lv_obj_t* card = nullptr;
    lv_obj_t* image = nullptr;
    int animal = -1;
    int style = -1;
    int16_t x = INT16_MIN;
    int16_t y = INT16_MIN;
    bool visible = false;
  };

  void set_visible(lv_obj_t* obj, bool visible);
  void sync_card(CardNode& node, const Card& card, int index, int hint_index, int celebrating_index,
                 int wiggling_index, uint32_t now_ms);

  ui::ArgbImage animals_[kAnimalCount];
  ui::ArgbImage clues_[kAnimalCount];
  CardNode cards_[AnimalSoundsGame::kMaxCards];
  lv_obj_t* speaker_ = nullptr;
  lv_obj_t* speaker_icon_ = nullptr;
  lv_obj_t* question_ = nullptr;
  lv_obj_t* clue_icon_ = nullptr;
  Phase last_phase_ = Phase::INTRO;
  int wiggle_index_ = -1;
  uint32_t phase_start_ms_ = 0;
  uint32_t wiggle_start_ms_ = 0;
  int sound_state_ = -1;
  bool initialised_ = false;
};

}  // namespace little_arcade::animal_sounds
