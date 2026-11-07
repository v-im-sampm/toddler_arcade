#pragma once
#include <cstdint>

#include "little_arcade/games/hatch_a_pet.hpp"
#include "little_arcade/ui/ctx2d.hpp"
#include "lvgl.h"

namespace little_arcade::hatch_a_pet {

// Hatch-a-Pet screen: a line-by-line port of v2/reference/hatch_a_pet.html.
// hatch_a_pet_paint records each frame with the canvas-2D software renderer
// (ui::c2d), which redraws only the 32x32 tiles that changed into an RGB565
// frame shown by one full-screen lv_image. The device and the desktop
// simulator run this same code, so they show the same pixels.
//
// Memory (PSRAM on the device, held only while Hatch-a-Pet is open):
// background XRGB 300 KiB + frame RGB565 150 KiB + display list ~140 KiB.
class HatchAPetView {
 public:
  void create(lv_obj_t* parent);
  // Paints the current game state (at most ~60 fps) and then tells the game
  // the frame was drawn (the reference decays the badge press in drawBadge).
  void sync(HatchAPetGame& game, uint32_t now_ms);
  // Frees the frame buffers; called when the game closes.
  void release();

  bool ready() const { return front_ != nullptr; }
  uint32_t frames() const { return frames_; }
  uint32_t tiles_drawn() const { return tiles_drawn_; }
  uint32_t tiles_by_worker() const { return tiles_worker_; }
  uint32_t syncs() const { return syncs_; }
  uint32_t paint_us_avg() const { return syncs_ ? static_cast<uint32_t>(paint_us_sum_ / syncs_) : 0; }
  uint32_t render_us_avg() const { return syncs_ ? static_cast<uint32_t>(render_us_sum_ / syncs_) : 0; }
  uint32_t paint_us_max() const { return paint_us_max_; }
  uint32_t render_us_max() const { return render_us_max_; }
  void reset_stats() {
    frames_ = tiles_drawn_ = tiles_worker_ = syncs_ = paint_us_max_ = render_us_max_ = 0;
    paint_us_sum_ = render_us_sum_ = 0;
  }

 private:
  bool allocate();

  ui::c2d::DisplayList list_;
  ui::c2d::Context ctx_{list_};
  ui::c2d::TileCache cache_;
  uint32_t* background_ = nullptr;
  uint16_t* front_ = nullptr;
  lv_image_dsc_t dsc_{};
  lv_obj_t* image_ = nullptr;
  uint32_t last_paint_ms_ = 0;
  bool painted_ = false;
  uint32_t frames_ = 0;
  uint32_t tiles_drawn_ = 0;
  uint32_t tiles_worker_ = 0;
  uint32_t syncs_ = 0;
  uint64_t paint_us_sum_ = 0, render_us_sum_ = 0;
  uint32_t paint_us_max_ = 0, render_us_max_ = 0;
};

}  // namespace little_arcade::hatch_a_pet
