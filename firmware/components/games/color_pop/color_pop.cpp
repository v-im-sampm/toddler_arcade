#include "little_arcade/games/color_pop.hpp"

namespace little_arcade::color_pop {

namespace {
uint32_t saturating_sub(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }
}  // namespace

void ColorPopGame::enter(GameContext& context) {
  ctx_ = &context;
  rounds_ = 0;
  have_previous_target_ = false;
  new_round();
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

void ColorPopGame::exit() {
  ctx_ = nullptr;
  count_ = 0;
  hint_ms_ = 0;
  wiggle_ms_ = 0;
  wiggle_index_ = -1;
  phase_ = Phase::INTRO;
}

uint32_t ColorPopGame::active_contexts() const {
  return static_cast<uint32_t>(Context::Colors);
}

void ColorPopGame::new_round() {
  // Grow gently from 2 to 4 bubbles; never shrinks back as a penalty.
  const std::size_t n = kMinBubbles + (rounds_ < kMaxBubbles - kMinBubbles
                                           ? rounds_
                                           : kMaxBubbles - kMinBubbles);
  Color colors[kColorCount] = {Color::RED, Color::BLUE, Color::GREEN, Color::YELLOW};
  uint8_t slots[kSlotCount] = {0, 1, 2, 3, 4, 5};
  ctx_->rng.shuffle(colors);
  ctx_->rng.shuffle(slots);
  count_ = n;
  for (std::size_t i = 0; i < n; ++i) bubbles_[i] = Bubble{colors[i], kSlots[slots[i]], false};

  std::size_t pick = ctx_->rng.below(static_cast<uint32_t>(n));
  if (have_previous_target_ && bubbles_[pick].color == target_) pick = (pick + 1) % n;
  target_ = bubbles_[pick].color;
  have_previous_target_ = true;
  hint_ms_ = 0;
  wiggle_ms_ = 0;
  wiggle_index_ = -1;
}

void ColorPopGame::start_play() {
  phase_ = Phase::PLAY;
  phase_ms_ = 0;
  ctx_->feedback.play(Cue::PROMPT, static_cast<uint8_t>(target_));
}

void ColorPopGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  hint_ms_ = saturating_sub(hint_ms_, dt_ms);
  wiggle_ms_ = saturating_sub(wiggle_ms_, dt_ms);
  phase_ms_ += dt_ms;
  if (phase_ == Phase::INTRO && phase_ms_ >= kIntroMs) {
    start_play();
  } else if (phase_ == Phase::CELEBRATE && phase_ms_ >= kCelebrateMs) {
    new_round();
    start_play();
  }
}

int ColorPopGame::index_of(Color c) const {
  for (std::size_t i = 0; i < count_; ++i) {
    if (bubbles_[i].color == c) return static_cast<int>(i);
  }
  return -1;
}

int ColorPopGame::hit_test(int16_t x, int16_t y) const {
  for (std::size_t i = 0; i < count_; ++i) {
    if (bubbles_[i].rect.contains(x, y)) return static_cast<int>(i);
  }
  return -1;
}

void ColorPopGame::choose(int index) {
  if (index < 0) {
    // Named a colour that is not on screen: soft nudge and show the answer.
    ctx_->feedback.play(Cue::TRY_AGAIN, 0);
    hint_ms_ = kHintMs;
    return;
  }
  Bubble& b = bubbles_[index];
  if (b.color == target_) {
    b.popped = true;
    ++rounds_;
    phase_ = Phase::CELEBRATE;
    phase_ms_ = 0;
    hint_ms_ = 0;
    ctx_->feedback.play(Cue::SUCCESS, static_cast<uint8_t>(b.color));
    ctx_->feedback.play(Cue::CELEBRATE, 0);
    return;
  }
  wiggle_index_ = index;
  wiggle_ms_ = kWiggleMs;
  hint_ms_ = kHintMs;
  ctx_->feedback.play(Cue::TRY_AGAIN, 0);
  ctx_->feedback.play(Cue::PROMPT, static_cast<uint8_t>(target_));
}

void ColorPopGame::handle(const InputEvent& event) {
  if (!ctx_ || phase_ == Phase::CELEBRATE) return;  // taps/voice during the pop are ignored

  if (phase_ == Phase::INTRO) {
    // Any input skips the intro; it is not counted as an answer.
    start_play();
    return;
  }

  if (event.source == Source::TOUCH) {
    const int hit = hit_test(event.x, event.y);
    if (hit >= 0) {
      ctx_->feedback.play(Cue::TAP, 0);
      choose(hit);
    } else if (kTargetBadge.contains(event.x, event.y)) {
      // Touch alternative to AGAIN.
      ctx_->feedback.play(Cue::PROMPT, static_cast<uint8_t>(target_));
      hint_ms_ = kHintMs;
    }
    return;
  }

  switch (event.intent) {
    case Intent::RED:
      choose(index_of(Color::RED));
      break;
    case Intent::BLUE:
      choose(index_of(Color::BLUE));
      break;
    case Intent::GREEN:
      choose(index_of(Color::GREEN));
      break;
    case Intent::YELLOW:
      choose(index_of(Color::YELLOW));
      break;
    case Intent::AGAIN:
    case Intent::HELP:
      ctx_->feedback.play(Cue::PROMPT, static_cast<uint8_t>(target_));
      hint_ms_ = kHintMs;
      break;
    default:
      break;
  }
}

}  // namespace little_arcade::color_pop
