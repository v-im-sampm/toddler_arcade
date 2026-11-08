#pragma once
// Fades a game's ExternalScore in and out when the launcher enters or leaves that game, so a
// score with an ambient bed (Color Pop's waves) never keeps playing over the menu or another
// game. While closed and silent the inner score is not rendered at all: its clock and command
// queue pause and resume on the next entry. UI thread: set_open(); audio thread: render_add().
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "little_arcade/audio/external_score.hpp"

namespace launcher {

class GatedScore final : public little_arcade::audio::ExternalScore {
 public:
  static constexpr std::size_t kBlock = 64;
  static constexpr int32_t kFadeSamples = 1280;  // 80 ms at 16 kHz

  explicit GatedScore(little_arcade::audio::ExternalScore& inner) : inner_(inner) {}
  void set_open(bool open) { open_.store(open, std::memory_order_relaxed); }
  bool open() const { return open_.load(std::memory_order_relaxed); }
  int32_t gain() const { return gain_; }  // 0..kFadeSamples; audio thread / tests

  void render_add(int32_t* out, std::size_t n) override {
    const bool open = open_.load(std::memory_order_relaxed);
    if (!open && gain_ == 0) return;
    if (open && gain_ == kFadeSamples) { inner_.render_add(out, n); return; }
    int32_t tmp[kBlock];
    while (n) {
      const std::size_t m = std::min(n, kBlock);
      std::fill(tmp, tmp + m, 0);
      inner_.render_add(tmp, m);
      for (std::size_t i = 0; i < m; ++i) {
        gain_ = open ? std::min<int32_t>(gain_ + 1, kFadeSamples) : std::max<int32_t>(gain_ - 1, 0);
        out[i] += static_cast<int32_t>(static_cast<int64_t>(tmp[i]) * gain_ / kFadeSamples);
      }
      out += m;
      n -= m;
    }
  }

 private:
  little_arcade::audio::ExternalScore& inner_;
  std::atomic<bool> open_{false};
  int32_t gain_ = 0;
};

}  // namespace launcher
