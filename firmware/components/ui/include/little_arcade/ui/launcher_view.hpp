#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/scene_manager.hpp"
#include "little_arcade/ui/argb_image.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/fx.hpp"
#include "lvgl.h"

namespace little_arcade::ui {

// Picture for a launcher tile. Each game owns its art; the shell maps ids.
struct TileArt {
  const lv_image_dsc_t* image = nullptr;
  uint32_t bg_rgb = 0xFFFFFF;
};
using TileArtFn = TileArt (*)(GameId id);

// LVGL launcher (layout: docs/mockups/hatch_a_pet.html, menu frames).
// Picture tiles at SceneManager::kTileRects; empty slots are hidden; the
// next-page button and page dots appear only with more than one page.
// Visual only: SceneManager owns paging and launching. No heap after create().
class LauncherView {
 public:
  static constexpr int kTileArtW = 108;
  static constexpr int kTileArtH = 68;

  void create(lv_obj_t* parent, TileArtFn art);
  void sync(const SceneManager& scenes);

 private:
  static constexpr std::size_t kMaxDots = 4;
  struct Tile {
    lv_obj_t* shadow = nullptr;
    lv_obj_t* card = nullptr;
    lv_obj_t* art = nullptr;
    const GameEntry* shown = nullptr;
  };

  TileArtFn art_ = nullptr;
  Tile tiles_[SceneManager::kTilesPerPage];
  Backdrop backdrop_;
  ArgbImage logo_;
  A8Image<32, 32> arrow_{};
  lv_obj_t* pager_ = nullptr;
  lv_obj_t* dots_[kMaxDots]{};
  std::size_t page_shown_ = SIZE_MAX;
  std::size_t pages_shown_ = SIZE_MAX;
  bool initialised_ = false;
};

}  // namespace little_arcade::ui
