#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "little_arcade/audio/synth.hpp"
#include "little_arcade/sampler.hpp"

namespace little_arcade::audio {

// One-slot RAM sampler for Sound Jar: records from the microphone, trims and
// normalizes the take, and plays it back pitched (Q16.16 resampling with
// linear interpolation), reversed, with echo, ring-mod "robot" and vibrato
// "wobble". No heap, no floats in mix()/capture().
//
// Threading: the ISampler methods run on the UI task and only push commands
// into a lock-free SPSC ring or read atomics. capture() and mix() run on the
// audio task, which owns every buffer. wipe() is never lost: it is an epoch
// counter, so commands queued before a wipe are discarded.
//
// Privacy: buffers are borrowed (PSRAM on the device), never logged or
// persisted, zeroed on wipe(), and the raw take is zeroed as soon as it has
// been processed, so at most one trimmed clip exists.
class Sampler final : public ISampler {
 public:
  static constexpr uint32_t kMaxSamples = kSampleRateHz * kMaxRecordMs / 1000u;  // 48 000
  static constexpr uint32_t kEchoSamples = kSampleRateHz / 4;                     // 250 ms
  static constexpr std::size_t kVoiceSlots = 3;  // two playing + one fading out
  static constexpr int32_t kQuietPeak = 512;     // about -36 dBFS
  static constexpr uint32_t kMinRecordSamples = kSampleRateHz * 150u / 1000u;
  static constexpr int32_t kTargetPeak = 16384;  // -6 dBFS
  static constexpr uint32_t kPreRollSamples = kSampleRateHz * 20u / 1000u;
  static constexpr uint32_t kTailSamples = kSampleRateHz * 80u / 1000u;
  // Silence trim: 5 ms windows; sound must stay above the threshold for 3
  // windows (15 ms) in a row, so screen-tap clicks and pops are cut too.
  static constexpr uint32_t kTrimWindowSamples = kSampleRateHz * 5u / 1000u;
  static constexpr uint32_t kTrimRunWindows = 3;
  static constexpr int32_t kTrimPeakDivisor = 10;  // threshold >= loudest run / 10 (-20 dB)
  static constexpr int32_t kTrimNoiseFactor = 2;   // threshold >= 2x the quietest window (+6 dB)
  static constexpr uint32_t kFadeSamples = kSampleRateHz * 5u / 1000u;
  static constexpr uint32_t kAttackSamples = kSampleRateHz * 3u / 1000u;
  static constexpr uint32_t kReleaseSamples = kSampleRateHz * 40u / 1000u;
  static constexpr uint32_t kSlewSamples = kSampleRateHz * 10u / 1000u;
  static constexpr int32_t kEchoFeedbackQ15 = 14746;  // 0.45
  static constexpr uint32_t kRobotHz = 55;
  static constexpr uint32_t kWobbleHz = 6;
  static constexpr int32_t kWobbleDepthQ16 = 5931;  // 2^(1.5/12) - 1
  static constexpr int32_t kSoftKnee = 24576;
  static constexpr std::size_t kQueueSize = 16;  // power of two

  // `clip` and `record` hold kMaxSamples, `echo` kEchoSamples. They must
  // outlive the sampler. All three are zeroed here.
  Sampler(int16_t* clip, int16_t* record, int16_t* echo);
  Sampler(const Sampler&) = delete;
  Sampler& operator=(const Sampler&) = delete;

  // ---- setup / UI task ----
  void set_mic_hardware(bool present) { mic_hw_.store(present, std::memory_order_relaxed); }
  void set_mic_allowed(bool allowed) { mic_allowed_.store(allowed, std::memory_order_relaxed); }
  bool mic_allowed() const { return mic_allowed_.load(std::memory_order_relaxed); }

  bool mic_available() const override {
    return mic_hw_.load(std::memory_order_relaxed) && mic_allowed_.load(std::memory_order_relaxed);
  }
  void start_recording() override;
  void stop_recording() override;
  void cancel_recording() override;
  RecordResult take_record_result() override;
  uint16_t record_progress_permille() const override { return progress_.load(std::memory_order_relaxed); }
  uint8_t input_level() const override { return level_.load(std::memory_order_relaxed); }
  void play(uint32_t rate_q16, bool loop) override;
  void bend(uint32_t rate_q16) override;
  void end_loop() override;
  void release() override;
  void stop_all() override;
  void set_effects(uint8_t mask) override;
  void load_builtin(uint8_t index) override;
  void wipe() override;
  uint32_t clip_generation() const override { return generation_.load(std::memory_order_acquire); }
  uint32_t clip_samples() const override { return clip_len_pub_.load(std::memory_order_relaxed); }
  uint8_t thumb(std::size_t column) const override {
    return column < kThumbColumns ? thumb_[column].load(std::memory_order_relaxed) : 0;
  }
  int16_t playhead_permille() const override { return playhead_.load(std::memory_order_relaxed); }
  uint32_t commands_dropped() const { return dropped_.load(std::memory_order_relaxed); }

  // ---- audio task ----
  // Microphone samples (mono, 16 kHz). Stored only while recording.
  void capture(const int16_t* in, std::size_t n);
  // Applies queued commands, then adds the sampler voices into out[0..n).
  // While recording the whole block is silenced (no feedback howl).
  void mix(int16_t* out, std::size_t n);
  bool recording() const { return rec_active_; }

  // Synthesized fallback sounds; also used by host tests.
  static Sound builtin_sound(uint8_t index);

 private:
  enum class Op : uint8_t { START_REC, STOP_REC, CANCEL_REC, PLAY, BEND, END_LOOP, RELEASE, STOP_ALL, EFFECTS, BUILTIN };
  struct Command {
    Op op;
    uint8_t epoch;
    uint8_t a;
    uint32_t v;
  };
  struct Voice {
    bool active = false;
    bool loop = false;
    bool releasing = false;
    uint32_t pos_q16 = 0;
    uint32_t rate = kRateUnity;
    uint32_t target = kRateUnity;
    uint32_t slew = 0;
    int32_t env = 0;  // Q15
    uint32_t age = 0;
  };

  void push(Op op, uint8_t a = 0, uint32_t v = 0);
  void apply(const Command& c);
  void do_wipe();
  void begin_recording();
  void finish_recording();
  // Finds the sustained sound in x[0, n): first/last sample at or above an
  // adaptive threshold. False if there is no sustained sound >= kQuietPeak.
  static bool find_sound(const int16_t* x, uint32_t n, uint32_t* first, uint32_t* last);
  void abort_recording();
  void start_voice(uint32_t rate, bool loop);
  void silence_voices();
  void render_builtin(uint8_t index);
  // Copies src[start, start+len) into the clip scaled to kTargetPeak with
  // short fades, then publishes the new clip.
  void install_clip(const int16_t* src, uint32_t start, uint32_t len, int32_t peak);
  void publish_clip();
  int32_t voice_sample(Voice& v, uint32_t wobble_q16);

  // Borrowed buffers (audio task only after construction).
  int16_t* clip_;
  int16_t* record_;
  int16_t* echo_;

  // UI -> audio
  Command ring_[kQueueSize]{};
  std::atomic<uint32_t> head_{0};
  std::atomic<uint32_t> tail_{0};
  std::atomic<uint8_t> epoch_{0};
  std::atomic<uint32_t> dropped_{0};
  std::atomic<bool> mic_hw_{false};
  std::atomic<bool> mic_allowed_{true};

  // audio -> UI
  std::atomic<uint8_t> result_{0};
  std::atomic<uint16_t> progress_{0};
  std::atomic<uint8_t> level_{0};
  std::atomic<uint32_t> generation_{0};
  std::atomic<uint32_t> clip_len_pub_{0};
  std::atomic<int16_t> playhead_{-1};
  std::atomic<uint8_t> thumb_[kThumbColumns];

  // audio task state
  uint8_t done_epoch_ = 0;
  uint32_t clip_len_ = 0;
  bool rec_active_ = false;
  uint32_t rec_count_ = 0;
  int32_t dc_x1_ = 0;
  int32_t dc_y1_ = 0;
  uint8_t effects_ = 0;
  Voice voices_[kVoiceSlots];
  uint32_t voice_age_ = 0;
  uint32_t echo_pos_ = 0;
  uint32_t robot_phase_ = 0;
  uint32_t wobble_phase_ = 0;
  int16_t sine_[256]{};
  Synth synth_;
};

}  // namespace little_arcade::audio
