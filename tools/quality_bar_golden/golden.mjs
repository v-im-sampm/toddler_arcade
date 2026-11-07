// Renders a v2/reference sketch deterministically in Edge (seeded Math.random,
// no rAF loop, fixed 1/60 s steps), replays the game's script, and diffs every
// captured frame against the C++ port's reference render. Writes
// <out>/report.html with side-by-side PNGs.
//
//   node golden.mjs [game] [path/to/<game>_reference_render.exe]
//
// Games:
//   hatch_a_pet  hatch_a_pet.html (starts on START), script_hatch_a_pet.txt -> out_hatch_a_pet/  (default)
//
// Color Pop left pixel parity when it moved to PixelRoot32 sprites; its mode
// (and script.txt) live in git history.
//
// Exit code 1 when any frame exceeds the fidelity thresholds below.
import { chromium } from 'playwright-core';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..', '..');
const GAMES = {
  hatch_a_pet: { html: 'hatch_a_pet.html', script: 'script_hatch_a_pet.txt', out: 'out_hatch_a_pet',
                 title: 'Hatch-a-Pet: hatch_a_pet.html' },
};
const args = process.argv.slice(2);
const game = args[0] in GAMES ? args.shift() : 'hatch_a_pet';
const cfg = GAMES[game];
const out = path.join(here, cfg.out);
const scriptPath = path.join(here, cfg.script);
const exe = args[0] ||
  path.join(root, 'firmware', 'build-host', 'games', game, 'Debug', `${game}_reference_render.exe`);
const W = 320, H = 240;
// Fidelity thresholds per frame (8-bit channels, XRGB output before RGB565).
const MAX_MEAN_ABS = 1.5;      // mean |diff| over all channels
const MAX_BAD_PCT = 1.0;       // % of pixels whose worst channel differs by > 32

// ---------- script ----------
const cmds = [];
let gseed = 1234, cseed = 99, end = 0;
for (const raw of fs.readFileSync(scriptPath, 'utf8').split(/\r?\n/)) {
  const line = raw.trim();
  if (!line || line.startsWith('#')) continue;
  const [op, ...a] = line.split(/\s+/);
  const n = a.map(Number);
  if (op === 'seed') { gseed = n[0]; cseed = n[1]; }
  else if (op === 'end') end = n[0];
  else cmds.push({ op, frame: n[0], x: n[1], y: n[2] });
}

// ---------- C++ frames ----------
fs.rmSync(out, { recursive: true, force: true });
fs.mkdirSync(out, { recursive: true });
console.log(execFileSync(exe, [scriptPath, out], { encoding: 'utf8' }).trim());

function readPpm(file) {
  const buf = fs.readFileSync(file);
  let pos = 0, fields = [];
  while (fields.length < 4) {
    while (/\s/.test(String.fromCharCode(buf[pos]))) pos++;
    let s = '';
    while (!/\s/.test(String.fromCharCode(buf[pos]))) s += String.fromCharCode(buf[pos++]);
    fields.push(s);
  }
  pos++;
  return buf.subarray(pos, pos + W * H * 3);
}

// ---------- reference frames ----------
const browser = await chromium.launch({ channel: 'msedge', headless: true });
const page = await browser.newPage({ viewport: { width: 400, height: 300 }, deviceScaleFactor: 1 });
await page.emulateMedia({ reducedMotion: 'no-preference' });
await page.addInitScript(cosmetic => {
  let a = cosmetic;  // reference mulberry32, used as frng (Math.random)
  Math.random = function () {
    a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a);
    t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296;
  };
  window.requestAnimationFrame = () => 0;  // we drive update()/draw manually
}, cseed);
const url = pathToFileURL(path.join(root, 'v2', 'reference', cfg.html)).href + `?seed=${gseed}`;
await page.goto(url);
const golden = await page.evaluate(({ cmds, end, game }) => {
  /* global cv, ctx, st, update, drawScene, drawStart, ac */
  cv.width = 320; cv.height = 240; cv.style.width = '320px'; cv.style.height = '240px';
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  {
    // Web Audio becomes an inert stub: the sketch sees `ac` set (so audio-gated
    // logic such as the bird timer runs) but no noise buffer consumes Math.random.
    const stub = new Proxy(function () {}, {
      get: (_, k) => (k === 'currentTime' ? 0 : k === 'state' ? 'running' : stub),
      set: () => true,
      apply: () => stub,
      construct: () => stub,
    });
    window.initAudio = () => { if (!ac) ac = stub; };
  }
  const tap = (x, y) => {
    const r = cv.getBoundingClientRect();
    cv.dispatchEvent(new PointerEvent('pointerdown', { clientX: r.left + x, clientY: r.top + y, bubbles: true, cancelable: true }));
  };
  const frames = {};
  for (let f = 1; f <= end; f++) {
    for (const c of cmds) {
      if (c.frame !== f) continue;
      if (c.op === 'tap') tap(c.x, c.y);
    }
    update(1 / 60);
    drawScene();
    if (st.phase === 'START') drawStart();
    if (cmds.some(c => c.op === 'capture' && c.frame === f)) {
      frames[f] = Array.from(ctx.getImageData(0, 0, 320, 240).data);
    }
  }
  return frames;
}, { cmds, end, game });

// ---------- compare ----------
const rows = [];
let failed = 0;
for (const key of Object.keys(golden).sort((a, b) => a - b)) {
  const f = Number(key), name = String(f).padStart(4, '0');
  const g = golden[key];
  const c = readPpm(path.join(out, `cpp_${name}.ppm`));
  const d = readPpm(path.join(out, `cpp565_${name}.ppm`));
  let sum = 0, bad = 0, max = 0, se = 0;
  const diff = new Uint8ClampedArray(W * H * 4);
  for (let i = 0; i < W * H; i++) {
    let worst = 0;
    for (let k = 0; k < 3; k++) {
      const e = Math.abs(g[i * 4 + k] - c[i * 3 + k]);
      sum += e; se += e * e; worst = Math.max(worst, e);
    }
    if (worst > 32) bad++;
    max = Math.max(max, worst);
    const v = Math.min(255, worst * 4);
    diff.set([v, v * 0.3, 255 - v, 255], i * 4);
  }
  const mean = sum / (W * H * 3), badPct = bad * 100 / (W * H);
  const psnr = se ? 10 * Math.log10(255 * 255 / (se / (W * H * 3))) : Infinity;
  const ok = mean <= MAX_MEAN_ABS && badPct <= MAX_BAD_PCT;
  if (!ok) failed++;
  rows.push({ f, name, mean, badPct, max, psnr, ok, g, c: Array.from(c), d: Array.from(d), diff: Array.from(diff) });
  console.log(`frame ${name}: mean|d| ${mean.toFixed(2)}  >32: ${badPct.toFixed(2)}%  max ${max}  PSNR ${psnr.toFixed(1)} dB  ${ok ? 'ok' : 'FAIL'}`);
}

// ---------- side-by-side PNGs + report ----------
for (const r of rows) {
  const png = await page.evaluate(({ g, c, d, diff }) => {
    const k = document.createElement('canvas'); k.width = 320 * 4 + 30; k.height = 240;
    const x = k.getContext('2d'); x.fillStyle = '#1E2245'; x.fillRect(0, 0, k.width, k.height);
    const put = (arr, rgb, ox) => {
      const img = x.createImageData(320, 240);
      for (let i = 0; i < 320 * 240; i++) {
        if (rgb) { img.data[i * 4] = arr[i * 3]; img.data[i * 4 + 1] = arr[i * 3 + 1]; img.data[i * 4 + 2] = arr[i * 3 + 2]; img.data[i * 4 + 3] = 255; }
        else for (let j = 0; j < 4; j++) img.data[i * 4 + j] = arr[i * 4 + j];
      }
      x.putImageData(img, ox, 0);
    };
    put(g, false, 0); put(c, true, 330); put(d, true, 660); put(diff, false, 990);
    return k.toDataURL('image/png').split(',')[1];
  }, r);
  fs.writeFileSync(path.join(out, `side_${r.name}.png`), Buffer.from(png, 'base64'));
}
fs.writeFileSync(path.join(out, 'report.html'), `<!doctype html><meta charset="utf-8"><title>${cfg.title} reference vs port</title>
<style>body{background:#1E2245;color:#E6F5FF;font:14px system-ui;margin:16px}img{image-rendering:pixelated;width:${(320 * 4 + 30) * 2}px;max-width:100%}
.bad{color:#FF9A92}</style><h1>${cfg.title} (Edge) vs C++ port</h1>
<p>Columns: reference canvas · port (XRGB) · port as the device shows it (RGB565, dithered) · |difference| ×4.</p>
${rows.map(r => `<h3 class="${r.ok ? '' : 'bad'}">frame ${r.f} (t=${(r.f / 60).toFixed(2)} s): mean |d| ${r.mean.toFixed(2)}, ${r.badPct.toFixed(2)}% px > 32, PSNR ${r.psnr.toFixed(1)} dB</h3><img src="side_${r.name}.png">`).join('\n')}`);
await browser.close();
console.log(`${rows.length} frames compared, ${failed} over threshold. Report: ${path.join(out, 'report.html')}`);
process.exit(failed ? 1 : 0);
