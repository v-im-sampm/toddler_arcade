#include "little_arcade/audio/color_pop_score.hpp"

#include <cmath>

#include "little_arcade/audio/score_dsp.hpp"
#include "little_arcade/audio/synth.hpp"

namespace little_arcade::audio {
namespace {

using score_dsp::curve_db;
using score_dsp::kPi;
using score_dsp::makeup_db;
using score_dsp::sine_at;

constexpr float kSr = static_cast<float>(kSampleRateHz);
constexpr float kFloor = 0.0001f;  // env() start/end value
constexpr int kPent[5] = {0, 2, 4, 7, 9};

// Compressor: attack 3 ms, release 150 ms (curve in score_dsp.hpp).
// White noise at 16 kHz packs 3x the power of 48 kHz noise into the band.
constexpr float kNoiseScale = 0.57735f;  // sqrt(16000 / 48000)

inline float incr_f(float hz) { return hz / kSr * 4294967296.0f; }
inline uint32_t incr_for(float hz) { return static_cast<uint32_t>(incr_f(hz)); }

inline uint32_t samples(float seconds) { return static_cast<uint32_t>(seconds * kSr + 0.5f); }

}  // namespace

float ColorPopScore::degree_hz(int degree, int octave) {
  const int o = degree >= 0 ? degree / 5 : -((4 - degree) / 5);
  const int i = ((degree % 5) + 5) % 5;
  return 523.25f * std::pow(2.f, static_cast<float>(kPent[i] + 12 * (o + octave)) / 12.f);
}

uint8_t ColorPopScore::color_degree(uint8_t color) {
  constexpr uint8_t kDeg[4] = {0, 4, 2, 1};  // RED, BLUE, GREEN, YELLOW (reference COLORS.deg)
  return kDeg[color & 3];
}

std::size_t ColorPopScore::notes() const {
  std::size_t n = 0;
  for (const Note& note : notes_) n += note.active ? 1 : 0;
  return n;
}

void ColorPopScore::start_audio() {
  if (running_ && master_target_ > 0) return;
  running_ = true;
  master_target_ = 0.35f;
  master_coef_ = 1.f - std::exp(-1.f / (0.1f * kSr));
  waves_ = true;
  // RBJ lowpass, f0 = 420 Hz, Web Audio Q = 1 dB.
  const float w0 = 2.f * kPi * 420.f / kSr;
  const float alpha = std::sin(w0) / (2.f * std::pow(10.f, 1.f / 20.f));
  const float cw = std::cos(w0), a0 = 1.f + alpha;
  b0_ = (1.f - cw) * 0.5f / a0;
  b1_ = (1.f - cw) / a0;
  b2_ = b0_;
  a1_ = -2.f * cw / a0;
  a2_ = (1.f - alpha) / a0;
}

void ColorPopScore::stop() {
  if (!running_) return;
  master_target_ = 0;
  master_coef_ = 1.f - std::exp(-1.f / (0.02f * kSr));
}

bool ColorPopScore::voice_ok(float seconds) {
  std::size_t k = 0;
  for (std::size_t i = 0; i < voice_n_; ++i) {
    if (voice_end_[i] > now_) voice_end_[k++] = voice_end_[i];
  }
  voice_n_ = k;
  if (voice_n_ >= kMaxVoices) return false;
  voice_end_[voice_n_++] = now_ + samples(seconds);
  return true;
}

void ColorPopScore::marimba(float freq, float when, float vel, float dur) {
  if (!running_ || !voice_ok(dur)) return;
  for (Note& n : notes_) {
    if (n.active) continue;
    n = Note{};
    n.kind = Kind::MARIMBA;
    n.active = true;
    n.start = now_ + samples(when);
    n.len = samples(dur + 0.05f);
    n.attack = samples(0.003f);
    n.peak = 0.45f * vel;
    n.decay = samples(dur) - n.attack;
    n.mul = std::pow(kFloor / n.peak, 1.f / static_cast<float>(n.decay));
    n.g2 = 0.18f * vel;
    n.decay2 = samples(0.09f) - n.attack;
    n.mul2 = std::pow(kFloor / n.g2, 1.f / static_cast<float>(n.decay2));
    n.freq = freq;
    n.inc = incr_for(freq);
    n.inc2 = incr_for(freq * 3.99f);
    n.overtone = freq * 3.99f < 7800.f;
    return;
  }
}

void ColorPopScore::plop(float when, float vel, float hi) {
  if (!running_ || !voice_ok(0.15f)) return;
  for (Note& n : notes_) {
    if (n.active) continue;
    n = Note{};
    n.kind = Kind::PLOP;
    n.active = true;
    n.start = now_ + samples(when);
    n.len = samples(0.18f);
    n.attack = samples(0.003f);
    n.peak = 0.35f * vel;
    n.decay = samples(0.14f) - n.attack;
    n.mul = std::pow(kFloor / n.peak, 1.f / static_cast<float>(n.decay));
    n.f = 760.f * hi;
    n.fmul = std::pow(230.f / 760.f, 1.f / static_cast<float>(samples(0.07f)));
    n.incf = incr_f(n.f);
    n.glide = samples(0.07f);
    return;
  }
}

void ColorPopScore::motif(uint8_t color) {
  struct Step {
    int8_t deg, oct;
    float t, v;
  };
  static constexpr Step kRed[] = {{0, -1, 0.f, .7f}, {0, -1, .2f, 1.f}};
  static constexpr Step kBlue[] = {{4, 0, 0.f, .8f}, {3, 0, .12f, .8f}, {2, 0, .24f, .9f}};
  static constexpr Step kGreen[] = {{2, 0, 0.f, .8f}, {3, 0, .1f, .8f}, {4, 0, .2f, .9f}};
  static constexpr Step kYellow[] = {{5, 0, 0.f, .7f}, {7, 0, .09f, .7f}, {5, 0, .18f, .7f}, {7, 0, .27f, .8f}};
  const Step* steps = kRed;
  std::size_t count = 2;
  switch (color & 3) {
    case 1:
      steps = kBlue;
      count = 3;
      break;
    case 2:
      steps = kGreen;
      count = 3;
      break;
    case 3:
      steps = kYellow;
      count = 4;
      break;
    default: break;
  }
  for (std::size_t i = 0; i < count; ++i) marimba(degree_hz(steps[i].deg, steps[i].oct), steps[i].t, steps[i].v, 0.55f);
}

void ColorPopScore::celebrate(bool big) {
  static constexpr int8_t kBig[] = {0, 1, 2, 3, 4, 5, 7, 10};
  static constexpr int8_t kSmall[] = {0, 2, 4, 5};
  const int8_t* d = big ? kBig : kSmall;
  const std::size_t count = big ? 8 : 4;
  for (std::size_t i = 0; i < count; ++i) {
    marimba(degree_hz(d[i], 0), static_cast<float>(i) * (big ? 0.11f : 0.09f), 0.8f, 0.55f);
  }
}

bool ColorPopScore::cue(Cue c, uint8_t variant) {
  switch (c) {
    case Cue::AMBIENT: start_audio(); return true;
    case Cue::PROMPT: start_audio(); motif(variant); return true;
    case Cue::SUCCESS:
      start_audio();
      plop(0, 1, 1);
      marimba(degree_hz(color_degree(variant), 0), 0.03f, 1, 0.55f);
      return true;
    case Cue::CELEBRATE: start_audio(); celebrate(variant != 0); return true;
    case Cue::REACTION: start_audio(); plop(0, 0.6f, 1.6f); return true;
    case Cue::NOTE: {
      start_audio();
      const int deg = variant & 0x0F;
      switch (variant & 0xF0) {
        case 0x10:
          plop(0, 0.55f, 1.4f);
          marimba(degree_hz(deg, 1), 0.02f, 0.45f, 0.35f);
          return true;
        case 0x20: marimba(degree_hz(0, 1), 0, 0.3f, 0.25f); return true;
        case 0x30: marimba(degree_hz(deg, 1), 0, 0.4f, 0.3f); return true;
        default: return false;
      }
    }
    default: return false;
  }
}

float ColorPopScore::next_note(Note& n) {
  // Gain envelope: 0.0001 -> peak (linear, 3 ms) -> 0.0001 (exponential).
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
  if (n.kind == Kind::MARIMBA) {
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
      s += g2 * sine_at(n.phase2);
      n.phase2 += n.inc2;
    }
  } else {
    s = sine_at(n.phase);
    n.phase += static_cast<uint32_t>(n.incf);
    if (n.pos < n.glide) n.incf *= n.fmul;
  }
  if (++n.pos >= n.len) n.active = false;
  return g * s;
}

void ColorPopScore::render_add(int32_t* out, std::size_t count) {
  if (!running_) {
    now_ += static_cast<uint32_t>(count);
    return;
  }
  const float lfo_incr = 0.11f / kSr * 4294967296.0f;
  // Detector coefficients per compressor block (3 ms attack, 150 ms release).
  static const float att = 1.f - std::exp(-static_cast<float>(kCompBlock) / (0.003f * kSr));
  static const float rel = 1.f - std::exp(-static_cast<float>(kCompBlock) / (0.15f * kSr));
  static const float makeup = makeup_db();
  float buf[kCompBlock];
  for (std::size_t i0 = 0; i0 < count; i0 += kCompBlock) {
    const std::size_t len = count - i0 < kCompBlock ? count - i0 : kCompBlock;
    float peak = 0;
    for (std::size_t j = 0; j < len; ++j) {
      float x = 0;
      for (Note& n : notes_) {
        if (n.active && static_cast<int32_t>(now_ - n.start) >= 0) x += next_note(n);
      }
      if (waves_) {
        noise_ ^= noise_ << 13;
        noise_ ^= noise_ >> 17;
        noise_ ^= noise_ << 5;
        // Same lowpassed noise power as the reference's 48 kHz white noise.
        const float w = (static_cast<float>(noise_) * (2.f / 4294967296.f) - 1.f) * kNoiseScale;
        const float y = b0_ * w + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
        x2_ = x1_;
        x1_ = w;
        y2_ = y1_;
        y1_ = y;
        x += y * (0.018f + 0.014f * sine_at(lfo_phase_));
        lfo_phase_ += static_cast<uint32_t>(lfo_incr);
      }
      master_ += (master_target_ - master_) * master_coef_;
      x *= master_;
      buf[j] = x;
      const float a = std::fabs(x);
      if (a > peak) peak = a;
      ++now_;
    }
    // Compressor (feed-forward peak detector), one gain update per block; the
    // applied gain ramps linearly so block steps never click.
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
  if (master_target_ == 0 && master_ < 1e-4f) {
    // Stop finished: silence everything until the next AMBIENT.
    for (Note& n : notes_) n.active = false;
    voice_n_ = 0;
    waves_ = false;
    running_ = false;
    master_ = 0;
    env_db_ = 0;
    comp_gain_ = 1;
    x1_ = x2_ = y1_ = y2_ = 0;
  }
}

}  // namespace little_arcade::audio
