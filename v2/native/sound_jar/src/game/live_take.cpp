#include "game/live_take.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>

namespace sound_jar {
namespace {
constexpr double kLoud = 512.0 / 32768.0, kActive = 0.002;  // reference LOUD / ACTIVE
constexpr float kDcPole = 0.995f;                           // one-pole DC blocker (~13 Hz at 16 kHz)
// "Sounding" is relative to the room: kFloorRatio x the quietest 20 ms frame of this recording (including the
// discarded lead-in), never below the reference ACTIVE and never above LOUD, so speech always counts. Device
// evidence (INMP441 at x16): floor -55 dBFS, silent-room frames up to -45 dBFS, speech peaks -18..-20 dBFS; 3x
// (+9.5 dB) still counted room noise, 10x (+20 dB) does not.
constexpr double kFloorRatio = 10.0;
double active_threshold(double floor) { return std::clamp(floor * kFloorRatio, kActive, kLoud); }
}  // namespace

LiveTake::LiveTake(int16_t* buffer, ClockUs clock) : buf_(buffer), clock_(clock) {
  std::memset(buf_, 0, sizeof(int16_t) * kMaxSamples);
}

// ---------------- UI thread ----------------
void LiveTake::push(Op op) {
  const uint32_t head = head_.load(std::memory_order_relaxed);
  // The game sends at most a few requests per recording; a full ring would mean the audio thread is not
  // running, in which case nothing is being captured. Wipes still clear the UI-side state.
  if (head - tail_.load(std::memory_order_acquire) >= kQueue) return;
  ring_[head % kQueue] = op;
  head_.store(head + 1, std::memory_order_release);
}

void LiveTake::reset_meters() {
  len_.store(0, std::memory_order_relaxed);
  voiced_samples_.store(0, std::memory_order_relaxed);
  last_active_.store(-1, std::memory_order_relaxed);
  level_.store(0, std::memory_order_relaxed);
}

void LiveTake::clear_ui() {
  std::fill(std::begin(lev_), std::end(lev_), 0.0);
  std::fill(std::begin(play_lev_), std::end(play_lev_), 0.0);
  data_.dur = 0;
  data_.frames = 0;
  data_.pcm = nullptr;
  data_.samples = 0;
}

void LiveTake::arm() {
  ready_.store(false, std::memory_order_release);
  clear_ui();
  reset_meters();  // the audio thread is idle here; it resets them again when it applies the request
  mute_.store(true, std::memory_order_release);
  arm_at_us_.store(clock_ ? static_cast<uint32_t>(clock_()) : 0, std::memory_order_relaxed);
  push(Op::Arm);
}

void LiveTake::stop() { push(Op::Stop); }

void LiveTake::wipe() {
  ready_.store(false, std::memory_order_release);
  clear_ui();
  push(Op::Wipe);
}

double LiveTake::voiced() const {
  return voiced_samples_.load(std::memory_order_relaxed) / static_cast<double>(kTakeSampleRate);
}

double LiveTake::elapsed() const { return len_.load(std::memory_order_relaxed) / static_cast<double>(kTakeSampleRate); }

bool LiveTake::should_stop() const {
  const int len = len_.load(std::memory_order_relaxed), last = last_active_.load(std::memory_order_relaxed);
  if (len >= kMaxSamples) return true;
  return last >= 0 && (len - last) >= kTakeSampleRate / 2;
}

bool LiveTake::finalize() {
  if (state() != State::Captured) return false;
  const int n = len_.load(std::memory_order_acquire);
  const int frames = (n + kFrameSamples - 1) / kFrameSamples;
  // Raw 20 ms RMS levels (into play_lev_ as scratch) and the first/last sounding frame.
  int first = -1, last = -1;
  const double thr = active_threshold(floor_.load(std::memory_order_relaxed));
  for (int f = 0; f < frames; ++f) {
    const int a = f * kFrameSamples, b = std::min(n, a + kFrameSamples);
    double s = 0;
    for (int i = a; i < b; ++i) s += static_cast<double>(buf_[i]) * buf_[i];
    play_lev_[f] = std::sqrt(s / (b - a)) / 32768.0;
    if (play_lev_[f] >= thr) { if (first < 0) first = f; last = f; }
  }
  int peak = 0;
  const int f0 = std::max(0, first - 1), start = f0 * kFrameSamples;
  const int end = first < 0 ? 0 : std::min(n, (last + 3) * kFrameSamples), m = end - start;  // 40 ms after: word tails
  for (int i = start; i < end; ++i) peak = std::max(peak, std::abs(static_cast<int>(buf_[i])));
  if (first < 0 || m <= 0 || peak == 0) {
    zero_buffer();
    clear_ui();
    state_.store(State::Idle, std::memory_order_release);
    return false;
  }
  for (int f = 0; f * kFrameSamples < m; ++f) lev_[f] = play_lev_[f0 + f];
  const double gain = std::min(kMaxGain, 0.5 * 32768.0 / peak);
  for (int i = 0; i < m; ++i) {  // in place: reads buf_[start + i] before anything at or after it is written
    const double fade = std::min({1.0, i / static_cast<double>(kFadeSamples), (m - 1 - i) / static_cast<double>(kFadeSamples)});
    const double v = std::round(buf_[start + i] * gain * fade);
    buf_[i] = static_cast<int16_t>(std::clamp(v, -32768.0, 32767.0));
  }
  std::memset(buf_ + m, 0, sizeof(int16_t) * (kMaxSamples - m));  // nothing of the raw input outlives the clip
  const int mf = (m + kFrameSamples - 1) / kFrameSamples;
  for (int f = 0; f < kMaxFrames; ++f) {
    if (f >= mf) { lev_[f] = play_lev_[f] = 0; continue; }
    const int a = f * kFrameSamples, b = std::min(m, a + kFrameSamples);
    double s = 0;
    for (int i = a; i < b; ++i) s += static_cast<double>(buf_[i]) * buf_[i];
    play_lev_[f] = std::sqrt(s / (b - a)) / 32768.0;
  }
  data_.dur = m / static_cast<double>(kTakeSampleRate);
  data_.frames = mf;
  data_.pcm = buf_;
  data_.samples = m;
  samples_.store(m, std::memory_order_relaxed);
  gen_.fetch_add(1, std::memory_order_acq_rel);
  state_.store(State::Ready, std::memory_order_release);
  ready_.store(true, std::memory_order_release);
  return true;
}

// ---------------- audio thread ----------------
void LiveTake::zero_buffer() {
  // volatile stores so the privacy wipe cannot be elided
  volatile int16_t* p = buf_;
  for (int i = 0; i < kMaxSamples; ++i) p[i] = 0;
}

void LiveTake::capture(const int16_t* in, std::size_t n) {
  uint32_t tail = tail_.load(std::memory_order_relaxed);
  const uint32_t head = head_.load(std::memory_order_acquire);
  while (tail != head) {
    const Op op = ring_[tail % kQueue];
    tail_.store(++tail, std::memory_order_release);
    switch (op) {
      case Op::Arm:
        gen_.fetch_add(1, std::memory_order_acq_rel);
        ready_.store(false, std::memory_order_release);
        zero_buffer();
        reset_meters();
        peak_.store(0, std::memory_order_relaxed);
        samples_.store(0, std::memory_order_relaxed);
        frame_sumsq_ = 0;
        frame_n_ = 0;
        frame_disc_ = false;
        skip_ = kDiscardSamples;
        floor_.store(1.f, std::memory_order_relaxed);
        first_block_ = true;
        mute_.store(true, std::memory_order_release);  // a wipe queued just before (new recording) unmuted
        state_.store(State::Recording, std::memory_order_release);
        break;
      case Op::Stop:
        if (state() == State::Recording) state_.store(State::Captured, std::memory_order_release);
        mute_.store(false, std::memory_order_release);
        break;
      case Op::Wipe:
        gen_.fetch_add(1, std::memory_order_acq_rel);
        ready_.store(false, std::memory_order_release);
        zero_buffer();
        reset_meters();
        samples_.store(0, std::memory_order_relaxed);
        frame_sumsq_ = 0;
        frame_n_ = 0;
        state_.store(State::Idle, std::memory_order_release);
        mute_.store(false, std::memory_order_release);
        break;
    }
  }
  if (!in || n == 0) return;
  const bool rec = state() == State::Recording;
  if (rec && first_block_) {
    first_block_ = false;
    takes_.fetch_add(1, std::memory_order_relaxed);
    if (clock_)
      latency_us_.store(static_cast<uint32_t>(clock_()) - arm_at_us_.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
  }
  int len = len_.load(std::memory_order_relaxed);
  // The DC blocker runs on every block, recording or not, so a recording never starts with its settling transient.
  for (std::size_t i = 0; i < n; ++i) {
    const float x = in[i];
    const float y = x - dc_x1_ + kDcPole * dc_y1_;
    dc_x1_ = x;
    dc_y1_ = y;
    if (!rec || len >= kMaxSamples) continue;
    const int16_t s = static_cast<int16_t>(std::clamp(std::lround(y * gain_), -32768L, 32767L));
    // The lead-in (speaker tail, the tap itself) only measures the room's floor; it is never kept.
    if (skip_ > 0) { --skip_; frame_disc_ = true; } else { buf_[len++] = s; }
    frame_sumsq_ += static_cast<double>(s) * s;
    if (++frame_n_ == kFrameSamples) {
      const double rms = std::sqrt(frame_sumsq_ / kFrameSamples) / 32768.0;
      float floor = floor_.load(std::memory_order_relaxed);
      if (rms < floor) floor_.store(floor = static_cast<float>(rms), std::memory_order_relaxed);
      if (!frame_disc_) {
        level_.store(static_cast<float>(rms), std::memory_order_relaxed);
        if (rms > peak_.load(std::memory_order_relaxed)) peak_.store(static_cast<float>(rms), std::memory_order_relaxed);
        if (rms >= active_threshold(floor)) last_active_.store(len, std::memory_order_relaxed);
        if (rms >= kLoud) voiced_samples_.fetch_add(kFrameSamples, std::memory_order_relaxed);
      }
      frame_sumsq_ = 0;
      frame_n_ = 0;
      frame_disc_ = false;
    }
  }
  if (!rec) return;
  len_.store(len, std::memory_order_release);
  if (len >= kMaxSamples) state_.store(State::Captured, std::memory_order_release);  // 3 s cap
}

}  // namespace sound_jar
