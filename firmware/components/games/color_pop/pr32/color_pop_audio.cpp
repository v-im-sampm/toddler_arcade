#include "color_pop_audio.hpp"

#include <cmath>

#include "audio/AudioMusicTypes.h"
#include "audio/ApuCore.h"

namespace little_arcade::color_pop::pr32 {

namespace pa = pixelroot32::audio;

namespace {
constexpr std::size_t kChunk = 64;
// The APU's linear decay to silence is held for this share of the original
// exponential note length (equal-ish loudness, same perceived tail).
constexpr float kDecayShare = 0.6f;
// APU int16 output -> arcade mix. The cue peak lands near 0.4 FS before the
// VolumeLimiter (color_pop_pr32_test checks the range).
constexpr float kGain = 1.0f;

struct DecayPreset {
  float seconds;
  pa::InstrumentPreset preset;
};

pa::InstrumentPreset pluck(float decay) {
  pa::InstrumentPreset p{};
  p.baseVolume = 1.0f;
  p.duty = 0.5f;
  p.defaultOctave = 4;
  p.attackTime = 0.003f;
  p.decayTime = decay;
  p.sustainLevel = 0.0f;
  p.releaseTime = 0.005f;
  return p;
}

// The event's preset must outlive the note: one static preset per length used.
const pa::InstrumentPreset& preset_for(float seconds) {
  static const DecayPreset kPresets[] = {
      {0.09f, pluck(0.09f * kDecayShare)}, {0.14f, pluck(0.14f * kDecayShare)},
      {0.25f, pluck(0.25f * kDecayShare)}, {0.30f, pluck(0.30f * kDecayShare)},
      {0.35f, pluck(0.35f * kDecayShare)}, {0.55f, pluck(0.55f * kDecayShare)}};
  const DecayPreset* best = &kPresets[0];
  for (const DecayPreset& p : kPresets) {
    if (std::fabs(p.seconds - seconds) < std::fabs(best->seconds - seconds)) best = &p;
  }
  return best->preset;
}

pa::AudioEvent sine(float hz, float seconds, float volume) {
  pa::AudioEvent e{};
  e.type = pa::WaveType::SINE;
  e.frequency = hz;
  e.duration = seconds * kDecayShare + 0.01f;
  e.volume = volume > 1.0f ? 1.0f : volume;
  e.duty = 0.5f;
  e.preset = &preset_for(seconds);
  return e;
}

uint8_t color_degree(uint8_t color) {
  static constexpr uint8_t kDegree[4] = {0, 4, 2, 1};  // RED, BLUE, GREEN, YELLOW
  return kDegree[color & 3];
}
}  // namespace

float ColorPopAudio::degree_hz(int degree, int octave) {
  static constexpr int kSemis[5] = {0, 2, 4, 7, 9};
  const int wrap = degree >= 0 ? degree / 5 : -((4 - degree) / 5);
  const int idx = degree - wrap * 5;
  return 523.25f * std::pow(2.0f, static_cast<float>(octave + wrap) + static_cast<float>(kSemis[idx]) / 12.0f);
}

void ColorPopAudio::add(float when, const pa::AudioEvent& event) {
  if (pending_n_ >= kMaxPending) return;  // drop rather than grow: a burst of taps
  pending_[pending_n_++] = Pending{clock_ + static_cast<uint64_t>(when * kRateHz), event};
}

void ColorPopAudio::marimba(float freq, float when, float vel, float dur) {
  add(when, sine(freq, dur, 0.45f * vel));
  add(when, sine(freq * 3.99f, 0.09f, 0.18f * vel));  // wooden overtone
}

void ColorPopAudio::plop(float when, float vel, float hi) {
  pa::AudioEvent e = sine(760.0f * hi, 0.14f, 0.35f * vel);
  e.sweepEndHz = 230.0f * hi;
  e.sweepDurationSec = 0.07f;
  e.sweepCurve = pa::SweepCurve::Exponential;
  add(when, e);
}

bool ColorPopAudio::cue(Cue c, uint8_t variant) {
  struct Step {
    int8_t deg, oct;
    float t, v;
  };
  switch (c) {
    case Cue::AMBIENT: return true;  // the original's wave bed is not ported (quiet scene)
    case Cue::PROMPT: {
      static constexpr Step kRed[] = {{0, -1, 0.f, .7f}, {0, -1, .2f, 1.f}};
      static constexpr Step kBlue[] = {{4, 0, 0.f, .8f}, {3, 0, .12f, .8f}, {2, 0, .24f, .9f}};
      static constexpr Step kGreen[] = {{2, 0, 0.f, .8f}, {3, 0, .1f, .8f}, {4, 0, .2f, .9f}};
      static constexpr Step kYellow[] = {{5, 0, 0.f, .7f}, {7, 0, .09f, .7f}, {5, 0, .18f, .7f}, {7, 0, .27f, .8f}};
      const Step* steps = kRed;
      std::size_t count = 2;
      switch (variant & 3) {
        case 1: steps = kBlue, count = 3; break;
        case 2: steps = kGreen, count = 3; break;
        case 3: steps = kYellow, count = 4; break;
        default: break;
      }
      for (std::size_t i = 0; i < count; ++i) {
        marimba(degree_hz(steps[i].deg, steps[i].oct), steps[i].t, steps[i].v, 0.55f);
      }
      return true;
    }
    case Cue::SUCCESS:
      plop(0, 1, 1);
      marimba(degree_hz(color_degree(variant), 0), 0.03f, 1, 0.55f);
      return true;
    case Cue::CELEBRATE: {
      static constexpr int8_t kBig[] = {0, 1, 2, 3, 4, 5, 7, 10};
      static constexpr int8_t kSmall[] = {0, 2, 4, 5};
      const bool big = variant != 0;
      const int8_t* d = big ? kBig : kSmall;
      const std::size_t count = big ? 8 : 4;
      for (std::size_t i = 0; i < count; ++i) {
        marimba(degree_hz(d[i], 0), static_cast<float>(i) * (big ? 0.11f : 0.09f), 0.8f, 0.55f);
      }
      return true;
    }
    case Cue::REACTION: plop(0, 0.6f, 1.6f); return true;
    case Cue::NOTE: {
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

void ColorPopAudio::stop() {
  pending_n_ = 0;
  for (int ch = 0; ch < pa::ApuCore::NUM_CHANNELS; ++ch) {
    pa::AudioCommand cmd;
    cmd.type = pa::AudioCommandType::STOP_CHANNEL;
    cmd.channelIndex = static_cast<uint8_t>(ch);
    engine_.submitCommand(cmd);
  }
  // Let the stop commands drain on the next render.
  quiet_at_ = clock_ + kChunk;
}

void ColorPopAudio::render_add(int32_t* out, std::size_t n) {
  int16_t buf[kChunk];
  std::size_t done = 0;
  while (done < n) {
    const std::size_t m = n - done < kChunk ? n - done : kChunk;
    for (std::size_t i = 0; i < pending_n_;) {
      if (pending_[i].at <= clock_) {
        const pa::AudioEvent& e = pending_[i].event;
        engine_.playEvent(e);
        const uint64_t end = clock_ + static_cast<uint64_t>((e.duration + 0.02f) * kRateHz);
        if (end > quiet_at_) quiet_at_ = end;
        pending_[i] = pending_[--pending_n_];
      } else {
        ++i;
      }
    }
    if (clock_ < quiet_at_) {
      engine_.generateSamples(buf, static_cast<int>(m));
      for (std::size_t i = 0; i < m; ++i) out[done + i] += static_cast<int32_t>(static_cast<float>(buf[i]) * kGain);
    }
    clock_ += m;
    done += m;
  }
}

}  // namespace little_arcade::color_pop::pr32
