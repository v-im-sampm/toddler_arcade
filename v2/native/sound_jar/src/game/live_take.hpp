#pragma once
// The live microphone take (v2/games/sound_jar.md §2/§5): one clip of at most 3 s at 16 kHz, kept in RAM
// only and never persisted, logged or sent. The reference has no microphone (it replays built-in voices); this
// replaces only where the take's samples and the recording level come from. The jar's colour, the take
// rotation (gameplay RNG) and every rule stay the reference's.
//
// Threads: the UI thread calls arm()/stop()/wipe()/finalize() and reads the meters; the audio thread calls
// capture() with each microphone block (n may be 0 to only service requests). Requests travel through a
// lock-free single-producer/single-consumer ring and are applied on the audio thread, so the sample buffer is
// written only by the audio thread while capturing and only by the UI thread in finalize() (after the audio
// thread acknowledged stop()). Zeroing (wipe and every new recording) happens on the audio thread, before any
// later sampler voice can read the buffer.
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "game/takes.hpp"

namespace sound_jar {

class LiveTake {
 public:
  static constexpr int kMaxSamples = 3 * kTakeSampleRate;  // design: one clip <= 3 s
  static constexpr int kFrameSamples = 320;                // reference FRAME (20 ms)
  static constexpr int kMaxFrames = kMaxSamples / kFrameSamples;
  static constexpr int kFadeSamples = 80;   // reference: 5 ms fades
  static constexpr int kDiscardSamples = kFrameSamples * 5;  // native: the first 100 ms after arm are not kept
  static constexpr double kMaxGain = 32;    // normalise at most +30 dB (a voiced take is >= 512 RMS)
  enum class State : uint8_t { Idle, Recording, Captured, Ready };
  using ClockUs = uint64_t (*)();

  // `buffer` holds kMaxSamples int16 and must outlive this object (PSRAM on the device).
  explicit LiveTake(int16_t* buffer, ClockUs clock = nullptr);
  LiveTake(const LiveTake&) = delete;
  LiveTake& operator=(const LiveTake&) = delete;

  // ---- UI thread ----
  void set_allowed(bool allowed) { allowed_.store(allowed, std::memory_order_relaxed); }  // parent mic switch
  bool allowed() const { return allowed_.load(std::memory_order_relaxed); }
  bool hardware() const { return hardware_.load(std::memory_order_acquire); }
  bool available() const { return hardware() && allowed(); }
  void arm();   // zero the previous clip and start capturing
  void stop();  // stop capturing; the clip then waits for finalize()
  void wipe();  // stop and zero the clip (privacy wipe)
  State state() const { return state_.load(std::memory_order_acquire); }
  double level() const { return level_.load(std::memory_order_relaxed); }  // last 20 ms RMS of the input, 0..1
  double voiced() const;   // seconds of 20 ms frames at or above 512/32768 RMS (reference LOUD)
  double elapsed() const;  // seconds captured
  // End rule in captured time: 0.5 s after the last sounding frame, or 3 s. "Sounding" is the reference ACTIVE
  // (0.002 RMS) raised to 10x the recording's quietest frame (capped at LOUD), so room noise never counts.
  bool should_stop() const;
  // Once state() == Captured: trim to the sounding frames (one frame before, two after), normalise the peak to 0.5
  // (-6 dBFS, as renderTake()), 5 ms fades, and the 20 ms envelopes; zeroes everything after the clip.
  // Returns false (and leaves the take unusable) when there is nothing to keep.
  bool finalize();
  const TakeData& data() const { return data_; }  // the finalized take (all zero until then)
  uint32_t start_latency_us() const { return latency_us_.load(std::memory_order_relaxed); }
  uint32_t takes() const { return takes_.load(std::memory_order_relaxed); }

  // ---- audio thread ----
  void set_hardware(bool present) { hardware_.store(present, std::memory_order_release); }
  // Fixed input gain applied after the DC blocker (the mic's raw level vs. the reference's browser-AGC levels).
  void set_input_gain(float gain) { gain_ = gain; }  // call before the audio thread starts
  // Loudest 20 ms RMS of the last recording (0..1), kept after wipes; an aggregate for calibration logs only.
  double peak_level() const { return peak_.load(std::memory_order_relaxed); }
  // Quietest 20 ms RMS of the last recording (its noise floor, 0..1); an aggregate for calibration logs only.
  double floor_level() const { return floor_.load(std::memory_order_relaxed); }
  void capture(const int16_t* in, std::size_t n);
  // Sampler voices read these; a voice started on one generation stops when it changes (wipe / re-arm).
  bool ready() const { return ready_.load(std::memory_order_acquire); }
  const int16_t* pcm() const { return buf_; }
  int samples() const { return samples_.load(std::memory_order_acquire); }
  uint32_t generation() const { return gen_.load(std::memory_order_acquire); }
  // True from arm() until the audio thread applies stop()/wipe(): the speaker stays silent (SJ-6).
  bool capturing() const { return mute_.load(std::memory_order_acquire); }

 private:
  enum class Op : uint8_t { Arm, Stop, Wipe };
  static constexpr uint32_t kQueue = 8;
  void push(Op op);
  void zero_buffer();
  void reset_meters();
  void clear_ui();

  int16_t* buf_;
  ClockUs clock_;
  std::atomic<bool> allowed_{true}, hardware_{false}, ready_{false}, mute_{false};
  std::atomic<State> state_{State::Idle};
  Op ring_[kQueue]{};
  std::atomic<uint32_t> head_{0}, tail_{0};
  std::atomic<uint32_t> gen_{0}, takes_{0}, latency_us_{0};
  std::atomic<uint32_t> arm_at_us_{0};  // low 32 bits of the clock (wraps every ~71 min; differences stay valid)
  std::atomic<int> len_{0}, samples_{0}, voiced_samples_{0}, last_active_{-1};
  std::atomic<float> level_{0}, peak_{0}, floor_{0};
  // audio thread only
  float gain_ = 1;
  float dc_x1_ = 0, dc_y1_ = 0;
  double frame_sumsq_ = 0;
  int frame_n_ = 0, skip_ = 0;
  bool first_block_ = false, frame_disc_ = false;
  // UI thread only
  double lev_[kMaxFrames]{}, play_lev_[kMaxFrames]{};
  TakeData data_{"mic", 0, lev_, play_lev_, 0, nullptr, 0};
};

}  // namespace sound_jar
