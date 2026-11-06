#include "little_arcade/games/shape_garage_view.hpp"

#include <cmath>

#include "little_arcade/games/shape_garage_art.hpp"
#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/palette.hpp"
#include "little_arcade/ui/scene_art.hpp"

namespace little_arcade::shape_garage {

namespace {
using ui::to_lv;
namespace pal = ui::palette;
using G = ShapeGarageGame;

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* object = lv_obj_create(parent);
  lv_obj_remove_style_all(object);
  lv_obj_remove_flag(object, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return object;
}

lv_obj_t* image(lv_obj_t* parent) {
  lv_obj_t* object = lv_image_create(parent);
  lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
  return object;
}

lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h, int radius, uint32_t fill, uint32_t edge,
              int edge_width) {
  lv_obj_t* object = plain(parent);
  lv_obj_set_pos(object, x, y);
  lv_obj_set_size(object, w, h);
  lv_obj_set_style_radius(object, radius, 0);
  lv_obj_set_style_bg_color(object, to_lv(fill), 0);
  lv_obj_set_style_bg_grad_color(object, to_lv(ui::mix(fill, ui::tok::kCream.light, 0.2f)), 0);
  lv_obj_set_style_bg_grad_dir(object, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(object, to_lv(edge), 0);
  lv_obj_set_style_border_width(object, edge_width, 0);
  return object;
}

void visible(lv_obj_t* object, bool show) {
  const bool shown = !lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN);
  if (shown == show) return;
  if (show) {
    lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
  }
}

void outline(lv_obj_t* object, bool show, uint32_t color, bool& previous, int width = 3) {
  if (previous == show) return;
  previous = show;
  lv_obj_set_style_outline_color(object, to_lv(color), 0);
  lv_obj_set_style_outline_width(object, show ? width : 0, 0);
  lv_obj_set_style_outline_pad(object, show ? 2 : 0, 0);
}

int32_t round_i(float value) { return static_cast<int32_t>(std::lround(value)); }

}  // namespace

void ShapeGarageView::create(lv_obj_t* parent) {
  namespace art = shape_garage::art;

  lv_obj_set_style_bg_color(parent, to_lv(pal::kSky), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  ui::show_backdrop(parent, ui::World::WORKSHOP);

  if (robot_img_.allocate(art::kRobotImgW, art::kRobotImgH)) {
    ui::Canvas canvas = robot_img_.canvas();
    art::paint_robot(canvas, 0);
    robot_ = image(parent);
    lv_image_set_src(robot_, robot_img_.dsc());
    lv_obj_set_pos(robot_, 32, 38);
  }

  for (std::size_t i = 0; i < kPieceCount; ++i) {
    if (slot_imgs_[i].allocate(art::kShapeImgW, art::kShapeImgH)) {
      ui::Canvas canvas = slot_imgs_[i].canvas();
      art::paint_shape_icon(canvas, static_cast<Shape>(i), true);
    }
    if (part_imgs_[i].allocate(art::kShapeImgW, art::kShapeImgH)) {
      ui::Canvas canvas = part_imgs_[i].canvas();
      art::paint_shape_icon(canvas, static_cast<Shape>(i), false);
    }

    const Rect& slot = G::kSlotRects[i];
    slot_objects_[i] = image(parent);
    lv_obj_set_pos(slot_objects_[i], slot.x, slot.y);
    lv_obj_set_size(slot_objects_[i], slot.w, slot.h);
    slot_y_[i] = slot.y;

    const Rect& tray = G::kTrayRects[i];
    tray_cards_[i] = box(parent, tray.x + 3, tray.y + 3, tray.w - 6, tray.h - 6, 14,
                          pal::kWhite, pal::kChromeEdge, 2);
    piece_objects_[i] = image(parent);
    lv_obj_set_size(piece_objects_[i], tray.w, tray.h);
    lv_obj_set_pos(piece_objects_[i], tray.x, tray.y);
    piece_x_[i] = tray.x;
    piece_y_[i] = tray.y;
  }

  head_card_ = box(parent, G::kRobotHead.x + 3, G::kRobotHead.y + 3, G::kRobotHead.w - 6,
                   G::kRobotHead.h - 6, LV_RADIUS_CIRCLE, pal::kWhite, pal::kChromeEdge, 2);
  if (head_img_.allocate(art::kHeadImgW, art::kHeadImgH)) {
    ui::Canvas canvas = head_img_.canvas();
    art::paint_head_icon(canvas);
    head_object_ = image(parent);
    lv_image_set_src(head_object_, head_img_.dsc());
    lv_obj_set_pos(head_object_, G::kRobotHead.x + 4, G::kRobotHead.y + 4);
    lv_obj_set_size(head_object_, G::kRobotHead.w - 8, G::kRobotHead.h - 8);
  }
  lifted_object_ = image(parent);
  lv_obj_set_size(lifted_object_, 44, 44);
  lv_obj_set_pos(lifted_object_, G::kRobotHead.x + 6, G::kRobotHead.y + 6);
  visible(lifted_object_, false);

  static constexpr uint32_t kSparkleColors[kSparkles] = {
      pal::kYellow, pal::kWhite, pal::kPink, pal::kBlue, pal::kYellow};
  for (int i = 0; i < kSparkles; ++i) {
    sparkle_objects_[i] = plain(parent);
    lv_obj_set_size(sparkle_objects_[i], 7, 7);
    lv_obj_set_style_radius(sparkle_objects_[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sparkle_objects_[i], to_lv(kSparkleColors[i]), 0);
    lv_obj_set_style_bg_opa(sparkle_objects_[i], LV_OPA_COVER, 0);
    visible(sparkle_objects_[i], false);
  }

  for (lv_obj_t* object : slot_objects_) visible(object, false);
  for (lv_obj_t* object : tray_cards_) visible(object, false);
  for (lv_obj_t* object : piece_objects_) visible(object, false);
  visible(head_card_, false);
  if (head_object_) visible(head_object_, false);
  initialised_ = false;
}

void ShapeGarageView::sync(const ShapeGarageGame& game, uint32_t now_ms) {
  if (!initialised_ || game.phase() != last_phase_) {
    last_phase_ = game.phase();
    phase_start_ms_ = now_ms;
  }
  initialised_ = true;

  if (game.robot_style() != shown_style_) {
    shown_style_ = game.robot_style();
    if (robot_img_.ready()) {
      ui::Canvas canvas = robot_img_.canvas();
      art::paint_robot(canvas, shown_style_);
      if (robot_) {
        lv_image_set_src(robot_, robot_img_.dsc());
        lv_obj_invalidate(robot_);
      }
    }
  }

  const int wiggling = game.wiggling();
  if (wiggling != last_wiggling_) {
    last_wiggling_ = wiggling;
    wiggle_start_ms_ = now_ms;
  }

  const int snapping = game.snapping();
  if (snapping != last_snapping_) {
    last_snapping_ = snapping;
    snap_start_ms_ = now_ms;
  }
  const bool snap_active = snapping >= 0 && now_ms - snap_start_ms_ < G::kSnapMs;
  std::size_t snapping_piece = kPieceCount;
  if (snap_active) {
    const Shape snapping_shape = game.slot(static_cast<std::size_t>(snapping)).shape;
    for (std::size_t i = 0; i < kPieceCount; ++i) {
      if (game.piece(i).shape == snapping_shape) snapping_piece = i;
    }
  }
  const bool dancing = game.phase() == Phase::CELEBRATE;
  const float bob = dancing ? std::sin(static_cast<float>(now_ms - phase_start_ms_) / 150.0f) * 2.0f : 0.0f;
  const int32_t bob_y = round_i(bob);
  if (robot_ && robot_y_ != 38 + bob_y) {
    robot_y_ = 38 + bob_y;
    lv_obj_set_pos(robot_, 32, robot_y_);
  }

  for (std::size_t i = 0; i < kPieceCount; ++i) {
    const Slot& slot = game.slot(i);
    const Piece& piece = game.piece(i);
    const Rect& slot_rect = slot.rect;
    const Rect& tray_rect = piece.rect;
    if (!slot_known_[i] || shown_slots_[i] != slot.shape) {
      shown_slots_[i] = slot.shape;
      slot_known_[i] = true;
    }
    if (!piece_known_[i] || shown_pieces_[i] != piece.shape) {
      shown_pieces_[i] = piece.shape;
      piece_known_[i] = true;
      const auto shape_index = static_cast<std::size_t>(piece.shape);
      const lv_image_dsc_t* source = part_imgs_[shape_index].ready() ? part_imgs_[shape_index].dsc() : nullptr;
      if (piece_sources_[i] != source) {
        piece_sources_[i] = source;
        if (source) lv_image_set_src(piece_objects_[i], source);
      }
    }

    const bool slot_is_snapping = snap_active && snapping == static_cast<int>(i);
    const bool occupied = slot.filled && !slot_is_snapping;
    const auto shape_index = static_cast<std::size_t>(slot.shape);
    const lv_image_dsc_t* slot_source = nullptr;
    if (occupied && part_imgs_[shape_index].ready()) {
      slot_source = part_imgs_[shape_index].dsc();
    } else if (slot_imgs_[shape_index].ready()) {
      slot_source = slot_imgs_[shape_index].dsc();
    }
    if (slot_sources_[i] != slot_source) {
      slot_sources_[i] = slot_source;
      if (slot_source) lv_image_set_src(slot_objects_[i], slot_source);
    }
    const int32_t slot_y = slot_rect.y + bob_y;
    if (slot_y_[i] != slot_y) {
      slot_y_[i] = slot_y;
      lv_obj_set_pos(slot_objects_[i], slot_rect.x, slot_y);
    }
    visible(slot_objects_[i], true);
    outline(slot_objects_[i], game.hint_slot() == static_cast<int>(i), pal::kYellow, slot_highlighted_[i]);

    const bool in_tray = !piece.placed;
    const bool selected = game.selected() == static_cast<int>(i);
    int32_t piece_y = tray_rect.y - (selected ? 12 : 0);
    if (game.wiggling() == static_cast<int>(i)) {
      const float elapsed = static_cast<float>(now_ms - wiggle_start_ms_);
      piece_y += round_i(std::sin(elapsed / 52.0f) * (1.0f - std::fmin(1.0f, elapsed / G::kWiggleMs)) * 3.0f);
    }
    if (i != snapping_piece) {
      const int32_t piece_x = tray_rect.x;
      if (piece_x_[i] != piece_x || piece_y_[i] != piece_y) {
        piece_x_[i] = piece_x;
        piece_y_[i] = piece_y;
        lv_obj_set_pos(piece_objects_[i], piece_x, piece_y);
      }
    }
    visible(piece_objects_[i], in_tray && i != snapping_piece);
    const bool highlighted = game.hint_piece() == static_cast<int>(i);
    outline(tray_cards_[i], selected || highlighted, pal::kYellow, tray_selected_[i]);
    visible(tray_cards_[i], true);
  }

  visible(head_card_, true);
  const int selected = game.selected();
  if (head_object_) visible(head_object_, selected < 0);
  if (selected >= 0) {
    const auto shape_index = static_cast<std::size_t>(game.piece(static_cast<std::size_t>(selected)).shape);
    const lv_image_dsc_t* source = part_imgs_[shape_index].ready() ? part_imgs_[shape_index].dsc() : nullptr;
    if (lifted_source_ != source) {
      lifted_source_ = source;
      if (source) lv_image_set_src(lifted_object_, source);
    }
    visible(lifted_object_, true);
  } else {
    visible(lifted_object_, false);
  }
  outline(head_card_, game.hint_piece() >= 0 || game.hint_slot() >= 0, pal::kYellow, head_highlighted_);

  if (snap_active) {
    const std::size_t slot_index = static_cast<std::size_t>(snapping);
    const Shape shape = game.slot(slot_index).shape;
    const std::size_t piece_index = snapping_piece;
    if (piece_index < kPieceCount) {
      const Piece& piece = game.piece(piece_index);
      const Rect& from = piece.rect;
      const Rect& to = game.slot(slot_index).rect;
      const float t = std::fmin(1.0f, static_cast<float>(now_ms - snap_start_ms_) / G::kSnapMs);
      const int32_t x = round_i(from.x + (to.x - from.x) * t);
      const int32_t y = round_i(from.y + (to.y - from.y) * t + std::sin(t * 3.14159265f) * -10.0f);
      const auto shape_index = static_cast<std::size_t>(shape);
      const lv_image_dsc_t* source = part_imgs_[shape_index].ready() ? part_imgs_[shape_index].dsc() : nullptr;
      if (piece_sources_[piece_index] != source) {
        piece_sources_[piece_index] = source;
        if (source) lv_image_set_src(piece_objects_[piece_index], source);
      }
      if (piece_x_[piece_index] != x || piece_y_[piece_index] != y) {
        piece_x_[piece_index] = x;
        piece_y_[piece_index] = y;
        lv_obj_set_pos(piece_objects_[piece_index], x, y);
      }
      visible(piece_objects_[piece_index], true);
    }
  }

  for (int i = 0; i < kSparkles; ++i) {
    if (!dancing) {
      visible(sparkle_objects_[i], false);
      continue;
    }
    const float t = static_cast<float>(now_ms - phase_start_ms_) / 240.0f + static_cast<float>(i) * 1.25f;
    const int32_t x = 56 + i * 48 + round_i(std::sin(t) * 3.0f);
    const int32_t y = 55 + round_i(std::sin(t * 1.3f) * 5.0f);
    if (sparkle_x_[i] != x || sparkle_y_[i] != y) {
      sparkle_x_[i] = x;
      sparkle_y_[i] = y;
      lv_obj_set_pos(sparkle_objects_[i], x, y);
    }
    visible(sparkle_objects_[i], true);
  }
}

}  // namespace little_arcade::shape_garage
