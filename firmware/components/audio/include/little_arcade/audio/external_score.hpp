#pragma once
#include <cstddef>
#include <cstdint>

namespace little_arcade::audio {

// A game-owned score mixed by AudioFeedback before the VolumeLimiter.
// render_add() runs on the audio task only and must not block or allocate;
// it adds int16-scale samples at kSampleRateHz into out[0..n).
class ExternalScore {
 public:
  virtual ~ExternalScore() = default;
  virtual void render_add(int32_t* out, std::size_t n) = 0;
};

}  // namespace little_arcade::audio
