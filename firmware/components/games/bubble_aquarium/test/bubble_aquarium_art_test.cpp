#include <vector>

#include "little_arcade/games/bubble_aquarium_art.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;

namespace {
std::vector<uint32_t> render(int w, int h, void (*paint)(ui::Canvas&)) {
  std::vector<uint32_t> px(static_cast<std::size_t>(w * h));
  ui::Canvas c{px.data(), w, h};
  paint(c);
  return px;
}
}

int main() {
  using namespace bubble_aquarium;
  std::vector<std::vector<uint32_t>> fish;
  for (std::size_t i = 0; i < kFishTypeCount; ++i) {
    std::vector<uint32_t> px(art::kFishW * art::kFishH);
    ui::Canvas c{px.data(), art::kFishW, art::kFishH};
    art::paint_fish(c, static_cast<FishType>(i));
    EXPECT(ui::count_opaque(c) > 400);
    EXPECT((c.at(0, 0) >> 24) == 0);
    for (const auto& prior : fish) EXPECT(prior != px);
    fish.push_back(px);
  }
  std::vector<std::vector<uint32_t>> visitors;
  for (std::size_t i = 0; i < kVisitorCount; ++i) {
    std::vector<uint32_t> px(art::kVisitorW * art::kVisitorH);
    ui::Canvas c{px.data(), art::kVisitorW, art::kVisitorH};
    art::paint_visitor(c, static_cast<Visitor>(i));
    EXPECT(ui::count_opaque(c) > 200);
    for (const auto& prior : visitors) EXPECT(prior != px);
    visitors.push_back(px);
  }
  auto bubble = render(art::kBubbleSize, art::kBubbleSize, art::paint_bubble);
  auto food = render(art::kFoodSize, art::kFoodSize, art::paint_food);
  EXPECT(bubble != food);
  EXPECT(ui::count_opaque(ui::Canvas{bubble.data(), art::kBubbleSize, art::kBubbleSize}) > 20);
  return testing::finish("bubble_aquarium_art_test");
}
