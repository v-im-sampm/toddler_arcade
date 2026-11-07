#pragma once
#include <array>
#include <cstdint>

namespace aquarium {
enum class Phase { Start, Intro, Swim };
enum class Mode { Swim, Food, Gulp, Loop, Visit, Hello, Sleep };
enum class Cue { Welcome, Feed, Plip, Gulp, Happy, Bubble, Pop, Glass, Fish, Visitor, Bed, Hint };
struct Audio {
  virtual ~Audio() = default;
  virtual void play(Cue cue, int variant = 0) = 0;
  virtual void bedtime(bool on) = 0;
  virtual void playing(bool on) = 0;
};
struct Fish {
  float x, y, vx = 0, vy = 0, tx, ty, lx = 0, ly = 0;
  int face = 1, food = -1;
  Mode mode = Mode::Swim;
  float wait = 0, timer = 0, happy = 0, peek = 0, tail = 0, last_tap = -9;
  bool asleep = false, loop_up = true;
};
struct Pellet {
  float x, y = 50, vx, vy, age = 0, phase = 0;
  int eater = -1, mode = 0; // 0 falling, 1 sinking, 2 resting
};
struct Bubble {
  float x, y = 198, base, radius, speed, phase, age = 0, growth = 0.25f, drift, wobble_speed, wobble_width;
};
struct Visitor {
  int kind = 0;
  float x = 0, y = 126, age = 0, wave = 0;
  bool active = false, sang = false;
};
struct Drag {
  enum class Kind { None, Shaker, Fish, Glass } kind = Kind::None;
  float x = 0, y = 0, start_x = 0, start_y = 0, distance = 0;
  int fish = -1;
  bool active = false;
};
class Game {
public:
  explicit Game(Audio& audio, uint32_t seed = 1234);
  void down(float x, float y);
  void move(float x, float y);
  void up();
  void tick(float seconds);
  void reset();
  void set_reduced(bool on) { reduced_ = on; }
  bool reduced() const { return reduced_; }
  Phase phase() const { return phase_; }
  bool bed() const { return bed_; }
  float dim() const { return dim_; }
  float time() const { return time_; }
  int meals() const { return meals_; }
  int visits() const { return visits_; }
  int hint() const { return hint_; }
  int pellet_count() const { return pellet_count_; }
  int bubble_count() const { return bubble_count_; }
  const std::array<Fish, 3>& fish() const { return fish_; }
  const std::array<Pellet, 6>& pellets() const { return pellets_; }
  const std::array<Bubble, 8>& bubbles() const { return bubbles_; }
  const Visitor& visitor() const { return visitor_; }
private:
  float random();
  void step();
  void feed();
  void burst();
  void spawn_bubble();
  void choose_target(int i);
  void toggle_bed();
  void attract(float x, float y, int i);
  int nearest(float x, float y) const;
  Audio& audio_;
  uint32_t rng_;
  std::array<Fish, 3> fish_{};
  std::array<Pellet, 6> pellets_{};
  std::array<Bubble, 8> bubbles_{};
  Visitor visitor_{};
  Drag drag_{};
  Phase phase_ = Phase::Start;
  bool bed_ = false, reduced_ = false, spoken_ = false;
  float time_ = 0, accumulator_ = 0, intro_ = 0, idle_ = 0, dim_ = 0;
  float stream_timer_ = 0, burp_ = 6;
  float last_button_[3] = {-9, -9, -9};
  int stream_ = 0, pellet_count_ = 0, bubble_count_ = 0, meals_ = 0;
  int visits_ = 0, pending_visits_ = 0, next_visitor_ = 0, hint_ = 0;
};
}
