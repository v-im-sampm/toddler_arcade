#pragma once
// Sound Jar gameplay, ported from v2/reference/sound_jar.html (see ../../GAME_SPEC.md).
// Portable and deterministic: no LVGL, SDL, audio or hardware dependencies. Gameplay randomness
// (take order) comes from grng = mulberry32(seed); cosmetic randomness (particles, blinks) from a
// separate frng = mulberry32(seed ^ 0x9E3779B9), so taps never perturb the take order.
#include <cstdint>
#include <vector>

#include "game/takes.hpp"
#include "little_arcade/intent.hpp"

namespace sound_jar {

inline constexpr int kWidth = 320, kHeight = 240;
inline constexpr int kMaxParticles = 96;  // reference TOK.deviceMaxParticles
inline constexpr int kMaxRings = 24;      // reference ring()
inline constexpr int kMaxLiveVoices = 4;  // reference startVoice(): steal the oldest live voice

// tokens.json hue triples [light, base, shadow].
struct Hue { uint32_t c[3]; };
inline constexpr Hue kRed{{0xFF9A92, 0xEF4B4F, 0xB8323A}}, kYellow{{0xFFEB9C, 0xFFD23F, 0xC99A0E}},
    kGreen{{0x8EE0B4, 0x2DB26E, 0x1E8452}}, kTeal{{0x8FE3DE, 0x21B3B0, 0x127A7D}}, kSky{{0xA9DBFB, 0x4FB0F0, 0x2B79B8}},
    kBlue{{0x9DB8FA, 0x3A6FE8, 0x2448A8}}, kPurple{{0xC9AEF5, 0x8F5BE0, 0x6236A6}}, kPink{{0xFBB6DC, 0xF06BB8, 0xB23E86}},
    kCream{{0xFFFBEF, 0xFFF3D6, 0xD9C7A3}};
inline constexpr uint32_t kWhite = 0xFFFFFF, kHintGlow = 0xFFE066, kInk = 0x3B3550;

enum class PadShape : uint8_t { Circle, Square, Triangle, Heart, Star };
struct PadDef { int semitones; PadShape shape; Hue hue; double s; };
inline constexpr PadDef kPads[5] = {
    {-12, PadShape::Circle, kPurple, 1.00}, {-5, PadShape::Square, kBlue, 0.90}, {0, PadShape::Triangle, kTeal, 0.81},
    {7, PadShape::Heart, kPink, 0.73},      {12, PadShape::Star, kYellow, 0.66},
};
enum Fx : int { kBack = 0, kEcho = 1, kRobot = 2, kWobble = 3 };
struct FxDef { Hue hue; double x; };
inline constexpr FxDef kFx[4] = {{kSky, 82}, {kTeal, 134}, {kBlue, 186}, {kGreen, 238}};

enum class Mote : uint8_t { Bubble, Heart, Star, Dot };
struct TakeStyle { Hue hue; Mote mote; };
inline constexpr int kFrog = 0, kWhisper = 4;
inline constexpr TakeStyle kTakes[kTakeCount] = {
    {kGreen, Mote::Bubble}, {kPink, Mote::Heart}, {kYellow, Mote::Star}, {kSky, Mote::Dot}, {kCream, Mote::Dot}};

// Stage geometry (logical px).
inline constexpr double kJarX = 160, kJarY = 120, kLidX = 160, kLidY = 70, kLidRestX = 102, kLidRestY = 128,
                        kLidRestA = -1.25, kRecX = 268, kRecY = 128, kRecR = 28, kBinX = 44, kBinY = 128, kLedge = 157,
                        kPadFoot = 232, kPadR = 27, kFxY = 30, kFxR = 21;
inline double pad_x(int i) { return 32 + 64 * i; }
inline double pad_y(int i) { return kPadFoot - kPadR * kPads[i].s; }

// Ambience: [landing time on st.amb, lane, degree] on a 14 s loop.
struct Drip { double t; int lane, degree; };
inline constexpr Drip kDrips[6] = {{1.8, 0, 2}, {4.2, 1, 4}, {6.1, 0, 0}, {8.7, 1, 3}, {10.9, 0, 1}, {12.6, 1, 4}};
inline constexpr double kAmbLoop = 14, kAmbAhead = 0.12, kAmbG = 900;
struct Lane { double x, tip, land; };
inline constexpr Lane kLanes[2] = {{91, 55, kLedge}, {232, 55, kLedge}};

enum class Phase : uint8_t { Start, Intro, Empty, Countdown, Recording, Processing, Full, TooQuiet };
enum class Shape : uint8_t { Circle, Star, Rect, Note };

struct Particle {
  double x, y, vx, vy, g, life, max;
  Shape shape;
  uint32_t color;
  double size, rot, vr;
  bool additive;
};
struct Ring { double x, y, r0, r1, t, dur; uint32_t color; double w, a; };

// Pressable objects share the reference's spring + debounce fields.
struct Body { double q = 0, qv = 0, last_tap = -9, wig = 0; };
struct Jar : Body {
  int sound = kFrog, mist = kFrog;
  double level = 0.38, level_to = 0.38;
  bool has_next = false;
  int next_mist = 0;
  double next_level = 0;
  double ang = 0, happy = 0, hmm = 0, look_x = 0, look_y = 1, blink = 2.5, visor = 0, visor_to = 0, mouth = 0,
         swirl = 0, hum = 0, wob_a = 0;
};
struct Lid { double u = 0, to = 0; };
struct Pad : Body { double blink = 0, glow = 0, hop = 9, sing = 0, bend = 0; bool awake = false; };
struct FxButton : Body { double on = 0; };
struct RecButton : Body { double louder = 0; };
struct Bin : Body { double lid = 0; };

// JS mirror of one sampler voice (drives the mouth, pad glow, loop dots and hints).
struct Voice {
  uint32_t id;
  int pad;  // -1 = the jar itself
  int take;
  double pos, dur;
  bool loop;
  double rate, bend;
  bool rel;
  double fade;
  bool rev;
};
struct Hold { int pad; int pointer; double y0, t; uint32_t voice; double bend; bool looped; };
enum class Target : uint8_t { None, Record, Pad, Fx };
struct Suggestion { Target kind; int index; double x, y, r; };

// Cue sounds in reference terms (each is one reference function).
enum class Cue : uint8_t { TapNote, PadNote, LidPop, LidShut, SoftShut, Tada, Hmm, Hint, FxOn, FxOff, FxBump, Wake, Whoosh };

// Audio events. The game applies the reference's phase gate (voiceOk(): silent while
// RECORDING) before calling cue()/drip(); the score applies the 8-voice limit and ducking.
class Sound {
 public:
  virtual ~Sound() = default;
  virtual void start() {}  // initAudio(): master ramps 0 -> 0.35
  virtual void start_music() {}
  virtual void stop_music() {}
  virtual void cue(Cue cue, int arg) { (void)cue; (void)arg; }
  virtual void voice_start(uint32_t id, int take, double rate, bool reverse, bool loop) { (void)id; (void)take; (void)rate; (void)reverse; (void)loop; }
  virtual void voice_release(uint32_t id, double tau) { (void)id; (void)tau; }
  virtual void voice_bend(uint32_t id, double semitones) { (void)id; (void)semitones; }
  virtual void voice_loop(uint32_t id) { (void)id; }
  virtual void apply_fx(bool echo, bool robot, bool wobble) { (void)echo; (void)robot; (void)wobble; }
  virtual void duck(double seconds) { (void)seconds; }
  virtual void drip(double delay, int degree) { (void)delay; (void)degree; }
};

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

double degree_hz(int degree, int octave = 0);  // reference degFreq()

class Game {
 public:
  explicit Game(Sound& sound, uint32_t seed = 1234);

  // Pointer events in 320x240 logical units (mouse == touch). `pointer` distinguishes touches.
  void down(double x, double y, int pointer = 0);
  void move(double x, double y, int pointer = 0);
  void up(int pointer = 0);
  void tick(double dt);  // reference update(dt) + musicTick(); callers clamp dt to 0.05
  void intent(little_arcade::Intent intent);
  void home();
  void set_reduced(bool reduced) { reduced_ = reduced; }
  bool reduced() const { return reduced_; }

  Phase phase() const { return phase_; }
  bool can_play() const { return phase_ == Phase::Empty || phase_ == Phase::Full || phase_ == Phase::TooQuiet; }
  double time() const { return time_; }
  double phase_time() const { return t_; }
  double amb() const { return amb_; }
  double input_level() const { return lev_; }  // st.lev
  double voiced() const { return voiced_; }
  int take() const { return take_; }
  int tries() const { return tries_; }
  int actions() const { return actions_; }
  int hint() const { return hint_; }
  double idle() const { return idle_; }
  bool spoken() const { return spoken_; }
  bool fx_on(int i) const;
  const std::vector<int>& fx() const { return fx_; }
  const std::vector<int>& order() const { return order_; }
  const Jar& jar() const { return jar_; }
  const Lid& lid() const { return lid_; }
  const Pad& pad(int i) const { return pads_[i]; }
  const FxButton& fx_button(int i) const { return fxb_[i]; }
  const RecButton& rec() const { return recb_; }
  const Bin& bin() const { return bin_; }
  const std::vector<Voice>& voices() const { return voices_; }
  const std::vector<Particle>& particles() const { return parts_; }
  const std::vector<Ring>& rings() const { return rings_; }
  bool holding() const { return has_hold_; }
  const Hold& hold() const { return hold_; }
  bool has_pending() const { return has_pending_; }
  bool music_on() const { return music_on_; }
  bool pad_looping(int i) const;
  bool any_live_voice() const;
  // Reference suggest(): the next helpful target (kind None when there is none).
  Suggestion suggest() const;
  // Reference autoTarget(): the logical point a helpful player taps next (false when none).
  bool auto_target(double& x, double& y) const;

 private:
  struct EmitSpec {
    double sp0, sp1, l0, l1, gravity;
    Shape shape;
    double size;
    const uint32_t* colors;
    int ncolors;
    double up;
    bool additive;
  };
  bool voice_ok() const { return started_ && phase_ != Phase::Recording; }
  void cue(Cue c, int arg = 0);
  void start_game();
  int next_take();
  void pour_out(int mist, double level);
  void start_recording();
  void finish_recording();
  void too_quiet();
  void lid_closed();
  void celebrate();
  void record_tap();
  void toggle_fx(int i);
  void bin_tap();
  void jar_tap();
  void pad_down(int i, double y, int pointer);
  void release_hold();
  void preview();
  bool press(Body& o);
  Voice* find_voice(uint32_t id);
  uint32_t start_voice(int pad, bool loop);
  void release_voice(Voice& v, double tau);
  void stop_all_voices();
  void bend_voice(Voice& v, double semis);
  void apply_fx();
  double voice_level(const Voice& v) const;
  void emit(double x, double y, int n, const EmitSpec& o);
  void fly_mist(int n, const uint32_t* colors);
  void add_part(const Particle& p);
  void ring(double x, double y, double r0, double r1, double dur, uint32_t color, double w, double delay = 0,
            double alpha = 1);
  void spring(Body& o, double dt);
  void music_tick();

  Sound& sound_;
  Mulberry32 grng_, frng_;
  bool reduced_ = false, started_ = false;
  Phase phase_ = Phase::Start;
  double time_ = 0, t_ = 0, idle_ = 0, amb_ = 0, voiced_ = 0, last_act_ = -1, lev_ = 0, duck_t_ = 0, nudge_ = 0;
  int hint_ = 0, actions_ = 0, tries_ = 0, take_ = -1;
  bool spoken_ = false;
  std::vector<int> order_, fx_;
  Jar jar_;
  Lid lid_;
  Pad pads_[5];
  FxButton fxb_[4];
  RecButton recb_;
  Bin bin_;
  std::vector<Voice> voices_;
  uint32_t next_voice_id_ = 1;
  bool has_hold_ = false;
  Hold hold_{};
  Body* pressed_ = nullptr;
  bool has_pending_ = false;
  double pending_t_ = 0;
  std::vector<Particle> parts_;
  std::vector<Ring> rings_;
  bool music_on_ = false;
  int music_i_ = 0, music_loop_ = 0;
};

}  // namespace sound_jar
