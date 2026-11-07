#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

// Hatch-a-Pet: a line-by-line port of the game logic in
// v2/reference/hatch_a_pet.html (update(), the pointerdown handler, knock(),
// hatch(), react(), chooseYes()/chooseNo(), newEgg(), nextCreature(), the
// hint ladder and every random draw, in the same order). Clocks and timers
// that gate phase changes are seconds in doubles, like the reference, so a
// seeded run follows the reference frame for frame; positions, springs,
// particles and trig are single precision (the ESP32-S3 FPU has no doubles,
// software doubles are slow). The
// painter (view/hatch_a_pet_paint) reads the public state and draws it
// exactly like the reference's draw functions.
namespace little_arcade::hatch_a_pet {

// Original creatures, same order as the reference CREATURES array.
enum class Creature : uint8_t {
  BLOOP,       // round teal blob with one curly antenna
  FLUFFWING,   // peach puffball with two tiny wings
  SPROUTLING,  // green bean body with a leaf on top
  ZIGZAG,      // purple lizard-ish critter with a zigzag tail and spikes
  PUDDLEPUP,   // blue droplet puppy with floppy ears
  GLIMMERBUG,  // yellow beetle with a dotted shell
  ROLYPOLE,    // orange striped ball
  SNORKLE,     // pink critter with a long curly snout
};
inline constexpr std::size_t kCreatureCount = 8;

// Egg decoration (reference PATTERNS).
enum class EggPattern : uint8_t { SPOTS, STRIPES, ZIGZAGS, STARS };
inline constexpr std::size_t kEggPatternCount = 4;

struct Swatch {
  uint32_t base, shadow, light;
  uint8_t degree;  // chirp pitch: F major pentatonic degree
};
// Reference CREATURES table.
constexpr Swatch swatch(Creature c) {
  switch (c) {
    case Creature::BLOOP: return {0x2EC4B6, 0x1A8F86, 0x9EEBE3, 0};
    case Creature::FLUFFWING: return {0xFFAA80, 0xD9774D, 0xFFD9C2, 1};
    case Creature::SPROUTLING: return {0x5BC85B, 0x3A9442, 0xB5EBA5, 2};
    case Creature::ZIGZAG: return {0x9B6BE8, 0x6A42B8, 0xCBB0FA, 3};
    case Creature::PUDDLEPUP: return {0x4D9BF0, 0x2E6CC0, 0xA9D2FB, 4};
    case Creature::GLIMMERBUG: return {0xFFD23F, 0xC99A0E, 0xFFEB9C, 5};
    case Creature::ROLYPOLE: return {0xFF8A2A, 0xC2621B, 0xFFC08A, 6};
    case Creature::SNORKLE: return {0xF06BB8, 0xB8448A, 0xFFB3DD, 7};
  }
  return {0, 0, 0, 0};
}

// START is the reference's tap-to-start overlay (the device skips it: the
// launcher tap already started audio). The rest follow the reference.
enum class Phase : uint8_t { START, INTRO, EGG, PRE_HATCH, HATCH, PET, BYE, ROLL };

// egg: q/qv = squash spring (q > 0 stretches: sx = 1 - q, sy = 1 + q; pivot
// at the egg bottom); ang = rock angle.
struct Egg {
  Creature creature = Creature::BLOOP;
  EggPattern pattern = EggPattern::SPOTS;
  int need = 3, cracks = 0;
  float q = 0, qv = 0, ang = 0;
  double wob = 0, next_wob = 3;
  float x = 160, y_off = 0, roll = 0;
  bool landed = false;
  double last_tap = -9;
  float look_x = 0, look_y = 1;
  double blink = 3;
};

// pet: react = active reaction (0 giggle, 1 jump, 2 sneeze, -1 none), rt = its
// time left.
struct Pet {
  float x = 160, y = 150, s = 0, q = 0, qv = 0;
  int react = -1;
  double rt = 0;
  int next = 0, reacts = 0;
  double t = 0;
  float look_x = 0, look_y = 1;
  double blink = 2, last_tap = -9;
  float from_x = 0, from_y = 0, to_x = 0, to_y = 0;
};

struct Halves {
  bool on = false;
  double t = 0;
  float top_x = 0, top_y = 0, top_vx = 0, top_vy = 0, top_a = 0, top_va = 0;
  float bot_y = 0;
};

struct Choices {
  bool show = false;
  double t = 0, out = 0, idle = 0;
};

struct Badge {
  float sv = 0;  // press impulse
  double happy = 0;
  double last_tap = -9;
};

// A friend on the shelf (session-only, RAM).
struct Friend {
  Creature c;
  float x, y, tx, ph, wave, q, qv;
};

enum class ParticleShape : uint8_t { CIRCLE, STAR, RECT, HEART };
struct Particle {
  float x, y, vx, vy, g, life, max;
  ParticleShape shape;
  uint32_t color;
  float size, rot, vr;
  bool add;
};

struct RingFx {
  float x, y, r0, r1, t, dur;
  uint32_t color;
  float w;
};

// Dandelion fluff drifting by.
struct Decor {
  float x, y, vx, ph, r;
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

// Feedback cue variants, played by the Hatch-a-Pet score in the audio layer
// (audio/hatch_a_pet_score). The reference's setTimeout() audio delays are
// offsets inside the score, not game state.
//   Cue::AMBIENT 0   startGame(): master ramp-in + startMusic() (lullaby)
//   Cue::SUCCESS 0   hatchSound()
//   Cue::REACTION    kind | creature (below)
//   Cue::NOTE        variant (below)
namespace cue {
inline constexpr uint8_t kGiggle = 0x00;     // REACTION | c: reactSound(0, c)
inline constexpr uint8_t kJump = 0x10;       // REACTION | c: reactSound(1, c)
inline constexpr uint8_t kSneeze = 0x20;     // REACTION | c: reactSound(2, c)
inline constexpr uint8_t kNameChirp = 0x30;  // REACTION | c: nameChirp(c)
inline constexpr uint8_t kBye = 0x40;        // REACTION | c: byeSound(c)
inline constexpr uint8_t kKnock = 0x10;            // NOTE | n: knockSound(n)
inline constexpr uint8_t kKnockKnock = 0x20;       // NOTE: knockKnock()
inline constexpr uint8_t kKnockKnockLater = 0x21;  // NOTE: setTimeout(knockKnock, 200)
inline constexpr uint8_t kTrill = 0x30;            // NOTE: trill()
inline constexpr uint8_t kLand = 0x40;             // NOTE: woodblock(degFreq(0, -1), 0, 0.5)
inline constexpr uint8_t kIntroPlop = 0x41;        // NOTE: plop(0, 0.6, 0.6)
inline constexpr uint8_t kChoicePlop = 0x42;       // NOTE: plop(0, 0.8, 1.2)
inline constexpr uint8_t kChoice = 0x43;           // NOTE: choiceSound()
inline constexpr uint8_t kTap = 0x44;              // NOTE: marimba(degFreq(0, 1), 0, 0.3, 0.25)
inline constexpr uint8_t kShelf = 0x50;            // NOTE | c: chirp(degFreq(deg, 1), 0, 0.35)
inline constexpr uint8_t kBird = 0x60;             // NOTE | q (0..15): birdSound(), v = 0.1 + q / 15 * 0.05
}  // namespace cue

class HatchAPetGame final : public IGame {
 public:
  static constexpr int W = 320, H = 240;
  // Reference geometry.
  static constexpr float kEggX = 160, kEggY = 138, kEggRx = 50, kEggRy = 64;
  static constexpr float kNestX = 160, kNestY = 198, kNestRx = 64;
  static constexpr float kPetX = 160, kPetY = 150, kPetHit = 52;
  static constexpr float kNoX = 48, kNoY = 168, kYesX = 272, kYesY = 168, kChoiceR = 28, kChoiceHit = 38;
  static constexpr std::size_t kShelfSlots = 6;
  static constexpr float kShelf[kShelfSlots][2] = {{24, 221}, {54, 221}, {84, 221}, {236, 221}, {266, 221}, {296, 221}};
  static constexpr float kMini = 0.32f;
  static constexpr float kCrackY = -14;  // egg-local crack baseline
  static constexpr uint8_t kMinTaps = 3, kMaxTaps = 5;
  // Touch targets (reference hit areas, as rects for players and tests).
  static constexpr Rect kEggHit{84, 64, 152, 150};
  static constexpr Rect kPromptBadge{132, 4, 56, 56};
  static constexpr Rect kCreature{136, 126, 48, 48};  // inside the 52 px pet radius
  static constexpr Rect kNoButton{28, 148, 40, 40};   // inside the 38 px choice radius
  static constexpr Rect kYesButton{252, 148, 40, 40};
  static constexpr std::size_t kMaxParticles = 96;  // TOK.deviceMaxParticles
  static constexpr std::size_t kMaxRings = 32;
  static constexpr std::size_t kMaxDecor = 8;

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
  // Fresh reference page state: the START overlay, audio not started yet.
  // The reference-diff harness starts here; enter() skips START.
  void reset_to_start();

  bool active() const { return ctx_ != nullptr; }
  Phase phase() const { return phase_; }
  double time() const { return time_; }
  double phase_time() const { return t_; }
  int hint() const { return hint_; }
  double idle() const { return idle_; }
  uint32_t hatch_count() const { return hatches_; }
  const Egg& egg() const { return egg_; }
  const Pet& pet() const { return pet_; }
  const Halves& halves() const { return halves_; }
  const Choices& choices() const { return choices_; }
  const Badge& badge() const { return badge_; }
  std::size_t shelf_count() const { return shelf_n_; }
  const Friend& shelf(std::size_t i) const { return shelf_[i]; }
  std::size_t particle_count() const { return part_n_; }
  const Particle& particle(std::size_t i) const { return parts_[i]; }
  std::size_t ring_count() const { return ring_n_; }
  const RingFx& ring(std::size_t i) const { return rings_[i]; }
  std::size_t decor_count() const { return decor_n_; }
  const Decor& decor(std::size_t i) const { return decor_[i]; }
  bool voice_enabled() const { return ctx_ != nullptr && ctx_->voice_enabled; }

 private:
  struct EmitOpts {
    float speed0, speed1, life0, life1, gravity;
    ParticleShape shape;
    float size, up;
    const uint32_t* colors;
    int color_count;
    bool additive;
  };

  void play(Cue c, uint8_t variant);
  void emit(float x, float y, int n, const EmitOpts& o);
  void add_ring(float x, float y, float r0, float r1, float dur, uint32_t color, float w);
  void spawn_fluff(float x, float y);
  Creature next_creature();
  void new_egg();
  void start_game();
  void start_egg_play();
  void home();
  void knock();
  void hatch();
  void react();
  void choose_yes();
  void choose_no();
  void tap_badge();

  GameContext* ctx_ = nullptr;
  Mulberry32 grng_{1234};
  Mulberry32 frng_{1};
  bool audio_ = false;  // reference `ac` (created by startGame)

  Phase phase_ = Phase::START;
  double time_ = 0, t_ = 0, last_dt_ = 0;
  Creature bag_[kCreatureCount]{};
  int bag_n_ = 0;
  int last_ = -1;
  uint32_t hatches_ = 0;
  double idle_ = 0;
  int hint_ = 0;
  bool spoken_ = false;

  Egg egg_{};
  Pet pet_{};
  Halves halves_{};
  Choices choices_{};
  Badge badge_{};
  Friend shelf_[kShelfSlots]{};
  std::size_t shelf_n_ = 0;
  Particle parts_[kMaxParticles]{};
  std::size_t part_n_ = 0;
  RingFx rings_[kMaxRings]{};
  std::size_t ring_n_ = 0;
  Decor decor_[kMaxDecor]{};
  std::size_t decor_n_ = 0;
};

}  // namespace little_arcade::hatch_a_pet
