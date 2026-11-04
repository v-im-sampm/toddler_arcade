#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

namespace little_arcade::rocket_rescue {

// Each planet has its own surface pattern so planets are told apart without
// colour or audio (docs/acceptance.md accessibility).
enum class Look : uint8_t { RINGED, SPOTTED, STRIPED };
// Original friendly characters waiting on a planet to be picked up.
enum class Friend : uint8_t { MOON_PUP, STAR_BUNNY, COMET_KITTY };
inline constexpr std::size_t kPlanetCount = 3;

enum class Phase : uint8_t { INTRO, PLAY, CELEBRATE };
// DOCKED: resting on the launch pad or a planet. FLYING: gliding to the
// destination. HOVERING: stopped mid-flight; waits forever for GO. There is no
// crash, fuel or time-out state.
enum class RocketState : uint8_t { DOCKED, FLYING, HOVERING };

struct Point {
  int16_t x;
  int16_t y;
};

struct Planet {
  Rect rect;
  Look look;
  Friend buddy;
  bool rescued;
};

struct Star {
  uint8_t slot;         // index into kStarSlots
  bool visible;
  uint32_t respawn_ms;  // > 0 while waiting to twinkle back in
};

// Pure scene logic; an LVGL view reads the accessors and renders. Owns no
// heap memory. All timing is driven by update(dt_ms).
class RocketRescueGame final : public IGame {
 public:
  static constexpr Rect kPlanetRects[kPlanetCount] = {
      {16, 68, 56, 56}, {132, 68, 56, 56}, {248, 68, 56, 56}};
  static constexpr std::size_t kStarSlotCount = 5;
  static constexpr Rect kStarSlots[kStarSlotCount] = {
      {16, 132, 48, 48}, {80, 132, 48, 48}, {144, 132, 48, 48}, {208, 132, 48, 48}, {272, 132, 48, 48}};
  static constexpr std::size_t kStarCount = 3;
  static constexpr Rect kGoButton{16, 188, 96, 48};
  static constexpr Rect kStopButton{208, 188, 96, 48};
  // Shows the planet to visit next (its look + friend); tap = AGAIN.
  static constexpr Rect kPromptBadge{132, 4, 56, 56};
  // Launch pad between GO and STOP; drawn only, not a target.
  static constexpr Point kPadPoint{160, 212};

  static constexpr uint32_t kIntroMs = 1500;
  static constexpr uint32_t kTripMs = 2500;  // gentle glide, any distance
  static constexpr uint32_t kCelebrateMs = 1800;
  static constexpr uint32_t kHintMs = 1500;
  static constexpr uint32_t kWaveMs = 800;
  static constexpr uint32_t kStarRespawnMs = 900;

  // Cue variants (README cue table).
  static constexpr uint8_t kNoteTwinkle = 0;
  static constexpr uint8_t kNoteLaunch = 1;
  static constexpr uint8_t kNoteHover = 2;
  static constexpr uint8_t kCelebrateRescue = 0;
  static constexpr uint8_t kCelebrateMission = 1;

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override;

  Phase phase() const { return phase_; }
  bool active() const { return ctx_ != nullptr; }

  RocketState rocket_state() const { return rocket_; }
  Point rocket_position() const;
  // Planet the rocket rests on (-1 = launch pad or not docked).
  int docked_at() const { return rocket_ == RocketState::DOCKED ? docked_at_ : -1; }
  // Planet the rocket is flying/hovering towards (-1 when docked).
  int destination() const { return rocket_ == RocketState::DOCKED ? -1 : destination_; }
  uint32_t trip_progress_permille() const;

  const Planet& planet(std::size_t i) const { return planets_[i]; }
  // Next planet with a friend waiting (shown on the prompt badge), or -1.
  int suggested() const;
  std::size_t rescued_count() const;
  uint32_t rescues_total() const { return rescues_; }
  uint32_t missions_completed() const { return missions_; }
  bool hint_active() const { return hint_ms_ > 0; }
  // Planet whose friend is waving hello, or -1.
  int waving() const { return wave_ms_ > 0 ? wave_index_ : -1; }

  const Star& star(std::size_t i) const { return stars_[i]; }
  Rect star_rect(std::size_t i) const { return kStarSlots[stars_[i].slot]; }
  uint32_t stars_collected() const { return stars_collected_; }

 private:
  void new_mission();
  void start_play();
  void go();
  void stop();
  void fly_to(int planet);
  void arrive();
  void wave(int planet);
  void collect_star(std::size_t i);
  void respawn_star(std::size_t i);
  void replay_prompt();
  int planet_at(int16_t x, int16_t y) const;
  int star_at(int16_t x, int16_t y) const;
  static Point center_of(const Rect& r);

  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  uint32_t phase_ms_ = 0;

  Planet planets_[kPlanetCount]{};
  uint8_t order_[kPlanetCount] = {0, 1, 2};  // suggested visiting order

  RocketState rocket_ = RocketState::DOCKED;
  int docked_at_ = -1;
  int destination_ = -1;
  Point origin_{kPadPoint};
  uint32_t trip_ms_ = 0;

  Star stars_[kStarCount]{};
  uint32_t stars_collected_ = 0;

  uint32_t rescues_ = 0;
  uint32_t missions_ = 0;
  uint32_t hint_ms_ = 0;
  uint32_t wave_ms_ = 0;
  int wave_index_ = -1;
  bool mission_done_ = false;
};

}  // namespace little_arcade::rocket_rescue
