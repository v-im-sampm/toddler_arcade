#pragma once

#include <initializer_list>

#include "little_arcade/geometry.hpp"
#include "little_arcade/ui/fx.hpp"
#include "little_arcade/ui/shade.hpp"

namespace little_arcade::ui {

enum class World : uint8_t {
  BEACH,
  NEST,
  BRIDGE,
  WORKSHOP,
  FARM,
  SNACK_SHACK,
  ECHO_CAVE,
  REEF,
  SPACE,
  MIRROR_POND,
};

namespace scene_art {

inline void cloud(Canvas& c, float x, float y, float s, uint8_t opa = 190) {
  fill_ellipse(c, x, y + 3 * s, 14 * s, 6 * s, 0, tok::kInk, static_cast<uint8_t>(opa / 7));
  fill_circle(c, x - 8 * s, y, 8 * s, tok::kWhite, opa);
  fill_circle(c, x + 1 * s, y - 4 * s, 11 * s, tok::kWhite, opa);
  fill_circle(c, x + 12 * s, y, 8 * s, tok::kWhite, opa);
}

inline void waves(Canvas& c, int y, uint32_t color, uint8_t opa, float phase) {
  Path p;
  for (int x = 0; x <= kScreenWidth; x += 12) {
    const float yy = static_cast<float>(y) + 2.0f * std::sin(static_cast<float>(x) * 0.045f + phase);
    if (x == 0) p.move(static_cast<float>(x), yy);
    else p.line(static_cast<float>(x), yy);
  }
  stroke_path(c, p, 1.5f, color, false, opa);
}

inline void beach(Canvas& c, bool reef) {
  fill_vgradient(c, 0, 0, kScreenWidth, 72, tok::kSkyDay, tok::kSkyHaze);
  fill_vgradient(c, 0, 68, kScreenWidth, 212, reef ? 0x75CBEF : tok::kSeaTop, reef ? 0x207AB8 : tok::kSeaBottom);
  cloud(c, 44, 26, 0.9f, 160);
  cloud(c, 226, 42, 0.7f, 125);
  waves(c, 70, tok::kWhite, 110, 0.0f);
  waves(c, 112, tok::kWhite, 54, 1.2f);
  waves(c, 176, tok::kWhite, 42, 2.1f);

  Path shore;
  shore.move(0, 201).quad(42, 196, 74, 202).quad(110, 207, 151, 201).quad(204, 194, 242, 201)
      .quad(280, 207, 320, 201).line(320, 240).line(0, 240);
  fill_path(c, shore, reef ? 0xE9D3A5 : tok::kSand);
  stroke_path(c, Path{}.move(0, 201).quad(42, 196, 74, 202).quad(110, 207, 151, 201)
                   .quad(204, 194, 242, 201).quad(280, 207, 320, 201),
              2.0f, tok::kSandShadow, false, 210);

  if (reef) {
    for (int x : {26, 290}) {
      Path coral;
      coral.move(static_cast<float>(x), 218).line(static_cast<float>(x - 5), 200)
          .line(static_cast<float>(x - 15), 193).line(static_cast<float>(x - 13), 185)
          .line(static_cast<float>(x - 4), 194).line(static_cast<float>(x), 181)
          .line(static_cast<float>(x + 3), 197).line(static_cast<float>(x + 13), 188)
          .line(static_cast<float>(x + 15), 195).line(static_cast<float>(x + 5), 204)
          .line(static_cast<float>(x + 6), 218);
      shaded_path(c, coral, tok::kPink, 1.2f);
    }
    for (int i = 0; i < 12; ++i) {
      const float x = static_cast<float>((i * 53 + 17) % 310 + 5);
      const float y = static_cast<float>((i * 37 + 85) % 182 + 8);
      stroke_circle(c, x, y, 2.0f + static_cast<float>(i % 3), 1.2f, tok::kWhite);
      fill_circle(c, x - 1, y - 1, 0.8f, tok::kWhite, 190);
    }
  } else {
    for (int i = 0; i < 48; ++i) {
      const float x = static_cast<float>((i * 67 + 13) % kScreenWidth);
      const float y = static_cast<float>(207 + (i * 29) % 30);
      fill_circle(c, x, y, 0.7f + static_cast<float>(i % 3) * 0.25f, tok::kSandShadow, 110);
    }
  }
}

inline void workshop(Canvas& c, bool cool) {
  fill_vgradient(c, 0, 0, kScreenWidth, kScreenHeight, cool ? 0xC8F3F4 : 0xFFE0A7,
                 cool ? 0x80D4DE : 0xFFB36A);
  fill_ellipse(c, 42, 86, 46, 42, 0, cool ? tok::kTeal.light : tok::kYellow.light, 90);
  for (int i = 0; i < 5; ++i) {
    const int x = 44 + i * 58;
    fill_circle(c, static_cast<float>(x), 186, 4, tok::kBrown.light, 110);
    fill_circle(c, static_cast<float>(x), 186, 1.4f, tok::kCream.light, 200);
  }
  fill_vgradient(c, 0, 205, kScreenWidth, kScreenHeight, tok::kBrown.light, tok::kBrown.base);
  for (int y : {216, 231}) waves(c, y, tok::kBrown.shadow, 65, 0.0f);
}

inline void farm(Canvas& c) {
  fill_vgradient(c, 0, 0, kScreenWidth, 166, tok::kSkyDay, tok::kSkyHaze);
  cloud(c, 66, 42, 0.8f, 145);
  cloud(c, 250, 26, 0.55f, 120);
  fill_ellipse(c, 42, 178, 126, 35, 0, tok::kGrassDark, 230);
  fill_ellipse(c, 244, 181, 145, 39, 0, tok::kLime.base, 245);
  fill_vgradient(c, 0, 192, kScreenWidth, kScreenHeight, tok::kLime.light, tok::kGrass);
  for (int i = 0; i < 9; ++i) {
    const float x = static_cast<float>(18 + i * 37);
    const float y = static_cast<float>(220 + i % 3 * 5);
    fill_circle(c, x, y, 3.2f, i % 2 ? tok::kYellow.base : tok::kPink.base);
    fill_circle(c, x, y, 1.2f, tok::kCream.light);
    stroke_path(c, Path{}.move(x, y + 3).line(x - 1, y + 11), 1.3f, tok::kGrassDark);
  }
}

inline void nest(Canvas& c) {
  fill_vgradient(c, 0, 0, kScreenWidth, kScreenHeight, 0xD5F1FF, 0xF9DDE9);
  cloud(c, 56, 48, 0.65f, 110);
  cloud(c, 270, 68, 0.5f, 100);
  fill_ellipse(c, 160, 224, 112, 28, 0, tok::kBrown.shadow, 190);
  fill_ellipse(c, 160, 218, 104, 26, 0, tok::kBrown.base, 250);
  fill_ellipse(c, 160, 212, 90, 20, 0, tok::kBrown.light, 255);
  for (int i = 0; i < 9; ++i) {
    const float x = static_cast<float>(76 + i * 21);
    Path twig;
    twig.move(x, 232.0f)
        .quad(x + 25.0f, 218.0f - static_cast<float>(i % 2) * 4.0f, x + 45.0f, 237.0f);
    stroke_path(c, twig, 2.2f, i % 2 ? tok::kBrown.shadow : tok::kBrown.base);
  }
  for (int i = 0; i < 4; ++i) {
    const float x = static_cast<float>(48 + i * 76);
    fill_ellipse(c, x, 178, 14, 6, -0.5f, tok::kGreen.light, 175);
  }
}

inline void bridge(Canvas& c) {
  fill_vgradient(c, 0, 0, kScreenWidth, kScreenHeight, 0xF6DBFF, 0xACDDFC);
  const float cx = 160, cy = 190;
  const uint32_t colors[] = {tok::kPink.base, tok::kOrange.base, tok::kYellow.base, tok::kGreen.base,
                             tok::kBlue.base, tok::kPurple.base};
  for (int i = 0; i < 6; ++i) {
    const float r = 104.0f - static_cast<float>(i) * 11.0f;
    paint(c, 44, 40, 276, 202, colors[i], 255, [=](float x, float y) {
      const float dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
      return d2 <= (r + 4) * (r + 4) && d2 >= (r - 4) * (r - 4) && y <= cy;
    }, 2);
  }
  cloud(c, 36, 185, 0.95f, 230);
  cloud(c, 282, 181, 0.95f, 230);
  fill_ellipse(c, 160, 226, 160, 23, 0, tok::kTeal.light, 130);
}

inline void snack_shack(Canvas& c) {
  fill_vgradient(c, 0, 0, kScreenWidth, 204, 0xFFD8B8, 0xFFB5A7);
  fill_ellipse(c, 260, 74, 74, 62, 0, tok::kPink.light, 95);
  fill_vgradient(c, 0, 200, kScreenWidth, kScreenHeight, tok::kOrange.light, tok::kOrange.base);
  for (int y : {211, 228}) waves(c, y, tok::kOrange.shadow, 75, 0.0f);
  fill_ellipse(c, 42, 198, 30, 12, 0, tok::kCream.light, 100);
}

inline void cave(Canvas& c) {
  fill_vgradient(c, 0, 0, kScreenWidth, kScreenHeight, 0x383B78, 0x20234F);
  fill_ellipse(c, 156, 142, 128, 112, 0, tok::kPurple.base, 35);
  for (int i = 0; i < 8; ++i) {
    const int x = i * 47 - 8;
    Path crystal;
    crystal.move(static_cast<float>(x), 0).line(static_cast<float>(x + 12), 0)
        .line(static_cast<float>(x + 9), static_cast<float>(24 + (i * 13) % 24))
        .line(static_cast<float>(x + 5), 54).line(static_cast<float>(x + 2), static_cast<float>(20 + (i * 13) % 24));
    fill_path(c, crystal, i % 2 ? 0x5A5D9A : 0x444783, 220);
  }
  for (int i = 0; i < 16; ++i) {
    const float x = static_cast<float>((i * 73 + 19) % kScreenWidth);
    const float y = static_cast<float>((i * 41 + 24) % 190 + 20);
    fill_circle(c, x, y, 1.0f + static_cast<float>(i % 2), tok::kTeal.light, 130);
  }
  fill_ellipse(c, 160, 250, 164, 40, 0, 0x171A3D, 255);
  waves(c, 218, tok::kPurple.light, 100, 0.0f);
}

inline void reef(Canvas& c) {
  beach(c, true);
  fill_ellipse(c, 160, 22, 185, 72, 0, tok::kTeal.light, 20);
}

inline void space(Canvas& c) {
  fill_vgradient(c, 0, 0, kScreenWidth, kScreenHeight, 0x2C326D, 0x141A43);
  fill_ellipse(c, 172, 106, 124, 84, 0, tok::kPurple.base, 38);
  fill_ellipse(c, 66, 156, 56, 40, 0, tok::kPink.base, 30);
  for (int i = 0; i < 36; ++i) {
    const float x = static_cast<float>((i * 83 + 17) % kScreenWidth);
    const float y = static_cast<float>((i * 47 + 11) % kScreenHeight);
    const float r = i % 7 == 0 ? 2.2f : 0.9f + static_cast<float>(i % 2) * 0.4f;
    fill_circle(c, x, y, r, i % 4 == 0 ? tok::kYellow.light : tok::kWhite, 180);
  }
  Path orbit;
  orbit.move(12, 198).quad(156, 168, 308, 202);
  stroke_path(c, orbit, 1.0f, tok::kBlue.light, false, 85);
}

inline void mirror_pond(Canvas& c) {
  fill_vgradient(c, 0, 0, kScreenWidth, 96, 0xF8D8E9, 0xE8F4FF);
  fill_vgradient(c, 0, 88, kScreenWidth, kScreenHeight, 0xB6E9EB, 0x61C5CD);
  cloud(c, 50, 37, 0.6f, 115);
  for (int i = 0; i < 4; ++i) {
    const float x = static_cast<float>(26 + i * 91);
    const float y = 199.0f + static_cast<float>(i % 2) * 14.0f;
    fill_ellipse(c, x, y, 27.0f, 8.0f, 0.15f, tok::kGreen.base, 240);
    fill_circle(c, x, y - 5.0f, 4.0f, tok::kPink.light, 220);
  }
  waves(c, 123, tok::kWhite, 75, 0.0f);
  waves(c, 158, tok::kWhite, 48, 2.0f);
}

inline void paint(Canvas& c, World world) {
  c.clear();
  switch (world) {
    case World::BEACH: beach(c, false); break;
    case World::NEST: nest(c); break;
    case World::BRIDGE: bridge(c); break;
    case World::WORKSHOP: workshop(c, false); break;
    case World::FARM: farm(c); break;
    case World::SNACK_SHACK: snack_shack(c); break;
    case World::ECHO_CAVE: cave(c); break;
    case World::REEF: reef(c); break;
    case World::SPACE: space(c); break;
    case World::MIRROR_POND: mirror_pond(c); break;
  }
}

}  // namespace scene_art

inline lv_obj_t* show_backdrop(lv_obj_t* parent, World world) {
  Backdrop& backdrop = scene_backdrop();
  Canvas c = backdrop.begin();
  if (!c.px) return nullptr;
  scene_art::paint(c, world);
  lv_obj_t* img = backdrop.show(parent);
  if (img) lv_obj_move_to_index(img, 0);
  return img;
}

}  // namespace little_arcade::ui
