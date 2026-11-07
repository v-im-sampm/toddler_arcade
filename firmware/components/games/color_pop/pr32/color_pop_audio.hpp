#pragma once
#include <cstddef>
#include <cstdint>

#include "audio/AudioEngine.h"
#include "little_arcade/audio/scene_audio.hpp"

namespace little_arcade::color_pop::pr32 {

// Color Pop's sound on the PixelRoot32 APU. Each cue becomes timed APU events
// (sine "marimba" notes with a decaying envelope, a swept sine "plop"), the
// same notes and timing as the game's original Web Audio score. The APU is
// pulled by AudioFeedback's render loop (ISceneAudio::render_add), so the
// events, the queue and the rendering all live on the audio task.
class ColorPopAudio final : public audio::ISceneAudio {
 public:
  static constexpr uint32_t kRateHz = 16000;  // audio::kSampleRateHz
  static constexpr std::size_t kMaxPending = 48;

  explicit ColorPopAudio(pixelroot32::audio::AudioEngine& engine) : engine_(engine) {}

  bool cue(Cue cue, uint8_t variant) override;
  void stop() override;
  void render_add(int32_t* out, std::size_t n) override;

  // Pentatonic degree -> Hz (C5 major pentatonic; 5 degrees per octave).
  static float degree_hz(int degree, int octave);
  std::size_t pending() const { return pending_n_; }
  bool sounding() const { return pending_n_ > 0 || clock_ < quiet_at_; }

 private:
  struct Pending {
    uint64_t at;  // sample clock
    pixelroot32::audio::AudioEvent event;
  };
  void add(float when, const pixelroot32::audio::AudioEvent& event);
  void marimba(float freq, float when, float vel, float dur);
  void plop(float when, float vel, float hi);

  pixelroot32::audio::AudioEngine& engine_;
  Pending pending_[kMaxPending]{};
  std::size_t pending_n_ = 0;
  uint64_t clock_ = 0;     // samples rendered
  uint64_t quiet_at_ = 0;  // every fired voice has ended by this sample
};

}  // namespace little_arcade::color_pop::pr32
