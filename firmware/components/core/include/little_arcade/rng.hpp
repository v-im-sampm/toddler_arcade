#pragma once
#include <cstddef>
#include <cstdint>

namespace little_arcade {

// Small deterministic PRNG (xorshift32). Same seed -> same sequence on host and
// device, so scenes can be replayed in tests.
class Rng {
 public:
  explicit constexpr Rng(uint32_t seed = 1) : state_(seed ? seed : kFallbackSeed) {}

  constexpr void reseed(uint32_t seed) { state_ = seed ? seed : kFallbackSeed; }

  constexpr uint32_t next() {
    uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x;
    return x;
  }

  // Uniform-enough value in [0, bound). bound == 0 returns 0.
  constexpr uint32_t below(uint32_t bound) { return bound ? next() % bound : 0; }

  template <typename T, std::size_t N>
  constexpr void shuffle(T (&items)[N]) {
    shuffle(items, N);
  }

  template <typename T>
  constexpr void shuffle(T* items, std::size_t count) {
    for (std::size_t i = count; i > 1; --i) {
      const std::size_t j = below(static_cast<uint32_t>(i));
      T tmp = items[i - 1];
      items[i - 1] = items[j];
      items[j] = tmp;
    }
  }

 private:
  static constexpr uint32_t kFallbackSeed = 0x9E3779B9u;
  uint32_t state_;
};

}  // namespace little_arcade
