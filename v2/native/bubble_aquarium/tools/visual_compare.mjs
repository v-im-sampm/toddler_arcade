import { createRequire } from 'node:module';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..', '..', '..', '..');
const require = createRequire(path.join(root, 'tools', 'quality_bar_golden', 'package.json'));
const { chromium } = require('playwright-core');
const exe = process.argv[2] || path.join(root, 'build-v2-native', 'bubble_aquarium', 'Release',
                                        'bubble_aquarium_native.exe');
const out = process.argv[3] || path.join(root, 'build-v2-native', 'bubble_aquarium', 'comparison');
fs.mkdirSync(out, { recursive: true });
const nativeFile = path.join(out, 'native.bmp');
console.log(execFileSync(exe, ['--check', '--seed', '1234', '--screenshot', nativeFile], { encoding: 'utf8' }).trim());

const browser = await chromium.launch({ channel: 'msedge', headless: true });
let ref;
try {
  const page = await browser.newPage({ viewport: { width: 400, height: 300 }, deviceScaleFactor: 1 });
  await page.addInitScript(() => { window.requestAnimationFrame = () => 0; });
  await page.goto(pathToFileURL(path.join(root, 'v2', 'reference', 'bubble_aquarium.html')).href + '?seed=1234');
  ref = await page.evaluate(() => {
    const cv = document.getElementById('c');
    cv.width = 320; cv.height = 240;
    const ctx = cv.getContext('2d');
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    startGame();
    for (let i = 0; i < 50; ++i) {
      if (i === 32) shake();
      update(0.05);
    }
    blowBubbles(); toggleBed();
    for (let i = 0; i < 40; ++i) update(0.05);
    drawScene();
    const pixels = ctx.getImageData(0, 0, 320, 240).data;
    const rgb = new Uint8Array(320 * 240 * 3);
    for (let i = 0, j = 0; i < pixels.length; i += 4) {
      rgb[j++] = pixels[i]; rgb[j++] = pixels[i + 1]; rgb[j++] = pixels[i + 2];
    }
    let bytes = '';
    for (let i = 0; i < rgb.length; i += 8192) bytes += String.fromCharCode(...rgb.subarray(i, i + 8192));
    return { png: cv.toDataURL('image/png').split(',')[1], rgb: btoa(bytes),
             meals: st.meals, bubbles: bubbles.length };
  });
} finally {
  await browser.close();
}
fs.writeFileSync(path.join(out, 'reference.png'), Buffer.from(ref.png, 'base64'));
const rgb = Buffer.from(ref.rgb, 'base64');
const bmp = fs.readFileSync(nativeFile);
if (bmp.toString('ascii', 0, 2) !== 'BM' || bmp.readUInt32LE(18) !== 320 ||
    bmp.readUInt32LE(22) !== 240 || bmp.readUInt16LE(28) !== 24)
  throw new Error('Unexpected native screenshot format');
const offset = bmp.readUInt32LE(10), stride = 320 * 3;
let total = 0, bad = 0;
for (let y = 0; y < 240; ++y) {
  for (let x = 0; x < 320; ++x) {
    const a = (y * 320 + x) * 3, b = offset + (239 - y) * stride + x * 3;
    const dr = Math.abs(rgb[a] - bmp[b + 2]), dg = Math.abs(rgb[a + 1] - bmp[b + 1]);
    const db = Math.abs(rgb[a + 2] - bmp[b]);
    total += dr + dg + db;
    if (Math.max(dr, dg, db) > 32) ++bad;
  }
}
const report = { scenario: '2.5s start + feed; burst + bedtime; 2s settle', seed: 1234,
                 width: 320, height: 240, referenceMeals: ref.meals, referenceBubbles: ref.bubbles,
                 meanAbsoluteRGB: +(total / (320 * 240 * 3)).toFixed(2),
                 pixelsOver32Percent: +(bad / (320 * 240) * 100).toFixed(2),
                 reference: 'reference.png', native: 'native.bmp' };
fs.writeFileSync(path.join(out, 'report.json'), JSON.stringify(report, null, 2) + '\n');
fs.writeFileSync(path.join(out, 'comparison.html'),
  '<title>Bubble Aquarium: original vs LVGL</title><style>body{background:#1E2245;color:white;font:16px sans-serif}img{width:640px;image-rendering:pixelated}</style>' +
  '<h1>Original Canvas / LVGL 9 native</h1><img src="reference.png"><img src="native.bmp">' +
  `<pre>${JSON.stringify(report, null, 2)}</pre>`);
console.log(JSON.stringify(report));
if (report.meanAbsoluteRGB > 14 || report.pixelsOver32Percent > 18) {
  console.error('Visual comparison exceeds the documented 14 RGB MAE / 18% high-error thresholds');
  process.exitCode = 1;
}
