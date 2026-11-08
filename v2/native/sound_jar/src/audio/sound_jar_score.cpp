#include "audio/sound_jar_score.hpp"

#include <cmath>

#include "little_arcade/audio/score_dsp.hpp"

namespace sound_jar {
namespace {
using little_arcade::audio::score_dsp::curve_db;
using little_arcade::audio::score_dsp::kPi;
using little_arcade::audio::score_dsp::makeup_db;
using little_arcade::audio::score_dsp::sine_at;

constexpr float kSr = static_cast<float>(SoundJarScore::kSampleRate);
constexpr float kFloor = 0.0001f;  // env() start/end value
constexpr float kRoot = 293.66f;   // reference ROOT (D4)

inline float incr_f(float hz) { return hz / kSr * 4294967296.0f; }
inline uint32_t samples(float seconds) { return static_cast<uint32_t>(seconds * kSr + 0.5f); }
inline float tau_coef(float tau) { return 1.f - std::exp(-1.f / (tau * kSr)); }
inline float deg(int d, int o = 0) { return static_cast<float>(degree_hz(d, o)); }

// reference mulberry32 as a raw state step (whoosh noise buffer = mulberry32(5)).
inline float mulberry(uint32_t& a) {
  a += 0x6D2B79F5u;
  uint32_t t = (a ^ (a >> 15)) * (1u | a);
  t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
  return static_cast<float>(static_cast<double>(t ^ (t >> 14)) / 4294967296.0);
}
}  // namespace

std::size_t SoundJarScore::notes() const {
  std::size_t n = 0;
  for (const Note& note : notes_) n += note.active ? 1 : 0;
  return n;
}

std::size_t SoundJarScore::sampler_voices() const {
  std::size_t n = 0;
  for (const SamplerVoice& v : sampler_) n += v.active ? 1 : 0;
  return n;
}

// ---------------- UI thread ----------------
void SoundJarScore::push(const Command& c) {
  const uint32_t head = head_.load(std::memory_order_relaxed);
  if (head - tail_.load(std::memory_order_acquire) >= kQueue) {
    dropped_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  ring_[head % kQueue] = c;
  head_.store(head + 1, std::memory_order_release);
}
void SoundJarScore::start() { push({Op::Start, 0, 0, 0, 0, 0}); }
void SoundJarScore::start_music() { push({Op::StartMusic, 0, 0, 0, 0, 0}); }
void SoundJarScore::stop_music() { push({Op::StopMusic, 0, 0, 0, 0, 0}); }
void SoundJarScore::cue(Cue c, int arg) {
  push({Op::Cue, static_cast<uint8_t>(c), static_cast<uint8_t>(arg), 0, 0, 0});
}
void SoundJarScore::voice_start(uint32_t id, int take, double rate, bool reverse, bool loop) {
  push({Op::VoiceStart, static_cast<uint8_t>(take), static_cast<uint8_t>((reverse ? 1 : 0) | (loop ? 2 : 0)), id,
        static_cast<float>(rate), 0});
}
void SoundJarScore::voice_release(uint32_t id, double tau) {
  push({Op::VoiceRelease, 0, 0, id, static_cast<float>(tau), 0});
}
void SoundJarScore::voice_bend(uint32_t id, double semitones) {
  push({Op::VoiceBend, 0, 0, id, static_cast<float>(semitones), 0});
}
void SoundJarScore::voice_loop(uint32_t id) { push({Op::VoiceLoop, 0, 0, id, 0, 0}); }
void SoundJarScore::apply_fx(bool echo, bool robot, bool wobble) {
  push({Op::Fx, 0, static_cast<uint8_t>((echo ? 1 : 0) | (robot ? 2 : 0) | (wobble ? 4 : 0)), 0, 0, 0});
}
void SoundJarScore::duck(double seconds) { push({Op::Duck, 0, 0, 0, static_cast<float>(seconds), 0}); }
void SoundJarScore::drip(double delay, int degree) {
  push({Op::Drip, static_cast<uint8_t>(degree), 0, 0, static_cast<float>(delay), 0});
}

// ---------------- audio thread ----------------
void SoundJarScore::duck_at(float seconds) {
  const uint32_t until = now_ + samples(seconds);
  if (!ducking_ || static_cast<int32_t>(until - duck_until_) > 0) duck_until_ = until;
  ducking_ = true;
}

bool SoundJarScore::voice_ok(float seconds) {
  std::size_t k = 0;
  for (std::size_t i = 0; i < voice_n_; ++i)
    if (static_cast<int32_t>(voice_end_[i] - now_) > 0) voice_end_[k++] = voice_end_[i];
  voice_n_ = k;
  if (voice_n_ >= kMaxCueVoices) { ++refused_; return false; }
  voice_end_[voice_n_++] = now_ + samples(seconds);
  duck_at(seconds);
  return true;
}

bool SoundJarScore::music_voice_ok(uint32_t start, float seconds) {
  std::size_t k = 0;
  for (std::size_t i = 0; i < music_n_; ++i)
    if (static_cast<int32_t>(music_end_[i] - start) > 0) music_end_[k++] = music_end_[i];
  music_n_ = k;
  if (music_n_ >= kMaxMusicVoices) return false;
  music_end_[music_n_++] = start + samples(seconds);
  return true;
}

SoundJarScore::Note* SoundJarScore::slot() {
  for (Note& n : notes_)
    if (!n.active) return &n;
  return nullptr;
}

void SoundJarScore::marimba(float freq, float when, float vel, float dur) {
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
  n->incf = incr_f(freq);
  n->fmul = 1;
  n->overtone = freq * 3.99f < 7600.f;
  n->g2peak = 0.18f * vel;
  n->decay2 = samples(0.09f) - n->attack;
  n->mul2 = std::pow(kFloor / n->g2peak, 1.f / static_cast<float>(n->decay2));
  n->inc2 = incr_f(freq * 3.99f);
}

void SoundJarScore::glide(float f0, float f1, float glide_s, float when, float peak, float dur, float attack,
                          bool music) {
  if (!running_ || (!music && !voice_ok(dur))) return;
  Note* n = slot();
  if (!n) return;
  *n = Note{};
  n->active = true;
  n->music = music;
  n->start = now_ + samples(when);
  n->len = samples(dur + 0.04f);
  n->attack = samples(attack);
  n->peak = peak;
  n->decay = samples(dur) - n->attack;
  n->mul = std::pow(kFloor / n->peak, 1.f / static_cast<float>(n->decay));
  n->incf = incr_f(f0);
  n->glide_len = samples(glide_s);
  n->fmul = std::pow(f1 / f0, 1.f / static_cast<float>(n->glide_len));
}

void SoundJarScore::woodblock(float freq, float when, float vel) {
  glide(freq, freq * 0.92f, 0.05f, when, 0.42f * vel, 0.12f, 0.003f);
  glide(freq * 2.76f, freq * 2.76f, 0.01f, when, 0.14f * vel, 0.04f, 0.003f);
}

void SoundJarScore::whoosh() {
  if (!running_ || !voice_ok(0.5f)) return;
  for (Whoosh& w : whoosh_) {
    if (w.active) continue;
    w = Whoosh{};
    w.active = true; w.rng = 5u;
    break;
  }
  glide(deg(4), deg(0, -1), 0.35f, 0.02f, 0.08f, 0.4f, 0.003f);
}

void SoundJarScore::play_cue(Cue c, int arg) {
  switch (c) {
    case Cue::TapNote: marimba(deg(0, 1), 0, 0.3f, 0.25f); return;
    case Cue::PadNote: marimba(kRoot * std::pow(2.f, kPads[arg % 5].semitones / 12.f), 0, 0.35f, 0.3f); return;
    case Cue::LidPop:
      glide(deg(0, -1), deg(0), 0.06f, 0, 0.3f, 0.13f, 0.003f);
      marimba(deg(4), 0.05f, 0.3f, 0.25f);
      return;
    case Cue::LidShut: plop(0, 1, 0.9f); woodblock(deg(0, -1), 0.02f, 0.5f); return;
    case Cue::SoftShut: woodblock(deg(0, -1), 0, 0.35f); return;
    case Cue::Tada: {
      static constexpr int kD[4] = {0, 2, 4, 5};
      for (int i = 0; i < 4; ++i) marimba(deg(kD[i]), i * 0.09f, 0.7f, 0.4f);
      return;
    }
    case Cue::Hmm:
      glide(deg(2, -1), deg(2, -1), 0.01f, 0, 0.2f, 0.26f, 0.04f);
      glide(deg(2, -1), deg(4, -1), 0.12f, 0.3f, 0.2f, 0.34f, 0.04f);
      return;
    case Cue::Hint:
      glide(deg(0, -1), deg(0, -1), 0.01f, 0, 0.2f, 0.3f, 0.06f);
      glide(deg(2, -1), deg(2, -1), 0.01f, 0.34f, 0.18f, 0.36f, 0.06f);
      return;
    case Cue::FxOn: chirp(deg(1 + arg), 0, 0.7f); marimba(deg(1 + arg, 1), 0.06f, 0.4f, 0.3f); return;
    case Cue::FxOff: plop(0, 0.6f, 1.3f); return;
    case Cue::FxBump: plop(0.06f, 0.4f, 1.3f); return;
    case Cue::Wake: chirp(deg(2), 0, 0.8f); chirp(deg(4), 0.14f, 0.7f); return;
    case Cue::Whoosh: whoosh(); return;
  }
}

void SoundJarScore::play_drip(float delay, int degree) {
  if (!running_) return;
  const float f = deg(degree, 1);
  static constexpr float kD[2][2] = {{0.f, 0.06f}, {0.3f, 0.022f}};  // the plink, then its cave echo
  for (const auto& d : kD) {
    if (!music_voice_ok(now_ + samples(delay + d[0]), 0.3f)) continue;
    // reference: osc stops at +0.33 s (env reaches its floor at 0.28 s)
    glide(f * 0.8f, f, 0.03f, delay + d[0], d[1], 0.28f, 0.004f, true);
  }
}

SoundJarScore::SamplerVoice* SoundJarScore::find(uint32_t id) {
  for (SamplerVoice& v : sampler_)
    if (v.active && v.id == id) return &v;
  return nullptr;
}

void SoundJarScore::apply(const Command& c) {
  switch (c.op) {
    case Op::Start:
      if (!running_) { running_ = true; master_ = 0; }
      return;
    case Op::StartMusic:
      music_on_ = true; music_ = 0; music_at_ = now_ + samples(0.5f);
      return;
    case Op::StopMusic: music_on_ = false; return;
    case Op::Cue: if (running_) play_cue(static_cast<Cue>(c.arg), c.flags); return;
    case Op::Drip: play_drip(c.a, c.arg); return;
    case Op::Duck: duck_at(c.a); return;
    case Op::Fx:
      echo_to_ = (c.flags & 1) ? 1.f : 0.f;
      dry_to_ = (c.flags & 2) ? 0.f : 1.f;
      wet_to_ = (c.flags & 2) ? 1.f : 0.f;
      wob_to_ = (c.flags & 4) ? 150.f : 0.f;
      return;
    case Op::VoiceStart: {
      if (!running_ || c.arg >= kTakeCount) return;
      SamplerVoice* v = nullptr;
      for (SamplerVoice& s : sampler_)
        if (!s.active) { v = &s; break; }
      if (!v) {  // all slots busy: reuse the quietest releasing tail (else slot 0)
        v = &sampler_[0];
        for (SamplerVoice& s : sampler_)
          if (s.rel && (!v->rel || s.gain < v->gain)) v = &s;
      }
      *v = SamplerVoice{};
      v->active = true; v->id = c.id; v->take = c.arg;
      v->rev = (c.flags & 1) != 0; v->loop = (c.flags & 2) != 0;
      v->rate = c.a; v->gain = kFloor; v->target = 0.9f;
      v->attack_left = samples(0.005f);
      const TakeData& tk = kTakeData[c.arg];
      duck_at(static_cast<float>(tk.dur) / c.a + 0.2f);
      return;
    }
    case Op::VoiceRelease: {
      SamplerVoice* v = find(c.id);
      if (!v || v->rel) return;
      v->rel = true; v->attack_left = 0;
      if (v->gain < kFloor) v->gain = kFloor;
      v->target = kFloor; v->coef = tau_coef(c.a);
      v->stop_left = samples(c.a * 6);
      return;
    }
    case Op::VoiceBend:
      if (SamplerVoice* v = find(c.id)) v->bend_target = c.a * 100.f;
      return;
    case Op::VoiceLoop:
      if (SamplerVoice* v = find(c.id)) v->loop = true;
      return;
  }
}

float SoundJarScore::next_note(Note& n) {
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
  float s = sine_at(n.phase);
  n.phase += static_cast<uint32_t>(n.incf);
  if (n.pos < n.glide_len) n.incf *= n.fmul;
  if (n.overtone) {
    float g2;
    if (n.pos < n.attack) {
      g2 = kFloor + (n.g2peak - kFloor) * static_cast<float>(n.pos) / static_cast<float>(n.attack);
      n.g2 = n.g2peak;
    } else if (n.pos < n.attack + n.decay2) {
      g2 = n.g2;
      n.g2 *= n.mul2;
    } else {
      g2 = kFloor;
    }
    // Reference graph: the overtone gain g2 feeds the body gain g.
    s += g2 * sine_at(n.phase2);
    n.phase2 += static_cast<uint32_t>(n.inc2);
  }
  if (++n.pos >= n.len) n.active = false;
  return g * s;
}

float SoundJarScore::next_whoosh(Whoosh& w) {
  // Web Audio band-pass (constant 0 dB peak), Q 1.4, 1800 -> 350 Hz exponential over 0.42 s.
  static const uint32_t sweep = samples(0.42f), attack = samples(0.08f), end = samples(0.46f), len = samples(0.5f);
  static const float decay_mul = std::pow(kFloor / 0.3f, 1.f / static_cast<float>(end - attack));
  if ((w.pos & 15u) == 0) {  // coefficients follow the sweep every 16 samples
    const float t = static_cast<float>(w.pos < sweep ? w.pos : sweep) / static_cast<float>(sweep);
    const float f = 1800.f * std::pow(350.f / 1800.f, t);
    const float w0 = 2.f * kPi * f / kSr, alpha = std::sin(w0) / (2.f * 1.4f), a0 = 1.f + alpha;
    w.b0 = alpha / a0; w.b2 = -alpha / a0; w.a1 = -2.f * std::cos(w0) / a0; w.a2 = (1.f - alpha) / a0;
  }
  const float x = mulberry(w.rng) * 2.f - 1.f;
  const float y = w.b0 * x + w.b2 * w.x2 - w.a1 * w.y1 - w.a2 * w.y2;
  w.x2 = w.x1; w.x1 = x; w.y2 = w.y1; w.y1 = y;
  float g;
  if (w.pos < attack) {
    g = kFloor + (0.3f - kFloor) * static_cast<float>(w.pos) / static_cast<float>(attack);
    w.g = 0.3f;
  } else if (w.pos < end) {
    g = w.g;
    w.g *= decay_mul;
  } else {
    g = kFloor;
  }
  if (++w.pos >= len) w.active = false;
  return y * g;
}

void SoundJarScore::render_add(int32_t* out, std::size_t count) {
  uint32_t tail = tail_.load(std::memory_order_relaxed);
  const uint32_t head = head_.load(std::memory_order_acquire);
  while (tail != head) {
    const Command c = ring_[tail % kQueue];
    tail_.store(++tail, std::memory_order_release);
    apply(c);
  }
  if (!running_) { now_ += static_cast<uint32_t>(count); return; }

  static const float master_c = tau_coef(0.1f), param_c = tau_coef(0.02f);
  static const float music_in = tau_coef(1.5f), music_out = tau_coef(0.15f);
  static const float duck_down = tau_coef(0.03f), duck_up = tau_coef(0.3f);
  static const float bend_block = 1.f - std::exp(-static_cast<float>(kCompBlock) / (0.02f * kSr));
  static const float att = 1.f - std::exp(-static_cast<float>(kCompBlock) / (0.003f * kSr));
  static const float rel = 1.f - std::exp(-static_cast<float>(kCompBlock) / (0.15f * kSr));
  static const float makeup = makeup_db();
  static const uint32_t ring_inc = static_cast<uint32_t>(incr_f(55.f)), lfo_inc = static_cast<uint32_t>(incr_f(6.f));
  float buf[kCompBlock];
  float step[kMaxSampler];
  for (std::size_t i0 = 0; i0 < count; i0 += kCompBlock) {
    const std::size_t len = count - i0 < kCompBlock ? count - i0 : kCompBlock;
    // Per block: smoothed bend + shared wobble -> playback step of each sampler voice.
    const float wob_cents = wob_ * sine_at(lfo_phase_);
    for (std::size_t k = 0; k < kMaxSampler; ++k) {
      SamplerVoice& v = sampler_[k];
      if (!v.active) continue;
      v.bend_cents += (v.bend_target - v.bend_cents) * bend_block;
      step[k] = v.rate * std::pow(2.f, (v.bend_cents + wob_cents) / 1200.f);
    }
    float peak = 0;
    for (std::size_t j = 0; j < len; ++j) {
      float x = 0, mus = 0, bus = 0;
      for (Note& n : notes_) {
        if (!n.active || static_cast<int32_t>(now_ - n.start) < 0) continue;
        const float s = next_note(n);
        if (n.music) mus += s; else x += s;
      }
      for (Whoosh& w : whoosh_)
        if (w.active) x += next_whoosh(w);
      for (std::size_t k = 0; k < kMaxSampler; ++k) {
        SamplerVoice& v = sampler_[k];
        if (!v.active) continue;
        const TakeData& tk = kTakeData[v.take];
        const int n = tk.samples;
        const int i = static_cast<int>(v.pos);
        const float frac = static_cast<float>(v.pos - i);
        auto at = [&](int idx) -> float {
          if (idx >= n) { if (!v.loop) return 0.f; idx -= n; }
          return static_cast<float>(tk.pcm[v.rev ? n - 1 - idx : idx]) * (1.f / 32768.f);
        };
        const float s = at(i) + (at(i + 1) - at(i)) * frac;
        if (v.attack_left > 0) {
          v.gain += (0.9f - v.gain) / static_cast<float>(v.attack_left);
          --v.attack_left;
        } else if (v.rel) {
          v.gain += (v.target - v.gain) * v.coef;
        }
        bus += s * v.gain;
        v.pos += step[k];
        if (v.pos >= n) {
          if (v.loop) v.pos -= n;
          else v.active = false;
        }
        if (v.rel && v.stop_left > 0 && --v.stop_left == 0) v.active = false;
      }
      // Sampler bus: dry + 55 Hz ring mod, then the 250 ms echo (feedback 0.45) to master.
      dry_ += (dry_to_ - dry_) * param_c;
      wet_ += (wet_to_ - wet_) * param_c;
      echo_ += (echo_to_ - echo_) * param_c;
      wob_ += (wob_to_ - wob_) * param_c;
      const float s_out = bus * dry_ + bus * sine_at(ring_phase_) * wet_;
      ring_phase_ += ring_inc;
      lfo_phase_ += lfo_inc;
      const float fb = 0.45f * echo_line_[echo_pos_];
      echo_line_[echo_pos_] = s_out * echo_ + fb;
      if (++echo_pos_ >= kEchoLen) echo_pos_ = 0;
      x += s_out + fb;
      // Music bus: fade in after 0.5 s (tau 1.5), out (tau 0.15); duck to 0.35 under cues.
      if (music_on_) {
        if (static_cast<int32_t>(now_ - music_at_) >= 0) music_ += (1.f - music_) * music_in;
      } else {
        music_ += (0.f - music_) * music_out;
      }
      if (ducking_ && static_cast<int32_t>(now_ - duck_until_) < 0) duck_ += (0.35f - duck_) * duck_down;
      else { ducking_ = false; duck_ += (1.f - duck_) * duck_up; }
      x += mus * music_ * duck_;
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
    const float gstep = (target - comp_gain_) / static_cast<float>(len);
    for (std::size_t j = 0; j < len; ++j) {
      comp_gain_ += gstep;
      out[i0 + j] += static_cast<int32_t>(buf[j] * comp_gain_ * 32767.f * kOutputTrim);
    }
  }
}

}  // namespace sound_jar
