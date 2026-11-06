#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "little_arcade/audio/color_pop_score.hpp"
#include "little_arcade/audio/sampler.hpp"
#include "little_arcade/audio/synth.hpp"
#include "little_arcade/feedback.hpp"
#include "little_arcade/volume.hpp"

namespace little_arcade::audio {

// IFeedback that turns game cues into synthesized sound.
//
// Threading: play()/set_scene()/set_level() run on the UI task (single
// producer); render() runs on the audio task (single consumer). They share
// only a lock-free ring and atomics, so neither side ever blocks. When the
// ring is full new cues are dropped (counted), never queued unboundedly.
//
// Safety (AGENTS.md rule 9): output starts muted and only ramps up after
// set_level(); every block passes through VolumeLimiter (parent cap, ramp,
// look-ahead peak limiter at the board's amp-safe ceiling). Voices are mixed
// in int32 so overlapping cues are never hard-clipped. No sound is produced
// at startup.
class AudioFeedback final : public IFeedback {
 public:
  static constexpr std::size_t kQueueSize = 16;  // power of two
  // Identical cues inside this window collapse into one (rapid toddler taps).
  static constexpr uint32_t kRepeatGuardMs = 60;

  explicit AudioFeedback(VolumeLimiter::Config config = {});

  // ---- UI task ----
  void play(Cue cue, uint8_t variant) override;
  void set_scene(uint8_t scene) override;
  void set_level(uint8_t level) { level_.store(level, std::memory_order_relaxed); }
  void set_parent_cap(uint8_t cap) { cap_.store(cap, std::memory_order_relaxed); }
  // Optional Sound Jar sampler, mixed before the limiter. Attach before the
  // audio task starts; it is then owned by the audio task.
  void attach_sampler(Sampler* sampler) { sampler_ = sampler; }

  // ---- audio task ----
  // Fills out[0..n) with limited mono PCM at kSampleRateHz.
  void render(int16_t* out, std::size_t n);

  // Diagnostics (any task).
  uint32_t cues_played() const { return played_.load(std::memory_order_relaxed); }
  uint32_t cues_dropped() const { return dropped_.load(std::memory_order_relaxed); }
  uint32_t cues_merged() const { return merged_.load(std::memory_order_relaxed); }
  int32_t peak_since_reset() { return peak_.exchange(0, std::memory_order_relaxed); }
  uint8_t level() const { return limiter_level_.load(std::memory_order_relaxed); }

 private:
  struct Event {
    uint8_t kind;  // 0 = cue, 1 = scene change
    uint8_t scene;
    uint8_t a;
    uint8_t b;
  };

  void push(const Event& e);
  void apply(const Event& e);

  Event ring_[kQueueSize]{};
  std::atomic<uint32_t> head_{0};  // written by producer
  std::atomic<uint32_t> tail_{0};  // written by consumer
  uint8_t scene_ = kSceneMenu;     // producer side

  std::atomic<uint8_t> level_{0};
  std::atomic<uint8_t> cap_;
  std::atomic<uint8_t> limiter_level_{0};
  std::atomic<uint32_t> played_{0};
  std::atomic<uint32_t> dropped_{0};
  std::atomic<uint32_t> merged_{0};
  std::atomic<int32_t> peak_{0};

  // consumer side
  static constexpr std::size_t kChunk = VolumeLimiter::kChunk;
  Synth synth_;
  ColorPopScore color_pop_;  // Color Pop's reference score (Web Audio port)
  Sampler* sampler_ = nullptr;
  VolumeLimiter limiter_;
  int32_t mix_[kChunk];
  int16_t sampler_out_[kChunk];
  uint32_t samples_ = 0;
  uint8_t requested_level_ = 0;
  Event last_{0xFF, 0, 0, 0};
  uint32_t last_at_ = 0;
};

}  // namespace little_arcade::audio
