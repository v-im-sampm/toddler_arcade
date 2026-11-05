#pragma once
#include <cstdint>

#include "little_arcade/geometry.hpp"
#include "little_arcade/rng.hpp"

namespace little_arcade::settings {

// Parent-only sound settings (docs/parental-safety.md, docs/acceptance.md G4):
// sound stays muted until a parent opts in. Entry is hidden behind holding the
// top-right Mic corner for 5 s plus a simple adult challenge (a sum with three
// answers); it is a toddler deterrent, not security. A wrong answer or 30 s of
// no input just closes the gate. Pure logic, UI task only, host-tested.
class ParentGate {
 public:
  enum class State : uint8_t { CLOSED, CHALLENGE, SETTINGS };

  static constexpr uint32_t kHoldMs = 5000;
  static constexpr uint32_t kIdleCloseMs = 30000;
  static constexpr Rect kHotspot{kScreenWidth - 56, 0, 56, 56};  // == kMicButton
  // Once a hold has started it survives small finger drift and brief NS2009
  // pressure dropouts (single not-pressed samples seen on the resistive panel).
  static constexpr int16_t kHoldSlackPx = 20;
  static constexpr Rect kHoldArea{kScreenWidth - 56 - kHoldSlackPx, 0, 56 + kHoldSlackPx, 56 + kHoldSlackPx};
  static constexpr uint32_t kReleaseGraceMs = 300;
  // Progress is shown only after this much holding, so quick taps stay quiet.
  static constexpr uint32_t kShowProgressMs = 1000;
  static constexpr uint8_t kMaxLevel = 6;  // == VolumeLimiter::kDefaultParentCap

  static constexpr int kChoiceCount = 3;
  static constexpr Rect kChoiceRects[kChoiceCount] = {
      {28, 120, 80, 64}, {120, 120, 80, 64}, {212, 120, 80, 64}};
  static constexpr int kLevelCount = kMaxLevel + 1;  // 0 (off) .. 6
  static constexpr Rect level_rect(int level) { return Rect{static_cast<int16_t>(6 + level * 44), 84, 40, 64}; }
  static constexpr Rect kDoneRect{100, 172, 120, 52};

  // `level` is the saved volume (0 = muted, the factory default).
  ParentGate(uint32_t seed, uint8_t level);

  // Raw pointer state, called every UI loop with the current touch sample.
  // Returns true when the caller should forward this press edge at (x, y) to
  // the launcher/game, i.e. it is a new press the gate did not consume.
  bool pointer(bool pressed, int16_t x, int16_t y, uint32_t now_ms);
  void close();

  State state() const { return state_; }
  bool open() const { return state_ != State::CLOSED; }
  uint8_t level() const { return level_; }
  // Milliseconds of the current hotspot hold (0 when not holding).
  uint32_t hold_ms() const { return holding_ ? hold_ms_ : 0; }
  uint8_t sum_a() const { return a_; }
  uint8_t sum_b() const { return b_; }
  uint8_t choice(int i) const { return choices_[i]; }
  // True once after the settings screen closes with a different level than
  // when it opened: the caller should persist level().
  bool take_level_changed();
  // True once after a level button was tapped (caller may play a preview).
  bool take_preview();

 private:
  void start_challenge();
  void on_press(int16_t x, int16_t y);

  Rng rng_;
  State state_ = State::CLOSED;
  uint8_t level_;
  uint8_t level_at_open_ = 0;
  uint8_t a_ = 0, b_ = 0;
  uint8_t choices_[kChoiceCount]{};
  uint8_t answer_index_ = 0;
  bool was_pressed_ = false;
  bool holding_ = false;
  uint32_t hold_start_ms_ = 0;
  uint32_t hold_ms_ = 0;
  bool release_pending_ = false;
  uint32_t release_ms_ = 0;
  uint32_t last_input_ms_ = 0;
  bool level_changed_ = false;
  bool preview_ = false;
};

}  // namespace little_arcade::settings
