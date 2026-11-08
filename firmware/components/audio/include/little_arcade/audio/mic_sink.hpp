#pragma once
#include <cstddef>
#include <cstdint>

namespace little_arcade::audio {

// Receives INMP441 blocks (16 kHz mono int16) on the audio task when passed in I2sAudioOptions::mic_sink.
// capture() must not block or allocate. The sink decides what (if anything) to keep; the driver itself
// stores and logs nothing.
class MicSink {
 public:
  virtual ~MicSink() = default;
  virtual void set_mic_hardware(bool present) = 0;  // called once after the I2S RX channel starts (or not)
  virtual void capture(const int16_t* samples, std::size_t n) = 0;
};

}  // namespace little_arcade::audio
