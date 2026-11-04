#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

namespace little_arcade::bubble_aquarium {

// Three original fish designs. Each has its own body shape and size, so
// colour is never the only distinguishing feature (docs/acceptance.md).
enum class FishType : uint8_t {
  STRIPEY,  // small oval fish with vertical stripes
  PUFFY,    // round, spotty puff fish
  LONGFIN,  // long fish with flowing tail fins
};
inline constexpr std::size_t kFishTypeCount = 3;

// Surprise visitors that swim across during CELEBRATE.
enum class Visitor : uint8_t { TURTLE, SEAHORSE, JELLY };
inline constexpr std::size_t kVisitorCount = 3;

enum class Phase : uint8_t { INTRO, PLAY, CELEBRATE };

struct Fish {
  FishType type;
  Rect rect;          // touch target and sprite box; always inside kTank
  int32_t x_q8;       // sub-pixel position of rect.x / rect.y (Q8)
  int32_t y_q8;
  int16_t wander_vx;  // calm swimming velocity (px/s) when there is no food
  int16_t wander_vy;
  uint32_t happy_ms;  // visual wiggle after a tap
  uint32_t munch_ms;  // visual "yum" glow after eating
  uint32_t meals;
};

struct Bubble {
  int16_t x;  // centre
  int16_t y;
  int32_t y_q8;
  uint8_t size;  // drawn radius in px (visual only)
  bool active;
  uint32_t serial;  // spawn order; the oldest is recycled when the pool is full
};

struct Food {
  int16_t x;  // centre
  int16_t y;
  int32_t y_q8;
  bool active;
};

// Pure scene logic; an LVGL view reads the accessors and renders. Owns no
// heap memory: fish, bubbles and food live in fixed-size pools that never
// grow. All timing is driven by update(dt_ms). There is no fail condition.
class BubbleAquariumGame final : public IGame {
 public:
  static constexpr std::size_t kFishCount = kFishTypeCount;  // one of each type
  static constexpr std::size_t kMaxBubbles = 8;
  static constexpr std::size_t kMaxFood = 6;
  static constexpr std::size_t kBurstBubbles = 3;
  static constexpr uint32_t kFeedsPerVisitor = 3;

  // Water area: tap anywhere to blow a bubble. Fish swim only in here.
  static constexpr Rect kTank = kPlayArea;
  // Optional top-bar controls (between Home and Mic, x in [64, 256]).
  static constexpr Rect kFoodButton{72, 0, 56, 56};
  static constexpr Rect kBubbleButton{192, 0, 56, 56};
  // Treasure-chest bubble maker that bursts bubbles (visual origin only).
  static constexpr int16_t kChestX = 160;
  static constexpr int16_t kChestY = 224;

  static constexpr int16_t kFishW[kFishTypeCount] = {64, 56, 80};
  static constexpr int16_t kFishH[kFishTypeCount] = {48, 56, 48};
  static constexpr int16_t kVisitorW = 72;
  static constexpr int16_t kVisitorH = 64;
  static constexpr int16_t kVisitorY = 120;
  static constexpr int16_t kBubbleHit = 24;  // half-size of a 48x48 pop box

  static constexpr uint32_t kIntroMs = 1500;
  static constexpr uint32_t kCelebrateMs = 3000;
  static constexpr uint32_t kHappyMs = 600;
  static constexpr uint32_t kMunchMs = 600;
  static constexpr uint32_t kPulseMs = 300;
  static constexpr uint32_t kStepMs = 50;          // physics sub-step
  static constexpr uint32_t kMaxPhysicsMs = 1000;  // physics time simulated per update()
  static constexpr int16_t kBubbleRise = 40;       // px/s
  static constexpr int16_t kFoodSink = 30;         // px/s
  static constexpr int16_t kChaseSpeed = 70;       // px/s
  static constexpr int16_t kFoodTopY = 72;
  static constexpr int16_t kFoodRestY = 228;

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override;

  Phase phase() const { return phase_; }
  bool active() const { return ctx_ != nullptr; }

  const Fish& fish(std::size_t i) const { return fish_[i]; }
  const Bubble& bubble(std::size_t i) const { return bubbles_[i]; }
  const Food& food(std::size_t i) const { return food_[i]; }
  std::size_t active_bubbles() const;
  std::size_t active_food() const;

  uint32_t meals_eaten() const { return meals_; }
  uint32_t visitors_seen() const { return visitors_seen_; }
  Visitor visitor() const { return visitor_; }
  bool visitor_visible() const { return phase_ == Phase::CELEBRATE; }
  // Visitor sprite box; slides from off-screen left to off-screen right.
  Rect visitor_rect() const;
  bool food_button_pulsing() const { return food_pulse_ms_ > 0; }
  bool bubble_button_pulsing() const { return bubble_pulse_ms_ > 0; }

 private:
  void start_play();
  void spawn_bubble(int16_t x, int16_t y);
  void bubble_burst();
  void drop_food();
  void tap_water(int16_t x, int16_t y);
  void step(uint32_t dt_ms);
  void step_fish(Fish& f, uint32_t dt_ms);
  void eat_check();
  void start_celebrate();
  int hit_fish(int16_t x, int16_t y) const;
  int hit_bubble(int16_t x, int16_t y) const;
  int nearest_food(const Fish& f) const;

  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  Fish fish_[kFishCount]{};
  Bubble bubbles_[kMaxBubbles]{};
  Food food_[kMaxFood]{};
  uint32_t bubble_serial_ = 0;
  uint32_t meals_ = 0;
  uint32_t feeds_since_visitor_ = 0;
  uint32_t visitors_seen_ = 0;
  Visitor visitor_ = Visitor::TURTLE;
  bool have_visitor_ = false;
  uint32_t phase_ms_ = 0;
  uint32_t food_pulse_ms_ = 0;
  uint32_t bubble_pulse_ms_ = 0;
  bool ambient_started_ = false;
};

}  // namespace little_arcade::bubble_aquarium
