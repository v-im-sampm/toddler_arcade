#include "little_arcade/games/bubble_aquarium_view.hpp"

#include <cmath>

#include "little_arcade/ui/palette.hpp"
#include "little_arcade/ui/scene_art.hpp"

namespace little_arcade::bubble_aquarium {
namespace {
using ui::to_lv;
namespace pal = ui::palette;

lv_obj_t* plain(lv_obj_t* parent) {
  auto* object = lv_obj_create(parent);
  lv_obj_remove_style_all(object);
  lv_obj_remove_flag(object, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return object;
}

void visible(lv_obj_t* object, bool on) {
  if (on) lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* image(lv_obj_t* parent, const ui::ArgbImage& art) {
  auto* object = lv_image_create(parent);
  lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
  if (art.ready()) lv_image_set_src(object, art.dsc());
  return object;
}

void paint(ui::ArgbImage& image, int width, int height, void (*fn)(ui::Canvas&)) {
  if (!image.allocate(width, height)) return;
  ui::Canvas canvas = image.canvas();
  fn(canvas);
}

lv_obj_t* button(lv_obj_t* parent, Rect rect, uint32_t fill, uint32_t edge) {
  auto* shadow = plain(parent);
  lv_obj_set_pos(shadow, rect.x + 5, rect.y + 6);
  lv_obj_set_size(shadow, rect.w - 6, rect.h - 6);
  lv_obj_set_style_radius(shadow, 15, 0);
  lv_obj_set_style_bg_color(shadow, to_lv(edge), 0);
  lv_obj_set_style_bg_opa(shadow, LV_OPA_COVER, 0);

  auto* face = plain(parent);
  lv_obj_set_pos(face, rect.x + 2, rect.y + 2);
  lv_obj_set_size(face, rect.w - 6, rect.h - 6);
  lv_obj_set_style_radius(face, 15, 0);
  lv_obj_set_style_bg_color(face, to_lv(fill), 0);
  lv_obj_set_style_bg_grad_color(face, to_lv(ui::tok::kCream.light), 0);
  lv_obj_set_style_bg_grad_dir(face, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(face, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(face, to_lv(edge), 0);
  lv_obj_set_style_border_width(face, 2, 0);
  lv_obj_set_style_outline_color(face, to_lv(pal::kYellow), 0);
  lv_obj_set_style_outline_width(face, 0, 0);
  return face;
}
}  // namespace

void BubbleAquariumView::create(lv_obj_t* parent) {
  lv_obj_set_style_bg_color(parent, to_lv(art::kWater), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  ui::show_backdrop(parent, ui::World::REEF);

  dim_overlay_ = plain(parent);
  lv_obj_set_pos(dim_overlay_, 0, 0);
  lv_obj_set_size(dim_overlay_, kScreenWidth, kScreenHeight);
  lv_obj_set_style_bg_color(dim_overlay_, to_lv(ui::tok::kNight), 0);
  lv_obj_set_style_bg_opa(dim_overlay_, 0, 0);

  paint(plant_art_, art::kPlantW, art::kPlantH, art::paint_plant);
  plants_[0] = image(parent, plant_art_);
  lv_obj_set_pos(plants_[0], 0, 150);
  plants_[1] = image(parent, plant_art_);
  lv_obj_set_pos(plants_[1], 264, 150);

  auto* chest_shadow = plain(parent);
  lv_obj_set_pos(chest_shadow, 198, 222);
  lv_obj_set_size(chest_shadow, 46, 15);
  lv_obj_set_style_radius(chest_shadow, 6, 0);
  lv_obj_set_style_bg_color(chest_shadow, to_lv(pal::kInk), 0);
  lv_obj_set_style_bg_opa(chest_shadow, LV_OPA_30, 0);
  chest_ = plain(parent);
  lv_obj_set_pos(chest_, 200, 214);
  lv_obj_set_size(chest_, 40, 20);
  lv_obj_set_style_radius(chest_, 4, 0);
  lv_obj_set_style_bg_color(chest_, to_lv(ui::tok::kOrange.base), 0);
  lv_obj_set_style_bg_opa(chest_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(chest_, to_lv(pal::kInk), 0);
  lv_obj_set_style_border_width(chest_, 2, 0);
  chest_lid_ = plain(parent);
  lv_obj_set_pos(chest_lid_, 198, 208);
  lv_obj_set_size(chest_lid_, 44, 9);
  lv_obj_set_style_radius(chest_lid_, 4, 0);
  lv_obj_set_style_bg_color(chest_lid_, to_lv(ui::tok::kOrange.light), 0);
  lv_obj_set_style_bg_opa(chest_lid_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(chest_lid_, to_lv(pal::kInk), 0);
  lv_obj_set_style_border_width(chest_lid_, 2, 0);

  for (std::size_t i = 0; i < kFishTypeCount; ++i) {
    if (fish_art_[i].allocate(art::kFishW, art::kFishH)) {
      ui::Canvas canvas = fish_art_[i].canvas();
      art::paint_fish(canvas, static_cast<FishType>(i));
    }
    fish_[i] = image(parent, fish_art_[i]);
  }
  paint(bubble_art_, art::kBubbleSize, art::kBubbleSize, art::paint_bubble);
  paint(food_art_, art::kFoodSize, art::kFoodSize, art::paint_food);
  for (std::size_t i = 0; i < kVisitorCount; ++i) {
    if (visitor_art_[i].allocate(art::kVisitorW, art::kVisitorH)) {
      ui::Canvas canvas = visitor_art_[i].canvas();
      art::paint_visitor(canvas, static_cast<Visitor>(i));
    }
  }
  for (auto& object : bubbles_) object = image(parent, bubble_art_);
  for (auto& object : food_) object = image(parent, food_art_);
  visitor_ = image(parent, visitor_art_[0]);
  visible(visitor_, false);

  const Rect controls[] = {BubbleAquariumGame::kFoodButton, BubbleAquariumGame::kBedtimeButton,
                           BubbleAquariumGame::kBubbleButton};
  lv_obj_t** buttons[] = {&food_button_, &bedtime_button_, &bubble_button_};
  const uint32_t fills[] = {ui::tok::kOrange.base, ui::tok::kPurple.light, ui::tok::kTeal.base};
  const uint32_t edges[] = {ui::tok::kOrange.shadow, ui::tok::kPurple.shadow, ui::tok::kTeal.shadow};
  for (int i = 0; i < 3; ++i) *buttons[i] = button(parent, controls[i], fills[i], edges[i]);

  auto* food_mark = image(food_button_, food_art_);
  lv_image_set_scale(food_mark, 320);
  lv_obj_center(food_mark);
  paint(moon_art_, art::kMoonSize, art::kMoonSize, art::paint_moon);
  paint(sun_art_, art::kMoonSize, art::kMoonSize, art::paint_sun);
  moon_icon_ = image(bedtime_button_, moon_art_);
  sun_icon_ = image(bedtime_button_, sun_art_);
  lv_obj_center(moon_icon_);
  lv_obj_center(sun_icon_);
  visible(sun_icon_, false);

  auto* bubble_mark = image(bubble_button_, bubble_art_);
  lv_image_set_scale(bubble_mark, 170);
  lv_obj_center(bubble_mark);

  hint_hand_ = plain(parent);
  lv_obj_set_size(hint_hand_, 8, 15);
  lv_obj_set_style_radius(hint_hand_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(hint_hand_, to_lv(ui::tok::kWhite), 0);
  lv_obj_set_style_bg_opa(hint_hand_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(hint_hand_, to_lv(pal::kInk), 0);
  lv_obj_set_style_border_width(hint_hand_, 1, 0);
  visible(hint_hand_, false);
}

void BubbleAquariumView::sync(const BubbleAquariumGame& game, uint32_t now_ms) {
  for (std::size_t i = 0; i < kFishTypeCount; ++i) {
    const Fish& fish = game.fish(i);
    const int16_t center_x = static_cast<int16_t>(fish.rect.x + fish.rect.w / 2);
    const int16_t center_y = static_cast<int16_t>(fish.rect.y + fish.rect.h / 2);
    lv_obj_set_pos(fish_[i], center_x - art::kFishW / 2, center_y - art::kFishH / 2);
    const bool highlight = fish.happy_ms || fish.munch_ms;
    lv_obj_set_style_outline_color(fish_[i], to_lv(pal::kYellow), 0);
    lv_obj_set_style_outline_width(fish_[i], highlight ? 2 : 0, 0);
    const bool sleeping = fish.mode == FishMode::SLEEP;
    const int32_t bob = sleeping
                            ? static_cast<int32_t>(std::sin(static_cast<float>(now_ms % 2400u) / 380.0f) * 2.0f)
                            : static_cast<int32_t>(std::sin(static_cast<float>(now_ms % 700u) / 110.0f) *
                                                   (fish.happy_ms ? 2.5f : 0.8f));
    lv_obj_set_style_translate_y(fish_[i], bob, 0);
    lv_obj_set_style_opa(fish_[i], sleeping ? LV_OPA_80 : LV_OPA_COVER, 0);
  }

  for (std::size_t i = 0; i < BubbleAquariumGame::kMaxBubbles; ++i) {
    const Bubble& bubble = game.bubble(i);
    visible(bubbles_[i], bubble.active);
    if (bubble.active) {
      lv_obj_set_pos(bubbles_[i], bubble.x - art::kBubbleSize / 2, bubble.y - art::kBubbleSize / 2);
      lv_image_set_scale(bubbles_[i], static_cast<uint16_t>(bubble.size * 512 / art::kBubbleSize));
    }
  }
  for (std::size_t i = 0; i < BubbleAquariumGame::kMaxFood; ++i) {
    const Food& pellet = game.food(i);
    visible(food_[i], pellet.active);
    if (pellet.active) lv_obj_set_pos(food_[i], pellet.x - art::kFoodSize / 2, pellet.y - art::kFoodSize / 2);
  }

  visible(visitor_, game.visitor_visible());
  if (game.visitor_visible()) {
    const auto visitor_index = static_cast<std::size_t>(game.visitor());
    if (visitor_art_[visitor_index].ready()) lv_image_set_src(visitor_, visitor_art_[visitor_index].dsc());
    const Rect rect = game.visitor_rect();
    lv_obj_set_pos(visitor_, rect.x, rect.y);
  }

  lv_obj_set_style_bg_opa(dim_overlay_, game.bedtime_dim(), 0);
  visible(moon_icon_, !game.bedtime());
  visible(sun_icon_, game.bedtime());
  lv_obj_set_style_outline_width(food_button_, game.food_button_pulsing() ? 3 : (game.hint_level() ? 4 : 0), 0);
  lv_obj_set_style_outline_width(bedtime_button_, game.bedtime_button_pulsing() ? 3 : 0, 0);
  lv_obj_set_style_outline_width(bubble_button_, game.bubble_button_pulsing() ? 3 : 0, 0);
  lv_obj_set_pos(chest_lid_, 198, game.chest_open() ? 204 : 208);

  const bool show_hint = game.hint_level() >= 2 && !game.bedtime();
  visible(hint_hand_, show_hint);
  if (show_hint) {
    lv_obj_set_pos(hint_hand_, 106, 77 + static_cast<int32_t>(std::sin(static_cast<float>(now_ms % 600u) / 100.0f) * 3.0f));
  }
  lv_obj_set_style_translate_x(plants_[0], static_cast<int32_t>(std::sin(static_cast<float>(now_ms % 2600u) / 420.0f) * 2), 0);
  lv_obj_set_style_translate_x(plants_[1], static_cast<int32_t>(std::sin(static_cast<float>(now_ms % 2600u) / 420.0f) * -2), 0);
}

}  // namespace little_arcade::bubble_aquarium
