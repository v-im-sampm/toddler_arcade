import { createRequire } from 'node:module';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..', '..', '..', '..');
const require = createRequire(path.join(root, 'tools', 'quality_bar_golden', 'package.json'));
const { chromium } = require('playwright-core');
const output = path.join(here, '..', 'assets', 'generated');
const reference = path.join(root, 'v2', 'reference', 'bubble_aquarium.html');
fs.mkdirSync(output, { recursive: true });

const browser = await chromium.launch({ channel: 'msedge', headless: true });
try {
  const page = await browser.newPage({ viewport: { width: 400, height: 300 }, deviceScaleFactor: 1 });
  await page.addInitScript(() => { window.requestAnimationFrame = () => 0; });
  await page.goto(pathToFileURL(reference).href + '?seed=1234');
  const manifest = { source: 'v2/reference/bubble_aquarium.html', size: [320, 240],
                     format: 'PNG + raw BGRA8888', sprites: [] };
  async function save(name, bounds, expression) {
    const data = await page.evaluate(({ bounds, expression }) => {
      // The reference sketch is the authoritative drawing implementation.
      // Draw in its own main-world canvas, then crop without reimplementing its paths.
      const cv = document.getElementById('c');
      cv.width = 320; cv.height = 240;
      const ctx = cv.getContext('2d');
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.clearRect(0, 0, 320, 240);
      (0, eval)(expression);
      const [x, y, w, h] = bounds;
      const cropped = document.createElement('canvas');
      cropped.width = w; cropped.height = h;
      const dst = cropped.getContext('2d');
      dst.drawImage(cv, x, y, w, h, 0, 0, w, h);
      const pixels = dst.getImageData(0, 0, w, h).data;
      const bgra = new Uint8Array(pixels.length);
      for (let i = 0; i < pixels.length; i += 4) {
        bgra[i] = pixels[i + 2]; bgra[i + 1] = pixels[i + 1];
        bgra[i + 2] = pixels[i]; bgra[i + 3] = pixels[i + 3];
      }
      let raw = '';
      for (let i = 0; i < bgra.length; i += 8192)
        raw += String.fromCharCode(...bgra.subarray(i, i + 8192));
      return { png: cropped.toDataURL('image/png').split(',')[1], raw: btoa(raw) };
    }, { bounds, expression });
    fs.writeFileSync(path.join(output, name + '.png'), Buffer.from(data.png, 'base64'));
    fs.writeFileSync(path.join(output, name + '.bgra'), Buffer.from(data.raw, 'base64'));
    manifest.sprites.push({ id: name, bounds, bytes: bounds[2] * bounds[3] * 4 });
  }
  await save('backdrop', [0, 0, 320, 240], 'drawBackdrop()');
  await save('start', [0, 0, 320, 240], 'drawStart()');
  await save('chrome_home', [4, 4, 48, 48], 'drawChrome()');
  await save('chrome_mic', [268, 4, 48, 48], 'drawChrome()');
  for (let j = 0; j < 6; ++j) {
    const t = (j / 6) * Math.PI * 2;
    await save(`rays_${j}`, [0, 62, 320, 144], `time=${t}; drawRays()`);
    await save(`caustics_${j}`, [0, 204, 320, 36], `time=${t}; drawCaustics()`);
    await save(`surface_${j}`, [0, 58, 320, 14], `time=${t}; drawSurface()`);
    for (const [layer, x, y, w, h] of [
      ['back_mid', 116, 140, 62, 80], ['back_right', 278, 140, 42, 80],
      ['front_left', 0, 120, 55, 100], ['front_right', 284, 145, 36, 75],
    ]) {
      const back = layer.startsWith('back');
      await save(`weed_${layer}_${j}`, [x, y, w, h], `time=${t}; drawSeaweed(${back})`);
    }
    for (let i = 0; i < 3; ++i) {
      for (let face of [-1, 1]) {
        for (const mood of ['normal', 'happy', 'sleep']) {
          await save(`fish_${i}_${face < 0 ? 'left' : 'right'}_${mood}_${j}`, [112, 80, 96, 80],
            `time=${t}; fish[${i}].x=160; fish[${i}].y=120; fish[${i}].ox=160; fish[${i}].oy=120; ` +
            `fish[${i}].face=${face}; fish[${i}].fx=${face}; fish[${i}].tail=${t}; ` +
            `fish[${i}].happy=${mood === 'happy' ? 0.6 : 0}; fish[${i}].doze=${mood === 'sleep' ? 1 : 0}; ` +
            `fish[${i}].asleep=${mood === 'sleep'}; fish[${i}].peek=0; fish[${i}].blink=1; ` +
            `fish[${i}].look=[${face * 10},0]; simA=1; drawFish(fish[${i}])`);
        }
      }
      await save(`visitor_${i}_${j}`, [104, 76, 112, 88],
        `time=${t}; visitor={k:${i},x:160,y:120,ox:160,oy:120,t:${t},wave:0,q:0}; simA=1; drawVisitor()`);
    }
  }
  for (let i = 0; i < 4; ++i) {
    await save(`chest_${i}`, [188, 184, 64, 56], `chest.lid=${i / 3}; chest.q=0; drawChest()`);
  }
  await save('pellet', [88, 88, 24, 24],
    'pellets=[{x:100,y:100,ox:100,oy:100}]; simA=1; drawPellets()');
  await save('bubble', [135, 95, 50, 50],
    'bubbles=[{x:160,y:120,ox:160,oy:120,r:20,g:1,age:0,ph:0}]; simA=1; drawBubbles()');
  await save('shadow', [130, 212, 60, 22],
    'fish.forEach((f,i)=>{f.x=i? -100:160;f.ox=f.x;f.y=140;f.oy=140}); visitor=null; simA=1; drawShadows()');
  await save('helper_hand', [90, 62, 36, 36],
    "st.phase='SWIM'; st.hint=2; st.bed=false; time=0; drawHelperHand()");
  for (const night of [0, 1]) {
    for (let i = 0; i < 6; ++i) {
      await save(`button_shaker_${night}_${i}`, [76, 4, 48, 48],
        `time=${i * 0.15}; st.bedK=${night}; st.hint=0; drawButtons()`);
      await save(`button_moon_${night}_${i}`, [136, 4, 48, 48],
        `time=${i * 0.15}; st.bedK=${night}; drawButtons()`);
      await save(`button_bubbles_${night}_${i}`, [196, 4, 48, 48],
        `time=${i * 0.15}; st.bedK=${night}; drawButtons()`);
      await save(`button_hint_${night}_${i}`, [76, 4, 48, 48],
        `time=${i * 0.15}; st.bedK=${night}; st.hint=1; drawButtons()`);
    }
  }
  fs.writeFileSync(path.join(output, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
  console.log(`Exported ${manifest.sprites.length} PNG + BGRA8888 sprites from ${reference}`);
} finally {
  await browser.close();
}
