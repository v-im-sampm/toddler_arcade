#include <vector>

#include "little_arcade/games/rocket_rescue_art.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;

int main() {
  using namespace rocket_rescue;
  std::vector<std::vector<uint32_t>> planets;
  for (int look = 0; look < static_cast<int>(kPlanetCount); ++look) {
    std::vector<uint32_t> px(art::kPlanetW * art::kPlanetH);
    ui::Canvas c{px.data(), art::kPlanetW, art::kPlanetH};
    art::paint_planet(c, static_cast<Look>(look), static_cast<Friend>(look));
    EXPECT(ui::count_opaque(c) > 700);
    EXPECT((c.at(0, 0) >> 24) == 0);
    for (const auto& prior : planets) EXPECT(prior != px);
    planets.push_back(px);
  }
  std::vector<uint32_t> rocket(art::kRocketSize * art::kRocketSize);
  ui::Canvas c{rocket.data(), art::kRocketSize, art::kRocketSize};
  art::paint_rocket(c);
  EXPECT(ui::count_opaque(c) > 200);
  return testing::finish("rocket_rescue_art_test");
}
