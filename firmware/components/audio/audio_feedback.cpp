#include "little_arcade/audio/audio_feedback.hpp"

#include "little_arcade/audio/sound_bank.hpp"

namespace little_arcade::audio {
namespace {
constexpr uint8_t kKindCue = 0;
constexpr uint8_t kKindScene = 1;
constexpr uint32_t kRepeatGuardSamples = AudioFeedback::kRepeatGuardMs * kSampleRateHz / 1000u;
}  // namespace

AudioFeedback::AudioFeedback(VolumeLimiter::Config config)
    : cap_(config.parent_cap), limiter_(config) {}

void AudioFeedback::push(const Event& e) {
  const uint32_t head = head_.load(std::memory_order_relaxed);
  if (head - tail_.load(std::memory_order_acquire) >= kQueueSize) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  ring_[head % kQueueSize] = e;
  head_.store(head + 1, std::memory_order_release);
}

void AudioFeedback::play(Cue cue, uint8_t variant) {
  push(Event{kKindCue, scene_, static_cast<uint8_t>(cue), variant});
}

void AudioFeedback::set_scene(uint8_t scene) {
  if (scene == scene_) return;
  push(Event{kKindScene, scene, scene_, scene});
  scene_ = scene;
}

void AudioFeedback::apply(const Event& e) {
  Sound sound;
  if (e.kind == kKindScene) {
    sound = scene_change_sound(e.a, e.b);
  } else {
    const bool repeat = last_.kind == e.kind && last_.scene == e.scene && last_.a == e.a && last_.b == e.b;
    if (repeat && samples_ - last_at_ < kRepeatGuardSamples) {
      merged_.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    last_ = e;
    last_at_ = samples_;
    sound = sound_for(e.scene, static_cast<Cue>(e.a), e.b);
  }
  if (sound.empty()) return;
  synth_.trigger(sound);
  played_.fetch_add(1, std::memory_order_relaxed);
}

void AudioFeedback::render(int16_t* out, std::size_t n) {
  const uint8_t cap = cap_.load(std::memory_order_relaxed);
  if (cap != limiter_.parent_cap()) limiter_.set_parent_cap(cap);
  const uint8_t want = level_.load(std::memory_order_relaxed);
  if (want != requested_level_) {
    requested_level_ = want;
    limiter_.set_level(want);
  }
  limiter_level_.store(limiter_.level(), std::memory_order_relaxed);

  uint32_t tail = tail_.load(std::memory_order_relaxed);
  const uint32_t head = head_.load(std::memory_order_acquire);
  while (tail != head) {
    const Event e = ring_[tail % kQueueSize];
    tail_.store(++tail, std::memory_order_release);
    apply(e);
  }

  synth_.render(out, n);
  if (sampler_) sampler_->mix(out, n);  // silences the block while recording
  limiter_.process(out, n);

  int32_t peak = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const int32_t a = out[i] < 0 ? -static_cast<int32_t>(out[i]) : out[i];
    if (a > peak) peak = a;
  }
  int32_t prev = peak_.load(std::memory_order_relaxed);
  while (peak > prev && !peak_.compare_exchange_weak(prev, peak, std::memory_order_relaxed)) {
  }
  samples_ += static_cast<uint32_t>(n);
}

}  // namespace little_arcade::audio
