#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/feedback.hpp"

namespace little_arcade::audio {

// Hatch-a-Pet's sound, ported from the Web Audio code in
// v2/reference/hatch_a_pet.html (marimba(), glide(), plop(), woodblock(),
// chirp(), the lullaby (startMusic()/musicTick()/duckMusic()), the 8-voice cue
// limit, the 0.35 master ramp-in and the -14 dB compressor with its 6 ms
// pre-delay). AudioFeedback routes Hatch-a-Pet cues here instead of
// the tone bank. Cue variants are produced by hatch_a_pet.cpp (see
// hatch_a_pet::cue):
//   AMBIENT 0            startGame(): master ramp-in + startMusic()
//   SUCCESS 0            hatchSound()
//   REACTION kind|c      0x00 giggle, 0x10 jump, 0x20 sneeze (reactSound),
//                        0x30 nameChirp, 0x40 byeSound; c = creature (deg)
//   NOTE 0x10|n          knockSound(n)
//   NOTE 0x20 / 0x21     knockKnock() now / setTimeout(knockKnock, 200)
//   NOTE 0x30            trill()
//   NOTE 0x40            landing woodblock(degFreq(0, -1), 0, 0.5)
//   NOTE 0x41 / 0x42     plop(0, .6, .6) intro landing / plop(0, .8, 1.2) choice
//   NOTE 0x43            choiceSound()
//   NOTE 0x44            background tap marimba(degFreq(0, 1), 0, .3, .25)
//   NOTE 0x50|c          shelf friend chirp(degFreq(deg, 1), 0, .35)
//   NOTE 0x60|q          birdSound(), v = 0.1 + q / 15 * 0.05
// The sketch's setTimeout(knockKnock, 200) becomes a +0.2 s schedule offset.
//
// The lullaby (LULLABY in the sketch) is an original 8-bar tune in 3/4 at
// 72 bpm, F major pentatonic: a soft sine melody and a slow sine bass. It has
// its own 6-voice pool (never takes cue voices), fades in over ~1.5 s and
// ducks to 0.35 while cue voices sound.
//
// Differences forced by the device: 16 kHz output, so a marimba overtone above
// 7.8 kHz is omitted instead of aliasing; the result is scaled by kOutputTrim
// so the busiest moments stay under the speaker ceiling at full volume (see
// audio_hatch_a_pet_score_test busy-sequence numbers; mix ratios unchanged).
//
// Single-threaded: owned by the audio task. No heap.
class HatchAPetScore {
 public:
  static constexpr std::size_t kMaxVoices = 8;  // reference voiceOk() limit
  static constexpr std::size_t kMaxNotes = 24;  // scheduled + sounding
  static constexpr std::size_t kMusicVoices = 6;  // reference musicVoiceOk() pool
  static constexpr float kOutputTrim = 1.4f;

  // Returns false for cues the score does not own (the caller falls back).
  bool cue(Cue cue, uint8_t variant);
  // Fades out (no click) and drops everything; next AMBIENT ramps in again.
  void stop();
  // Adds the score into out[0..n) (int16 scale, may exceed it before limiting).
  void render_add(int32_t* out, std::size_t n);

  bool running() const { return running_; }
  std::size_t notes() const;
  std::size_t music_notes() const;
  // Music bus gain right now (fade-in x duck), for tests.
  float music_gain() const { return music_fade_ * duck_; }
  // Compressor gain reduction right now (dB, <= 0); diagnostics/tests.
  float comp_reduction_db() const { return env_db_; }
  // Reference degFreq(deg, oct): F major pentatonic from F5 (698.46 Hz).
  static float degree_hz(int degree, int octave);

 private:
  enum class Kind : uint8_t { MARIMBA, GLIDE };
  struct Note {
    Kind kind;
    bool active;
    uint32_t start;  // sample time
    uint32_t len;    // samples until stop
    uint32_t pos;
    uint32_t attack, decay;  // samples (attack, then exponential to dur)
    float peak, mul;         // decay multiplier per sample
    float g;                 // current envelope value
    float g2, mul2;          // marimba overtone envelope
    uint32_t decay2;
    uint32_t phase, phase2;
    uint32_t inc, inc2;      // marimba phase increments (body, overtone)
    float incf, fmul;        // glide phase increment and its per-sample ratio
    uint32_t glide;          // glide length, samples
    bool overtone;
  };
  // Lullaby voice: sine with the same env() as the cues.
  struct MusicNote {
    bool active;
    uint32_t start, len, pos, attack, decay;
    uint32_t until;  // start + dur: musicVoiceOk() bookkeeping
    float peak, g, mul;
    uint32_t phase, inc;
  };

  void start_audio();
  bool voice_ok(float seconds);
  Note* slot();
  void marimba(float freq, float when, float vel, float dur);
  void glide(float f0, float f1, float glide_s, float when, float peak, float dur);
  void plop(float when, float vel, float hi);
  void woodblock(float freq, float when, float vel);
  void chirp(float freq, float when, float vel);
  void knock_knock(float when);
  void react(int kind, int deg);
  float next_note(Note& n);
  void schedule_music(uint32_t horizon);
  static float next_music(MusicNote& n);

  Note notes_[kMaxNotes]{};
  uint32_t voice_end_[kMaxVoices]{};
  std::size_t voice_n_ = 0;
  uint32_t now_ = 0;
  bool running_ = false;

  // Master gain (setTargetAtTime 0.35, tau 0.1 s) and stop fade.
  float master_ = 0, master_target_ = 0, master_coef_ = 0;
  // Lullaby: event cursor (index into the song, loop count), its own voices,
  // the fade-in bus (setTargetAtTime(1, t0, 1.5)) and the cue duck.
  MusicNote music_[kMusicVoices]{};
  bool music_on_ = false;
  uint32_t music_t0_ = 0;
  std::size_t music_next_ = 0;
  uint32_t music_loop_ = 0;
  float music_fade_ = 0;
  float duck_ = 1;
  uint32_t duck_until_ = 0;
  // Compressor pre-delay (Chromium DynamicsCompressor: 6 ms look-ahead).
  static constexpr std::size_t kPreDelay = 96;
  float delay_[kPreDelay]{};
  std::size_t delay_pos_ = 0;
  // Compressor state: gain reduction (dB), updated once per kCompBlock samples
  // like Chromium's DynamicsCompressorKernel, and the linear gain applied
  // (ramped across each block).
  static constexpr std::size_t kCompBlock = 16;
  float env_db_ = 0;
  float comp_gain_ = 1;
};

}  // namespace little_arcade::audio
