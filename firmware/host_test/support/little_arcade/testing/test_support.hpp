#pragma once
// Minimal host test helpers shared by core and game tests (host only).
#include <cstdint>
#include <cstdio>

#include "little_arcade/feedback.hpp"

namespace little_arcade::testing {

inline int& failures() {
  static int count = 0;
  return count;
}

inline void expect(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("FAIL %s:%d: %s\n", file, line, expr);
    ++failures();
  }
}

inline int finish(const char* suite) {
  if (failures()) {
    std::printf("%s: %d failure(s)\n", suite, failures());
    return 1;
  }
  std::printf("PASS: %s (host only; not device validation)\n", suite);
  return 0;
}

// Records cues so tests can assert on requested feedback.
class RecordingFeedback final : public IFeedback {
 public:
  static constexpr int kCapacity = 256;
  struct Entry {
    Cue cue;
    uint8_t variant;
  };
  void play(Cue cue, uint8_t variant) override {
    if (count < kCapacity) entries[count] = Entry{cue, variant};
    ++count;
  }
  int count_of(Cue cue) const {
    int n = 0;
    for (int i = 0; i < count && i < kCapacity; ++i) n += entries[i].cue == cue ? 1 : 0;
    return n;
  }
  bool any(Cue cue) const { return count_of(cue) > 0; }
  const Entry* last() const { return count > 0 && count <= kCapacity ? &entries[count - 1] : nullptr; }
  void clear() { count = 0; }

  Entry entries[kCapacity]{};
  int count = 0;
};

}  // namespace little_arcade::testing

#define EXPECT(cond) ::little_arcade::testing::expect(static_cast<bool>(cond), #cond, __FILE__, __LINE__)
