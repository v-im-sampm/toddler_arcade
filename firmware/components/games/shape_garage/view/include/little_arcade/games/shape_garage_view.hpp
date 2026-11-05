#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/games/shape_garage.hpp"
#include "little_arcade/ui/argb_image.hpp"
#include "lvgl.h"

namespace little_arcade::shape_garage {

class ShapeGarageView {
 public:
  void create(lv_obj_t* parent);
  void sync(const ShapeGarageGame& game, uint32_t now_ms);
  void reset() { initialised_ = false; }

 private:
  static constexpr std::size_t kPieceCount = ShapeGarageGame::kPieceCount;
  static constexpr int kSparkles = 5;

  ui::ArgbImage robot_img_;
  ui::ArgbImage slot_imgs_[kShapeCount];
  ui::ArgbImage part_imgs_[kShapeCount];
  ui::ArgbImage head_img_;

  lv_obj_t* robot_ = nullptr;
  lv_obj_t* slot_objects_[kPieceCount]{};
  lv_obj_t* tray_cards_[kPieceCount]{};
  lv_obj_t* piece_objects_[kPieceCount]{};
  lv_obj_t* head_card_ = nullptr;
  lv_obj_t* head_object_ = nullptr;
  lv_obj_t* lifted_object_ = nullptr;
  lv_obj_t* sparkle_objects_[kSparkles]{};

  const lv_image_dsc_t* slot_sources_[kPieceCount]{};
  const lv_image_dsc_t* piece_sources_[kPieceCount]{};
  const lv_image_dsc_t* lifted_source_ = nullptr;
  Shape shown_slots_[kPieceCount]{};
  bool slot_known_[kPieceCount]{};
  Shape shown_pieces_[kPieceCount]{};
  bool piece_known_[kPieceCount]{};
  bool slot_highlighted_[kPieceCount]{};
  bool tray_selected_[kPieceCount]{};
  bool head_highlighted_ = false;
  int32_t robot_y_ = INT32_MIN;
  int32_t slot_y_[kPieceCount] = {INT32_MIN, INT32_MIN, INT32_MIN};
  int32_t piece_x_[kPieceCount] = {INT32_MIN, INT32_MIN, INT32_MIN};
  int32_t piece_y_[kPieceCount] = {INT32_MIN, INT32_MIN, INT32_MIN};
  int32_t sparkle_x_[kSparkles] = {INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN};
  int32_t sparkle_y_[kSparkles] = {INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN};
  int last_wiggling_ = -1;
  uint32_t wiggle_start_ms_ = 0;
  int last_snapping_ = -1;
  uint32_t snap_start_ms_ = 0;
  uint8_t shown_style_ = 0xFF;
  Phase last_phase_ = Phase::INTRO;
  uint32_t phase_start_ms_ = 0;
  bool initialised_ = false;
};

}  // namespace little_arcade::shape_garage
