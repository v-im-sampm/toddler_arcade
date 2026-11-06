#include "little_arcade/games/bubble_aquarium_view.hpp"

#include <cmath>

#include "little_arcade/ui/palette.hpp"

namespace little_arcade::bubble_aquarium {
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
lv_obj_t* image(lv_obj_t* parent, const ui::ArgbImage& art) {
  auto* o = lv_image_create(parent);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  if (art.ready()) lv_image_set_src(o, art.dsc());
  return o;
}
void paint(ui::ArgbImage& image, int w, int h, void (*fn)(ui::Canvas&)) {
  if (!image.allocate(w, h)) return;
  ui::Canvas c = image.canvas();
  fn(c);
}
}  // namespace

void BubbleAquariumView::create(lv_obj_t* parent) {
  lv_obj_set_style_bg_color(parent, to_lv(art::kWater), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  auto* horizon = plain(parent);
  lv_obj_set_pos(horizon, 0, 62);
  lv_obj_set_size(horizon, kScreenWidth, 3);
  lv_obj_set_style_bg_color(horizon, to_lv(art::kDeepWater), 0);
  lv_obj_set_style_bg_opa(horizon, LV_OPA_50, 0);
  auto* sand = plain(parent);
  lv_obj_set_pos(sand, 0, 222);
  lv_obj_set_size(sand, kScreenWidth, 18);
  lv_obj_set_style_bg_color(sand, to_lv(pal::kGround), 0);
  lv_obj_set_style_bg_opa(sand, LV_OPA_COVER, 0);
  for (std::size_t i = 0; i < kFishTypeCount; ++i) {
    if (fish_art_[i].allocate(art::kFishW, art::kFishH)) {
      ui::Canvas c = fish_art_[i].canvas();
      art::paint_fish(c, static_cast<FishType>(i));
    }
    fish_[i] = image(parent, fish_art_[i]);
  }
  paint(bubble_art_, art::kBubbleSize, art::kBubbleSize, art::paint_bubble);
  paint(food_art_, art::kFoodSize, art::kFoodSize, art::paint_food);
  for (std::size_t i = 0; i < kVisitorCount; ++i) {
    if (visitor_art_[i].allocate(art::kVisitorW, art::kVisitorH)) {
      ui::Canvas c = visitor_art_[i].canvas();
      art::paint_visitor(c, static_cast<Visitor>(i));
    }
  }
  for (auto& obj : bubbles_) obj = image(parent, bubble_art_);
  for (auto& obj : food_) obj = image(parent, food_art_);
  visitor_ = image(parent, visitor_art_[0]);
  visible(visitor_, false);

  const Rect controls[] = {BubbleAquariumGame::kFoodButton, BubbleAquariumGame::kBubbleButton};
  lv_obj_t** buttons[] = {&food_button_, &bubble_button_};
  for (int i = 0; i < 2; ++i) {
    *buttons[i] = plain(parent);
    lv_obj_set_pos(*buttons[i], controls[i].x + 3, controls[i].y + 3);
    lv_obj_set_size(*buttons[i], controls[i].w - 6, controls[i].h - 6);
    lv_obj_set_style_radius(*buttons[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(*buttons[i], to_lv(pal::kWhite), 0);
    lv_obj_set_style_bg_opa(*buttons[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(*buttons[i], to_lv(pal::kChromeEdge), 0);
    lv_obj_set_style_border_width(*buttons[i], 2, 0);
    lv_obj_set_style_outline_color(*buttons[i], to_lv(pal::kYellow), 0);
    lv_obj_set_style_outline_width(*buttons[i], 0, 0);
  }
  auto* label = lv_label_create(food_button_);
  lv_label_set_text_static(label, LV_SYMBOL_DOWN);
  lv_obj_center(label);
  auto* bubble_mark = plain(bubble_button_);
  lv_obj_set_size(bubble_mark, 20, 20);
  lv_obj_center(bubble_mark);
  lv_obj_set_style_radius(bubble_mark, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(bubble_mark, to_lv(art::kWater), 0);
  lv_obj_set_style_bg_opa(bubble_mark, LV_OPA_30, 0);
  lv_obj_set_style_border_color(bubble_mark, to_lv(pal::kBlue), 0);
  lv_obj_set_style_border_width(bubble_mark, 2, 0);

  chest_ = plain(parent);
  lv_obj_set_pos(chest_, 142, 213);
  lv_obj_set_size(chest_, 36, 20);
  lv_obj_set_style_radius(chest_, 6, 0);
  lv_obj_set_style_bg_color(chest_, to_lv(pal::kOrange), 0);
  lv_obj_set_style_bg_opa(chest_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(chest_, to_lv(pal::kInk), 0);
  lv_obj_set_style_border_width(chest_, 2, 0);
  initialized_ = false;
}

void BubbleAquariumView::sync(const BubbleAquariumGame& game, uint32_t now_ms) {
  initialized_ = true;
  shown_phase_ = static_cast<int>(game.phase());
  for (std::size_t i = 0; i < kFishTypeCount; ++i) {
    const Fish& f = game.fish(i);
    lv_obj_set_pos(fish_[i], f.rect.x - 8, f.rect.y - 8);
    const bool highlight = f.happy_ms || f.munch_ms;
    lv_obj_set_style_outline_color(fish_[i], to_lv(pal::kYellow), 0);
    lv_obj_set_style_outline_width(fish_[i], highlight ? 2 : 0, 0);
    const float elapsed = static_cast<float>(now_ms % 565u);
    lv_obj_set_style_translate_y(fish_[i], f.happy_ms ? static_cast<int32_t>(std::sin(elapsed / 90.0f) * 2.0f) : 0, 0);
  }
  for (std::size_t i = 0; i < BubbleAquariumGame::kMaxBubbles; ++i) {
    const Bubble& b = game.bubble(i);
    visible(bubbles_[i], b.active);
    if (b.active) lv_obj_set_pos(bubbles_[i], b.x - art::kBubbleSize / 2, b.y - art::kBubbleSize / 2);
  }
  for (std::size_t i = 0; i < BubbleAquariumGame::kMaxFood; ++i) {
    const Food& f = game.food(i);
    visible(food_[i], f.active);
    if (f.active) lv_obj_set_pos(food_[i], f.x - art::kFoodSize / 2, f.y - art::kFoodSize / 2);
  }
  const bool visitor = game.visitor_visible();
  visible(visitor_, visitor);
  if (visitor) {
    if (visitor_art_[static_cast<std::size_t>(game.visitor())].ready())
      lv_image_set_src(visitor_, visitor_art_[static_cast<std::size_t>(game.visitor())].dsc());
    const Rect r = game.visitor_rect();
    lv_obj_set_pos(visitor_, r.x, r.y);
  }
  if (game.food_button_pulsing()) lv_obj_set_style_outline_width(food_button_, 3, 0);
  else lv_obj_set_style_outline_width(food_button_, 0, 0);
  if (game.bubble_button_pulsing()) lv_obj_set_style_outline_width(bubble_button_, 3, 0);
  else lv_obj_set_style_outline_width(bubble_button_, 0, 0);
  (void)now_ms;  // Fish and bubble movement is deterministic in the scene update.
}

}  // namespace little_arcade::bubble_aquarium
