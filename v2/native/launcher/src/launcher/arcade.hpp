#pragma once
// Scene glue shared by the desktop and device launchers: owns the menu, both games, their views
// and three LVGL screens, and routes touch through launcher::Shell. Platform mains provide the
// display, touch polling, the AudioFeedback backend and the asset sources.
#include <cstdint>
#include <string>

#include "audio/color_pop_score.hpp"
#include "game/aquarium.hpp"
#include "game/color_pop.hpp"
#include "launcher/aquarium_cues.hpp"
#include "launcher/gated_score.hpp"
#include "launcher/menu_view.hpp"
#include "launcher/shell.hpp"
#include "little_arcade/feedback.hpp"
#include "lvgl.h"
#include "ui/aquarium_view.hpp"
#include "ui/argb_image.hpp"
#include "ui/asset_pack.hpp"
#include "ui/color_pop_view.hpp"

namespace launcher {

class Arcade {
 public:
  Arcade(little_arcade::IFeedback& feedback, uint32_t seed);

  // Attach this to the AudioFeedback before its audio task starts.
  GatedScore& score() { return gate_; }

  // Builds and shows the menu. `aquarium_dir` is the desktop BGRA sprite folder (ignored on the
  // device, which reads the Aquarium `assets` partition). Returns false with `error` set.
  bool create_menu(const color_pop::AssetPack& pack, const std::string& aquarium_dir, std::string* error);
  // Builds both games' screens (Aquarium copies its sprites into PSRAM here on the device).
  bool create_games(const color_pop::AssetPack& pack, const std::string& aquarium_dir, std::string* error);

  // Touch, in 320x240 logical pixels. Mouse on desktop behaves the same.
  void down(double x, double y);
  void move(double x, double y);
  void up();
  // Advances the shell and the active game by dt seconds and syncs the active view.
  void frame(double dt);

  const Shell& shell() const { return shell_; }
  const color_pop::Game& color_pop() const { return cp_; }
  const aquarium::Game& aquarium() const { return aq_; }
  color_pop::ColorPopScore& color_pop_score() { return cp_score_; }

 private:
  void launch(Scene scene);
  void leave(Scene from);

  little_arcade::IFeedback& feedback_;
  color_pop::ColorPopScore cp_score_;
  GatedScore gate_{cp_score_};
  AquariumCues aq_cues_;
  color_pop::Game cp_;
  aquarium::Game aq_;
  Shell shell_;
  MenuView menu_;
  color_pop::View cp_view_;
  aquarium::View aq_view_;
  aquarium::ArgbImage aq_backdrop_, aq_fish_, aq_bubble_;
  lv_obj_t* menu_screen_ = nullptr;
  lv_obj_t* cp_screen_ = nullptr;
  lv_obj_t* aq_screen_ = nullptr;
  bool forwarding_ = false;  // current touch began inside a game (drags go to the game)
};

}  // namespace launcher
