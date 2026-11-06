#include "little_arcade/games/robo_buddy_view.hpp"

#include <cmath>

#include "little_arcade/ui/chrome_view.hpp"
#include "little_arcade/ui/palette.hpp"
#include "little_arcade/ui/scene_art.hpp"

namespace little_arcade::robo_buddy {

namespace {
using ui::to_lv;
namespace pal = ui::palette;

constexpr uint32_t kRobot = 0x43C6BE;
constexpr uint32_t kRobotShade = 0x279A99;
constexpr uint32_t kRobotEdge = 0x246C78;
constexpr uint32_t kFace = 0xE9FCFF;
constexpr uint32_t kAntenna = 0xF5C84C;
constexpr uint32_t kSnackColors[] = {0xF2A65A, 0x67C7E8, 0xE987C3};

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* object = lv_obj_create(parent);
  lv_obj_remove_style_all(object);
  lv_obj_remove_flag(object, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return object;
}

void line_style(lv_obj_t* line, uint32_t color, int width) {
  lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_line_color(line, to_lv(color), 0);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
}

void pulse_outline(lv_obj_t* object, bool active, uint32_t now_ms) {
  const int width = active ? 2 + static_cast<int>((std::sin(static_cast<float>(now_ms) / 180.0f) + 1.0f) * 1.5f) : 0;
  lv_obj_set_style_outline_width(object, width, 0);
  lv_obj_set_style_outline_pad(object, width ? 2 : 0, 0);
  lv_obj_set_style_outline_color(object, to_lv(pal::kYellow), 0);
}
}  // namespace

lv_obj_t* RoboBuddyView::shape(lv_obj_t* parent, int x, int y, int w, int h, int radius, uint32_t fill,
                               uint32_t border, int border_width) {
  lv_obj_t* object = plain(parent);
  lv_obj_set_pos(object, x, y);
  lv_obj_set_size(object, w, h);
  lv_obj_set_style_radius(object, radius, 0);
  lv_obj_set_style_bg_color(object, to_lv(fill), 0);
  lv_obj_set_style_bg_grad_color(object, to_lv(ui::mix(fill, ui::tok::kWhite, 0.18f)), 0);
  lv_obj_set_style_bg_grad_dir(object, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(object, to_lv(border), 0);
  lv_obj_set_style_border_width(object, border_width, 0);
  return object;
}

void RoboBuddyView::visible(lv_obj_t* object, bool show) {
  if (show) {
    lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
  }
}

void RoboBuddyView::create(lv_obj_t* parent) {
  using G = RoboBuddyGame;
  lv_obj_set_style_bg_color(parent, to_lv(pal::kSky), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  ui::show_backdrop(parent, ui::World::WORKSHOP);

  lv_obj_t* shadow = shape(parent, 119, 207, 82, 12, LV_RADIUS_CIRCLE, pal::kInk, pal::kInk, 0);
  lv_obj_set_style_bg_opa(shadow, LV_OPA_20, 0);

  antenna_ = lv_line_create(parent);
  antenna_points_[0] = {160, 78};
  antenna_points_[1] = {160, 63};
  antenna_points_[2] = {168, 57};
  lv_line_set_points(antenna_, antenna_points_, 3);
  line_style(antenna_, kRobotEdge, 5);
  antenna_tip_ = shape(parent, 162, 50, 14, 14, LV_RADIUS_CIRCLE, kAntenna, kRobotEdge, 2);

  left_arm_ = shape(parent, 78, 124, 38, 15, 8, kRobotShade, kRobotEdge, 2);
  right_arm_ = shape(parent, 204, 124, 38, 15, 8, kRobotShade, kRobotEdge, 2);
  left_hand_ = shape(parent, 78, 130, 19, 19, LV_RADIUS_CIRCLE, pal::kYellow, kRobotEdge, 2);
  right_hand_ = shape(parent, 223, 130, 19, 19, LV_RADIUS_CIRCLE, pal::kYellow, kRobotEdge, 2);

  body_ = shape(parent, 99, 67, 122, 147, 38, kRobot, kRobotEdge, 4);
  face_panel_ = shape(parent, 109, 78, 102, 67, 26, kFace, kRobotEdge, 3);

  for (int i = 0; i < 2; ++i) {
    const int x = i == 0 ? 132 : 174;
    eyes_[i] = shape(parent, x, 94, 15, 20, LV_RADIUS_CIRCLE, pal::kWhite, kRobotEdge, 1);
    pupils_[i] = shape(parent, x + 5, 100, 7, 9, LV_RADIUS_CIRCLE, pal::kInk, pal::kInk, 0);
    closed_eyes_[i] = lv_line_create(parent);
    closed_eye_points_[i][0] = {static_cast<lv_value_precise_t>(x - 1), 105};
    closed_eye_points_[i][1] = {static_cast<lv_value_precise_t>(x + 7), 109};
    closed_eye_points_[i][2] = {static_cast<lv_value_precise_t>(x + 16), 105};
    lv_line_set_points(closed_eyes_[i], closed_eye_points_[i], 3);
    line_style(closed_eyes_[i], pal::kInk, 4);
    heart_parts_[i][0] = shape(parent, x - 1, 97, 10, 10, LV_RADIUS_CIRCLE, pal::kPink, pal::kPink, 0);
    heart_parts_[i][1] = shape(parent, x + 7, 97, 10, 10, LV_RADIUS_CIRCLE, pal::kPink, pal::kPink, 0);
    heart_parts_[i][2] = shape(parent, x + 3, 104, 12, 10, LV_RADIUS_CIRCLE, pal::kPink, pal::kPink, 0);
    for (lv_obj_t* part : heart_parts_[i]) visible(part, false);
  }

  mouth_ = lv_line_create(parent);
  line_style(mouth_, pal::kMouth, 4);
  tongue_ = shape(parent, 154, 127, 13, 9, 6, pal::kPink, pal::kMouth, 1);
  visible(tongue_, false);
  for (int i = 0; i < 2; ++i) {
    cheek_dots_[i] = shape(parent, i == 0 ? 118 : 195, 120, 8, 5, LV_RADIUS_CIRCLE, pal::kCheek, pal::kCheek, 0);
  }

  belly_ = shape(parent, 124, 153, 72, 48, 22, kRobotShade, kRobotEdge, 2);
  belly_dial_ = shape(parent, 146, 164, 28, 27, LV_RADIUS_CIRCLE, pal::kYellow, kRobotEdge, 2);
  for (int i = 0; i < 3; ++i) {
    shape(parent, 132 + i * 10, 184, 6, 6, LV_RADIUS_CIRCLE, pal::kWhite, pal::kWhite, 0);
  }

  food_button_ = shape(parent, G::kFoodButton.x + 3, G::kFoodButton.y + 3, G::kFoodButton.w - 6, G::kFoodButton.h - 6,
                       24, pal::kWhite, pal::kChromeEdge, 3);
  food_icon_ = shape(parent, G::kFoodButton.x + 23, G::kFoodButton.y + 23, 34, 34, LV_RADIUS_CIRCLE,
                     kSnackColors[0], kRobotEdge, 2);
  for (int i = 0; i < 3; ++i) {
    food_marks_[i] = shape(parent, G::kFoodButton.x + 31 + i * 9, G::kFoodButton.y + 31 + (i % 2) * 9, 4, 4,
                           LV_RADIUS_CIRCLE, pal::kInk, pal::kInk, 0);
  }

  sleep_button_ = shape(parent, G::kSleepButton.x + 3, G::kSleepButton.y + 3, G::kSleepButton.w - 6,
                        G::kSleepButton.h - 6, 24, pal::kWhite, pal::kChromeEdge, 3);
  shape(parent, G::kSleepButton.x + 25, G::kSleepButton.y + 22, 30, 34, LV_RADIUS_CIRCLE,
        pal::kPurple, pal::kPurple, 0);
  shape(parent, G::kSleepButton.x + 35, G::kSleepButton.y + 17, 27, 29, LV_RADIUS_CIRCLE,
        pal::kWhite, pal::kWhite, 0);
  for (int i = 0; i < 2; ++i) {
    zeds_[i] = lv_label_create(parent);
    lv_label_set_text_static(zeds_[i], i == 0 ? "Z" : "z");
    lv_obj_set_style_text_color(zeds_[i], to_lv(pal::kPurple), 0);
    lv_obj_set_pos(zeds_[i], 207 + i * 12, 88 - i * 9);
    visible(zeds_[i], false);
  }

  hello_button_ = shape(parent, G::kHelloButton.x + 2, G::kHelloButton.y + 2, G::kHelloButton.w - 4,
                        G::kHelloButton.h - 4, LV_RADIUS_CIRCLE, pal::kWhite, pal::kChromeEdge, 2);
  hello_icon_ = lv_label_create(hello_button_);
  lv_label_set_text_static(hello_icon_, "?");
  lv_obj_set_style_text_color(hello_icon_, to_lv(kRobotEdge), 0);
  lv_obj_center(hello_icon_);

}

void RoboBuddyView::sync_face(Mood mood) {
  if (mood == shown_mood_) return;
  shown_mood_ = mood;
  const bool heart_eyes = mood == Mood::YUMMY;
  const bool closed = mood == Mood::HAPPY || mood == Mood::SLEEPY;
  const bool wink = mood == Mood::SILLY;
  for (int i = 0; i < 2; ++i) {
    visible(eyes_[i], !heart_eyes && !closed && !(wink && i == 1));
    visible(pupils_[i], !heart_eyes && !closed && !(wink && i == 1));
    visible(closed_eyes_[i], closed || (wink && i == 1));
    for (lv_obj_t* part : heart_parts_[i]) visible(part, heart_eyes);
  }

  if (mood == Mood::SLEEPY) {
    mouth_points_[0] = {154, 124};
    mouth_points_[1] = {160, 127};
    mouth_points_[2] = {166, 124};
  } else if (mood == Mood::YUMMY) {
    mouth_points_[0] = {153, 124};
    mouth_points_[1] = {156, 130};
    mouth_points_[2] = {163, 130};
    mouth_points_[3] = {167, 124};
  } else if (mood == Mood::SILLY) {
    mouth_points_[0] = {153, 123};
    mouth_points_[1] = {158, 127};
    mouth_points_[2] = {164, 124};
    mouth_points_[3] = {169, 126};
  } else {
    mouth_points_[0] = {150, 123};
    mouth_points_[1] = {154, 127};
    mouth_points_[2] = {160, 129};
    mouth_points_[3] = {166, 127};
    mouth_points_[4] = {170, 123};
  }
  const uint32_t points = mood == Mood::SLEEPY ? 3 : (mood == Mood::YUMMY ? 4 : (mood == Mood::SILLY ? 4 : 5));
  lv_line_set_points(mouth_, mouth_points_, points);
  visible(tongue_, mood == Mood::SILLY);
  visible(cheek_dots_[0], mood == Mood::YUMMY || mood == Mood::HAPPY);
  visible(cheek_dots_[1], mood == Mood::YUMMY || mood == Mood::HAPPY);
}

void RoboBuddyView::sync_snack(Snack snack) {
  if (snack == shown_snack_) return;
  shown_snack_ = snack;
  const uint32_t color = kSnackColors[static_cast<uint8_t>(snack) % 3];
  lv_obj_set_style_bg_color(food_icon_, to_lv(color), 0);
  const bool cookie = snack == Snack::BOLT_COOKIE;
  const bool battery = snack == Snack::BATTERY_BAR;
  lv_obj_set_style_radius(food_icon_, battery ? 5 : LV_RADIUS_CIRCLE, 0);
  for (int i = 0; i < 3; ++i) visible(food_marks_[i], cookie || (!battery && i == 0));
}

void RoboBuddyView::sync(const RoboBuddyGame& game, uint32_t now_ms) {
  sync_face(game.mood());
  sync_snack(game.snack());

  const bool sleeping = game.sleeping();
  for (lv_obj_t* z : zeds_) visible(z, sleeping && !game.dozing());
  pulse_outline(food_button_, game.hint_active() && !sleeping, now_ms);
  pulse_outline(sleep_button_, game.hint_active() && sleeping, now_ms);
  pulse_outline(hello_button_, game.hint_active(), now_ms);

  float bob = 0.0f;
  if (game.dancing()) bob = std::sin(static_cast<float>(now_ms) / 125.0f) * 3.0f;
  else if (game.phase() == Phase::CELEBRATE) bob = std::sin(static_cast<float>(now_ms) / 130.0f) * 2.0f;
  else if (sleeping) bob = std::sin(static_cast<float>(now_ms) / 550.0f) * 1.5f;
  const int dy = static_cast<int>(std::lround(bob));
  lv_obj_set_y(body_, 67 + dy);
  lv_obj_set_y(face_panel_, 78 + dy);
  lv_obj_set_y(belly_, 153 + dy);
  lv_obj_set_y(belly_dial_, 164 + dy);
  lv_obj_set_y(antenna_tip_, 50 + dy +
                                 (game.mood() == Mood::HAPPY
                                      ? static_cast<int>(std::sin(static_cast<float>(now_ms) / 110.0f) * 2.0f)
                                      : 0));
  for (int i = 0; i < 2; ++i) {
    lv_obj_set_y(eyes_[i], 94 + dy);
    lv_obj_set_y(pupils_[i], 100 + dy);
    lv_obj_set_y(heart_parts_[i][0], 97 + dy);
    lv_obj_set_y(heart_parts_[i][1], 97 + dy);
    lv_obj_set_y(heart_parts_[i][2], 104 + dy);
    const int base_x = i == 0 ? 132 : 174;
    closed_eye_points_[i][0] = {static_cast<lv_value_precise_t>(base_x - 1), 105};
    closed_eye_points_[i][1] = {static_cast<lv_value_precise_t>(base_x + 7), 109};
    closed_eye_points_[i][2] = {static_cast<lv_value_precise_t>(base_x + 16), 105};
    lv_line_set_points(closed_eyes_[i], closed_eye_points_[i], 3);
  }
  antenna_points_[0] = {160, static_cast<lv_value_precise_t>(78 + dy)};
  antenna_points_[1] = {160, static_cast<lv_value_precise_t>(63 + dy)};
  antenna_points_[2] = {168, static_cast<lv_value_precise_t>(57 + dy)};
  lv_line_set_points(antenna_, antenna_points_, 3);
  lv_obj_set_y(mouth_, dy);
  for (lv_obj_t* eye : closed_eyes_) lv_obj_set_y(eye, dy);
  lv_obj_set_y(tongue_, 127 + dy);
  lv_obj_set_y(cheek_dots_[0], 120 + dy);
  lv_obj_set_y(cheek_dots_[1], 120 + dy);
  lv_obj_set_style_bg_color(belly_dial_, to_lv(sleeping ? pal::kBlue : pal::kYellow), 0);
  if (sleeping && !game.dozing()) lv_obj_set_style_bg_color(belly_dial_, to_lv(kRobotShade), 0);

  const bool waving = game.mood() == Mood::HAPPY || game.dancing();
  const int pose_offset = game.phase() == Phase::CELEBRATE ? (static_cast<int>(game.pose()) - 1) * 3 : 0;
  lv_obj_set_y(left_arm_, 124 + dy - pose_offset);
  lv_obj_set_y(left_hand_, 130 + dy - pose_offset);
  lv_obj_set_y(right_arm_, (waving ? 111 : 124) + dy + pose_offset);
  lv_obj_set_y(right_hand_, (waving ? 111 : 130) + dy + pose_offset);
}

}  // namespace little_arcade::robo_buddy
