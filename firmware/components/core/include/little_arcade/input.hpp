#pragma once
#include <cstdint>

#include "little_arcade/intent.hpp"

namespace little_arcade {

enum class Source : uint8_t { TOUCH, VOICE };

// Touch events carry screen coordinates (landscape 320x240) and may carry an
// intent when they hit a navigation control. Voice events carry an intent and
// x = y = -1. Game code switches on `intent`, never on recognizer text.
struct InputEvent {
  Source source;
  Intent intent;
  int16_t x;
  int16_t y;
  uint32_t timestamp_ms;
};

inline constexpr int16_t kNoCoordinate = -1;

constexpr InputEvent make_voice_event(Intent intent, uint32_t timestamp_ms) {
  return InputEvent{Source::VOICE, intent, kNoCoordinate, kNoCoordinate, timestamp_ms};
}

constexpr InputEvent make_touch_event(int16_t x, int16_t y, uint32_t timestamp_ms,
                                      Intent intent = Intent::UNKNOWN) {
  return InputEvent{Source::TOUCH, intent, x, y, timestamp_ms};
}

}  // namespace little_arcade
