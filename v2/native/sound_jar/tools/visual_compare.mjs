// Seeded browser-vs-LVGL comparison for Sound Jar.
// For every tools/scenarios/*.txt script, runs the original Canvas reference (v2/reference/sound_jar.html)
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
const exe = args[0] || path.join(root, 'build-v2-native', 'sound_jar', 'Release', 'sound_jar_native.exe');
const out = args[1] || path.join(root, 'build-v2-native', 'sound_jar', 'comparison');
const pack = path.join(path.dirname(exe), 'sound_jar.pack');
fs.mkdirSync(out, { recursive: true });

// Thresholds measured with seed 1234 on the scenarios (max observed: MAE 4.79 in 01_start, 2.08% > 32 in
// 06_fx_two). Residuals: RGB565 output, integer sprite placement, quantised animation frames and LVGL vs Skia
// anti-aliasing at edges. Negative controls (native vs native): only the helper hand differs -> 5.21% > 32;
// only the robot visor -> 8.92%; empty vs recording -> 12.5%. So a missing/misplaced major sprite fails, while
// smaller drift is caught by the exact state-parity check (phase, takes, fx, voices, particle positions).
const MAX_MAE = Number(process.env.SJ_MAX_MAE ?? 5.5);       // mean absolute RGB error per channel (0..255)
const MAX_OVER32 = Number(process.env.SJ_MAX_OVER32 ?? 3.0); // % of pixels with any channel off by more than 32
const PSUM_TOL = 1e-3;

const scenarios = fs.readdirSync(path.join(here, 'scenarios')).filter(f => f.endsWith('.txt'))
  .filter(f => !only || f.includes(only)).sort();

function parseScript(text) {
  return text.split(/\r?\n/).map(l => l.replace(/#.*/, '').trim()).filter(Boolean)
    .map(l => { const [op, a, b] = l.split(/\s+/); return { op, a: +(a ?? 0), b: +(b ?? 0), word: a }; });
}

const browser = await chromium.launch({ channel: 'msedge', headless: true });
const results = [];
let failed = false;
try {
  for (const file of scenarios) {
    const name = path.basename(file, '.txt');
    const scriptPath = path.join(here, 'scenarios', file);
    const commands = parseScript(fs.readFileSync(scriptPath, 'utf8'));
    if (commands.some(c => c.op === 'intent')) { console.log(`SKIP ${name}: voice intents have no browser equivalent`); continue; }

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
    }, (seed ^ 0x9E3779B9) | 0);
    await page.goto(pathToFileURL(path.join(root, 'v2', 'reference', 'sound_jar.html')).href + `?seed=${seed}`);
    const ref = await page.evaluate(cmds => {
      const g = eval;  // indirect eval reaches top-level let/const bindings of the page script
      g('initAudio = () => {}');  // no Web Audio: the JS voice mirrors still run (native mirrors them the same way)
      const cv = document.getElementById('c');
      cv.style.width = '320px'; cv.style.height = '240px'; cv.width = 320; cv.height = 240;
      g('scale = 1'); ctx.setTransform(1, 0, 0, 1, 0, 0);
      const draw = () => { drawScene(); if (st.phase === 'START') drawStart(); };
      const ev = (type, x, y) => {
        const r = cv.getBoundingClientRect();
        cv.dispatchEvent(new PointerEvent(type, { pointerId: 1, clientX: r.left + x * r.width / 320,
          clientY: r.top + y * r.height / 240, bubbles: true, cancelable: true }));
      };
      let lx = 0, ly = 0;
      draw();
      for (const c of cmds) {
        if (c.op === 'step') for (let i = 0; i < c.a; ++i) { update(1 / 64); draw(); }
        else if (c.op === 'down') { lx = c.a; ly = c.b; ev('pointerdown', c.a, c.b); }
        else if (c.op === 'move') { lx = c.a; ly = c.b; ev('pointermove', c.a, c.b); }
        else if (c.op === 'up') ev('pointerup', lx, ly);
        else if (c.op === 'tap') { ev('pointerdown', c.a, c.b); ev('pointerup', c.a, c.b); }
        else if (c.op === 'home') { ev('pointerdown', 20, 20); ev('pointerup', 20, 20); }
        else if (c.op === 'reduced') g(`reduced = ${c.a !== 0}`);
        else if (c.op === 'auto') {
          const t = autoTarget();
          if (!t) throw new Error('no auto target');
          ev('pointerdown', t.x, t.y); ev('pointerup', t.x, t.y);
        } else throw new Error(`unknown command ${c.op}`);
      }
      draw();
      const px = ctx.getImageData(0, 0, 320, 240).data;
      const rgb = new Uint8Array(320 * 240 * 3);
      for (let i = 0, j = 0; i < px.length; i += 4) { rgb[j++] = px[i]; rgb[j++] = px[i + 1]; rgb[j++] = px[i + 2]; }
      let bytes = '';
      for (let i = 0; i < rgb.length; i += 8192) bytes += String.fromCharCode(...rgb.subarray(i, i + 8192));
      const P = g('parts');
      const state = { phase: st.phase, take: st.take, sound: jar.sound, mist: jar.mist, tries: st.tries,
        actions: st.actions, hint: st.hint, level: jar.level, lid: lid.u, voices: g('svoices').length,
        parts: P.length, rings: g('rings').length, fx: [...st.fx], order: [...st.order],
        psum: P.reduce((s, p) => s + p.x + p.y, 0) };
      return { png: cv.toDataURL('image/png').split(',')[1], rgb: btoa(bytes), state };
    }, commands);
    await page.close();
    fs.writeFileSync(path.join(out, `${name}.reference.png`), Buffer.from(ref.png, 'base64'));

    // ---- state parity ----
    const problems = [];
    const rs = ref.state, ns = nativeState;
    for (const k of ['phase', 'take', 'sound', 'mist', 'tries', 'actions', 'hint', 'voices', 'parts', 'rings'])
      if (rs[k] !== ns[k]) problems.push(`${k}: reference ${rs[k]} native ${ns[k]}`);
    for (const k of ['fx', 'order'])
      if (JSON.stringify(rs[k]) !== JSON.stringify(ns[k])) problems.push(`${k}: reference ${rs[k]} native ${ns[k]}`);
    if (Math.abs(rs.level - ns.level) > 1e-3) problems.push(`level: reference ${rs.level} native ${ns.level}`);
    if (Math.abs(rs.lid - ns.lid) > 1e-3) problems.push(`lid: reference ${rs.lid} native ${ns.lid}`);
    if (Math.abs(rs.psum - ns.psum) > Math.max(PSUM_TOL, 1e-6 * Math.abs(rs.psum)) * 10)
      problems.push(`particle positions: reference ${rs.psum.toFixed(4)} native ${ns.psum.toFixed(4)}`);

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
                stateMatch: problems.length === 0, problems, phase: ns.phase, take: ns.take, fx: ns.fx, parts: ns.parts };
    const ok = r.stateMatch && r.mae <= MAX_MAE && r.over32 <= MAX_OVER32;
    r.pass = ok; if (!ok) failed = true;
    results.push(r);
    console.log(`${ok ? 'PASS' : 'FAIL'} ${name}: MAE ${r.mae} over32 ${r.over32}% state ${r.stateMatch ? 'match' : problems.join('; ')}`);
  }
} finally {
  await browser.close();
}
const report = { seed, thresholds: { meanAbsoluteRGB: MAX_MAE, pixelsOver32Percent: MAX_OVER32 }, results };
fs.writeFileSync(path.join(out, 'report.json'), JSON.stringify(report, null, 2) + '\n');
fs.writeFileSync(path.join(out, 'comparison.html'),
  '<title>Sound Jar: original vs LVGL</title><style>body{background:#1E2245;color:white;font:14px sans-serif}' +
  'img{width:480px;image-rendering:pixelated;margin:2px}</style><h1>Original Canvas / LVGL 9 native / diff x4</h1>' +
  results.map(r => `<h2>${r.scenario}: MAE ${r.mae}, &gt;32: ${r.over32}% ${r.pass ? 'PASS' : 'FAIL'}</h2>` +
    `<img src="${r.scenario}.reference.png"><img src="${r.scenario}.native.bmp"><img src="${r.scenario}.diff.bmp">`).join('') +
  `<pre>${JSON.stringify(report, null, 2)}</pre>`);
if (failed) { console.error('Sound Jar visual comparison failed (see report.json)'); process.exitCode = 1; }
