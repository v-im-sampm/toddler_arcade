#include "model.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace hatch_a_pet {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kTapDebounce = 0.08;
constexpr double kSpringK = 380, kSpringC = 14;
constexpr double kChoiceWait = 5, kChoiceInputBlock = 0.3;
constexpr double kEggX = 160, kPetX = 160, kPetY = 150;
constexpr double kPetHit = 52;
constexpr double kShelfHit = 16;
constexpr std::array<Creature, 8> kCreatures{{
    Creature::Bloop, Creature::Fluffwing, Creature::Sproutling, Creature::Zigzag,
    Creature::Puddlepup, Creature::Glimmerbug, Creature::RolyPole, Creature::Snorkle
}};

double clamp(double value, double low, double high) {
  return value < low ? low : (value > high ? high : value);
}
double lerp(double from, double to, double amount) {
  return from + (to - from) * amount;
}
double ease_out_cubic(double t) {
  return 1 - std::pow(1 - t, 3);
}
double ease_out_back(double t) {
  constexpr double c1 = 1.70158, c3 = c1 + 1;
  return 1 + c3 * std::pow(t - 1, 3) + c1 * std::pow(t - 1, 2);
}
bool in_rect(double x, double y, double left, double top, double width, double height) {
  return x >= left && x < left + width && y >= top && y < top + height;
}
double distance(double x, double y, double other_x, double other_y) {
  return std::hypot(x - other_x, y - other_y);
}
void spring(double& position, double& velocity, double target, double dt) {
  // Same two semi-implicit substeps as the reference's stable 60 Hz spring.
  for (int i = 0; i < 2; ++i) {
    const double h = dt / 2;
    const double acceleration = -kSpringK * (position - target) - kSpringC * velocity;
    velocity += acceleration * h;
    position += velocity * h;
  }
}
int index(Creature creature) {
  return static_cast<int>(creature);
}
}  // namespace

std::string_view cue_name(CueId cue) {
  switch (cue) {
    case CueId::MusicStart: return "music.start";
    case CueId::MusicStop: return "music.stop";
    case CueId::EggLand: return "egg.land";
    case CueId::PromptKnock: return "egg.prompt";
    case CueId::EggHint: return "egg.hint";
    case CueId::Knock: return "knock";
    case CueId::PreHatchTrill: return "pre_hatch.trill";
    case CueId::HatchPop: return "hatch.pop";
    case CueId::CreatureChirp: return "creature.chirp";
    case CueId::PetGiggle: return "pet.giggle";
    case CueId::PetJump: return "pet.jump";
    case CueId::PetSneeze: return "pet.sneeze";
    case CueId::ChoiceAppear: return "choice.appear";
    case CueId::ChoiceYes: return "choice.yes";
    case CueId::ChoiceNo: return "choice.no";
    case CueId::WaveBye: return "wave.bye";
    case CueId::ShelfWave: return "shelf.wave";
    case CueId::EmptyTap: return "touch.empty";
  }
  return "unknown";
}

std::string_view creature_name(Creature creature) {
  constexpr std::array<std::string_view, 8> names{{
      "BLOOP", "FLUFFWING", "SPROUTLING", "ZIGZAG",
      "PUDDLEPUP", "GLIMMERBUG", "ROLYPOLE", "SNORKLE"
  }};
  const int i = index(creature);
  return i >= 0 && i < static_cast<int>(names.size()) ? names[i] : names[0];
}

double Mulberry32::next() {
  state_ += 0x6D2B79F5u;
  uint32_t t = (state_ ^ (state_ >> 15)) * (1u | state_);
  t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
  return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
}

Model::Model(uint32_t seed)
    : gameplay_rng_(seed), cosmetic_rng_(seed ^ 0x9E3779B9u) {
  state_.shelf.reserve(kShelfSlots.size());
  bag_.reserve(kCreatures.size());
  cues_.reserve(kMaxQueuedCues);
}

void Model::emit(CueId id, int variant) {
  if (cues_.size() == kMaxQueuedCues) cues_.erase(cues_.begin());
  cues_.push_back(CueEvent{id, variant});
}

bool Model::pop_cue(CueEvent& cue) {
  if (cues_.empty()) return false;
  cue = cues_.front();
  cues_.erase(cues_.begin());
  return true;
}

std::vector<CueEvent> Model::take_cues() {
  std::vector<CueEvent> result(cues_);
  cues_.clear();
  return result;
}

Creature Model::next_creature() {
  if (bag_.empty()) {
    bag_.assign(kCreatures.begin(), kCreatures.end());
    for (int i = static_cast<int>(bag_.size()) - 1; i > 0; --i) {
      const int j = static_cast<int>(gameplay_rng_.next() * (i + 1));
      std::swap(bag_[i], bag_[j]);
    }
    if (static_cast<int>(bag_.back()) == last_creature_)
      std::swap(bag_.front(), bag_.back());
  }
  const Creature next = bag_.back();
  bag_.pop_back();
  return next;
}

void Model::new_egg() {
  state_.egg.creature = next_creature();
  last_creature_ = index(state_.egg.creature);
  state_.egg.pattern = static_cast<Pattern>(static_cast<int>(gameplay_rng_.next() * 4));
  state_.egg.need = 3 + static_cast<int>(gameplay_rng_.next() * 3);
  state_.egg.cracks = 0;
  state_.egg.squash = 0;
  state_.egg.squash_velocity = 0;
  state_.egg.angle = 0;
  state_.egg.wobble_seconds = 0;
  state_.egg.wobble_in = 3 + cosmetic_rng_.next() * 2;
  state_.idle_seconds = 0;
  state_.hint = 0;
  state_.spoken_hint = false;
  state_.choice.selected = Decision::None;
}

void Model::start_game() {
  emit(CueId::MusicStart);
  state_.phase = Phase::Intro;
  state_.phase_elapsed = 0;
  new_egg();
  state_.egg.y_offset = -200;
}

void Model::start_egg_play() {
  state_.phase = Phase::Egg;
  state_.phase_elapsed = 0;
  state_.prompt_seconds = 0.2;
  state_.badge_squash_velocity = -6;
}

void Model::home() {
  emit(CueId::MusicStop);
  state_.shelf.clear();
  state_.shell.visible = false;
  state_.choice.visible = false;
  state_.choice.fade_seconds = 0;
  state_.choice.selected = Decision::None;
  state_.phase = Phase::Start;
  state_.phase_elapsed = 0;
  state_.egg.y_offset = 0;
  bag_.clear();
  last_creature_ = -1;
}

void Model::knock() {
  ++state_.egg.cracks;
  state_.egg.squash_velocity = -3.2;
  state_.idle_seconds = 0;
  state_.hint = 0;
  state_.spoken_hint = false;
  state_.egg.wobble_seconds = 0;
  emit(CueId::Knock, state_.egg.cracks);
  if (state_.egg.cracks >= state_.egg.need) {
    state_.phase = Phase::PreHatch;
    state_.phase_elapsed = 0;
    emit(CueId::PreHatchTrill);
  }
}

void Model::hatch() {
  state_.phase = Phase::Hatch;
  state_.phase_elapsed = 0;
  ++state_.hatches;
  state_.shell.visible = true;
  state_.shell.elapsed = 0;
  state_.shell.top_x = 0;
  state_.shell.top_y = 0;
  state_.shell.top_vx = 70;
  state_.shell.top_vy = -260;
  state_.shell.top_rotation = 0;
  state_.shell.top_angular_velocity = 5;
  state_.shell.bottom_y = 0;
  state_.pet.x = kPetX;
  state_.pet.y = kPetY;
  state_.pet.scale = 0.2;
  state_.pet.squash = 0;
  state_.pet.squash_velocity = 0;
  state_.pet.reaction = Reaction::None;
  state_.pet.reaction_seconds = 0;
  state_.pet.reactions = 0;
  state_.pet.elapsed = 0;
  state_.pet.next_reaction = 0;
  state_.badge_happy_seconds = 1.2;
  state_.badge_squash_velocity = -10;
  emit(CueId::HatchPop);
}

void Model::react() {
  const Reaction reaction = static_cast<Reaction>(state_.pet.next_reaction);
  state_.pet.reaction = reaction;
  state_.pet.next_reaction = (state_.pet.next_reaction + 1) % 3;
  state_.pet.reaction_seconds = reaction == Reaction::Sneeze ? 0.8 : 0.7;
  ++state_.pet.reactions;
  state_.pet.elapsed = 0;
  state_.choice.idle_seconds = 0;
  if (reaction == Reaction::Jump) state_.pet.squash_velocity = -3;
  emit(reaction == Reaction::Giggle ? CueId::PetGiggle
       : reaction == Reaction::Jump ? CueId::PetJump : CueId::PetSneeze,
       index(state_.egg.creature));
}

void Model::choose_yes() {
  state_.choice.visible = false;
  state_.choice.fade_seconds = 0.35;
  state_.choice.selected = Decision::AnotherEgg;
  emit(CueId::ChoiceYes);
  emit(CueId::WaveBye, index(state_.egg.creature));
  if (state_.shelf.size() >= kShelfSlots.size()) {
    state_.shelf.erase(state_.shelf.begin());
    for (std::size_t i = 0; i < state_.shelf.size(); ++i)
      state_.shelf[i].target_x = kShelfSlots[i][0];
  }
  const auto& slot = kShelfSlots[state_.shelf.size()];
  state_.pet.from_x = state_.pet.x;
  state_.pet.from_y = state_.pet.y;
  state_.pet.to_x = slot[0];
  state_.pet.to_y = slot[1];
  state_.phase = Phase::Bye;
  state_.phase_elapsed = 0;
}

void Model::choose_no() {
  state_.choice.visible = false;
  state_.choice.fade_seconds = 0.35;
  state_.choice.selected = Decision::Stay;
  emit(CueId::ChoiceNo);
  state_.pet.reactions = 0;
  state_.pet.elapsed = 0;
  state_.pet.reaction = Reaction::Giggle;
  state_.pet.reaction_seconds = 0.7;
  emit(CueId::PetGiggle, index(state_.egg.creature));
}

void Model::badge_tap() {
  state_.idle_seconds = 0;
  state_.choice.idle_seconds = 0;
  state_.badge_squash_velocity = -12;
  if (state_.phase == Phase::Egg) {
    state_.egg.wobble_seconds = 0.5;
    state_.egg.look_x = 0;
    state_.egg.look_y = -60;
    emit(CueId::PromptKnock);
  } else if (state_.phase == Phase::Pet) {
    state_.pet.look_x = 0;
    state_.pet.look_y = -120;
    if (state_.pet.reaction == Reaction::None) {
      state_.pet.reaction = Reaction::Giggle;
      state_.pet.reaction_seconds = 0.7;
    }
    emit(CueId::CreatureChirp, index(state_.egg.creature));
  }
}

bool Model::command(Command action) {
  switch (action) {
    case Command::Help:
      if (state_.phase == Phase::Egg) {
        state_.hint = std::max(state_.hint, 1);
        state_.idle_seconds = 0;
        state_.egg.wobble_seconds = 0.5;
        emit(CueId::EggHint);
        return true;
      }
      if (state_.phase == Phase::Pet) {
        badge_tap();
        return true;
      }
      return false;
    case Command::Again:
      if (state_.phase == Phase::Egg || state_.phase == Phase::Pet) {
        badge_tap();
        return true;
      }
      return false;
    case Command::Yes:
      if (state_.phase == Phase::Pet && state_.choice.visible &&
          state_.choice.elapsed > kChoiceInputBlock) {
        choose_yes();
        return true;
      }
      return false;
    case Command::No:
      if (state_.phase == Phase::Pet && state_.choice.visible &&
          state_.choice.elapsed > kChoiceInputBlock) {
        choose_no();
        return true;
      }
      return false;
  }
  return false;
}

void Model::pointer_down(double x, double y) {
  TouchFeedback& touch = state_.touch;
  ++touch.serial;
  touch.x = x;
  touch.y = y;
  touch.target = TouchTarget::Empty;
  touch.down = true;
  state_.egg.look_x = x - kEggX;
  state_.egg.look_y = y - (138 - 14);
  state_.pet.look_x = x - state_.pet.x;
  state_.pet.look_y = y - state_.pet.y;

  const auto target = [&touch](TouchTarget value) { touch.target = value; };
  if (state_.phase == Phase::Start) {
    target(TouchTarget::Start);
    start_game();
    return;
  }
  if (in_rect(x, y, 0, 0, 56, 56)) {
    home();
    target(TouchTarget::Home);
    return;
  }
  if (in_rect(x, y, 264, 0, 56, 56)) {
    target(TouchTarget::Mic);
    return;  // The reference sketch's microphone tile is disabled.
  }
  if (state_.phase == Phase::Intro) {
    target(TouchTarget::IntroSkip);
    state_.phase_elapsed = 99;  // Reference intro is skippable on any non-chrome tap.
    return;
  }
  if (state_.phase == Phase::PreHatch || state_.phase == Phase::Hatch ||
      state_.phase == Phase::Bye || state_.phase == Phase::Roll) {
    target(TouchTarget::Blocked);  // Immediate renderer acknowledgement; gameplay stays locked.
    return;
  }
  if (in_rect(x, y, 132, 4, 56, 56)) {
    target(TouchTarget::Badge);
    badge_tap();
    return;
  }
  if (state_.phase == Phase::Egg) {
    if (in_rect(x, y, kEggHitRect[0], kEggHitRect[1], kEggHitRect[2], kEggHitRect[3])) {
      target(TouchTarget::Egg);
      if (state_.elapsed - last_egg_tap_ >= kTapDebounce) {
        last_egg_tap_ = state_.elapsed;
        knock();
      }
      return;
    }
  } else if (state_.phase == Phase::Pet) {
    if (state_.choice.visible && state_.choice.elapsed > kChoiceInputBlock) {
      if (distance(x, y, kChoiceCenters[1][0], kChoiceCenters[1][1]) < kChoiceHitRadius) {
        target(TouchTarget::ChoiceYes);
        choose_yes();
        return;
      }
      if (distance(x, y, kChoiceCenters[0][0], kChoiceCenters[0][1]) < kChoiceHitRadius) {
        target(TouchTarget::ChoiceNo);
        choose_no();
        return;
      }
    }
    if (distance(x, y, state_.pet.x, state_.pet.y) < kPetHit) {
      target(TouchTarget::Pet);
      if (state_.elapsed - last_pet_tap_ >= kTapDebounce) {
        last_pet_tap_ = state_.elapsed;
        react();
      }
      return;
    }
  }
  for (ShelfFriend& friend_pet : state_.shelf) {
    if (distance(x, y, friend_pet.x, friend_pet.y) < kShelfHit) {
      target(TouchTarget::Shelf);
      friend_pet.wave_seconds = 0.6;
      emit(CueId::ShelfWave, index(friend_pet.creature));
      return;
    }
  }
  target(TouchTarget::Empty);
  emit(CueId::EmptyTap);
  if (state_.phase == Phase::Egg) state_.idle_seconds = 0;
}

void Model::pointer_up() {
  TouchFeedback& touch = state_.touch;
  if (!touch.down) return;
  touch.down = false;
  touch.released_target = touch.target;
  ++touch.release_serial;
}

void Model::tick(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0) return;
  const bool prompt_was_active = state_.phase == Phase::Egg && state_.prompt_seconds > 0;
  state_.elapsed += seconds;
  state_.phase_elapsed += seconds;

  if (state_.phase == Phase::Intro) {
    const double k = clamp(state_.phase_elapsed / 0.7, 0, 1);
    state_.egg.y_offset = lerp(-200, 0, ease_out_cubic(k));
    if (k >= 1 && !state_.egg.landed) {
      state_.egg.landed = true;
      state_.egg.squash_velocity = -4;
      emit(CueId::EggLand);
    }
    if (state_.phase_elapsed >= 1.2) {
      state_.egg.y_offset = 0;
      state_.egg.landed = false;
      start_egg_play();
    }
  } else if (state_.phase == Phase::Egg) {
    state_.idle_seconds += seconds;
    if (state_.idle_seconds > 6 && state_.hint < 1) state_.hint = 1;
    if (state_.idle_seconds > 12 && !state_.spoken_hint) {
      state_.spoken_hint = true;
      state_.hint = 2;
      state_.badge_squash_velocity = -8;
      emit(CueId::PromptKnock);
    }
    state_.egg.wobble_in -= seconds;
    if (state_.egg.wobble_in <= 0) {
      state_.egg.wobble_seconds = 0.5;
      state_.egg.wobble_in = state_.hint >= 1 ? 1.6 : 3 + cosmetic_rng_.next() * 2;
    }
  } else if (state_.phase == Phase::PreHatch) {
    if (state_.phase_elapsed >= 0.4) hatch();
  } else if (state_.phase == Phase::Hatch) {
    if (state_.phase_elapsed >= 1.6) {
      state_.phase = Phase::Pet;
      state_.phase_elapsed = 0;
      state_.pet.elapsed = 0;
      emit(CueId::CreatureChirp, index(state_.egg.creature));
      state_.pet.reaction = Reaction::Jump;
      state_.pet.reaction_seconds = 0.7;
      state_.pet.squash_velocity = -3;
    }
  } else if (state_.phase == Phase::Pet) {
    state_.pet.elapsed += seconds;
    if (!state_.choice.visible && state_.choice.fade_seconds <= 0 &&
        (state_.pet.reactions >= 3 || state_.pet.elapsed >= kChoiceWait)) {
      state_.choice.visible = true;
      state_.choice.elapsed = 0;
      state_.choice.idle_seconds = 0;
      state_.choice.selected = Decision::None;
      emit(CueId::ChoiceAppear);
    }
    if (state_.choice.visible) {
      state_.choice.elapsed += seconds;
      state_.choice.idle_seconds += seconds;
    }
  } else if (state_.phase == Phase::Bye) {
    const double k = clamp(state_.phase_elapsed / 0.9, 0, 1);
    state_.pet.x = lerp(state_.pet.from_x, state_.pet.to_x, k);
    state_.pet.y = lerp(state_.pet.from_y, state_.pet.to_y, k) - std::sin(k * kPi) * 40;
    state_.pet.scale = lerp(1, 0.32, ease_out_cubic(k));
    if (k >= 1) {
      state_.shelf.push_back(ShelfFriend{state_.egg.creature, state_.pet.to_x, state_.pet.to_y,
                                         state_.pet.to_x, cosmetic_rng_.next() * 6.28, 0, 0, -3});
      new_egg();
      state_.egg.x = 380;
      state_.egg.roll = 0;
      state_.phase = Phase::Roll;
      state_.phase_elapsed = 0;
    }
  } else if (state_.phase == Phase::Roll) {
    const double k = clamp(state_.phase_elapsed / 0.9, 0, 1);
    state_.egg.x = lerp(380, kEggX, ease_out_cubic(k));
    state_.egg.roll = -(state_.egg.x - kEggX) / 50;
    if (k >= 1) {
      state_.egg.x = kEggX;
      state_.egg.roll = 0;
      state_.egg.squash_velocity = -3;
      emit(CueId::Knock, 0);
      start_egg_play();
    }
  }

  if (prompt_was_active && state_.phase == Phase::Egg) {
    state_.prompt_seconds -= seconds;
    if (state_.prompt_seconds <= 0) {
      state_.prompt_seconds = 0;
      emit(CueId::PromptKnock);
    }
  }

  spring(state_.egg.squash, state_.egg.squash_velocity, 0, seconds);
  state_.egg.breath = std::sin(state_.elapsed * 2 * kPi / 2.8) * 0.01;
  if (state_.egg.wobble_seconds > 0) {
    state_.egg.wobble_seconds = std::max(0.0, state_.egg.wobble_seconds - seconds);
    const double k = 1 - state_.egg.wobble_seconds / 0.5;
    const double amplitude = (state_.hint >= 1 ? 0.09 : 0.05);
    state_.egg.angle = std::sin(k * kPi * 4) * amplitude * (1 - k);
  } else {
    state_.egg.angle = 0;
  }
  if (state_.phase == Phase::PreHatch) {
    state_.egg.angle = std::sin(state_.phase_elapsed * 70) * 0.07 * (state_.phase_elapsed / 0.4 + 0.3);
  }
  state_.egg.blink_seconds -= seconds;
  if (state_.egg.blink_seconds < -0.12) state_.egg.blink_seconds = 2 + cosmetic_rng_.next() * 3;

  if (state_.shell.visible) {
    state_.shell.elapsed += seconds;
    state_.shell.top_vy += 600 * seconds;
    state_.shell.top_x += state_.shell.top_vx * seconds;
    state_.shell.top_y += state_.shell.top_vy * seconds;
    state_.shell.top_rotation += state_.shell.top_angular_velocity * seconds;
    const double t = state_.shell.elapsed;
    state_.shell.bottom_y = 30 * ease_out_cubic(clamp((t - 0.4) / 0.8, 0, 1));
    if (t >= 1.3) state_.shell.visible = false;
  }

  if (state_.phase == Phase::Hatch)
    state_.pet.scale = lerp(0.2, 1, ease_out_back(clamp((state_.phase_elapsed - 0.05) / 0.5, 0, 1)));
  else if (state_.phase == Phase::Pet)
    state_.pet.scale = 1;

  if (state_.pet.reaction != Reaction::None) {
    const double was = state_.pet.reaction_seconds;
    state_.pet.reaction_seconds -= seconds;
    if (state_.pet.reaction == Reaction::Jump && state_.pet.reaction_seconds <= 0.55 && was > 0.55)
      state_.pet.squash_velocity = 4;
    if (state_.pet.reaction == Reaction::Jump && state_.pet.reaction_seconds <= 0.12 && was > 0.12)
      state_.pet.squash_velocity = -4;
    if (state_.pet.reaction == Reaction::Sneeze && state_.pet.reaction_seconds <= 0.45 && was > 0.45)
      state_.pet.squash_velocity = -5;
    if (state_.pet.reaction_seconds <= 0) state_.pet.reaction = Reaction::None;
  }
  const double sneeze_in = state_.pet.reaction == Reaction::Sneeze && state_.pet.reaction_seconds > 0.45
      ? 0.1 * clamp((0.8 - state_.pet.reaction_seconds) / 0.35, 0, 1) : 0;
  spring(state_.pet.squash, state_.pet.squash_velocity, sneeze_in, seconds);
  state_.pet.blink_seconds -= seconds;
  if (state_.pet.blink_seconds < -0.12) state_.pet.blink_seconds = 2 + cosmetic_rng_.next() * 4;
  state_.pet.pose_y_offset = 0;
  state_.pet.pose_rotation = 0;
  if (state_.pet.reaction == Reaction::Giggle) {
    state_.pet.pose_rotation = std::sin(state_.pet.reaction_seconds * 40) * 0.12;
  } else if (state_.pet.reaction == Reaction::Jump) {
    const double k = clamp((0.55 - state_.pet.reaction_seconds) / 0.43, 0, 1);
    state_.pet.pose_y_offset = -std::sin(kPi * k) * 26;
  } else if (state_.pet.reaction == Reaction::Sneeze) {
    if (state_.pet.reaction_seconds > 0.45)
      state_.pet.pose_rotation = -0.08 * clamp((0.8 - state_.pet.reaction_seconds) / 0.35, 0, 1);
  }
  if (state_.phase == Phase::Bye) {
    state_.pet.pose_y_offset = 0;
    state_.pet.pose_rotation = std::sin(state_.phase_elapsed * 18) * 0.1;
  }

  for (ShelfFriend& friend_pet : state_.shelf) {
    friend_pet.phase += seconds;
    friend_pet.wave_seconds = std::max(0.0, friend_pet.wave_seconds - seconds);
    spring(friend_pet.squash, friend_pet.squash_velocity, 0, seconds);
    friend_pet.x += (friend_pet.target_x - friend_pet.x) * std::min(seconds * 6, 1.0);
  }
  state_.choice.fade_seconds = std::max(0.0, state_.choice.fade_seconds - seconds);
  state_.badge_happy_seconds = std::max(0.0, state_.badge_happy_seconds - seconds);
  if (state_.phase != Phase::Start && state_.phase != Phase::Intro)
    state_.badge_squash_velocity *= std::pow(0.86, seconds * 60);
}

}  // namespace hatch_a_pet
