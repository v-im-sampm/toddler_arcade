#include "audio/color_pop_score.hpp"

#include <cmath>

#include "little_arcade/audio/score_dsp.hpp"

namespace color_pop {
namespace {
using little_arcade::audio::score_dsp::curve_db;
using little_arcade::audio::score_dsp::kPi;
using little_arcade::audio::score_dsp::makeup_db;
using little_arcade::audio::score_dsp::sine_at;

constexpr float kSr = static_cast<float>(ColorPopScore::kSampleRate);
constexpr float kFloor = 0.0001f;  // env() start/end value

inline float incr_f(float hz) { return hz / kSr * 4294967296.0f; }
inline uint32_t incr_for(float hz) { return static_cast<uint32_t>(incr_f(hz)); }
inline uint32_t samples(float seconds) { return static_cast<uint32_t>(seconds * kSr + 0.5f); }

// Reference MOTIFS: [degree, octave, time s, velocity].
struct MotifNote { int8_t degree, octave; float time, vel; };
constexpr MotifNote kRed[] = {{0, -1, 0.f, 0.7f}, {0, -1, 0.2f, 1.f}};
constexpr MotifNote kBlue[] = {{4, 0, 0.f, 0.8f}, {3, 0, 0.12f, 0.8f}, {2, 0, 0.24f, 0.9f}};
constexpr MotifNote kGreen[] = {{2, 0, 0.f, 0.8f}, {3, 0, 0.1f, 0.8f}, {4, 0, 0.2f, 0.9f}};
constexpr MotifNote kYellow[] = {{5, 0, 0.f, 0.7f}, {7, 0, 0.09f, 0.7f}, {5, 0, 0.18f, 0.7f}, {7, 0, 0.27f, 0.8f}};

// Web Audio BiquadFilter low-pass: Q is in dB for this type (default 1 dB).
struct Lowpass {
  float b0, b1, b2, a1, a2;
  Lowpass() {
    const float w0 = 2.f * kPi * 420.f / kSr;
    const float q = std::pow(10.f, 1.f / 20.f);
    const float alpha = std::sin(w0) / (2.f * q), cw = std::cos(w0), a0 = 1.f + alpha;
    b0 = (1.f - cw) / 2.f / a0; b1 = (1.f - cw) / a0; b2 = b0;
    a1 = -2.f * cw / a0; a2 = (1.f - alpha) / a0;
  }
};
}  // namespace

std::size_t ColorPopScore::notes() const {
  std::size_t n = 0;
  for (const Note& note : notes_) n += note.active ? 1 : 0;
  return n;
}

// ---------------- UI thread ----------------
void ColorPopScore::push(const Command& c) {
  const uint32_t head = head_.load(std::memory_order_relaxed);
  if (head - tail_.load(std::memory_order_acquire) >= kQueue) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  ring_[head % kQueue] = c;
  head_.store(head + 1, std::memory_order_release);
}
void ColorPopScore::start() { push({Op::Start, 0, 0, 0, 0, 0}); }
void ColorPopScore::marimba(double freq, double when, double vel, double dur) {
  push({Op::Marimba, 0, static_cast<float>(freq), static_cast<float>(when), static_cast<float>(vel), static_cast<float>(dur)});
}
void ColorPopScore::plop(double when, double vel, double hi) {
  push({Op::Plop, 0, static_cast<float>(when), static_cast<float>(vel), static_cast<float>(hi), 0});
}
void ColorPopScore::motif(Color color, double delay) {
  push({Op::Motif, static_cast<uint8_t>(color), static_cast<float>(delay), 0, 0, 0});
}
void ColorPopScore::celebrate(bool big, double delay) {
  push({Op::Celebrate, static_cast<uint8_t>(big ? 1 : 0), static_cast<float>(delay), 0, 0, 0});
}

// ---------------- audio thread ----------------
bool ColorPopScore::voice_ok(float seconds) {
  std::size_t k = 0;
  for (std::size_t i = 0; i < voice_n_; ++i) {
    if (static_cast<int32_t>(voice_end_[i] - now_) > 0) voice_end_[k++] = voice_end_[i];
  }
  voice_n_ = k;
  if (voice_n_ >= kMaxVoices) { ++refused_; return false; }
  voice_end_[voice_n_++] = now_ + samples(seconds);
  return true;
}

ColorPopScore::Note* ColorPopScore::slot() {
  for (Note& n : notes_) if (!n.active) return &n;
  return nullptr;
}

void ColorPopScore::play_marimba(float freq, float when, float vel, float dur) {
  if (!running_ || !voice_ok(dur)) return;
  Note* n = slot();
  if (!n) return;
  *n = Note{};
  n->active = true;
  n->start = now_ + samples(when);
  n->len = samples(dur + 0.05f);
  n->attack = samples(0.003f);
  n->peak = 0.45f * vel;
  n->decay = samples(dur) - n->attack;
  n->mul = std::pow(kFloor / n->peak, 1.f / static_cast<float>(n->decay));
  n->g2 = 0.18f * vel;
  n->decay2 = samples(0.09f) - n->attack;
  n->mul2 = std::pow(kFloor / n->g2, 1.f / static_cast<float>(n->decay2));
  n->inc = incr_for(freq);
  n->inc2 = incr_for(freq * 3.99f);
  n->overtone = freq * 3.99f < 7800.f;
}

void ColorPopScore::play_plop(float when, float vel, float hi) {
  if (!running_ || !voice_ok(0.15f)) return;
  Note* n = slot();
  if (!n) return;
  *n = Note{};
  n->active = true;
  n->glide = true;
  n->start = now_ + samples(when);
  n->len = samples(0.18f);
  n->attack = samples(0.003f);
  n->peak = 0.35f * vel;
  n->decay = samples(0.14f) - n->attack;
  n->mul = std::pow(kFloor / n->peak, 1.f / static_cast<float>(n->decay));
  n->glide_len = samples(0.07f);
  n->incf = incr_f(760.f * hi);
  n->fmul = std::pow(230.f / 760.f, 1.f / static_cast<float>(n->glide_len));
}

void ColorPopScore::play_motif(Color color) {
  const MotifNote* m = kRed;
  std::size_t count = 2;
  switch (color) {
    case Color::Red: m = kRed; count = 2; break;
    case Color::Blue: m = kBlue; count = 3; break;
    case Color::Green: m = kGreen; count = 3; break;
    case Color::Yellow: m = kYellow; count = 4; break;
  }
  for (std::size_t i = 0; i < count; ++i)
    play_marimba(static_cast<float>(degree_hz(m[i].degree, m[i].octave)), m[i].time, m[i].vel, 0.55f);
}

void ColorPopScore::play_celebrate(bool big) {
  static constexpr int kSmall[] = {0, 2, 4, 5};
  static constexpr int kBig[] = {0, 1, 2, 3, 4, 5, 7, 10};
  const int* d = big ? kBig : kSmall;
  const int n = big ? 8 : 4;
  for (int i = 0; i < n; ++i)
    play_marimba(static_cast<float>(degree_hz(d[i])), static_cast<float>(i) * (big ? 0.11f : 0.09f), 0.8f, 0.55f);
}

void ColorPopScore::apply(const Command& c) {
  switch (c.op) {
    case Op::Start:
      if (!running_) {
        running_ = true;
        master_ = 0;
      }
      return;
    case Op::Marimba: play_marimba(c.a, c.b, c.c, c.d); return;
    case Op::Plop: play_plop(c.a, c.b, c.c); return;
    case Op::Motif:
    case Op::Celebrate:
      if (!running_) return;  // reference: if (!ac) return
      if (c.a <= 0) {
        c.op == Op::Motif ? play_motif(static_cast<Color>(c.arg)) : play_celebrate(c.arg != 0);
        return;
      }
      for (Pending& p : pending_) {
        if (!p.active) { p = Pending{true, now_ + samples(c.a), c.op, c.arg}; return; }
      }
      return;
  }
}

float ColorPopScore::next_note(Note& n) {
  float g;
  if (n.pos < n.attack) {
    g = kFloor + (n.peak - kFloor) * static_cast<float>(n.pos) / static_cast<float>(n.attack);
    n.g = n.peak;
  } else if (n.pos < n.attack + n.decay) {
    g = n.g;
    n.g *= n.mul;
  } else {
    g = kFloor;
  }
  float s;
  if (!n.glide) {
    s = sine_at(n.phase);
    n.phase += n.inc;
    if (n.overtone) {
      float g2;
      if (n.pos < n.attack) {
        g2 = kFloor + (n.g2 - kFloor) * static_cast<float>(n.pos) / static_cast<float>(n.attack);
      } else if (n.pos < n.attack + n.decay2) {
        g2 = n.g2;
        n.g2 *= n.mul2;
      } else {
        g2 = kFloor;
      }
      // Reference graph: overtone gain g2 feeds the body gain g.
      s += g2 * sine_at(n.phase2);
      n.phase2 += n.inc2;
    }
  } else {
    s = sine_at(n.phase);
    n.phase += static_cast<uint32_t>(n.incf);
    if (n.pos < n.glide_len) n.incf *= n.fmul;
  }
  if (++n.pos >= n.len) n.active = false;
  return g * s;
}

void ColorPopScore::render_add(int32_t* out, std::size_t count) {
  uint32_t tail = tail_.load(std::memory_order_relaxed);
  const uint32_t head = head_.load(std::memory_order_acquire);
  while (tail != head) {
    const Command c = ring_[tail % kQueue];
    tail_.store(++tail, std::memory_order_release);
    apply(c);
  }
  if (!running_) { now_ += static_cast<uint32_t>(count); return; }

  static const Lowpass lp;
  static const float master_c = 1.f - std::exp(-1.f / (0.1f * kSr));
  static const float att = 1.f - std::exp(-static_cast<float>(kCompBlock) / (0.003f * kSr));
  static const float rel = 1.f - std::exp(-static_cast<float>(kCompBlock) / (0.15f * kSr));
  static const float makeup = makeup_db();
  static const uint32_t lfo_inc = incr_for(0.11f);
  float buf[kCompBlock];
  for (std::size_t i0 = 0; i0 < count; i0 += kCompBlock) {
    const std::size_t len = count - i0 < kCompBlock ? count - i0 : kCompBlock;
    for (Pending& p : pending_) {
      if (p.active && static_cast<int32_t>(now_ - p.at) >= 0) {
        p.active = false;
        p.op == Op::Motif ? play_motif(static_cast<Color>(p.arg)) : play_celebrate(p.arg != 0);
      }
    }
    float peak = 0;
    for (std::size_t j = 0; j < len; ++j) {
      float x = 0;
      for (Note& n : notes_) {
        if (n.active && static_cast<int32_t>(now_ - n.start) >= 0) x += next_note(n);
      }
      noise_ ^= noise_ << 13; noise_ ^= noise_ >> 17; noise_ ^= noise_ << 5;
      const float w = static_cast<float>(noise_) * (2.f / 4294967296.f) - 1.f;
      const float y = lp.b0 * w + lp.b1 * x1_ + lp.b2 * x2_ - lp.a1 * y1_ - lp.a2 * y2_;
      x2_ = x1_; x1_ = w; y2_ = y1_; y1_ = y;
      x += y * (0.018f + 0.014f * sine_at(lfo_phase_));
      lfo_phase_ += lfo_inc;
      master_ += (0.35f - master_) * master_c;
      x *= master_;
      const float a = std::fabs(x);
      if (a > peak) peak = a;
      buf[j] = delay_[delay_pos_];
      delay_[delay_pos_] = x;
      if (++delay_pos_ >= kPreDelay) delay_pos_ = 0;
      ++now_;
    }
    const float in_db = peak > 1e-6f ? 20.f * std::log10(peak) : -120.f;
    const float want = curve_db(in_db) - in_db;
    env_db_ += (want - env_db_) * (want < env_db_ ? att : rel);
    const float target = std::pow(10.f, (env_db_ + makeup) / 20.f);
    const float step = (target - comp_gain_) / static_cast<float>(len);
    for (std::size_t j = 0; j < len; ++j) {
      comp_gain_ += step;
      out[i0 + j] += static_cast<int32_t>(buf[j] * comp_gain_ * 32767.f * kOutputTrim);
    }
  }
}

}  // namespace color_pop
