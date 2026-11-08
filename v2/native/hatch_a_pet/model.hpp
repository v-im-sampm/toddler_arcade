#pragma once

// Portable Hatch-a-Pet gameplay model. The LVGL view owns drawing and audio;
// this model owns reference-timed state transitions and deterministic choices.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace hatch_a_pet {

inline constexpr int kStageWidth = 320;
inline constexpr int kStageHeight = 240;
inline constexpr std::size_t kMaxQueuedCues = 32;
inline constexpr std::array<double, 4> kEggHitRect{{84, 64, 152, 150}};
inline constexpr std::array<std::array<double, 2>, 2> kChoiceCenters{{{{48, 168}}, {{272, 168}}}};
inline constexpr double kChoiceHitRadius = 38;
inline constexpr std::array<std::array<double, 2>, 6> kShelfSlots{{
    {{24, 221}}, {{54, 221}}, {{84, 221}}, {{236, 221}}, {{266, 221}}, {{296, 221}}
}};

enum class Phase : uint8_t { Start, Intro, Egg, PreHatch, Hatch, Pet, Bye, Roll };
enum class Creature : uint8_t {
  Bloop, Fluffwing, Sproutling, Zigzag, Puddlepup, Glimmerbug, RolyPole, Snorkle
};
enum class Pattern : uint8_t { Spots, Stripes, Zigzags, Stars };
enum class Reaction : int8_t { None = -1, Giggle = 0, Jump = 1, Sneeze = 2 };
enum class Decision : uint8_t { None, AnotherEgg, Stay };
enum class Command : uint8_t { Help, Again, Yes, No };
enum class TouchTarget : uint8_t {
  None, Start, Home, Mic, IntroSkip, Badge, Egg, Pet, ChoiceNo, ChoiceYes,
  Shelf, Empty, Blocked
};

enum class CueId : uint8_t {
  MusicStart, MusicStop, EggLand, PromptKnock, EggHint, Knock, PreHatchTrill,
  HatchPop, CreatureChirp, PetGiggle, PetJump, PetSneeze, ChoiceAppear,
  ChoiceYes, ChoiceNo, WaveBye, ShelfWave, EmptyTap
};

struct CueEvent {
  CueId id = CueId::EmptyTap;
  int variant = 0;  // Knock number, creature index, or reaction-specific detail.
  friend bool operator==(const CueEvent& a, const CueEvent& b) {
    return a.id == b.id && a.variant == b.variant;
  }
};

std::string_view cue_name(CueId cue);
std::string_view creature_name(Creature creature);

// Unsigned arithmetic intentionally wraps to match the reference's mulberry32.
class Mulberry32 {
 public:
  explicit Mulberry32(uint32_t seed) : state_(seed) {}
  double next();

 private:
  uint32_t state_;
};

struct EggView {
  Creature creature = Creature::Bloop;
  Pattern pattern = Pattern::Spots;
  int need = 3;
  int cracks = 0;
  double x = 160, y_offset = 0, roll = 0;
  double squash = 0, squash_velocity = 0, angle = 0;
  double wobble_seconds = 0, wobble_in = 3, breath = 0;
  double look_x = 0, look_y = 1, blink_seconds = 3;
  bool landed = false;
};

struct PetView {
  double x = 160, y = 150, scale = 0, squash = 0, squash_velocity = 0;
  double reaction_seconds = 0, elapsed = 0;
  double pose_y_offset = 0, pose_rotation = 0;
  Reaction reaction = Reaction::None;
  int reactions = 0;
  int next_reaction = 0;
  double look_x = 0, look_y = 1, blink_seconds = 2;
  double from_x = 160, from_y = 150, to_x = 160, to_y = 150;
};

struct ShellView {
  bool visible = false;
  double elapsed = 0;
  double top_x = 0, top_y = 0, top_vx = 70, top_vy = -260;
  double top_rotation = 0, top_angular_velocity = 5;
  double bottom_y = 0;
};

struct ChoiceView {
  bool visible = false;
  double elapsed = 0, fade_seconds = 0, idle_seconds = 0;
  Decision selected = Decision::None;
};

struct ShelfFriend {
  Creature creature = Creature::Bloop;
  double x = 0, y = 221, target_x = 0;
  double phase = 0, wave_seconds = 0;
  double squash = 0, squash_velocity = 0;
};

struct TouchFeedback {
  uint64_t serial = 0;
  uint64_t release_serial = 0;
  double x = 0, y = 0;
  TouchTarget target = TouchTarget::None;
  TouchTarget released_target = TouchTarget::None;
  bool down = false;
};

struct State {
  Phase phase = Phase::Start;
  double elapsed = 0, phase_elapsed = 0, idle_seconds = 0;
  int hint = 0;
  int hatches = 0;
  bool spoken_hint = false;
  double prompt_seconds = 0;
  double badge_squash_velocity = 0, badge_happy_seconds = 0;
  EggView egg{};
  PetView pet{};
  ShellView shell{};
  ChoiceView choice{};
  std::vector<ShelfFriend> shelf;
  TouchFeedback touch{};
};

class Model {
 public:
  explicit Model(uint32_t seed = 1234);

  // Pointer-down performs the reference's pointerdown action immediately.
  // Pointer-up only ends the press state; it never repeats the action.
  void pointer_down(double x, double y);
  void pointer_up();
  void pointer_cancel() { pointer_up(); }
  void tick(double seconds);
  void home();
  bool command(Command command);  // Voice/keyboard adapter; same decision handlers as touch.

  const State& state() const { return state_; }
  bool pop_cue(CueEvent& cue);  // Allocation-free path for a renderer/audio queue.
  std::vector<CueEvent> take_cues();

 private:
  Creature next_creature();
  void new_egg();
  void start_game();
  void start_egg_play();
  void knock();
  void hatch();
  void react();
  void choose_yes();
  void choose_no();
  void badge_tap();
  void emit(CueId id, int variant = 0);

  State state_{};
  Mulberry32 gameplay_rng_;
  Mulberry32 cosmetic_rng_;
  std::vector<Creature> bag_;
  int last_creature_ = -1;
  double last_egg_tap_ = -9, last_pet_tap_ = -9;
  std::vector<CueEvent> cues_;
};

}  // namespace hatch_a_pet
