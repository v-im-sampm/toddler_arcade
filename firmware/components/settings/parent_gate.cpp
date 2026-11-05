#include "little_arcade/settings/parent_gate.hpp"

namespace little_arcade::settings {

ParentGate::ParentGate(uint32_t seed, uint8_t level, bool mic_enabled)
    : rng_(seed ^ 0x5EC0A7E5u), level_(level > kMaxLevel ? kMaxLevel : level), mic_enabled_(mic_enabled) {}

bool ParentGate::pointer(bool pressed, int16_t x, int16_t y, uint32_t now_ms) {
  const bool edge = pressed && !was_pressed_;
  was_pressed_ = pressed;

  if (state_ != State::CLOSED) {
    if (edge) {
      last_input_ms_ = now_ms;
      on_press(x, y);
    } else if (now_ms - last_input_ms_ >= kIdleCloseMs) {
      close();
    }
    return false;
  }

  // Closed: only a press that starts on the hotspot and stays there arms the
  // hold. The hotspot (Mic button) never reaches the games.
  if (holding_) {
    if (!pressed) {
      // A short gap is a touch dropout, not a release.
      if (!release_pending_) {
        release_pending_ = true;
        release_ms_ = now_ms;
      } else if (now_ms - release_ms_ > kReleaseGraceMs) {
        holding_ = false;
      }
      return false;
    }
    release_pending_ = false;
    if (!kHoldArea.contains(x, y)) {
      holding_ = false;  // slid away: a new press elsewhere still reaches the game
      return edge;
    }
    hold_ms_ = now_ms - hold_start_ms_;
    if (hold_ms_ >= kHoldMs) {
      holding_ = false;
      last_input_ms_ = now_ms;
      start_challenge();
    }
    return false;
  }
  if (edge && kHotspot.contains(x, y)) {
    holding_ = true;
    release_pending_ = false;
    hold_start_ms_ = now_ms;
    hold_ms_ = 0;
    return false;
  }
  return edge;
}

void ParentGate::start_challenge() {
  a_ = static_cast<uint8_t>(2 + rng_.below(6));  // 2..7
  b_ = static_cast<uint8_t>(2 + rng_.below(6));
  const uint8_t answer = static_cast<uint8_t>(a_ + b_);
  // Distractors: answer-1/-2.. and +1/+2.. so all three are distinct and > 0.
  const uint8_t lo = static_cast<uint8_t>(answer - 1 - rng_.below(2));
  const uint8_t hi = static_cast<uint8_t>(answer + 1 + rng_.below(2));
  uint8_t values[kChoiceCount] = {answer, lo, hi};
  rng_.shuffle(values);
  for (int i = 0; i < kChoiceCount; ++i) {
    choices_[i] = values[i];
    if (values[i] == answer) answer_index_ = static_cast<uint8_t>(i);
  }
  state_ = State::CHALLENGE;
}

void ParentGate::on_press(int16_t x, int16_t y) {
  if (state_ == State::CHALLENGE) {
    for (int i = 0; i < kChoiceCount; ++i) {
      if (!kChoiceRects[i].contains(x, y)) continue;
      if (i == answer_index_) {
        state_ = State::SETTINGS;
        level_at_open_ = level_;
        mic_at_open_ = mic_enabled_;
      } else {
        close();  // no retry loop and no failure screen
      }
      return;
    }
    return;
  }
  // SETTINGS
  for (int l = 0; l < kLevelCount; ++l) {
    if (level_rect(l).contains(x, y)) {
      level_ = static_cast<uint8_t>(l);
      preview_ = l > 0;
      return;
    }
  }
  if (kMicToggleRect.contains(x, y)) {
    mic_enabled_ = !mic_enabled_;
    return;
  }
  if (kDoneRect.contains(x, y)) close();
}

void ParentGate::close() {
  if (state_ == State::SETTINGS && level_ != level_at_open_) level_changed_ = true;
  if (state_ == State::SETTINGS && mic_enabled_ != mic_at_open_) mic_changed_ = true;
  state_ = State::CLOSED;
  holding_ = false;
  preview_ = false;
}

bool ParentGate::take_level_changed() {
  const bool c = level_changed_;
  level_changed_ = false;
  return c;
}

bool ParentGate::take_mic_changed() {
  const bool c = mic_changed_;
  mic_changed_ = false;
  return c;
}

bool ParentGate::take_preview() {
  const bool p = preview_;
  preview_ = false;
  return p;
}

}  // namespace little_arcade::settings
