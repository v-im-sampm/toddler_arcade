#include "little_arcade/ui/launcher_view.hpp"

#include "little_arcade/ui/launcher_art.hpp"
#include "little_arcade/ui/palette.hpp"

namespace little_arcade::ui {

namespace {
namespace pal = palette;

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}

lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h, uint32_t rgb, lv_opa_t opa, int radius) {
  lv_obj_t* o = plain(parent);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_bg_color(o, to_lv(rgb), 0);
  lv_obj_set_style_bg_opa(o, opa, 0);
  return o;
}

void set_visible(lv_obj_t* o, bool visible) {
  if (visible) {
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
}
}  // namespace

void LauncherView::create(lv_obj_t* parent, TileArtFn art) {
  art_ = art;
  lv_obj_set_style_bg_color(parent, to_lv(pal::kSky), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  box(parent, 0, 0, kScreenWidth, 62, pal::kSkyDark, LV_OPA_COVER, 0);
  box(parent, 0, kScreenHeight - 10, kScreenWidth, 10, pal::kGround, LV_OPA_COVER, 0);

  if (logo_.allocate(kLogoW, kLogoH)) {
    Canvas c = logo_.canvas();
    paint_logo(c);
    lv_obj_t* img = lv_image_create(parent);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(img, logo_.dsc());
    lv_obj_set_pos(img, 120, 12);
  }

  for (std::size_t slot = 0; slot < SceneManager::kTilesPerPage; ++slot) {
    const Rect& r = SceneManager::kTileRects[slot];
    Tile& t = tiles_[slot];
    t.shadow = box(parent, r.x + 5, r.y + 6, r.w - 6, r.h - 6, pal::kInk, LV_OPA_20, 14);
    t.card = box(parent, r.x + 2, r.y + 2, r.w - 6, r.h - 6, pal::kWhite, LV_OPA_COVER, 14);
    lv_obj_set_style_border_color(t.card, to_lv(pal::kInk), 0);
    lv_obj_set_style_border_width(t.card, 3, 0);
    lv_obj_set_style_clip_corner(t.card, true, 0);
    t.art = lv_image_create(t.card);
    lv_obj_remove_flag(t.art, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(t.art);
    t.shown = nullptr;
    set_visible(t.shadow, false);
    set_visible(t.card, false);
  }

  const Rect& nr = SceneManager::kNextPageButton;
  pager_ = box(parent, nr.x + 2, nr.y + 2, nr.w - 4, nr.h - 4, pal::kChrome, LV_OPA_COVER, 12);
  lv_obj_set_style_border_color(pager_, to_lv(pal::kChromeEdge), 0);
  lv_obj_set_style_border_width(pager_, 2, 0);
  lv_obj_t* arrow = lv_image_create(pager_);
  lv_obj_remove_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
  lv_image_set_src(arrow, arrow_.build([](float x, float y) { return next_arrow_icon(x, y, 32.0f); }));
  lv_obj_set_style_image_recolor(arrow, to_lv(pal::kInk), 0);
  lv_obj_set_style_image_recolor_opa(arrow, LV_OPA_COVER, 0);
  lv_obj_align(arrow, LV_ALIGN_TOP_MID, 0, 14);
  for (lv_obj_t*& d : dots_) d = box(pager_, 0, 0, 7, 7, pal::kInk, LV_OPA_COVER, LV_RADIUS_CIRCLE);
  set_visible(pager_, false);

  page_shown_ = SIZE_MAX;
  pages_shown_ = SIZE_MAX;
  initialised_ = true;
}

void LauncherView::sync(const SceneManager& scenes) {
  if (!initialised_) return;
  for (std::size_t slot = 0; slot < SceneManager::kTilesPerPage; ++slot) {
    Tile& t = tiles_[slot];
    const GameEntry* entry = scenes.tile(slot);
    if (entry == t.shown) continue;
    t.shown = entry;
    set_visible(t.shadow, entry != nullptr);
    set_visible(t.card, entry != nullptr);
    if (!entry) continue;
    const TileArt art = art_ ? art_(entry->id) : TileArt{};
    lv_obj_set_style_bg_color(t.card, to_lv(art.bg_rgb), 0);
    set_visible(t.art, art.image != nullptr);
    if (art.image) lv_image_set_src(t.art, art.image);
  }

  const std::size_t pages = scenes.page_count();
  if (pages == pages_shown_ && scenes.page() == page_shown_) return;
  pages_shown_ = pages;
  page_shown_ = scenes.page();
  set_visible(pager_, pages > 1);
  if (pages <= 1) return;
  const int inner_w = SceneManager::kNextPageButton.w - 4;
  const int first_x = inner_w / 2 - static_cast<int>(pages - 1) * 5 - 3;
  for (std::size_t i = 0; i < kMaxDots; ++i) {
    set_visible(dots_[i], i < pages);
    lv_obj_set_pos(dots_[i], first_x + static_cast<int>(i) * 10, 74);
    lv_obj_set_style_bg_opa(dots_[i], static_cast<lv_opa_t>(i == page_shown_ ? LV_OPA_COVER : LV_OPA_30), 0);
  }
}

}  // namespace little_arcade::ui
