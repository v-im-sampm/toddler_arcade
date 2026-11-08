#pragma once
// The reference's built-in "recordings" (v2/reference/sound_jar.html renderTake()). The data is
// generated at build time by tools/export_assets.mjs into assets/generated/takes_data.cpp:
// `lev` / `play_lev` are the 20 ms RMS envelopes as exact doubles (they drive gameplay: the
// too-quiet test, the mist level, the jar's mouth and pad glow); `pcm` is the processed clip
// (peak 0.5) as int16 at 16 kHz for the sampler.
#include <cstdint>

namespace sound_jar {

inline constexpr int kTakeCount = 5;
inline constexpr int kLiveTake = kTakeCount;  // sampler slot for the microphone take (game/live_take.hpp)
inline constexpr int kTakeSampleRate = 16000;
inline constexpr double kLevelFrame = 0.02;  // reference FRAME

struct TakeData {
  const char* name;
  double dur;  // seconds (samples / 16000)
  const double* lev;
  const double* play_lev;
  int frames;
  const int16_t* pcm;
  int samples;
};

extern const TakeData kTakeData[kTakeCount];

}  // namespace sound_jar
