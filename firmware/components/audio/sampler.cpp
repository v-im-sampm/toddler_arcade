#include "little_arcade/audio/sampler.hpp"

#include <cmath>
#include <cstring>

namespace little_arcade::audio {
namespace {
constexpr int32_t kEnvFull = 32768;
constexpr int32_t kAttackStep = kEnvFull / static_cast<int32_t>(Sampler::kAttackSamples);
constexpr int32_t kReleaseStep = kEnvFull / static_cast<int32_t>(Sampler::kReleaseSamples);
constexpr uint32_t kRobotInc = static_cast<uint32_t>((static_cast<uint64_t>(Sampler::kRobotHz) << 32) / kSampleRateHz);
constexpr uint32_t kWobbleInc = static_cast<uint32_t>((static_cast<uint64_t>(Sampler::kWobbleHz) << 32) / kSampleRateHz);

int16_t clamp16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return static_cast<int16_t>(v);
}

int32_t abs32(int32_t v) { return v < 0 ? -v : v; }

int32_t soft_limit(int32_t x) {
  constexpr int32_t k = Sampler::kSoftKnee;
  if (x > k) x = k + (x - k) / 4;
  if (x < -k) x = -k + (x + k) / 4;
  return x;
}

Tone tone(Wave w, uint16_t f0, uint16_t f1, uint16_t ms, uint8_t level, uint8_t attack, uint8_t release) {
  Tone t;
  t.wave = w;
  t.f0_hz = f0;
  t.f1_hz = f1;
  t.ms = ms;
  t.level = level;
  t.attack_ms = attack;
  t.release_ms = release;
  return t;
}
}  // namespace

Sampler::Sampler(int16_t* clip, int16_t* record, int16_t* echo) : clip_(clip), record_(record), echo_(echo) {
  std::memset(clip_, 0, kMaxSamples * sizeof(int16_t));
  std::memset(record_, 0, kMaxSamples * sizeof(int16_t));
  std::memset(echo_, 0, kEchoSamples * sizeof(int16_t));
  for (auto& t : thumb_) t.store(0, std::memory_order_relaxed);
  for (std::size_t i = 0; i < 256; ++i) {
    sine_[i] = static_cast<int16_t>(std::lround(32767.0 * std::sin(6.283185307179586 * static_cast<double>(i) / 256.0)));
  }
}

Sound Sampler::builtin_sound(uint8_t index) {
  switch (index % kBuiltinCount) {
    case 0:  // boing: spring up, then a long wobbly fall
      return Sound{}
          .add(tone(Wave::TRIANGLE, 220, 660, 90, 220, 3, 10))
          .add(tone(Wave::SINE, 660, 160, 420, 230, 4, 140));
    case 1:  // quack-like pulse glides
      return Sound{}
          .add(tone(Wave::PULSE, 600, 420, 140, 190, 4, 30))
          .add(tone(Wave::PULSE, 0, 0, 40, 0, 0, 0))
          .add(tone(Wave::PULSE, 560, 380, 170, 190, 4, 60));
    default:  // drum thump: a tick of noise into a falling sine
      return Sound{}
          .add(tone(Wave::NOISE, 3000, 3000, 20, 150, 1, 8))
          .add(tone(Wave::SINE, 150, 45, 300, 255, 2, 160));
  }
}

// ---- UI task ------------------------------------------------------------

void Sampler::push(Op op, uint8_t a, uint32_t v) {
  const uint32_t head = head_.load(std::memory_order_relaxed);
  if (head - tail_.load(std::memory_order_acquire) >= kQueueSize) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  ring_[head % kQueueSize] = Command{op, epoch_.load(std::memory_order_relaxed), a, v};
  head_.store(head + 1, std::memory_order_release);
}

void Sampler::start_recording() {
  if (!mic_available()) return;
  result_.store(static_cast<uint8_t>(RecordResult::NONE), std::memory_order_relaxed);
  push(Op::START_REC);
}
void Sampler::stop_recording() { push(Op::STOP_REC); }
void Sampler::cancel_recording() { push(Op::CANCEL_REC); }

ISampler::RecordResult Sampler::take_record_result() {
  return static_cast<RecordResult>(
      result_.exchange(static_cast<uint8_t>(RecordResult::NONE), std::memory_order_acq_rel));
}

void Sampler::play(uint32_t rate_q16, bool loop) { push(Op::PLAY, loop ? 1 : 0, rate_q16); }
void Sampler::bend(uint32_t rate_q16) { push(Op::BEND, 0, rate_q16); }
void Sampler::end_loop() { push(Op::END_LOOP); }
void Sampler::release() { push(Op::RELEASE); }
void Sampler::stop_all() { push(Op::STOP_ALL); }
void Sampler::set_effects(uint8_t mask) { push(Op::EFFECTS, static_cast<uint8_t>(mask & kFxAll)); }
void Sampler::load_builtin(uint8_t index) { push(Op::BUILTIN, index); }

void Sampler::wipe() {
  // Bumping the epoch makes the audio task wipe before anything newer and
  // discard anything older, even if the ring is full.
  epoch_.fetch_add(1, std::memory_order_release);
  result_.store(static_cast<uint8_t>(RecordResult::NONE), std::memory_order_relaxed);
}

// ---- audio task ---------------------------------------------------------

void Sampler::do_wipe() {
  rec_active_ = false;
  rec_count_ = 0;
  std::memset(clip_, 0, kMaxSamples * sizeof(int16_t));
  std::memset(record_, 0, kMaxSamples * sizeof(int16_t));
  std::memset(echo_, 0, kEchoSamples * sizeof(int16_t));
  for (Voice& v : voices_) v = Voice{};
  synth_.stop_all();
  clip_len_ = 0;
  effects_ = 0;
  progress_.store(0, std::memory_order_relaxed);
  level_.store(0, std::memory_order_relaxed);
  playhead_.store(-1, std::memory_order_relaxed);
  publish_clip();
}

void Sampler::silence_voices() {
  for (Voice& v : voices_) v = Voice{};
  std::memset(echo_, 0, kEchoSamples * sizeof(int16_t));
  playhead_.store(-1, std::memory_order_relaxed);
}

void Sampler::begin_recording() {
  silence_voices();
  std::memset(record_, 0, kMaxSamples * sizeof(int16_t));
  rec_count_ = 0;
  rec_active_ = true;
  progress_.store(0, std::memory_order_relaxed);
}

void Sampler::abort_recording() {
  if (!rec_active_) return;
  rec_active_ = false;
  std::memset(record_, 0, rec_count_ * sizeof(int16_t));
  rec_count_ = 0;
  progress_.store(0, std::memory_order_relaxed);
  level_.store(0, std::memory_order_relaxed);
}

void Sampler::finish_recording() {
  if (!rec_active_) return;
  rec_active_ = false;
  const uint32_t n = rec_count_;
  int32_t peak = 0;
  for (uint32_t i = 0; i < n; ++i) {
    const int32_t a = abs32(record_[i]);
    if (a > peak) peak = a;
  }
  RecordResult result = RecordResult::TOO_QUIET;
  if (n >= kMinRecordSamples && peak >= kQuietPeak) {
    const int32_t threshold = peak / 10;
    uint32_t first = 0;
    while (first < n && abs32(record_[first]) <= threshold) ++first;
    uint32_t last = n - 1;
    while (last > first && abs32(record_[last]) <= threshold) --last;
    const uint32_t start = first > kPreRollSamples ? first - kPreRollSamples : 0;
    const uint32_t end = last + 1 + kTailSamples < n ? last + 1 + kTailSamples : n;
    install_clip(record_, start, end - start, peak);
    result = RecordResult::ACCEPTED;
  }
  // The raw take is gone either way; only the trimmed clip remains.
  std::memset(record_, 0, n * sizeof(int16_t));
  rec_count_ = 0;
  progress_.store(0, std::memory_order_relaxed);
  level_.store(0, std::memory_order_relaxed);
  result_.store(static_cast<uint8_t>(result), std::memory_order_release);
}

void Sampler::install_clip(const int16_t* src, uint32_t start, uint32_t len, int32_t peak) {
  silence_voices();
  if (len > kMaxSamples) len = kMaxSamples;
  const int64_t gain_q16 = peak > 0 ? (static_cast<int64_t>(kTargetPeak) << 16) / peak : 0;
  for (uint32_t i = 0; i < len; ++i) {
    int64_t s = src[start + i] * gain_q16 >> 16;
    if (i < kFadeSamples) s = s * i / kFadeSamples;
    if (len - 1 - i < kFadeSamples) s = s * (len - 1 - i) / kFadeSamples;
    clip_[i] = clamp16(static_cast<int32_t>(s));
  }
  if (clip_len_ > len) std::memset(clip_ + len, 0, (clip_len_ - len) * sizeof(int16_t));
  clip_len_ = len;
  publish_clip();
}

void Sampler::publish_clip() {
  for (std::size_t c = 0; c < kThumbColumns; ++c) {
    const uint32_t a = static_cast<uint32_t>(c * clip_len_ / kThumbColumns);
    const uint32_t b = static_cast<uint32_t>((c + 1) * clip_len_ / kThumbColumns);
    int32_t peak = 0;
    for (uint32_t i = a; i < b; ++i) {
      const int32_t v = abs32(clip_[i]);
      if (v > peak) peak = v;
    }
    const int32_t t = peak * 255 / kTargetPeak;
    thumb_[c].store(static_cast<uint8_t>(t > 255 ? 255 : t), std::memory_order_relaxed);
  }
  clip_len_pub_.store(clip_len_, std::memory_order_relaxed);
  generation_.fetch_add(1, std::memory_order_release);
}

void Sampler::render_builtin(uint8_t index) {
  if (rec_active_) return;
  synth_.stop_all();
  synth_.trigger(builtin_sound(index));
  uint32_t len = 0;
  constexpr uint32_t kChunk = 256;
  while (len + kChunk <= kMaxSamples && synth_.active_voices() > 0) {
    synth_.render(clip_ + len, kChunk);
    len += kChunk;
  }
  synth_.stop_all();
  int32_t peak = 0;
  for (uint32_t i = 0; i < len; ++i) {
    const int32_t a = abs32(clip_[i]);
    if (a > peak) peak = a;
  }
  // In place: install_clip reads index i before writing it.
  const uint32_t old_len = clip_len_;
  clip_len_ = len > old_len ? len : old_len;  // so the tail beyond `len` is cleared
  install_clip(clip_, 0, len, peak);
}

void Sampler::start_voice(uint32_t rate, bool loop) {
  if (clip_len_ < 2) return;
  if (rate < kRateMin) rate = kRateMin;
  if (rate > kRateMax) rate = kRateMax;
  // Keep at most two sounding voices: fade the oldest, and any previous loop
  // when a new loop starts (one finger holds one pad).
  Voice* newest = nullptr;
  std::size_t sounding = 0;
  for (Voice& v : voices_) {
    if (!v.active || v.releasing) continue;
    if (loop && v.loop) {
      v.releasing = true;
      continue;
    }
    ++sounding;
    if (!newest || v.age > newest->age) newest = &v;
  }
  if (sounding >= 2) {
    for (Voice& v : voices_) {
      if (v.active && !v.releasing && &v != newest) v.releasing = true;
    }
  }
  Voice* slot = nullptr;
  for (Voice& v : voices_) {
    if (!v.active) {
      slot = &v;
      break;
    }
  }
  if (!slot) {  // steal the oldest
    slot = &voices_[0];
    for (Voice& v : voices_) {
      if (v.age < slot->age) slot = &v;
    }
  }
  *slot = Voice{};
  slot->active = true;
  slot->loop = loop;
  slot->rate = slot->target = rate;
  slot->age = ++voice_age_;
}

void Sampler::apply(const Command& c) {
  switch (c.op) {
    case Op::START_REC: begin_recording(); break;
    case Op::STOP_REC: finish_recording(); break;
    case Op::CANCEL_REC: abort_recording(); break;
    case Op::PLAY:
      if (!rec_active_) start_voice(c.v, c.a != 0);
      break;
    case Op::BEND: {
      Voice* newest = nullptr;
      for (Voice& v : voices_) {
        if (v.active && v.loop && !v.releasing && (!newest || v.age > newest->age)) newest = &v;
      }
      if (!newest) break;
      uint32_t rate = c.v < kRateMin ? kRateMin : (c.v > kRateMax ? kRateMax : c.v);
      newest->target = rate;
      const uint32_t diff = rate > newest->rate ? rate - newest->rate : newest->rate - rate;
      newest->slew = diff / kSlewSamples + 1;
      break;
    }
    case Op::END_LOOP:
      for (Voice& v : voices_) v.loop = false;
      break;
    case Op::RELEASE:
      for (Voice& v : voices_) {
        if (v.active && v.loop) v.releasing = true;
      }
      break;
    case Op::STOP_ALL:
      for (Voice& v : voices_) {
        if (v.active) v.releasing = true;
      }
      break;
    case Op::EFFECTS:
      if ((c.a & kFxEcho) && !(effects_ & kFxEcho)) std::memset(echo_, 0, kEchoSamples * sizeof(int16_t));
      effects_ = c.a;
      break;
    case Op::BUILTIN: render_builtin(c.a); break;
  }
}

void Sampler::capture(const int16_t* in, std::size_t n) {
  int32_t peak = 0;
  for (std::size_t i = 0; i < n; ++i) {
    // DC blocker (pole 0.995) runs continuously so it has settled before a take.
    const int32_t x = in[i];
    const int32_t y = x - dc_x1_ + static_cast<int32_t>((static_cast<int64_t>(dc_y1_) * 32604) >> 15);
    dc_x1_ = x;
    dc_y1_ = y;
    if (!rec_active_) continue;
    const int16_t s = clamp16(y);
    const int32_t a = abs32(s);
    if (a > peak) peak = a;
    record_[rec_count_++] = s;
    if (rec_count_ >= kMaxSamples) {
      finish_recording();
      return;
    }
  }
  if (!rec_active_) return;
  progress_.store(static_cast<uint16_t>(rec_count_ * 1000u / kMaxSamples), std::memory_order_relaxed);
  const int32_t lvl = peak >> 5;  // speech near the mic is quiet: show it generously
  level_.store(static_cast<uint8_t>(lvl > 255 ? 255 : lvl), std::memory_order_relaxed);
}

int32_t Sampler::voice_sample(Voice& v, uint32_t wobble_q16) {
  if (v.slew) {
    if (v.rate < v.target) {
      v.rate = v.target - v.rate > v.slew ? v.rate + v.slew : v.target;
    } else {
      v.rate = v.rate - v.target > v.slew ? v.rate - v.slew : v.target;
    }
    if (v.rate == v.target) v.slew = 0;
  }
  uint32_t idx = v.pos_q16 >> 16;
  if (idx >= clip_len_ - 1) {
    if (!v.loop) {
      v.active = false;
      return 0;
    }
    v.pos_q16 -= (clip_len_ - 1) << 16;
    idx = v.pos_q16 >> 16;
    if (idx >= clip_len_ - 1) v.pos_q16 = idx = 0;
  }
  const int32_t frac = static_cast<int32_t>(v.pos_q16 & 0xFFFFu);
  int32_t a;
  int32_t b;
  if (effects_ & kFxReverse) {
    a = clip_[clip_len_ - 1 - idx];
    b = clip_[clip_len_ - 2 - idx];
  } else {
    a = clip_[idx];
    b = clip_[idx + 1];
  }
  const int32_t s = a + static_cast<int32_t>((static_cast<int64_t>(b - a) * frac) >> 16);

  if (v.releasing) {
    v.env -= kReleaseStep;
    if (v.env <= 0) {
      v.active = false;
      return 0;
    }
  } else if (v.env < kEnvFull) {
    v.env = v.env + kAttackStep > kEnvFull ? kEnvFull : v.env + kAttackStep;
  }
  const uint32_t step = static_cast<uint32_t>((static_cast<uint64_t>(v.rate) * wobble_q16) >> 16);
  v.pos_q16 += step;
  return (s * v.env) >> 15;
}

void Sampler::mix(int16_t* out, std::size_t n) {
  // Wipe before, and drop commands from before, any newer epoch.
  const uint8_t epoch = epoch_.load(std::memory_order_acquire);
  if (epoch != done_epoch_) {
    do_wipe();
    done_epoch_ = epoch;
  }
  uint32_t tail = tail_.load(std::memory_order_relaxed);
  const uint32_t head = head_.load(std::memory_order_acquire);
  while (tail != head) {
    const Command c = ring_[tail % kQueueSize];
    tail_.store(++tail, std::memory_order_release);
    if (c.epoch != done_epoch_) {
      if (static_cast<uint8_t>(c.epoch - done_epoch_) >= 128) continue;  // older than the last wipe
      do_wipe();
      done_epoch_ = c.epoch;
    }
    apply(c);
  }

  if (rec_active_) {
    std::memset(out, 0, n * sizeof(int16_t));
    return;
  }

  bool any = false;
  for (const Voice& v : voices_) any = any || v.active;
  const bool echo = (effects_ & kFxEcho) != 0;
  if (!any && !echo) return;

  const bool robot = (effects_ & kFxRobot) != 0;
  const bool wobble = (effects_ & kFxWobble) != 0;
  for (std::size_t i = 0; i < n; ++i) {
    uint32_t wobble_q16 = kRateUnity;
    if (wobble) {
      const int32_t p = static_cast<int32_t>(wobble_phase_ >> 16);  // 0..65535
      const int32_t tri = p < 32768 ? p * 2 - 32768 : (65535 - p) * 2 - 32768;
      wobble_q16 = static_cast<uint32_t>(65536 + ((kWobbleDepthQ16 * tri) >> 15));
      wobble_phase_ += kWobbleInc;
    }
    int32_t x = 0;
    if (any) {
      for (Voice& v : voices_) {
        if (v.active) x += voice_sample(v, wobble_q16);
      }
    }
    if (robot) {
      x = static_cast<int32_t>((static_cast<int64_t>(x) * sine_[robot_phase_ >> 24]) >> 15);
      robot_phase_ += kRobotInc;
    }
    if (echo) {
      const int32_t y = x + ((echo_[echo_pos_] * kEchoFeedbackQ15) >> 15);
      echo_[echo_pos_] = clamp16(y);
      echo_pos_ = echo_pos_ + 1 >= kEchoSamples ? 0 : echo_pos_ + 1;
      x = y;
    }
    out[i] = clamp16(out[i] + soft_limit(x));
  }

  const Voice* newest = nullptr;
  for (const Voice& v : voices_) {
    if (v.active && (!newest || v.age > newest->age)) newest = &v;
  }
  int16_t head_pos = -1;
  if (newest && clip_len_ > 1) {
    const uint32_t p = static_cast<uint32_t>((static_cast<uint64_t>(newest->pos_q16 >> 16) * 1000u) / (clip_len_ - 1));
    head_pos = static_cast<int16_t>(p > 1000 ? 1000 : p);
    if (effects_ & kFxReverse) head_pos = static_cast<int16_t>(1000 - head_pos);
  }
  playhead_.store(head_pos, std::memory_order_relaxed);
}

}  // namespace little_arcade::audio
