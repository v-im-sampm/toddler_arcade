#pragma once
// Color Pop's sound, ported from the Web Audio code in v2/reference/quality_bar.html:
// initAudio() (master 0 -> 0.35 ramp, tau 0.1 s; -14 dB / 10:1 compressor), startWaves()
// (looped noise -> 420 Hz low-pass -> 0.018 +/- 0.014 LFO at 0.11 Hz), marimba(), plop(),
// MOTIFS/playMotif(), celebrate() and the 8-voice voiceOk() limit.
//
// Threading: the game calls the Sound methods on the UI thread; they only push into a
// lock-free single-producer/single-consumer ring. render_add() runs on the audio thread.
// No heap after construction.
//
// Differences from the browser (documented in README): 16 kHz output, so a marimba overtone
// above 7.8 kHz is omitted instead of aliasing; the noise is a xorshift generator instead of
// Math.random (keeps the game's RNG streams untouched); the compressor is the Web Audio static
// curve with a 6 ms pre-delay and 16-sample gain blocks (shared score_dsp.hpp).
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "game/color_pop.hpp"
#include "little_arcade/audio/external_score.hpp"

namespace color_pop {

class ColorPopScore final : public Sound, public little_arcade::audio::ExternalScore {
 public:
  static constexpr uint32_t kSampleRate = 16000;
  static constexpr std::size_t kMaxVoices = 8;   // reference voiceOk()
  static constexpr std::size_t kMaxNotes = 24;
  static constexpr std::size_t kQueue = 64;      // power of two
  static constexpr float kOutputTrim = 1.4f;     // same headroom policy as HatchAPetScore

  // ---- UI thread (Sound) ----
  void start() override;
  void marimba(double freq, double when, double vel, double dur) override;
  void plop(double when, double vel, double hi) override;
  void motif(Color color, double delay) override;
  void celebrate(bool big, double delay) override;

  // ---- audio thread ----
  void render_add(int32_t* out, std::size_t n) override;

  // Diagnostics / tests (audio thread or after it stopped).
  bool running() const { return running_; }
  std::size_t notes() const;
  uint32_t dropped() const { return dropped_.load(std::memory_order_relaxed); }
  uint32_t voice_refusals() const { return refused_; }

 private:
  enum class Op : uint8_t { Start, Marimba, Plop, Motif, Celebrate };
  struct Command { Op op; uint8_t arg; float a, b, c, d; };
  struct Pending { bool active; uint32_t at; Op op; uint8_t arg; };
  struct Note {
    bool active, glide, overtone;
    uint32_t start, len, pos, attack, decay, decay2, glide_len;
    float peak, g, mul, g2, mul2, incf, fmul;
    uint32_t phase, phase2, inc, inc2;
  };

  void push(const Command& c);
  void apply(const Command& c);
  bool voice_ok(float seconds);
  Note* slot();
  void play_marimba(float freq, float when, float vel, float dur);
  void play_plop(float when, float vel, float hi);
  void play_motif(Color color);
  void play_celebrate(bool big);
  float next_note(Note& n);

  Command ring_[kQueue]{};
  std::atomic<uint32_t> head_{0}, tail_{0}, dropped_{0};

  Note notes_[kMaxNotes]{};
  Pending pending_[8]{};
  uint32_t voice_end_[kMaxVoices]{};
  std::size_t voice_n_ = 0;
  uint32_t refused_ = 0;
  uint32_t now_ = 0;
  bool running_ = false;
  float master_ = 0;
  // waves: xorshift noise, RBJ low-pass state, LFO phase
  uint32_t noise_ = 0x12345678u;
  float x1_ = 0, x2_ = 0, y1_ = 0, y2_ = 0;
  uint32_t lfo_phase_ = 0;
  // compressor (see HatchAPetScore)
  static constexpr std::size_t kPreDelay = 96, kCompBlock = 16;
  float delay_[kPreDelay]{};
  std::size_t delay_pos_ = 0;
  float env_db_ = 0, comp_gain_ = 1;
};

}  // namespace color_pop
