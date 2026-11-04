#pragma once
#include <cstdint>

#include "little_arcade/intent.hpp"

namespace little_arcade {

// Push-to-talk listening window (docs/voice.md): stop after 4 s of silence or
// 8 s total. Pure timing logic; the capture task owns the microphone and must
// discard audio when the session ends. No audio is ever stored.
class PushToTalkSession {
 public:
  struct Config {
    uint32_t silence_timeout_ms = 4000;
    uint32_t max_duration_ms = 8000;
  };
  enum class State : uint8_t { IDLE, LISTENING };
  enum class EndReason : uint8_t { NONE, SILENCE, MAX_DURATION, RESULT, CANCELLED };

  PushToTalkSession() : PushToTalkSession(Config{}) {}
  explicit PushToTalkSession(Config config) : config_(config) {}

  // Returns false if already listening (repeated mic taps are harmless).
  bool start(uint32_t now_ms);
  void on_voice_activity(uint32_t now_ms);
  // Call periodically; returns the reason the session ended this tick, if any.
  EndReason tick(uint32_t now_ms);
  void finish_with_result() { end(EndReason::RESULT); }
  void cancel() { end(EndReason::CANCELLED); }

  State state() const { return state_; }
  bool listening() const { return state_ == State::LISTENING; }
  EndReason last_end_reason() const { return last_end_; }

 private:
  void end(EndReason reason);

  Config config_;
  State state_ = State::IDLE;
  EndReason last_end_ = EndReason::NONE;
  uint32_t started_ms_ = 0;
  uint32_t last_activity_ms_ = 0;
};

// Decides whether a recognizer result becomes a typed intent event.
// Policy from docs/voice.md: context-restricted grammar, never act on low
// confidence, 750 ms dedupe, 1.5 s cooldown after speaker playback (no AEC).
class VoiceIntentFilter {
 public:
  struct Config {
    uint8_t min_confidence_percent = 70;  // provisional; calibrate in V01
    uint32_t dedupe_ms = 750;
    uint32_t playback_cooldown_ms = 1500;
  };
  enum class Verdict : uint8_t {
    ACCEPT,
    LOW_CONFIDENCE,     // UI: soft "say it again" + show touch choices
    OUT_OF_CONTEXT,     // silently ignored
    DUPLICATE,          // silently ignored
    PLAYBACK_COOLDOWN,  // likely self-trigger from speaker
    UNKNOWN_INTENT,
  };

  VoiceIntentFilter() : VoiceIntentFilter(Config{}) {}
  explicit VoiceIntentFilter(Config config) : config_(config) {}

  void on_playback_finished(uint32_t now_ms);
  void set_playback_active(bool active) { playback_active_ = active; }
  Verdict evaluate(Intent intent, uint8_t confidence_percent, uint32_t active_contexts,
                   uint32_t now_ms);

 private:
  Config config_;
  bool playback_active_ = false;
  bool have_playback_end_ = false;
  uint32_t playback_end_ms_ = 0;
  bool have_last_ = false;
  Intent last_intent_ = Intent::UNKNOWN;
  uint32_t last_accept_ms_ = 0;
};

}  // namespace little_arcade
