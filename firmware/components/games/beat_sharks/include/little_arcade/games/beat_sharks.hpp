#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"
#include "little_arcade/sampler.hpp"

namespace little_arcade::beat_sharks {

inline constexpr std::size_t kRows = 3;   // 0 = your sound, 1 = taps, 2 = drums
inline constexpr std::size_t kSteps = 6;  // one loop = 6 eighth notes
// A cell holds 0 (empty water) or sound 1..kSoundsPerRow. Sound 1 is the big
// shark (lowest), sound 3 the small one (highest).
inline constexpr uint8_t kSoundsPerRow = 3;
inline constexpr std::size_t kTempoCount = 3;  // turtle, walk, rabbit

enum class Phase : uint8_t { INTRO, PLAY, RECORDING, CELEBRATE };
enum class Celebration : uint8_t { NONE, NEW_SOUND, FULL_OCEAN };

// Beat Sharks (docs/prd/beat_sharks.md, games/beat_sharks.md): a 3 x 6 step
// sequencer. Tapping a cell cycles empty -> big -> middle -> small shark ->
// empty; every shark the fin passes sings its row's sound. Pure scene logic:
// no heap, all timing from update(dt_ms).
//
// Rows 1-2 play synth sounds through IFeedback (Cue::NOTE, variant
// row * kSoundsPerRow + sound - 1). Row 0 plays the shared ISampler clip at
// three pitches; without a sampler it falls back to Cue::NOTE variants 0-2.
// Without a microphone the Record button becomes a sparkle that cycles the
// sampler's built-in sounds.
//
// Privacy: the clip is wiped on exit, pause/sleep and parent-gate entry.
class BeatSharksGame final : public IGame {
 public:
  static constexpr Rect kPlayButton{60, 0, 56, 56};
  static constexpr Rect kRecordButton{120, 0, 56, 56};
  // Stops short of the Mic corner (kMicButton), which the games never get.
  static constexpr Rect kTempoButton{180, 0, 80, 56};
  static constexpr int16_t kCellX0 = 6, kCellPitchX = 52, kCellW = 48;
  static constexpr int16_t kCellY0 = 62, kCellPitchY = 60, kCellH = 52;
  static constexpr Rect cell_rect(std::size_t row, std::size_t step) {
    return Rect{static_cast<int16_t>(kCellX0 + kCellPitchX * static_cast<int>(step)),
                static_cast<int16_t>(kCellY0 + kCellPitchY * static_cast<int>(row)), kCellW, kCellH};
  }

  static constexpr uint16_t kStepMs[kTempoCount] = {500, 375, 280};
  static constexpr uint8_t kDefaultTempo = 1;
  // Voice row pitches, big shark first: -5, 0 and +7 semitones.
  static constexpr uint32_t kVoiceRates[kSoundsPerRow] = {49097, ISampler::kRateUnity, 98193};

  static constexpr uint32_t kIntroMs = 1200;
  static constexpr uint32_t kCelebrateMs = 1500;
  static constexpr uint32_t kHopMs = 160;
  static constexpr uint32_t kGlowMs = 300;
  static constexpr uint32_t kWiggleMs = 700;
  // The fin reaching a just-filled cell this soon plays it anyway: no preview.
  static constexpr uint32_t kPreviewGuardMs = 60;
  // Short sounds loop better: stop at half of the engine's 3 s maximum.
  static constexpr uint16_t kRecordStopPermille = 500;
  static constexpr uint32_t kRecordTimeoutMs = ISampler::kMaxRecordMs + 700;

  explicit BeatSharksGame(ISampler* sampler = nullptr) : sampler_(sampler) {}
  void attach_sampler(ISampler* sampler) { sampler_ = sampler; }

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override { return 0; }  // AGAIN/HELP are Global
  void on_pause() override;
  void on_resume() override;

  // The parent gate opened: drop the clip (privacy) and stop the loop.
  void privacy_wipe();

  Phase phase() const { return phase_; }
  bool active() const { return ctx_ != nullptr; }
  uint32_t phase_ms() const { return phase_ms_; }
  Celebration celebration() const { return phase_ == Phase::CELEBRATE ? celebration_ : Celebration::NONE; }
  uint8_t cell(std::size_t row, std::size_t step) const { return row < kRows && step < kSteps ? cells_[row][step] : 0; }
  bool hopping(std::size_t row, std::size_t step) const {
    return row < kRows && step < kSteps && hop_ms_[row][step] > 0;
  }
  bool playing() const { return playing_; }
  // Column the fin is on (the last step that sounded), or -1 before the first.
  int step() const { return step_; }
  // How far the fin has swum toward the next column, 0..999.
  uint16_t step_progress_permille() const {
    return playing_ ? static_cast<uint16_t>(accum_ms_ * 1000u / kStepMs[tempo_]) : 0;
  }
  uint8_t tempo() const { return tempo_; }
  bool sparkle_mode() const { return !sampler_ || !sampler_->mic_available(); }
  uint8_t builtin_index() const { return builtin_; }
  uint32_t record_ms() const { return phase_ == Phase::RECORDING ? phase_ms_ : 0; }
  bool record_lit() const { return record_glow_ms_ > 0; }
  bool wiggling() const { return wiggle_ms_ > 0; }
  uint32_t wiggle_ms() const { return wiggle_ms_; }
  bool full_ocean() const;
  uint32_t steps_played() const { return steps_played_; }
  uint32_t hits() const { return hits_; }
  uint32_t sounds_caught() const { return caught_; }
  const ISampler* sampler() const { return sampler_; }

 private:
  void reset_pattern();
  void to_play(bool prompt);
  void start_transport();
  void fire_step();
  void play_sound(std::size_t row, uint8_t sound);
  void tap_cell(std::size_t row, std::size_t step);
  void cycle_tempo();
  void press_record();
  void finish_recording(ISampler::RecordResult result);
  void place_voice_sharks_if_empty();
  void celebrate(Celebration kind);
  void check_full_ocean();

  ISampler* sampler_;
  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  Celebration celebration_ = Celebration::NONE;
  uint32_t phase_ms_ = 0;
  uint8_t cells_[kRows][kSteps]{};
  uint16_t hop_ms_[kRows][kSteps]{};
  bool playing_ = false;
  bool was_playing_ = false;  // transport state saved while recording
  int step_ = -1;
  uint32_t accum_ms_ = 0;
  uint8_t tempo_ = kDefaultTempo;
  uint8_t builtin_ = 0;
  uint32_t record_glow_ms_ = 0;
  uint32_t wiggle_ms_ = 0;
  bool stopping_ = false;
  bool ocean_armed_ = true;
  uint32_t steps_played_ = 0;
  uint32_t hits_ = 0;
  uint32_t caught_ = 0;
};

}  // namespace little_arcade::beat_sharks
