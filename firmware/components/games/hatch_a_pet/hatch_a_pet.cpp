#include "little_arcade/games/hatch_a_pet.hpp"

namespace little_arcade::hatch_a_pet {

namespace {
uint32_t saturating_sub(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }
}  // namespace

void HatchAPetGame::enter(GameContext& context) {
  ctx_ = &context;
  hatches_ = 0;
  pets_ = 0;
  bag_next_ = kCreatureCount;
  have_previous_ = false;
  new_egg();
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

void HatchAPetGame::exit() {
  ctx_ = nullptr;
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
  cracks_ = 0;
  hint_ms_ = 0;
  wobble_ms_ = 0;
  react_ms_ = 0;
}

uint32_t HatchAPetGame::active_contexts() const {
  return static_cast<uint32_t>(Context::General);  // YES / NO
}

Creature HatchAPetGame::draw_creature() {
  if (bag_next_ >= kCreatureCount) {
    for (std::size_t i = 0; i < kCreatureCount; ++i) bag_[i] = static_cast<Creature>(i);
    ctx_->rng.shuffle(bag_);
    // Never hatch the same creature twice in a row across a bag refill.
    if (have_previous_ && bag_[0] == creature_) {
      bag_[0] = bag_[kCreatureCount - 1];
      bag_[kCreatureCount - 1] = creature_;
    }
    bag_next_ = 0;
  }
  have_previous_ = true;
  return bag_[bag_next_++];
}

void HatchAPetGame::new_egg() {
  creature_ = draw_creature();
  pattern_ = static_cast<EggPattern>(ctx_->rng.below(static_cast<uint32_t>(kEggPatternCount)));
  taps_needed_ = static_cast<uint8_t>(kMinTaps + ctx_->rng.below(kMaxTaps - kMinTaps + 1u));
  cracks_ = 0;
  hint_ms_ = 0;
  wobble_ms_ = 0;
  react_ms_ = 0;
}

void HatchAPetGame::start_egg_play() {
  phase_ = Phase::EGG;
  phase_ms_ = 0;
  ctx_->feedback.play(Cue::PROMPT, static_cast<uint8_t>(Prompt::TAP_THE_EGG));
}

void HatchAPetGame::replay_prompt() {
  ctx_->feedback.play(Cue::PROMPT, static_cast<uint8_t>(current_prompt()));
  hint_ms_ = kHintMs;
}

void HatchAPetGame::crack() {
  ctx_->feedback.play(Cue::TAP, 0);
  ++cracks_;
  wobble_ms_ = kWobbleMs;
  hint_ms_ = 0;
  // Rising pitched "tok" per crack: variant 1..5.
  ctx_->feedback.play(Cue::NOTE, cracks_);
  if (cracks_ >= taps_needed_) {
    phase_ = Phase::HATCH;
    phase_ms_ = 0;
    ++hatches_;
    ctx_->feedback.play(Cue::SUCCESS, static_cast<uint8_t>(creature_));
    ctx_->feedback.play(Cue::CELEBRATE, 0);
  }
}

void HatchAPetGame::pet() {
  ++pets_;
  react_ms_ = kReactMs;
  ctx_->feedback.play(Cue::REACTION, static_cast<uint8_t>(creature_));
}

void HatchAPetGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  hint_ms_ = saturating_sub(hint_ms_, dt_ms);
  wobble_ms_ = saturating_sub(wobble_ms_, dt_ms);
  react_ms_ = saturating_sub(react_ms_, dt_ms);
  phase_ms_ += dt_ms;
  if (phase_ == Phase::INTRO && phase_ms_ >= kIntroMs) {
    start_egg_play();
  } else if (phase_ == Phase::HATCH && phase_ms_ >= kHatchMs) {
    phase_ = Phase::PET;
    phase_ms_ = 0;
    react_ms_ = kReactMs;  // creature says hello (visual + sound)
    ctx_->feedback.play(Cue::REACTION, static_cast<uint8_t>(creature_));
    ctx_->feedback.play(Cue::PROMPT, static_cast<uint8_t>(Prompt::ANOTHER_EGG));
  }
}

void HatchAPetGame::handle(const InputEvent& event) {
  if (!ctx_ || phase_ == Phase::HATCH) return;  // input during the hatch animation is ignored

  if (phase_ == Phase::INTRO) {
    // Any input skips the intro; it is not counted as a crack.
    start_egg_play();
    return;
  }

  if (event.source == Source::TOUCH) {
    if (kPromptBadge.contains(event.x, event.y)) {
      replay_prompt();  // touch alternative to AGAIN/HELP
      return;
    }
    if (!kPlayArea.contains(event.x, event.y)) return;
    if (phase_ == Phase::EGG) {
      crack();  // every tap in the play area cracks the egg (touch for YES)
    } else if (kYesButton.contains(event.x, event.y)) {
      new_egg();  // touch for YES / AGAIN
      start_egg_play();
    } else {
      pet();  // creature, NO button, or anywhere else: the pet reacts (touch for NO)
    }
    return;
  }

  if (phase_ == Phase::EGG) {
    switch (event.intent) {
      case Intent::YES:
        crack();
        break;
      case Intent::AGAIN:
      case Intent::HELP:
        replay_prompt();
        break;
      default:
        break;  // NO / NEXT: nothing to undo, the egg just waits
    }
    return;
  }

  // PET
  switch (event.intent) {
    case Intent::YES:
    case Intent::AGAIN:
    case Intent::NEXT:
      new_egg();
      start_egg_play();
      break;
    case Intent::NO:
      pet();  // stay with this pet
      break;
    case Intent::HELP:
      replay_prompt();
      break;
    default:
      break;
  }
}

}  // namespace little_arcade::hatch_a_pet
