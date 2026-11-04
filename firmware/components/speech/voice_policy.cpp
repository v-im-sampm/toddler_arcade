#include "little_arcade/voice_policy.hpp"

namespace little_arcade {

namespace {
uint32_t elapsed(uint32_t now, uint32_t then) { return static_cast<uint32_t>(now - then); }
}  // namespace

bool PushToTalkSession::start(uint32_t now_ms) {
  if (state_ == State::LISTENING) return false;
  state_ = State::LISTENING;
  last_end_ = EndReason::NONE;
  started_ms_ = now_ms;
  last_activity_ms_ = now_ms;
  return true;
}

void PushToTalkSession::on_voice_activity(uint32_t now_ms) {
  if (state_ == State::LISTENING) last_activity_ms_ = now_ms;
}

PushToTalkSession::EndReason PushToTalkSession::tick(uint32_t now_ms) {
  if (state_ != State::LISTENING) return EndReason::NONE;
  if (elapsed(now_ms, started_ms_) >= config_.max_duration_ms) {
    end(EndReason::MAX_DURATION);
  } else if (elapsed(now_ms, last_activity_ms_) >= config_.silence_timeout_ms) {
    end(EndReason::SILENCE);
  } else {
    return EndReason::NONE;
  }
  return last_end_;
}

void PushToTalkSession::end(EndReason reason) {
  if (state_ != State::LISTENING) return;
  state_ = State::IDLE;
  last_end_ = reason;
}

void VoiceIntentFilter::on_playback_finished(uint32_t now_ms) {
  playback_active_ = false;
  have_playback_end_ = true;
  playback_end_ms_ = now_ms;
}

VoiceIntentFilter::Verdict VoiceIntentFilter::evaluate(Intent intent, uint8_t confidence_percent,
                                                       uint32_t active_contexts,
                                                       uint32_t now_ms) {
  if (intent_info(intent) == nullptr) return Verdict::UNKNOWN_INTENT;
  if (playback_active_ ||
      (have_playback_end_ && elapsed(now_ms, playback_end_ms_) < config_.playback_cooldown_ms)) {
    return Verdict::PLAYBACK_COOLDOWN;
  }
  if (confidence_percent < config_.min_confidence_percent) return Verdict::LOW_CONFIDENCE;
  if (!intent_allowed(intent, active_contexts)) return Verdict::OUT_OF_CONTEXT;
  if (have_last_ && intent == last_intent_ && elapsed(now_ms, last_accept_ms_) < config_.dedupe_ms) {
    return Verdict::DUPLICATE;
  }
  have_last_ = true;
  last_intent_ = intent;
  last_accept_ms_ = now_ms;
  return Verdict::ACCEPT;
}

}  // namespace little_arcade
