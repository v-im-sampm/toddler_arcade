#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/feedback.hpp"

namespace little_arcade::audio {

// A scene-owned sound engine that AudioFeedback mixes in for one scene (e.g.
// Color Pop's PixelRoot32 APU score). Every method runs on the audio task.
class ISceneAudio {
 public:
  virtual ~ISceneAudio() = default;
  // Returns false for cues it does not own (AudioFeedback falls back to the
  // shared tone bank).
  virtual bool cue(Cue cue, uint8_t variant) = 0;
  // The scene was left: silence everything now.
  virtual void stop() = 0;
  // Adds mono PCM (int16 scale, may exceed it before limiting) into out[0..n).
  virtual void render_add(int32_t* out, std::size_t n) = 0;
};

}  // namespace little_arcade::audio
