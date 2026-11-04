#include "little_arcade/games/bubble_aquarium.hpp"

namespace little_arcade::bubble_aquarium {

namespace {
uint32_t saturating_sub(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }

int32_t clamp32(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Distance travelled in Q8 pixels at `speed` px/s over `dt_ms`.
int32_t travel_q8(int32_t speed, uint32_t dt_ms) {
  return speed * static_cast<int32_t>(dt_ms) * 256 / 1000;
}

int16_t to_px(int32_t q8) { return static_cast<int16_t>(q8 / 256); }

int16_t signed_speed(Rng& rng, uint32_t base, uint32_t spread) {
  const int32_t magnitude = static_cast<int32_t>(base + rng.below(spread));
  return static_cast<int16_t>(rng.below(2) ? magnitude : -magnitude);
}

// Cue variants (see README cue table).
constexpr uint8_t kNoteBlubCount = 3;  // NOTE 0..2: bubble blown by a water tap
constexpr uint8_t kNotePop = 3;        // NOTE 3: bubble popped
constexpr uint8_t kNoteBurst = 4;      // NOTE 4: treasure chest bubble burst
}  // namespace

void BubbleAquariumGame::enter(GameContext& context) {
  ctx_ = &context;
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
  bubble_serial_ = 0;
  meals_ = 0;
  feeds_since_visitor_ = 0;
  visitors_seen_ = 0;
  visitor_ = Visitor::TURTLE;
  have_visitor_ = false;
  food_pulse_ms_ = 0;
  bubble_pulse_ms_ = 0;
  ambient_started_ = false;
  for (auto& b : bubbles_) b = Bubble{};
  for (auto& f : food_) f = Food{};

  // One fish of each type, each in its own swim lane (lane order shuffled).
  static constexpr int16_t kLaneY[kFishCount] = {72, 120, 176};
  uint8_t lanes[kFishCount] = {0, 1, 2};
  ctx_->rng.shuffle(lanes);
  for (std::size_t i = 0; i < kFishCount; ++i) {
    const int16_t w = kFishW[i];
    const int16_t h = kFishH[i];
    const int16_t x = static_cast<int16_t>(kTank.x + ctx_->rng.below(static_cast<uint32_t>(kTank.w - w)));
    const int16_t y = kLaneY[lanes[i]];
    Fish& f = fish_[i];
    f = Fish{};
    f.type = static_cast<FishType>(i);
    f.rect = Rect{x, y, w, h};
    f.x_q8 = static_cast<int32_t>(x) * 256;
    f.y_q8 = static_cast<int32_t>(y) * 256;
    f.wander_vx = signed_speed(ctx_->rng, 20, 20);
    f.wander_vy = signed_speed(ctx_->rng, 4, 8);
  }
}

void BubbleAquariumGame::exit() {
  ctx_ = nullptr;
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
  for (auto& b : bubbles_) b = Bubble{};
  for (auto& f : food_) f = Food{};
  food_pulse_ms_ = 0;
  bubble_pulse_ms_ = 0;
}

uint32_t BubbleAquariumGame::active_contexts() const {
  // Only Global intents (AGAIN) are used; the SceneManager adds Global itself.
  return 0;
}

std::size_t BubbleAquariumGame::active_bubbles() const {
  std::size_t n = 0;
  for (const auto& b : bubbles_) n += b.active ? 1 : 0;
  return n;
}

std::size_t BubbleAquariumGame::active_food() const {
  std::size_t n = 0;
  for (const auto& f : food_) n += f.active ? 1 : 0;
  return n;
}

Rect BubbleAquariumGame::visitor_rect() const {
  const uint32_t t = phase_ms_ < kCelebrateMs ? phase_ms_ : kCelebrateMs;
  const int32_t travel = static_cast<int32_t>(kScreenWidth + kVisitorW);
  const int32_t x = -static_cast<int32_t>(kVisitorW) +
                    travel * static_cast<int32_t>(t) / static_cast<int32_t>(kCelebrateMs);
  return Rect{static_cast<int16_t>(x), kVisitorY, kVisitorW, kVisitorH};
}

void BubbleAquariumGame::start_play() {
  phase_ = Phase::PLAY;
  phase_ms_ = 0;
  ctx_->feedback.play(Cue::PROMPT, 0);
  if (!ambient_started_) {
    ctx_->feedback.play(Cue::AMBIENT, 0);
    ambient_started_ = true;
  }
}

void BubbleAquariumGame::start_celebrate() {
  uint32_t v = ctx_->rng.below(static_cast<uint32_t>(kVisitorCount));
  if (have_visitor_ && v == static_cast<uint32_t>(visitor_)) v = (v + 1) % kVisitorCount;
  visitor_ = static_cast<Visitor>(v);
  have_visitor_ = true;
  ++visitors_seen_;
  feeds_since_visitor_ = 0;
  phase_ = Phase::CELEBRATE;
  phase_ms_ = 0;
  ctx_->feedback.play(Cue::CELEBRATE, static_cast<uint8_t>(v));
}

void BubbleAquariumGame::spawn_bubble(int16_t x, int16_t y) {
  // Fixed pool: reuse a free slot, otherwise recycle the oldest bubble.
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
  Bubble& b = bubbles_[slot];
  b.x = x;
  b.y = y;
  b.y_q8 = static_cast<int32_t>(y) * 256;
  b.size = static_cast<uint8_t>(8 + ctx_->rng.below(9));
  b.active = true;
  b.serial = bubble_serial_++;
}

void BubbleAquariumGame::bubble_burst() {
  bubble_pulse_ms_ = kPulseMs;
  ctx_->feedback.play(Cue::NOTE, kNoteBurst);
  for (std::size_t i = 0; i < kBurstBubbles; ++i) {
    const int16_t offset = static_cast<int16_t>((static_cast<int>(i) - 1) * 16);
    spawn_bubble(static_cast<int16_t>(kChestX + offset), kChestY);
  }
}

void BubbleAquariumGame::drop_food() {
  food_pulse_ms_ = kPulseMs;
  ctx_->feedback.play(Cue::TAP, 0);
  for (auto& f : food_) {
    if (f.active) continue;
    f.x = static_cast<int16_t>(kTank.x + 40 + ctx_->rng.below(static_cast<uint32_t>(kTank.w - 80)));
    f.y = kFoodTopY;
    f.y_q8 = static_cast<int32_t>(kFoodTopY) * 256;
    f.active = true;
    return;
  }
  // Pool full: the shaker still wiggles, the fish simply finish what is there.
}

void BubbleAquariumGame::tap_water(int16_t x, int16_t y) {
  ctx_->feedback.play(Cue::NOTE, static_cast<uint8_t>(ctx_->rng.below(kNoteBlubCount)));
  spawn_bubble(x, y);
}

int BubbleAquariumGame::hit_fish(int16_t x, int16_t y) const {
  // Later fish are drawn on top, so test them first.
  for (std::size_t i = kFishCount; i > 0; --i) {
    if (fish_[i - 1].rect.contains(x, y)) return static_cast<int>(i - 1);
  }
  return -1;
}

int BubbleAquariumGame::hit_bubble(int16_t x, int16_t y) const {
  for (std::size_t i = 0; i < kMaxBubbles; ++i) {
    const Bubble& b = bubbles_[i];
    if (!b.active) continue;
    const int dx = x - b.x;
    const int dy = y - b.y;
    if (dx >= -kBubbleHit && dx < kBubbleHit && dy >= -kBubbleHit && dy < kBubbleHit) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int BubbleAquariumGame::nearest_food(const Fish& f) const {
  const int cx = f.rect.x + f.rect.w / 2;
  const int cy = f.rect.y + f.rect.h / 2;
  int best = -1;
  int best_dist = 0;
  for (std::size_t i = 0; i < kMaxFood; ++i) {
    if (!food_[i].active) continue;
    const int dx = food_[i].x - cx;
    const int dy = food_[i].y - cy;
    const int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    if (best < 0 || dist < best_dist) {
      best = static_cast<int>(i);
      best_dist = dist;
    }
  }
  return best;
}

void BubbleAquariumGame::step_fish(Fish& f, uint32_t dt_ms) {
  const int food = nearest_food(f);
  int32_t vx = f.wander_vx;
  int32_t vy = f.wander_vy;
  if (food >= 0) {
    const Food& p = food_[food];
    vx = clamp32((p.x - (f.rect.x + f.rect.w / 2)) * 2, -kChaseSpeed, kChaseSpeed);
    vy = clamp32((p.y - (f.rect.y + f.rect.h / 2)) * 2, -kChaseSpeed, kChaseSpeed);
  }
  const int32_t min_x = static_cast<int32_t>(kTank.x) * 256;
  const int32_t max_x = static_cast<int32_t>(kTank.x + kTank.w - f.rect.w) * 256;
  const int32_t min_y = static_cast<int32_t>(kTank.y) * 256;
  const int32_t max_y = static_cast<int32_t>(kTank.y + kTank.h - f.rect.h) * 256;
  f.x_q8 = clamp32(f.x_q8 + travel_q8(vx, dt_ms), min_x, max_x);
  f.y_q8 = clamp32(f.y_q8 + travel_q8(vy, dt_ms), min_y, max_y);
  if (food < 0) {
    // Gentle bounce off the glass while wandering.
    if ((f.x_q8 <= min_x && f.wander_vx < 0) || (f.x_q8 >= max_x && f.wander_vx > 0)) {
      f.wander_vx = static_cast<int16_t>(-f.wander_vx);
    }
    if ((f.y_q8 <= min_y && f.wander_vy < 0) || (f.y_q8 >= max_y && f.wander_vy > 0)) {
      f.wander_vy = static_cast<int16_t>(-f.wander_vy);
    }
  }
  f.rect.x = to_px(f.x_q8);
  f.rect.y = to_px(f.y_q8);
}

void BubbleAquariumGame::eat_check() {
  for (auto& p : food_) {
    if (!p.active) continue;
    for (auto& f : fish_) {
      if (!f.rect.contains(p.x, p.y)) continue;
      p.active = false;
      ++f.meals;
      f.munch_ms = kMunchMs;
      ++meals_;
      ++feeds_since_visitor_;
      ctx_->feedback.play(Cue::SUCCESS, static_cast<uint8_t>(f.type));
      break;
    }
  }
  if (phase_ == Phase::PLAY && feeds_since_visitor_ >= kFeedsPerVisitor) start_celebrate();
}

void BubbleAquariumGame::step(uint32_t dt_ms) {
  for (auto& b : bubbles_) {
    if (!b.active) continue;
    b.y_q8 -= travel_q8(kBubbleRise, dt_ms);
    if (b.y_q8 < static_cast<int32_t>(kTank.y) * 256) {
      b.active = false;  // reached the surface
    } else {
      b.y = to_px(b.y_q8);
    }
  }
  const int32_t rest_q8 = static_cast<int32_t>(kFoodRestY) * 256;
  for (auto& p : food_) {
    if (!p.active) continue;
    p.y_q8 += travel_q8(kFoodSink, dt_ms);
    if (p.y_q8 > rest_q8) p.y_q8 = rest_q8;  // rests on the sand until eaten
    p.y = to_px(p.y_q8);
  }
  for (auto& f : fish_) step_fish(f, dt_ms);
  eat_check();
}

void BubbleAquariumGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  food_pulse_ms_ = saturating_sub(food_pulse_ms_, dt_ms);
  bubble_pulse_ms_ = saturating_sub(bubble_pulse_ms_, dt_ms);
  for (auto& f : fish_) {
    f.happy_ms = saturating_sub(f.happy_ms, dt_ms);
    f.munch_ms = saturating_sub(f.munch_ms, dt_ms);
  }
  phase_ms_ += dt_ms;

  uint32_t remaining = dt_ms < kMaxPhysicsMs ? dt_ms : kMaxPhysicsMs;
  while (remaining > 0) {
    const uint32_t s = remaining < kStepMs ? remaining : kStepMs;
    step(s);
    remaining -= s;
  }

  if (phase_ == Phase::INTRO && phase_ms_ >= kIntroMs) {
    start_play();
  } else if (phase_ == Phase::CELEBRATE && phase_ms_ >= kCelebrateMs) {
    phase_ = Phase::PLAY;
    phase_ms_ = 0;
    if (feeds_since_visitor_ >= kFeedsPerVisitor) start_celebrate();
  }
}

void BubbleAquariumGame::handle(const InputEvent& event) {
  if (!ctx_) return;

  if (phase_ == Phase::INTRO) {
    // Any input skips the intro; it does not also act.
    start_play();
    return;
  }

  // PLAY and CELEBRATE accept the same calm free-play input; the visitor just
  // swims by, so taps or voice during the animation are always safe.
  if (event.source == Source::TOUCH) {
    const int16_t x = event.x;
    const int16_t y = event.y;
    if (kFoodButton.contains(x, y)) {
      drop_food();
    } else if (kBubbleButton.contains(x, y)) {
      bubble_burst();
    } else if (kTank.contains(x, y)) {
      const int fish = hit_fish(x, y);
      if (fish >= 0) {
        Fish& f = fish_[fish];
        f.happy_ms = kHappyMs;
        ctx_->feedback.play(Cue::REACTION, static_cast<uint8_t>(f.type));
        return;
      }
      const int bubble = hit_bubble(x, y);
      if (bubble >= 0) {
        bubbles_[bubble].active = false;
        ctx_->feedback.play(Cue::NOTE, kNotePop);
        return;
      }
      tap_water(x, y);
    }
    return;
  }

  if (event.intent == Intent::AGAIN) bubble_burst();
}

}  // namespace little_arcade::bubble_aquarium
