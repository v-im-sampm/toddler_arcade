#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"
#include "little_arcade/sampler.hpp"

namespace little_arcade::sound_jar {

inline constexpr std::size_t kPadCount = 5;
inline constexpr std::size_t kEffectCount = 4;

// Effect buttons, left to right; each maps to one ISampler effect bit.
enum class Effect : uint8_t { BACKWARDS, ECHO, ROBOT, WOBBLE };
constexpr uint8_t effect_bit(std::size_t i) {
  return i == 0 ? ISampler::kFxReverse : i == 1 ? ISampler::kFxEcho : i == 2 ? ISampler::kFxRobot : ISampler::kFxWobble;
}

enum class Phase : uint8_t { INTRO, PLAY, RECORDING, CELEBRATE };

// Sound Jar (games/sound_jar.md): catch a sound, then play it big/small,
// backwards, echoey, robotic or wobbly. Pure scene logic; the view reads the
// accessors. No heap; all timing comes from update(dt_ms).
//
// Sound goes through ISampler (owned by the audio component). Without one
// (audio failed to start) every control still works visually and pads fall
// back to Cue::NOTE. Without a microphone (or the parent turned it off) the
// Record button becomes a sparkle button that cycles built-in sounds.
//
// Privacy: the clip is wiped on exit, pause/sleep and parent-gate entry.
class SoundJarGame final : public IGame {
 public:
  static constexpr Rect kRecordButton{60, 0, 64, 56};
  // Stops short of the Mic corner (kMicButton), which the games never get.
  static constexpr Rect kJar{128, 0, 132, 56};
  static constexpr Rect kEffects[kEffectCount] = {
      {4, 64, 72, 52}, {84, 64, 72, 52}, {164, 64, 72, 52}, {244, 64, 72, 52}};
  static constexpr Rect kPads[kPadCount] = {
      {4, 124, 56, 112}, {68, 124, 56, 112}, {132, 124, 56, 112}, {196, 124, 56, 112}, {260, 124, 56, 112}};
  // Big creature = low voice, tiny creature = high voice.
  static constexpr int8_t kPadSemitones[kPadCount] = {-12, -5, 0, 7, 12};
  static constexpr uint32_t kPadRates[kPadCount] = {32768, 49097, 65536, 98193, 131072};

  static constexpr uint32_t kIntroMs = 1200;
  static constexpr uint32_t kCelebrateMs = 1500;
  static constexpr uint32_t kGlowMs = 300;
  static constexpr uint32_t kWiggleMs = 700;
  static constexpr uint32_t kHoldLoopMs = 350;     // a longer hold keeps looping
  static constexpr uint32_t kPointerLostMs = 250;  // no pointer stream: treat as lifted
  static constexpr int16_t kBendDeadZone = 12;     // px before a slide starts bending
  // Recording stops itself at ISampler::kMaxRecordMs; this is a backstop.
  static constexpr uint32_t kRecordTimeoutMs = ISampler::kMaxRecordMs + 700;

  explicit SoundJarGame(ISampler* sampler = nullptr) : sampler_(sampler) {}
  void attach_sampler(ISampler* sampler) { sampler_ = sampler; }

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override { return 0; }  // AGAIN/HELP are Global
  void on_pause() override;
  void on_resume() override;

  // Raw touch state every UI loop while this game is shown (hold and slide).
  void pointer(bool pressed, int16_t x, int16_t y);
  // The parent gate opened: drop the clip (privacy) and start fresh.
  void privacy_wipe();

  Phase phase() const { return phase_; }
  bool active() const { return ctx_ != nullptr; }
  bool effect_on(std::size_t i) const { return i < kEffectCount && (effects_ & effect_bit(i)) != 0; }
  uint8_t effects() const { return effects_; }
  bool pad_lit(std::size_t i) const { return i < kPadCount && (glow_ms_[i] > 0 || held_pad_ == static_cast<int>(i)); }
  int held_pad() const { return held_pad_; }
  bool looping() const { return held_pad_ >= 0 && hold_ms_ >= kHoldLoopMs; }
  // Slide position as a fractional pad index x100 (0..400) while bending, else -1.
  int bend_position() const { return bending_ ? bend_pos_ : -1; }
  bool jar_lit() const { return jar_glow_ms_ > 0; }
  bool wiggling() const { return wiggle_ms_ > 0; }
  uint32_t wiggle_ms() const { return wiggle_ms_; }
  // Record button shows a sparkle (built-in sounds) instead of a red dot.
  bool sparkle_mode() const { return !sampler_ || !sampler_->mic_available(); }
  uint8_t builtin_index() const { return builtin_; }
  uint32_t record_ms() const { return phase_ == Phase::RECORDING ? phase_ms_ : 0; }
  uint32_t phase_ms() const { return phase_ms_; }
  uint32_t sounds_caught() const { return caught_; }
  uint32_t plays() const { return plays_; }
  int last_pad() const { return last_pad_; }
  const ISampler* sampler() const { return sampler_; }

  // Q16 playback rate for a slide position (pad index x100, 0..400).
  static uint32_t rate_for_position(int pos_x100);

 private:
  void fresh_jar();
  void to_play(bool prompt);
  void press_pad(std::size_t pad);
  void lift_pad();
  void toggle_effect(std::size_t i);
  void play_once(uint32_t rate);
  void press_record();
  void finish_recording(ISampler::RecordResult result);

  ISampler* sampler_;
  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  uint32_t phase_ms_ = 0;
  uint8_t effects_ = 0;
  uint8_t builtin_ = 0;
  uint32_t glow_ms_[kPadCount]{};
  uint32_t jar_glow_ms_ = 0;
  uint32_t wiggle_ms_ = 0;
  int held_pad_ = -1;
  uint32_t hold_ms_ = 0;
  uint32_t since_pointer_ms_ = 0;
  int16_t press_x_ = 0;
  bool bending_ = false;
  int bend_pos_ = -1;
  uint32_t sent_rate_ = 0;
  bool stopping_ = false;
  int last_pad_ = -1;
  uint32_t caught_ = 0;
  uint32_t plays_ = 0;
};

}  // namespace little_arcade::sound_jar
