#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

namespace little_arcade::animal_sounds {

// Original, simple cartoon animals (no licensed characters).
enum class Animal : uint8_t { COW, DUCK, SHEEP, PIG, DOG, CAT, FROG, OWL };
inline constexpr std::size_t kAnimalCount = 8;

// Visual clue shown on the sound button while the sound "plays", so the game
// stays playable with audio disabled (docs/acceptance.md). One per animal.
enum class Clue : uint8_t { HOOFPRINT, WEBBED_FOOT, WOOL_CLOUD, CURLY_TAIL, BONE, WHISKERS, LILY_PAD, FEATHER };
constexpr Clue clue_for(Animal a) { return static_cast<Clue>(static_cast<uint8_t>(a)); }

enum class Phase : uint8_t { INTRO, PLAY, CELEBRATE };

struct Card {
  Animal animal;
  Rect rect;
};

// Pure scene logic; an LVGL view reads the accessors and renders. Owns no
// heap memory. All timing is driven by update(dt_ms). Audio never blocks:
// sound_playing() is only a visual "sound waves" timer.
class AnimalSoundsGame final : public IGame {
 public:
  static constexpr std::size_t kMinCards = 2;
  static constexpr std::size_t kMaxCards = 4;
  static constexpr std::size_t kSlotCount = 6;
  static constexpr Rect kSlots[kSlotCount] = {
      {8, 68, 96, 80},  {112, 68, 96, 80},  {216, 68, 96, 80},
      {8, 156, 96, 80}, {112, 156, 96, 80}, {216, 156, 96, 80},
  };
  // Big speaker button (top bar): touch alternative to AGAIN/HELP. Shows the
  // target's clue while the sound plays.
  static constexpr Rect kSoundButton{132, 4, 56, 56};
  static constexpr uint8_t kPromptWhoMadeThatSound = 0;
  static constexpr uint32_t kIntroMs = 1500;
  static constexpr uint32_t kCelebrateMs = 1800;
  static constexpr uint32_t kSoundShowMs = 1200;
  static constexpr uint32_t kWiggleMs = 400;
  static constexpr uint32_t kHintMs = 1500;

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override;

  Phase phase() const { return phase_; }
  std::size_t card_count() const { return count_; }
  const Card& card(std::size_t i) const { return cards_[i]; }
  Animal target() const { return target_; }
  int target_index() const;
  uint32_t rounds_completed() const { return rounds_; }
  bool hint_active() const { return hint_ms_ > 0; }
  bool sound_playing() const { return sound_ms_ > 0; }
  // Index of the card doing a gentle "not me" wiggle, or -1.
  int wiggling() const { return wiggle_ms_ > 0 ? wiggle_index_ : -1; }
  // Index of the card bouncing during CELEBRATE, or -1.
  int celebrating() const { return phase_ == Phase::CELEBRATE ? chosen_index_ : -1; }
  bool active() const { return ctx_ != nullptr; }

 private:
  void new_round();
  void start_play();
  void replay(bool with_hint);
  void choose(int index);
  int hit_test(int16_t x, int16_t y) const;

  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  Card cards_[kMaxCards]{};
  std::size_t count_ = 0;
  Animal target_ = Animal::COW;
  bool have_previous_target_ = false;
  uint32_t rounds_ = 0;
  uint32_t phase_ms_ = 0;
  uint32_t hint_ms_ = 0;
  uint32_t sound_ms_ = 0;
  uint32_t wiggle_ms_ = 0;
  int wiggle_index_ = -1;
  int chosen_index_ = -1;
};

}  // namespace little_arcade::animal_sounds
