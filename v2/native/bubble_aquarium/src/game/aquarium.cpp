#include "game/aquarium.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace aquarium {
namespace {
constexpr float step_s = 0.05f, pi = 3.14159265f;
constexpr float home_x[] = {92, 222, 176}, home_y[] = {122, 158, 104};
constexpr float bed_x[] = {80, 150, 254}, bed_y[] = {178, 182, 166};
constexpr float lane_low[] = {96, 124, 92}, lane_high[] = {160, 186, 150};
constexpr float speed[] = {20, 16, 24}, mouth[] = {25, 26, 33};
constexpr float visitor_y[] = {126, 122, 116}, visitor_half_x[] = {42, 24, 28};
constexpr float visitor_half_y[] = {26, 38, 34};
float distance(float x, float y, float a, float b) { return std::hypot(x - a, y - b); }
float clamp(float v, float a, float b) { return std::max(a, std::min(v, b)); }
bool inside(float x, float y, float a, float b, float w, float h) {
  return x >= a && x < a + w && y >= b && y < b + h;
}
}
Game::Game(Audio& audio, uint32_t seed) : audio_(audio), rng_(seed) { reset(); }
float Game::random() {
  rng_ += 0x6D2B79F5u;
  uint32_t t = (rng_ ^ (rng_ >> 15)) * (rng_ | 1u);
  t = (t + (t ^ (t >> 7)) * (t | 61u)) ^ t;
  return static_cast<float>(t ^ (t >> 14)) / 4294967296.0f;
}
void Game::reset() {
  phase_ = Phase::Start; bed_ = spoken_ = false;
  time_ = accumulator_ = intro_ = idle_ = dim_ = 0;
  burp_ = 6; stream_ = pellet_count_ = bubble_count_ = meals_ = visits_ = pending_visits_ = next_visitor_ = hint_ = 0;
  stream_timer_ = 0;
  for (auto& last : last_button_) last = -9;
  visitor_ = {}; drag_ = {};
  for (int i = 0; i < 3; ++i) {
    fish_[i] = {};
    fish_[i].x = fish_[i].tx = home_x[i];
    fish_[i].y = fish_[i].ty = home_y[i];
    fish_[i].face = i == 1 ? -1 : 1;
    fish_[i].wait = 0.8f + i * 0.5f;
  }
  audio_.bedtime(false);
  audio_.playing(false);
}
void Game::feed() {
  audio_.play(Cue::Feed, pellet_count_ == 6);
  if (pellet_count_ == 6) return;
  float x = 100 + (random() - 0.5f) * 8;
  pellets_[pellet_count_++] = {x, 50, 8 + random() * 40, 20 + random() * 20, 0, random() * 2 * pi};
}
void Game::burst() {
  stream_ = std::min(8, stream_ + 5);
  stream_timer_ = std::min(stream_timer_, 0.1f);
  audio_.play(Cue::Bubble);
}
void Game::spawn_bubble() {
  if (bubble_count_ == 8) {
    for (int i = 1; i < 8; ++i) bubbles_[i - 1] = bubbles_[i];
    --bubble_count_;
  }
  const float r = 18 + random() * 4, x = 220 + (random() - 0.5f) * 16;
  const float speed = 26 + random() * 14, phase = random() * 2 * pi;
  const float wf = 1.2f + random() * 0.8f, wa = 5 + random() * 9;
  bubbles_[bubble_count_++] = {x, 198, x, r, speed, phase, 0, 0.25f,
                               (random() - 0.5f) * 18, wf, wa};
}
int Game::nearest(float x, float y) const {
  int best = -1; float d = 1e9f;
  for (int i = 0; i < 3; ++i) {
    if (fish_[i].mode == Mode::Food || fish_[i].mode == Mode::Gulp || fish_[i].mode == Mode::Loop) continue;
    float v = distance(x, y, fish_[i].x, fish_[i].y);
    if (v < d) { d = v; best = i; }
  }
  return best;
}
void Game::attract(float x, float y, int i) {
  if (i < 0) return;
  Fish& f = fish_[i];
  if (f.mode == Mode::Sleep) { f.peek = 1.4f; return; }
  f.mode = Mode::Visit;
  f.tx = clamp(x, 40, 280); f.ty = clamp(y, 88, 196);
}
void Game::toggle_bed() {
  bed_ = !bed_; idle_ = 0; hint_ = 0; audio_.bedtime(bed_); audio_.play(Cue::Bed, bed_);
  for (auto& f : fish_) {
    if (bed_ && (f.mode == Mode::Swim || f.mode == Mode::Hello || f.mode == Mode::Visit)) {
      f.mode = Mode::Sleep; f.asleep = false;
    } else if (!bed_ && f.mode == Mode::Sleep) { f.mode = Mode::Swim; f.wait = 0.4f; f.asleep = false; }
  }
}
void Game::down(float x, float y) {
  if (phase_ == Phase::Start) {
    reset(); phase_ = Phase::Intro; stream_ = 3; stream_timer_ = 0.3f;
    audio_.playing(true); audio_.play(Cue::Welcome); return;
  }
  if (inside(x, y, 0, 0, 56, 56)) { reset(); return; }
  if (inside(x, y, 264, 0, 56, 56)) return; // disabled mic
  if (phase_ == Phase::Intro) { intro_ = 1.5f; return; }
  idle_ = 0; hint_ = 0; spoken_ = false;
  if (inside(x, y, 72, 0, 56, 56)) {
    if (time_ - last_button_[0] >= 0.08f) { last_button_[0] = time_; feed(); }
    drag_ = {Drag::Kind::Shaker, x, y, x, y}; return;
  }
  if (inside(x, y, 132, 0, 56, 56)) { if (time_ - last_button_[1] >= 0.08f) { last_button_[1] = time_; toggle_bed(); } return; }
  if (inside(x, y, 192, 0, 56, 56)) { if (time_ - last_button_[2] >= 0.08f) { last_button_[2] = time_; burst(); } return; }
  if (y < 56) return;
  for (int i = bubble_count_ - 1; i >= 0; --i) {
    const auto& b = bubbles_[i];
    if (distance(x, y, b.x, b.y) <= b.radius * b.growth + 6) {
      for (int j = i + 1; j < bubble_count_; ++j) bubbles_[j - 1] = bubbles_[j];
      --bubble_count_; audio_.play(Cue::Pop); return;
    }
  }
  if (visitor_.active && std::abs(x - visitor_.x) < visitor_half_x[visitor_.kind] &&
      std::abs(y - visitor_.y) < visitor_half_y[visitor_.kind]) {
    visitor_.wave = 0.8f; audio_.play(Cue::Visitor, visitor_.kind); return;
  }
  const float hx[] = {32, 32, 40}, hy[] = {24, 28, 24};
  for (int i = 2; i >= 0; --i) {
    Fish& f = fish_[i];
    if (std::abs(x - f.x) < hx[i] && std::abs(y - f.y) < hy[i]) {
      if (time_ - f.last_tap >= 0.08f) {
        f.last_tap = time_; f.happy = 0.8f; audio_.play(Cue::Fish, i);
        if (f.mode == Mode::Sleep) f.peek = 1.4f;
        else if (f.mode == Mode::Swim || f.mode == Mode::Visit) { f.mode = Mode::Hello; f.timer = 0.6f; }
      }
      drag_ = {Drag::Kind::Fish, x, y, x, y, 0, i}; return;
    }
  }
  if (inside(x, y, 190, 186, 60, 48)) { burst(); return; }
  audio_.play(Cue::Glass);
  const int i = nearest(x, y); attract(x, y, i);
  drag_ = {Drag::Kind::Glass, x, y, x, y, 0, i};
}
void Game::move(float x, float y) {
  if (drag_.kind == Drag::Kind::None) return;
  const float d = distance(x, y, drag_.x, drag_.y);
  drag_.distance += d; drag_.x = x; drag_.y = y;
  if (drag_.kind == Drag::Kind::Shaker) {
    if (drag_.distance >= 26 && time_ - last_button_[0] >= 0.12f) {
      drag_.distance = 0; last_button_[0] = time_; feed();
    }
    return;
  }
  if (!drag_.active) {
    if (distance(x, y, drag_.start_x, drag_.start_y) < 12) return;
    drag_.active = true;
  }
  if (drag_.fish >= 0) attract(x, y, drag_.fish);
}
void Game::up() { drag_ = {}; }
void Game::choose_target(int i) {
  Fish& f = fish_[i];
  f.tx = hint_ >= 2 ? 84 + random() * 76 : 40 + random() * 240;
  f.ty = hint_ >= 2 ? 92 + random() * 24 :
         visitor_.active ? std::min(184.0f, visitor_y[visitor_.kind] + visitor_half_y[visitor_.kind] + 22) + random() * 10 :
         lane_low[i] + random() * (lane_high[i] - lane_low[i]);
}
void Game::step() {
  if (phase_ == Phase::Intro) { intro_ += step_s; if (intro_ >= 1.5f) { phase_ = Phase::Swim; idle_ = 0; } }
  else {
    idle_ += step_s;
    if (!bed_) {
      if (idle_ > 6 && hint_ == 0) hint_ = 1;
      if (idle_ > 12 && !spoken_) { spoken_ = true; hint_ = 2; audio_.play(Cue::Hint); }
    }
  }
  stream_timer_ -= step_s;
  if (stream_ > 0 && stream_timer_ <= 0) { --stream_; stream_timer_ = 0.3f; spawn_bubble(); }
  burp_ -= step_s;
  if (burp_ <= 0) { stream_ = std::min(8, stream_ + 2); burp_ = (bed_ ? 14 : 9) + random() * 5; }
  for (int i = 0; i < pellet_count_; ++i) {
    auto& p = pellets_[i]; p.age += step_s;
    if (p.mode == 0) {
      p.vy += 15; p.x += p.vx * step_s; p.y += p.vy * step_s;
      if (p.y >= 62) { p.y = 62; p.mode = 1; p.vx *= 0.3f; p.vy *= 0.25f; audio_.play(Cue::Plip); }
    } else if (p.mode == 1) {
      p.vy += (19 - p.vy) * 0.15f; p.vx *= 0.9f;
      p.x += p.vx * step_s + std::sin(p.age * 2.4f + p.phase) * 0.35f;
      p.y += p.vy * step_s;
      if (p.y >= 211) { p.y = 211; p.mode = 2; }
    }
    if (p.mode != 0 && p.eater < 0) {
      int closest = -1; float best = 1e9f;
      for (int f = 0; f < 3; ++f) {
        if (fish_[f].food >= 0 || (drag_.active && drag_.fish == f)) continue;
        Mode mode = fish_[f].mode;
        if (mode != Mode::Swim && mode != Mode::Hello && mode != Mode::Visit && mode != Mode::Sleep) continue;
        float d = distance(p.x, p.y, fish_[f].x, fish_[f].y);
        if (d < best) { best = d; closest = f; }
      }
      if (closest >= 0) {
        p.eater = closest; Fish& f = fish_[closest];
        f.food = i; f.mode = Mode::Food; f.timer = 0; f.asleep = false;
      }
    }
  }
  for (int i = 0; i < 3; ++i) {
    Fish& f = fish_[i];
    f.happy = std::max(0.0f, f.happy - step_s); f.peek = std::max(0.0f, f.peek - step_s);
    f.tail += step_s * (f.asleep ? 2 : 5 + std::hypot(f.vx, f.vy) * 0.1f);
    float tx = f.tx, ty = f.ty, s = speed[i] * (bed_ ? 0.6f : 1.0f);
    if (f.mode == Mode::Swim) {
      f.wait -= step_s;
      if (f.wait <= 0 && distance(f.x, f.y, tx, ty) < 10) { choose_target(i); f.wait = 0.6f + random() * 1.4f; }
      if (f.wait > 0) { tx = f.x; ty = f.y; }
    } else if (f.mode == Mode::Food) {
      f.timer += step_s; s = bed_ ? 34 : 62;
      if (f.food >= 0) {
        Pellet& p = pellets_[f.food];
        if (std::abs(p.x - f.x) > 6) f.face = p.x > f.x ? 1 : -1;
        tx = p.x - f.face * mouth[i]; ty = std::min(196.0f, p.y - 5);
        if (distance(p.x, p.y, f.x + f.face * mouth[i], f.y + 5) < 10) {
          int consumed = f.food; f.food = -1;
          for (int j = consumed + 1; j < pellet_count_; ++j) pellets_[j - 1] = pellets_[j];
          --pellet_count_;
          for (auto& other : fish_) if (other.food > consumed) --other.food;
          f.mode = Mode::Gulp; f.timer = 0.3f; f.happy = 1.2f; audio_.play(Cue::Gulp, i);
          ++meals_;
          if (meals_ % 3 == 0) {
            if (visitor_.active) pending_visits_ = std::min(2, pending_visits_ + 1);
            else {
              visitor_.kind = next_visitor_; next_visitor_ = (next_visitor_ + 1) % 3;
              visitor_.x = visitor_.kind == 1 ? 390 : -70;
              visitor_.y = visitor_y[visitor_.kind]; visitor_.age = 0; visitor_.active = true;
              visitor_.sang = false; ++visits_;
            }
          }
          continue;
        }
      }
      if (f.timer > 8) { if (f.food >= 0) pellets_[f.food].eater = -1; f.food = -1; f.mode = bed_ ? Mode::Sleep : Mode::Swim; }
    } else if (f.mode == Mode::Gulp) {
      f.timer -= step_s; if (f.timer <= 0) {
        if (bed_) { f.mode = Mode::Sleep; f.asleep = false; }
        else if (reduced_) { f.mode = Mode::Hello; f.timer = 0.8f; }
        else { f.mode = Mode::Loop; f.timer = 0; f.lx = f.x; f.ly = f.y; f.loop_up = f.y >= 122; }
        audio_.play(Cue::Happy, i);
      }
      continue;
    } else if (f.mode == Mode::Loop) {
      f.timer += step_s;
      float u = std::min(1.0f, f.timer / 1.6f), a = 2 * pi * u * u * (3 - 2 * u);
      f.x = f.lx + f.face * 18 * std::sin(a);
      f.y = f.ly + (f.loop_up ? -1 : 1) * 18 * (1 - std::cos(a));
      if (u == 1) { f.mode = Mode::Swim; f.wait = 0.3f; }
      continue;
    } else if (f.mode == Mode::Hello) {
      f.timer -= step_s; if (f.timer <= 0) { f.mode = Mode::Swim; f.wait = 0.4f; }
      continue;
    } else if (f.mode == Mode::Sleep) {
      tx = bed_x[i]; ty = bed_y[i]; s = 26;
      if (distance(f.x, f.y, tx, ty) < 8) f.asleep = true;
      if (f.asleep) { tx = f.x; ty = f.y; }
    } else if (f.mode == Mode::Visit) {
      s = 46;
      if (!drag_.active && distance(f.x, f.y, tx, ty) < 8) {
        f.mode = Mode::Hello; f.timer = 0.6f; audio_.play(Cue::Fish, i);
      }
    }
    float dx = tx - f.x, dy = ty - f.y, d = std::hypot(dx, dy);
    float v = s * std::min(1.0f, d / (f.mode == Mode::Food ? 24 : 30));
    float desired_x = d > 0.5f ? dx / d * v : 0;
    float desired_y = d > 0.5f ? dy / d * v : 0;
    if (f.mode == Mode::Swim || f.mode == Mode::Visit || f.mode == Mode::Food) {
      float separation_x = 0, separation_y = 0, group_x = 0, group_y = 0;
      int neighbors = 0;
      for (int j = 0; j < 3; ++j) {
        if (j == i) continue;
        const auto& other = fish_[j];
        const float ax = f.x - other.x, ay = (f.y - other.y) * 1.4f;
        const float gap = std::hypot(ax, ay);
        if (gap > 0.01f && gap < 56) {
          float force = (56 - gap) / 56 * 30;
          separation_x += ax / gap * force; separation_y += ay / gap * force;
        }
        if (gap < 90 && other.mode == Mode::Swim) {
          group_x += other.vx; group_y += other.vy; ++neighbors;
        }
      }
      float social = f.mode == Mode::Food ? 0.35f : 1;
      desired_x += separation_x * social;
      desired_y += separation_y * social;
      if (neighbors && f.mode == Mode::Swim) {
        desired_x += group_x / neighbors * 0.15f;
        desired_y += group_y / neighbors * 0.15f;
      }
      if (visitor_.active && f.mode == Mode::Swim && hint_ < 2) {
        float reach = visitor_half_x[visitor_.kind] + 70;
        float horizontal = std::abs(f.x - visitor_.x);
        float floor = std::min(184.0f, visitor_y[visitor_.kind] + visitor_half_y[visitor_.kind] + 22);
        if (horizontal < reach && f.y < floor)
          desired_y += (floor - f.y) * 1.5f * (1 - horizontal / reach);
      }
    }
    if (f.x < 40) desired_x += (40 - f.x) * 2;
    if (f.x > 280) desired_x -= (f.x - 280) * 2;
    if (f.y < 88) desired_y += (88 - f.y) * 2;
    if (f.y > 202) desired_y -= (f.y - 202) * 2;
    const float rate = f.mode == Mode::Food ? 0.2f : 0.125f;
    f.vx += (desired_x - f.vx) * rate;
    f.vy += (desired_y - f.vy) * rate;
    f.x = clamp(f.x + f.vx * step_s, 20, 300);
    f.y = clamp(f.y + f.vy * step_s, 84, 204);
    if (f.mode == Mode::Swim && std::abs(f.vx) > 6) f.face = f.vx > 0 ? 1 : -1;
  }
  for (int i = 0; i < bubble_count_;) {
    auto& b = bubbles_[i]; b.age += step_s;
    b.growth = std::min(1.0f, b.growth + step_s / 0.4f);
    b.base = clamp(b.base + b.drift * step_s, 34, 286);
    b.y -= b.speed * step_s * (0.4f + 0.6f * b.growth);
    b.x = b.base + std::sin(b.age * b.wobble_speed + b.phase) * b.wobble_width * b.growth;
    if (b.y - b.radius <= 64) {
      for (int j = i + 1; j < bubble_count_; ++j) bubbles_[j - 1] = bubbles_[j];
      --bubble_count_;
    } else ++i;
  }
  if (visitor_.active) {
    auto& v = visitor_; v.age += step_s; v.wave = std::max(0.0f, v.wave - step_s);
    v.x += (v.kind == 1 ? -1 : 1) * 44 * (bed_ ? 0.7f : 1) * step_s;
    v.y = visitor_y[v.kind] + std::sin(v.age * 1.3f) * 6;
    if (!v.sang && v.x > 40 && v.x < 280) { v.sang = true; audio_.play(Cue::Visitor, v.kind); }
    if (v.x > 400 || (v.x < -80 && v.kind == 1)) {
      v.active = false;
      if (pending_visits_ > 0) {
        --pending_visits_; v.kind = next_visitor_; next_visitor_ = (next_visitor_ + 1) % 3;
        v.x = v.kind == 1 ? 390 : -70; v.y = visitor_y[v.kind];
        v.age = 0; v.sang = false; v.active = true; ++visits_;
      }
    }
  }
}
void Game::tick(float seconds) {
  const float dt = clamp(seconds, 0, 0.05f);
  time_ += dt;
  dim_ = clamp(dim_ + (bed_ ? dt : -dt) / 2, 0, 1);
  if (phase_ == Phase::Start) return;
  accumulator_ += dt;
  while (accumulator_ >= step_s - 0.000001f) { accumulator_ -= step_s; step(); }
}
}
