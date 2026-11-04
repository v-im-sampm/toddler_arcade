#include "little_arcade/games/rainbow_piano.hpp"

namespace little_arcade::rainbow_piano {

namespace {
uint32_t saturating_sub(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }

int hit_test(const Rect* rects, std::size_t count, int16_t x, int16_t y) {
  for (std::size_t i = 0; i < count; ++i) {
    if (rects[i].contains(x, y)) return static_cast<int>(i);
  }
  return -1;
}
}  // namespace

void RainbowPianoGame::enter(GameContext& context) {
  ctx_ = &context;
  instrument_ = Instrument::BELLS;
  notes_played_ = 0;
  tunes_ = 0;
  last_key_ = -1;
  for (auto& g : glow_ms_) g = 0;
  new_tune();
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

void RainbowPianoGame::exit() {
  ctx_ = nullptr;
  for (auto& g : glow_ms_) g = 0;
  tune_pos_ = 0;
  last_key_ = -1;
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

uint32_t RainbowPianoGame::active_contexts() const {
  // Only AGAIN (Global) is used; the SceneManager always adds Global.
  return 0;
}

void RainbowPianoGame::new_tune() {
  // Short random tune; consecutive notes differ so the light visibly moves.
  for (std::size_t i = 0; i < kTuneLength; ++i) {
    uint8_t key = static_cast<uint8_t>(ctx_->rng.below(static_cast<uint32_t>(kKeyCount)));
    if (i > 0 && key == tune_[i - 1]) key = static_cast<uint8_t>((key + 1) % kKeyCount);
    tune_[i] = key;
  }
  tune_pos_ = 0;
}

void RainbowPianoGame::start_play(bool with_prompt) {
  phase_ = Phase::PLAY;
  phase_ms_ = 0;
  if (with_prompt) ctx_->feedback.play(Cue::PROMPT, 0);
}

void RainbowPianoGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  for (auto& g : glow_ms_) g = saturating_sub(g, dt_ms);
  phase_ms_ += dt_ms;
  if (phase_ == Phase::INTRO && phase_ms_ >= kIntroMs) {
    start_play(true);
  } else if (phase_ == Phase::CELEBRATE && phase_ms_ >= kCelebrateMs) {
    new_tune();
    phase_ = Phase::PLAY;
    phase_ms_ = 0;
  }
}

void RainbowPianoGame::press_key(std::size_t key) {
  // Synchronous: the note is requested before handle() returns (<100 ms goal).
  ctx_->feedback.play(Cue::NOTE, note_variant(instrument_, key));
  glow_ms_[key] = kKeyGlowMs;
  last_key_ = static_cast<int>(key);
  ++notes_played_;
  if (phase_ != Phase::PLAY || tune_[tune_pos_] != key) return;  // free play: no penalty
  ++tune_pos_;
  if (tune_pos_ >= kTuneLength) {
    ++tunes_;
    tune_pos_ = 0;
    phase_ = Phase::CELEBRATE;
    phase_ms_ = 0;
    ctx_->feedback.play(Cue::CELEBRATE, 0);
  }
}

void RainbowPianoGame::select_instrument(std::size_t index) {
  instrument_ = static_cast<Instrument>(index);
  ctx_->feedback.play(Cue::TAP, static_cast<uint8_t>(index));
}

void RainbowPianoGame::handle(const InputEvent& event) {
  if (!ctx_) return;
  // Any input skips the intro and still acts; the prompt is skipped so it
  // never talks over the child's first note.
  if (phase_ == Phase::INTRO) start_play(false);

  if (event.source == Source::TOUCH) {
    const int key = hit_test(kKeys, kKeyCount, event.x, event.y);
    if (key >= 0) {
      press_key(static_cast<std::size_t>(key));
      return;
    }
    const int inst = hit_test(kInstrumentButtons, kInstrumentCount, event.x, event.y);
    if (inst >= 0) select_instrument(static_cast<std::size_t>(inst));
    return;
  }

  if (event.intent == Intent::AGAIN) {
    // Same as tapping the last key again (or the lit key if none yet).
    const int key = last_key_ >= 0 ? last_key_ : static_cast<int>(tune_[tune_pos_]);
    press_key(static_cast<std::size_t>(key));
  }
}

}  // namespace little_arcade::rainbow_piano
