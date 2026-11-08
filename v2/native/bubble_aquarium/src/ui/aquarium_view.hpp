#pragma once
#include "game/aquarium.hpp"
#include "ui/argb_image.hpp"
#include "lvgl.h"
#include <array>
#include <memory>
#include <string>
#include <unordered_map>

namespace aquarium {
class View {
public:
  void create(lv_obj_t* parent, const std::string& asset_directory);
  void sync(const Game& game);
private:
  struct Sprite {
    std::shared_ptr<ArgbImage> image;
    int w, h;
  };
  void load(const std::string& name, int w, int h);
  lv_obj_t* object(lv_obj_t* parent, const std::string& sprite, int x, int y);
  void set(lv_obj_t* object, std::string& previous, const std::string& sprite);
  std::string directory_;
  std::unordered_map<std::string, Sprite> sprites_;
  std::array<lv_obj_t*, 3> fish_{}, buttons_{}, shadows_{};
  std::array<lv_obj_t*, 6> pellets_{};
  std::array<lv_obj_t*, 8> bubbles_{};
  std::array<lv_obj_t*, 4> weed_{};
  std::array<std::string, 3> fish_frame_{}, button_frame_{};
  std::array<std::string, 4> weed_frame_{};
  lv_obj_t* backdrop_ = nullptr;
  lv_obj_t* rays_ = nullptr;
  lv_obj_t* caustics_ = nullptr;
  lv_obj_t* chest_ = nullptr;
  lv_obj_t* visitor_ = nullptr;
  lv_obj_t* surface_ = nullptr;
  lv_obj_t* dim_ = nullptr;
  lv_obj_t* foreground_dim_ = nullptr;
  lv_obj_t* hint_hand_ = nullptr;
  lv_obj_t* start_ = nullptr;
  std::string rays_frame_, caustics_frame_, chest_frame_, visitor_frame_, surface_frame_;
};
}
