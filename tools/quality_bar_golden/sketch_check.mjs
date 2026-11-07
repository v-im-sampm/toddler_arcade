// Checks a v2/reference sketch against the rules its device port relies on
// (v2/docs/06 §4.5, firmware ui/ctx2d.hpp, v2/AGENTS.md). A diagnostic aid for
// sketch authors: it does not replace owner review on the panel.
//
//   node sketch_check.mjs <game | path\to\sketch.html> [--seconds 40] [--seed 1234] [--out dir]
//
// Static: no microphone, network, storage or external resources; no audio
// nodes the device scores cannot port (convolver, wave shaper, script/worklet
// DSP, 3D panner, media streams, speech synthesis); the shared entry points
// (drawScene, update, startGame, home, initAudio, drawStart, drawHud) exist.
// Runtime, in headless Edge with the sketch's own rAF loop and Web Audio: plays
// a seeded session of taps and drags (half of them on autoTarget() when the
// sketch defines that hook), an idle gap for the hint ladder, the D/R/P keys
// and Home. Fails on any console error or uncaught exception, and on canvas use
// outside drawHud() that ui/ctx2d cannot reproduce: clip, drawImage, text,
// shadows, filters, line dashes, patterns, image data, rect/roundRect/
// strokeRect/clearRect, lineCap other than butt/round, composite other than
// source-over/lighter, gradients with more than 4 stops, paths with more than
// 96 subpaths, more than 16 gradients or 640 fills/strokes in one frame, or
// more than 96 live particles under the device cap. A Web Audio census (node
// types, oscillator waveforms, filter types, buffer rates, peak live sources)
// lands in report.json; it fails on anything but one AudioContext, on
// non-sine oscillators (the ported scores share one sine table,
// firmware audio/score_dsp.hpp) and on oscillators at 7.8 kHz or more (the
// device runs at 16 kHz).
// Screenshots and contact.png (all shots at 1:1 logical size) go to <out>/<game>/.
import { chromium } from 'playwright-core';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..', '..');
const argv = process.argv.slice(2);
function opt(name, def) {
  const i = argv.indexOf(name);
  if (i < 0) return def;
  const v = argv[i + 1];
  argv.splice(i, 2);
  return v;
}
const seconds = Number(opt('--seconds', 40));
const seed = Number(opt('--seed', 1234));
const outRoot = path.resolve(opt('--out', path.join(here, 'out_check')));
const target = argv[0];
if (!target) {
  console.error('usage: node sketch_check.mjs <game | sketch.html> [--seconds N] [--seed N] [--out dir]');
  process.exit(2);
}
const file = target.endsWith('.html') ? path.resolve(target)
  : path.join(root, 'v2', 'reference', target === 'color_pop' ? 'quality_bar.html' : `${target}.html`);
const game = path.basename(file, '.html');
const out = path.join(outRoot, game);
const problems = [];
const LIMITS = { gradients: 16, items: 640, particles: 96 };

// ---------- static ----------
const src = fs.readFileSync(file, 'utf8');
const STATIC = [
  [/getUserMedia|mediaDevices|MediaRecorder|SpeechRecognition/, 'microphone / speech API'],
  [/\bfetch\s*\(|XMLHttpRequest|WebSocket|EventSource|sendBeacon|navigator\.(geolocation|bluetooth|usb)/, 'network or device API'],
  [/localStorage|sessionStorage|indexedDB|document\.cookie|\bcaches\./, 'persistent storage'],
  [/https?:\/\//, 'external URL'],
  [/<(img|link|iframe|audio|video|object|embed|source)\b|<script[^>]*\bsrc\s*=|url\(/i, 'external resource'],
  [/\bimport\s*\(|\bnew\s+Worker\b|\beval\s*\(|new\s+Function\b/, 'dynamic code'],
  [/create(Convolver|WaveShaper|ScriptProcessor|Panner|StereoPanner|MediaStreamSource|MediaElementSource|MediaStreamDestination)\b|audioWorklet|AudioWorkletNode|speechSynthesis|SpeechSynthesisUtterance/,
    'audio API the device scores cannot port'],
  [/exponentialRampToValueAtTime\(\s*0\s*[,)]/, 'exponential ramp to 0 (throws; use 0.0001)'],
];
src.split(/\r?\n/).forEach((line, i) => {
  for (const [re, what] of STATIC) {
    if (re.test(line)) problems.push(`static: ${what} at line ${i + 1}: ${line.trim().slice(0, 100)}`);
  }
});
for (const fn of ['drawScene', 'update', 'startGame', 'home', 'initAudio', 'drawHud', 'drawStart']) {
  if (!new RegExp(`function ${fn}\\s*\\(`).test(src)) problems.push(`static: missing top-level function ${fn}()`);
}

// ---------- runtime ----------
fs.rmSync(out, { recursive: true, force: true });
fs.mkdirSync(out, { recursive: true });
const browser = await chromium.launch({
  channel: 'msedge', headless: true, args: ['--autoplay-policy=no-user-gesture-required'],
});
const page = await browser.newPage({ viewport: { width: 667, height: 560 }, deviceScaleFactor: 1 });
await page.emulateMedia({ reducedMotion: 'no-preference' });
const errors = [];
const warnings = [];
page.on('console', m => {
  if (m.type() === 'error') errors.push(m.text());
  else if (m.type() === 'warning') warnings.push(m.text());
});
page.on('pageerror', e => errors.push(String(e.stack || e)));
page.on('request', r => {
  if (!/^(file|data|blob):/.test(r.url())) problems.push(`network request: ${r.url()}`);
});
await page.addInitScript(() => {
  const S = window.__sk = {
    hud: 0, frames: 0, grads: 0, items: 0, maxGrads: 0, maxItems: 0, gradHist: {}, viol: {},
    maxParts: 0, sceneMs: [], subpaths: 0,
  };
  const bad = what => { if (!S.hud) S.viol[what] = (S.viol[what] || 0) + 1; };
  const P = CanvasRenderingContext2D.prototype;
  for (const m of ['clip', 'drawImage', 'fillText', 'strokeText', 'measureText', 'setLineDash', 'createPattern',
    'getImageData', 'putImageData', 'createImageData', 'strokeRect', 'clearRect', 'rect', 'roundRect',
    'isPointInPath', 'isPointInStroke', 'createConicGradient', 'drawFocusIfNeeded', 'reset']) {
    const f = P[m];
    if (typeof f !== 'function') continue;
    P[m] = function (...a) { bad(`${m}()`); return f.apply(this, a); };
  }
  const wrap = (m, pre) => {
    const f = P[m];
    P[m] = function (...a) { pre(); return f.apply(this, a); };
  };
  const draw = () => {
    if (S.hud) return;
    S.items++;
    if (S.subpaths > 96) bad('path with >96 subpaths');
  };
  wrap('createLinearGradient', () => { if (!S.hud) S.grads++; });
  wrap('createRadialGradient', () => { if (!S.hud) S.grads++; });
  wrap('fill', draw);
  wrap('stroke', draw);
  wrap('fillRect', () => { if (!S.hud) S.items++; });
  wrap('beginPath', () => { S.subpaths = 0; });
  wrap('moveTo', () => { S.subpaths++; });
  const stops = new WeakMap();
  const add = CanvasGradient.prototype.addColorStop;
  CanvasGradient.prototype.addColorStop = function (o, c) {
    const n = (stops.get(this) || 0) + 1;
    stops.set(this, n);
    if (n === 5) bad('gradient with >4 stops');
    return add.call(this, o, c);
  };
  const guard = (prop, ok) => {
    const d = Object.getOwnPropertyDescriptor(P, prop);
    Object.defineProperty(P, prop, {
      ...d, set(v) { if (!ok(v)) bad(`${prop} = ${String(v)}`); d.set.call(this, v); },
    });
  };
  guard('lineCap', v => v === 'butt' || v === 'round');
  guard('globalCompositeOperation', v => v === 'source-over' || v === 'lighter');
  guard('shadowBlur', v => !v);
  guard('shadowOffsetX', v => !v);
  guard('shadowOffsetY', v => !v);
  guard('filter', v => v === 'none');
  guard('fillStyle', v => !(v instanceof CanvasPattern));
  guard('strokeStyle', v => !(v instanceof CanvasPattern));

  // Web Audio census (reported, partly gated after the session).
  const A = S.audio = {
    contexts: 0, offline: 0, creates: {}, oscTypes: {}, filterTypes: {}, bufferRates: {}, paramMods: 0,
    live: 0, maxLive: 0, maxOscHz: 0,
  };
  const bump = (o, k) => { o[k] = (o[k] || 0) + 1; };
  const ctxs = new WeakSet();
  const oscFreq = new WeakSet();
  const hz = (p, v) => { if (oscFreq.has(p) && Number.isFinite(v)) A.maxOscHz = Math.max(A.maxOscHz, Math.abs(v)); };
  for (const C of [window.BaseAudioContext, window.AudioContext]) {
    if (!C) continue;
    for (const m of Object.getOwnPropertyNames(C.prototype)) {
      const d = Object.getOwnPropertyDescriptor(C.prototype, m);
      if (!m.startsWith('create') || typeof d.value !== 'function') continue;
      const f = d.value;
      C.prototype[m] = function (...a) {
        if (!ctxs.has(this)) {
          ctxs.add(this);
          // Offline rendering of built-in samples is allowed; only live contexts are gated.
          if (window.OfflineAudioContext && this instanceof OfflineAudioContext) A.offline++; else A.contexts++;
        }
        bump(A.creates, m.slice(6));
        const n = f.apply(this, a);
        if (m === 'createOscillator') oscFreq.add(n.frequency);
        if (m === 'createBuffer') bump(A.bufferRates, String(a[2]));
        return n;
      };
    }
  }
  // AudioBufferSourceNode redeclares start(), so wrap every own start().
  for (const C of [window.AudioScheduledSourceNode, window.OscillatorNode, window.AudioBufferSourceNode,
    window.ConstantSourceNode]) {
    if (!C || !Object.prototype.hasOwnProperty.call(C.prototype, 'start')) continue;
    const start = C.prototype.start;
    C.prototype.start = function (...a) {
      const r = start.apply(this, a);
      if (this instanceof OscillatorNode) bump(A.oscTypes, this.type);
      A.live++;
      A.maxLive = Math.max(A.maxLive, A.live);
      this.addEventListener('ended', () => { A.live--; }, { once: true });
      return r;
    };
  }
  if (window.AudioNode) {
    const connect = AudioNode.prototype.connect;
    AudioNode.prototype.connect = function (dest, ...a) {
      if (dest instanceof AudioParam) A.paramMods++;
      if (this instanceof BiquadFilterNode) bump(A.filterTypes, this.type);
      return connect.call(this, dest, ...a);
    };
  }
  if (window.AudioParam) {
    const AP = AudioParam.prototype;
    for (const m of ['setValueAtTime', 'linearRampToValueAtTime', 'exponentialRampToValueAtTime', 'setTargetAtTime']) {
      const f = AP[m];
      AP[m] = function (v, ...a) { hz(this, v); return f.call(this, v, ...a); };
    }
    const curve = AP.setValueCurveAtTime;
    AP.setValueCurveAtTime = function (vals, ...a) { for (const v of vals) hz(this, v); return curve.call(this, vals, ...a); };
    const d = Object.getOwnPropertyDescriptor(AP, 'value');
    Object.defineProperty(AP, 'value', { ...d, set(v) { hz(this, v); d.set.call(this, v); } });
  }

  const raf = window.requestAnimationFrame.bind(window);
  window.requestAnimationFrame = cb => raf(t => {
    S.grads = 0; S.items = 0;
    cb(t);
    S.frames++;
    S.maxGrads = Math.max(S.maxGrads, S.grads);
    S.maxItems = Math.max(S.maxItems, S.items);
    S.gradHist[S.grads] = (S.gradHist[S.grads] || 0) + 1;
    try {
      // eslint-disable-next-line no-undef
      if (maxParticles === TOK.deviceMaxParticles) S.maxParts = Math.max(S.maxParts, parts.length);
    } catch { /* sketch without the shared particle pool */ }
  });
});

await page.goto(pathToFileURL(file).href + `?seed=${seed}`);
await page.waitForTimeout(400);
const hooks = await page.evaluate(() => {
  const S = window.__sk;
  const wrapFn = (name, before, after) => {
    const f = window[name];
    if (typeof f !== 'function') return false;
    window[name] = function (...a) { before(); try { return f.apply(this, a); } finally { after(); } };
    return true;
  };
  let t0 = 0;
  return {
    hud: wrapFn('drawHud', () => { S.hud++; }, () => { S.hud--; }),
    scene: wrapFn('drawScene', () => { t0 = performance.now(); }, () => { S.sceneMs.push(performance.now() - t0); }),
    autoTarget: typeof window.autoTarget === 'function',
  };
});
if (!hooks.scene) problems.push('runtime: drawScene() is not a global function (use a classic <script>)');
if (!hooks.hud) problems.push('runtime: drawHud() is not a global function');

const rect = await page.evaluate(() => {
  const r = document.querySelector('canvas').getBoundingClientRect();
  return { x: r.left, y: r.top, w: r.width, h: r.height };
});
const toPage = (x, y) => [rect.x + x / 320 * rect.w, rect.y + y / 240 * rect.h];
let a = (seed ^ 0x5EED) | 0;
const rng = () => {
  a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a);
  t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296;
};
const rnd = (lo, hi) => lo + rng() * (hi - lo);
const shots = [];
async function shot(name) {
  const f = `${String(shots.length).padStart(2, '0')}_${name}.png`;
  await page.locator('canvas').screenshot({ path: path.join(out, f) });
  shots.push(f);
}
async function tap(x, y) {
  const [px, py] = toPage(x, y);
  await page.mouse.move(px, py);
  await page.mouse.down();
  await page.waitForTimeout(40 + rng() * 60);
  await page.mouse.up();
}
async function drag(x0, y0, x1, y1) {
  const [p0, q0] = toPage(x0, y0), [p1, q1] = toPage(x1, y1);
  await page.mouse.move(p0, q0);
  await page.mouse.down();
  const n = 6 + Math.floor(rng() * 8);
  for (let i = 1; i <= n; i++) {
    await page.mouse.move(p0 + (p1 - p0) * i / n, q0 + (q1 - q0) * i / n);
    await page.waitForTimeout(30 + rng() * 30);
  }
  await page.mouse.up();
}
let autoTaps = 0;
async function act() {
  const r = rng();
  if (hooks.autoTarget && r < 0.5) {
    const p = await page.evaluate(() => {
      try { return autoTarget(); } catch (e) { return { err: String(e) }; }  // eslint-disable-line no-undef
    });
    if (p && p.err) problems.push(`runtime: autoTarget() threw: ${p.err}`);
    else if (p) { autoTaps++; return tap(p.x, p.y); }
  }
  if (r < 0.62) return drag(rnd(20, 300), rnd(70, 230), rnd(20, 300), rnd(70, 230));
  if (r < 0.70) return tap(160, 32);
  if (r < 0.78) return tap(rnd(60, 260), rnd(4, 56));
  return tap(rnd(4, 316), rnd(60, 236));
}
async function playFor(ms, label, every = 2500) {
  const until = Date.now() + ms;
  let next = Date.now() + every;
  while (Date.now() < until) {
    await act();
    await page.waitForTimeout(250 + rng() * 650);
    if (Date.now() >= next) { await shot(label); next += every; }
  }
}

const T = seconds * 1000;
await page.waitForTimeout(300);
await shot('start');
await tap(160, 120);
await page.waitForTimeout(900);
await shot('intro');
await playFor(T * 0.45, 'play');
await page.waitForTimeout(7000);
await shot('idle_7s');
await page.waitForTimeout(6500);
await shot('idle_13s');
await page.keyboard.press('d');
await page.waitForTimeout(600);
await shot('debug_hud');
await page.keyboard.press('d');
await page.keyboard.press('r');
await playFor(4000, 'reduced_motion', 3500);
await page.keyboard.press('r');
await page.keyboard.press('p');
await playFor(2000, 'web_cap', 99999);
await page.keyboard.press('p');
await playFor(Math.max(3000, T * 0.55 - 6000), 'play');
await tap(28, 28);
await page.waitForTimeout(600);
await shot('home');
await tap(160, 120);
await page.waitForTimeout(1500);
await shot('restart');

const stats = await page.evaluate(() => {
  const S = window.__sk;
  const ms = S.sceneMs.slice().sort((x, y) => x - y);
  return {
    frames: S.frames, maxGrads: S.maxGrads, maxItems: S.maxItems, gradHist: S.gradHist, viol: S.viol,
    maxParts: S.maxParts, sceneAvgMs: ms.reduce((x, y) => x + y, 0) / Math.max(1, ms.length),
    sceneP95Ms: ms[Math.floor(ms.length * 0.95)] || 0, audio: S.audio,
  };
});
const notes = [];
for (const e of errors) problems.push(`console error: ${e.split('\n').slice(0, 3).join(' | ')}`);
for (const [what, n] of Object.entries(stats.viol)) problems.push(`ctx2d: ${what} (${n}x outside drawHud)`);
if (stats.maxGrads > LIMITS.gradients) problems.push(`ctx2d: ${stats.maxGrads} gradients in one frame (limit ${LIMITS.gradients})`);
if (stats.maxItems > LIMITS.items) problems.push(`ctx2d: ${stats.maxItems} fills/strokes in one frame (limit ${LIMITS.items})`);
if (stats.maxParts > LIMITS.particles) problems.push(`particles: ${stats.maxParts} live under the device cap (limit ${LIMITS.particles})`);
if (stats.frames < seconds * 15) problems.push(`runtime: only ${stats.frames} frames drawn; the loop stalled?`);
if (hooks.autoTarget && !autoTaps) problems.push('runtime: autoTarget() never returned a point');
const au = stats.audio;
if (au.contexts !== 1) problems.push(`audio: ${au.contexts} AudioContexts (create exactly one, lazily, in initAudio)`);
for (const [type, n] of Object.entries(au.oscTypes)) {
  if (type !== 'sine') problems.push(`audio: ${n} ${type} oscillator starts (ported scores use sine voices)`);
}
if (au.maxOscHz >= 7800) {
  problems.push(`audio: oscillator at ${Math.round(au.maxOscHz)} Hz (keep partials < 7.8 kHz; the device runs at 16 kHz)`);
}
const COMMON = ['Gain', 'Oscillator', 'BiquadFilter', 'Buffer', 'BufferSource', 'DynamicsCompressor', 'Delay'];
const UNPORTABLE = ['Convolver', 'WaveShaper', 'ScriptProcessor', 'StereoPanner',
  'MediaStreamSource', 'MediaElementSource', 'MediaStreamDestination'];
for (const [what, n] of Object.entries(au.creates)) {
  if (UNPORTABLE.includes(what)) problems.push(`audio: create${what}() x${n} (not in the device score subset)`);
  else if (!COMMON.includes(what)) notes.push(`audio: create${what}() x${n}; check the device score can reproduce it`);
}

// ---------- contact sheet ----------
const sheet = await browser.newPage({ viewport: { width: 400, height: 300 } });
const png = await sheet.evaluate(async ({ imgs, names }) => {
  const cols = 4, w = 320, h = 240, pad = 4, lab = 12;
  const k = document.createElement('canvas');
  k.width = cols * (w + pad) + pad;
  k.height = Math.ceil(imgs.length / cols) * (h + lab + pad) + pad;
  const x = k.getContext('2d');
  x.fillStyle = '#1E2245'; x.fillRect(0, 0, k.width, k.height);
  for (let i = 0; i < imgs.length; i++) {
    const im = new Image(); im.src = imgs[i]; await im.decode();
    const cx = pad + (i % cols) * (w + pad), cy = pad + Math.floor(i / cols) * (h + lab + pad);
    x.drawImage(im, cx, cy + lab, w, h);
    x.fillStyle = '#E6F5FF'; x.font = '10px monospace'; x.fillText(names[i], cx + 2, cy + 9);
  }
  return k.toDataURL('image/png').split(',')[1];
}, {
  imgs: shots.map(f => 'data:image/png;base64,' + fs.readFileSync(path.join(out, f)).toString('base64')),
  names: shots,
});
fs.writeFileSync(path.join(out, 'contact.png'), Buffer.from(png, 'base64'));
await browser.close();

const report = { game, file, seed, seconds, hooks, autoTaps, stats, warnings: warnings.slice(0, 20), notes, problems, shots };
fs.writeFileSync(path.join(out, 'report.json'), JSON.stringify(report, null, 2));
const fmt = o => Object.entries(o).map(([k, n]) => `${k} ${n}`).join('/') || 'none';
console.log(`${game}: ${stats.frames} frames, scene avg ${stats.sceneAvgMs.toFixed(2)} ms (p95 ${stats.sceneP95Ms.toFixed(2)}), ` +
  `max ${stats.maxGrads} gradients / ${stats.maxItems} fills+strokes per frame, max ${stats.maxParts} particles, ` +
  `${autoTaps} autoTarget taps`);
console.log(`audio: ${au.contexts} context${au.offline ? ` (+${au.offline} offline)` : ''}, oscillators ${fmt(au.oscTypes)}, filters ${fmt(au.filterTypes)}, ` +
  `buffers ${fmt(au.bufferRates)} Hz, ${au.paramMods} param modulations, peak ${au.maxLive} live sources, ` +
  `max oscillator ${Math.round(au.maxOscHz)} Hz`);
console.log(`shots: ${path.join(out, 'contact.png')}`);
for (const w of warnings.slice(0, 5)) console.log(`warning: ${w}`);
for (const n of notes) console.log(`note: ${n}`);
if (problems.length) {
  console.log(`FAIL (${problems.length})`);
  for (const p of problems) console.log(`  - ${p}`);
  process.exit(1);
}
console.log('PASS');
