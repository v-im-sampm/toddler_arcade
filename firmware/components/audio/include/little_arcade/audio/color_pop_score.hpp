#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/feedback.hpp"

namespace little_arcade::audio {

// Color Pop's sound, ported from the Web Audio code in
// v2/reference/quality_bar.html (marimba(), plop(), startWaves(), MOTIFS,
// celebrate(), the 8-voice limit, the 0.35 master ramp-in and the -14 dB
// compressor). AudioFeedback routes Color Pop cues here instead of the tone
// bank. Cue variants are produced by color_pop.cpp:
//   AMBIENT              startWaves() + master ramp-in
//   PROMPT  colour       playMotif(colour)
//   SUCCESS colour       plop(0,1) + marimba(deg, 0.03, 1)
//   CELEBRATE 0 / 1      celebrate(small / big)
//   NOTE 0x10|deg        wrong-colour pop: plop(0,.55,1.4) + marimba(deg,oct 1,.02,.45,.35)
//   NOTE 0x20            water tap: marimba(deg 0, oct 1, 0, .3, .25)
//   NOTE 0x30|deg        celebration sparkle: marimba(deg, oct 1, 0, .4, .3)
//   REACTION 0           crab giggle: plop(0, .6, 1.6)
// Colour variants follow color_pop::Color (RED, BLUE, GREEN, YELLOW).
//
// Differences forced by the device: 16 kHz output, so a marimba overtone above
// 7.8 kHz (only the top celebration note) is omitted instead of aliasing; the
// result is scaled by kOutputTrim for loudness parity with the other games
// before the shared VolumeLimiter (mix ratios are unchanged).
//
// Single-threaded: owned by the audio task. No heap.
class ColorPopScore {
 public:
  static constexpr std::size_t kMaxVoices = 8;  // reference voiceOk() limit
  static constexpr std::size_t kMaxNotes = 16;  // scheduled + sounding
  static constexpr float kOutputTrim = 2.5f;

  // Returns false for cues the score does not own (the caller falls back).
  bool cue(Cue cue, uint8_t variant);
  // Fades out (no click) and drops everything; next AMBIENT ramps in again.
  void stop();
  // Adds the score into out[0..n) (int16 scale, may exceed it before limiting).
  void render_add(int32_t* out, std::size_t n);

  bool running() const { return running_; }
  std::size_t notes() const;
  // Reference helpers, exposed for tests.
  static float degree_hz(int degree, int octave);
  static uint8_t color_degree(uint8_t color);

 private:
  enum class Kind : uint8_t { MARIMBA, PLOP };
  struct Note {
    Kind kind;
    bool active;
    uint32_t start;  // sample time
    uint32_t len;    // samples until stop
    uint32_t pos;
    float freq, vel;
    uint32_t attack, decay;  // samples (attack, then exponential to dur)
    float peak, mul;         // decay multiplier per sample
    float g;                 // current envelope value
    float g2, mul2;          // marimba overtone envelope
    uint32_t decay2;
    float f, fmul;           // plop pitch glide
    uint32_t phase, phase2;
    uint32_t inc, inc2;      // marimba phase increments (body, overtone)
    float incf;              // plop phase increment (glides with f)
    uint32_t glide;          // plop glide length, samples
    bool overtone;
  };

  void start_audio();
  bool voice_ok(float seconds);
  void marimba(float freq, float when, float vel, float dur);
  void plop(float when, float vel, float hi);
  void motif(uint8_t color);
  void celebrate(bool big);
  float next_note(Note& n);

  Note notes_[kMaxNotes]{};
  uint32_t voice_end_[kMaxVoices]{};
  std::size_t voice_n_ = 0;
  uint32_t now_ = 0;
  bool running_ = false;

  // Master gain (setTargetAtTime 0.35, tau 0.1 s) and stop fade.
  float master_ = 0, master_target_ = 0, master_coef_ = 0;
  // Wave bed: white noise -> 420 Hz lowpass -> gain 0.018 + 0.014 * LFO(0.11 Hz).
  bool waves_ = false;
  uint32_t noise_ = 0x9E3779B9u;
  float b0_ = 0, b1_ = 0, b2_ = 0, a1_ = 0, a2_ = 0;
  float x1_ = 0, x2_ = 0, y1_ = 0, y2_ = 0;
  uint32_t lfo_phase_ = 0;
  // Compressor state: gain reduction (dB), updated once per kCompBlock samples
  // like Chromium's DynamicsCompressorKernel, and the linear gain applied
  // (ramped across each block).
  static constexpr std::size_t kCompBlock = 16;
  float env_db_ = 0;
  float comp_gain_ = 1;
};

}  // namespace little_arcade::audio
