#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

namespace little_arcade::color_pop {

enum class Color : uint8_t { RED, BLUE, GREEN, YELLOW };
inline constexpr std::size_t kColorCount = 4;

// Non-colour clue drawn inside each bubble so colour is never the only
// distinguishing feature (docs/acceptance.md accessibility).
enum class Pattern : uint8_t { HEART, WAVE, LEAF, STAR };
constexpr Pattern pattern_for(Color c) { return static_cast<Pattern>(static_cast<uint8_t>(c)); }

enum class Phase : uint8_t { INTRO, PLAY, CELEBRATE };

struct Bubble {
  Color color;
  Rect rect;
  bool popped;
};

// Pure scene logic; an LVGL view reads the accessors and renders. Owns no
// heap memory. All timing is driven by update(dt_ms).
class ColorPopGame final : public IGame {
 public:
  static constexpr std::size_t kMinBubbles = 2;
  static constexpr std::size_t kMaxBubbles = 4;
  static constexpr std::size_t kSlotCount = 6;
  static constexpr Rect kSlots[kSlotCount] = {
      {56, 80, 48, 48},  {136, 80, 48, 48},  {216, 80, 48, 48},
      {56, 142, 48, 48}, {136, 142, 48, 48}, {216, 142, 48, 48},
  };
  // Shows the requested colour + pattern, so the game works with audio off.
  static constexpr Rect kTargetBadge{132, 4, 56, 56};
  static constexpr uint32_t kIntroMs = 1200;
  static constexpr uint32_t kCelebrateMs = 1500;
  static constexpr uint32_t kWiggleMs = 400;
  static constexpr uint32_t kHintMs = 1500;

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override;

  Phase phase() const { return phase_; }
  std::size_t bubble_count() const { return count_; }
  const Bubble& bubble(std::size_t i) const { return bubbles_[i]; }
  Color target() const { return target_; }
  uint32_t rounds_completed() const { return rounds_; }
  bool hint_active() const { return hint_ms_ > 0; }
  // Index of the bubble doing a gentle "not me" wiggle, or -1.
  int wiggling() const { return wiggle_ms_ > 0 ? wiggle_index_ : -1; }
  bool active() const { return ctx_ != nullptr; }

 private:
  void new_round();
  void start_play();
  void choose(int index);
  int index_of(Color c) const;
  int hit_test(int16_t x, int16_t y) const;

  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  Bubble bubbles_[kMaxBubbles]{};
  std::size_t count_ = 0;
  Color target_ = Color::RED;
  bool have_previous_target_ = false;
  uint32_t rounds_ = 0;
  uint32_t phase_ms_ = 0;
  uint32_t hint_ms_ = 0;
  uint32_t wiggle_ms_ = 0;
  int wiggle_index_ = -1;
};

}  // namespace little_arcade::color_pop
