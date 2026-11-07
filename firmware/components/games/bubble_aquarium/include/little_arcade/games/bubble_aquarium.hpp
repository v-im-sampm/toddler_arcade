#pragma once

#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

namespace little_arcade::bubble_aquarium {

enum class FishType : uint8_t { STRIPEY, PUFFY, LONGFIN };
inline constexpr std::size_t kFishTypeCount = 3;

enum class Visitor : uint8_t { TURTLE, SEAHORSE, JELLY };
inline constexpr std::size_t kVisitorCount = 3;

enum class Phase : uint8_t { INTRO, SWIM, PLAY = SWIM };
enum class FishMode : uint8_t { SWIM, FOOD, GULP, HAPPY, HELLO, FOLLOW, SLEEP };
enum class FoodMode : uint8_t { FALL, SINK, REST };

struct Fish {
  FishType type = FishType::STRIPEY;
  Rect rect{};
  int32_t x_q8 = 0;
  int32_t y_q8 = 0;
  int16_t vx = 0;
  int16_t vy = 0;
  int16_t target_x = 0;
  int16_t target_y = 0;
  int8_t face = 1;
  int8_t food_index = -1;
  FishMode mode = FishMode::SWIM;
  uint16_t wait_ms = 0;
  uint16_t mode_ms = 0;
  uint16_t peek_ms = 0;
  bool nap_after_meal = false;
  uint32_t happy_ms = 0;
  uint32_t munch_ms = 0;
  uint32_t meals = 0;
};

struct Bubble {
  int16_t x = 0;
  int16_t y = 0;
  int32_t x_q8 = 0;
  int32_t y_q8 = 0;
  int16_t drift = 0;
  uint8_t size = 0;
  bool active = false;
  uint32_t age_ms = 0;
  uint32_t serial = 0;
};

struct Food {
  int16_t x = 0;
  int16_t y = 0;
  int32_t x_q8 = 0;
  int32_t y_q8 = 0;
  int16_t vy = 0;
  int8_t eater = -1;
  FoodMode mode = FoodMode::FALL;
  bool active = false;
  uint32_t age_ms = 0;
};

class BubbleAquariumGame final : public IGame {
 public:
  static constexpr std::size_t kFishCount = kFishTypeCount;
  static constexpr std::size_t kMaxBubbles = 8;
  static constexpr std::size_t kMaxFood = 6;
  static constexpr uint32_t kFeedsPerVisitor = 3;

  static constexpr Rect kTank = kPlayArea;
  static constexpr Rect kFoodButton{64, 0, 56, 56};
  static constexpr Rect kBedtimeButton{128, 0, 56, 56};
  static constexpr Rect kBubbleButton{192, 0, 56, 56};
  static constexpr Rect kChestHit{190, 186, 60, 48};
  static constexpr int16_t kSpoutX = 100;
  static constexpr int16_t kSpoutY = 50;
  static constexpr int16_t kChestX = 220;
  static constexpr int16_t kChestY = 228;

  static constexpr int16_t kFishW[kFishTypeCount] = {64, 64, 64};
  static constexpr int16_t kFishH[kFishTypeCount] = {48, 48, 48};
  static constexpr int16_t kVisitorW = 72;
  static constexpr int16_t kVisitorH = 64;
  static constexpr int16_t kVisitorY = 126;
  static constexpr int16_t kBubbleRadiusMin = 18;
  static constexpr int16_t kBubbleRadiusMax = 22;
  static constexpr int16_t kBubbleHit = kBubbleRadiusMax + 6;

  static constexpr uint32_t kIntroMs = 1500;
  static constexpr uint32_t kStepMs = 50;
  static constexpr uint32_t kMaxPhysicsMs = 1000;
  static constexpr uint32_t kHappyMs = 1600;
  static constexpr uint32_t kGulpMs = 300;
  static constexpr uint32_t kBubblePulseMs = 300;
  static constexpr uint32_t kHintVisualMs = 6000;
  static constexpr uint32_t kHintSpokenMs = 12000;
  static constexpr int16_t kBubbleRise = 34;
  static constexpr int16_t kFoodRestY = 211;

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override;

  Phase phase() const { return phase_; }
  bool active() const { return ctx_ != nullptr; }
  bool bedtime() const { return bedtime_; }
  uint8_t bedtime_dim() const { return bedtime_dim_; }
  uint8_t hint_level() const { return hint_level_; }

  const Fish& fish(std::size_t i) const { return fish_[i]; }
  const Bubble& bubble(std::size_t i) const { return bubbles_[i]; }
  const Food& food(std::size_t i) const { return food_[i]; }
  std::size_t active_bubbles() const;
  std::size_t active_food() const;

  uint32_t meals_eaten() const { return meals_; }
  uint32_t visitors_seen() const { return visitors_seen_; }
  Visitor visitor() const { return visitor_; }
  bool visitor_visible() const { return visitor_active_; }
  Rect visitor_rect() const;
  bool food_button_pulsing() const { return food_pulse_ms_ > 0; }
  bool bedtime_button_pulsing() const { return bedtime_pulse_ms_ > 0; }
  bool bubble_button_pulsing() const { return bubble_pulse_ms_ > 0; }
  bool chest_open() const { return chest_open_ms_ > 0; }

 private:
  void start_play();
  void set_bedtime(bool enabled);
  void bubble_burst();
  void drop_food();
  void spawn_bubble();
  void start_visitor();
  void step();
  void step_fish(Fish& fish, std::size_t index);
  void step_food();
  void step_bubbles();
  void step_visitor();
  void claim_food();
  void eat_food(std::size_t fish_index, std::size_t food_index);
  void pick_target(Fish& fish, std::size_t index);
  void move_toward(Fish& fish, int16_t x, int16_t y, int16_t speed);
  int hit_fish(int16_t x, int16_t y) const;
  int hit_bubble(int16_t x, int16_t y) const;
  int hit_visitor(int16_t x, int16_t y) const;
  int nearest_fish(int16_t x, int16_t y) const;
  void request_help();

  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  Fish fish_[kFishCount]{};
  Bubble bubbles_[kMaxBubbles]{};
  Food food_[kMaxFood]{};

  uint32_t bubble_serial_ = 0;
  uint32_t meals_ = 0;
  uint32_t visitors_seen_ = 0;
  Visitor visitor_ = Visitor::TURTLE;
  uint32_t intro_ms_ = 0;
  uint32_t sim_accumulator_ms_ = 0;
  uint32_t sim_time_ms_ = 0;
  uint32_t idle_ms_ = 0;
  uint32_t next_hint_wiggle_ms_ = 0;
  uint32_t bubble_spawn_ms_ = 0;
  uint32_t chest_burp_ms_ = 6000;
  uint32_t chest_open_ms_ = 0;
  uint32_t food_pulse_ms_ = 0;
  uint32_t bedtime_pulse_ms_ = 0;
  uint32_t bubble_pulse_ms_ = 0;
  int32_t visitor_x_q8_ = 0;
  uint16_t visitor_age_ms_ = 0;
  int16_t visitor_y_ = kVisitorY;
  int8_t visitor_direction_ = 1;
  uint8_t visitor_next_ = 0;
  uint8_t pending_bubbles_ = 0;
  uint8_t pending_visitors_ = 0;
  uint8_t hint_level_ = 0;
  uint8_t bedtime_dim_ = 0;
  uint16_t bedtime_fade_ms_ = 0;
  bool visitor_active_ = false;
  bool visitor_waved_ = false;
  bool bedtime_ = false;
  bool hint_announced_ = false;
  bool ambient_started_ = false;
};

}  // namespace little_arcade::bubble_aquarium
