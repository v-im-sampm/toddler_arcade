#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

namespace little_arcade::robo_buddy {

// Original design: "Bolt", a round teal robot with a single bendy antenna,
// two big round screen-eyes, a belly dial and stubby spring arms.
//
// Moods drive the face drawn on the robot's screen-eyes. Each mood has a
// distinct face shape (not only colour), so it reads without audio.
enum class Mood : uint8_t {
  CALM,    // soft round eyes, small smile
  HAPPY,   // crescent eyes, big smile, antenna bobs (pet head)
  SILLY,   // one wink + tongue, body wobble (tickle belly)
  YUMMY,   // heart eyes, cheeks glow, munching mouth (feed snack)
  SLEEPY,  // closed line eyes, floating "Z" bubbles (sleep)
};
inline constexpr std::size_t kMoodCount = 5;

enum class Action : uint8_t { NONE, PET, TICKLE, FEED, SLEEP, WAKE };

// Snack shown on the food button; changes after every feed.
enum class Snack : uint8_t { BOLT_COOKIE, BATTERY_BAR, GEAR_GUMMY };
inline constexpr std::size_t kSnackCount = 3;

// In-game nap is the SLEEP phase; it is unrelated to the SceneManager's
// system sleep.
enum class Phase : uint8_t { INTRO, PLAY, CELEBRATE, SLEEP };

// Pure scene logic; an LVGL view reads the accessors and renders. Owns no
// heap memory. All timing is driven by update(dt_ms).
class RoboBuddyGame final : public IGame {
 public:
  // Robot body parts are the main touch targets.
  static constexpr Rect kHead{104, 64, 112, 72};
  static constexpr Rect kBelly{104, 144, 112, 88};
  // Large icon buttons either side of the robot.
  static constexpr Rect kFoodButton{12, 128, 80, 80};
  static constexpr Rect kSleepButton{228, 128, 80, 80};
  // Top-bar "wave hello" button: touch alternative to HELP (replays the
  // prompt and pulses the action icons).
  static constexpr Rect kHelloButton{132, 0, 56, 56};
  static constexpr std::size_t kTargetCount = 5;
  static constexpr Rect kTargets[kTargetCount] = {kHead, kBelly, kFoodButton, kSleepButton,
                                                  kHelloButton};

  static constexpr uint8_t kPoseCount = 3;        // animation variants per reaction
  static constexpr uint32_t kDanceEvery = 5;      // every 5th reaction is a dance
  static constexpr uint32_t kIntroMs = 1500;
  static constexpr uint32_t kReactMs = 1200;
  static constexpr uint32_t kDanceMs = 2000;
  static constexpr uint32_t kDozeMs = 800;        // yawn; input ignored meanwhile
  static constexpr uint32_t kHintMs = 1500;
  static constexpr uint32_t kCalmAfterMs = 4000;  // mood relaxes to CALM when idle

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override;

  Phase phase() const { return phase_; }
  Mood mood() const { return mood_; }
  Action last_action() const { return last_action_; }
  uint8_t pose() const { return pose_; }
  Snack snack() const { return snack_; }
  bool sleeping() const { return phase_ == Phase::SLEEP; }
  bool dozing() const { return phase_ == Phase::SLEEP && phase_ms_ < kDozeMs; }
  bool dancing() const { return phase_ == Phase::CELEBRATE && dancing_; }
  bool hint_active() const { return hint_ms_ > 0; }
  uint32_t reactions() const { return reactions_; }
  uint32_t naps() const { return naps_; }
  // Bit per Mood value that has been shown since enter().
  uint32_t moods_seen_mask() const { return moods_seen_; }
  bool active() const { return ctx_ != nullptr; }

 private:
  void reset_state();
  void start_play();
  void set_mood(Mood m);
  void perform(Action action);
  void react(Action action);
  void go_to_sleep();
  void wake_up();
  void show_hint();
  Action hit_test(int16_t x, int16_t y) const;

  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  Mood mood_ = Mood::CALM;
  Action last_action_ = Action::NONE;
  uint8_t pose_ = 0;
  Snack snack_ = Snack::BOLT_COOKIE;
  bool dancing_ = false;
  uint32_t reactions_ = 0;
  uint32_t naps_ = 0;
  uint32_t moods_seen_ = 0;
  uint32_t phase_ms_ = 0;
  uint32_t hint_ms_ = 0;
  uint32_t idle_ms_ = 0;
};

}  // namespace little_arcade::robo_buddy
