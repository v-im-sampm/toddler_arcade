#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/feedback.hpp"
#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"
#include "little_arcade/input.hpp"
#include "little_arcade/rng.hpp"

namespace little_arcade {

enum class GameId : uint8_t {
  COLOR_POP,
  HATCH_A_PET,
  RAINBOW_PIANO,
  ROBO_BUDDY,
  SHAPE_GARAGE,
  ANIMAL_SOUNDS,
  ROCKET_RESCUE,
  SILLY_FACES,
  FEED_MONSTER,
  BUBBLE_AQUARIUM,
  SOUND_JAR,
};

struct GameEntry {
  GameId id;
  Intent menu_intent;  // spoken launcher name, e.g. Intent::COLORS
  IGame* game;         // statically allocated; never owned by the manager
};

// Top-right push-to-talk button. Owned by the UI/speech glue, which starts a
// listening session; the SceneManager ignores taps here.
inline constexpr Rect kMicButton{kScreenWidth - 56, 0, 56, 56};

// Owns launcher state and navigation. Runs on the UI task only.
// Input priority: Home/BACK > PAUSE/RESUME > launcher or game actions.
class SceneManager {
 public:
  enum class Mode : uint8_t { MENU, PLAYING, PAUSED, SLEEPING };

  struct Config {
    uint32_t touch_debounce_ms = 80;
    uint32_t sleep_after_ms = 10u * 60u * 1000u;  // docs/parental-safety.md
    uint32_t base_seed = 0xC0FFEEu;
  };

  static constexpr std::size_t kTilesPerPage = 4;
  static constexpr Rect kTileRects[kTilesPerPage] = {
      {8, 68, 120, 80}, {136, 68, 120, 80}, {8, 156, 120, 80}, {136, 156, 120, 80}};
  static constexpr Rect kNextPageButton{264, 100, 48, 96};

  SceneManager(const GameEntry* entries, std::size_t count, IFeedback& feedback);
  SceneManager(const GameEntry* entries, std::size_t count, IFeedback& feedback, Config config);

  void handle(const InputEvent& event);
  void update(uint32_t dt_ms);

  void set_audio_enabled(bool enabled) { context_.audio_enabled = enabled; }
  void set_voice_enabled(bool enabled) { context_.voice_enabled = enabled; }

  Mode mode() const { return mode_; }
  const GameEntry* current() const { return current_; }
  std::size_t page() const { return page_; }
  std::size_t page_count() const { return (count_ + kTilesPerPage - 1) / kTilesPerPage; }
  const GameEntry* tile(std::size_t slot) const;
  // Contexts the voice filter should accept right now (0 while asleep).
  uint32_t active_contexts() const;

 private:
  void handle_menu(const InputEvent& event);
  void launch(const GameEntry& entry);
  void go_home();
  void sleep();
  void wake();
  bool accept_touch(const InputEvent& event);

  const GameEntry* entries_;
  std::size_t count_;
  Config config_;
  Rng rng_;
  GameContext context_;
  Mode mode_ = Mode::MENU;
  Mode mode_before_sleep_ = Mode::MENU;
  const GameEntry* current_ = nullptr;
  std::size_t page_ = 0;
  uint32_t idle_ms_ = 0;
  uint32_t launches_ = 0;
  uint32_t last_touch_ms_ = 0;
  bool have_touch_ = false;
};

}  // namespace little_arcade
