#include <vector>

#include "little_arcade/games/silly_faces_art.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;

int main() {
  using namespace silly_faces;
  std::vector<std::vector<uint32_t>> emotions;
  for (int i = 0; i < static_cast<int>(kEmotionCount); ++i) {
    std::vector<uint32_t> px(art::kFaceW * art::kFaceH);
    ui::Canvas c{px.data(), art::kFaceW, art::kFaceH};
    art::paint_face(c, static_cast<Emotion>(i), static_cast<Emotion>(i), 0, Topper::ANTENNAE);
    EXPECT(ui::count_opaque(c) > 4000);
    EXPECT((c.at(0, 0) >> 24) == 0);
    for (const auto& prior : emotions) EXPECT(prior != px);
    emotions.push_back(px);
  }
  std::vector<std::vector<uint32_t>> looks;
  for (int i = 0; i < static_cast<int>(kBodyColorCount); ++i) {
    std::vector<uint32_t> px(art::kFaceW * art::kFaceH);
    ui::Canvas c{px.data(), art::kFaceW, art::kFaceH};
    art::paint_face(c, Emotion::HAPPY, Emotion::HAPPY, static_cast<uint8_t>(i),
                    static_cast<Topper>(i % kTopperCount));
    for (const auto& prior : looks) EXPECT(prior != px);
    looks.push_back(px);
  }
  return testing::finish("silly_faces_art_test");
}
