// Host tests for the procedural audio path (synth, sound bank, AudioFeedback).
// Host only: proves determinism and digital safety limits, not acoustic level.
#include <cstdlib>
#include <vector>

#include "little_arcade/audio/animal_clips.hpp"
#include "little_arcade/audio/audio_feedback.hpp"
#include "little_arcade/audio/sound_bank.hpp"
#include "little_arcade/audio/synth.hpp"
#include "little_arcade/scene_manager.hpp"
#include "little_arcade/testing/test_support.hpp"

using namespace little_arcade;
using namespace little_arcade::audio;

namespace {

constexpr Cue kAllCues[] = {Cue::TAP,       Cue::PROMPT, Cue::SUCCESS,  Cue::TRY_AGAIN,
                            Cue::CELEBRATE, Cue::NOTE,   Cue::REACTION, Cue::AMBIENT};
constexpr uint8_t kScenes[] = {IFeedback::kSceneMenu, static_cast<uint8_t>(GameId::COLOR_POP),
                               static_cast<uint8_t>(GameId::HATCH_A_PET),
                               static_cast<uint8_t>(GameId::FEED_MONSTER),
                               static_cast<uint8_t>(GameId::ANIMAL_SOUNDS),
                               static_cast<uint8_t>(GameId::RAINBOW_PIANO),
                               static_cast<uint8_t>(GameId::ROBO_BUDDY),
                               static_cast<uint8_t>(GameId::SHAPE_GARAGE)};

std::size_t ms_samples(uint32_t ms) { return static_cast<std::size_t>(ms) * kSampleRateHz / 1000u; }

std::vector<int16_t> render(Synth& s, std::size_t n) {
  std::vector<int16_t> out(n);
  s.render(out.data(), n);
  return out;
}

int peak(const std::vector<int16_t>& v) {
  int m = 0;
  for (int16_t x : v) m = std::abs(static_cast<int>(x)) > m ? std::abs(static_cast<int>(x)) : m;
  return m;
}

Sound beep(uint16_t hz, uint16_t ms, uint8_t level = 200) {
  return Sound{}.add(Tone{Wave::SINE, hz, hz, ms, level, 0, 0});
}

void test_idle_synth_is_silent() {
  Synth s;
  EXPECT(peak(render(s, 1024)) == 0);
  EXPECT(s.active_voices() == 0);
}

void test_sound_plays_and_ends() {
  Synth s;
  s.trigger(beep(440, 100));
  EXPECT(s.active_voices() == 1);
  const auto body = render(s, ms_samples(100));
  EXPECT(peak(body) > 20000);  // 200/255 of full-scale sine
  const auto after = render(s, 64);
  EXPECT(peak(after) == 0 && s.active_voices() == 0);
}

void test_edges_ramp_so_no_clicks() {
  Synth s;
  s.trigger(beep(1000, 50));
  const auto v = render(s, ms_samples(50));
  EXPECT(v.front() == 0);
  EXPECT(std::abs(static_cast<int>(v.back())) < 400);
  // During the minimum 3 ms attack the envelope rises gradually.
  int max_step = 0;
  for (std::size_t i = 1; i < ms_samples(Synth::kMinEdgeMs); ++i) {
    const int d = std::abs(static_cast<int>(v[i]) - static_cast<int>(v[i - 1]));
    max_step = d > max_step ? d : max_step;
  }
  EXPECT(max_step < 12000);
}

void test_deterministic() {
  Synth a, b;
  const Sound snd = sound_for(static_cast<uint8_t>(GameId::HATCH_A_PET), Cue::NOTE, 3);  // includes noise
  a.trigger(snd);
  b.trigger(snd);
  EXPECT(render(a, 4000) == render(b, 4000));
}

void test_voice_stealing_caps_polyphony() {
  Synth s;
  for (int i = 0; i < 10; ++i) s.trigger(beep(static_cast<uint16_t>(300 + 50 * i), 500, 60));
  EXPECT(s.active_voices() == Synth::kVoices);
  s.stop_all();
  EXPECT(s.active_voices() == 0 && peak(render(s, 128)) == 0);
}

void test_rest_is_silent_and_glide_moves_pitch() {
  Synth s;
  s.trigger(Sound{}.add(Tone{Wave::SINE, 0, 0, 40, 200, 0, 0}).add(Tone{Wave::TRIANGLE, 200, 2000, 200, 200, 0, 0}));
  EXPECT(peak(render(s, ms_samples(40))) == 0);
  auto crossings = [](const std::vector<int16_t>& v) {
    int n = 0;
    for (std::size_t i = 1; i < v.size(); ++i) n += (v[i - 1] < 0) != (v[i] < 0) ? 1 : 0;
    return n;
  };
  const int early = crossings(render(s, ms_samples(50)));
  render(s, ms_samples(100));
  const int late = crossings(render(s, ms_samples(45)));
  EXPECT(late > early * 3);
}

void test_clip_plays_with_ramped_edges_then_tones() {
  std::vector<int16_t> square(ms_samples(60));
  for (std::size_t i = 0; i < square.size(); ++i) square[i] = (i / 8) % 2 ? 20000 : -20000;  // worst-case unfaded data
  const PcmClip clip{square.data(), static_cast<uint32_t>(square.size())};
  Synth s;
  s.trigger(Sound{}.with_clip(clip, 255).add(Tone{Wave::SINE, 440, 440, 40, 200, 0, 0}));
  EXPECT(s.active_voices() == 1);
  const auto body = render(s, square.size());
  EXPECT(body.front() == 0 && std::abs(static_cast<int>(body.back())) < 400);  // edges ramp, no clicks
  EXPECT(peak(body) > 19000 && peak(body) <= 20000);
  EXPECT(peak(render(s, ms_samples(40))) > 20000);  // the tone follows the clip
  render(s, 64);
  EXPECT(s.active_voices() == 0);

  Synth only;
  only.trigger(Sound{}.with_clip(clip, 128));
  EXPECT(peak(render(only, square.size())) <= 10100);  // clip_level scales the data
  EXPECT(peak(render(only, 64)) == 0 && only.active_voices() == 0);
  EXPECT(Sound{}.with_clip(PcmClip{}, 200).empty());
}

void test_animal_clips_are_safe_recordings() {
  for (uint8_t a = 0; a < kAnimalClipCount; ++a) {
    const PcmClip& c = animal_clip(a);
    EXPECT(c.samples != nullptr && c.count >= ms_samples(300) && c.count <= ms_samples(kMaxSoundMs));
    int top = 0;
    int64_t energy = 0;
    for (uint32_t i = 0; i < c.count; ++i) {
      const int v = std::abs(static_cast<int>(c.samples[i]));
      top = v > top ? v : top;
      energy += static_cast<int64_t>(v) * v;
    }
    EXPECT(top <= 30000);                                     // normalised below full scale
    EXPECT(energy / c.count > 1000 * 1000);                   // not silent (RMS > ~1000)
    EXPECT(std::abs(static_cast<int>(c.samples[0])) < 2000);  // faded in and out offline
    EXPECT(std::abs(static_cast<int>(c.samples[c.count - 1])) < 2000);
  }
}

void test_bank_covers_every_cue_safely() {
  for (uint8_t scene : kScenes) {
    for (Cue cue : kAllCues) {
      for (uint8_t variant = 0; variant < 12; ++variant) {
        const Sound s = sound_for(scene, cue, variant);
        const bool silent_ambient = cue == Cue::AMBIENT && scene != static_cast<uint8_t>(GameId::ROBO_BUDDY);
        EXPECT(s.empty() == silent_ambient);
        EXPECT(s.count <= Sound::kMaxTones);
        EXPECT(s.duration_ms() <= kMaxSoundMs);
        EXPECT(!s.has_clip() || s.clip_level <= kMaxToneLevel);
        for (uint8_t i = 0; i < s.count; ++i) {
          const Tone& t = s.tones[i];
          EXPECT(t.level <= kMaxToneLevel);
          EXPECT(t.f0_hz < kSampleRateHz / 2 && t.f1_hz < kSampleRateHz / 2);
          EXPECT(t.ms > 0);
        }
      }
    }
  }
}

void test_try_again_is_gentle() {
  for (uint8_t scene : kScenes) {
    const Sound s = sound_for(scene, Cue::TRY_AGAIN, 0);
    for (uint8_t i = 0; i < s.count; ++i) {
      EXPECT(s.tones[i].wave == Wave::SINE);  // never a harsh buzzer
      EXPECT(s.tones[i].f0_hz <= 600 && s.tones[i].level <= 130);
      EXPECT(s.tones[i].attack_ms >= 10);
    }
  }
}

bool same_sound(const Sound& a, const Sound& b) {
  if (a.count != b.count) return false;
  for (uint8_t i = 0; i < a.count; ++i) {
    const Tone &x = a.tones[i], &y = b.tones[i];
    if (x.wave != y.wave || x.f0_hz != y.f0_hz || x.f1_hz != y.f1_hz || x.ms != y.ms) return false;
  }
  return true;
}

void test_game_palettes_are_distinct() {
  const uint8_t pop = static_cast<uint8_t>(GameId::COLOR_POP);
  const uint8_t hatch = static_cast<uint8_t>(GameId::HATCH_A_PET);
  const uint8_t piano = static_cast<uint8_t>(GameId::RAINBOW_PIANO);
  const uint8_t feed = static_cast<uint8_t>(GameId::FEED_MONSTER);
  const uint8_t robo = static_cast<uint8_t>(GameId::ROBO_BUDDY);
  const uint8_t shapes = static_cast<uint8_t>(GameId::SHAPE_GARAGE);
  const uint8_t animals = static_cast<uint8_t>(GameId::ANIMAL_SOUNDS);
  for (uint8_t a = 0; a < 4; ++a) {
    for (uint8_t b = static_cast<uint8_t>(a + 1); b < 4; ++b) {
      EXPECT(!same_sound(sound_for(pop, Cue::PROMPT, a), sound_for(pop, Cue::PROMPT, b)));
    }
  }
  for (uint8_t a = 0; a < 8; ++a) {
    for (uint8_t b = static_cast<uint8_t>(a + 1); b < 8; ++b) {
      EXPECT(!same_sound(sound_for(hatch, Cue::REACTION, a), sound_for(hatch, Cue::REACTION, b)));
    }
  }
  // Crack "toks" rise in pitch toward hatching (variants 1..5).
  for (uint8_t c = 1; c < 5; ++c) {
    EXPECT(sound_for(hatch, Cue::NOTE, c).tones[1].f0_hz < sound_for(hatch, Cue::NOTE, static_cast<uint8_t>(c + 1)).tones[1].f0_hz);
  }
  EXPECT(!same_sound(sound_for(hatch, Cue::PROMPT, 0), sound_for(pop, Cue::PROMPT, 0)));
  for (uint8_t count = 1; count <= 3; ++count) {
    EXPECT(sound_for(feed, Cue::PROMPT, count).count > 0);
    EXPECT(sound_for(feed, Cue::SUCCESS, count).count > 0);
    EXPECT(sound_for(feed, Cue::PROMPT, count).duration_ms() <= kMaxSoundMs);
    EXPECT(sound_for(feed, Cue::SUCCESS, count).duration_ms() <= kMaxSoundMs);
    for (uint8_t other = static_cast<uint8_t>(count + 1); other <= 3; ++other) {
      EXPECT(!same_sound(sound_for(feed, Cue::PROMPT, count), sound_for(feed, Cue::PROMPT, other)));
    }
  }
  EXPECT(sound_for(feed, Cue::REACTION, 0).count > 0);
  EXPECT(sound_for(feed, Cue::CELEBRATE, 0).count > 0);
  constexpr uint16_t notes[] = {523, 587, 659, 784, 880};
  for (uint8_t key = 0; key < 5; ++key) {
    const Sound bells = sound_for(piano, Cue::NOTE, key);
    const Sound flute = sound_for(piano, Cue::NOTE, static_cast<uint8_t>(5 + key));
    const Sound drum = sound_for(piano, Cue::NOTE, static_cast<uint8_t>(10 + key));
    EXPECT(bells.tones[0].f0_hz == notes[key] && flute.tones[0].f0_hz == notes[key]);
    EXPECT(drum.tones[0].f0_hz == notes[key] / 4);
    EXPECT(!same_sound(bells, flute) && !same_sound(flute, drum));
  }
  for (Cue cue : {Cue::PROMPT, Cue::TAP, Cue::CELEBRATE}) {
    const Sound piano_sound = sound_for(piano, cue, 0);
    EXPECT(piano_sound.count > 0 && piano_sound.duration_ms() <= kMaxSoundMs);
  }
  EXPECT(!same_sound(sound_for(robo, Cue::PROMPT, 0), sound_for(robo, Cue::PROMPT, 1)));
  EXPECT(sound_for(robo, Cue::AMBIENT, 0).count > 0);
  for (uint8_t mood = 1; mood <= 5; ++mood) {
    const Sound reaction = sound_for(robo, Cue::REACTION, mood);
    EXPECT(reaction.count > 0);
    EXPECT(reaction.duration_ms() <= kMaxSoundMs);
    for (uint8_t other = static_cast<uint8_t>(mood + 1); other <= 5; ++other) {
      EXPECT(!same_sound(reaction, sound_for(robo, Cue::REACTION, other)));
    }
  }
  for (uint8_t prompt = 1; prompt <= 3; ++prompt) {
    EXPECT(sound_for(shapes, Cue::PROMPT, prompt).count > 0);
    EXPECT(sound_for(shapes, Cue::SUCCESS, static_cast<uint8_t>(prompt - 1)).count > 0);
    for (uint8_t other = static_cast<uint8_t>(prompt + 1); other <= 3; ++other) {
      EXPECT(!same_sound(sound_for(shapes, Cue::PROMPT, prompt), sound_for(shapes, Cue::PROMPT, other)));
    }
  }
  EXPECT(!same_sound(sound_for(shapes, Cue::PROMPT, 0), sound_for(shapes, Cue::PROMPT, 1)));
  EXPECT(sound_for(shapes, Cue::CELEBRATE, 0).duration_ms() < 600);
  EXPECT(sound_for(animals, Cue::PROMPT, 0).count > 0);
  for (uint8_t animal = 0; animal < 8; ++animal) {
    const Sound call = sound_for(animals, Cue::NOTE, animal);
    EXPECT(call.has_clip() && call.clip == &animal_clip(animal));
    EXPECT(call.duration_ms() >= 300 && call.duration_ms() <= kMaxSoundMs);
    for (uint8_t other = static_cast<uint8_t>(animal + 1); other < 8; ++other) {
      EXPECT(call.clip != sound_for(animals, Cue::NOTE, other).clip);
    }
  }
  EXPECT(scene_change_sound(pop, pop).count == 0);
  EXPECT(scene_change_sound(IFeedback::kSceneMenu, pop).count > 0);
  EXPECT(scene_change_sound(pop, IFeedback::kSceneMenu).count > 0);
}

std::vector<int16_t> pull(AudioFeedback& fb, std::size_t n) {
  std::vector<int16_t> out(n);
  fb.render(out.data(), n);
  return out;
}

void test_feedback_starts_muted() {
  AudioFeedback fb;
  fb.play(Cue::CELEBRATE, 0);
  EXPECT(peak(pull(fb, ms_samples(900))) == 0);  // no startup sound before a level is set
  EXPECT(fb.cues_played() == 1);
}

void test_feedback_plays_after_level_and_respects_limits() {
  AudioFeedback fb;
  fb.set_level(VolumeLimiter::kMaxLevel);  // asks for max; parent cap clamps it
  pull(fb, 256);
  EXPECT(fb.level() == VolumeLimiter::kDefaultParentCap);
  pull(fb, ms_samples(400));  // finish the ramp
  // Worst case: every voice busy with loud overlapping sounds.
  fb.set_scene(static_cast<uint8_t>(GameId::COLOR_POP));
  fb.play(Cue::SUCCESS, 3);
  fb.play(Cue::CELEBRATE, 0);
  fb.play(Cue::PROMPT, 3);
  fb.play(Cue::TAP, 0);
  const auto out = pull(fb, ms_samples(1200));
  EXPECT(peak(out) > 1000);
  EXPECT(peak(out) <= VolumeLimiter::kPeakLimit);
  EXPECT(fb.peak_since_reset() == peak(out));
}

void test_feedback_merges_repeats_and_drops_overflow() {
  AudioFeedback fb;
  fb.play(Cue::TAP, 0);
  fb.play(Cue::TAP, 0);
  fb.play(Cue::TAP, 1);
  pull(fb, 64);
  EXPECT(fb.cues_played() == 2 && fb.cues_merged() == 1);
  pull(fb, ms_samples(AudioFeedback::kRepeatGuardMs + 5));
  fb.play(Cue::TAP, 0);
  pull(fb, 64);
  EXPECT(fb.cues_played() == 3);

  AudioFeedback full;
  for (std::size_t i = 0; i < AudioFeedback::kQueueSize + 5; ++i) full.play(Cue::NOTE, static_cast<uint8_t>(i));
  EXPECT(full.cues_dropped() == 5);
  pull(full, 64);
  EXPECT(full.cues_played() == AudioFeedback::kQueueSize);
}

class SceneRecorder final : public IFeedback {
 public:
  void play(Cue, uint8_t) override {}
  void set_scene(uint8_t scene) override { scenes.push_back(scene); }
  std::vector<uint8_t> scenes;
};

class QuietGame final : public IGame {
 public:
  void enter(GameContext&) override {}
  void update(uint32_t) override {}
  void handle(const InputEvent&) override {}
  void exit() override {}
  uint32_t active_contexts() const override { return 0; }
};

void test_scene_manager_reports_scene_changes() {
  SceneRecorder rec;
  QuietGame game;
  const GameEntry entries[] = {{GameId::HATCH_A_PET, Intent::EGGS, &game}};
  SceneManager sm(entries, 1, rec);
  sm.handle(make_voice_event(Intent::EGGS, 0));
  sm.handle(make_voice_event(Intent::BACK, 200));
  EXPECT(rec.scenes.size() == 2);
  EXPECT(rec.scenes.size() == 2 && rec.scenes[0] == static_cast<uint8_t>(GameId::HATCH_A_PET));
  EXPECT(rec.scenes.size() == 2 && rec.scenes[1] == IFeedback::kSceneMenu);
}

}  // namespace

int main() {
  test_idle_synth_is_silent();
  test_sound_plays_and_ends();
  test_edges_ramp_so_no_clicks();
  test_deterministic();
  test_voice_stealing_caps_polyphony();
  test_rest_is_silent_and_glide_moves_pitch();
  test_clip_plays_with_ramped_edges_then_tones();
  test_animal_clips_are_safe_recordings();
  test_bank_covers_every_cue_safely();
  test_try_again_is_gentle();
  test_game_palettes_are_distinct();
  test_feedback_starts_muted();
  test_feedback_plays_after_level_and_respects_limits();
  test_feedback_merges_repeats_and_drops_overflow();
  test_scene_manager_reports_scene_changes();
  return testing::finish("audio_synth_test");
}
