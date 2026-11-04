#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

namespace little_arcade::hatch_a_pet {

// Original creatures (simple 2D shapes; see README for descriptions).
enum class Creature : uint8_t {
  BLOOP,       // round teal blob with one curly antenna
  FLUFFWING,   // peach puffball with two tiny wings
  SPROUTLING,  // green bean body with a leaf on top
  ZIGZAG,      // purple lizard-ish critter with a zigzag tail
  PUDDLEPUP,   // blue droplet puppy with floppy ears
  GLIMMERBUG,  // yellow beetle with a dotted shell
  ROLYPOLE,    // orange striped ball that rolls
  SNORKLE,     // pink critter with a long curly snout
};
inline constexpr std::size_t kCreatureCount = 8;

// Egg decoration, chosen independently so eggs look varied.
enum class EggPattern : uint8_t { SPOTS, STRIPES, ZIGZAGS, STARS };
inline constexpr std::size_t kEggPatternCount = 4;

// INTRO -> EGG (PLAY) -> HATCH (CELEBRATE) -> PET (PLAY) -> EGG ...
enum class Phase : uint8_t { INTRO, EGG, HATCH, PET };

// Variants for Cue::PROMPT.
enum class Prompt : uint8_t { TAP_THE_EGG = 0, ANOTHER_EGG = 1 };

// Pure scene logic; an LVGL view reads the accessors and renders. Owns no
// heap memory. All timing is driven by update(dt_ms).
class HatchAPetGame final : public IGame {
 public:
  static constexpr uint8_t kMinTaps = 3;
  static constexpr uint8_t kMaxTaps = 5;
  // Big egg in the middle of the play area (INTRO/EGG/HATCH).
  static constexpr Rect kEgg{104, 68, 112, 136};
  // Creature in the middle of the play area (PET).
  static constexpr Rect kCreature{96, 72, 128, 128};
  // PET choices: left = NO ("stay with my pet"), right = YES ("another egg").
  static constexpr Rect kNoButton{16, 136, 64, 64};
  static constexpr Rect kYesButton{240, 136, 64, 64};
  // Optional top-bar badge: the current prompt as a picture; tap = hear it again.
  static constexpr Rect kPromptBadge{132, 4, 56, 56};

  static constexpr uint32_t kIntroMs = 1200;
  static constexpr uint32_t kHatchMs = 1600;
  static constexpr uint32_t kWobbleMs = 300;
  static constexpr uint32_t kReactMs = 700;
  static constexpr uint32_t kHintMs = 1500;

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override;

  Phase phase() const { return phase_; }
  Creature creature() const { return creature_; }
  EggPattern egg_pattern() const { return pattern_; }
  uint8_t cracks() const { return cracks_; }
  uint8_t taps_needed() const { return taps_needed_; }
  uint32_t hatch_count() const { return hatches_; }
  uint32_t pet_count() const { return pets_; }
  Prompt current_prompt() const { return phase_ == Phase::PET ? Prompt::ANOTHER_EGG : Prompt::TAP_THE_EGG; }
  bool hint_active() const { return hint_ms_ > 0; }
  bool wobbling() const { return wobble_ms_ > 0; }
  bool reacting() const { return react_ms_ > 0; }
  bool active() const { return ctx_ != nullptr; }

 private:
  void new_egg();
  void start_egg_play();
  void crack();
  void pet();
  void replay_prompt();
  Creature draw_creature();

  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  Creature creature_ = Creature::BLOOP;
  EggPattern pattern_ = EggPattern::SPOTS;
  // Shuffle bag: every creature hatches once before any repeats.
  Creature bag_[kCreatureCount]{};
  std::size_t bag_next_ = kCreatureCount;
  bool have_previous_ = false;
  uint8_t cracks_ = 0;
  uint8_t taps_needed_ = kMinTaps;
  uint32_t hatches_ = 0;
  uint32_t pets_ = 0;
  uint32_t phase_ms_ = 0;
  uint32_t hint_ms_ = 0;
  uint32_t wobble_ms_ = 0;
  uint32_t react_ms_ = 0;
};

}  // namespace little_arcade::hatch_a_pet
