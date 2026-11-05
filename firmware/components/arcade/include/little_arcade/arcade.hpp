#pragma once
#include <cstdint>

#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/games/color_pop_view.hpp"
#include "little_arcade/games/hatch_a_pet.hpp"
#include "little_arcade/games/hatch_a_pet_view.hpp"
#include "little_arcade/games/rainbow_piano.hpp"
#include "little_arcade/games/rainbow_piano_view.hpp"
#include "little_arcade/games/robo_buddy.hpp"
#include "little_arcade/games/robo_buddy_view.hpp"
#include "little_arcade/games/shape_garage.hpp"
#include "little_arcade/games/shape_garage_view.hpp"
#include "little_arcade/parent_view.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/settings/parent_gate.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/launcher_view.hpp"
#include "lvgl.h"

namespace little_arcade {

// The arcade shell shared by the device firmware and the desktop simulator:
// the games, the SceneManager (launcher, Home, sleep) and one LVGL layer per
// scene. Only the visible layer is synced each frame. Runs on the UI task.
class Arcade {
 public:
  enum class Layer : uint8_t { NONE, MENU, COLOR_POP, HATCH_A_PET, RAINBOW_PIANO, ROBO_BUDDY, SHAPE_GARAGE };

  // `volume_level` is the parent-saved level (0 = muted, the default).
  Arcade(IFeedback& feedback, uint32_t seed, uint8_t volume_level = 0);
  Arcade(const Arcade&) = delete;
  Arcade& operator=(const Arcade&) = delete;

  // Builds every scene's objects once on `screen`.
  void create(lv_obj_t* screen);
  // Raw touch-panel state, sampled every UI loop: drives the hidden parent
  // gate (hold the Mic corner 5 s) and forwards new presses to touch().
  void pointer(bool pressed, int16_t x, int16_t y, uint32_t now_ms);
  // A press edge for the launcher/game (bench scripts call this directly).
  void touch(int16_t x, int16_t y, uint32_t now_ms);
  void voice(Intent intent, uint32_t now_ms);
  // Advances game time and syncs the visible view. Call once per UI loop.
  void tick(uint32_t now_ms);

  // Parent-chosen volume (0..6). The caller applies it to the audio output
  // and persists it when take_volume_changed() returns true.
  uint8_t volume_level() const { return gate_.level(); }
  bool take_volume_changed() { return gate_.take_level_changed(); }
  bool parent_gate_open() const { return gate_.open(); }

  const SceneManager& scenes() const { return scenes_; }
  Layer layer() const { return shown_; }
  const color_pop::ColorPopGame& color_pop() const { return color_pop_; }
  const hatch_a_pet::HatchAPetGame& hatch_a_pet() const { return hatch_a_pet_; }
  const hatch_a_pet::HatchAPetView& hatch_a_pet_view() const { return hatch_view_; }
  const rainbow_piano::RainbowPianoGame& rainbow_piano() const { return rainbow_piano_; }
  const robo_buddy::RoboBuddyGame& robo_buddy() const { return robo_buddy_; }
  const shape_garage::ShapeGarageGame& shape_garage() const { return shape_garage_; }

 private:
  static constexpr std::size_t kGameCount = 5;

  Layer wanted_layer() const;
  void show_layer(Layer layer);

  color_pop::ColorPopGame color_pop_;
  hatch_a_pet::HatchAPetGame hatch_a_pet_;
  rainbow_piano::RainbowPianoGame rainbow_piano_;
  robo_buddy::RoboBuddyGame robo_buddy_;
  shape_garage::ShapeGarageGame shape_garage_;
  GameEntry entries_[kGameCount];
  IFeedback& feedback_;
  SceneManager scenes_;
  settings::ParentGate gate_;
  ParentView parent_view_;

  ui::LauncherView launcher_;
  color_pop::ColorPopView color_view_;
  hatch_a_pet::HatchAPetView hatch_view_;
  rainbow_piano::RainbowPianoView rainbow_piano_view_;
  robo_buddy::RoboBuddyView robo_buddy_view_;
  shape_garage::ShapeGarageView shape_garage_view_;
  ui::ChromeView chrome_;
  lv_obj_t* layers_[7]{};
  lv_obj_t* sleep_overlay_ = nullptr;
  Layer shown_ = Layer::NONE;
  bool sleeping_ = false;
  bool have_tick_ = false;
  uint32_t last_ms_ = 0;
};

}  // namespace little_arcade
