#include "little_arcade/games/animal_sounds.hpp"

namespace little_arcade::animal_sounds {

namespace {
uint32_t saturating_sub(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }
uint8_t variant_of(Animal a) { return static_cast<uint8_t>(a); }
}  // namespace

void AnimalSoundsGame::enter(GameContext& context) {
  ctx_ = &context;
  rounds_ = 0;
  have_previous_target_ = false;
  new_round();
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

void AnimalSoundsGame::exit() {
  ctx_ = nullptr;
  count_ = 0;
  hint_ms_ = 0;
  sound_ms_ = 0;
  wiggle_ms_ = 0;
  wiggle_index_ = -1;
  chosen_index_ = -1;
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

uint32_t AnimalSoundsGame::active_contexts() const {
  // No animal-specific context exists in config/commands.json; this scene only
  // uses Global intents (AGAIN, HELP), which the SceneManager always adds.
  return 0;
}

void AnimalSoundsGame::new_round() {
  // Grow gently from 2 to 4 cards; never shrinks back as a penalty.
  const std::size_t n = kMinCards + (rounds_ < kMaxCards - kMinCards ? rounds_ : kMaxCards - kMinCards);
  Animal animals[kAnimalCount] = {Animal::COW, Animal::DUCK, Animal::SHEEP, Animal::PIG,
                                  Animal::DOG, Animal::CAT,  Animal::FROG,  Animal::OWL};
  uint8_t slots[kSlotCount] = {0, 1, 2, 3, 4, 5};
  ctx_->rng.shuffle(animals);
  ctx_->rng.shuffle(slots);
  count_ = n;
  for (std::size_t i = 0; i < n; ++i) cards_[i] = Card{animals[i], kSlots[slots[i]]};

  std::size_t pick = ctx_->rng.below(static_cast<uint32_t>(n));
  if (have_previous_target_ && cards_[pick].animal == target_) pick = (pick + 1) % n;
  target_ = cards_[pick].animal;
  have_previous_target_ = true;
  hint_ms_ = 0;
  sound_ms_ = 0;
  wiggle_ms_ = 0;
  wiggle_index_ = -1;
  chosen_index_ = -1;
}

void AnimalSoundsGame::replay(bool with_hint) {
  sound_ms_ = kSoundShowMs;  // sound-wave animation + clue on the speaker button
  if (with_hint) hint_ms_ = kHintMs;
  ctx_->feedback.play(Cue::NOTE, variant_of(target_));
}

void AnimalSoundsGame::start_play() {
  phase_ = Phase::PLAY;
  phase_ms_ = 0;
  ctx_->feedback.play(Cue::PROMPT, kPromptWhoMadeThatSound);
  replay(false);
}

void AnimalSoundsGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  hint_ms_ = saturating_sub(hint_ms_, dt_ms);
  sound_ms_ = saturating_sub(sound_ms_, dt_ms);
  wiggle_ms_ = saturating_sub(wiggle_ms_, dt_ms);
  phase_ms_ += dt_ms;
  if (phase_ == Phase::INTRO && phase_ms_ >= kIntroMs) {
    start_play();
  } else if (phase_ == Phase::CELEBRATE && phase_ms_ >= kCelebrateMs) {
    new_round();
    start_play();
  }
}

int AnimalSoundsGame::target_index() const {
  for (std::size_t i = 0; i < count_; ++i) {
    if (cards_[i].animal == target_) return static_cast<int>(i);
  }
  return -1;
}

int AnimalSoundsGame::hit_test(int16_t x, int16_t y) const {
  for (std::size_t i = 0; i < count_; ++i) {
    if (cards_[i].rect.contains(x, y)) return static_cast<int>(i);
  }
  return -1;
}

void AnimalSoundsGame::choose(int index) {
  const Card& c = cards_[index];
  if (c.animal == target_) {
    ++rounds_;
    chosen_index_ = index;
    phase_ = Phase::CELEBRATE;
    phase_ms_ = 0;
    hint_ms_ = 0;
    sound_ms_ = 0;
    wiggle_ms_ = 0;
    ctx_->feedback.play(Cue::SUCCESS, variant_of(c.animal));
    ctx_->feedback.play(Cue::CELEBRATE, 0);
    return;
  }
  // Gentle nudge: the card wiggles, the right card glows, the sound replays.
  wiggle_index_ = index;
  wiggle_ms_ = kWiggleMs;
  ctx_->feedback.play(Cue::TRY_AGAIN, 0);
  replay(true);
}

void AnimalSoundsGame::handle(const InputEvent& event) {
  if (!ctx_ || phase_ == Phase::CELEBRATE) return;  // input during celebration is ignored

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
    } else if (kSoundButton.contains(event.x, event.y)) {
      replay(true);  // touch alternative to AGAIN / HELP
    }
    return;
  }

  switch (event.intent) {
    case Intent::AGAIN:
    case Intent::HELP:
      replay(true);
      break;
    default:
      break;
  }
}

}  // namespace little_arcade::animal_sounds
