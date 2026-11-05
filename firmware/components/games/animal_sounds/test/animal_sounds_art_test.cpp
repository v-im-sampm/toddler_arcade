#include <cstdint>

#include "little_arcade/games/animal_sounds_art.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade::animal_sounds;
namespace ui = little_arcade::ui;

namespace {

uint64_t signature(const ui::Canvas& canvas) {
  uint64_t value = 1469598103934665603ull;
  for (int i = 0; i < canvas.w * canvas.h; ++i) {
    value ^= canvas.px[i];
    value *= 1099511628211ull;
  }
  return value;
}

void test_all_animals_and_clues_paint_distinct_art() {
  uint32_t pixels[art::kAnimalW * art::kAnimalH]{};
  uint32_t clue_pixels[art::kClueSize * art::kClueSize]{};
  uint64_t animal_signatures[kAnimalCount]{};
  uint64_t clue_signatures[kAnimalCount]{};
  for (std::size_t i = 0; i < kAnimalCount; ++i) {
    ui::Canvas animal_canvas{pixels, art::kAnimalW, art::kAnimalH};
    animal_canvas.clear();
    art::paint_animal(animal_canvas, static_cast<Animal>(i));
    EXPECT(ui::count_opaque(animal_canvas) > 500);
    animal_signatures[i] = signature(animal_canvas);

    ui::Canvas clue_canvas{clue_pixels, art::kClueSize, art::kClueSize};
    clue_canvas.clear();
    art::paint_clue(clue_canvas, clue_for(static_cast<Animal>(i)));
    EXPECT(ui::count_opaque(clue_canvas) > 100);
    clue_signatures[i] = signature(clue_canvas);
  }
  for (std::size_t i = 0; i < kAnimalCount; ++i) {
    for (std::size_t j = i + 1; j < kAnimalCount; ++j) {
      EXPECT(animal_signatures[i] != animal_signatures[j]);
      EXPECT(clue_signatures[i] != clue_signatures[j]);
    }
  }

  uint32_t tile_pixels[art::kTileW * art::kTileH]{};
  ui::Canvas tile_canvas{tile_pixels, art::kTileW, art::kTileH};
  art::paint_tile_art(tile_canvas);
  EXPECT(ui::count_opaque(tile_canvas) > 800);
}

}  // namespace

int main() {
  test_all_animals_and_clues_paint_distinct_art();
  return 0;
}
