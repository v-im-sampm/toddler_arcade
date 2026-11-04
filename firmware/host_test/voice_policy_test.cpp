#include "little_arcade/testing/test_support.hpp"
#include "little_arcade/voice_policy.hpp"

using namespace little_arcade;
using Verdict = VoiceIntentFilter::Verdict;
using End = PushToTalkSession::EndReason;

namespace {

constexpr uint32_t kColorPop = Context::Colors | Context::Global;

void test_session_silence_timeout() {
  PushToTalkSession s;
  EXPECT(s.start(100));
  EXPECT(!s.start(200));  // repeated mic tap is harmless
  EXPECT(s.tick(4000) == End::NONE);
  s.on_voice_activity(3000);
  EXPECT(s.tick(6999) == End::NONE);
  EXPECT(s.tick(7000) == End::SILENCE);
  EXPECT(!s.listening() && s.last_end_reason() == End::SILENCE);
  EXPECT(s.tick(9000) == End::NONE);
}

void test_session_max_duration() {
  PushToTalkSession s;
  s.start(0);
  for (uint32_t t = 500; t < 8000; t += 500) {
    s.on_voice_activity(t);
    EXPECT(s.tick(t) == End::NONE);
  }
  s.on_voice_activity(8000);
  EXPECT(s.tick(8000) == End::MAX_DURATION);
}

void test_session_result_cancel_and_wrap() {
  PushToTalkSession s;
  s.start(0xFFFFFF00u);  // timer wraps during the session
  EXPECT(s.tick(0x00000100u) == End::NONE);  // 512 ms elapsed
  EXPECT(s.tick(0x00000D00u) == End::NONE);  // 3584 ms elapsed
  s.finish_with_result();
  EXPECT(s.last_end_reason() == End::RESULT && !s.listening());
  s.cancel();  // no-op when idle
  EXPECT(s.last_end_reason() == End::RESULT);
  EXPECT(s.start(10));
  s.cancel();
  EXPECT(s.last_end_reason() == End::CANCELLED);
}

void test_filter_accepts_in_context() {
  VoiceIntentFilter f;
  EXPECT(f.evaluate(Intent::RED, 90, kColorPop, 1000) == Verdict::ACCEPT);
  EXPECT(f.evaluate(Intent::BACK, 90, kColorPop, 1100) == Verdict::ACCEPT);
}

void test_filter_rejections() {
  VoiceIntentFilter f;
  EXPECT(f.evaluate(Intent::UNKNOWN, 99, kColorPop, 0) == Verdict::UNKNOWN_INTENT);
  EXPECT(f.evaluate(Intent::RED, 69, kColorPop, 0) == Verdict::LOW_CONFIDENCE);
  EXPECT(f.evaluate(Intent::RED, 70, kColorPop, 0) == Verdict::ACCEPT);
  EXPECT(f.evaluate(Intent::GO, 99, kColorPop, 5000) == Verdict::OUT_OF_CONTEXT);
  EXPECT(f.evaluate(Intent::RED, 99, 0, 9000) == Verdict::OUT_OF_CONTEXT);  // asleep
}

void test_filter_dedupe() {
  VoiceIntentFilter f;
  EXPECT(f.evaluate(Intent::RED, 90, kColorPop, 1000) == Verdict::ACCEPT);
  EXPECT(f.evaluate(Intent::RED, 90, kColorPop, 1749) == Verdict::DUPLICATE);
  EXPECT(f.evaluate(Intent::BLUE, 90, kColorPop, 1749) == Verdict::ACCEPT);
  EXPECT(f.evaluate(Intent::BLUE, 90, kColorPop, 2499) == Verdict::ACCEPT);
  // Rejected results never refresh the dedupe window.
  EXPECT(f.evaluate(Intent::GREEN, 10, kColorPop, 2600) == Verdict::LOW_CONFIDENCE);
  EXPECT(f.evaluate(Intent::GREEN, 90, kColorPop, 2700) == Verdict::ACCEPT);
}

void test_filter_playback_cooldown() {
  VoiceIntentFilter f;
  f.set_playback_active(true);
  EXPECT(f.evaluate(Intent::RED, 99, kColorPop, 100) == Verdict::PLAYBACK_COOLDOWN);
  f.on_playback_finished(1000);
  EXPECT(f.evaluate(Intent::RED, 99, kColorPop, 2499) == Verdict::PLAYBACK_COOLDOWN);
  EXPECT(f.evaluate(Intent::RED, 99, kColorPop, 2500) == Verdict::ACCEPT);
}

}  // namespace

int main() {
  test_session_silence_timeout();
  test_session_max_duration();
  test_session_result_cancel_and_wrap();
  test_filter_accepts_in_context();
  test_filter_rejections();
  test_filter_dedupe();
  test_filter_playback_cooldown();
  return little_arcade::testing::finish("voice policy tests");
}
