#include "little_arcade/games/sound_jar.hpp"

namespace little_arcade::sound_jar {

namespace {
uint32_t saturating_sub(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }

int hit_test(const Rect* rects, std::size_t count, int16_t x, int16_t y) {
  for (std::size_t i = 0; i < count; ++i) {
    if (rects[i].contains(x, y)) return static_cast<int>(i);
  }
  return -1;
}

constexpr int kPadPitchX = 64;  // distance between pad centres
constexpr int kFirstPadCentreX = 32;
}  // namespace

uint32_t SoundJarGame::rate_for_position(int pos_x100) {
  if (pos_x100 <= 0) return kPadRates[0];
  const int last = static_cast<int>(kPadCount - 1) * 100;
  if (pos_x100 >= last) return kPadRates[kPadCount - 1];
  const std::size_t i = static_cast<std::size_t>(pos_x100 / 100);
  const uint32_t f = static_cast<uint32_t>(pos_x100 % 100);
  return kPadRates[i] + (kPadRates[i + 1] - kPadRates[i]) * f / 100u;
}

void SoundJarGame::enter(GameContext& context) {
  ctx_ = &context;
  for (auto& g : glow_ms_) g = 0;
  jar_glow_ms_ = wiggle_ms_ = 0;
  held_pad_ = -1;
  bending_ = false;
  stopping_ = false;
  last_pad_ = -1;
  caught_ = plays_ = 0;
  fresh_jar();
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

void SoundJarGame::exit() {
  if (sampler_) sampler_->wipe();  // nothing the child recorded outlives the game
  ctx_ = nullptr;
  for (auto& g : glow_ms_) g = 0;
  held_pad_ = -1;
  bending_ = false;
  stopping_ = false;
  effects_ = 0;
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

void SoundJarGame::on_pause() {
  held_pad_ = -1;
  bending_ = false;
  stopping_ = false;
  if (sampler_) sampler_->wipe();  // also cancels a recording in progress
  if (phase_ == Phase::RECORDING) {
    phase_ = Phase::PLAY;
    phase_ms_ = 0;
  }
}

void SoundJarGame::on_resume() {
  if (!sampler_) return;
  sampler_->load_builtin(builtin_);
  sampler_->set_effects(effects_);
}

void SoundJarGame::privacy_wipe() {
  if (!ctx_) return;
  on_pause();
  on_resume();
}

void SoundJarGame::fresh_jar() {
  builtin_ = 0;
  effects_ = 0;
  if (!sampler_) return;
  sampler_->wipe();
  sampler_->load_builtin(builtin_);  // a boing is already in the jar
}

void SoundJarGame::to_play(bool prompt) {
  phase_ = Phase::PLAY;
  phase_ms_ = 0;
  if (prompt) ctx_->feedback.play(Cue::PROMPT, 0);
}

void SoundJarGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  for (auto& g : glow_ms_) g = saturating_sub(g, dt_ms);
  jar_glow_ms_ = saturating_sub(jar_glow_ms_, dt_ms);
  wiggle_ms_ = saturating_sub(wiggle_ms_, dt_ms);
  phase_ms_ += dt_ms;
  if (held_pad_ >= 0) {
    hold_ms_ += dt_ms;
    since_pointer_ms_ += dt_ms;
    if (since_pointer_ms_ >= kPointerLostMs) lift_pad();
  }

  switch (phase_) {
    case Phase::INTRO:
      if (phase_ms_ >= kIntroMs) to_play(true);
      break;
    case Phase::RECORDING: {
      const ISampler::RecordResult r = sampler_ ? sampler_->take_record_result() : ISampler::RecordResult::TOO_QUIET;
      if (r != ISampler::RecordResult::NONE) {
        finish_recording(r);
      } else if (phase_ms_ >= kRecordTimeoutMs) {
        sampler_->cancel_recording();
        finish_recording(ISampler::RecordResult::TOO_QUIET);
      }
      break;
    }
    case Phase::CELEBRATE:
      if (phase_ms_ >= kCelebrateMs) to_play(false);
      break;
    case Phase::PLAY:
      break;
  }
}

void SoundJarGame::handle(const InputEvent& event) {
  if (!ctx_) return;
  const bool touch = event.source == Source::TOUCH;

  if (phase_ == Phase::RECORDING) {
    // Only the Record button (stop) reacts; voice never controls recording.
    if (touch && kRecordButton.contains(event.x, event.y) && !stopping_) {
      stopping_ = true;
      sampler_->stop_recording();
    }
    return;
  }
  // Any input skips the intro and still acts, without talking over it.
  if (phase_ == Phase::INTRO) to_play(false);

  if (touch) {
    if (kRecordButton.contains(event.x, event.y)) {
      press_record();
      return;
    }
    if (kJar.contains(event.x, event.y)) {
      jar_glow_ms_ = kGlowMs;
      play_once(ISampler::kRateUnity);
      return;
    }
    const int fx = hit_test(kEffects, kEffectCount, event.x, event.y);
    if (fx >= 0) {
      toggle_effect(static_cast<std::size_t>(fx));
      return;
    }
    const int pad = hit_test(kPads, kPadCount, event.x, event.y);
    if (pad >= 0) {
      press_x_ = event.x;
      press_pad(static_cast<std::size_t>(pad));
    }
    return;
  }

  if (event.intent == Intent::AGAIN) {
    if (last_pad_ >= 0) {
      glow_ms_[last_pad_] = kGlowMs;
      play_once(kPadRates[last_pad_]);
    } else {
      jar_glow_ms_ = kGlowMs;
      play_once(ISampler::kRateUnity);
    }
  } else if (event.intent == Intent::HELP) {
    ctx_->feedback.play(Cue::PROMPT, 0);
  }
}

void SoundJarGame::pointer(bool pressed, int16_t x, int16_t y) {
  (void)y;  // sliding off the pads vertically keeps the note going
  since_pointer_ms_ = 0;
  if (held_pad_ < 0) return;
  if (!pressed) {
    lift_pad();
    return;
  }
  const int dx = x - press_x_;
  if (!bending_ && dx < kBendDeadZone && dx > -kBendDeadZone) return;
  bending_ = true;
  int pos = (x - kFirstPadCentreX) * 100 / kPadPitchX;
  const int last = static_cast<int>(kPadCount - 1) * 100;
  pos = pos < 0 ? 0 : (pos > last ? last : pos);
  bend_pos_ = pos;
  const uint32_t rate = rate_for_position(pos);
  if (rate != sent_rate_ && sampler_) sampler_->bend(rate);
  sent_rate_ = rate;
}

void SoundJarGame::press_pad(std::size_t pad) {
  if (held_pad_ >= 0) lift_pad();  // a missed lift: finish the old one first
  held_pad_ = static_cast<int>(pad);
  hold_ms_ = 0;
  since_pointer_ms_ = 0;
  bending_ = false;
  bend_pos_ = -1;
  glow_ms_[pad] = kGlowMs;
  last_pad_ = static_cast<int>(pad);
  ++plays_;
  sent_rate_ = kPadRates[pad];
  // Starts looping at once (< 100 ms); a short tap just ends the loop so the
  // sound plays through once.
  if (sampler_) {
    sampler_->play(kPadRates[pad], true);
  } else {
    ctx_->feedback.play(Cue::NOTE, static_cast<uint8_t>(pad));
  }
}

void SoundJarGame::lift_pad() {
  if (held_pad_ < 0) return;
  if (sampler_) {
    if (hold_ms_ >= kHoldLoopMs || bending_) {
      sampler_->release();
    } else {
      sampler_->end_loop();
    }
  }
  glow_ms_[held_pad_] = kGlowMs;
  held_pad_ = -1;
  bending_ = false;
  bend_pos_ = -1;
}

void SoundJarGame::toggle_effect(std::size_t i) {
  effects_ = static_cast<uint8_t>(effects_ ^ effect_bit(i));
  if (sampler_) {
    sampler_->set_effects(effects_);
  } else {
    ctx_->feedback.play(Cue::TAP, static_cast<uint8_t>(i));
  }
  play_once(ISampler::kRateUnity);  // hear the change right away
}

void SoundJarGame::play_once(uint32_t rate) {
  ++plays_;
  if (sampler_) {
    sampler_->play(rate, false);
  } else {
    ctx_->feedback.play(Cue::NOTE, 2);
  }
}

void SoundJarGame::press_record() {
  if (held_pad_ >= 0) lift_pad();
  if (sparkle_mode()) {
    builtin_ = static_cast<uint8_t>((builtin_ + 1) % ISampler::kBuiltinCount);
    if (sampler_) sampler_->load_builtin(builtin_);
    phase_ = Phase::CELEBRATE;
    phase_ms_ = 0;
    jar_glow_ms_ = kCelebrateMs;
    play_once(ISampler::kRateUnity);
    return;
  }
  sampler_->start_recording();
  phase_ = Phase::RECORDING;
  phase_ms_ = 0;
  stopping_ = false;
}

void SoundJarGame::finish_recording(ISampler::RecordResult result) {
  stopping_ = false;
  if (result == ISampler::RecordResult::ACCEPTED) {
    ++caught_;
    phase_ = Phase::CELEBRATE;
    phase_ms_ = 0;
    jar_glow_ms_ = kCelebrateMs;
    play_once(ISampler::kRateUnity);
    return;
  }
  // Too quiet or too short: keep the old sound, gentle wiggle, no failure cue.
  to_play(false);
  wiggle_ms_ = kWiggleMs;
}

}  // namespace little_arcade::sound_jar
