#pragma once
// LVGL 9 renderer for Color Pop. Persistent lv_image objects (one per sprite layer, in the
// reference's draw order) are re-pointed at exported frames and transformed each sync; short-
// lived FX (decor bubbles, rings, particles) are drawn by one custom-draw object. The view only
// reads Game state: no gameplay decisions live here.
#include <cstdint>

#include "game/color_pop.hpp"
#include "lvgl.h"
#include "ui/asset_pack.hpp"

namespace color_pop {

class View {
 public:
  // Returns false (and logs the missing sprite name to `missing`) if the pack is incomplete.
  bool create(lv_obj_t* screen, const AssetPack& pack, const char** missing = nullptr);
  void sync(const Game& game);

 private:
  struct Layer {
    lv_obj_t* obj = nullptr;
    const Sprite* sprite = nullptr;
    int32_t x = INT32_MIN, y = INT32_MIN, rot = 0;
    uint32_t sx = 256, sy = 256;
    lv_opa_t opa = LV_OPA_COVER;
    uint32_t color = 0xFFFFFFFF;
    bool hidden = false;
  };
  struct BubbleLayers { Layer glow, body, rim; };
  struct CrabLayers { Layer legs, arm_l, arm_r, body, pupil_l, pupil_r; };
  struct Box { int32_t x1 = 0, y1 = 0, x2 = -1, y2 = -1; void add(int32_t, int32_t, int32_t, int32_t); };

  static void fx_event(lv_event_t* e);
  void draw_fx(lv_layer_t* layer);
  // FX dirty tracking on a coarse tile grid: one union box would cover most of the screen
  // once decor bubbles drift from the sea bubbles to the surface.
  static constexpr int kTile = 20, kTilesX = 320 / kTile, kTilesY = 240 / kTile;
  using Tiles = uint8_t[kTilesY][kTilesX];
  void fx_tiles(const Game& game, Tiles& tiles) const;
  void invalidate_fx(const Tiles& now);

  const Sprite* need(const char* name);
  Layer make(lv_obj_t* parent, const Sprite* sprite, bool mask);
  static void place(Layer& l, const Sprite* sprite, double x, double y, double rot = 0,
                    double sx = 1, double sy = 1, double opa = 1);
  static void tint(Layer& l, uint32_t rgb);
  static void hide(Layer& l);

  const AssetPack* pack_ = nullptr;
  const char* missing_ = nullptr;
  const Game* game_ = nullptr;

  Layer bg_, cloud_[2], shimmer_;
  BubbleLayers bubbles_[6];
  CrabLayers crab_;
  Layer hand_, badge_glow_, badge_, home_, mic_, start_button_;
  lv_obj_t* fx_ = nullptr;
  lv_obj_t* veil_ = nullptr;
  Tiles fx_prev_{};

  const Sprite* shimmer_frames_[16]{};
  const Sprite* bubble_body_[4]{};
  const Sprite* bubble_glow_[4]{};
  const Sprite* bubble_rim_ = nullptr;
  const Sprite* badge_frames_[4][2]{};
  const Sprite* crab_legs_[8]{};
  const Sprite* crab_arm_l_[6]{};
  const Sprite* crab_arm_r_[6]{};
  const Sprite* crab_point_[29]{};
  const Sprite* crab_body_[2][2]{};  // [blink][happy]
  const Sprite* decor_[18]{};
  const Sprite* pcircle_[13]{};
  const Sprite* pstar_[7][6]{};
  const Sprite* prect_[4][12]{};
};

}  // namespace color_pop
