#include <cstdint>
#include <vector>

#include "little_arcade/games/hatch_a_pet_art.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::hatch_a_pet;

namespace {
struct Image {
  std::vector<uint32_t> px;
  ui::Canvas c;
  Image(int w, int h) : px(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0u), c{px.data(), w, h} {}
  uint32_t alpha(int x, int y) const { return c.at(x, y) >> 24; }
  // True when the outermost ring of pixels is fully transparent (art fits).
  bool border_clear() const {
    for (int x = 0; x < c.w; ++x) {
      if (alpha(x, 0) || alpha(x, c.h - 1)) return false;
    }
    for (int y = 0; y < c.h; ++y) {
      if (alpha(0, y) || alpha(c.w - 1, y)) return false;
    }
    return true;
  }
};

constexpr EggPattern kPatterns[] = {EggPattern::SPOTS, EggPattern::STRIPES, EggPattern::ZIGZAGS, EggPattern::STARS};
}  // namespace

int main() {
  // Crack geometry: endpoints, interpolation and clamping.
  EXPECT(art::crack_v(-0.5f) == 0.50f && art::crack_v(0.5f) == 0.52f);
  EXPECT(art::crack_v(-2.0f) == 0.50f && art::crack_v(2.0f) == 0.52f);
  const float mid = art::crack_v(-0.41f);
  EXPECT(mid > 0.45f && mid < 0.47f);
  EXPECT(art::visible_crack_points(0, 4) == 0);
  EXPECT(art::visible_crack_points(1, 4) >= 2);  // the first tap already shows a crack
  EXPECT(art::visible_crack_points(4, 4) == art::kCrackPoints);
  EXPECT(art::visible_crack_points(9, 4) == art::kCrackPoints);
  for (int need = HatchAPetGame::kMinTaps; need <= HatchAPetGame::kMaxTaps; ++need) {
    for (int k = 1; k < need; ++k) {
      EXPECT(art::visible_crack_points(k + 1, need) >= art::visible_crack_points(k, need));  // grows monotonically
    }
  }

  // Egg shape: wider below the join; the outline grow test is consistent.
  EXPECT(art::egg_inside(56, 70, 56, 0, 112, 136));
  EXPECT(!art::egg_inside(0, 0, 56, 0, 112, 136));
  EXPECT(art::egg_inside(56, -0.5f, 56, 0, 112, 136, 1.0f) && !art::egg_inside(56, -0.5f, 56, 0, 112, 136));

  for (const EggPattern p : kPatterns) {
    Image whole(art::kEggImgW, art::kEggImgH), top(art::kEggImgW, art::kEggImgH), bottom(art::kEggImgW, art::kEggImgH);
    art::paint_egg_image(whole.c, p, art::Piece::WHOLE);
    art::paint_egg_image(top.c, p, art::Piece::TOP);
    art::paint_egg_image(bottom.c, p, art::Piece::BOTTOM);
    EXPECT(whole.border_clear() && top.border_clear() && bottom.border_clear());
    EXPECT(whole.alpha(art::kEggImgW / 2, art::kEggImgH / 2) == 255);
    // Away from the crack, the two pieces reproduce the whole egg exactly and
    // never overlap; near the crack each piece only has its own side.
    int mismatches = 0;
    for (int y = 0; y < art::kEggImgH; ++y) {
      for (int x = 0; x < art::kEggImgW; ++x) {
        const float crack_y = 2 + art::crack_v((static_cast<float>(x) + 0.5f - art::kEggImgW * 0.5f) / 112.0f) * 136.0f;
        const float d = static_cast<float>(y) + 0.5f - crack_y;
        if (d < -2 && bottom.alpha(x, y)) ++mismatches;
        if (d > 2 && top.alpha(x, y)) ++mismatches;
        if ((d < -2 || d > 2) && (top.c.at(x, y) | bottom.c.at(x, y)) != whole.c.at(x, y)) ++mismatches;
      }
    }
    EXPECT(mismatches == 0);
    // The pattern colour is actually present on the shell.
    int coloured = 0;
    for (const uint32_t v : whole.px) coloured += (v & 0xFFFFFFu) == art::pattern_rgb(p) && (v >> 24) == 255;
    EXPECT(coloured > 300);

    Image icon(art::kEggIconW, art::kEggIconH);
    art::paint_egg_icon(icon.c, p);
    EXPECT(icon.border_clear());
    EXPECT(icon.alpha(art::kEggIconW / 2, art::kEggIconH / 2) == 255);
  }

  // Every creature fits its image (nothing clipped), is mostly solid around
  // the centre and differs from the others.
  std::vector<std::vector<uint32_t>> seen;
  for (std::size_t i = 0; i < kCreatureCount; ++i) {
    Image img(art::kCreatureImg, art::kCreatureImg);
    art::paint_creature_image(img.c, static_cast<Creature>(i));
    EXPECT(img.border_clear());
    EXPECT(img.alpha(art::kCreatureImg / 2, art::kCreatureImg / 2 + 20) == 255);
    const int opaque = ui::count_opaque(img.c);
    EXPECT(opaque > 5000 && opaque < art::kCreatureImg * art::kCreatureImg / 2);
    for (const auto& other : seen) EXPECT(other != img.px);
    seen.push_back(img.px);
  }

  Image tile(art::kTileW, art::kTileH);
  art::paint_tile_art(tile.c);
  EXPECT(tile.border_clear());
  EXPECT(ui::count_opaque(tile.c) > 1500);

  return testing::finish("hatch_a_pet_art_test");
}
