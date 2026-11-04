#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

namespace little_arcade::shape_garage {

// Three shape types; every round uses each exactly once.
enum class Shape : uint8_t { CIRCLE, SQUARE, TRIANGLE };
inline constexpr std::size_t kShapeCount = 3;

enum class Phase : uint8_t { INTRO, PLAY, CELEBRATE };

// PROMPT variants: 0 = "Let's fix the robot!", 1 + shape = "Where does the <shape> go?".
inline constexpr uint8_t kPromptFixRobot = 0;
constexpr uint8_t prompt_for(Shape s) { return static_cast<uint8_t>(1u + static_cast<uint8_t>(s)); }

// A shaped hole on the robot's chest.
struct Slot {
  Shape shape;
  Rect rect;
  bool filled;
};

// A loose part in the tray at the bottom of the screen.
struct Piece {
  Shape shape;
  Rect rect;  // home position in the tray
  bool placed;
};

// Pure scene logic; an LVGL view reads the accessors and renders. Owns no
// heap memory. All timing is driven by update(dt_ms).
//
// Play is tap-to-place: tap a piece to lift it, then tap a slot. Slot-first
// also works (tapping an empty slot lifts its matching piece). Dragging is
// never required.
class ShapeGarageGame final : public IGame {
 public:
  static constexpr std::size_t kPieceCount = kShapeCount;
  // Robot chest slots (upper play area) and tray positions (bottom).
  static constexpr Rect kSlotRects[kPieceCount] = {
      {48, 72, 64, 64}, {128, 72, 64, 64}, {208, 72, 64, 64}};
  static constexpr Rect kTrayRects[kPieceCount] = {
      {48, 164, 64, 64}, {128, 164, 64, 64}, {208, 164, 64, 64}};
  // Slots accept taps slightly outside their drawn edge (forgiving aim).
  static constexpr int16_t kSlotForgiveness = 6;
  // Robot head in the top bar: shows the lifted shape (or a wrench) and is
  // the touch alternative to AGAIN/HELP.
  static constexpr Rect kRobotHead{132, 4, 56, 56};

  static constexpr uint32_t kIntroMs = 1200;
  static constexpr uint32_t kCelebrateMs = 1800;
  static constexpr uint32_t kSnapMs = 300;
  static constexpr uint32_t kWiggleMs = 400;
  static constexpr uint32_t kHintMs = 1500;
  // The first mismatch with a lifted piece gives a gentle nudge + hint; the
  // next mismatch with the same piece lets it slide into its own slot.
  static constexpr uint8_t kNudgesBeforeAssist = 1;
  static constexpr uint8_t kRobotStyles = 3;

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override;

  Phase phase() const { return phase_; }
  const Slot& slot(std::size_t i) const { return slots_[i]; }
  const Piece& piece(std::size_t i) const { return pieces_[i]; }
  // Index of the lifted piece, or -1.
  int selected() const { return selected_; }
  std::size_t filled_count() const;
  uint32_t robots_repaired() const { return rounds_; }
  uint8_t robot_style() const { return robot_style_; }
  // Glowing slot / piece hint, or -1.
  int hint_slot() const { return hint_ms_ > 0 ? hint_slot_ : -1; }
  int hint_piece() const { return hint_ms_ > 0 ? hint_piece_ : -1; }
  // Piece doing a gentle "not there" wiggle, or -1.
  int wiggling() const { return wiggle_ms_ > 0 ? wiggle_index_ : -1; }
  // Slot whose piece is currently snapping in, or -1.
  int snapping() const { return snap_ms_ > 0 ? snap_slot_ : -1; }
  bool active() const { return ctx_ != nullptr; }

  // Forgiving slot hit area (drawn rect grown by kSlotForgiveness).
  static constexpr Rect slot_hit_area(const Rect& r) {
    return Rect{static_cast<int16_t>(r.x - kSlotForgiveness), static_cast<int16_t>(r.y - kSlotForgiveness),
                static_cast<int16_t>(r.w + 2 * kSlotForgiveness),
                static_cast<int16_t>(r.h + 2 * kSlotForgiveness)};
  }

 private:
  void new_round();
  void start_play();
  void select_piece(int index);
  void put_back();
  void try_place(int slot_index);
  void place(int piece_index, int slot_index);
  void replay_prompt();
  void set_hint(int slot_index, int piece_index);
  int slot_for(Shape s) const;
  int piece_for(Shape s) const;
  int first_unplaced() const;
  int hit_piece(int16_t x, int16_t y) const;
  int hit_slot(int16_t x, int16_t y) const;

  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  Slot slots_[kPieceCount]{};
  Piece pieces_[kPieceCount]{};
  int selected_ = -1;
  uint8_t nudges_ = 0;
  uint8_t robot_style_ = 0;
  uint32_t rounds_ = 0;
  uint32_t phase_ms_ = 0;
  uint32_t hint_ms_ = 0;
  int hint_slot_ = -1;
  int hint_piece_ = -1;
  uint32_t wiggle_ms_ = 0;
  int wiggle_index_ = -1;
  uint32_t snap_ms_ = 0;
  int snap_slot_ = -1;
};

}  // namespace little_arcade::shape_garage
