#include "little_arcade/games/beat_sharks.hpp"

namespace little_arcade::beat_sharks {

namespace {
uint32_t saturating_sub(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }
}  // namespace

bool BeatSharksGame::full_ocean() const {
  for (std::size_t s = 0; s < kSteps; ++s) {
    bool any = false;
    for (std::size_t r = 0; r < kRows; ++r) any = any || cells_[r][s] != 0;
    if (!any) return false;
  }
  return true;
}

void BeatSharksGame::reset_pattern() {
  for (auto& row : cells_)
    for (auto& v : row) v = 0;
  for (auto& row : hop_ms_)
    for (auto& h : row) h = 0;
  // Starter beat: big BOOM on steps 1 and 4, so Play alone makes music.
  cells_[2][0] = 1;
  cells_[2][3] = 1;
}

void BeatSharksGame::enter(GameContext& context) {
  ctx_ = &context;
  reset_pattern();
  tempo_ = kDefaultTempo;
  playing_ = was_playing_ = false;
  step_ = -1;
  accum_ms_ = 0;
  builtin_ = 0;
  record_glow_ms_ = wiggle_ms_ = 0;
  stopping_ = false;
  ocean_armed_ = true;
  celebration_ = Celebration::NONE;
  steps_played_ = hits_ = caught_ = 0;
  if (sampler_) {
    sampler_->wipe();
    sampler_->set_effects(0);  // Sound Jar's effects never leak in
    sampler_->load_builtin(builtin_);
  }
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

void BeatSharksGame::exit() {
  if (sampler_) sampler_->wipe();  // nothing the child recorded outlives the game
  ctx_ = nullptr;
  playing_ = false;
  stopping_ = false;
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

void BeatSharksGame::on_pause() {
  playing_ = was_playing_ = false;
  stopping_ = false;
  for (auto& row : hop_ms_)
    for (auto& h : row) h = 0;
  if (sampler_) sampler_->wipe();  // also cancels a recording in progress
  if (phase_ == Phase::RECORDING || phase_ == Phase::CELEBRATE) {
    phase_ = Phase::PLAY;
    phase_ms_ = 0;
  }
}

void BeatSharksGame::on_resume() {
  // Comes back stopped (no surprise sound); the child presses Play.
  if (!sampler_) return;
  sampler_->set_effects(0);
  sampler_->load_builtin(builtin_);
}

void BeatSharksGame::privacy_wipe() {
  if (!ctx_) return;
  on_pause();
  on_resume();
}

void BeatSharksGame::to_play(bool prompt) {
  phase_ = Phase::PLAY;
  phase_ms_ = 0;
  celebration_ = Celebration::NONE;
  if (prompt) ctx_->feedback.play(Cue::PROMPT, 0);
}

void BeatSharksGame::start_transport() {
  playing_ = true;
  step_ = 0;
  accum_ms_ = 0;
  ++steps_played_;
  fire_step();
}

void BeatSharksGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  for (auto& row : hop_ms_)
    for (auto& h : row) h = static_cast<uint16_t>(saturating_sub(h, dt_ms));
  record_glow_ms_ = saturating_sub(record_glow_ms_, dt_ms);
  wiggle_ms_ = saturating_sub(wiggle_ms_, dt_ms);
  phase_ms_ += dt_ms;

  if (playing_) {
    // Remainder carry keeps the loop from drifting with uneven frame times.
    accum_ms_ += dt_ms;
    const uint32_t ms = kStepMs[tempo_];
    if (accum_ms_ >= ms) {
      const uint32_t n = accum_ms_ / ms;
      accum_ms_ %= ms;
      steps_played_ += n;
      // A long stall skips ahead and sounds only the current step, never a pile-up.
      step_ = static_cast<int>((static_cast<uint32_t>(step_) + n) % kSteps);
      fire_step();
    }
  }

  switch (phase_) {
    case Phase::INTRO:
      if (phase_ms_ >= kIntroMs) {
        to_play(true);
        start_transport();
      }
      break;
    case Phase::RECORDING: {
      if (!sampler_) {
        finish_recording(ISampler::RecordResult::TOO_QUIET);
        break;
      }
      if (!stopping_ && sampler_->record_progress_permille() >= kRecordStopPermille) {
        stopping_ = true;
        sampler_->stop_recording();
      }
      const ISampler::RecordResult r = sampler_->take_record_result();
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

void BeatSharksGame::fire_step() {
  if (step_ < 0) return;
  const std::size_t s = static_cast<std::size_t>(step_);
  for (std::size_t r = 0; r < kRows; ++r) {
    if (cells_[r][s] == 0) continue;
    hop_ms_[r][s] = kHopMs;
    play_sound(r, cells_[r][s]);
  }
}

void BeatSharksGame::play_sound(std::size_t row, uint8_t sound) {
  if (sound == 0 || sound > kSoundsPerRow) return;
  ++hits_;
  if (row == 0 && sampler_) {
    // The voice row is mono: a new hit fades the previous one out.
    sampler_->stop_all();
    sampler_->play(kVoiceRates[sound - 1], false);
    return;
  }
  ctx_->feedback.play(Cue::NOTE, static_cast<uint8_t>(row * kSoundsPerRow + sound - 1));
}

void BeatSharksGame::handle(const InputEvent& event) {
  if (!ctx_) return;
  const bool touch = event.source == Source::TOUCH;

  if (phase_ == Phase::RECORDING) {
    // Only the Record button (stop) reacts; voice never controls recording.
    if (touch && kRecordButton.contains(event.x, event.y) && !stopping_) {
      stopping_ = true;
      if (sampler_) sampler_->stop_recording();
    }
    return;
  }
  // Any input skips the intro and starts the beat, without talking over it.
  if (phase_ == Phase::INTRO) {
    to_play(false);
    start_transport();
    if (touch && kPlayButton.contains(event.x, event.y)) return;  // it is already playing
  }

  if (touch) {
    if (kPlayButton.contains(event.x, event.y)) {
      if (playing_) {
        playing_ = false;  // gentle: sounding hits finish, the fin parks
      } else {
        start_transport();
      }
      return;
    }
    if (kRecordButton.contains(event.x, event.y)) {
      press_record();
      return;
    }
    if (kTempoButton.contains(event.x, event.y)) {
      cycle_tempo();
      return;
    }
    for (std::size_t r = 0; r < kRows; ++r) {
      for (std::size_t s = 0; s < kSteps; ++s) {
        if (cell_rect(r, s).contains(event.x, event.y)) {
          tap_cell(r, s);
          return;
        }
      }
    }
    return;
  }

  if (event.intent == Intent::AGAIN) {
    start_transport();  // from step 1
  } else if (event.intent == Intent::HELP) {
    ctx_->feedback.play(Cue::PROMPT, 0);
  }
}

void BeatSharksGame::tap_cell(std::size_t row, std::size_t step) {
  uint8_t& v = cells_[row][step];
  v = static_cast<uint8_t>((v + 1) % (kSoundsPerRow + 1));
  if (v == 0) {
    hop_ms_[row][step] = 0;  // back to empty water, quietly
  } else {
    hop_ms_[row][step] = kHopMs;
    const bool fin_arriving = playing_ && step == (static_cast<std::size_t>(step_) + 1) % kSteps &&
                              kStepMs[tempo_] - accum_ms_ <= kPreviewGuardMs;
    if (!fin_arriving) play_sound(row, v);
  }
  check_full_ocean();
}

void BeatSharksGame::cycle_tempo() {
  // Applies from the next step; the loop never restarts.
  tempo_ = static_cast<uint8_t>((tempo_ + 1) % kTempoCount);
  ctx_->feedback.play(Cue::TAP, tempo_);
}

void BeatSharksGame::press_record() {
  if (sparkle_mode()) {
    builtin_ = static_cast<uint8_t>((builtin_ + 1) % ISampler::kBuiltinCount);
    if (sampler_) sampler_->load_builtin(builtin_);
    record_glow_ms_ = kGlowMs;
    place_voice_sharks_if_empty();
    play_sound(0, 2);
    check_full_ocean();
    return;
  }
  was_playing_ = playing_;
  playing_ = false;  // the fin holds its place while we listen
  sampler_->stop_all();
  sampler_->start_recording();
  phase_ = Phase::RECORDING;
  phase_ms_ = 0;
  stopping_ = false;
}

void BeatSharksGame::finish_recording(ISampler::RecordResult result) {
  stopping_ = false;
  playing_ = was_playing_;
  was_playing_ = false;
  if (result == ISampler::RecordResult::ACCEPTED) {
    ++caught_;
    place_voice_sharks_if_empty();
    if (full_ocean()) ocean_armed_ = false;  // one celebration at a time
    celebrate(Celebration::NEW_SOUND);
    record_glow_ms_ = kCelebrateMs;
    if (!playing_) play_sound(0, 2);  // hear the new sound right away
    return;
  }
  // Too quiet or too short: keep the old sound, gentle wiggle, no failure cue.
  to_play(false);
  wiggle_ms_ = kWiggleMs;
}

void BeatSharksGame::place_voice_sharks_if_empty() {
  for (uint8_t v : cells_[0])
    if (v != 0) return;
  cells_[0][0] = 2;  // middle shark = the sound as recorded
  cells_[0][3] = 2;
}

void BeatSharksGame::celebrate(Celebration kind) {
  phase_ = Phase::CELEBRATE;
  phase_ms_ = 0;
  celebration_ = kind;
  if (kind == Celebration::FULL_OCEAN) ctx_->feedback.play(Cue::CELEBRATE, 0);
}

void BeatSharksGame::check_full_ocean() {
  if (!full_ocean()) {
    ocean_armed_ = true;
    return;
  }
  if (!ocean_armed_) return;
  ocean_armed_ = false;
  celebrate(Celebration::FULL_OCEAN);
}

}  // namespace little_arcade::beat_sharks
