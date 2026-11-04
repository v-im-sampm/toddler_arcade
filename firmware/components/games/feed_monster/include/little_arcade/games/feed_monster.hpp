#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

namespace little_arcade::feed_monster {

enum class Phase : uint8_t { INTRO, PLAY, CELEBRATE };

// Pure scene logic for "Feed the Monster" (G09). An LVGL view reads the
// accessors and renders an original, friendly round monster with a snack
// bowl. Owns no heap memory; all timing is driven by update(dt_ms).
//
// Rules: the monster asks for 1-3 snacks (request badge shows N dots). Each
// snack fed lights one dot on the monster's tummy and is counted aloud. When
// the requested count is reached the monster celebrates. Extra snacks are
// never penalised: they just make the monster giggle.
class FeedMonsterGame final : public IGame {
 public:
  static constexpr uint8_t kMinRequest = 1;
  static constexpr uint8_t kMaxRequest = 3;

  // Tap to feed one snack.
  static constexpr Rect kFoodBowl{8, 72, 112, 96};
  // Tap the monster to hear/see the request again (touch alternative to AGAIN/HELP).
  static constexpr Rect kMonster{136, 72, 176, 96};
  // Count-along buttons (1, 2, 3 dots): touch alternatives to ONE/TWO/THREE.
  static constexpr std::size_t kNumberButtonCount = 3;
  static constexpr Rect kNumberButtons[kNumberButtonCount] = {
      {8, 176, 96, 56}, {112, 176, 96, 56}, {216, 176, 96, 56}};
  // Top-bar badge showing N dots (works with audio off); tap also replays the request.
  static constexpr Rect kRequestBadge{132, 4, 56, 56};

  static constexpr uint32_t kIntroMs = 1200;
  static constexpr uint32_t kCelebrateMs = 1800;
  static constexpr uint32_t kChewMs = 350;
  static constexpr uint32_t kGiggleMs = 500;
  static constexpr uint32_t kHintMs = 1500;
  static constexpr uint32_t kIdleHintMs = 6000;

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override;

  Phase phase() const { return phase_; }
  uint8_t requested() const { return requested_; }
  uint8_t fed() const { return fed_; }
  uint32_t rounds_completed() const { return rounds_; }
  // Snacks offered beyond the request this round (giggles, never a penalty).
  uint32_t extra_snacks() const { return extra_; }
  bool chewing() const { return chew_ms_ > 0; }
  bool giggling() const { return giggle_ms_ > 0; }
  bool hint_active() const { return hint_ms_ > 0; }
  bool active() const { return ctx_ != nullptr; }

 private:
  void new_round();
  void start_play();
  void replay_prompt();
  void feed_to(uint8_t count);
  void giggle();

  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  uint8_t requested_ = kMinRequest;
  uint8_t fed_ = 0;
  bool have_previous_ = false;
  uint32_t rounds_ = 0;
  uint32_t extra_ = 0;
  uint32_t phase_ms_ = 0;
  uint32_t idle_ms_ = 0;
  uint32_t chew_ms_ = 0;
  uint32_t giggle_ms_ = 0;
  uint32_t hint_ms_ = 0;
  bool idle_hint_given_ = false;
};

}  // namespace little_arcade::feed_monster
