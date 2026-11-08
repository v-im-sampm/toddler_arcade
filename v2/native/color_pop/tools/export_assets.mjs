// Exports Color Pop artwork by running the ORIGINAL Canvas drawing functions of
// v2/reference/quality_bar.html in headless Edge. Nothing is redrawn by hand: each sprite is
// produced by calling the sketch's own draw function with chosen state, optionally with a layer
// filter that lets only some of that function's fill()/stroke() calls through (to separate
// moving parts). Outputs:
//   assets/generated/*.png            per-sprite previews (gitignored)
//   assets/generated/color_pop.pack   runtime pack for desktop + device (gitignored)
//   device/main/pack_signature.hpp    firmware/asset match check (gitignored, generated)
//   assets/manifest.json              provenance, crops, origins, formats, timing, bytes
//   assets/contact_sheet.png          visual review sheet
import { createRequire } from 'node:module';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const project = path.resolve(here, '..');
const root = path.resolve(project, '..', '..', '..');
const require = createRequire(path.join(root, 'tools', 'quality_bar_golden', 'package.json'));
const { chromium } = require('playwright-core');
const referenceRel = 'v2/reference/quality_bar.html';
const reference = path.join(root, referenceRel);
const assetsDir = path.join(project, 'assets');
const generated = path.join(assetsDir, 'generated');
fs.mkdirSync(generated, { recursive: true });

const TAU = Math.PI * 2;
// Format codes stored in the pack (mapped to LVGL colour formats by src/ui/asset_pack.cpp).
const FMT = { RGB565: 1, RGB565A8: 2, A8: 3 };
const specs = [];
const add = (id, fmt, crop, origin, expr, filter = null, meta = {}) =>
  specs.push({ id, fmt, crop, origin, expr, filter, meta });

// ---- background, clouds, shimmer (drawBackground / the cached bg layer) ----
add('bg', 'RGB565', [0, 0, 320, 240], [0, 0], 'ctx.drawImage(bg, 0, 0, W, H)', null,
    { source: 'paintBg() cached layer', note: 'ordered 4x4 dither to RGB565' });
add('cloud', 'A8', [88, 4, 46, 30], [100, 22], 'time = 25; reduced = false; drawBackground()',
    "(m) => m === 'fill'", { source: 'drawBackground() clouds', recolor: '#FFFFFF' });
for (let k = 0; k < 16; ++k) {
  add(`shimmer_${k}`, 'A8', [0, 62, 320, 10], [0, 62],
      `time = ${(k / 16) * (TAU / 1.2)}; drawBackground()`, "(m) => m === 'stroke'",
      { source: 'drawBackground() horizon shimmer', recolor: '#FFFFFF', frame: k, frames: 16,
        period_s: TAU / 1.2, full_width: true });
}

// ---- bubbles (drawBubble): body, iridescent rim mask, hint glow masks ----
const bubble = (color, extra = '') =>
  `bubbles = [{color:'${color}', x:160, y:120, alpha:1, rot:0, sx:1, sy:1, s:1, glow:0, ph:0${extra}}]; ` +
  'time = 0; drawBubble(bubbles[0])';
for (const color of ['RED', 'YELLOW', 'GREEN', 'BLUE']) {
  add(`bubble_${color.toLowerCase()}`, 'RGB565A8', [124, 84, 72, 80], [160, 120], bubble(color),
      '(m, c) => !(m === "stroke" && (c.lineWidth === 2.5 || c.lineWidth === 6))',
      { source: 'drawBubble() without rim/glow layers', pad: 3 });
}
add('bubble_rim', 'A8', [124, 84, 72, 72], [160, 120], bubble('RED'),
    '(m, c) => { if (m !== "stroke" || c.lineWidth !== 2.5) return false; c.strokeStyle = "#fff"; c.globalAlpha = 1; return true; }',
    { source: 'drawBubble() iridescent rim arc at ph=0', recolor: 'hsl((time*60+ph*40)%360,90%,80%) @ 0.35',
      rotate: 'ph*0.3 + rot', pad: 3 });
for (let k = 1; k <= 4; ++k) {
  add(`bubble_glow_${k}`, 'A8', [116, 76, 88, 88], [160, 120], bubble('RED', `, glow:${k / 4}`).replace('glow:0, ', ''),
      '(m, c) => { if (m !== "stroke" || c.lineWidth !== 6) return false; c.globalAlpha = 1; return true; }',
      { source: 'drawBubble() hint glow ring', glow: k / 4, recolor: '#FFE066 @ (0.35+0.5g)', pad: 3 });
}

// ---- badge (drawBadge) ----
for (const color of ['RED', 'YELLOW', 'GREEN', 'BLUE']) {
  for (const happy of [0, 1]) {
    add(`badge_${color.toLowerCase()}_${happy ? 'happy' : 'glyph'}`, 'RGB565A8', [130, 2, 60, 60], [160, 32],
        `st.target='${color}'; st.phase='CELEBRATE'; st.hint=0; badge.happy=${happy}; badge.sv=0; time=0; drawBadge()`,
        null, { source: 'drawBadge()' });
  }
}
add('badge_glow', 'A8', [124, 4, 72, 76], [160, 42],
    "ctx.translate(0, 10); st.target='RED'; st.phase='PLAY'; st.hint=1; badge.happy=0; badge.sv=0; time=0; drawBadge()",
    '(m, c) => { if (m !== "stroke" || c.lineWidth !== 4) return false; c.globalAlpha = 1; return true; }',
    { source: 'drawBadge() hint glow ring', recolor: '#FFE066 @ 0.4+0.4sin' });

// ---- crab (drawCrab) split into moving parts by draw-call order ----
// drawCrab() op order: legs 0-5, left arm/claw 6-8, right arm/claw 9-11, body 12-13,
// eye stalks/whites/outlines, pupils (ink fills), mouth, cheeks.
const crabOrigin = [102, 122];  // translate(60,0) + drawCrab's (42, 222+yOff) with yOff=-100
const crabBase = 'ctx.translate(60, 0); TOK.breathScale = 0; crab.yOff = -100; crab.look = [0, -1]; ' +
                 'crab.blink = 1; crab.clap = 0; crab.giggle = 0; crab.dance = 0; crab.point = null; time = 0; ';
const crabCrop = [52, 70, 100, 70];
const ops = (a, b) => `(m, c, i) => i >= ${a} && i <= ${b}`;
for (let k = 0; k < 8; ++k) {
  add(`crab_legs_${k}`, 'A8', crabCrop, crabOrigin, crabBase + `time = ${(k / 8) * (TAU / 6)}; drawCrab()`, ops(0, 5),
      { source: 'drawCrab() legs', recolor: '#C2621B', frame: k, frames: 8, period_s: TAU / 6 });
}
for (let k = 0; k <= 5; ++k) {
  const u = Math.asin(k / 5) / (3 * Math.PI);  // clapT = |sin(3*pi*u)| = k/5
  // Cancel drawCrab's clap jump (-8 sin(pi u)) so the part stays at the crab origin.
  const expr = crabBase.replace('ctx.translate(60, 0)', `ctx.translate(60, ${8 * Math.sin(Math.PI * u)})`) +
               `crab.clap = ${0.6 * (1 - u)}; drawCrab()`;
  add(`crab_arm_l_${k}`, 'RGB565A8', crabCrop, crabOrigin, expr, ops(6, 8), { source: 'drawCrab() left arm', clapT: k / 5 });
  add(`crab_arm_r_${k}`, 'RGB565A8', crabCrop, crabOrigin, expr, ops(9, 11), { source: 'drawCrab() right arm', clapT: k / 5 });
}
const pointAngles = [];
for (let a = -80; a <= 60; a += 5) pointAngles.push(a);
pointAngles.forEach((deg, k) => {
  const r = deg * Math.PI / 180;
  add(`crab_point_${k}`, 'RGB565A8', crabCrop, crabOrigin,
      crabBase + `crab.point = [42 + 100 * ${Math.cos(r)}, 222 + 100 * ${Math.sin(r)}]; drawCrab()`, ops(9, 11),
      { source: 'drawCrab() right arm pointing', angle_deg: deg });
});
const notPupil = '(m, c, i) => i >= 12 && !(m === "fill" && c.fillStyle === "#3b3550")';
for (const blink of [0, 1]) {
  for (const happy of [0, 1]) {
    add(`crab_body_${blink ? 'blink' : 'open'}_${happy ? 'happy' : 'calm'}`, 'RGB565A8', crabCrop, crabOrigin,
        crabBase + `crab.blink = ${blink ? -0.05 : 1}; crab.clap = ${happy ? 0.6 : 0}; drawCrab()`, notPupil,
        { source: 'drawCrab() body, eyes (no pupils), mouth, cheeks' });
  }
}
// Left pupil: look=[0,-1] puts it at crab-local (-8, -34.8).
add('crab_pupil', 'A8', [88, 81, 12, 12], [crabOrigin[0] - 8, crabOrigin[1] - 34.8],
    crabBase + 'drawCrab()', '(m, c) => m === "fill" && c.fillStyle === "#3b3550"',
    { source: 'drawCrab() pupil', recolor: '#3B3550' });

// ---- helper hand, chrome, start overlay ----
add('hand', 'RGB565A8', [148, 104, 52, 52], [174, 130],
    "st.phase='PLAY'; st.hint=2; st.target='RED'; bubbles=[{color:'RED', state:'float', x:160, y:100}]; time=0; drawHelperHand()",
    null, { source: 'drawHelperHand() at bob 0' });
add('chrome_home', 'RGB565A8', [4, 4, 48, 48], [4, 4], 'drawChrome()', null, { source: 'drawChrome() Home' });
add('chrome_mic', 'RGB565A8', [268, 4, 48, 48], [268, 4], 'drawChrome()', null, { source: 'drawChrome() disabled mic (alpha 0.45)' });
add('start_veil', 'RGB565A8', [0, 0, 1, 1], [0, 0], 'time = 0; drawStart()', '(m, c, i) => i === 0',
    { source: 'drawStart() veil colour (filled natively over 320x240)', pad: 0 });
add('start_button', 'RGB565A8', [118, 78, 84, 84], [160, 120], 'time = 0; drawStart()', '(m, c, i) => i >= 1',
    { source: 'drawStart() play button at scale 1' });

// ---- FX (drawFx): decor bubble rings, particle shapes ----
for (let k = 0; k <= 17; ++k) {
  const r = 1.5 + k * 0.5;
  add(`decor_${k}`, 'A8', [144, 104, 32, 32], [160, 120],
      `parts = []; rings = []; decor = [{x:160, y:120, r:${r}, vy:0, ph:0}]; drawFx()`, null,
      { source: 'drawFx() decor bubble', radius: r, recolor: '#FFFFFF' });
}
const part = (shape, size, rot) =>
  `decor = []; rings = []; parts = [{x:160, y:120, shape:'${shape}', size:${size}, rot:${rot}, life:1, max:1, color:'#FFFFFF', add:false}]; drawFx()`;
for (let k = 0; k <= 12; ++k) {
  const r = 1 + k * 0.5;
  add(`pcircle_${k}`, 'A8', [150, 110, 20, 20], [160, 120], part('circle', r, 0), null,
      { source: 'drawFx() circle particle', radius: r });
}
const starSizes = [3.5, 4, 4.5, 5, 5.5, 6, 6.5];
starSizes.forEach((size, si) => {
  for (let a = 0; a < 6; ++a) {
    add(`pstar_${si}_${a}`, 'A8', [150, 110, 20, 20], [160, 120], part('star', size, (a / 6) * (TAU / 5)), null,
        { source: 'drawFx() sparkle star', size, rot: (a / 6) * (TAU / 5) });
  }
});
const rectSizes = [4.5, 5.5, 6.5, 7.5];
rectSizes.forEach((size, si) => {
  for (let a = 0; a < 12; ++a) {
    add(`prect_${si}_${a}`, 'A8', [150, 110, 20, 20], [160, 120], part('rect', size, (a / 12) * Math.PI), null,
        { source: 'drawFx() confetti rect', size, rot: (a / 12) * Math.PI });
  }
});

// -------------------------------------------------------------------------------------
const browser = await chromium.launch({ channel: 'msedge', headless: true });
const results = [];
let browserVersion = '';
try {
  browserVersion = browser.version();
  const page = await browser.newPage({ viewport: { width: 400, height: 300 }, deviceScaleFactor: 1 });
  await page.addInitScript(() => {
    window.requestAnimationFrame = () => 0;
    // Layer filter hook: decides per fill()/stroke()/fillRect()/drawImage() call whether the
    // sketch's own drawing call reaches the canvas. Off (null) by default.
    window.__filter = null; window.__op = 0;
    const P = CanvasRenderingContext2D.prototype;
    for (const m of ['fill', 'stroke', 'fillRect', 'drawImage']) {
      const original = P[m];
      P[m] = function (...args) {
        if (window.__filter && this.canvas.id === 'c') {
          const index = window.__op++;
          if (m === 'drawImage' || !window.__filter(m, this, index)) return;
        }
        return original.apply(this, args);
      };
    }
  });
  const url = pathToFileURL(reference).href + '?seed=1234';
  for (const spec of specs) {
    await page.goto(url);
    const data = await page.evaluate(({ spec }) => {
      const cv = document.getElementById('c');
      cv.width = 320; cv.height = 240;
      const c = cv.getContext('2d');
      c.setTransform(1, 0, 0, 1, 0, 0);
      c.clearRect(0, 0, 320, 240);
      window.__op = 0;
      window.__filter = spec.filter ? (0, eval)('(' + spec.filter + ')') : (spec.id === 'bg' ? null : () => true);
      (0, eval)(spec.expr);
      window.__filter = null;
      let [x, y, w, h] = spec.crop;
      const full = c.getImageData(x, y, w, h).data;
      // Auto-crop to the drawn pixels (keeps the crop for opaque / single-pixel layers).
      let x0 = w, y0 = h, x1 = -1, y1 = -1;
      for (let j = 0; j < h; ++j) for (let i = 0; i < w; ++i) {
        if (full[(j * w + i) * 4 + 3]) { x0 = Math.min(x0, i); y0 = Math.min(y0, j); x1 = Math.max(x1, i); y1 = Math.max(y1, j); }
      }
      const edge = y0 === 0 || y1 === h - 1 || (!spec.meta.full_width && (x0 === 0 || x1 === w - 1));
      if (x1 < 0) { x0 = 0; y0 = 0; x1 = 0; y1 = 0; }
      if (spec.id !== 'bg') { x += x0; y += y0; w = x1 - x0 + 1; h = y1 - y0 + 1; }
      // Transparent margin for sprites drawn with LVGL scale/rotation: its transformed
      // blit loses the outermost source columns, which flattens a tightly cropped edge.
      const pad = spec.id === 'bg' || spec.meta.full_width ? 0 : (spec.meta.pad ?? 2);
      if (pad) {
        const nx = Math.max(0, x - pad), ny = Math.max(0, y - pad);
        w = Math.min(320, x + w + pad) - nx; h = Math.min(240, y + h + pad) - ny; x = nx; y = ny;
      }
      const out = document.createElement('canvas');
      out.width = w; out.height = h;
      const o = out.getContext('2d');
      o.drawImage(cv, x, y, w, h, 0, 0, w, h);
      const rgba = c.getImageData(x, y, w, h).data;
      let raw = '';
      for (let i = 0; i < rgba.length; i += 8192) raw += String.fromCharCode(...rgba.subarray(i, i + 8192));
      return { crop: [x, y, w, h], edge: spec.id !== 'bg' && spec.id !== 'start_veil' && edge,
               png: out.toDataURL('image/png').split(',')[1], rgba: btoa(raw) };
    }, { spec });
    if (data.edge) throw new Error(`${spec.id}: drawn pixels touch the search crop ${spec.crop}; widen it`);
    results.push({ spec, ...data });
  }

  // Contact sheet for review (each sprite on a checkerboard).
  const sheet = await page.evaluate(async ({ items }) => {
    const cell = 100, cols = 12, rows = Math.ceil(items.length / cols);
    const cv = document.createElement('canvas');
    cv.width = cols * cell; cv.height = rows * cell;
    const c = cv.getContext('2d');
    c.fillStyle = '#20243f'; c.fillRect(0, 0, cv.width, cv.height);
    for (let i = 0; i < items.length; ++i) {
      const x = (i % cols) * cell, y = Math.floor(i / cols) * cell;
      for (let j = 0; j < 9; ++j) for (let k = 0; k < 10; ++k) {
        c.fillStyle = (j + k) % 2 ? '#8a8fa8' : '#b9bdd0'; c.fillRect(x + 2 + k * 9.6, y + 2 + j * 9, 9.6, 9);
      }
      const img = new Image();
      img.src = 'data:image/png;base64,' + items[i].png;
      await img.decode();
      const s = Math.min(1, 94 / img.width, 80 / img.height) * (img.width < 40 && img.height < 40 ? 2 : 1);
      c.imageSmoothingEnabled = false;
      c.drawImage(img, x + 2, y + 2, img.width * s, img.height * s);
      c.fillStyle = '#fff'; c.font = '9px sans-serif'; c.fillText(items[i].id, x + 3, y + 96);
    }
    return cv.toDataURL('image/png').split(',')[1];
  }, { items: results.map(r => ({ id: r.spec.id, png: r.png })) });
  fs.writeFileSync(path.join(assetsDir, 'contact_sheet.png'), Buffer.from(sheet, 'base64'));
} finally {
  await browser.close();
}

// ---- convert + pack ----
const bayer = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]];
const q = (v, max, d) => Math.max(0, Math.min(max, Math.round((v / 255) * max + d)));
// Fill the RGB of fully transparent pixels from visible neighbours (a few passes) so
// bilinear sampling at scaled sprite edges doesn't pull in black.
function bleed(w, h, rgba, color) {
  const known = new Uint8Array(w * h);
  const rgb = new Float32Array(w * h * 3);
  for (let i = 0; i < w * h; ++i) if (rgba[i * 4 + 3]) {
    known[i] = 1; rgb[i * 3] = rgba[i * 4]; rgb[i * 3 + 1] = rgba[i * 4 + 1]; rgb[i * 3 + 2] = rgba[i * 4 + 2];
  }
  for (let pass = 0; pass < 4; ++pass) {
    const next = known.slice();
    for (let y = 0; y < h; ++y) for (let x = 0; x < w; ++x) {
      const i = y * w + x;
      if (known[i]) continue;
      let n = 0, r = 0, g = 0, b = 0;
      for (let dy = -1; dy <= 1; ++dy) for (let dx = -1; dx <= 1; ++dx) {
        const xx = x + dx, yy = y + dy;
        if (xx < 0 || yy < 0 || xx >= w || yy >= h || !known[yy * w + xx]) continue;
        const j = yy * w + xx; ++n; r += rgb[j * 3]; g += rgb[j * 3 + 1]; b += rgb[j * 3 + 2];
      }
      if (!n) continue;
      rgb[i * 3] = r / n; rgb[i * 3 + 1] = g / n; rgb[i * 3 + 2] = b / n; next[i] = 1;
      color.writeUInt16LE((q(r / n, 31, 0) << 11) | (q(g / n, 63, 0) << 5) | q(b / n, 31, 0), i * 2);
    }
    known.set(next);
  }
}

function encode(fmt, w, h, rgba, dither) {
  if (fmt === 'A8') {
    const out = Buffer.alloc(w * h);
    for (let i = 0; i < w * h; ++i) out[i] = rgba[i * 4 + 3];
    return { data: out, stride: w };
  }
  const color = Buffer.alloc(w * h * 2);
  for (let y = 0; y < h; ++y) for (let x = 0; x < w; ++x) {
    const i = y * w + x, d = dither ? (bayer[y & 3][x & 3] + 0.5) / 16 - 0.5 : 0;
    const v = (q(rgba[i * 4], 31, d) << 11) | (q(rgba[i * 4 + 1], 63, d) << 5) | q(rgba[i * 4 + 2], 31, d);
    color.writeUInt16LE(v, i * 2);
  }
  if (fmt === 'RGB565') return { data: color, stride: w * 2 };
  bleed(w, h, rgba, color);
  const alpha = Buffer.alloc(w * h);
  for (let i = 0; i < w * h; ++i) alpha[i] = rgba[i * 4 + 3];
  return { data: Buffer.concat([color, alpha]), stride: w * 2 };
}

const ENTRY = 52, HEADER = 32;
const entries = [], chunks = [], manifest = [];
let offset = HEADER + ENTRY * results.length;
for (const r of results) {
  const { spec } = r;
  const [x, y, w, h] = r.crop;
  const rgba = Buffer.from(r.rgba, 'base64');
  const { data, stride } = encode(spec.fmt, w, h, rgba, spec.id === 'bg');
  const pad = (4 - (offset % 4)) % 4;
  if (pad) { chunks.push(Buffer.alloc(pad)); offset += pad; }
  const ox = spec.origin[0] - x, oy = spec.origin[1] - y;
  entries.push({ id: spec.id, fmt: FMT[spec.fmt], w, h, stride, ox, oy, offset, size: data.length });
  chunks.push(data);
  offset += data.length;
  fs.writeFileSync(path.join(generated, spec.id + '.png'), Buffer.from(r.png, 'base64'));
  manifest.push({ id: spec.id, format: spec.fmt, width: w, height: h, bytes: data.length,
                  crop: [x, y, w, h], origin: [+ox.toFixed(3), +oy.toFixed(3)],
                  call: spec.expr, layer_filter: spec.filter, ...spec.meta });
}
const table = Buffer.alloc(ENTRY * entries.length);
entries.forEach((e, i) => {
  const o = i * ENTRY;
  if (e.id.length > 23) throw new Error(`name too long: ${e.id}`);
  table.write(e.id, o, 'ascii');
  table.writeUInt32LE(e.fmt, o + 24);
  table.writeUInt16LE(e.w, o + 28); table.writeUInt16LE(e.h, o + 30);
  table.writeUInt32LE(e.stride, o + 32);
  table.writeFloatLE(e.ox, o + 36); table.writeFloatLE(e.oy, o + 40);
  table.writeUInt32LE(e.offset, o + 44); table.writeUInt32LE(e.size, o + 48);
});
const payload = Buffer.concat([table, ...chunks]);
const signature = crypto.createHash('sha256').update(payload).digest().subarray(0, 16);
const header = Buffer.alloc(HEADER);
header.write('CPOP', 0, 'ascii');
header.writeUInt32LE(1, 4);
header.writeUInt32LE(entries.length, 8);
header.writeUInt32LE(HEADER + payload.length, 12);
signature.copy(header, 16);
const pack = Buffer.concat([header, payload]);
fs.writeFileSync(path.join(generated, 'color_pop.pack'), pack);

const deviceHeader = path.join(project, 'device', 'main', 'pack_signature.hpp');
fs.mkdirSync(path.dirname(deviceHeader), { recursive: true });
fs.writeFileSync(deviceHeader, `// Generated by tools/export_assets.mjs; do not edit.
#pragma once
#include <cstdint>
namespace color_pop::pack {
inline constexpr uint8_t signature[16] = {${[...signature].map(n => '0x' + n.toString(16).padStart(2, '0')).join(', ')}};
inline constexpr uint32_t bytes = ${pack.length}u;
inline constexpr uint32_t entries = ${entries.length}u;
}
`);

const byFormat = {};
for (const m of manifest) byFormat[m.format] = (byFormat[m.format] || 0) + m.bytes;
const sourceHash = crypto.createHash('sha256').update(fs.readFileSync(reference)).digest('hex');
fs.writeFileSync(path.join(assetsDir, 'manifest.json'), JSON.stringify({
  game: 'color_pop',
  source: referenceRel,
  source_sha256: sourceHash,
  exporter: 'v2/native/color_pop/tools/export_assets.mjs',
  renderer: 'Microsoft Edge (headless, playwright-core) Canvas 2D at 320x240, devicePixelRatio 1',
  logical_size: [320, 240],
  pack: { file: 'assets/generated/color_pop.pack', bytes: pack.length, signature: signature.toString('hex'),
          header_bytes: HEADER, entry_bytes: ENTRY,
          formats: { RGB565: 'LVGL RGB565 (bg dithered)', RGB565A8: 'LVGL RGB565A8: RGB565 plane then A8 plane',
                     A8: 'LVGL A8 alpha mask, recoloured at runtime' } },
  memory: { total_image_bytes: manifest.reduce((a, m) => a + m.bytes, 0), by_format: byFormat,
            device: 'memory-mapped from the assets flash partition; 0 bytes copied to PSRAM' },
  sprites: manifest,
}, null, 2) + '\n');
console.log(`Exported ${entries.length} sprites from ${referenceRel} (Edge ${browserVersion}); ` +
            `pack ${pack.length} bytes; by format ${JSON.stringify(byFormat)}`);
