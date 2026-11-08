#include "game/sound_jar.hpp"

#include <algorithm>
#include <cmath>

namespace sound_jar {
namespace {
constexpr double kPi = 3.141592653589793;
constexpr int kPent[5] = {0, 2, 4, 7, 9};
constexpr double kWiggle = 0.36, kIdleVisual = 6, kIdleSpoken = 12, kTapDebounce = 0.08;
constexpr double kSpringK = 380, kSpringC = 14, kHoldLoop = 0.35, kDeadZone = 12, kPxPerSemitone = 8, kMaxBend = 5;
constexpr double kLoud = 512.0 / 32768.0, kActive = 0.002;
constexpr uint32_t kSparkleColors[2] = {0xFFEB9C, 0xFFFBEF};

double clamp(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
double lerp(double a, double b, double t) { return a + (b - a) * t; }
bool in_rect(double x, double y, double rx, double ry, double rw, double rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}
int js_round(double v) { return static_cast<int>(std::floor(v + 0.5)); }
double sign(double v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }

// reference CYCLE: the fixed hint tour once the jar holds a new take.
struct CycleStep { Target kind; int index; };
constexpr CycleStep kCycle[9] = {{Target::Pad, 2}, {Target::Fx, kEcho},  {Target::Pad, 0},
                                 {Target::Fx, kWobble}, {Target::Pad, 4}, {Target::Fx, kRobot},
                                 {Target::Pad, 1}, {Target::Fx, kBack},  {Target::Pad, 3}};
}  // namespace

double degree_hz(int degree, int octave) {
  const int o = static_cast<int>(std::floor(degree / 5.0));
  const int i = ((degree % 5) + 5) % 5;
  return 587.33 * std::pow(2.0, (kPent[i] + 12.0 * (o + octave)) / 12.0);
}

Game::Game(Sound& sound, uint32_t seed) : sound_(sound), grng_(seed), frng_(seed ^ 0x9E3779B9u) {
  for (int i = 0; i < 5; ++i) pads_[i].blink = 1.5 + i * 0.9;
}

bool Game::fx_on(int i) const { return std::find(fx_.begin(), fx_.end(), i) != fx_.end(); }

bool Game::any_live_voice() const {
  for (const Voice& v : voices_)
    if (!v.rel) return true;
  return false;
}

bool Game::pad_looping(int i) const {
  for (const Voice& v : voices_)
    if (v.pad == i && v.loop && !v.rel) return true;
  return false;
}

void Game::cue(Cue c, int arg) {
  if (voice_ok()) sound_.cue(c, arg);
}

// ---------- particles, rings ----------
void Game::add_part(const Particle& p) {
  if (static_cast<int>(parts_.size()) >= kMaxParticles) parts_.erase(parts_.begin());
  parts_.push_back(p);
}

void Game::emit(double x, double y, int n, const EmitSpec& o) {
  n = std::max(1, js_round(n * (reduced_ ? 0.5 : 1)));
  for (int i = 0; i < n; ++i) {
    const double a = frng_.next() * kPi * 2;
    const double sp = lerp(o.sp0, o.sp1, frng_.next());
    const double life = lerp(o.l0, o.l1, frng_.next());
    Particle p{};
    p.x = x; p.y = y;
    p.vx = std::cos(a) * sp; p.vy = std::sin(a) * sp - o.up;
    p.g = o.gravity; p.life = life; p.max = life; p.shape = o.shape;
    p.color = o.colors[static_cast<int>(frng_.next() * o.ncolors)];
    p.size = o.size * lerp(0.7, 1.3, frng_.next());
    p.rot = frng_.next() * 6.28;
    p.vr = (frng_.next() - 0.5) * 8;
    p.additive = o.additive;
    add_part(p);
  }
}

void Game::fly_mist(int n, const uint32_t* colors) {
  n = std::max(1, js_round(n * (reduced_ ? 0.5 : 1)));
  for (int i = 0; i < n; ++i) {
    const double x = kJarX + (frng_.next() - 0.5) * 50;
    const double y = kJarY + 10 + (frng_.next() - 0.5) * 28;
    const double tx = kBinX + (frng_.next() - 0.5) * 18, ty = 108;
    const double T = 0.5, g = 240;
    Particle p{};
    p.x = x; p.y = y; p.vx = (tx - x) / T; p.vy = (ty - y) / T - g * T / 2; p.g = g;
    p.life = T; p.max = T; p.shape = Shape::Circle; p.color = colors[i % 2];
    p.size = 4 + frng_.next() * 3; p.rot = 0; p.vr = 0; p.additive = false;
    add_part(p);
  }
}

void Game::ring(double x, double y, double r0, double r1, double dur, uint32_t color, double w, double delay,
                double alpha) {
  if (static_cast<int>(rings_.size()) >= kMaxRings) rings_.erase(rings_.begin());
  rings_.push_back(Ring{x, y, r0, r1, -delay, dur, color, w, alpha});
}

// ---------- sampler voice mirrors ----------
Voice* Game::find_voice(uint32_t id) {
  if (id == 0) return nullptr;
  for (Voice& v : voices_)
    if (v.id == id) return &v;
  return nullptr;
}

uint32_t Game::start_voice(int pad, bool loop) {
  const int take = jar_live_ && jar_.sound != kFrog ? kLiveTake : jar_.sound;
  int live = 0;
  for (const Voice& w : voices_) live += w.rel ? 0 : 1;
  if (live >= kMaxLiveVoices) {  // steal the oldest live voice
    for (Voice& v : voices_)
      if (!v.rel) { release_voice(v, 0.02); break; }
  }
  Voice v{};
  v.id = next_voice_id_++;
  v.pad = pad; v.take = take; v.pos = 0; v.dur = take_data(take).dur; v.loop = loop;
  v.rate = std::pow(2.0, (pad >= 0 ? kPads[pad].semitones : 0) / 12.0);
  v.bend = 0; v.rel = false; v.fade = 1; v.rev = fx_on(kBack);
  voices_.push_back(v);
  if (fx_on(kEcho))
    for (int n = 1; n <= 3; ++n) ring(kJarX, kJarY, 30, 62, 0.5, kTeal.c[0], 2, 0.25 * n, 0.8 * std::pow(0.45, n - 1));
  if (started_) sound_.voice_start(v.id, take, v.rate, v.rev, loop);
  return v.id;
}

void Game::release_voice(Voice& v, double tau) {
  if (v.rel) return;
  v.rel = true;
  sound_.voice_release(v.id, tau);
}

void Game::stop_all_voices() {
  for (Voice& v : voices_) release_voice(v, 0.03);
}

void Game::bend_voice(Voice& v, double semis) {
  v.bend = semis;
  sound_.voice_bend(v.id, semis);
}

void Game::apply_fx() {
  if (started_) sound_.apply_fx(fx_on(kEcho), fx_on(kRobot), fx_on(kWobble));
}

double Game::voice_level(const Voice& v) const {
  const TakeData& tk = take_data(v.take);
  const double p = v.rev ? v.dur - v.pos : v.pos;
  const int f = static_cast<int>(std::floor(p / kLevelFrame));
  return (f >= 0 && f < tk.frames ? tk.play_lev[f] : 0) * v.fade;
}

// ---------- actions ----------
void Game::start_game() {
  started_ = true;
  sound_.start();
  apply_fx();
  sound_.start_music();
  music_on_ = true; music_i_ = 0; music_loop_ = 0;
  phase_ = Phase::Intro; t_ = 0;
}

int Game::next_take() {
  if (order_.empty()) {
    int b[3] = {1, 2, 3};
    for (int i = 2; i > 0; --i) {
      const int j = static_cast<int>(grng_.next() * (i + 1));
      std::swap(b[i], b[j]);
    }
    order_ = {kWhisper, b[0], b[1], b[2]};  // popped from the end
  }
  const int t = order_.back();
  order_.pop_back();
  return t;
}

void Game::pour_out(int mist, double level) {
  jar_.level_to = 0;
  jar_.has_next = true; jar_.next_mist = mist; jar_.next_level = level;
}

void Game::wipe_mic() {
  jar_live_ = false; live_pending_ = false;
  if (mic_) mic_->wipe();
}

void Game::start_recording() {
  stop_all_voices(); has_hold_ = false; has_pending_ = false;
  wipe_mic();  // design §5: a new recording wipes the previous clip
  rec_live_ = mic_live();
  take_ = next_take(); ++tries_;
  if (jar_.level > 0.04) {
    const uint32_t* c = kTakes[jar_.mist].hue.c;
    emit(kJarX, 78, 6, {8, 24, 0.6, 0.9, -30, Shape::Circle, 5, c, 2, 20, false});
  }
  jar_.sound = kFrog; pour_out(take_, 0); hush_ = false;
  lid_.to = 1;
  emit(kLidX, 62, 5, {40, 110, 0.35, 0.7, 60, Shape::Star, 5, kSparkleColors, 2, 0, true});
  ring(kRecX, kRecY, 14, 40, 0.45, kRed.c[0], 3);
  voiced_ = 0; last_act_ = -1; lev_ = 0; recb_.louder = 0; t_ = 0;
  if (rec_live_) {
    // Native, user-requested: a live take records from the tap. No countdown, no ribbon, and no lid
    // pop (capture mutes the speaker, which would chop it, and the microphone would hear it).
    jar_.mist = jar_.next_mist; jar_.has_next = false;
    phase_ = Phase::Recording;
    mic_->arm();
    return;
  }
  cue(Cue::LidPop);
  phase_ = Phase::Countdown;
}

void Game::finish_recording() {
  t_ = 0; lev_ = 0; lid_.to = 0;
  if (jar_.has_next) { jar_.mist = jar_.next_mist; jar_.has_next = false; }
  if (rec_live_) {
    voiced_ = mic_->voiced();
    mic_->stop();
    if (voiced_ < 0.15) { wipe_mic(); too_quiet_live(); return; }
    live_pending_ = true;  // finalized once the audio thread acknowledges the stop (see tick)
  }
  if (voiced_ < 0.15) { too_quiet(); return; }
  phase_ = Phase::Processing; jar_.sound = take_; jar_.level_to = 0.62;
}

void Game::too_quiet() {
  phase_ = Phase::TooQuiet; jar_.wig = 0.7; jar_.hmm = 1.6; recb_.louder = 1.4;
  cue(Cue::Hmm); pour_out(kFrog, 0.38);
}

void Game::too_quiet_live() {
  // Native, user-requested: after a silent live take the jar stays empty and quiet (no built-in frog)
  // and the helper hand keeps pointing at the microphone button until the next recording.
  phase_ = Phase::TooQuiet; t_ = 0; jar_.wig = 0.7; jar_.hmm = 1.6; recb_.louder = 1.4; lid_.to = 0;
  cue(Cue::Hmm); jar_.sound = kFrog; pour_out(kFrog, 0);
  hush_ = true; hint_ = 2; spoken_ = true; nudge_ = 0;
}

void Game::lid_closed() {
  if (phase_ != Phase::Processing) { cue(Cue::SoftShut); return; }
  const uint32_t* h = kTakes[jar_.sound].hue.c;
  cue(Cue::LidShut); jar_.qv = -3.5; ring(kLidX, kLidY, 10, 42, 0.4, kWhite, 2.5);
  const uint32_t c[2] = {h[0], 0xFFFBEF};
  emit(kLidX, kLidY - 4, 8, {40, 110, 0.35, 0.7, 60, Shape::Star, 5, c, 2, 0, true});
}

void Game::celebrate() {
  phase_ = Phase::Full; t_ = 0; jar_.happy = 1.5; jar_.qv = -2.5;
  const uint32_t* h = kTakes[jar_.sound].hue.c;
  const uint32_t c[4] = {h[0], h[1], 0xFFFBEF, kYellow.c[1]};
  emit(kJarX, 92, 36, {60, 140, 0.9, 1.5, 170, Shape::Rect, 6, c, 4, 70, false});
  for (int i = 0; i < 5; ++i) pads_[i].hop = -0.08 * i;
  cue(Cue::Tada); has_pending_ = true; pending_t_ = 0.45;
}

void Game::record_tap() {
  if (phase_ == Phase::Recording && t_ > 0.3) { finish_recording(); return; }
  if (!can_play()) { ring(kRecX, kRecY, 12, 36, 0.4, kWhite, 2); return; }
  start_recording();
}

void Game::toggle_fx(int i) {
  const FxDef& f = kFx[i];
  ++actions_;
  const auto k = std::find(fx_.begin(), fx_.end(), i);
  if (k != fx_.end()) {
    fx_.erase(k); cue(Cue::FxOff); ring(f.x, kFxY, 8, 26, 0.35, f.hue.c[0], 2);
  } else {
    if (fx_.size() >= 2) {  // max 2 at once: a 3rd turns off the oldest
      const int old = fx_.front();
      fx_.erase(fx_.begin());
      fxb_[old].qv = 2.5; ring(kFx[old].x, kFxY, 6, 24, 0.35, kWhite, 2); cue(Cue::FxBump);
    }
    fx_.push_back(i); cue(Cue::FxOn, i);
    const uint32_t c[2] = {f.hue.c[0], 0xFFFBEF};
    emit(f.x, kFxY, 6, {40, 110, 0.35, 0.7, 60, Shape::Star, 5, c, 2, 0, true});
    ring(f.x, kFxY, 10, 30, 0.4, f.hue.c[0], 3);
    if (can_play()) { has_pending_ = true; pending_t_ = 0.18; }
  }
  jar_.visor_to = fx_on(kRobot) ? 1 : 0;
  apply_fx();
}

void Game::bin_tap() {
  bin_.lid = 1; cue(Cue::Whoosh); ring(kBinX, 110, 8, 36, 0.45, kWhite, 2);
  stop_all_voices(); has_hold_ = false; has_pending_ = false;
  wipe_mic();  // emptying the jar is the explicit wipe (also aborts a live recording)
  if (jar_.level > 0.04) fly_mist(10, kTakes[jar_.mist].hue.c);
  if (!can_play()) {  // abort a recording
    lid_.to = 0;
    if (jar_.has_next) { jar_.mist = jar_.next_mist; jar_.has_next = false; }
  }
  jar_.sound = kFrog; pour_out(kFrog, 0.38); jar_.qv = -2; hush_ = false;
  phase_ = Phase::Empty; t_ = 0;
}

void Game::jar_tap() {
  ring(kJarX, kJarY, 16, 56, 0.45, kWhite, 2);
  if (!can_play()) return;
  if (hush_) { jar_.wig = 0.5; return; }  // empty after a silent take: a wobble, no voice
  start_voice(-1, false);
  emit(kJarX, 80, 4, {40, 110, 0.35, 0.7, 60, Shape::Star, 5, kSparkleColors, 2, 0, true});
}

void Game::pad_down(int i, double y, int pointer) {
  if (has_hold_) release_hold();
  has_hold_ = true;
  hold_ = Hold{i, pointer, y, 0, 0, 0, false};
  const PadDef& P = kPads[i];
  const double x = pad_x(i), py = pad_y(i);
  if (!can_play() || hush_) { ring(x, py, 8, 30, 0.4, kWhite, 2); return; }
  ++actions_;
  hold_.voice = start_voice(i, false);
  pads_[i].glow = 1;
  const uint32_t c[2] = {P.hue.c[0], 0xFFFBEF};
  emit(x, py - 20 * P.s, 3, {10, 30, 0.6, 0.9, -40, Shape::Note, 4, c, 2, 45, false});
  ring(x, py, 10, 36, 0.4, P.hue.c[0], 2.5);
}

void Game::release_hold() {
  has_hold_ = false;
  Voice* v = find_voice(hold_.voice);
  if (v && v->loop) release_voice(*v, 0.05);  // loops stop on release; one-shots play out
}

void Game::preview() {
  if (!can_play() || hush_ || has_hold_ || any_live_voice()) return;
  start_voice(-1, false);
  jar_.qv = -1.5;
}

// ---------- input ----------
bool Game::press(Body& o) {
  if (time_ - o.last_tap < kTapDebounce) return false;
  o.last_tap = time_; o.qv = -3.2; pressed_ = &o;
  return true;
}

void Game::down(double x, double y, int pointer) {
  jar_.look_x = x - kJarX; jar_.look_y = y - 112;
  if (phase_ == Phase::Start) { start_game(); return; }
  if (in_rect(x, y, 0, 0, 56, 56)) { home(); return; }
  if (in_rect(x, y, 264, 0, 56, 56)) { ring(292, 28, 6, 24, 0.4, kWhite, 2); return; }  // mic: disabled here
  if (phase_ == Phase::Intro) { t_ = 99; return; }
  idle_ = 0; hint_ = 0; spoken_ = false;
  for (int i = 0; i < 4; ++i)
    if (in_rect(x, y, kFx[i].x - 24, 6, 48, 48)) { if (press(fxb_[i])) toggle_fx(i); return; }
  if (in_rect(x, y, 232, 96, 72, 72)) { if (press(recb_)) record_tap(); return; }
  if (in_rect(x, y, 16, 96, 56, 56)) { if (press(bin_)) bin_tap(); return; }
  if (in_rect(x, y, 112, 60, 96, 96)) { if (press(jar_)) jar_tap(); return; }
  for (int i = 0; i < 5; ++i)
    if (in_rect(x, y, 64 * i, 168, 64, 72)) { if (press(pads_[i])) pad_down(i, y, pointer); return; }
  ring(x, y, 3, 22, 0.5, kWhite, 2);
  emit(x, y, 4, {40, 110, 0.35, 0.7, 60, Shape::Star, 5, kSparkleColors, 2, 0, true});
  cue(Cue::TapNote);
}

void Game::move(double x, double y, int pointer) {
  (void)x;
  if (!has_hold_ || pointer != hold_.pointer) return;
  const double dy = hold_.y0 - y, a = std::abs(dy);  // slide up = higher
  const double bend = a <= kDeadZone ? 0 : sign(dy) * std::min(kMaxBend, (a - kDeadZone) / kPxPerSemitone);
  if (bend != hold_.bend) {
    hold_.bend = bend;
    Voice* v = find_voice(hold_.voice);
    if (v && !v->rel) bend_voice(*v, bend);
  }
}

void Game::up(int pointer) {
  if (pressed_) { pressed_->qv = std::max(0.0, pressed_->qv) + 2.2; pressed_ = nullptr; }
  if (has_hold_ && pointer == hold_.pointer) release_hold();
}

void Game::intent(little_arcade::Intent in) {
  using little_arcade::Intent;
  switch (in) {
    case Intent::START:
      if (phase_ == Phase::Start) start_game();
      break;
    case Intent::BACK:
      home();
      break;
    case Intent::AGAIN:  // the same action as tapping the jar (it plays its sound)
      if (phase_ == Phase::Start || phase_ == Phase::Intro) break;
      down(kJarX, kJarY, 99);
      up(99);
      break;
    case Intent::HELP:  // the 12 s hint path: the jar hums and the hand points at the next thing
      if (!can_play()) break;
      idle_ = std::max(idle_, kIdleSpoken);
      spoken_ = true; hint_ = 2; nudge_ = 0;
      cue(Cue::Hint); jar_.hum = 0.9; jar_.qv = -2.2;
      break;
    default:
      break;
  }
}

void Game::home() {
  if (music_on_) { sound_.stop_music(); music_on_ = false; }
  stop_all_voices(); voices_.clear();
  wipe_mic(); rec_live_ = false; hush_ = false;  // Home = exit: wipe the clip
  has_hold_ = false; pressed_ = nullptr; has_pending_ = false; parts_.clear(); rings_.clear();
  phase_ = Phase::Start; t_ = 0; idle_ = 0; hint_ = 0; spoken_ = false; actions_ = 0; tries_ = 0;
  order_.clear(); take_ = -1; fx_.clear(); amb_ = 0; voiced_ = 0; last_act_ = -1; lev_ = 0;
  jar_.sound = kFrog; jar_.mist = kFrog; jar_.level = 0.38; jar_.level_to = 0.38; jar_.has_next = false;
  jar_.q = 0; jar_.qv = 0; jar_.wig = 0; jar_.happy = 0; jar_.hmm = 0; jar_.visor = 0; jar_.visor_to = 0; jar_.hum = 0;
  lid_.u = 0; lid_.to = 0; recb_.louder = 0; bin_.lid = 0;
  for (Pad& p : pads_) { p.awake = false; p.hop = 9; p.glow = 0; }
  apply_fx();
}

// ---------- update ----------
void Game::spring(Body& o, double dt) {
  for (int i = 0; i < 2; ++i) {
    const double h = dt / 2, a = -kSpringK * o.q - kSpringC * o.qv;
    o.qv += a * h; o.q += o.qv * h;
  }
}

Suggestion Game::suggest() const {
  if (phase_ == Phase::Empty || phase_ == Phase::TooQuiet) return {Target::Record, 0, kRecX, kRecY, kRecR + 5};
  if (phase_ != Phase::Full) return {Target::None, 0, 0, 0, 0};
  const CycleStep& c = kCycle[actions_ % 9];
  if (c.kind == Target::Pad) return {Target::Pad, c.index, pad_x(c.index), pad_y(c.index), kPadR * kPads[c.index].s + 6};
  return {Target::Fx, c.index, kFx[c.index].x, kFxY, kFxR + 4};
}

bool Game::auto_target(double& x, double& y) const {
  if (phase_ == Phase::Start) { x = 160; y = 120; return true; }
  const Suggestion s = suggest();
  if (s.kind == Target::None) return false;
  x = s.x; y = s.y;
  return true;
}

void Game::music_tick() {
  for (;;) {
    const Drip& e = kDrips[music_i_];
    const double T = music_loop_ * kAmbLoop + e.t;
    if (T > amb_ + kAmbAhead) return;
    if (amb_ - T < 0.1 && phase_ != Phase::Recording) sound_.drip(std::max(0.0, T - amb_), e.degree);
    if (++music_i_ >= 6) { music_i_ = 0; ++music_loop_; }
  }
}

void Game::advance(double elapsed) {
  double left = std::min(elapsed, kMaxCatchUp);
  while (left > 1e-9) {
    const double step = std::min(kMaxStep, left);
    tick(step);
    left -= step;
  }
}

void Game::tick(double dt) {
  time_ += dt; t_ += dt;
  const double m = reduced_ ? 0.3 : 1;
  if (phase_ != Phase::Start) amb_ += dt;
  if (phase_ == Phase::Intro) {  // the jar wakes, then the pads one by one
    const bool skip = t_ > 50;
    if (t_ >= 0.3 && t_ - dt < 0.3) { jar_.qv = -3; cue(Cue::Wake); }
    for (int i = 0; i < 5; ++i) {
      Pad& p = pads_[i];
      if (!p.awake && t_ >= 0.35 + 0.1 * i) { p.awake = true; p.hop = 0; if (!skip) cue(Cue::PadNote, i); }
    }
    if (t_ >= 1.3) { phase_ = Phase::Empty; t_ = 0; }
  } else if (phase_ == Phase::Countdown) {
    if (t_ >= 1.35) {
      phase_ = Phase::Recording; t_ = 0;
      if (jar_.has_next) { jar_.mist = jar_.next_mist; jar_.has_next = false; }
      if (rec_live_) mic_->arm();
    }
  } else if (phase_ == Phase::Recording) {
    if (rec_live_) {  // the microphone's level; its end rule runs in captured (wall-clock) time
      lev_ = mic_->level();
      voiced_ = mic_->voiced();
      jar_.level_to = std::min(0.66, 0.06 + voiced_ * 0.9);
      if (mic_->should_stop()) finish_recording();
    } else {
      const TakeData& tk = kTakeData[take_];
      const int f = static_cast<int>(std::floor((t_ - 0.3) / kLevelFrame));
      lev_ = f >= 0 && f < tk.frames ? tk.lev[f] : 0;  // the built-in take arrives as the input level
      if (lev_ >= kActive) last_act_ = t_;
      if (lev_ >= kLoud) voiced_ += dt;
      jar_.level_to = std::min(0.66, 0.06 + voiced_ * 0.9);
      if ((last_act_ >= 0 && t_ - last_act_ >= 0.5) || t_ >= 3) finish_recording();
    }
  } else if (phase_ == Phase::Processing) {
    if (live_pending_ && mic_->state() == LiveTake::State::Captured) {
      jar_live_ = mic_->finalize();
      live_pending_ = false;
      if (!jar_live_) { too_quiet_live(); return; }  // nothing above the room's noise survived the trim
    }
    // A live take waits for the audio thread (one 16 ms block); should it never answer, the built-in take plays.
    if (t_ >= 0.5 && (!live_pending_ || t_ >= 1.5)) { live_pending_ = false; celebrate(); }
  } else if (phase_ == Phase::TooQuiet) {
    if (t_ >= 1.4) { phase_ = Phase::Empty; t_ = 0; }
  }

  // idle hints: 6 s the next thing glows and wiggles; 12 s the jar hums and the helper hand shows it
  if (can_play() && !has_hold_ && !any_live_voice()) {
    idle_ += dt;
    if (idle_ > kIdleVisual && hint_ < 1) { hint_ = 1; nudge_ = 0; }
    if (idle_ > kIdleSpoken && !spoken_) { spoken_ = true; hint_ = 2; cue(Cue::Hint); jar_.hum = 0.9; jar_.qv = -2.2; }
  }
  if (hush_ && can_play()) { hint_ = 2; spoken_ = true; }  // keep pointing at the microphone
  if (hint_ >= 1 && (nudge_ -= dt) <= 0) {
    nudge_ = 1.6;
    const Suggestion s = suggest();
    if (s.kind == Target::Record) recb_.wig = kWiggle;
    else if (s.kind == Target::Pad) pads_[s.index].wig = kWiggle;
    else if (s.kind == Target::Fx) fxb_[s.index].wig = kWiggle;
  }

  for (Voice& v : voices_) {
    v.pos += dt * v.rate * std::pow(2.0, v.bend / 12);
    if (v.loop && v.pos >= v.dur) v.pos = std::fmod(v.pos, v.dur);
    if (v.rel) v.fade -= dt / 0.15;
  }
  voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
                               [](const Voice& v) { return !((v.loop || v.pos < v.dur) && v.fade > 0); }),
                voices_.end());
  if (has_hold_) {
    hold_.t += dt;
    if (!hold_.looped && hold_.t >= kHoldLoop && can_play()) {  // hold > 350 ms: the sound loops
      hold_.looped = true;
      Voice* v = find_voice(hold_.voice);
      if (v && !v->rel && v->pos < v->dur - 0.03) {
        v->loop = true;
        sound_.voice_loop(v->id);
      } else {
        hold_.voice = start_voice(hold_.pad, true);
        if (Voice* nv = find_voice(hold_.voice)) bend_voice(*nv, hold_.bend);
      }
      ring(pad_x(hold_.pad), pad_y(hold_.pad), 12, 40, 0.5, kPads[hold_.pad].hue.c[0], 2.5);
    }
  }
  bool looping = false;
  for (const Voice& v : voices_) looping = looping || (v.loop && !v.rel);
  if (started_ && looping && (duck_t_ -= dt) <= 0) { duck_t_ = 0.4; sound_.duck(0.6); }
  if (has_pending_ && (pending_t_ -= dt) <= 0) { has_pending_ = false; preview(); }

  // jar: mist level (drains first when a new colour waits), mouth from the voices, face timers, sway
  jar_.level += (jar_.level_to - jar_.level) * std::min(1.0, dt * (jar_.has_next ? 6 : 4));
  if (jar_.has_next && jar_.level < 0.02) {
    jar_.mist = jar_.next_mist; jar_.level_to = jar_.next_level; jar_.has_next = false;
  }
  double lev = 0;
  for (Pad& p : pads_) p.sing = 0;
  for (const Voice& v : voices_) {
    const double l = voice_level(v);
    lev = std::max(lev, l);
    if (v.pad >= 0) pads_[v.pad].sing = std::max(pads_[v.pad].sing, l);
  }
  jar_.mouth += (clamp(lev / 0.15, 0, 1) - jar_.mouth) * std::min(1.0, dt * 25);
  const double rl = phase_ == Phase::Recording ? clamp(lev_ / 0.12, 0, 1) : 0;
  jar_.swirl += dt * (0.8 + 3 * std::max(rl, jar_.mouth)) * (fx_on(kBack) ? -1 : 1) * m;
  jar_.wob_a += ((fx_on(kWobble) ? 1 : 0) - jar_.wob_a) * std::min(1.0, dt * 4);
  jar_.visor += (jar_.visor_to - jar_.visor) * std::min(1.0, dt * 8);
  jar_.wig = std::max(0.0, jar_.wig - dt); jar_.happy = std::max(0.0, jar_.happy - dt);
  jar_.hmm = std::max(0.0, jar_.hmm - dt); jar_.hum = std::max(0.0, jar_.hum - dt);
  const double wk = 1 - jar_.wig / 0.7;
  jar_.ang = ((jar_.wig > 0 ? std::sin(wk * kPi * 4) * 0.1 * (1 - wk) : 0) +
              std::sin(time_ * 2 * kPi * 1.2) * 0.035 * jar_.wob_a) * m;
  spring(jar_, dt);
  jar_.blink -= dt;
  if (jar_.blink < -0.12) jar_.blink = 2 + frng_.next() * 4;
  const double lu = lid_.u;  // lid hop: 0.4 s open, 0.3 s shut
  lid_.u = lid_.to > lid_.u ? std::min(lid_.to, lid_.u + dt / 0.4) : std::max(lid_.to, lid_.u - dt / 0.3);
  if (lu > 0 && lid_.u == 0) lid_closed();

  for (int i = 0; i < 5; ++i) {
    Pad& p = pads_[i];
    spring(p, dt);
    p.bend += ((has_hold_ && hold_.pad == i ? hold_.bend : 0) - p.bend) * std::min(1.0, dt * 14);
    p.glow = std::max(p.sing > 0.005 ? 0.55 + 0.45 * clamp(p.sing / 0.15, 0, 1) : 0, p.glow - dt * 2.5);
    p.hop += dt; p.wig = std::max(0.0, p.wig - dt);
    p.blink -= dt;
    if (p.blink < -0.12) p.blink = 2.4 + frng_.next() * 3.5;
  }
  for (int i = 0; i < 4; ++i) {
    FxButton& b = fxb_[i];
    spring(b, dt);
    b.on += ((fx_on(i) ? 1 : 0) - b.on) * std::min(1.0, dt * 10);
    b.wig = std::max(0.0, b.wig - dt);
  }
  spring(recb_, dt); recb_.louder = std::max(0.0, recb_.louder - dt); recb_.wig = std::max(0.0, recb_.wig - dt);
  spring(bin_, dt); bin_.lid = std::max(0.0, bin_.lid - dt * 1.6);

  for (Particle& p : parts_) {
    p.life -= dt; p.vy += p.g * dt; p.x += p.vx * dt; p.y += p.vy * dt; p.rot += p.vr * dt;
  }
  parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Particle& p) { return p.life <= 0; }),
               parts_.end());
  for (Ring& r : rings_) r.t += dt;
  rings_.erase(std::remove_if(rings_.begin(), rings_.end(), [](const Ring& r) { return r.t >= r.dur; }), rings_.end());

  if (music_on_) music_tick();
}

}  // namespace sound_jar
