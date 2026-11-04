#pragma once
#include <cstdint>

#include "little_arcade/games/hatch_a_pet.hpp"
#include "little_arcade/ui/argb_image.hpp"
#include "lvgl.h"

namespace little_arcade::hatch_a_pet {

// LVGL renderer for HatchAPetGame (layout: docs/mockups/hatch_a_pet.html).
// create() builds every object once. sync() reads the game's accessors and
// only touches properties that changed, so a still scene invalidates nothing.
// Art is painted into PSRAM images lazily (one egg pattern / creature at a
// time, pre-warmed while the child taps the egg) and cached for good.
// Objects are not clickable: the owner routes touches to the game.
class HatchAPetView {
 public:
  void create(lv_obj_t* parent);
  void sync(const HatchAPetGame& game, uint32_t now_ms);
  // Restart animation timing (called when the game is re-entered).
  void reset() { initialised_ = false; }

  // Art rasterization stats (for profiling logs).
  uint32_t rasters() const { return rasters_; }
  uint32_t max_raster_ms() const { return max_raster_ms_; }
  uint32_t total_raster_ms() const { return total_raster_ms_; }

  // Property cache around an lv_obj: setters skip unchanged values.
  struct Sprite {
    lv_obj_t* obj = nullptr;
    int32_t x = INT32_MIN, y = INT32_MIN, w = -1, h = -1;
    int32_t rot = 0, sx = 256, sy = 256, opa = 255;
    const void* src = nullptr;
    bool visible = true;
    void show(bool v);
    void pos(int32_t nx, int32_t ny);
    void size(int32_t nw, int32_t nh);
    void rotation(int32_t deci_deg);
    void scale(int32_t nsx, int32_t nsy);
    void image_opa(int32_t o);
    void bg_opa(int32_t o);
    void source(const lv_image_dsc_t* dsc);
  };

 private:
  static constexpr int kSparkles = 12;
  static constexpr int kHearts = 3;
  static constexpr int kPieces = 3;  // art::Piece WHOLE / TOP / BOTTOM

  const lv_image_dsc_t* egg(EggPattern p, int piece, bool allow_paint);
  const lv_image_dsc_t* creature(Creature c, bool allow_paint);
  bool prewarm(const HatchAPetGame& game);
  void hide_play();
  void sync_badge(const HatchAPetGame& game, uint32_t now_ms);
  void sync_intro(const HatchAPetGame& game, uint32_t now_ms);
  void sync_egg(const HatchAPetGame& game, uint32_t now_ms);
  void sync_hatch(const HatchAPetGame& game, uint32_t now_ms);
  void sync_pet(const HatchAPetGame& game, uint32_t now_ms);
  void place_whole_egg(EggPattern p, int32_t dy, float rot, int crack_points);
  void place_shadow(int32_t cx, int32_t cy, int32_t rx);

  ui::ArgbImage eggs_[kEggPatternCount][kPieces];
  bool egg_painted_[kEggPatternCount][kPieces]{};
  ui::ArgbImage creatures_[kCreatureCount];
  bool creature_painted_[kCreatureCount]{};
  ui::ArgbImage icons_[kEggPatternCount];
  ui::ArgbImage glow_img_;
  ui::ArgbImage heart_img_;

  Sprite shadow_, glow_, whole_, crack_, bottom_, creature_, top_;
  Sprite sparkles_[kSparkles];
  Sprite hearts_[kHearts];
  Sprite yes_ring_, no_card_, yes_card_, yes_icon_;
  Sprite badge_, badge_q_, badge_icon_, badge_crack_;
  lv_point_precise_t crack_pts_[8]{};
  int crack_n_ = -1;
  float crack_rot_ = 0.0f;
  lv_point_precise_t badge_crack_pts_[8]{};

  Phase last_phase_ = Phase::INTRO;
  uint32_t phase_start_ms_ = 0;
  uint8_t last_cracks_ = 0;
  uint32_t wobble_start_ms_ = 0;
  bool wobble_armed_ = false;
  uint32_t last_pets_ = 0;
  uint32_t react_start_ms_ = 0;
  bool react_armed_ = false;
  int badge_state_ = -2;
  int32_t badge_pad_ = -1;
  bool initialised_ = false;

  uint32_t rasters_ = 0;
  uint32_t max_raster_ms_ = 0;
  uint32_t total_raster_ms_ = 0;
};

}  // namespace little_arcade::hatch_a_pet
