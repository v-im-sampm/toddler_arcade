#pragma once
#include <cstdint>

namespace little_arcade::ui::tok {

// Design tokens from v2/design/tokens.json, mirrored by the quality-bar sketch
// (v2/reference/quality_bar.html). Every game scene paints with these so the
// arcade shares one look: shaded 3-tone hues, ink outlines, cream chrome,
// soft gradients and a warm hint glow.
struct Hue {
  uint32_t shadow;
  uint32_t base;
  uint32_t light;
};

inline constexpr Hue kRed{0xB8323A, 0xEF4B4F, 0xFF9A92};
inline constexpr Hue kOrange{0xC2621B, 0xFF8A2A, 0xFFC07F};
inline constexpr Hue kYellow{0xC99A0E, 0xFFD23F, 0xFFEB9C};
inline constexpr Hue kLime{0x6E9A1F, 0xA6D13B, 0xD6EE95};
inline constexpr Hue kGreen{0x1E8452, 0x2DB26E, 0x8EE0B4};
inline constexpr Hue kTeal{0x127A7D, 0x21B3B0, 0x8FE3DE};
inline constexpr Hue kSky{0x2B79B8, 0x4FB0F0, 0xA9DBFB};
inline constexpr Hue kBlue{0x2448A8, 0x3A6FE8, 0x9DB8FA};
inline constexpr Hue kPurple{0x6236A6, 0x8F5BE0, 0xC9AEF5};
inline constexpr Hue kPink{0xB23E86, 0xF06BB8, 0xFBB6DC};
inline constexpr Hue kBrown{0x7A4E2D, 0xB07A4F, 0xDDB892};
inline constexpr Hue kCream{0xD9C7A3, 0xFFF3D6, 0xFFFBEF};

inline constexpr uint32_t kInk = 0x3B3550;
inline constexpr uint32_t kInkSoft = 0x6F6888;
inline constexpr uint32_t kPaper = 0xFFF8EC;
inline constexpr uint32_t kNight = 0x1E2245;
inline constexpr uint32_t kWhite = 0xFFFFFF;
inline constexpr uint32_t kHintGlow = 0xFFE066;
inline constexpr uint32_t kCheek = 0xF06BB8;  // painted at ~45% opacity

// Scene roles.
inline constexpr uint32_t kSkyDay = 0xBFE6FF;
inline constexpr uint32_t kSkyHaze = 0xE6F5FF;
inline constexpr uint32_t kSkyDusk = 0xF8C9A8;
inline constexpr uint32_t kSkyNight = 0x2A2F66;
inline constexpr uint32_t kSeaTop = 0xA9DBFB;
inline constexpr uint32_t kSeaBottom = 0x6FBDF2;
inline constexpr uint32_t kSand = 0xF3DDB0;
inline constexpr uint32_t kSandShadow = 0xDDB892;
inline constexpr uint32_t kGrass = 0xA6D13B;
inline constexpr uint32_t kGrassDark = 0x6E9A1F;

// Motion.
inline constexpr float kSpringBouncyK = 380.0f;
inline constexpr float kSpringBouncyC = 14.0f;
inline constexpr float kSpringSoftK = 160.0f;
inline constexpr float kSpringSoftC = 18.0f;
inline constexpr float kSquashPressX = 0.88f;  // press: narrow and tall, then pop
inline constexpr float kSquashPressY = 1.14f;
inline constexpr float kBreathScale = 0.025f;
inline constexpr uint32_t kBreathPeriodMs = 2800;
inline constexpr float kBobPx = 2.0f;
inline constexpr float kWiggleDeg = 6.0f;
inline constexpr uint32_t kWiggleMs = 360;
inline constexpr uint32_t kGlowPeriodMs = 900;
inline constexpr uint32_t kMaxInputBlockMs = 300;

// Particles (device budget; the FX layer never exceeds kFxMaxParticles).
inline constexpr int kFxMaxParticles = 48;
inline constexpr int kFxMaxRings = 6;

}  // namespace little_arcade::ui::tok
