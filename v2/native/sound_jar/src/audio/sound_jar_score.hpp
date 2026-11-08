#pragma once
// Sound Jar's sound, ported from the Web Audio code in v2/reference/sound_jar.html:
// initAudio() (master 0 -> 0.35, tau 0.1 s; -14 dB / 10:1 compressor), the sampler bus
// (dry / 55 Hz ring mod -> echo 250 ms, feedback 0.45; 6 Hz wobble +/-150 cents on every
// voice's detune), marimba() / glide() / whoosh() cues with the 8-voice voiceOk() limit,
// the cave drips on their own 6-voice music bus and duckMusic().
//
// Threading: the game calls the Sound methods on the UI thread; they only push into a
// lock-free single-producer/single-consumer ring. render_add() runs on the audio thread and
// never allocates (the echo line is a fixed member array).
//
// Differences from the browser (documented in GAME_SPEC.md / README): 16 kHz output (the takes
// are 16 kHz already); sampler playback uses linear interpolation; voice detune / wobble and the
// band-pass sweep are updated every 16 samples; AudioParam setTargetAtTime curves are one-pole
// filters at the same time constants.
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "game/sound_jar.hpp"
#include "little_arcade/audio/external_score.hpp"

namespace sound_jar {

class SoundJarScore final : public Sound, public little_arcade::audio::ExternalScore {
 public:
  static constexpr uint32_t kSampleRate = 16000;
  static constexpr std::size_t kMaxCueVoices = 8;  // reference voiceOk()
  static constexpr std::size_t kMaxMusicVoices = 6;  // reference AMB.voices
  static constexpr std::size_t kMaxNotes = 32;
  static constexpr std::size_t kMaxSampler = 8;  // 4 live + releasing tails
  static constexpr std::size_t kQueue = 128;     // power of two
  static constexpr float kOutputTrim = 1.4f;     // same headroom policy as the other reference scores

  // ---- UI thread (Sound) ----
  void start() override;
  void start_music() override;
  void stop_music() override;
  void cue(Cue cue, int arg) override;
  void voice_start(uint32_t id, int take, double rate, bool reverse, bool loop) override;
  void voice_release(uint32_t id, double tau) override;
  void voice_bend(uint32_t id, double semitones) override;
  void voice_loop(uint32_t id) override;
  void apply_fx(bool echo, bool robot, bool wobble) override;
  void duck(double seconds) override;
  void drip(double delay, int degree) override;

  // ---- audio thread ----
  void render_add(int32_t* out, std::size_t n) override;

  // Optional microphone take (set before audio starts). Its voices stop when the clip is wiped or re-armed, and
  // nothing is output while it captures: the amp has no mute pin, so this avoids feedback (design §4, SJ-6).
  void set_live(const LiveTake* live) { live_ = live; }

  // Diagnostics / tests (audio thread or after it stopped).
  bool running() const { return running_; }
  std::size_t notes() const;
  std::size_t sampler_voices() const;
  uint32_t dropped() const { return dropped_.load(std::memory_order_relaxed); }
  uint32_t voice_refusals() const { return refused_; }
  float duck_gain() const { return duck_; }
  float music_gain() const { return music_; }
  float echo_send() const { return echo_; }
  float dry_gain() const { return dry_; }
  float wet_gain() const { return wet_; }
  float wobble_depth() const { return wob_; }

 private:
  enum class Op : uint8_t { Start, StartMusic, StopMusic, Cue, VoiceStart, VoiceRelease, VoiceBend, VoiceLoop, Fx, Duck, Drip };
  struct Command { Op op; uint8_t arg, flags; uint32_t id; float a, b; };
  struct Note {
    bool active, music, overtone;
    uint32_t start, len, pos, attack, decay, decay2, glide_len;
    float peak, g, mul, g2peak, g2, mul2, incf, fmul, inc2;
    uint32_t phase, phase2;
  };
  struct SamplerVoice {
    bool active, loop, rev, rel;
    uint32_t id;
    int take;
    const int16_t* pcm;
    int n;
    uint32_t gen;  // live take generation at start
    double pos;  // sample index into the (possibly reversed) clip
    float rate, gain, target, coef;
    uint32_t attack_left, stop_left;
    float bend_cents, bend_target;
  };
  struct Whoosh {
    bool active;
    uint32_t pos;
    uint32_t rng;
    float x1, x2, y1, y2;
    float b0, b2, a1, a2, g;
  };

  void push(const Command& c);
  void apply(const Command& c);
  bool voice_ok(float seconds);
  bool music_voice_ok(uint32_t start, float seconds);
  void duck_at(float seconds);
  Note* slot();
  void marimba(float freq, float when, float vel, float dur);
  void glide(float f0, float f1, float glide_s, float when, float peak, float dur, float attack, bool music = false);
  void plop(float when, float vel, float hi) { glide(760.f * hi, 230.f * hi, 0.07f, when, 0.35f * vel, 0.14f, 0.003f); }
  void woodblock(float freq, float when, float vel);
  void chirp(float freq, float when, float vel) { glide(freq, freq * 1.5f, 0.08f, when, 0.25f * vel, 0.12f, 0.003f); }
  void whoosh();
  void play_cue(Cue cue, int arg);
  void play_drip(float delay, int degree);
  SamplerVoice* find(uint32_t id);
  float next_note(Note& n);
  float next_whoosh(Whoosh& w);

  Command ring_[kQueue]{};
  std::atomic<uint32_t> head_{0}, tail_{0}, dropped_{0};
  const LiveTake* live_ = nullptr;

  Note notes_[kMaxNotes]{};
  SamplerVoice sampler_[kMaxSampler]{};
  Whoosh whoosh_[2]{};
  uint32_t voice_end_[kMaxCueVoices]{};
  std::size_t voice_n_ = 0;
  uint32_t music_end_[kMaxMusicVoices]{};
  std::size_t music_n_ = 0;
  uint32_t refused_ = 0;
  uint32_t now_ = 0;
  bool running_ = false, music_on_ = false;
  float master_ = 0;
  // music bus: fade-in starts music_at_; duck dips to 0.35 until duck_until_
  float music_ = 0, duck_ = 1;
  uint32_t music_at_ = 0, duck_until_ = 0;
  bool ducking_ = false;
  // sampler bus
  float dry_ = 1, wet_ = 0, echo_ = 0, wob_ = 0;
  float dry_to_ = 1, wet_to_ = 0, echo_to_ = 0, wob_to_ = 0;
  uint32_t ring_phase_ = 0, lfo_phase_ = 0;
  static constexpr std::size_t kEchoLen = 4000;  // 250 ms at 16 kHz
  float echo_line_[kEchoLen]{};
  std::size_t echo_pos_ = 0;
  // compressor (see HatchAPetScore)
  static constexpr std::size_t kPreDelay = 96, kCompBlock = 16;
  float delay_[kPreDelay]{};
  std::size_t delay_pos_ = 0;
  float env_db_ = 0, comp_gain_ = 1;
  float mute_gain_ = 1;  // 0 while the microphone captures
};

}  // namespace sound_jar
