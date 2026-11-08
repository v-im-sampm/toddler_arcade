// Exports Sound Jar artwork and voice takes by running the ORIGINAL code of
// v2/reference/sound_jar.html in headless Edge. Nothing is redrawn by hand: each sprite comes from
// one of the sketch's own draw functions (drawBackground, drawAmbient, drawBin, drawRecord, drawLid,
// drawJar, jarFace, drawPads, drawFxButtons, drawHelperHand, drawChrome, drawStart, drawFx, and the
// mote sub-path helpers) called with chosen state. A layer filter lets only selected fill()/stroke()
// calls through, which separates the moving parts. The built-in voice takes are rendered by the
// sketch's own renderTake() and exported as data.
// Outputs:
//   assets/generated/*.png              per-sprite previews (gitignored)
//   assets/generated/sound_jar.pack     runtime sprite pack for desktop + device (gitignored)
//   assets/generated/takes_data.cpp     take levels + 16 kHz PCM for gameplay and audio (gitignored)
//   device/main/pack_signature.hpp      firmware/asset match check (gitignored)
//   assets/manifest.json                provenance, crops, origins, formats, timing, bytes
//   assets/contact_sheet.png            visual review sheet
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
const referenceRel = 'v2/reference/sound_jar.html';
const reference = path.join(root, referenceRel);
const assetsDir = path.join(project, 'assets');
const generated = path.join(assetsDir, 'generated');
fs.mkdirSync(generated, { recursive: true });

const TAU = Math.PI * 2;
const FMT = { RGB565: 1, RGB565A8: 2, A8: 3 };
const specs = [];
// opts: filter (JS source of (method, ctx, opIndex) => bool), meta, noEdge (crop is a deliberate cut)
const add = (id, fmt, crop, origin, expr, filter = null, meta = {}, noEdge = false) =>
  specs.push({ id, fmt, crop, origin, expr, filter, meta, noEdge });

// Every spec starts from the same neutral scene state (the page is reused for a batch of specs).
const RESET = "st.phase='EMPTY'; st.t=0; st.fx=[]; st.hint=0; st.lev=0; st.amb=0; time=0; reduced=false; TOK.breathScale=0; " +
  "Object.assign(jar,{level:0,levelTo:0,mist:0,q:0,qv:0,ang:0,wig:0,happy:0,hmm:0,visor:0,visorTo:0,hum:0,blink:1,look:[0,0],swirl:0}); " +
  "lid.u=0; lid.to=0; recb.q=0; recb.louder=0; recb.wig=0; bin.q=0; bin.lid=0; parts=[]; rings=[]; svoices=[]; hold=null; " +
  "pads.forEach(p=>Object.assign(p,{q:0,glow:0,hop:9,awake:true,sing:0,bend:0,wig:0,blink:1})); " +
  "fxb.forEach(b=>Object.assign(b,{q:0,on:0,wig:0})); ";
const noFace = body => `{ const f = jarFace; jarFace = () => {}; try { ${body} } finally { jarFace = f; } }`;
// Ops of one object drawn under translate(x, ...): k-th op inside that object (by transform e).
const objOps = (x, sel, a1 = false) =>
  `(() => { let n = 0; return (m, c) => { if (Math.abs(c.getTransform().e - ${x}) > 0.01) return false; ` +
  `const k = n++; if (![${sel}].includes(k)) return false; ${a1 ? "c.globalAlpha = 1; c.globalCompositeOperation = 'source-over';" : ''} return true; }; })()`;
const ops = (sel, a1 = false) =>
  `(m, c, i) => { if (![${sel}].includes(i)) return false; ${a1 ? "c.globalAlpha = 1; c.globalCompositeOperation = 'source-over';" : ''} return true; }`;
const range = (a, b) => Array.from({ length: b - a + 1 }, (_, k) => a + k).join(',');

// ---- background: drawBackground() + the static contact shadows of bin, record, jar and pads ----
add('bg', 'RGB565', [0, 0, 320, 240], [0, 0],
    "drawBackground(); window.__filter = (m, c, i) => i === 0; " +
    "window.__op = 0; drawBin(); window.__op = 0; drawRecord(); window.__op = 0; drawJar(); window.__op = 0; drawPads();",
    '() => true', { source: 'drawBackground() + shadow op of drawBin/drawRecord/drawJar/drawPads', note: 'ordered 4x4 dither to RGB565' }, true);

// ---- ambience (drawAmbient) ----
add('speck_1', 'A8', [14, 39, 11, 11], [19, 44], 'drawAmbient()', ops('0', true),
    { source: 'drawAmbient() speck r=1', recolor: HUE('teal', 0), opa: '0.32+0.22 sin(time(0.7+0.17g)+1.9g)*m' });
add('speck_2', 'A8', [86, 79, 13, 13], [92, 85], 'drawAmbient()', ops('1', true),
    { source: 'drawAmbient() speck r=2', recolor: HUE('teal', 0) });
for (let k = 0; k < 8; ++k) {
  const rot = (k / 8) * (TAU / 5);
  let t = 2 * rot, n = 0;
  while (!((t % 4) / 4 > 0.3 && (t % 4) / 4 < 0.7)) t = 2 * rot + (++n) * (4 * Math.PI / 5);
  const gi = [1, 3, 6][Math.floor(t / 4) % 3];
  add(`glint_${k}`, 'A8', [0, 38, 320, 24], [gi * 47 - 3, 50], `time = ${t}; drawAmbient()`, ops('4', true),
      { source: 'drawAmbient() crystal glint star', frame: k, frames: 8, rot, recolor: HUE('purple', 0), blend: 'additive' }, true);
}
const T_FALL = Math.sqrt(2 * (157 - 56) / 900);
for (let k = 0; k < 8; ++k) {
  const kk = (k + 0.5) / 8, u = -1.1 + kk * (1.1 - T_FALL);
  add(`drip_swell_${k}`, 'A8', [81, 48, 20, 20], [91, 56 + 2 * kk], `st.amb = ${1.8 + u}; drawAmbient()`, ops('4', true),
      { source: 'drawAmbient() drop swelling at the crystal tip', k: kk, recolor: HUE('teal', 0), opa: 0.85 });
}
add('drip_fall', 'A8', [81, 129, 20, 20], [91, 139], `st.amb = ${1.8 - 0.2}; drawAmbient()`, ops('4', true),
    { source: 'drawAmbient() falling drop', recolor: HUE('teal', 0), opa: 0.85 });
for (let k = 0; k < 12; ++k) {
  const kk = (k + 0.5) / 12;
  add(`drip_ring_${k}`, 'A8', [66, 148, 50, 20], [91, 158], `st.amb = ${1.8 + kk * 0.45}; drawAmbient()`, ops('4', true),
      { source: 'drawAmbient() splash ring', k: kk, recolor: HUE('teal', 0), opa: '0.6(1-k)' });
}
add('drip_drop', 'A8', [81, 146, 9, 10], [91 - 7 * 0.75, 157 - Math.sin(0.75 * Math.PI) * 9],
    `st.amb = ${1.8 + 0.75 * 0.45}; drawAmbient()`, ops('5', true),
    { source: 'drawAmbient() splash droplet (left one)', recolor: HUE('teal', 0), pad: 1 });

// ---- bin, record button, lid ----
add('bin_body', 'RGB565A8', [14, 100, 60, 64], [44, 157], 'drawBin()', ops(range(1, 4)), { source: 'drawBin() body, ribs, highlight' });
add('bin_lid', 'RGB565A8', [14, 92, 60, 30], [65, 110], 'drawBin()', ops(range(5, 7)), { source: 'drawBin() lid + handle (hinge pivot)' });
add('rec_body', 'RGB565A8', [232, 92, 72, 72], [268, 156], 'drawRecord()', '(m, c, i) => i >= 1', { source: 'drawRecord() button + mic glyph' });
add('rec_dot_lit', 'RGB565A8', [244, 76, 20, 20], [254, 86], "st.phase = 'RECORDING'; drawRecord()", ops('1,2'),
    { source: 'drawRecord() lit countdown dot (scaled by easeOutBack natively)' });
add('rec_dot_dim', 'RGB565A8', [244, 76, 20, 20], [254, 86], "st.phase = 'COUNTDOWN'; st.t = 0; drawRecord()", ops('1'),
    { source: 'drawRecord() unlit countdown dot' });
for (let r = 34; r <= 52; ++r) {
  add(`rec_arc_${r}`, 'A8', [204, 80, 128, 96], [268, 128],
      `st.phase = 'RECORDING'; st.lev = 0.12; REC.r = ${r - 6}; try { drawRecord(); } finally { REC.r = 28; }`, ops('7', true),
      { source: 'drawRecord() level / louder arcs (one ring)', radius: r, recolor: 'HUE.red[0] or TOK.hintGlow' });
}
add('lid', 'RGB565A8', [124, 52, 72, 28], [160, 70], "st.phase = 'START'; drawLid(false)", null, { source: 'drawLid() closed pose' });
add('lid_glow', 'A8', [124, 46, 72, 36], [160, 70], 'drawLid(false)', ops('0', true),
    { source: 'drawLid() EMPTY invitation glow', recolor: HUE('teal', 0), blend: 'additive', opa: '0.22+0.12 sin(2.2 time)' });

// ---- jar (drawJar / jarFace): layers in jar-local space, origin = jar foot (160, 156) ----
const FOOT = [160, 156];
const jarCrop = [100, 44, 120, 120];
add('jar_back', 'RGB565A8', jarCrop, FOOT, noFace('lid.u = 0.5; drawJar();'), ops('1'), { source: 'drawJar() glass back gradient' });
add('jar_front', 'RGB565A8', jarCrop, FOOT, noFace('lid.u = 0.5; drawJar();'), ops(range(2, 5)),
    { source: 'drawJar() glass edge, highlights, rim' });
add('visor_body', 'RGB565A8', jarCrop, FOOT, noFace('jar.visor = 1; drawJar();'), ops(range(2, 5)),
    { source: 'drawJar() robot visor at visor=1 (slid by vy natively)' });
add('visor_bar', 'A8', [150, 112, 20, 16], FOOT, noFace('jar.visor = 1; drawJar();'), ops('6', true),
    { source: 'drawJar() visor scan bar at x=0', recolor: HUE('teal', 0), opa: '0.6 min(1, 1.5 visor)' });
const MIST = '(m, c) => m === "fill" && Math.abs(c.globalAlpha - 0.9) < 1e-3';
const SWIRL = '(m, c) => m === "stroke" && c.lineWidth === 2.5 && Math.abs(c.globalAlpha - 0.55) < 1e-3';
const SHEEN = '(m, c) => m === "stroke" && c.lineWidth === 1.5 && Math.abs(c.globalAlpha - 0.55) < 1e-3';
const LEVELS = 33, PHASES = 8, HUES = 5, SWIRL_LEVELS = 17, SWIRL_ANGLES = 16;
const capRows = L => {
  const ys = 120 + 32 - 64 * L, amp = Math.min(1.6, L * 6);
  return [Math.floor(ys - amp - 1.5), Math.ceil(ys + amp + 1.5)];
};
for (let li = 1; li <= LEVELS; ++li) {
  const L = li * 0.02, [c0, c1] = capRows(L);
  for (let h = 0; h < HUES; ++h) {
    add(`mist_${h}_${li}`, 'RGB565A8', [104, c1, 112, 160 - c1], FOOT, noFace(`jar.mist = ${h}; jar.level = ${L}; drawJar();`), MIST,
        { source: 'drawJar() mist body below the wave band', hue: h, level: L }, true);
  }
  for (const red of [0, 1]) {
    for (let k = 0; k < PHASES; ++k) {
      const t = (k / PHASES) * (TAU / 2.4);
      const st = `reduced = ${!!red}; time = ${t}; jar.level = ${L}; drawJar();`;
      add(`cap_${red}_${li}_${k}`, 'A8', [104, c0, 112, c1 - c0], FOOT, noFace(st), MIST,
          { source: 'drawJar() mist wave band', level: L, phase: k, reduced: !!red, recolor: 'mist gradient at the band' }, true);
      add(`sheen_${red}_${li}_${k}`, 'A8', [104, c0 - 3, 112, c1 - c0 + 6], FOOT, noFace(st), SHEEN,
          { source: 'drawJar() surface sheen', level: L, phase: k, reduced: !!red, recolor: '#FFFFFF' });
    }
  }
}
for (let li = 1; li <= SWIRL_LEVELS; ++li) {
  for (let k = 0; k < SWIRL_ANGLES; ++k) {
    add(`swirl_${li}_${k}`, 'A8', [104, 60, 112, 100], FOOT,
        noFace(`jar.level = ${li * 0.04}; jar.swirl = ${(k / SWIRL_ANGLES) * Math.PI}; drawJar();`), SWIRL,
        { source: 'drawJar() mist swirls', level: li * 0.04, angle: (k / SWIRL_ANGLES) * Math.PI, recolor: 'hue[0]' });
  }
}
// Motes: drawJar's mote sub-paths (heartSub / starSub / arcs) at its sizes, alpha 0.8.
const MOTE_S = Array.from({ length: 8 }, (_, k) => 0.15 + k * 0.85 / 7);
const moteExpr = body => "ctx.globalAlpha = 0.8; ctx.fillStyle = '#FFFFFF'; ctx.strokeStyle = '#FFFFFF'; ctx.lineWidth = 1.2; ctx.beginPath(); " + body;
MOTE_S.forEach((s, k) => {
  add(`mote_bubble_${k}`, 'A8', [150, 110, 20, 20], [160, 120],
      moteExpr(`ctx.moveTo(160 + ${2.6 * s}, 120); ctx.arc(160, 120, ${2.6 * s}, 0, 7); ctx.stroke();`), null, { source: 'drawJar() bubble mote', s });
  add(`mote_dot_${k}`, 'A8', [150, 110, 20, 20], [160, 120],
      moteExpr(`ctx.moveTo(160 + ${1.8 * s}, 120); ctx.arc(160, 120, ${1.8 * s}, 0, 7); ctx.fill();`), null, { source: 'drawJar() dot mote', s });
  add(`mote_heart_${k}`, 'A8', [150, 110, 20, 20], [160, 120], moteExpr(`heartSub(160, 120, ${3.6 * s}); ctx.fill();`), null,
      { source: 'drawJar() heartSub mote', s });
  for (let a = 0; a < 6; ++a) {
    const rot = (a / 6) * (TAU / 5);
    add(`mote_star_${k}_${a}`, 'A8', [150, 110, 20, 20], [160, 120],
        moteExpr(`starSub(160, 120, ${3.8 * s}, ${1.6 * s}, ${rot}); ctx.fill();`), null, { source: 'drawJar() starSub mote', s, rot });
  }
});
// Face (jarFace(mood, mouth) at the jar's translate(0, 2)).
const face = (mood, mouth = 0) => `ctx.translate(160, 122); jarFace('${mood}', ${mouth});`;
const faceCrop = [116, 92, 88, 52];
add('face_cheeks', 'RGB565A8', faceCrop, FOOT, face('idle'), ops('0'), { source: 'jarFace() cheeks' });
add('eyes_open_s', 'RGB565A8', faceCrop, FOOT, face('idle'), ops('1,2'), { source: 'jarFace() eye whites er=6.5' });
add('eyes_open_l', 'RGB565A8', faceCrop, FOOT, face('listen'), ops('1,2'), { source: 'jarFace() eye whites er=7.5' });
add('pupils_s', 'RGB565A8', faceCrop, FOOT, face('idle'), ops('3,4'), { source: 'jarFace() pupils at look=0 (offset natively)' });
add('pupils_l', 'RGB565A8', faceCrop, FOOT, face('listen'), ops('3,4'), { source: 'jarFace() pupils looking at the record button' });
add('eyes_shut', 'RGB565A8', faceCrop, FOOT, face('sleep'), ops('1'), { source: 'jarFace() closed eyes (sleep / blink)' });
add('eyes_happy', 'RGB565A8', faceCrop, FOOT, face('happy'), ops('1'), { source: 'jarFace() happy eyes' });
add('brows', 'RGB565A8', faceCrop, FOOT, face('hmm'), ops('5'), { source: 'jarFace() hmm brows' });
add('mouth_hmm', 'RGB565A8', faceCrop, FOOT, face('hmm'), ops('6'), { source: 'jarFace() hmm mouth' });
add('mouth_o', 'RGB565A8', faceCrop, [160, 131], face('listen'), ops('5'), { source: 'jarFace() "o" mouth (hum scale natively)' });
add('mouth_happy', 'RGB565A8', faceCrop, FOOT, face('happy'), ops('2'), { source: 'jarFace() happy mouth' });
add('mouth_sleep', 'RGB565A8', faceCrop, FOOT, face('sleep'), ops('2'), { source: 'jarFace() sleep mouth' });
add('mouth_smile', 'RGB565A8', faceCrop, FOOT, face('idle'), ops('5'), { source: 'jarFace() idle smile' });
for (let k = 0; k < 16; ++k) {
  add(`mouth_sing_${k}`, 'RGB565A8', faceCrop, FOOT, face('idle', (k + 1) / 16), ops('5,6'),
      { source: 'jarFace() singing mouth', mouth: (k + 1) / 16 });
}

// ---- pads (drawPads): per pad, origin = pad foot (x, 232) ----
for (let i = 0; i < 5; ++i) {
  const x = 32 + 64 * i, P = 2.6 + 0.12 * i, n = Math.ceil(i / Math.PI);
  const t = (n * Math.PI - i) * P / TAU;                 // breathing term = 0
  const crop = [x - 34, 168, 68, 70], O = [x, 232];
  const pad = extra => `time = ${t}; ${extra} drawPads();`;
  add(`pad${i}_body`, 'RGB565A8', crop, O, pad(''), objOps(x, '0,1,2'), { source: 'drawPads() body gradient, outline, highlight', pad: i });
  add(`pad${i}_eyes`, 'RGB565A8', crop, O, pad(''), objOps(x, '3,4'), { source: 'padFace() open eyes', pad: i });
  add(`pad${i}_smile`, 'RGB565A8', crop, O, pad(''), objOps(x, '5'), { source: 'padFace() smile', pad: i });
  add(`pad${i}_shut`, 'RGB565A8', crop, O, pad(`pads[${i}].awake = false;`), objOps(x, '3'), { source: 'padFace() closed eyes', pad: i });
  add(`pad${i}_cheer`, 'RGB565A8', crop, O, pad(`pads[${i}].hop = 0.5;`), objOps(x, '3'), { source: 'padFace() cheering eyes', pad: i });
  add(`pad${i}_glow`, 'A8', [x - 50, 150, 100, 90], O, pad(`pads[${i}].glow = 1;`), objOps(x, '0', true),
      { source: 'drawPads() glow disc', pad: i, recolor: 'hue[0]', blend: 'additive', opa: '0.35 glow' }, true);
  for (let k = 0; k < 8; ++k) {
    add(`pad${i}_sing_${k}`, 'RGB565A8', crop, O, pad(`pads[${i}].sing = ${0.15 * (k + 1) / 8};`), objOps(x, '5'),
        { source: 'padFace() singing mouth', pad: i, sing: 0.15 * (k + 1) / 8 });
  }
}

// ---- effect buttons (drawFxButtons / fxIcon) ----
const FXX = [82, 134, 186, 238];
const fxY = (i, t) => 30 + Math.sin(t * 2.1 + i * 1.7) * 2;
const fxSpec = (id, i, t, on, sel, fmt, meta) =>
  add(id, fmt, [FXX[i] - 30, 0, 60, 60], [FXX[i], fxY(i, t)], `time = ${t}; fxb[${i}].on = ${on}; drawFxButtons();`,
      objOps(FXX[i], sel, fmt === 'A8'), meta);
fxSpec('fxbody_off', 0, 0, 0, '0,1', 'RGB565A8', { source: 'drawFxButtons() off body r=21' });
for (let i = 0; i < 4; ++i) {
  const iconOps = ['2,3', '2,3,4', '2,3,4,5,6', '2'][i];
  fxSpec(`fx${i}_body`, i, 0, 1, '0,1', 'RGB565A8', { source: 'drawFxButtons() on body r=22.68', fx: i });
  fxSpec(`fx${i}_icon`, i, 0, 0, iconOps, 'RGB565A8', { source: 'fxIcon() off', fx: i });
}
for (let k = 0; k <= 8; ++k) {
  const t = Math.asin(k / 8) / (Math.PI * 1.2);
  fxSpec(`fx0_on_${k}`, 0, t, 1, '2,3', 'A8', { source: 'fxIcon(BACK) on', dx: -2 * k / 8, recolor: '#FFFFFF' });
}
fxSpec('fx1_dot', 1, 0, 1, '2', 'A8', { source: 'fxIcon(ECHO) dot', recolor: '#FFFFFF' });
fxSpec('fx1_ring0', 1, 0, 1, '3', 'A8', { source: 'fxIcon(ECHO) ring 0', recolor: '#FFFFFF', opa: '0.6+0.4 sin(5 time m)' });
fxSpec('fx1_ring1', 1, 0, 1, '4', 'A8', { source: 'fxIcon(ECHO) ring 1', recolor: '#FFFFFF', opa: '0.6+0.4 sin(5 time m - 1.4)' });
for (let k = 0; k <= 8; ++k) {
  const by = -1.2 + 0.3 * k, t = Math.asin(by / 1.2) / 6;
  fxSpec(`fx2_on_${k}`, 2, t, 1, '2,3,4,5,6', 'A8', { source: 'fxIcon(ROBOT) on', by, recolor: '#FFFFFF' });
}
for (let k = 0; k < 16; ++k) {
  fxSpec(`fx3_on_${k}`, 3, (k / 16) * TAU / 5, 1, '2', 'A8', { source: 'fxIcon(WOBBLE) on', phase: (k / 16) * TAU, recolor: '#FFFFFF' });
}

// ---- helper hand, chrome, start overlay ----
add('hand', 'RGB565A8', [144, 92, 52, 52], [168, 116],
    "{ const f = suggest; suggest = () => ({ x: 160, y: 100, r: 20, o: {} }); st.hint = 2; try { drawHelperHand(); } finally { suggest = f; } }",
    null, { source: 'drawHelperHand() at bob 0' });
add('chrome_home', 'RGB565A8', [4, 4, 48, 48], [4, 4], 'drawChrome()', null, { source: 'drawChrome() Home' });
add('chrome_mic', 'RGB565A8', [268, 4, 48, 48], [268, 4], 'drawChrome()', null, { source: 'drawChrome() disabled mic (alpha 0.45)' });
add('start_veil', 'RGB565A8', [0, 0, 1, 1], [0, 0], 'drawStart()', '(m, c, i) => i === 0',
    { source: 'drawStart() veil colour (filled natively over 320x240)', pad: 0 }, true);
add('start_button', 'RGB565A8', [118, 78, 84, 84], [160, 120], 'drawStart()', '(m, c, i) => i >= 1',
    { source: 'drawStart() play button at scale 1' });

// ---- particles (drawFx) ----
const part = (shape, size, rot) =>
  `parts = [{x:160, y:120, shape:'${shape}', size:${size}, rot:${rot}, life:1, max:1, color:'#FFFFFF', add:false}]; drawFx()`;
for (let k = 0; k <= 12; ++k) {
  const r = 1 + k * 0.5;
  add(`pcircle_${k}`, 'A8', [150, 110, 20, 20], [160, 120], part('circle', r, 0), null, { source: 'drawFx() circle particle', radius: r });
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
const noteSizes = [3, 3.5, 4, 4.5, 5];
noteSizes.forEach((size, si) => {
  add(`pnote_${si}`, 'A8', [148, 104, 24, 24], [160, 120], part('note', size, 0), null, { source: 'drawFx() note particle', size });
});

function HUE(name, k) {
  const H = { teal: ['#8FE3DE', '#21B3B0', '#127A7D'], purple: ['#C9AEF5', '#8F5BE0', '#6236A6'] };
  return H[name][k];
}

// -------------------------------------------------------------------------------------
const browser = await chromium.launch({ channel: 'msedge', headless: true });
const results = [];
let browserVersion = '', takes = null;
const BATCH = 120;
try {
  browserVersion = browser.version();
  const page = await browser.newPage({ viewport: { width: 400, height: 300 }, deviceScaleFactor: 1 });
  await page.addInitScript(() => {
    window.requestAnimationFrame = () => 0;
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
  await page.goto(url);
  takes = await page.evaluate(() => (0, eval)('TAKES').map(t => ({
    name: t.name, peak: t.peak, mote: t.mote, hue: t.hue, dur: t.dur, lev: Array.from(t.lev), playLev: t.playLev,
    pcm: Array.from(t.pcm, v => Math.max(-32768, Math.min(32767, Math.round(v * 32767)))) })));
  for (let b = 0; b < specs.length; b += BATCH) {
    await page.goto(url);
    const batch = specs.slice(b, b + BATCH).map(s => ({ ...s, expr: RESET + s.expr }));
    const out = await page.evaluate(({ batch }) => {
      const g = eval;
      const cv = document.getElementById('c');
      cv.width = 320; cv.height = 240;
      const c = cv.getContext('2d');
      const res = [];
      for (const spec of batch) {
        c.setTransform(1, 0, 0, 1, 0, 0);
        c.globalAlpha = 1; c.globalCompositeOperation = 'source-over';
        c.clearRect(0, 0, 320, 240);
        c.save();
        window.__op = 0;
        window.__filter = spec.filter ? g('(' + spec.filter + ')') : () => true;
        g(spec.expr);
        window.__filter = null;
        c.restore();
        const [sx, sy, sw, sh] = spec.crop;
        const full = c.getImageData(sx, sy, sw, sh).data;
        let x0 = sw, y0 = sh, x1 = -1, y1 = -1;
        for (let j = 0; j < sh; ++j) for (let i = 0; i < sw; ++i) {
          if (full[(j * sw + i) * 4 + 3]) { x0 = Math.min(x0, i); y0 = Math.min(y0, j); x1 = Math.max(x1, i); y1 = Math.max(y1, j); }
        }
        const edge = !spec.noEdge && x1 >= 0 && (x0 === 0 || y0 === 0 || x1 === sw - 1 || y1 === sh - 1);
        if (spec.id === 'bg') { x0 = 0; y0 = 0; x1 = sw - 1; y1 = sh - 1; }
        const empty = x1 < 0;
        if (empty) { x0 = 0; y0 = 0; x1 = 0; y1 = 0; }
        // Transparent margin (not canvas content: split layers must not pick up their neighbours).
        const pad = spec.id === 'bg' ? 0 : (spec.meta.pad ?? 2);
        const w = x1 - x0 + 1 + 2 * pad, h = y1 - y0 + 1 + 2 * pad;
        const rgba = new Uint8ClampedArray(w * h * 4);
        if (!empty) for (let j = y0; j <= y1; ++j) for (let i = x0; i <= x1; ++i) {
          const s = (j * sw + i) * 4, d = ((j - y0 + pad) * w + (i - x0 + pad)) * 4;
          rgba[d] = full[s]; rgba[d + 1] = full[s + 1]; rgba[d + 2] = full[s + 2]; rgba[d + 3] = full[s + 3];
        }
        let png = '';
        if (w * h <= 320 * 240) {
          const o = document.createElement('canvas'); o.width = w; o.height = h;
          o.getContext('2d').putImageData(new ImageData(rgba, w, h), 0, 0);
          png = o.toDataURL('image/png').split(',')[1];
        }
        let raw = '';
        for (let i = 0; i < rgba.length; i += 8192) raw += String.fromCharCode(...rgba.subarray(i, i + 8192));
        res.push({ crop: [sx + x0 - pad, sy + y0 - pad, w, h], edge, empty, png, rgba: btoa(raw) });
      }
      return res;
    }, { batch });
    out.forEach((data, k) => {
      const spec = specs[b + k];
      if (data.edge) throw new Error(`${spec.id}: drawn pixels touch the search crop ${spec.crop}; widen it`);
      results.push({ spec, ...data });
    });
    process.stdout.write(`\r  rendered ${results.length}/${specs.length}`);
  }
  process.stdout.write('\n');
  const empties = results.filter(r => r.empty && !/^mist_/.test(r.spec.id)).map(r => r.spec.id);
  if (empties.length) throw new Error(`empty sprites (layer filter selected nothing): ${empties.join(', ')}`);

  // Contact sheet: one representative of each animated family (all frames are in assets/generated/).
  const rep = results.filter(r => !/^(mist_\d_(?!17)|cap_|sheen_(?!0_17_0)|swirl_(?!9_0)|mote_star_\d_[1-5]|pstar_\d_[1-5]|prect_\d_([1-9]|1\d)|pad\d_sing_[0-6]|mouth_sing_([0-9]|1[0-4])$|rec_arc_(?!34|51)|fx\d_on_([1-7]|9|1\d)|drip_(swell|ring)_([1-6]|[89]|1\d))/.test(r.spec.id));
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
      if (!items[i].png) continue;
      const img = new Image();
      img.src = 'data:image/png;base64,' + items[i].png;
      await img.decode();
      const s = Math.min(1, 94 / img.width, 80 / img.height) * (img.width < 40 && img.height < 40 ? 2 : 1);
      c.imageSmoothingEnabled = false;
      c.drawImage(img, x + 2, y + 2, img.width * s, img.height * s);
      c.fillStyle = '#fff'; c.font = '9px sans-serif'; c.fillText(items[i].id, x + 3, y + 96);
    }
    return cv.toDataURL('image/png').split(',')[1];
  }, { items: rep.map(r => ({ id: r.spec.id, png: r.png })) });
  fs.writeFileSync(path.join(assetsDir, 'contact_sheet.png'), Buffer.from(sheet, 'base64'));
} finally {
  await browser.close();
}

// ---- convert + pack ----
const bayer = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]];
const q = (v, max, d) => Math.max(0, Math.min(max, Math.round((v / 255) * max + d)));
// Fill the RGB of fully transparent pixels from visible neighbours so bilinear sampling at scaled
// sprite edges doesn't pull in black.
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
for (const old of fs.readdirSync(generated)) if (old.endsWith('.png')) fs.unlinkSync(path.join(generated, old));
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
  if (r.png) fs.writeFileSync(path.join(generated, spec.id + '.png'), Buffer.from(r.png, 'base64'));
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
header.write('SJAR', 0, 'ascii');
header.writeUInt32LE(1, 4);
header.writeUInt32LE(entries.length, 8);
header.writeUInt32LE(HEADER + payload.length, 12);
signature.copy(header, 16);
const pack = Buffer.concat([header, payload]);
fs.writeFileSync(path.join(generated, 'sound_jar.pack'), pack);

const deviceHeader = path.join(project, 'device', 'main', 'pack_signature.hpp');
fs.mkdirSync(path.dirname(deviceHeader), { recursive: true });
fs.writeFileSync(deviceHeader, `// Generated by tools/export_assets.mjs; do not edit.
#pragma once
#include <cstdint>
namespace sound_jar::pack {
inline constexpr uint8_t signature[16] = {${[...signature].map(n => '0x' + n.toString(16).padStart(2, '0')).join(', ')}};
inline constexpr uint32_t bytes = ${pack.length}u;
inline constexpr uint32_t entries = ${entries.length}u;
}
`);

// ---- takes: levels (exact doubles, drive gameplay) + int16 PCM (drives the sampler) ----
const num = v => (Number.isInteger(v) ? v.toFixed(1) : String(v));
let cpp = `// Generated by v2/native/sound_jar/tools/export_assets.mjs from ${referenceRel} renderTake(); do not edit.
#include "game/takes.hpp"

namespace sound_jar {
namespace {
`;
let pcmBytes = 0;
takes.forEach((t, i) => {
  cpp += `const double kLev${i}[] = {${t.lev.map(num).join(', ')}};\n`;
  cpp += `const double kPlayLev${i}[] = {${t.playLev.map(num).join(', ')}};\n`;
  cpp += `const int16_t kPcm${i}[] = {${t.pcm.join(',')}};\n`;
  pcmBytes += t.pcm.length * 2;
});
cpp += `}  // namespace\n\nconst TakeData kTakeData[kTakeCount] = {\n`;
takes.forEach((t, i) => {
  cpp += `    {"${t.name}", ${num(t.dur)}, kLev${i}, kPlayLev${i}, ${t.lev.length}, kPcm${i}, ${t.pcm.length}},\n`;
});
cpp += '};\n\n}  // namespace sound_jar\n';
fs.writeFileSync(path.join(generated, 'takes_data.cpp'), cpp);

const byFormat = {};
for (const m of manifest) byFormat[m.format] = (byFormat[m.format] || 0) + m.bytes;
const sourceHash = crypto.createHash('sha256').update(fs.readFileSync(reference)).digest('hex');
fs.writeFileSync(path.join(assetsDir, 'manifest.json'), JSON.stringify({
  game: 'sound_jar',
  source: referenceRel,
  source_sha256: sourceHash,
  exporter: 'v2/native/sound_jar/tools/export_assets.mjs',
  renderer: `Microsoft Edge ${browserVersion} (headless, playwright-core) Canvas 2D at 320x240, devicePixelRatio 1`,
  logical_size: [320, 240],
  frame_timing: {
    mist_wave: `${PHASES} phase frames over 2*pi/2.4 s (time*2.4), x2 for reduced motion (half amplitude)`,
    mist_levels: `${LEVELS} levels, step 0.02 (1.28 px of fill height)`,
    swirl: `${SWIRL_LEVELS} levels (step 0.04) x ${SWIRL_ANGLES} angles over pi (two arcs, period pi)`,
    glint: '8 rotation frames over 2*pi/5', motes: '8 sizes; stars 6 rotations over 2*pi/5',
    pad_sing: '8 mouth frames over sing 0..0.15', jar_sing: '16 mouth frames over mouth 0..1',
    fx_icons: 'BACK 9 dx frames, ROBOT 9 antenna frames, WOBBLE 16 phase frames, ECHO rings via opacity',
    drips: '8 swell frames, 12 splash frames',
  },
  takes: { file: 'assets/generated/takes_data.cpp', sample_rate: 16000,
           pcm: 'renderTake() pcm (peak 0.5) as int16', pcm_bytes: pcmBytes,
           list: takes.map(t => ({ name: t.name, dur_s: t.dur, samples: t.pcm.length, level_frames: t.lev.length })) },
  pack: { file: 'assets/generated/sound_jar.pack', bytes: pack.length, signature: signature.toString('hex'),
          header_bytes: HEADER, entry_bytes: ENTRY, sprites: entries.length,
          formats: { RGB565: 'LVGL RGB565 (bg dithered)', RGB565A8: 'LVGL RGB565A8: RGB565 plane then A8 plane',
                     A8: 'LVGL A8 alpha mask, recoloured at runtime' } },
  memory: { total_image_bytes: manifest.reduce((a, m) => a + m.bytes, 0), by_format: byFormat,
            device: 'memory-mapped from the assets flash partition; 0 bytes copied to PSRAM; takes PCM in app rodata' },
  sprites: manifest,
}, null, 2) + '\n');
console.log(`Exported ${entries.length} sprites from ${referenceRel} (Edge ${browserVersion}); ` +
            `pack ${pack.length} bytes; by format ${JSON.stringify(byFormat)}; takes PCM ${pcmBytes} bytes`);
