#include "little_arcade/audio/hatch_a_pet_score.hpp"

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

// Reference LULLABY / SONG: [beat, degree, octave, peak, attack (s), dur (s)],
// already in the sketch's sort order (beat, then louder first).
struct SongEvent {
  uint8_t beat;
  int8_t degree, octave;
  float peak, attack, dur;
};
constexpr float kSpb = 60.f / 72.f;
constexpr uint32_t kSongBeats = 24;
constexpr float kMel = 0.6f;               // melody tail past the note
constexpr float kBass = 3.f * kSpb + 0.3f;
constexpr SongEvent kSong[] = {
    {0, 4, -1, 0.08f, 0.02f, 2 * kSpb + kMel},  {0, 0, -2, 0.06f, 0.25f, kBass},
    {2, 3, -1, 0.08f, 0.02f, 1 * kSpb + kMel},  {3, 2, -1, 0.08f, 0.02f, 1 * kSpb + kMel},
    {3, -2, -2, 0.06f, 0.25f, kBass},           {4, 3, -1, 0.08f, 0.02f, 1 * kSpb + kMel},
    {5, 4, -1, 0.08f, 0.02f, 1 * kSpb + kMel},  {6, 5, -1, 0.08f, 0.02f, 2 * kSpb + kMel},
    {6, -1, -2, 0.06f, 0.25f, kBass},           {8, 4, -1, 0.08f, 0.02f, 1 * kSpb + kMel},
    {9, 3, -1, 0.08f, 0.02f, 3 * kSpb + kMel},  {9, -2, -2, 0.06f, 0.25f, kBass},
    {12, 2, -1, 0.08f, 0.02f, 2 * kSpb + kMel}, {12, 0, -2, 0.06f, 0.25f, kBass},
    {14, 1, -1, 0.08f, 0.02f, 1 * kSpb + kMel}, {15, 0, -1, 0.08f, 0.02f, 1 * kSpb + kMel},
    {15, -1, -2, 0.06f, 0.25f, kBass},          {16, 1, -1, 0.08f, 0.02f, 1 * kSpb + kMel},
    {17, 2, -1, 0.08f, 0.02f, 1 * kSpb + kMel}, {18, 3, -1, 0.08f, 0.02f, 2 * kSpb + kMel},
    {18, -2, -2, 0.06f, 0.25f, kBass},          {20, 1, -1, 0.08f, 0.02f, 1 * kSpb + kMel},
    {21, 0, -1, 0.08f, 0.02f, 3 * kSpb + kMel}, {21, 0, -2, 0.06f, 0.25f, kBass},
};
constexpr std::size_t kSongLen = sizeof(kSong) / sizeof(kSong[0]);

// Sample time of song beat b (exact rational: 16000 * 60 / 72 = 40000 / 3).
inline uint32_t beat_samples(uint32_t b) {
  return static_cast<uint32_t>((static_cast<uint64_t>(b) * kSampleRateHz * 60u + 36u) / 72u);
}

inline float incr_f(float hz) { return hz / kSr * 4294967296.0f; }
inline uint32_t incr_for(float hz) { return static_cast<uint32_t>(incr_f(hz)); }
inline uint32_t samples(float seconds) { return static_cast<uint32_t>(seconds * kSr + 0.5f); }

}  // namespace

float HatchAPetScore::degree_hz(int degree, int octave) {
  const int o = degree >= 0 ? degree / 5 : -((4 - degree) / 5);
  const int i = ((degree % 5) + 5) % 5;
  return 698.46f * std::pow(2.f, static_cast<float>(kPent[i] + 12 * (o + octave)) / 12.f);
}

std::size_t HatchAPetScore::notes() const {
  std::size_t n = 0;
  for (const Note& note : notes_) n += note.active ? 1 : 0;
  return n;
}

std::size_t HatchAPetScore::music_notes() const {
  std::size_t n = 0;
  for (const MusicNote& note : music_) n += note.active ? 1 : 0;
  return n;
}

void HatchAPetScore::start_audio() {
  if (running_ && master_target_ > 0) return;
  running_ = true;
  master_target_ = 0.35f;
  master_coef_ = 1.f - std::exp(-1.f / (0.1f * kSr));
  // startMusic(): bus at 0, then setTargetAtTime(1, now + 0.5, 1.5).
  music_on_ = true;
  music_t0_ = now_ + samples(0.5f);
  music_next_ = 0;
  music_loop_ = 0;
  music_fade_ = 0;
  for (MusicNote& n : music_) n.active = false;
}

void HatchAPetScore::stop() {
  if (!running_) return;
  music_on_ = false;
  master_target_ = 0;
  master_coef_ = 1.f - std::exp(-1.f / (0.02f * kSr));
}

bool HatchAPetScore::voice_ok(float seconds) {
  std::size_t k = 0;
  for (std::size_t i = 0; i < voice_n_; ++i) {
    if (voice_end_[i] > now_) voice_end_[k++] = voice_end_[i];
  }
  voice_n_ = k;
  if (voice_n_ >= kMaxVoices) return false;
  voice_end_[voice_n_++] = now_ + samples(seconds);
  // duckMusic(now, len)
  if (static_cast<int32_t>(now_ + samples(seconds) - duck_until_) > 0) duck_until_ = now_ + samples(seconds);
  return true;
}

void HatchAPetScore::schedule_music(uint32_t horizon) {
  // musicTick(): every song event due before the horizon, in order.
  while (music_on_) {
    const SongEvent& e = kSong[music_next_];
    const uint32_t t = music_t0_ + beat_samples(music_loop_ * kSongBeats + e.beat);
    if (static_cast<int32_t>(t - horizon) > 0) return;
    if (++music_next_ >= kSongLen) {
      music_next_ = 0;
      ++music_loop_;
    }
    // musicVoiceOk(t, dur): its own pool, never the cue voices.
    std::size_t busy = 0;
    MusicNote* free = nullptr;
    for (MusicNote& n : music_) {
      if (n.active && static_cast<int32_t>(n.until - t) > 0) ++busy;
      if (!n.active && !free) free = &n;
    }
    if (busy >= kMusicVoices || !free) continue;
    MusicNote& n = *free;
    n = MusicNote{};
    n.active = true;
    n.start = t;
    n.len = samples(e.dur + 0.05f);
    n.until = t + samples(e.dur);
    n.attack = samples(e.attack);
    n.decay = samples(e.dur) - n.attack;
    n.peak = e.peak;
    n.mul = std::pow(kFloor / n.peak, 1.f / static_cast<float>(n.decay));
    n.inc = incr_for(degree_hz(e.degree, e.octave));
  }
}

float HatchAPetScore::next_music(MusicNote& n) {
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
  const float s = sine_at(n.phase);
  n.phase += n.inc;
  if (++n.pos >= n.len) n.active = false;
  return g * s;
}

HatchAPetScore::Note* HatchAPetScore::slot() {
  for (Note& n : notes_) {
    if (!n.active) return &n;
  }
  return nullptr;
}

void HatchAPetScore::marimba(float freq, float when, float vel, float dur) {
  if (!running_ || !voice_ok(dur)) return;
  Note* n = slot();
  if (!n) return;
  *n = Note{};
  n->kind = Kind::MARIMBA;
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

void HatchAPetScore::glide(float f0, float f1, float glide_s, float when, float peak, float dur) {
  if (!running_ || !voice_ok(dur)) return;
  Note* n = slot();
  if (!n) return;
  *n = Note{};
  n->kind = Kind::GLIDE;
  n->active = true;
  n->start = now_ + samples(when);
  n->len = samples(dur + 0.04f);
  n->attack = samples(0.003f);
  n->peak = peak;
  n->decay = samples(dur) - n->attack;
  n->mul = std::pow(kFloor / n->peak, 1.f / static_cast<float>(n->decay));
  n->glide = samples(glide_s);
  n->incf = incr_f(f0);
  n->fmul = std::pow(f1 / f0, 1.f / static_cast<float>(n->glide));
}

void HatchAPetScore::plop(float when, float vel, float hi) {
  glide(760.f * hi, 230.f * hi, 0.07f, when, 0.35f * vel, 0.14f);
}

void HatchAPetScore::woodblock(float freq, float when, float vel) {
  glide(freq, freq * 0.92f, 0.05f, when, 0.42f * vel, 0.12f);
  glide(freq * 2.76f, freq * 2.76f, 0.01f, when, 0.14f * vel, 0.04f);
}

void HatchAPetScore::chirp(float freq, float when, float vel) {
  glide(freq, freq * 1.5f, 0.08f, when, 0.25f * vel, 0.12f);
}

void HatchAPetScore::knock_knock(float when) {
  woodblock(degree_hz(0, -1), when, 0.8f);
  woodblock(degree_hz(2, -1), when + 0.22f, 0.9f);
}

void HatchAPetScore::react(int kind, int d) {
  if (kind == 0) {  // giggle
    for (int i = 0; i < 4; ++i) chirp(degree_hz(d + 4 - i, 0), static_cast<float>(i) * 0.07f, 0.55f);
  } else if (kind == 1) {  // jump + land
    glide(330.f, 660.f, 0.18f, 0, 0.3f, 0.25f);
    woodblock(degree_hz(0, -1), 0.42f, 0.5f);
  } else {  // a-a-choo
    const float f = degree_hz(d, 0);
    glide(f, f * 1.3f, 0.25f, 0, 0.18f, 0.3f);
    plop(0.35f, 1, 1.6f);
    marimba(degree_hz(d, 1), 0.37f, 0.5f, 0.3f);
  }
}

bool HatchAPetScore::cue(Cue c, uint8_t variant) {
  switch (c) {
    case Cue::AMBIENT: start_audio(); return true;
    case Cue::SUCCESS: {  // hatchSound()
      start_audio();
      plop(0, 1, 0.8f);
      static constexpr int8_t kDeg[] = {0, 2, 4, 5, 7};
      for (int i = 0; i < 5; ++i) marimba(degree_hz(kDeg[i], 0), 0.12f + static_cast<float>(i) * 0.09f, 0.8f, 0.55f);
      return true;
    }
    case Cue::REACTION: {
      start_audio();
      const int deg = variant & 0x0F;  // CREATURES[c].deg == c
      switch (variant & 0xF0) {
        case 0x00: react(0, deg); return true;
        case 0x10: react(1, deg); return true;
        case 0x20: react(2, deg); return true;
        case 0x30: {  // nameChirp
          const float f = degree_hz(deg, 0);
          chirp(f, 0, 0.9f);
          chirp(f * 1.25f, 0.14f, 1);
          return true;
        }
        case 0x40: {  // byeSound
          const float f = degree_hz(deg, 0);
          chirp(f * 1.25f, 0, 0.8f);
          chirp(f, 0.16f, 0.7f);
          return true;
        }
        default: return false;
      }
    }
    case Cue::NOTE: {
      start_audio();
      const int lo = variant & 0x0F;
      switch (variant & 0xF0) {
        case 0x10: woodblock(degree_hz(lo - 1, -1), 0, 0.9f); return true;  // knockSound(n)
        case 0x20: knock_knock(lo == 1 ? 0.2f : 0.f); return true;
        case 0x30:  // trill()
          for (int i = 0; i < 6; ++i) {
            marimba(degree_hz(i % 2 ? 7 : 5, 0), static_cast<float>(i) * 0.065f, 0.35f + static_cast<float>(i) * 0.08f,
                    0.18f);
          }
          return true;
        case 0x40:
          switch (lo) {
            case 0: woodblock(degree_hz(0, -1), 0, 0.5f); return true;
            case 1: plop(0, 0.6f, 0.6f); return true;
            case 2: plop(0, 0.8f, 1.2f); return true;
            case 3:  // choiceSound()
              marimba(degree_hz(2, 0), 0, 0.5f, 0.3f);
              marimba(degree_hz(4, 0), 0.1f, 0.5f, 0.3f);
              return true;
            case 4: marimba(degree_hz(0, 1), 0, 0.3f, 0.25f); return true;
            default: return false;
          }
        case 0x50: chirp(degree_hz(lo, 1), 0, 0.35f); return true;
        case 0x60: {  // birdSound()
          const float v = 0.1f + static_cast<float>(lo) / 15.f * 0.05f;
          glide(2600.f, 3400.f, 0.06f, 0, v, 0.09f);
          glide(2700.f, 3600.f, 0.06f, 0.13f, v, 0.09f);
          return true;
        }
        default: return false;
      }
    }
    default: return false;
  }
}

float HatchAPetScore::next_note(Note& n) {
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
  float s = sine_at(n.phase);
  if (n.kind == Kind::MARIMBA) {
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
    n.phase += static_cast<uint32_t>(n.incf);
    if (n.pos < n.glide) n.incf *= n.fmul;
  }
  if (++n.pos >= n.len) n.active = false;
  return g * s;
}

void HatchAPetScore::render_add(int32_t* out, std::size_t count) {
  if (!running_) {
    now_ += static_cast<uint32_t>(count);
    return;
  }
  // Music bus one-poles: fade-in (tau 1.5 s), duck down (0.03 s) and back (0.3 s).
  static const float fade_c = 1.f - std::exp(-1.f / (1.5f * kSr));
  static const float duck_dn = 1.f - std::exp(-1.f / (0.03f * kSr));
  static const float duck_up = 1.f - std::exp(-1.f / (0.3f * kSr));
  // Detector coefficients per compressor block (3 ms attack, 150 ms release).
  static const float att = 1.f - std::exp(-static_cast<float>(kCompBlock) / (0.003f * kSr));
  static const float rel = 1.f - std::exp(-static_cast<float>(kCompBlock) / (0.15f * kSr));
  static const float makeup = makeup_db();
  float buf[kCompBlock];
  for (std::size_t i0 = 0; i0 < count; i0 += kCompBlock) {
    const std::size_t len = count - i0 < kCompBlock ? count - i0 : kCompBlock;
    schedule_music(now_ + samples(0.4f));
    float peak = 0;
    for (std::size_t j = 0; j < len; ++j) {
      float x = 0;
      for (Note& n : notes_) {
        if (n.active && static_cast<int32_t>(now_ - n.start) >= 0) x += next_note(n);
      }
      float m = 0;
      for (MusicNote& n : music_) {
        if (n.active && static_cast<int32_t>(now_ - n.start) >= 0) m += next_music(n);
      }
      if (music_on_ && static_cast<int32_t>(now_ - music_t0_) >= 0) music_fade_ += (1.f - music_fade_) * fade_c;
      if (static_cast<int32_t>(now_ - duck_until_) < 0) {
        duck_ += (0.35f - duck_) * duck_dn;
      } else {
        duck_ += (1.f - duck_) * duck_up;
      }
      x += m * music_fade_ * duck_;
      master_ += (master_target_ - master_) * master_coef_;
      x *= master_;
      // The detector sees the input now; the gain lands on the 6 ms delayed
      // signal (Chromium's compressor pre-delay), so onsets are caught early.
      const float a = std::fabs(x);
      if (a > peak) peak = a;
      buf[j] = delay_[delay_pos_];
      delay_[delay_pos_] = x;
      if (++delay_pos_ >= kPreDelay) delay_pos_ = 0;
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
    for (MusicNote& n : music_) n.active = false;
    voice_n_ = 0;
    music_on_ = false;
    music_fade_ = 0;
    duck_ = 1;
    running_ = false;
    master_ = 0;
    env_db_ = 0;
    comp_gain_ = 1;
    for (float& d : delay_) d = 0;
    delay_pos_ = 0;
  }
}

}  // namespace little_arcade::audio
