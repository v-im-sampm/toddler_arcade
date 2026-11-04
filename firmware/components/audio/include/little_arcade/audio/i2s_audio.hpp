#pragma once
#include <cstdint>

#include "little_arcade/audio/audio_feedback.hpp"

namespace little_arcade::audio {

struct I2sAudioOptions {
  // Bring-up diagnostic for H03: reads the INMP441 and logs only aggregate
  // RMS/peak numbers every few seconds. No samples are stored or sent.
  bool mic_level_probe = false;
  uint32_t log_period_ms = 5000;
};

// Starts I2S0 (TX to the MAX98357A, optional RX from the INMP441 on the same
// BCLK/WS) and a high-priority audio task that pulls PCM from `feedback`.
// Refuses to start (returns false) if any audio pin is TBD. ESP-IDF only.
bool start_i2s_audio(AudioFeedback& feedback, const I2sAudioOptions& options);

}  // namespace little_arcade::audio
