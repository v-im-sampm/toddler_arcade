#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

namespace little_arcade::rainbow_piano {

enum class Instrument : uint8_t { BELLS, FLUTE, DRUM };
inline constexpr std::size_t kInstrumentCount = 3;
inline constexpr std::size_t kKeyCount = 5;

// Each key has its own colour (red..purple, low..high) and its own shape icon
// so keys are never distinguished by colour alone.
enum class KeyShape : uint8_t { CIRCLE, SQUARE, TRIANGLE, HEART, STAR };
constexpr KeyShape shape_for_key(std::size_t key) { return static_cast<KeyShape>(key % kKeyCount); }

// Cue::NOTE variant = instrument * kKeyCount + key (0..14), so the audio layer
// can map each (instrument, pitch) pair to one pre-rendered asset.
constexpr uint8_t note_variant(Instrument instrument, std::size_t key) {
  return static_cast<uint8_t>(static_cast<std::size_t>(instrument) * kKeyCount + key);
}

enum class Phase : uint8_t { INTRO, PLAY, CELEBRATE };

// Pure scene logic; an LVGL view reads the accessors and renders. Owns no
// heap memory. All timing is driven by update(dt_ms).
//
// Free play: every key press emits Cue::NOTE synchronously inside handle(), in
// every phase, so response latency is bounded by the input path only. An
// optional "follow-light" glows on the next key of a short tune; pressing the
// lit key advances it and finishing the tune triggers a short celebration.
// Pressing any other key just plays that note; nothing is ever lost.
class RainbowPianoGame final : public IGame {
 public:
  static constexpr Rect kKeys[kKeyCount] = {
      {4, 120, 56, 112}, {68, 120, 56, 112}, {132, 120, 56, 112}, {196, 120, 56, 112}, {260, 120, 56, 112},
  };
  static constexpr Rect kInstrumentButtons[kInstrumentCount] = {
      {60, 64, 56, 48}, {132, 64, 56, 48}, {204, 64, 56, 48},
  };
  static constexpr std::size_t kTuneLength = 5;
  static constexpr uint32_t kIntroMs = 1200;
  static constexpr uint32_t kCelebrateMs = 1500;
  static constexpr uint32_t kKeyGlowMs = 300;

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override;

  Phase phase() const { return phase_; }
  Instrument instrument() const { return instrument_; }
  // True while a key shows its "pressed" glow (visual counterpart of NOTE).
  bool key_lit(std::size_t key) const { return key < kKeyCount && glow_ms_[key] > 0; }
  // Key the follow-light points at, or -1 during INTRO/CELEBRATE.
  int follow_key() const {
    return phase_ == Phase::PLAY ? static_cast<int>(tune_[tune_pos_]) : -1;
  }
  std::size_t tune_progress() const { return tune_pos_; }
  uint8_t tune_note(std::size_t i) const { return i < kTuneLength ? tune_[i] : 0; }
  uint32_t notes_played() const { return notes_played_; }
  uint32_t tunes_completed() const { return tunes_; }
  int last_key() const { return last_key_; }
  bool active() const { return ctx_ != nullptr; }

 private:
  void new_tune();
  void start_play(bool with_prompt);
  void press_key(std::size_t key);
  void select_instrument(std::size_t index);

  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  Instrument instrument_ = Instrument::BELLS;
  uint8_t tune_[kTuneLength]{};
  std::size_t tune_pos_ = 0;
  uint32_t glow_ms_[kKeyCount]{};
  uint32_t phase_ms_ = 0;
  uint32_t notes_played_ = 0;
  uint32_t tunes_ = 0;
  int last_key_ = -1;
};

}  // namespace little_arcade::rainbow_piano
