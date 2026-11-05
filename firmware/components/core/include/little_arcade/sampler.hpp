#pragma once
#include <cstddef>
#include <cstdint>

namespace little_arcade {

// UI-task view of the one-slot sound sampler used by Sound Jar
// (games/sound_jar.md). The implementation lives in the audio component and
// runs its DSP on the audio task; every call here only queues a command or
// reads an atomic, so it never blocks.
//
// Privacy (docs/parental-safety.md): the clip lives only in RAM, is never
// written to flash/NVS/logs/transports, and wipe() zeroes it.
class ISampler {
 public:
  // Effect bits for set_effects(); they stack.
  static constexpr uint8_t kFxReverse = 1u << 0;
  static constexpr uint8_t kFxEcho = 1u << 1;
  static constexpr uint8_t kFxRobot = 1u << 2;
  static constexpr uint8_t kFxWobble = 1u << 3;
  static constexpr uint8_t kFxAll = kFxReverse | kFxEcho | kFxRobot | kFxWobble;

  // Playback rate in Q16.16 (65536 = original pitch and speed).
  static constexpr uint32_t kRateUnity = 65536u;
  static constexpr uint32_t kRateMin = kRateUnity / 2;  // -12 semitones
  static constexpr uint32_t kRateMax = kRateUnity * 2;  // +12 semitones

  static constexpr uint32_t kMaxRecordMs = 3000;
  static constexpr std::size_t kThumbColumns = 64;
  static constexpr uint8_t kBuiltinCount = 3;  // boing, quack, drum

  enum class RecordResult : uint8_t { NONE, ACCEPTED, TOO_QUIET };

  virtual ~ISampler() = default;

  // True when recording is possible: microphone hardware is running and the
  // parent setting allows it. When false the game uses built-in sounds.
  virtual bool mic_available() const = 0;

  // Recording mutes the speaker and stops all voices. stop_recording() trims
  // and normalizes; the outcome arrives through take_record_result(). A
  // recording also stops by itself at kMaxRecordMs.
  virtual void start_recording() = 0;
  virtual void stop_recording() = 0;
  virtual void cancel_recording() = 0;
  virtual RecordResult take_record_result() = 0;
  // 0..1000 of kMaxRecordMs captured so far (0 when idle).
  virtual uint16_t record_progress_permille() const = 0;
  // Recent microphone loudness 0..255 while recording (aggregate only).
  virtual uint8_t input_level() const = 0;

  // Starts a voice (at most two; the oldest is released). `loop` keeps it
  // repeating; end_loop() lets it finish its current pass, release() fades it
  // out in 40 ms. bend() retunes the newest looping voice.
  virtual void play(uint32_t rate_q16, bool loop) = 0;
  virtual void bend(uint32_t rate_q16) = 0;
  virtual void end_loop() = 0;
  virtual void release() = 0;
  virtual void stop_all() = 0;
  virtual void set_effects(uint8_t mask) = 0;

  // Replaces the clip with an in-project synthesized sound (0..kBuiltinCount).
  virtual void load_builtin(uint8_t index) = 0;
  // Zeroes every sample buffer and stops all sound.
  virtual void wipe() = 0;

  // Increments whenever the clip changes (new recording, built-in, wipe).
  virtual uint32_t clip_generation() const = 0;
  virtual uint32_t clip_samples() const = 0;
  // Peak per column, 0..255, for the jar's waveform picture.
  virtual uint8_t thumb(std::size_t column) const = 0;
  // Newest voice position 0..1000 through the clip, or -1 when silent.
  virtual int16_t playhead_permille() const = 0;
};

}  // namespace little_arcade
