#pragma once
#include <array>
#include "lvgl.h"
#include "model.hpp"
#include "ui/asset_pack.hpp"

namespace hatch_a_pet {
class View {
 public:
  bool create(lv_obj_t* screen, const color_pop::AssetPack& pack, const char** missing);
  void sync(const State& state, bool reduced_motion);

 private:
  struct Layer {
    lv_obj_t* image = nullptr;
    const color_pop::Sprite* sprite = nullptr;
  };
  Layer make(lv_obj_t* parent);
  void show(Layer& layer, const char* name, double x, double y, double sx = 1,
            double sy = 1, double rotation = 0, double opacity = 1, uint32_t tint = 0);
  void hide(Layer& layer);
  const color_pop::AssetPack* pack_ = nullptr;
  Layer background_, clouds_[2], nest_back_, nest_front_, egg_glow_, egg_, decal_, crack_, pet_, shell_top_, shell_bot_;
  Layer top_decal_, bot_decal_;
  Layer badge_glow_, badge_base_, badge_, hand_, home_, mic_, start_;
  Layer choice_glow_no_,choice_glow_yes_,choice_no_,choice_yes_;
  std::array<Layer, 6> shelf_{};
  struct Particle {
    Layer layer;
    double x=0,y=0,vx=0,vy=0,life=0,duration=0,gravity=0,angle=0,angular=0;
    uint32_t color=0;
    const char* shape="particle_star";
  };
  std::array<Particle,64> particles_{};
  uint32_t visual_rng_=0x9E3779B9u;
  double last_elapsed_=0;
  int last_hatches_=0,last_cracks_=0;
  void emit(double x,double y,int count,const char* shape,uint32_t color,double gravity);
  lv_obj_t* veil_ = nullptr;
  lv_obj_t* feedback_ = nullptr;
  uint64_t touch_serial_ = 0;
  double feedback_time_ = -1;
  const char* missing_ = nullptr;
};
}
