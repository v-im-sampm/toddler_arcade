#include "little_arcade/games/rocket_rescue_view.hpp"

#include "little_arcade/ui/palette.hpp"
#include "little_arcade/ui/scene_art.hpp"

namespace little_arcade::rocket_rescue {
namespace {
using ui::to_lv;
namespace pal = ui::palette;
lv_obj_t* plain(lv_obj_t* parent) {
  auto* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
void visible(lv_obj_t* o, bool on) {
  if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
void paint(ui::ArgbImage& image, int w, int h, void (*fn)(ui::Canvas&)) {
  if (!image.allocate(w, h)) return;
  ui::Canvas c = image.canvas();
  fn(c);
}
}  // namespace

void RocketRescueView::create(lv_obj_t* parent) {
  lv_obj_set_style_bg_color(parent, to_lv(art::kSpace), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  ui::show_backdrop(parent, ui::World::SPACE);
  for (std::size_t i = 0; i < kPlanetCount; ++i) {
    planets_[i].allocate(art::kPlanetW, art::kPlanetH);
    planet_[i] = lv_image_create(parent);
    lv_obj_remove_flag(planet_[i], LV_OBJ_FLAG_CLICKABLE);
  }
  paint(rocket_art_, art::kRocketSize, art::kRocketSize, art::paint_rocket);
  paint(star_art_, art::kStarSize, art::kStarSize, art::paint_star);
  rocket_ = lv_image_create(parent);
  lv_obj_remove_flag(rocket_, LV_OBJ_FLAG_CLICKABLE);
  if (rocket_art_.ready()) lv_image_set_src(rocket_, rocket_art_.dsc());
  for (auto& obj : stars_) {
    obj = lv_image_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    if (star_art_.ready()) lv_image_set_src(obj, star_art_.dsc());
  }
  prompt_ = plain(parent);
  lv_obj_set_pos(prompt_, 132, 4);
  lv_obj_set_size(prompt_, 56, 56);
  lv_obj_set_style_radius(prompt_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(prompt_, to_lv(pal::kWhite), 0);
  lv_obj_set_style_bg_opa(prompt_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(prompt_, to_lv(pal::kYellow), 0);
  lv_obj_set_style_border_width(prompt_, 3, 0);
  prompt_icon_ = lv_image_create(prompt_);
  lv_obj_remove_flag(prompt_icon_, LV_OBJ_FLAG_CLICKABLE);
  lv_image_set_scale(prompt_icon_, 80);
  prompt_question_ = lv_label_create(prompt_);
  lv_label_set_text_static(prompt_question_, "?");
  lv_obj_center(prompt_question_);
  const Rect rs[] = {RocketRescueGame::kGoButton, RocketRescueGame::kStopButton};
  lv_obj_t** buttons[] = {&go_, &stop_};
  const char* labels[] = {"GO", "STOP"};
  const uint32_t colors[] = {ui::tok::kGreen.base, ui::tok::kPink.base};
  const uint32_t edges[] = {ui::tok::kGreen.shadow, ui::tok::kPink.shadow};
  for (int i = 0; i < 2; ++i) {
    auto* lip = plain(parent);
    lv_obj_set_pos(lip, rs[i].x + 5, rs[i].y + 7);
    lv_obj_set_size(lip, rs[i].w - 6, rs[i].h - 6);
    lv_obj_set_style_radius(lip, 14, 0);
    lv_obj_set_style_bg_color(lip, to_lv(edges[i]), 0);
    lv_obj_set_style_bg_opa(lip, LV_OPA_COVER, 0);
    *buttons[i] = plain(parent);
    lv_obj_set_pos(*buttons[i], rs[i].x + 3, rs[i].y + 3);
    lv_obj_set_size(*buttons[i], rs[i].w - 6, rs[i].h - 6);
    lv_obj_set_style_radius(*buttons[i], 14, 0);
    lv_obj_set_style_bg_color(*buttons[i], to_lv(colors[i]), 0);
    lv_obj_set_style_bg_grad_color(*buttons[i], to_lv(i == 0 ? ui::tok::kGreen.light : ui::tok::kPink.light), 0);
    lv_obj_set_style_bg_grad_dir(*buttons[i], LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(*buttons[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(*buttons[i], to_lv(edges[i]), 0);
    lv_obj_set_style_border_width(*buttons[i], 3, 0);
    auto* label_obj = lv_label_create(*buttons[i]);
    lv_label_set_text_static(label_obj, labels[i]);
#if LV_FONT_MONTSERRAT_18
    lv_obj_set_style_text_font(label_obj, &lv_font_montserrat_18, 0);
#endif
    lv_obj_set_style_text_color(label_obj, to_lv(ui::tok::kInk), 0);
    lv_obj_center(label_obj);
  }
}

void RocketRescueView::sync(const RocketRescueGame& game, uint32_t now_ms) {
  (void)now_ms;
  const int suggested = game.suggested();
  for (std::size_t i = 0; i < kPlanetCount; ++i) {
    const Planet& p = game.planet(i);
    const int key = static_cast<int>(p.look) * static_cast<int>(kPlanetCount) + static_cast<int>(p.buddy);
    if (shown_looks_[i] != key && planets_[i].ready()) {
      shown_looks_[i] = key;
      ui::Canvas c = planets_[i].canvas();
      c.clear();
      art::paint_planet(c, p.look, p.buddy);
      if (planets_[i].ready()) {
        lv_image_set_src(planet_[i], planets_[i].dsc());
        lv_obj_invalidate(planet_[i]);
      }
    }
    lv_obj_set_pos(planet_[i], p.rect.x - 20, p.rect.y - 4);
    const bool target = game.phase() != Phase::INTRO && suggested == static_cast<int>(i);
    const bool celebrating = game.waving() == static_cast<int>(i) || (p.rescued && game.phase() == Phase::CELEBRATE);
    lv_obj_set_style_outline_color(planet_[i], to_lv(celebrating ? pal::kGreen : pal::kYellow), 0);
    lv_obj_set_style_outline_width(planet_[i], target || celebrating ? 3 : 0, 0);
    lv_obj_set_style_outline_pad(planet_[i], target || celebrating ? 2 : 0, 0);
  }
  lv_obj_set_style_border_width(prompt_, game.hint_active() ? 4 : 2, 0);
  lv_obj_set_style_border_color(prompt_, to_lv(game.missions_completed() ? pal::kGreen : pal::kYellow), 0);
  const bool has_prompt = suggested >= 0 && game.phase() != Phase::INTRO;
  visible(prompt_icon_, has_prompt);
  visible(prompt_question_, !has_prompt);
  if (has_prompt && planets_[suggested].ready()) {
    lv_image_set_src(prompt_icon_, planets_[suggested].dsc());
    lv_obj_center(prompt_icon_);
  }
  for (std::size_t i = 0; i < RocketRescueGame::kStarCount; ++i) {
    const Star& star = game.star(i);
    visible(stars_[i], star.visible);
    if (star.visible) {
      const Rect r = game.star_rect(i);
      lv_obj_set_pos(stars_[i], r.x + (r.w - art::kStarSize) / 2, r.y + (r.h - art::kStarSize) / 2);
    }
  }
  const Point p = game.rocket_position();
  lv_obj_set_pos(rocket_, p.x - art::kRocketSize / 2, p.y - art::kRocketSize / 2);
  initialized_ = true;
}

}  // namespace little_arcade::rocket_rescue
