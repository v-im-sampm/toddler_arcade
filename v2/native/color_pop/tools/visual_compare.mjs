// Seeded browser-vs-LVGL comparison for Color Pop.
// For every tools/scenarios/*.txt script, runs the original Canvas reference (v2/reference/quality_bar.html)
// and the native LVGL executable offscreen with the same seed and input timeline, then compares the
// gameplay state dump (must match) and the 320x240 pixels (must stay under measured thresholds).
//
// usage: node visual_compare.mjs [native.exe] [out_dir] [--seed N] [--only NAME]
import { createRequire } from 'node:module';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..', '..', '..', '..');
const require = createRequire(path.join(root, 'tools', 'quality_bar_golden', 'package.json'));
const { chromium } = require('playwright-core');

const args = process.argv.slice(2);
const opt = (name, def) => { const i = args.indexOf(name); if (i < 0) return def; const v = args[i + 1]; args.splice(i, 2); return v; };
const seed = parseInt(opt('--seed', '1234'), 10);
const only = opt('--only', null);
const exe = args[0] || path.join(root, 'build-v2-native', 'color_pop', 'Release', 'color_pop_native.exe');
const out = args[1] || path.join(root, 'build-v2-native', 'color_pop', 'comparison');
const pack = path.join(path.dirname(exe), 'color_pop.pack');
fs.mkdirSync(out, { recursive: true });

// Thresholds measured on the scenarios below with seed 1234 (max observed: MAE 3.72, 2.58% > 32, in
// 06_big_celebration). Residual differences come from RGB565 + Bayer dither of the background, integer
// sprite placement, quantised FX rotation frames and LVGL vs Skia anti-aliasing. Negative control: the
// same renderer with one bubble missing/moved (02_play vs 03_wrong_hint1) gives MAE 6.65 / 9.56%, so
// gross layering/scale/colour/placement errors fail; small gameplay drift is caught by the exact state
// parity check instead of by pixels.
const MAX_MAE = 5.0;          // mean absolute RGB error per channel (0..255)
const MAX_OVER32 = 4.0;       // % of pixels with any channel off by more than 32
const POS_TOL = 0.02, SCALE_TOL = 0.002;

const scenarios = fs.readdirSync(path.join(here, 'scenarios')).filter(f => f.endsWith('.txt'))
  .filter(f => !only || f.includes(only)).sort();

function parseScript(text) {
  return text.split(/\r?\n/).map(l => l.replace(/#.*/, '').trim()).filter(Boolean)
    .map(l => { const [op, a, b] = l.split(/\s+/); return { op, a: +(a ?? 0), b: +(b ?? 0) }; });
}

const browser = await chromium.launch({ channel: 'msedge', headless: true });
const results = [];
let failed = false;
try {
  for (const file of scenarios) {
    const name = path.basename(file, '.txt');
    const scriptPath = path.join(here, 'scenarios', file);
    const commands = parseScript(fs.readFileSync(scriptPath, 'utf8'));

    // ---- native ----
    const nativeBmp = path.join(out, `${name}.native.bmp`);
    const nativeOut = execFileSync(exe, ['--script', scriptPath, '--seed', String(seed), '--pack', pack,
                                         '--screenshot', nativeBmp, '--dump'], { encoding: 'utf8' });
    const nativeState = JSON.parse(nativeOut.trim().split(/\r?\n/).pop());

    // ---- browser reference ----
    const page = await browser.newPage({ viewport: { width: 400, height: 300 }, deviceScaleFactor: 1 });
    await page.addInitScript(cosmeticSeed => {
      // frng = Math.random is captured at load: seed it the same way as the native cosmetic stream.
      let a = cosmeticSeed | 0;
      Math.random = () => { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a);
        t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; };
      window.requestAnimationFrame = () => 0;
      window.setTimeout = (fn, ms, ...rest) => 0;   // only used for audio prompts; audio is stubbed
    }, (seed ^ 0x9E3779B9) | 0);
    await page.goto(pathToFileURL(path.join(root, 'v2', 'reference', 'quality_bar.html')).href + `?seed=${seed}`);
    const ref = await page.evaluate(cmds => {
      const g = eval;  // indirect eval reaches top-level let/const bindings of the page script
      g('initAudio = () => {}');
      const cv = document.getElementById('c');
      cv.style.width = '320px'; cv.style.height = '240px'; cv.width = 320; cv.height = 240;
      g('scale = 1'); ctx.setTransform(1, 0, 0, 1, 0, 0);
      const draw = () => {
        drawBackground();
        for (const b of g('bubbles')) if (b.t >= 0) drawBubble(b);
        drawCrab(); drawHelperHand(); drawFx(); drawBadge(); drawChrome();
        if (st.phase === 'START') drawStart();
      };
      const tap = (x, y) => {
        const r = cv.getBoundingClientRect();
        cv.dispatchEvent(new PointerEvent('pointerdown', { clientX: r.left + x * r.width / 320,
          clientY: r.top + y * r.height / 240, bubbles: true, cancelable: true }));
      };
      const floating = target => g('bubbles').find(b => b.state === 'float' && (b.color === st.target) === target);
      draw();
      for (const c of cmds) {
        if (c.op === 'step') for (let i = 0; i < c.a; ++i) { update(1 / 64); draw(); }
        else if (c.op === 'tap') tap(c.a, c.b);
        else if (c.op === 'home') tap(20, 20);
        else if (c.op === 'reduced') g(`reduced = ${c.a !== 0}`);
        else if (c.op === 'target' || c.op === 'wrong') {
          const b = floating(c.op === 'target');
          if (!b) throw new Error(`no floating ${c.op} bubble`);
          tap(b.x, b.y);
        } else throw new Error(`unknown command ${c.op}`);
      }
      const px = ctx.getImageData(0, 0, 320, 240).data;
      const rgb = new Uint8Array(320 * 240 * 3);
      for (let i = 0, j = 0; i < px.length; i += 4) { rgb[j++] = px[i]; rgb[j++] = px[i + 1]; rgb[j++] = px[i + 2]; }
      let bytes = '';
      for (let i = 0; i < rgb.length; i += 8192) bytes += String.fromCharCode(...rgb.subarray(i, i + 8192));
      const state = { phase: st.phase, round: st.round, target: st.target || '', count: st.count, hint: st.hint,
        successes: st.successes, parts: g('parts').length, decor: g('decor').length, rings: g('rings').length,
        bubbles: g('bubbles').map(b => ({ color: b.color, x: b.x, y: b.y, s: b.s })) };
      return { png: cv.toDataURL('image/png').split(',')[1], rgb: btoa(bytes), state };
    }, commands);
    await page.close();
    fs.writeFileSync(path.join(out, `${name}.reference.png`), Buffer.from(ref.png, 'base64'));

    // ---- state parity ----
    const problems = [];
    const rs = ref.state, ns = nativeState;
    for (const k of ['phase', 'round', 'target', 'count', 'hint', 'successes', 'parts', 'decor', 'rings'])
      if (rs[k] !== ns[k]) problems.push(`${k}: reference ${rs[k]} native ${ns[k]}`);
    if (rs.bubbles.length !== ns.bubbles.length) problems.push(`bubbles: ${rs.bubbles.length} vs ${ns.bubbles.length}`);
    else rs.bubbles.forEach((b, i) => {
      const n = ns.bubbles[i];
      if (b.color !== n.color || Math.abs(b.x - n.x) > POS_TOL || Math.abs(b.y - n.y) > POS_TOL || Math.abs(b.s - n.s) > SCALE_TOL)
        problems.push(`bubble ${i}: reference ${JSON.stringify(b)} native ${JSON.stringify(n)}`);
    });

    // ---- pixels ----
    const rgb = Buffer.from(ref.rgb, 'base64');
    const bmp = fs.readFileSync(nativeBmp);
    if (bmp.toString('ascii', 0, 2) !== 'BM' || bmp.readUInt32LE(18) !== 320 || bmp.readUInt32LE(22) !== 240 || bmp.readUInt16LE(28) !== 24)
      throw new Error('Unexpected native screenshot format');
    const offset = bmp.readUInt32LE(10), stride = 320 * 3;
    const diff = Buffer.alloc(54 + 320 * 240 * 3);
    bmp.copy(diff, 0, 0, 54);
    let total = 0, bad = 0;
    for (let y = 0; y < 240; ++y) for (let x = 0; x < 320; ++x) {
      const a = (y * 320 + x) * 3, b = offset + (239 - y) * stride + x * 3;
      const dr = Math.abs(rgb[a] - bmp[b + 2]), dg = Math.abs(rgb[a + 1] - bmp[b + 1]), db = Math.abs(rgb[a + 2] - bmp[b]);
      total += dr + dg + db;
      const m = Math.max(dr, dg, db);
      if (m > 32) ++bad;
      const v = Math.min(255, m * 4), o = 54 + (239 - y) * stride + x * 3;
      diff[o] = v; diff[o + 1] = v; diff[o + 2] = v;
    }
    fs.writeFileSync(path.join(out, `${name}.diff.bmp`), diff);
    const r = { scenario: name, mae: +(total / (320 * 240 * 3)).toFixed(2), over32: +(bad / (320 * 240) * 100).toFixed(2),
                stateMatch: problems.length === 0, problems,
                phase: ns.phase, round: ns.round, hint: ns.hint, successes: ns.successes, parts: ns.parts };
    const ok = r.stateMatch && r.mae <= MAX_MAE && r.over32 <= MAX_OVER32;
    r.pass = ok; if (!ok) failed = true;
    results.push(r);
    console.log(`${ok ? 'PASS' : 'FAIL'} ${name}: MAE ${r.mae} over32 ${r.over32}% state ${r.stateMatch ? 'match' : problems.join('; ')}`);
  }
} finally {
  await browser.close();
}
const report = { seed, thresholds: { meanAbsoluteRGB: MAX_MAE, pixelsOver32Percent: MAX_OVER32, positionTolerance: POS_TOL },
                 results };
fs.writeFileSync(path.join(out, 'report.json'), JSON.stringify(report, null, 2) + '\n');
fs.writeFileSync(path.join(out, 'comparison.html'),
  '<title>Color Pop: original vs LVGL</title><style>body{background:#1E2245;color:white;font:14px sans-serif}' +
  'img{width:480px;image-rendering:pixelated;margin:2px}</style><h1>Original Canvas / LVGL 9 native / diff x4</h1>' +
  results.map(r => `<h2>${r.scenario}: MAE ${r.mae}, &gt;32: ${r.over32}% ${r.pass ? 'PASS' : 'FAIL'}</h2>` +
    `<img src="${r.scenario}.reference.png"><img src="${r.scenario}.native.bmp"><img src="${r.scenario}.diff.bmp">`).join('') +
  `<pre>${JSON.stringify(report, null, 2)}</pre>`);
if (failed) { console.error('Color Pop visual comparison failed (see report.json)'); process.exitCode = 1; }
