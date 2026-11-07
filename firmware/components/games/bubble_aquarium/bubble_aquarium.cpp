#include "little_arcade/games/bubble_aquarium.hpp"

namespace little_arcade::bubble_aquarium {
namespace {
uint32_t saturating_sub(uint32_t value, uint32_t amount) {
  return value > amount ? value - amount : 0;
}

int32_t travel_q8(int32_t speed, uint32_t dt_ms) {
  return speed * static_cast<int32_t>(dt_ms) * 256 / 1000;
}

int16_t to_px(int32_t q8) { return static_cast<int16_t>(q8 / 256); }

int32_t abs32(int32_t value) { return value < 0 ? -value : value; }

int32_t clamp32(int32_t value, int32_t lo, int32_t hi) {
  return value < lo ? lo : (value > hi ? hi : value);
}

int32_t distance2(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
  const int32_t dx = x1 - x2;
  const int32_t dy = y1 - y2;
  return dx * dx + dy * dy;
}

constexpr uint8_t kNotePop = 3;
constexpr uint8_t kNoteBurst = 4;
constexpr int16_t kWaterSurface = 62;
constexpr int16_t kFishMinX = 40;
constexpr int16_t kFishMaxX = 280;
constexpr int16_t kFishMinY = 88;
constexpr int16_t kFishMaxY = 196;
constexpr int16_t kVisitorSpeed = 44;
constexpr uint32_t kVisitorMeals = 3;
constexpr uint32_t kVisitorTravelMs = 8000;
constexpr int16_t kLaneTop[kFishTypeCount] = {96, 124, 92};
constexpr int16_t kLaneBottom[kFishTypeCount] = {160, 186, 150};
constexpr int16_t kHomeX[kFishTypeCount] = {92, 222, 176};
constexpr int16_t kHomeY[kFishTypeCount] = {122, 158, 104};
constexpr int16_t kSwimSpeed[kFishTypeCount] = {20, 16, 24};
constexpr int8_t kVisitorDirection[kVisitorCount] = {1, -1, 1};
}  // namespace

void BubbleAquariumGame::enter(GameContext& context) {
  ctx_ = &context;
  phase_ = Phase::INTRO;
  bubble_serial_ = meals_ = visitors_seen_ = 0;
  intro_ms_ = sim_accumulator_ms_ = sim_time_ms_ = idle_ms_ = 0;
  next_hint_wiggle_ms_ = bubble_spawn_ms_ = chest_open_ms_ = 0;
  food_pulse_ms_ = bedtime_pulse_ms_ = bubble_pulse_ms_ = 0;
  chest_burp_ms_ = 6000;
  visitor_x_q8_ = 0;
  visitor_age_ms_ = 0;
  visitor_y_ = kVisitorY;
  visitor_direction_ = 1;
  visitor_next_ = pending_bubbles_ = pending_visitors_ = hint_level_ = bedtime_dim_ = 0;
  bedtime_fade_ms_ = 0;
  visitor_active_ = visitor_waved_ = bedtime_ = hint_announced_ = ambient_started_ = false;
  visitor_ = Visitor::TURTLE;
  for (auto& bubble : bubbles_) bubble = Bubble{};
  for (auto& pellet : food_) pellet = Food{};

  for (std::size_t i = 0; i < kFishCount; ++i) {
    Fish& fish = fish_[i];
    fish = Fish{};
    fish.type = static_cast<FishType>(i);
    fish.x_q8 = static_cast<int32_t>(kHomeX[i]) * 256;
    fish.y_q8 = static_cast<int32_t>(kHomeY[i]) * 256;
    fish.rect = Rect{static_cast<int16_t>(kHomeX[i] - kFishW[i] / 2),
                     static_cast<int16_t>(kHomeY[i] - kFishH[i] / 2), kFishW[i], kFishH[i]};
    fish.face = i == 1 ? -1 : 1;
    fish.target_x = kHomeX[i];
    fish.target_y = kHomeY[i];
    fish.wait_ms = static_cast<uint16_t>(800 + i * 500);
    fish.mode = FishMode::SWIM;
  }
}

void BubbleAquariumGame::exit() {
  ctx_ = nullptr;
  phase_ = Phase::INTRO;
  visitor_active_ = bedtime_ = false;
  pending_bubbles_ = pending_visitors_ = 0;
  for (auto& bubble : bubbles_) bubble = Bubble{};
  for (auto& pellet : food_) pellet = Food{};
  food_pulse_ms_ = bedtime_pulse_ms_ = bubble_pulse_ms_ = 0;
}

uint32_t BubbleAquariumGame::active_contexts() const {
  return 0;
}

std::size_t BubbleAquariumGame::active_bubbles() const {
  std::size_t count = 0;
  for (const auto& bubble : bubbles_) count += bubble.active ? 1 : 0;
  return count;
}

std::size_t BubbleAquariumGame::active_food() const {
  std::size_t count = 0;
  for (const auto& pellet : food_) count += pellet.active ? 1 : 0;
  return count;
}

Rect BubbleAquariumGame::visitor_rect() const {
  const int16_t center_x = to_px(visitor_x_q8_);
  return Rect{static_cast<int16_t>(center_x - kVisitorW / 2),
              static_cast<int16_t>(visitor_y_ - kVisitorH / 2), kVisitorW, kVisitorH};
}

void BubbleAquariumGame::start_play() {
  if (phase_ == Phase::SWIM) return;
  phase_ = Phase::SWIM;
  intro_ms_ = 0;
  idle_ms_ = 0;
  ctx_->feedback.play(Cue::PROMPT, 0);
  if (!ambient_started_) {
    ctx_->feedback.play(Cue::AMBIENT, 0);
    ambient_started_ = true;
  }
  pending_bubbles_ = 3;
  bubble_spawn_ms_ = 300;
}

void BubbleAquariumGame::set_bedtime(bool enabled) {
  if (bedtime_ == enabled) return;
  bedtime_ = enabled;
  bedtime_pulse_ms_ = kBubblePulseMs;
  idle_ms_ = 0;
  hint_level_ = 0;
  hint_announced_ = false;
  next_hint_wiggle_ms_ = 0;
  ctx_->feedback.play(Cue::NOTE, enabled ? 5 : 6);
  for (auto& fish : fish_) {
    if (enabled && (fish.mode == FishMode::SWIM || fish.mode == FishMode::HELLO ||
                    fish.mode == FishMode::FOLLOW)) {
      fish.mode = FishMode::SLEEP;
      fish.mode_ms = 0;
      fish.vx = fish.vy = 0;
    } else if (!enabled && fish.mode == FishMode::SLEEP) {
      fish.mode = FishMode::SWIM;
      fish.wait_ms = 400;
      pick_target(fish, static_cast<std::size_t>(fish.type));
    }
  }
}

void BubbleAquariumGame::bubble_burst() {
  bubble_pulse_ms_ = kBubblePulseMs;
  chest_open_ms_ = 1200;
  const std::size_t queued = active_bubbles() + pending_bubbles_;
  const std::size_t total = queued + 5;
  pending_bubbles_ = static_cast<uint8_t>(total > kMaxBubbles ? kMaxBubbles - active_bubbles()
                                                               : total - active_bubbles());
  bubble_spawn_ms_ = 100;
  ctx_->feedback.play(Cue::NOTE, kNoteBurst);
}

void BubbleAquariumGame::drop_food() {
  food_pulse_ms_ = kBubblePulseMs;
  ctx_->feedback.play(Cue::TAP, 0);
  for (auto& pellet : food_) {
    if (pellet.active) continue;
    pellet = Food{};
    pellet.active = true;
    pellet.mode = FoodMode::FALL;
    pellet.x = static_cast<int16_t>(kSpoutX + static_cast<int32_t>(ctx_->rng.below(17)) - 8);
    pellet.y = kSpoutY;
    pellet.x_q8 = static_cast<int32_t>(pellet.x) * 256;
    pellet.y_q8 = static_cast<int32_t>(pellet.y) * 256;
    return;
  }
}

void BubbleAquariumGame::spawn_bubble() {
  std::size_t slot = 0;
  bool found = false;
  for (std::size_t i = 0; i < kMaxBubbles; ++i) {
    if (!bubbles_[i].active) {
      slot = i;
      found = true;
      break;
    }
  }
  if (!found) {
    for (std::size_t i = 1; i < kMaxBubbles; ++i) {
      if (bubbles_[i].serial < bubbles_[slot].serial) slot = i;
    }
  }
  Bubble& bubble = bubbles_[slot];
  bubble = Bubble{};
  bubble.active = true;
  bubble.size = static_cast<uint8_t>(kBubbleRadiusMin + ctx_->rng.below(kBubbleRadiusMax - kBubbleRadiusMin + 1));
  bubble.x = static_cast<int16_t>(kChestX + static_cast<int32_t>(ctx_->rng.below(17)) - 8);
  bubble.y = 198;
  bubble.x_q8 = static_cast<int32_t>(bubble.x) * 256;
  bubble.y_q8 = static_cast<int32_t>(bubble.y) * 256;
  bubble.drift = static_cast<int16_t>(static_cast<int32_t>(ctx_->rng.below(19)) - 9);
  bubble.serial = bubble_serial_++;
}

void BubbleAquariumGame::start_visitor() {
  if (visitor_active_) {
    if (pending_visitors_ < 2) ++pending_visitors_;
    return;
  }
  visitor_ = static_cast<Visitor>(visitor_next_);
  visitor_direction_ = kVisitorDirection[visitor_next_];
  visitor_x_q8_ = static_cast<int32_t>(visitor_direction_ > 0 ? -70 : kScreenWidth + 70) * 256;
  visitor_y_ = kVisitorY;
  visitor_age_ms_ = 0;
  visitor_waved_ = false;
  visitor_active_ = true;
  visitor_next_ = static_cast<uint8_t>((visitor_next_ + 1) % kVisitorCount);
  ++visitors_seen_;
  ctx_->feedback.play(Cue::NOTE, static_cast<uint8_t>(7 + static_cast<uint8_t>(visitor_)));
}

void BubbleAquariumGame::pick_target(Fish& fish, std::size_t index) {
  if (hint_level_ >= 2 && !bedtime_) {
    fish.target_x = static_cast<int16_t>(84 + ctx_->rng.below(77));
    fish.target_y = static_cast<int16_t>(92 + ctx_->rng.below(25));
  } else if (visitor_active_) {
    fish.target_x = static_cast<int16_t>(kFishMinX + ctx_->rng.below(kFishMaxX - kFishMinX + 1));
    fish.target_y = static_cast<int16_t>(184 + ctx_->rng.below(11));
  } else {
    fish.target_x = static_cast<int16_t>(kFishMinX + ctx_->rng.below(kFishMaxX - kFishMinX + 1));
    fish.target_y = static_cast<int16_t>(kLaneTop[index] +
                                         ctx_->rng.below(kLaneBottom[index] - kLaneTop[index] + 1));
  }
}

void BubbleAquariumGame::move_toward(Fish& fish, int16_t x, int16_t y, int16_t speed) {
  const int32_t cx = to_px(fish.x_q8);
  const int32_t cy = to_px(fish.y_q8);
  const int32_t dx = static_cast<int32_t>(x) - cx;
  const int32_t dy = static_cast<int32_t>(y) - cy;
  const int32_t scale = abs32(dx) > abs32(dy) ? abs32(dx) : abs32(dy);
  if (scale == 0) return;
  const int16_t desired_vx = static_cast<int16_t>(dx * speed / scale);
  const int16_t desired_vy = static_cast<int16_t>(dy * speed / scale);
  fish.vx = static_cast<int16_t>(fish.vx + (desired_vx - fish.vx) / 4);
  fish.vy = static_cast<int16_t>(fish.vy + (desired_vy - fish.vy) / 4);
  fish.x_q8 += travel_q8(fish.vx, kStepMs);
  fish.y_q8 += travel_q8(fish.vy, kStepMs);
  fish.x_q8 = clamp32(fish.x_q8, kFishMinX * 256, kFishMaxX * 256);
  fish.y_q8 = clamp32(fish.y_q8, kFishMinY * 256, kFishMaxY * 256);
  if (fish.vx > 2) fish.face = 1;
  else if (fish.vx < -2) fish.face = -1;
  fish.rect.x = static_cast<int16_t>(to_px(fish.x_q8) - fish.rect.w / 2);
  fish.rect.y = static_cast<int16_t>(to_px(fish.y_q8) - fish.rect.h / 2);
}

void BubbleAquariumGame::claim_food() {
  for (std::size_t p = 0; p < kMaxFood; ++p) {
    Food& pellet = food_[p];
    if (!pellet.active || pellet.eater >= 0 || pellet.mode == FoodMode::FALL) continue;
    int best = -1;
    int32_t best_distance = 0;
    for (std::size_t i = 0; i < kFishCount; ++i) {
      Fish& fish = fish_[i];
      if (fish.food_index >= 0 || (fish.mode != FishMode::SWIM && fish.mode != FishMode::HELLO &&
                                   fish.mode != FishMode::FOLLOW && fish.mode != FishMode::SLEEP)) {
        continue;
      }
      const int32_t d = distance2(to_px(fish.x_q8), to_px(fish.y_q8), pellet.x, pellet.y);
      if (best < 0 || d < best_distance) {
        best = static_cast<int>(i);
        best_distance = d;
      }
    }
    if (best < 0) continue;
    Fish& fish = fish_[static_cast<std::size_t>(best)];
    pellet.eater = static_cast<int8_t>(best);
    fish.food_index = static_cast<int8_t>(p);
    fish.nap_after_meal = bedtime_ || fish.mode == FishMode::SLEEP;
    fish.mode = FishMode::FOOD;
    fish.mode_ms = 8000;
    fish.peek_ms = 0;
  }
}

void BubbleAquariumGame::eat_food(std::size_t fish_index, std::size_t food_index) {
  Fish& fish = fish_[fish_index];
  food_[food_index].active = false;
  food_[food_index].eater = -1;
  fish.food_index = -1;
  fish.mode = FishMode::GULP;
  fish.mode_ms = kGulpMs;
  fish.happy_ms = kHappyMs;
  fish.munch_ms = 600;
  ++fish.meals;
  ++meals_;
  ctx_->feedback.play(Cue::SUCCESS, static_cast<uint8_t>(fish.type));
  if (meals_ % kVisitorMeals == 0) start_visitor();
}

void BubbleAquariumGame::step_fish(Fish& fish, std::size_t index) {
  fish.happy_ms = saturating_sub(fish.happy_ms, kStepMs);
  fish.munch_ms = saturating_sub(fish.munch_ms, kStepMs);
  fish.peek_ms = static_cast<uint16_t>(saturating_sub(fish.peek_ms, kStepMs));
  switch (fish.mode) {
    case FishMode::SWIM:
      if (fish.wait_ms > kStepMs) {
        fish.wait_ms = static_cast<uint16_t>(fish.wait_ms - kStepMs);
        return;
      }
      if (distance2(to_px(fish.x_q8), to_px(fish.y_q8), fish.target_x, fish.target_y) < 100) {
        pick_target(fish, index);
        fish.wait_ms = static_cast<uint16_t>(600 + ctx_->rng.below(1401));
      } else {
        move_toward(fish, fish.target_x, fish.target_y, kSwimSpeed[index] * (bedtime_ ? 3 : 5) / 5);
      }
      break;
    case FishMode::FOOD: {
      if (fish.food_index < 0 || !food_[static_cast<std::size_t>(fish.food_index)].active) {
        fish.food_index = -1;
        fish.mode = bedtime_ ? FishMode::SLEEP : FishMode::SWIM;
        pick_target(fish, index);
        break;
      }
      if (fish.mode_ms <= kStepMs) {
        Food& pellet = food_[static_cast<std::size_t>(fish.food_index)];
        pellet.eater = -1;
        fish.food_index = -1;
        fish.mode = bedtime_ ? FishMode::SLEEP : FishMode::SWIM;
        pick_target(fish, index);
        break;
      }
      fish.mode_ms = static_cast<uint16_t>(fish.mode_ms - kStepMs);
      {
        const Food& pellet = food_[static_cast<std::size_t>(fish.food_index)];
        const int16_t mouth_x = static_cast<int16_t>(to_px(fish.x_q8) + fish.face * 25);
        if (distance2(mouth_x, to_px(fish.y_q8), pellet.x, pellet.y) <= 144) {
          eat_food(index, static_cast<std::size_t>(fish.food_index));
          break;
        }
        if (abs32(static_cast<int32_t>(pellet.x) - to_px(fish.x_q8)) > 6) {
          fish.face = pellet.x > to_px(fish.x_q8) ? 1 : -1;
        }
        move_toward(fish, static_cast<int16_t>(pellet.x - fish.face * 25),
                    static_cast<int16_t>(pellet.y - 4), bedtime_ ? 34 : 62);
      }
      break;
    }
    case FishMode::GULP:
      if (fish.mode_ms <= kStepMs) {
        fish.mode = FishMode::HAPPY;
        fish.mode_ms = static_cast<uint16_t>(kHappyMs - kStepMs);
      } else {
        fish.mode_ms = static_cast<uint16_t>(fish.mode_ms - kStepMs);
      }
      break;
    case FishMode::HAPPY:
      if (fish.mode_ms <= kStepMs) {
        fish.mode = bedtime_ || fish.nap_after_meal ? FishMode::SLEEP : FishMode::SWIM;
        fish.mode_ms = 0;
        fish.nap_after_meal = false;
        if (fish.mode == FishMode::SWIM) pick_target(fish, index);
      } else {
        fish.mode_ms = static_cast<uint16_t>(fish.mode_ms - kStepMs);
      }
      break;
    case FishMode::HELLO:
      if (fish.mode_ms <= kStepMs) {
        fish.mode = bedtime_ ? FishMode::SLEEP : FishMode::SWIM;
        fish.mode_ms = 0;
        if (fish.mode == FishMode::SWIM) pick_target(fish, index);
      } else {
        fish.mode_ms = static_cast<uint16_t>(fish.mode_ms - kStepMs);
      }
      break;
    case FishMode::FOLLOW:
      if (distance2(to_px(fish.x_q8), to_px(fish.y_q8), fish.target_x, fish.target_y) < 64) {
        fish.mode = FishMode::HELLO;
        fish.mode_ms = 600;
        fish.happy_ms = 600;
      } else {
        move_toward(fish, fish.target_x, fish.target_y, 46);
      }
      break;
    case FishMode::SLEEP:
      fish.vx = fish.vy = 0;
      break;
  }
}

void BubbleAquariumGame::step_food() {
  for (auto& pellet : food_) {
    if (!pellet.active) continue;
    pellet.age_ms += kStepMs;
    if (pellet.mode == FoodMode::FALL) {
      pellet.vy = static_cast<int16_t>(pellet.vy + 15);
      pellet.y_q8 += travel_q8(pellet.vy, kStepMs);
      if (to_px(pellet.y_q8) >= kWaterSurface) {
        pellet.y_q8 = kWaterSurface * 256;
        pellet.y = kWaterSurface;
        pellet.vy = 0;
        pellet.mode = FoodMode::SINK;
      }
    } else if (pellet.mode == FoodMode::SINK) {
      pellet.y_q8 += travel_q8(19, kStepMs);
      if (to_px(pellet.y_q8) >= kFoodRestY) {
        pellet.y_q8 = kFoodRestY * 256;
        pellet.mode = FoodMode::REST;
      }
    }
    pellet.y = to_px(pellet.y_q8);
  }
}

void BubbleAquariumGame::step_bubbles() {
  if (pending_bubbles_ > 0) {
    bubble_spawn_ms_ = saturating_sub(bubble_spawn_ms_, kStepMs);
    if (bubble_spawn_ms_ == 0) {
      spawn_bubble();
      --pending_bubbles_;
      bubble_spawn_ms_ = 300;
      ctx_->feedback.play(Cue::NOTE, static_cast<uint8_t>(ctx_->rng.below(3)));
    }
  }
  chest_burp_ms_ = saturating_sub(chest_burp_ms_, kStepMs);
  if (chest_burp_ms_ == 0) {
    const std::size_t queued = active_bubbles() + pending_bubbles_;
    const std::size_t amount = queued >= kMaxBubbles ? 0 : (kMaxBubbles - queued < 2 ? kMaxBubbles - queued : 2);
    pending_bubbles_ = static_cast<uint8_t>(pending_bubbles_ + amount);
    chest_burp_ms_ = (bedtime_ ? 14000u : 9000u) + ctx_->rng.below(5001);
  }
  for (auto& bubble : bubbles_) {
    if (!bubble.active) continue;
    bubble.age_ms += kStepMs;
    bubble.y_q8 -= travel_q8(kBubbleRise + static_cast<int32_t>(bubble.age_ms % 7), kStepMs);
    bubble.x_q8 += travel_q8(bubble.drift, kStepMs);
    bubble.x_q8 = clamp32(bubble.x_q8, 30 * 256, 290 * 256);
    bubble.x = to_px(bubble.x_q8);
    bubble.y = to_px(bubble.y_q8);
    if (bubble.y - bubble.size <= kWaterSurface + 2) bubble.active = false;
  }
}

void BubbleAquariumGame::step_visitor() {
  if (!visitor_active_) return;
  visitor_age_ms_ += kStepMs;
  const int16_t speed = static_cast<int16_t>(bedtime_ ? kVisitorSpeed * 7 / 10 : kVisitorSpeed);
  visitor_x_q8_ += travel_q8(visitor_direction_ * speed, kStepMs);
  const int16_t x = to_px(visitor_x_q8_);
  if ((visitor_direction_ > 0 && x > kScreenWidth + kVisitorW) ||
      (visitor_direction_ < 0 && x < -kVisitorW)) {
    visitor_active_ = false;
    if (pending_visitors_ > 0) {
      --pending_visitors_;
      start_visitor();
    }
  }
}

void BubbleAquariumGame::step() {
  sim_time_ms_ += kStepMs;
  if (phase_ == Phase::INTRO) {
    intro_ms_ += kStepMs;
    if (intro_ms_ >= kIntroMs) start_play();
  } else if (!bedtime_) {
    idle_ms_ += kStepMs;
    if (idle_ms_ >= kHintVisualMs && hint_level_ == 0) {
      hint_level_ = 1;
      next_hint_wiggle_ms_ = 0;
    }
    if (idle_ms_ >= kHintSpokenMs && !hint_announced_) {
      hint_announced_ = true;
      hint_level_ = 2;
      ctx_->feedback.play(Cue::PROMPT, 0);
    }
    if (hint_level_ >= 1) {
      next_hint_wiggle_ms_ = saturating_sub(next_hint_wiggle_ms_, kStepMs);
      if (next_hint_wiggle_ms_ == 0) {
        food_pulse_ms_ = 360;
        next_hint_wiggle_ms_ = 2400;
      }
    }
  }

  step_food();
  claim_food();
  for (std::size_t i = 0; i < kFishCount; ++i) step_fish(fish_[i], i);
  step_bubbles();
  step_visitor();
}

void BubbleAquariumGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  const uint32_t elapsed = dt_ms < kMaxPhysicsMs ? dt_ms : kMaxPhysicsMs;
  food_pulse_ms_ = saturating_sub(food_pulse_ms_, elapsed);
  bedtime_pulse_ms_ = saturating_sub(bedtime_pulse_ms_, elapsed);
  bubble_pulse_ms_ = saturating_sub(bubble_pulse_ms_, elapsed);
  chest_open_ms_ = saturating_sub(chest_open_ms_, elapsed);
  if (bedtime_) {
    bedtime_fade_ms_ = static_cast<uint16_t>(bedtime_fade_ms_ + elapsed > 2000 ? 2000 : bedtime_fade_ms_ + elapsed);
  } else {
    bedtime_fade_ms_ = static_cast<uint16_t>(bedtime_fade_ms_ > elapsed ? bedtime_fade_ms_ - elapsed : 0);
  }
  bedtime_dim_ = static_cast<uint8_t>(bedtime_fade_ms_ * 180 / 2000);

  sim_accumulator_ms_ += elapsed;
  while (sim_accumulator_ms_ >= kStepMs) {
    sim_accumulator_ms_ -= kStepMs;
    step();
  }
}

int BubbleAquariumGame::hit_fish(int16_t x, int16_t y) const {
  for (std::size_t i = kFishCount; i > 0; --i) {
    if (fish_[i - 1].rect.contains(x, y)) return static_cast<int>(i - 1);
  }
  return -1;
}

int BubbleAquariumGame::hit_bubble(int16_t x, int16_t y) const {
  int best = -1;
  int32_t best_distance = 0;
  for (std::size_t i = 0; i < kMaxBubbles; ++i) {
    const Bubble& bubble = bubbles_[i];
    if (!bubble.active) continue;
    const int32_t d = distance2(x, y, bubble.x, bubble.y);
    const int32_t radius = bubble.size + 6;
    if (d <= radius * radius && (best < 0 || d < best_distance)) {
      best = static_cast<int>(i);
      best_distance = d;
    }
  }
  return best;
}

int BubbleAquariumGame::hit_visitor(int16_t x, int16_t y) const {
  return visitor_active_ && visitor_rect().contains(x, y) ? 0 : -1;
}

int BubbleAquariumGame::nearest_fish(int16_t x, int16_t y) const {
  int best = -1;
  int32_t best_distance = 0;
  for (std::size_t i = 0; i < kFishCount; ++i) {
    const Fish& fish = fish_[i];
    if (fish.mode == FishMode::FOOD || fish.mode == FishMode::GULP || fish.mode == FishMode::HAPPY) continue;
    const int32_t d = distance2(to_px(fish.x_q8), to_px(fish.y_q8), x, y);
    if (best < 0 || d < best_distance) {
      best = static_cast<int>(i);
      best_distance = d;
    }
  }
  return best;
}

void BubbleAquariumGame::request_help() {
  idle_ms_ = 0;
  hint_level_ = 2;
  hint_announced_ = true;
  ctx_->feedback.play(Cue::PROMPT, 0);
}

void BubbleAquariumGame::handle(const InputEvent& event) {
  if (!ctx_) return;
  if (phase_ == Phase::INTRO) {
    start_play();
    return;
  }
  if (event.source == Source::VOICE) {
    if (event.intent == Intent::AGAIN || event.intent == Intent::HELP) request_help();
    return;
  }

  idle_ms_ = 0;
  hint_level_ = 0;
  hint_announced_ = false;
  next_hint_wiggle_ms_ = 0;
  const int16_t x = event.x;
  const int16_t y = event.y;
  if (kFoodButton.contains(x, y)) {
    drop_food();
  } else if (kBedtimeButton.contains(x, y)) {
    set_bedtime(!bedtime_);
  } else if (kBubbleButton.contains(x, y)) {
    bubble_burst();
  } else if (y >= kPlayArea.y) {
    if (hit_bubble(x, y) >= 0) {
      bubbles_[static_cast<std::size_t>(hit_bubble(x, y))].active = false;
      ctx_->feedback.play(Cue::NOTE, kNotePop);
      return;
    }
    if (hit_visitor(x, y) >= 0) {
      visitor_waved_ = true;
      ctx_->feedback.play(Cue::REACTION, static_cast<uint8_t>(visitor_));
      return;
    }
    const int fish_index = hit_fish(x, y);
    if (fish_index >= 0) {
      Fish& fish = fish_[static_cast<std::size_t>(fish_index)];
      if (fish.mode == FishMode::SLEEP) {
        fish.peek_ms = 1400;
      } else {
        fish.happy_ms = 800;
        if (fish.mode == FishMode::SWIM || fish.mode == FishMode::FOLLOW) {
          fish.mode = FishMode::HELLO;
          fish.mode_ms = 600;
        }
      }
      ctx_->feedback.play(Cue::REACTION, static_cast<uint8_t>(fish.type));
      return;
    }
    if (kChestHit.contains(x, y)) {
      bubble_burst();
      return;
    }
    const int nearest = nearest_fish(x, y);
    if (nearest >= 0) {
      Fish& fish = fish_[static_cast<std::size_t>(nearest)];
      if (fish.mode == FishMode::SLEEP) {
        fish.peek_ms = 1400;
      } else {
        fish.mode = FishMode::FOLLOW;
        fish.target_x = static_cast<int16_t>(clamp32(x, kFishMinX, kFishMaxX));
        fish.target_y = static_cast<int16_t>(clamp32(y, kFishMinY, kFishMaxY));
      }
      ctx_->feedback.play(Cue::NOTE, 2);
    }
  }
}

}  // namespace little_arcade::bubble_aquarium
