#pragma once
#include <cstdint>

#include "little_arcade/ui/argb_image.hpp"
#include "little_arcade/ui/motion.hpp"
#include "little_arcade/ui/particles.hpp"
#include "little_arcade/ui/tokens.hpp"
#include "lvgl.h"

namespace little_arcade::ui {

// Juice layer shared by every game: particles (sparkles, confetti, bursts,
// rising bubbles, hearts, notes) and expanding rings. Fixed pools of LVGL
// objects created once in create(); hidden when idle. Never exceeds
// tok::kFxMaxParticles particles or tok::kFxMaxRings rings.
//
// Create it last in a view (so it draws above the scene), call burst()/ring()
// on game events and update(now_ms) once per sync().
class FxLayer {
 public:
  static constexpr int kMaxParticles = tok::kFxMaxParticles;
  static constexpr int kMaxRings = tok::kFxMaxRings;

  void create(lv_obj_t* parent, uint32_t seed = 7);
  void clear();
  void set_reduced_motion(bool on) { sim_.set_reduced_motion(on); }

  int burst(float x, float y, int n, const FxEmit& e) { return sim_.emit(x, y, n, e); }
  // Expanding ring from r0 to r1 px over `ms`, fading out (touch ripples, pops).
  void ring(float x, float y, float r0, float r1, uint32_t ms, uint32_t rgb, int width = 2);
  void update(uint32_t now_ms);

  int live_particles() const { return sim_.live(); }

 private:
  struct Ring {
    lv_obj_t* obj = nullptr;
    float x = 0, y = 0, r0 = 0, r1 = 0;
    uint32_t start = 0, ms = 0;
    bool alive = false;
    bool started = false;
  };
  struct Slot {
    lv_obj_t* img = nullptr;
    int shape = -1;
    uint32_t color = 0;
    bool shown = false;
  };

  FxSim<kMaxParticles> sim_{7};
  Slot slots_[kMaxParticles];
  Ring rings_[kMaxRings];
  motion::Clock clock_;
  uint32_t now_ = 0;
};

// Shared full-screen painted background (RGB565, ordered-dithered). Only one
// game view exists at a time, so every scene reuses the same buffers: call
// scene_backdrop().begin(), paint into the canvas, then show() it as the
// first child of the view's parent.
class Backdrop {
 public:
  // Clears and returns the 320x240 ARGB scratch canvas (nullptr px on OOM).
  Canvas begin();
  // Converts the scratch to RGB565 and adds an lv_image at (0, 0).
  lv_obj_t* show(lv_obj_t* parent);
  bool ready() const { return rgb_ != nullptr; }

 private:
  uint16_t* rgb_ = nullptr;
  lv_image_dsc_t dsc_{};
};
Backdrop& scene_backdrop();

// Builds an lv_image for a painted ArgbImage with its pivot at the centre, so
// scale/rotation (squash, wiggle) stay anchored.
lv_obj_t* sprite(lv_obj_t* parent, const ArgbImage& art);
// Places a sprite centred on (cx, cy) with independent x/y scale (1.0 = 100%)
// and rotation in degrees. Cheap when nothing changed.
void place_sprite(lv_obj_t* img, const ArgbImage& art, float cx, float cy, float sx = 1.0f, float sy = 1.0f,
                  float rot_deg = 0.0f);

}  // namespace little_arcade::ui
