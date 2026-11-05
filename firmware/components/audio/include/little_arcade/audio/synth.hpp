#pragma once
#include <cstddef>
#include <cstdint>

namespace little_arcade::audio {

// Shared with the I2S RX side (ESP-SR expects 16 kHz), so TX uses it too.
inline constexpr uint32_t kSampleRateHz = 16000;

enum class Wave : uint8_t { SINE, TRIANGLE, PULSE, NOISE };

// One step of a sound. f0_hz == 0 is a rest. Pitch glides linearly from f0 to
// f1 across the step. Every step gets at least kMinEdgeMs of attack and
// release so starts and stops never click.
struct Tone {
  Wave wave = Wave::SINE;
  uint16_t f0_hz = 0;
  uint16_t f1_hz = 0;
  uint16_t ms = 0;
  uint8_t level = 0;  // 0..255
  uint8_t attack_ms = 0;
  uint8_t release_ms = 0;
};

// Mono PCM at kSampleRateHz, held in flash and never copied.
struct PcmClip {
  const int16_t* samples = nullptr;
  uint32_t count = 0;
};

// A short sound: an optional PCM clip followed by a sequence of tones, copied
// by value into a voice (the clip by pointer).
struct Sound {
  static constexpr std::size_t kMaxTones = 8;
  Tone tones[kMaxTones]{};
  uint8_t count = 0;
  const PcmClip* clip = nullptr;
  uint8_t clip_level = 0;  // 0..255, same scale as Tone::level

  Sound& add(const Tone& t) {
    if (count < kMaxTones) tones[count++] = t;
    return *this;
  }
  Sound& with_clip(const PcmClip& c, uint8_t level) {
    clip = &c;
    clip_level = level;
    return *this;
  }
  bool has_clip() const { return clip != nullptr && clip->count > 0; }
  bool empty() const { return count == 0 && !has_clip(); }
  uint32_t duration_ms() const {
    uint32_t total = has_clip() ? clip->count * 1000u / kSampleRateHz : 0u;
    for (uint8_t i = 0; i < count; ++i) total += tones[i].ms;
    return total;
  }
};

// Small polyphonic synth. No heap, no floats in render(), deterministic.
// Single-threaded: owned by the audio task.
class Synth {
 public:
  static constexpr std::size_t kVoices = 4;
  static constexpr uint32_t kMinEdgeMs = 3;

  // Starts on a free voice, otherwise replaces the oldest one.
  void trigger(const Sound& sound);
  void stop_all();
  std::size_t active_voices() const;

  // Overwrites out[0..n) with the mixed mono signal (int16 clamped).
  void render(int16_t* out, std::size_t n);

 private:
  struct Voice {
    Sound sound;
    uint8_t index = 0;
    bool active = false;
    bool in_clip = false;
    uint32_t clip_pos = 0;
    uint32_t age = 0;
    uint32_t pos = 0;  // sample within the current tone
    uint32_t len = 0;  // samples in the current tone
    uint32_t attack = 1;
    uint32_t release = 1;
    uint32_t phase = 0;
    int64_t incr = 0;  // phase increment, Q32 cycles/sample
    int64_t dincr = 0;
    uint16_t lfsr = 0xACE1u;
    int16_t noise = 0;
  };

  void start_tone(Voice& v);
  int32_t next_clip_sample(Voice& v);
  int32_t next_sample(Voice& v);

  Voice voices_[kVoices];
  uint32_t trigger_count_ = 0;
};

}  // namespace little_arcade::audio
