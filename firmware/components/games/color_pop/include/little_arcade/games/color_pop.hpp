#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

// Color Pop: a line-by-line port of the game logic in
// v2/reference/quality_bar.html (update(), the pointerdown handler, pop(),
// newRound(), the hint ladder and every random draw, in the same order).
// Times are seconds in doubles, like the reference, so a seeded run follows
// the reference frame for frame. The painter (view/color_pop_paint) reads the
// public state and draws it exactly like the reference's draw functions.
namespace little_arcade::color_pop {

// Same order as the reference NAMES array.
enum class Color : uint8_t { RED, BLUE, GREEN, YELLOW };
inline constexpr std::size_t kColorCount = 4;

// Non-colour clue drawn inside each bubble (docs/acceptance.md accessibility).
enum class Pattern : uint8_t { HEART, WAVE, LEAF, STAR };
constexpr Pattern pattern_for(Color c) { return static_cast<Pattern>(static_cast<uint8_t>(c)); }

struct Swatch {
  uint32_t base, shadow, light;
  uint8_t degree;  // pentatonic degree of the colour's success note
};
// Reference COLORS table.
constexpr Swatch swatch(Color c) {
  switch (c) {
    case Color::RED: return {0xEF4B4F, 0xB8323A, 0xFF9A92, 0};
    case Color::YELLOW: return {0xFFD23F, 0xC99A0E, 0xFFEB9C, 1};
    case Color::GREEN: return {0x2DB26E, 0x1E8452, 0x8EE0B4, 2};
    case Color::BLUE: return {0x3A6FE8, 0x2448A8, 0x9DB8FA, 4};
  }
  return {0, 0, 0, 0};
}

enum class Phase : uint8_t { INTRO, PLAY, CELEBRATE };
enum class BubbleState : uint8_t { RISE, FLOAT, PRESS, LEAVE, GONE };

struct Bubble {
  Color color;
  uint8_t slot;
  double bx, by, x, y;
  BubbleState state;
  double t, s, sv, sx, sy, rot, wig, glow, alpha, ph, last_tap;
};

enum class ParticleShape : uint8_t { CIRCLE, STAR, RECT };
struct Particle {
  double x, y, vx, vy, g, life, max;
  ParticleShape shape;
  uint32_t color;
  double size, rot, vr;
  bool add;
};

struct RingFx {
  double x, y, r0, r1, t, dur;
  uint32_t color;
  double w;
};

struct Decor {
  double x, y, r, vy, ph;
};

struct Crab {
  double y_off = 40;
  double look_x = 0, look_y = -1;
  double blink = 3, clap = 0, giggle = 0, dance = 0;
  bool pointing = false;
  double point_x = 0, point_y = 0;
};

struct Badge {
  double sv = 0;  // press impulse
  double happy = 0;
  double last_tap = -9;
};

// mulberry32, bit-exact with the reference.
class Mulberry32 {
 public:
  explicit Mulberry32(uint32_t seed = 1234) : a_(seed) {}
  void seed(uint32_t s) { a_ = s; }
  double next() {
    a_ += 0x6D2B79F5u;
    uint32_t t = (a_ ^ (a_ >> 15)) * (1u | a_);
    t = (t + (t ^ (t >> 7)) * (61u | t)) ^ t;
    return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
  }

 private:
  uint32_t a_;
};

// Feedback cue variants (the Color Pop score in the audio layer).
namespace cue {
inline constexpr uint8_t kOtherPop = 0x10;     // NOTE: | degree; wrong-colour pop
inline constexpr uint8_t kRipple = 0x20;       // NOTE: empty-water tap
inline constexpr uint8_t kSparkleTap = 0x30;   // NOTE: | degree; tap during celebration
inline constexpr uint8_t kCrabGiggle = 0;      // REACTION
}  // namespace cue

class ColorPopGame final : public IGame {
 public:
  static constexpr int W = 320, H = 240;
  static constexpr double R = 30, kHit = 40;
  static constexpr std::size_t kSlotCount = 6;
  static constexpr double kSlots[kSlotCount][2] = {{80, 104}, {160, 100}, {240, 104},
                                                  {102, 166}, {182, 170}, {262, 166}};
  static constexpr Rect kTargetBadge{132, 4, 56, 56};
  static constexpr Rect kCrab{4, 186, 76, 54};
  static constexpr std::size_t kMaxBubbles = 8;
  static constexpr std::size_t kMaxParticles = 96;  // TOK.deviceMaxParticles
  static constexpr std::size_t kMaxRings = 32;
  static constexpr std::size_t kMaxDecor = 48;
  static constexpr std::size_t kMaxRespawns = 8;

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override;

  // Deterministic stepping (tests and the reference-diff harness). The
  // reference clamps dt to 0.05 s.
  void step(double dt);
  void pointer_down(double x, double y);
  // The reference decays the badge impulse inside drawBadge(); call after
  // each painted frame.
  void frame_drawn();
  // Overrides the gameplay/cosmetic RNG seeds (reference: grng = seed 1234,
  // frng = Math.random).
  void seed(uint32_t gameplay, uint32_t cosmetic) {
    grng_.seed(gameplay);
    frng_.seed(cosmetic);
  }

  bool active() const { return ctx_ != nullptr; }
  Phase phase() const { return phase_; }
  double time() const { return time_; }
  double phase_time() const { return phase_t_; }
  bool has_target() const { return has_target_; }
  Color target() const { return target_; }
  int hint() const { return hint_; }
  uint32_t rounds_completed() const { return successes_; }
  int bubble_target_count() const { return count_; }

  std::size_t bubble_count() const { return bubble_n_; }
  const Bubble& bubble(std::size_t i) const { return bubbles_[i]; }
  std::size_t particle_count() const { return part_n_; }
  const Particle& particle(std::size_t i) const { return parts_[i]; }
  std::size_t ring_count() const { return ring_n_; }
  const RingFx& ring(std::size_t i) const { return rings_[i]; }
  std::size_t decor_count() const { return decor_n_; }
  const Decor& decor(std::size_t i) const { return decor_[i]; }
  const Crab& crab() const { return crab_; }
  const Badge& badge() const { return badge_; }
  bool voice_enabled() const { return ctx_ != nullptr && ctx_->voice_enabled; }

 private:
  struct EmitOpts {
    double speed0, speed1, life0, life1, gravity;
    ParticleShape shape;
    double size, up;
    const uint32_t* colors;
    int color_count;
    bool additive;
  };
  struct Respawn {
    Color color;
    uint8_t slot;
    double at;
  };
  struct Timer {
    double at;
    Cue cue;
    uint8_t variant;  // PROMPT: resolved to the target when it fires
  };

  void play(Cue c, uint8_t variant);
  void schedule(double delay, Cue c, uint8_t variant);
  void emit(double x, double y, int n, const EmitOpts& o);
  void add_ring(double x, double y, double r0, double r1, double dur, uint32_t color, double w);
  void spawn_decor(double x, double y, double r, double vy);
  Bubble make_bubble(Color c, uint8_t slot, double delay);
  Color next_target();
  void new_round();
  void set_hint(int level);
  void pop(Bubble& b);
  void press_color(Color c);
  void replay_prompt();

  GameContext* ctx_ = nullptr;
  Mulberry32 grng_{1234};
  Mulberry32 frng_{1};

  Phase phase_ = Phase::INTRO;
  double time_ = 0, phase_t_ = 0, last_dt_ = 0;
  uint32_t round_ = 0, successes_ = 0;
  int count_ = 2;
  bool has_target_ = false, has_last_ = false;
  Color target_ = Color::RED, last_ = Color::RED;
  Color bag_[kColorCount]{};
  int bag_n_ = 0;
  int misses_ = 0, hint_ = 0;
  double idle_ = 0;
  bool spoken_ = false, round_auto_help_ = false;
  int auto_streak_ = 0;
  double celebrate_for_ = 0;

  Bubble bubbles_[kMaxBubbles]{};
  std::size_t bubble_n_ = 0;
  Respawn respawns_[kMaxRespawns]{};
  std::size_t respawn_n_ = 0;
  Particle parts_[kMaxParticles]{};
  std::size_t part_n_ = 0;
  RingFx rings_[kMaxRings]{};
  std::size_t ring_n_ = 0;
  Decor decor_[kMaxDecor]{};
  std::size_t decor_n_ = 0;
  Timer timers_[4]{};
  std::size_t timer_n_ = 0;
  Crab crab_{};
  Badge badge_{};
};

}  // namespace little_arcade::color_pop
