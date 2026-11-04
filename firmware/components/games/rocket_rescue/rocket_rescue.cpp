#include "little_arcade/games/rocket_rescue.hpp"

namespace little_arcade::rocket_rescue {

namespace {
uint32_t saturating_sub(uint32_t a, uint32_t b) { return a > b ? a - b : 0; }
}  // namespace

Point RocketRescueGame::center_of(const Rect& r) {
  return Point{static_cast<int16_t>(r.x + r.w / 2), static_cast<int16_t>(r.y + r.h / 2)};
}

void RocketRescueGame::enter(GameContext& context) {
  ctx_ = &context;
  rescues_ = 0;
  missions_ = 0;
  stars_collected_ = 0;

  Look looks[kPlanetCount] = {Look::RINGED, Look::SPOTTED, Look::STRIPED};
  ctx_->rng.shuffle(looks);
  for (std::size_t i = 0; i < kPlanetCount; ++i) {
    planets_[i] = Planet{kPlanetRects[i], looks[i], Friend::MOON_PUP, false};
  }
  rocket_ = RocketState::DOCKED;  // fresh start on the launch pad
  docked_at_ = -1;
  destination_ = -1;
  origin_ = kPadPoint;
  trip_ms_ = 0;
  new_mission();

  uint8_t slots[kStarSlotCount] = {0, 1, 2, 3, 4};
  ctx_->rng.shuffle(slots);
  for (std::size_t i = 0; i < kStarCount; ++i) stars_[i] = Star{slots[i], true, 0};

  hint_ms_ = 0;
  wave_ms_ = 0;
  wave_index_ = -1;
  phase_ = Phase::INTRO;
  phase_ms_ = 0;
}

void RocketRescueGame::exit() {
  ctx_ = nullptr;
  phase_ = Phase::INTRO;
  rocket_ = RocketState::DOCKED;
  docked_at_ = -1;
  destination_ = -1;
  trip_ms_ = 0;
  hint_ms_ = 0;
  wave_ms_ = 0;
  wave_index_ = -1;
  mission_done_ = false;
}

uint32_t RocketRescueGame::active_contexts() const {
  return static_cast<uint32_t>(Context::Space);
}

void RocketRescueGame::new_mission() {
  Friend friends[kPlanetCount] = {Friend::MOON_PUP, Friend::STAR_BUNNY, Friend::COMET_KITTY};
  ctx_->rng.shuffle(friends);
  for (std::size_t i = 0; i < kPlanetCount; ++i) {
    planets_[i].buddy = friends[i];
    planets_[i].rescued = false;
  }
  for (std::size_t i = 0; i < kPlanetCount; ++i) order_[i] = static_cast<uint8_t>(i);
  ctx_->rng.shuffle(order_);
  // Don't suggest the planet the rocket is already sitting on.
  if (rocket_ == RocketState::DOCKED && docked_at_ >= 0 && order_[0] == docked_at_) {
    const uint8_t first = order_[0];
    order_[0] = order_[1];
    order_[1] = first;
  }
  mission_done_ = false;
}

int RocketRescueGame::suggested() const {
  for (uint8_t index : order_) {
    if (!planets_[index].rescued) return index;
  }
  return -1;
}

std::size_t RocketRescueGame::rescued_count() const {
  std::size_t n = 0;
  for (const Planet& p : planets_) n += p.rescued ? 1 : 0;
  return n;
}

uint32_t RocketRescueGame::trip_progress_permille() const {
  if (rocket_ == RocketState::DOCKED) return 0;
  return trip_ms_ >= kTripMs ? 1000u : trip_ms_ * 1000u / kTripMs;
}

Point RocketRescueGame::rocket_position() const {
  if (rocket_ == RocketState::DOCKED) {
    return docked_at_ >= 0 ? center_of(planets_[docked_at_].rect) : kPadPoint;
  }
  const Point to = center_of(planets_[destination_].rect);
  const int32_t p = static_cast<int32_t>(trip_progress_permille());
  return Point{static_cast<int16_t>(origin_.x + (to.x - origin_.x) * p / 1000),
               static_cast<int16_t>(origin_.y + (to.y - origin_.y) * p / 1000)};
}

void RocketRescueGame::start_play() {
  phase_ = Phase::PLAY;
  phase_ms_ = 0;
  const int next = suggested();
  if (next >= 0) ctx_->feedback.play(Cue::PROMPT, static_cast<uint8_t>(next));
}

void RocketRescueGame::replay_prompt() {
  const int next = suggested();
  if (next >= 0) ctx_->feedback.play(Cue::PROMPT, static_cast<uint8_t>(next));
  hint_ms_ = kHintMs;
}

void RocketRescueGame::wave(int planet) {
  wave_index_ = planet;
  wave_ms_ = kWaveMs;
  ctx_->feedback.play(Cue::REACTION, static_cast<uint8_t>(planets_[planet].buddy));
}

void RocketRescueGame::fly_to(int planet) {
  if (rocket_ == RocketState::DOCKED && docked_at_ == planet) {
    wave(planet);  // already here: the friend says hello
    return;
  }
  if (rocket_ != RocketState::DOCKED && destination_ == planet) {
    if (rocket_ == RocketState::HOVERING) go();  // same planet again = keep going
    return;
  }
  origin_ = rocket_position();
  destination_ = planet;
  trip_ms_ = 0;
  rocket_ = RocketState::FLYING;
  hint_ms_ = 0;
  ctx_->feedback.play(Cue::NOTE, kNoteLaunch);
}

void RocketRescueGame::go() {
  switch (rocket_) {
    case RocketState::FLYING:
      return;  // already on the way
    case RocketState::HOVERING:
      rocket_ = RocketState::FLYING;
      ctx_->feedback.play(Cue::NOTE, kNoteLaunch);
      return;
    case RocketState::DOCKED: {
      const int next = suggested();
      if (next >= 0) fly_to(next);
      return;
    }
  }
}

void RocketRescueGame::stop() {
  if (rocket_ != RocketState::FLYING) return;
  rocket_ = RocketState::HOVERING;
  ctx_->feedback.play(Cue::NOTE, kNoteHover);
}

void RocketRescueGame::arrive() {
  const int here = destination_;
  rocket_ = RocketState::DOCKED;
  docked_at_ = here;
  destination_ = -1;
  trip_ms_ = 0;
  origin_ = center_of(planets_[here].rect);

  Planet& p = planets_[here];
  if (p.rescued) {
    wave(here);  // friend already aboard earlier; a friendly visit
    return;
  }
  p.rescued = true;
  ++rescues_;
  hint_ms_ = 0;
  wave_index_ = here;
  wave_ms_ = kWaveMs;
  phase_ = Phase::CELEBRATE;
  phase_ms_ = 0;
  ctx_->feedback.play(Cue::SUCCESS, static_cast<uint8_t>(p.buddy));
  if (rescued_count() == kPlanetCount) {
    mission_done_ = true;
    ++missions_;
    ctx_->feedback.play(Cue::CELEBRATE, kCelebrateMission);
  } else {
    ctx_->feedback.play(Cue::CELEBRATE, kCelebrateRescue);
  }
}

void RocketRescueGame::respawn_star(std::size_t i) {
  bool used[kStarSlotCount] = {};
  for (std::size_t s = 0; s < kStarCount; ++s) used[stars_[s].slot] = true;  // includes own old slot
  uint8_t free_slots[kStarSlotCount] = {};
  uint32_t n = 0;
  for (std::size_t s = 0; s < kStarSlotCount; ++s) {
    if (!used[s]) free_slots[n++] = static_cast<uint8_t>(s);
  }
  if (n > 0) stars_[i].slot = free_slots[ctx_->rng.below(n)];
  stars_[i].visible = true;
  stars_[i].respawn_ms = 0;
}

void RocketRescueGame::collect_star(std::size_t i) {
  stars_[i].visible = false;
  stars_[i].respawn_ms = kStarRespawnMs;
  ++stars_collected_;
  ctx_->feedback.play(Cue::NOTE, kNoteTwinkle);
}

void RocketRescueGame::update(uint32_t dt_ms) {
  if (!ctx_) return;
  hint_ms_ = saturating_sub(hint_ms_, dt_ms);
  wave_ms_ = saturating_sub(wave_ms_, dt_ms);
  for (std::size_t i = 0; i < kStarCount; ++i) {
    if (stars_[i].visible) continue;
    stars_[i].respawn_ms = saturating_sub(stars_[i].respawn_ms, dt_ms);
    if (stars_[i].respawn_ms == 0) respawn_star(i);
  }

  phase_ms_ += dt_ms;
  switch (phase_) {
    case Phase::INTRO:
      if (phase_ms_ >= kIntroMs) start_play();
      break;
    case Phase::CELEBRATE:
      if (phase_ms_ >= kCelebrateMs) {
        if (mission_done_) new_mission();
        start_play();
      }
      break;
    case Phase::PLAY:
      if (rocket_ == RocketState::FLYING) {
        trip_ms_ += dt_ms;
        if (trip_ms_ >= kTripMs) arrive();
      }
      break;
  }
}

int RocketRescueGame::planet_at(int16_t x, int16_t y) const {
  for (std::size_t i = 0; i < kPlanetCount; ++i) {
    if (planets_[i].rect.contains(x, y)) return static_cast<int>(i);
  }
  return -1;
}

int RocketRescueGame::star_at(int16_t x, int16_t y) const {
  for (std::size_t i = 0; i < kStarCount; ++i) {
    if (stars_[i].visible && kStarSlots[stars_[i].slot].contains(x, y)) return static_cast<int>(i);
  }
  return -1;
}

void RocketRescueGame::handle(const InputEvent& event) {
  if (!ctx_ || phase_ == Phase::CELEBRATE) return;  // input during the celebration is ignored

  if (phase_ == Phase::INTRO) {
    // Any input skips the intro; it does not launch the rocket.
    start_play();
    return;
  }

  if (event.source == Source::TOUCH) {
    const int planet = planet_at(event.x, event.y);
    if (planet >= 0) {
      ctx_->feedback.play(Cue::TAP, 0);
      fly_to(planet);
      return;
    }
    const int star = star_at(event.x, event.y);
    if (star >= 0) {
      collect_star(static_cast<std::size_t>(star));
      return;
    }
    if (kGoButton.contains(event.x, event.y)) {
      ctx_->feedback.play(Cue::TAP, 0);
      go();
    } else if (kStopButton.contains(event.x, event.y)) {
      ctx_->feedback.play(Cue::TAP, 0);
      stop();
    } else if (kPromptBadge.contains(event.x, event.y)) {
      replay_prompt();  // touch alternative to AGAIN / HELP
    }
    return;
  }

  switch (event.intent) {
    case Intent::GO:
      go();
      break;
    case Intent::STOP:
      stop();
      break;
    case Intent::AGAIN:
    case Intent::HELP:
      replay_prompt();
      break;
    default:
      break;
  }
}

}  // namespace little_arcade::rocket_rescue
