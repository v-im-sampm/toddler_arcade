#pragma once
#include <cstddef>
#include <cstdint>

#include "little_arcade/game.hpp"
#include "little_arcade/geometry.hpp"

namespace little_arcade::silly_faces {

// The four expressions. Every eye style and every mouth style belongs to one
// emotion; when both match, the monster acts that feeling out. All four are
// treated the same (sad/sleepy are never "wrong").
enum class Emotion : uint8_t { HAPPY, SLEEPY, SURPRISED, SAD };
inline constexpr std::size_t kEmotionCount = 4;

// Original monster look (no licensed characters): a round blob body in one of
// four colours with one of three toppers.
enum class Topper : uint8_t { ANTENNAE, HORNS, TUFT };
inline constexpr std::size_t kTopperCount = 3;
inline constexpr std::size_t kBodyColorCount = 4;

enum class Phase : uint8_t { INTRO, PLAY, CELEBRATE };

// Which part of the scene is doing a short "boing" (visual counterpart of TAP).
enum class Part : uint8_t { NONE, EYES, MOUTH, WHOLE };

struct Face {
  Emotion eyes;
  Emotion mouth;
  constexpr bool matched() const { return eyes == mouth; }
};

// REACTION variants: 0..3 = emotion acted out, kSillyReaction = giggle for a
// mixed face. TAP variants identify what was touched.
inline constexpr uint8_t kSillyReaction = 4;
inline constexpr uint8_t kTapEyes = 0;
inline constexpr uint8_t kTapMouth = 1;
inline constexpr uint8_t kTapNewMonster = 2;
inline constexpr uint8_t kTapMix = 3;
inline constexpr uint8_t kCelebrateEmotion = 0;
inline constexpr uint8_t kCelebrateAllFour = 1;

// Pure scene logic; an LVGL view reads the accessors and renders. Owns no heap
// memory. All timing is driven by update(dt_ms). There is no grading: any face
// is fine, and no input ever produces TRY_AGAIN.
class SillyFacesGame final : public IGame {
 public:
  // Primary targets (all inside kPlayArea).
  static constexpr Rect kEyesZone{88, 68, 144, 64};
  static constexpr Rect kMouthZone{88, 144, 144, 64};
  static constexpr Rect kShowButton{8, 72, 64, 64};         // touch twin of YES
  static constexpr Rect kMixButton{8, 152, 64, 64};         // touch twin of NO
  static constexpr Rect kNewMonsterButton{248, 72, 64, 64};  // touch twin of NEXT
  // Top-bar prompt badge (between Home and Mic): touch twin of AGAIN/HELP.
  static constexpr Rect kPromptBadge{132, 4, 56, 56};

  static constexpr std::size_t kTargetCount = 6;
  static constexpr Rect kTargets[kTargetCount] = {kEyesZone,  kMouthZone,        kShowButton,
                                                  kMixButton, kNewMonsterButton, kPromptBadge};

  static constexpr uint32_t kIntroMs = 1200;
  static constexpr uint32_t kCelebrateMs = 1500;
  static constexpr uint32_t kBounceMs = 300;
  static constexpr uint32_t kHintMs = 1500;
  static constexpr uint32_t kIdleHintMs = 8000;  // gentle visual-only nudge, never a failure
  static constexpr uint8_t kAllEmotionsMask = 0x0F;

  void enter(GameContext& context) override;
  void update(uint32_t dt_ms) override;
  void handle(const InputEvent& event) override;
  void exit() override;
  uint32_t active_contexts() const override;

  Phase phase() const { return phase_; }
  Face face() const { return face_; }
  uint8_t body_color() const { return body_color_; }
  Topper topper() const { return topper_; }
  // Emotion being acted out during CELEBRATE (only meaningful if showing_silly() is false).
  Emotion showing() const { return showing_; }
  bool showing_silly() const { return showing_silly_; }
  // Bit i set once emotion i has been made (sticker row; never cleared while playing).
  uint8_t emotions_made() const { return made_mask_; }
  uint32_t faces_made() const { return faces_made_; }
  Part bouncing() const { return bounce_ms_ > 0 ? bounce_part_ : Part::NONE; }
  bool hint_active() const { return hint_ms_ > 0; }
  bool active() const { return ctx_ != nullptr; }

 private:
  void start_play();
  void cycle_eyes();
  void cycle_mouth();
  void after_part_change();
  void show_face();
  void mix_face();
  void new_monster();
  void replay_prompt();
  void bounce(Part part);
  void randomize_mixed_face();
  void celebrate(bool silly, Emotion emotion);

  GameContext* ctx_ = nullptr;
  Phase phase_ = Phase::INTRO;
  Face face_{Emotion::HAPPY, Emotion::SLEEPY};
  uint8_t body_color_ = 0;
  Topper topper_ = Topper::ANTENNAE;
  Emotion showing_ = Emotion::HAPPY;
  bool showing_silly_ = false;
  uint8_t made_mask_ = 0;
  uint32_t faces_made_ = 0;
  uint32_t phase_ms_ = 0;
  uint32_t idle_ms_ = 0;
  uint32_t bounce_ms_ = 0;
  uint32_t hint_ms_ = 0;
  Part bounce_part_ = Part::NONE;
};

}  // namespace little_arcade::silly_faces
