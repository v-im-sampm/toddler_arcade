#pragma once
// The main menu screen: two big picture tiles, no text. Each tile is a scaled-down snapshot of
// the game's own exported background with a few of its real characters bobbing on top, so the
// menu needs no new artwork. All sprites are borrowed from the games' asset packs.
#include <array>

#include "launcher/shell.hpp"
#include "lvgl.h"

namespace launcher {

struct MenuArt {
  // Color Pop (color_pop.pack): bg, bubble_red/yellow/blue, crab_body_open_happy.
  const lv_image_dsc_t* cp_bg = nullptr;
  std::array<const lv_image_dsc_t*, 3> cp_bubbles{};
  const lv_image_dsc_t* cp_crab = nullptr;
  // Bubble Aquarium (BGRA sprites): backdrop, fish_0_right_happy_0, bubble.
  const lv_image_dsc_t* aq_backdrop = nullptr;
  const lv_image_dsc_t* aq_fish = nullptr;
  const lv_image_dsc_t* aq_bubble = nullptr;
};

class MenuView {
 public:
  // Builds the menu on `screen` (a dedicated LVGL screen). Returns false if art is missing.
  bool create(lv_obj_t* screen, const MenuArt& art);
  void sync(const Shell& shell);

 private:
  struct Actor { lv_obj_t* obj = nullptr; int tile = 0, x = 0, y = 0, cur_y = -9999; float phase = 0, amp = 0; };
  lv_obj_t* image(lv_obj_t* parent, const lv_image_dsc_t* src, int x, int y, int scale);
  void add_actor(lv_obj_t* obj, int tile, int x, int y, float phase, float amp);
  std::array<lv_obj_t*, 2> frames_{}, thumbs_{};
  std::array<Actor, 8> actors_{};
  int actor_count_ = 0;
};

}  // namespace launcher
