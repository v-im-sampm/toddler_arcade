#include "little_arcade/games/feed_monster.hpp"

namespace little_arcade::feed_monster {

namespace {
uint32_t saturating_sub(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }
constexpr uint32_t kMaxExtraTracked = 999;
}  // namespace

void FeedMonsterGame::enter(GameContext& context) {
  ctx_ = &context;
  rounds_ = 0;
  have_previous_ = false;
  new_round();
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

void FeedMonsterGame::exit() {
  ctx_ = nullptr;
  phase_ = Phase::INTRO;
  fed_ = 0;
  extra_ = 0;
  chew_ms_ = 0;
  giggle_ms_ = 0;
  hint_ms_ = 0;
  idle_ms_ = 0;
}

uint32_t FeedMonsterGame::active_contexts() const {
  return static_cast<uint32_t>(Context::Counting);
}

void FeedMonsterGame::new_round() {
  const uint32_t span = static_cast<uint32_t>(kMaxRequest - kMinRequest + 1);
  uint8_t pick = static_cast<uint8_t>(kMinRequest + ctx_->rng.below(span));
  if (have_previous_ && pick == requested_) {
    pick = static_cast<uint8_t>(pick >= kMaxRequest ? kMinRequest : pick + 1);  // no back-to-back repeat
  }
  requested_ = pick;
  have_previous_ = true;
  fed_ = 0;
  extra_ = 0;
  chew_ms_ = 0;
  giggle_ms_ = 0;
  hint_ms_ = 0;
  idle_ms_ = 0;
  idle_hint_given_ = false;
}

void FeedMonsterGame::start_play() {
  phase_ = Phase::PLAY;
  phase_ms_ = 0;
  idle_ms_ = 0;
  ctx_->feedback.play(Cue::PROMPT, requested_);
}

void FeedMonsterGame::replay_prompt() {
  ctx_->feedback.play(Cue::PROMPT, requested_);
  hint_ms_ = kHintMs;
}

void FeedMonsterGame::giggle() {
  if (extra_ < kMaxExtraTracked) ++extra_;
  giggle_ms_ = kGiggleMs;
  ctx_->feedback.play(Cue::REACTION, 0);  // happy giggle, never a buzzer
}

// Feed snacks until the monster has eaten `count` (counting along). Asking
// for more than requested simply completes the round; asking for fewer than
// already eaten just shows the count again.
void FeedMonsterGame::feed_to(uint8_t count) {
  const uint8_t target = count < requested_ ? count : requested_;
  if (target <= fed_) {
    hint_ms_ = kHintMs;  // tummy dots glow so the child sees the current count
    return;
  }
  while (fed_ < target) {
    ++fed_;
    ctx_->feedback.play(Cue::SUCCESS, fed_);  // "one!", "two!", "three!" + munch
  }
  chew_ms_ = kChewMs;
  hint_ms_ = 0;
  if (fed_ >= requested_) {
    ++rounds_;
    phase_ = Phase::CELEBRATE;
    phase_ms_ = 0;
    ctx_->feedback.play(Cue::CELEBRATE, 0);
  }
}

void FeedMonsterGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  chew_ms_ = saturating_sub(chew_ms_, dt_ms);
  giggle_ms_ = saturating_sub(giggle_ms_, dt_ms);
  hint_ms_ = saturating_sub(hint_ms_, dt_ms);
  phase_ms_ += dt_ms;
  if (phase_ == Phase::INTRO && phase_ms_ >= kIntroMs) {
    start_play();
  } else if (phase_ == Phase::CELEBRATE && phase_ms_ >= kCelebrateMs) {
    new_round();
    start_play();
  } else if (phase_ == Phase::PLAY) {
    idle_ms_ += dt_ms;
    if (!idle_hint_given_ && idle_ms_ >= kIdleHintMs) {
      idle_hint_given_ = true;  // one gentle reminder, no timeout or failure
      replay_prompt();
    }
  }
}

void FeedMonsterGame::handle(const InputEvent& event) {
  if (!ctx_) return;

  if (phase_ == Phase::INTRO) {
    start_play();  // any input skips the intro; it is not counted as a snack
    return;
  }

  if (phase_ == Phase::CELEBRATE) {
    // Extra snacks are welcome: the monster giggles, nothing else changes.
    if (event.source == Source::TOUCH && kFoodBowl.contains(event.x, event.y)) giggle();
    return;
  }

  idle_ms_ = 0;

  if (event.source == Source::TOUCH) {
    if (kFoodBowl.contains(event.x, event.y)) {
      ctx_->feedback.play(Cue::TAP, 0);
      feed_to(static_cast<uint8_t>(fed_ + 1));
      return;
    }
    for (std::size_t i = 0; i < kNumberButtonCount; ++i) {
      if (kNumberButtons[i].contains(event.x, event.y)) {
        ctx_->feedback.play(Cue::TAP, 0);
        feed_to(static_cast<uint8_t>(i + 1));
        return;
      }
    }
    if (kMonster.contains(event.x, event.y) || kRequestBadge.contains(event.x, event.y)) {
      replay_prompt();
    }
    return;
  }

  switch (event.intent) {
    case Intent::ONE:
      feed_to(1);
      break;
    case Intent::TWO:
      feed_to(2);
      break;
    case Intent::THREE:
      feed_to(3);
      break;
    case Intent::AGAIN:
    case Intent::HELP:
      replay_prompt();
      break;
    default:
      break;
  }
}

}  // namespace little_arcade::feed_monster
