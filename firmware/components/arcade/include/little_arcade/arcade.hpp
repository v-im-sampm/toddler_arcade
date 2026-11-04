#pragma once
#include <cstdint>

#include "little_arcade/games/color_pop.hpp"
#include "little_arcade/games/color_pop_view.hpp"
#include "little_arcade/games/hatch_a_pet.hpp"
#include "little_arcade/games/hatch_a_pet_view.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/launcher_view.hpp"
#include "lvgl.h"

namespace little_arcade {

// The arcade shell shared by the device firmware and the desktop simulator:
// the games, the SceneManager (launcher, Home, sleep) and one LVGL layer per
// scene. Only the visible layer is synced each frame. Runs on the UI task.
class Arcade {
 public:
  enum class Layer : uint8_t { NONE, MENU, COLOR_POP, HATCH_A_PET };

  Arcade(IFeedback& feedback, uint32_t seed);
  Arcade(const Arcade&) = delete;
  Arcade& operator=(const Arcade&) = delete;

  // Builds every scene's objects once on `screen`.
  void create(lv_obj_t* screen);
  // Input from the touch panel (screen coordinates) or the voice pipeline.
  void touch(int16_t x, int16_t y, uint32_t now_ms);
  void voice(Intent intent, uint32_t now_ms);
  // Advances game time and syncs the visible view. Call once per UI loop.
  void tick(uint32_t now_ms);

  const SceneManager& scenes() const { return scenes_; }
  Layer layer() const { return shown_; }
  const color_pop::ColorPopGame& color_pop() const { return color_pop_; }
  const hatch_a_pet::HatchAPetGame& hatch_a_pet() const { return hatch_a_pet_; }
  const hatch_a_pet::HatchAPetView& hatch_a_pet_view() const { return hatch_view_; }

 private:
  static constexpr std::size_t kGameCount = 2;

  Layer wanted_layer() const;
  void show_layer(Layer layer);

  color_pop::ColorPopGame color_pop_;
  hatch_a_pet::HatchAPetGame hatch_a_pet_;
  GameEntry entries_[kGameCount];
  SceneManager scenes_;

  ui::LauncherView launcher_;
  color_pop::ColorPopView color_view_;
  hatch_a_pet::HatchAPetView hatch_view_;
  ui::ChromeView chrome_;
  lv_obj_t* layers_[4]{};
  lv_obj_t* sleep_overlay_ = nullptr;
  Layer shown_ = Layer::NONE;
  bool sleeping_ = false;
  bool have_tick_ = false;
  uint32_t last_ms_ = 0;
};

}  // namespace little_arcade
