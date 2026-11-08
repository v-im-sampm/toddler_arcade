#pragma once
// Color Pop gameplay, ported from v2/reference/quality_bar.html (see ../../GAME_SPEC.md).
// Portable and deterministic: no LVGL, SDL, audio or hardware dependencies.
#include <cstdint>
#include <vector>

#include "little_arcade/intent.hpp"

namespace color_pop {

inline constexpr int kWidth = 320, kHeight = 240;
inline constexpr double kRadius = 30, kHit = 40;
inline constexpr int kMaxParticles = 96;  // reference TOK.deviceMaxParticles
inline constexpr int kMaxDecor = 64;      // native bound (reference: unbounded)
inline constexpr int kMaxRings = 16;      // native bound (reference: unbounded)

// Reference COLORS keys; values index kPalette.
enum class Color : uint8_t { Red, Yellow, Green, Blue };
struct Palette { uint32_t base, shadow, light; int degree; };
inline constexpr Palette kPalette[4] = {
    {0xEF4B4F, 0xB8323A, 0xFF9A92, 0},
    {0xFFD23F, 0xC99A0E, 0xFFEB9C, 1},
    {0x2DB26E, 0x1E8452, 0x8EE0B4, 2},
    {0x3A6FE8, 0x2448A8, 0x9DB8FA, 4},
};
// Reference NAMES order (used by the shuffle bag and distractor choice).
inline constexpr Color kNames[4] = {Color::Red, Color::Blue, Color::Green, Color::Yellow};
inline constexpr double kSlots[6][2] = {{80, 104}, {160, 100}, {240, 104}, {102, 166}, {182, 170}, {262, 166}};

enum class Phase : uint8_t { Start, Intro, Play, Celebrate };
enum class BubbleState : uint8_t { Rise, Float, Press, Leave, Gone };
enum class Shape : uint8_t { Circle, Star, Rect };

struct Bubble {
  Color color;
  int slot;
  double bx, by, x, y;
  BubbleState state;
  double t, s, sv, sx, sy, rot, wig, glow, alpha, ph, last_tap;
};
struct Particle {
  double x, y, vx, vy, g, life, max;
  Shape shape;
  uint32_t color;
  double size, rot, vr;
  bool additive;
};
struct Ring { double x, y, r0, r1, t, dur; uint32_t color; double w; };
struct Decor { double x, y, r, vy, ph; };
struct Crab {
  double y_off = 40, look_x = 0, look_y = -1, blink = 3, clap = 0, giggle = 0, dance = 0;
  bool pointing = false;
  double point_x = 0, point_y = 0;
};
struct Badge { double sv = 0, happy = 0, last_tap = -9; };

// Mirrors the reference's mulberry32 bit for bit.
class Mulberry32 {
 public:
  explicit Mulberry32(uint32_t seed) : a_(seed) {}
  double next() {
    a_ += 0x6D2B79F5u;
    uint32_t t = (a_ ^ (a_ >> 15)) * (1u | a_);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
  }
 private:
  uint32_t a_;
};

// Sound events in reference terms. `when` is the Web Audio schedule offset.
// motif()/celebrate() carry the reference's setTimeout delay (0.25 s / 0.18 s);
// the score performs the voice-limit check at that later time, like the sketch.
class Sound {
 public:
  virtual ~Sound() = default;
  virtual void start() {}
  virtual void marimba(double freq, double when, double vel, double dur) { (void)freq; (void)when; (void)vel; (void)dur; }
  virtual void plop(double when, double vel, double hi) { (void)when; (void)vel; (void)hi; }
  virtual void motif(Color color, double delay) { (void)color; (void)delay; }
  virtual void celebrate(bool big, double delay) { (void)big; (void)delay; }
};

double degree_hz(int degree, int octave = 0);

class Game {
 public:
  explicit Game(Sound& sound, uint32_t seed = 1234);

  void down(double x, double y);   // pointerdown in 320x240 logical units
  void tick(double dt);            // reference update(dt); callers clamp dt to 0.05
  void intent(little_arcade::Intent intent);  // voice/keyboard alternative to touch
  void home();
  void set_reduced(bool reduced) { reduced_ = reduced; }
  bool reduced() const { return reduced_; }

  Phase phase() const { return phase_; }
  double time() const { return time_; }
  double phase_time() const { return t_; }
  bool has_target() const { return has_target_; }
  Color target() const { return target_; }
  int round() const { return round_; }
  int successes() const { return successes_; }
  int count() const { return count_; }
  int misses() const { return misses_; }
  int hint() const { return hint_; }
  double idle() const { return idle_; }
  int auto_streak() const { return auto_streak_; }
  bool round_auto_help() const { return round_auto_help_; }
  std::size_t pending_respawns() const { return respawns_.size(); }
  const std::vector<Bubble>& bubbles() const { return bubbles_; }
  const std::vector<Particle>& particles() const { return parts_; }
  const std::vector<Ring>& rings() const { return rings_; }
  const std::vector<Decor>& decor() const { return decor_; }
  const Crab& crab() const { return crab_; }
  const Badge& badge() const { return badge_; }
  // Index of the first floating target bubble (helper hand), or -1.
  int helper_bubble() const;

 private:
  struct Respawn { Color color; int slot; double at; };
  struct EmitSpec {
    double sp0, sp1, l0, l1, gravity;
    Shape shape;
    double size;
    const uint32_t* colors;
    int ncolors;
    double up;
    bool additive;
  };
  Color next_target();
  Bubble make_bubble(Color color, int slot, double delay);
  void new_round();
  void start_game();
  void set_hint(int level);
  void pop(Bubble& b);
  void emit(double x, double y, int n, const EmitSpec& o);
  void ring(double x, double y, double r0, double r1, double dur, uint32_t color, double w);
  void spawn_decor(double x, double y, double r, double vy);
  void badge_tap();

  Sound& sound_;
  Mulberry32 grng_, frng_;
  bool reduced_ = false;
  Phase phase_ = Phase::Start;
  double t_ = 0, time_ = 0, idle_ = 0, celebrate_for_ = 0;
  int round_ = 0, successes_ = 0, count_ = 2, misses_ = 0, hint_ = 0, auto_streak_ = 0;
  bool spoken_ = false, round_auto_help_ = false, started_audio_ = false;
  bool has_target_ = false, has_last_ = false;
  Color target_ = Color::Red, last_ = Color::Red;
  std::vector<Color> bag_;
  std::vector<Bubble> bubbles_;
  std::vector<Respawn> respawns_;
  std::vector<Particle> parts_;
  std::vector<Ring> rings_;
  std::vector<Decor> decor_;
  Crab crab_;
  Badge badge_;
};

}  // namespace color_pop
