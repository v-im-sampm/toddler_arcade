#include "little_arcade/audio/synth.hpp"

#include <cmath>

namespace little_arcade::audio {
namespace {

constexpr int32_t kWavePeak = 32767;
constexpr int32_t kSquarePeak = 20000;  // square/noise sound louder than sine at equal peak

struct SineTable {
  int16_t v[257];
  SineTable() {
    for (int i = 0; i <= 256; ++i) {
      v[i] = static_cast<int16_t>(std::lround(kWavePeak * std::sin(6.283185307179586 * i / 256.0)));
    }
  }
};

const SineTable& sine_table() {
  static const SineTable table;
  return table;
}

uint32_t ms_to_samples(uint32_t ms) { return ms * kSampleRateHz / 1000u; }

int64_t incr_for(uint32_t hz) { return static_cast<int64_t>((static_cast<uint64_t>(hz) << 32) / kSampleRateHz); }

}  // namespace

void Synth::trigger(const Sound& sound) {
  if (sound.empty()) return;
  Voice* pick = &voices_[0];
  for (Voice& v : voices_) {
    if (!v.active) {
      pick = &v;
      break;
    }
    if (v.age < pick->age) pick = &v;
  }
  Voice& v = *pick;
  v = Voice{};
  v.sound = sound;
  v.active = true;
  v.age = ++trigger_count_;
  v.lfsr = static_cast<uint16_t>(0xACE1u ^ (v.age * 0x9E37u));
  if (v.lfsr == 0) v.lfsr = 1;
  if (sound.has_clip()) {
    v.in_clip = true;
  } else {
    start_tone(v);
  }
}

void Synth::stop_all() {
  for (Voice& v : voices_) v.active = false;
}

std::size_t Synth::active_voices() const {
  std::size_t n = 0;
  for (const Voice& v : voices_) n += v.active ? 1u : 0u;
  return n;
}

void Synth::start_tone(Voice& v) {
  const Tone& t = v.sound.tones[v.index];
  v.pos = 0;
  v.len = ms_to_samples(t.ms);
  if (v.len == 0) v.len = 1;
  const uint32_t edge_max = v.len / 2 ? v.len / 2 : 1;
  const uint32_t a = ms_to_samples(t.attack_ms > kMinEdgeMs ? t.attack_ms : kMinEdgeMs);
  const uint32_t r = ms_to_samples(t.release_ms > kMinEdgeMs ? t.release_ms : kMinEdgeMs);
  v.attack = a > edge_max ? edge_max : (a ? a : 1);
  v.release = r > edge_max ? edge_max : (r ? r : 1);
  const uint32_t f1 = t.f1_hz ? t.f1_hz : t.f0_hz;
  v.incr = incr_for(t.f0_hz);
  v.dincr = (incr_for(f1) - v.incr) / static_cast<int64_t>(v.len);
}

// Clips get the same minimum edge ramp as tones, so even an unfaded clip
// cannot click when it starts or ends.
int32_t Synth::next_clip_sample(Voice& v) {
  const PcmClip& c = *v.sound.clip;
  const uint32_t edge = ms_to_samples(kMinEdgeMs);
  const uint32_t left = c.count - v.clip_pos;
  int32_t env = 32768;
  if (v.clip_pos < edge) env = static_cast<int32_t>((static_cast<uint64_t>(v.clip_pos) << 15) / edge);
  if (left <= edge) {
    const int32_t rel = static_cast<int32_t>((static_cast<uint64_t>(left - 1) << 15) / edge);
    if (rel < env) env = rel;
  }
  const int32_t s = c.samples[v.clip_pos++];
  return static_cast<int32_t>((static_cast<int64_t>(s) * env * v.sound.clip_level) / (32768 * 255));
}

int32_t Synth::next_sample(Voice& v) {
  if (v.in_clip) {
    if (v.clip_pos < v.sound.clip->count) return next_clip_sample(v);
    v.in_clip = false;
    if (v.sound.count == 0) {
      v.active = false;
      return 0;
    }
    start_tone(v);
  }
  while (v.pos >= v.len) {
    if (++v.index >= v.sound.count) {
      v.active = false;
      return 0;
    }
    start_tone(v);
  }
  const Tone& t = v.sound.tones[v.index];
  int32_t s = 0;
  if (t.f0_hz != 0) {
    const uint32_t old = v.phase;
    v.phase += static_cast<uint32_t>(v.incr);
    v.incr += v.dincr;
    if (v.incr < 0) v.incr = 0;
    switch (t.wave) {
      case Wave::SINE: {
        const SineTable& tab = sine_table();
        const uint32_t i = v.phase >> 24;
        const int32_t frac = static_cast<int32_t>((v.phase >> 8) & 0xFFFFu);
        s = tab.v[i] + (((tab.v[i + 1] - tab.v[i]) * frac) >> 16);
        break;
      }
      case Wave::TRIANGLE: {
        const int32_t p = static_cast<int32_t>(v.phase >> 16);
        s = (p < 32768 ? p * 2 : (65535 - p) * 2) - 32767;
        break;
      }
      case Wave::PULSE:
        s = v.phase < 0x80000000u ? kSquarePeak : -kSquarePeak;
        break;
      case Wave::NOISE:
        if (v.phase < old) {
          const uint16_t bit = static_cast<uint16_t>((v.lfsr ^ (v.lfsr >> 2) ^ (v.lfsr >> 3) ^ (v.lfsr >> 5)) & 1u);
          v.lfsr = static_cast<uint16_t>((v.lfsr >> 1) | (bit << 15));
          v.noise = static_cast<int16_t>((v.lfsr & 1u) ? kSquarePeak : -kSquarePeak);
        }
        s = v.noise;
        break;
    }
  }
  const uint32_t left = v.len - v.pos;
  int32_t env = 32768;
  if (v.pos < v.attack) env = static_cast<int32_t>((static_cast<uint64_t>(v.pos) << 15) / v.attack);
  if (left <= v.release) {
    const int32_t rel = static_cast<int32_t>((static_cast<uint64_t>(left - 1) << 15) / v.release);
    if (rel < env) env = rel;
  }
  ++v.pos;
  return static_cast<int32_t>((static_cast<int64_t>(s) * env * t.level) / (32768 * 255));
}

void Synth::render(int16_t* out, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    int32_t mix = 0;
    for (Voice& v : voices_) {
      if (v.active) mix += next_sample(v);
    }
    if (mix > 32767) mix = 32767;
    if (mix < -32768) mix = -32768;
    out[i] = static_cast<int16_t>(mix);
  }
}

}  // namespace little_arcade::audio
