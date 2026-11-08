// Build-time capture of the original reference Canvas functions. The native executable
// consumes only the RGB565/RGB565A8 pack; Edge and Node are not runtime dependencies.
import { createRequire } from 'node:module';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const game = path.resolve(here, '..');
const root = path.resolve(game, '..', '..', '..');
const reference = path.join(root, 'v2/reference/hatch_a_pet.html');
const { chromium } = createRequire(path.join(root, 'tools/quality_bar_golden/package.json'))('playwright-core');
const output = path.join(game, 'assets/generated');
fs.mkdirSync(output, { recursive: true });

const specs = [];
const add = (id, crop, origin, expr, filter = null, format = 'RGB565A8', timing = null) =>
  specs.push({ id, crop, origin, expr, filter, format, timing });
add('background', [0, 0, 320, 240], [0, 0], 'time=0; drawBackground()', `(m,c) => c.fillStyle !== 'rgba(255, 255, 255, 0.85)'`, 'RGB565');
add('cloud', [25, 0, 62, 46], [40, 22], 'time=10; drawBackground()',
  `(m,c) => c.fillStyle === 'rgba(255, 255, 255, 0.85)'`, 'A8');
add('nest_back', [90, 176, 140, 44], [160, 198], 'drawNestBack()');
add('nest_front', [88, 178, 144, 60], [160, 198], 'drawNestFront()');
add('egg_shell', [96, 65, 128, 148], [160, 138], "ctx.translate(160,138); shellFill('whole'); shellShine()");
add('egg_glow',[92,61,136,154],[160,138],
  "st.phase='EGG'; st.hint=1; egg.x=160; egg.yOff=0; egg.q=0; egg.ang=0; egg.roll=0; time=0; drawEgg()",
  '(m,c)=>m==="stroke"&&c.lineWidth===6','A8');
for (let p = 0; p < 4; ++p) {
  add(`egg_decal_${p}`, [96, 65, 128, 148], [160, 138],
    `ctx.translate(160,138); drawDecals(PATTERNS[${p}], '#FFFFFF', 'whole')`, null, 'A8');
  for (const half of ['top', 'bot'])
    add(`shell_${half}_decal_${p}`, [96, 65, 128, 148], [160, 138],
      `ctx.translate(160,138); drawDecals(PATTERNS[${p}], '#FFFFFF', '${half}')`, null, 'A8');
}
for (const half of ['top','bot'])
  add(`shell_${half}`, [96, 65, 128, 148], [160, 138],
    `ctx.translate(160,138); shellFill('${half}'); ${half === 'top' ? 'shellShine();' : ''}`);
for (let c = 1; c <= 5; ++c)
  add(`crack_${c}`, [109, 97, 102, 52], [160, 138],
    `ctx.translate(160,138); egg.cracks=${c}; egg.need=5; egg.blink=1; egg.look=[0,1]; drawCrack(${c}/5, false)`);
for (let k = 0; k < 8; ++k)
  for (const mood of ['idle', 'happy', 'open', 'squint'])
    add(`pet_${k}_${mood}`, [101, 57, 123, 139], [160, 150],
      `time=0; drawCreature(${k},160,150,1,0,0,'${mood}',[0,1],1,1)`);
for (const type of ['no', 'yes'])
  add(`choice_${type}`, type === 'no' ? [0, 120, 94, 96] : [226, 120, 94, 96],
    type === 'no' ? [48,168] : [272,168],
    `choices.show=true; choices.t=0.5; choices.idle=0; time=0; const originalChoice=choiceBubble; choiceBubble=(...a)=>{if(a[0]===${type === 'no' ? 48 : 272}) originalChoice(...a)}; drawChoices()`);
add('badge_base',[125,0,70,65],[160,32],
  "st.phase='EGG'; egg.creature=0; st.hint=0; badge.happy=0; badge.sv=0; time=0; drawBadge()",
  '(m,c,i)=>i<=1');
add('badge_glow',[122,0,76,70],[160,32],
  "st.phase='EGG'; st.hint=1; badge.happy=0; badge.sv=0; time=0; drawBadge()",
  '(m,c)=>m==="stroke"&&c.lineWidth===4','A8');
add('choice_glow',[0,116,96,104],[48,168],
  "choices.show=true; choices.t=0.5; choices.idle=7; time=0; const old=choiceBubble; choiceBubble=(...a)=>{if(a[0]===48)old(...a)};drawChoices()",
  '(m,c)=>m==="stroke"&&c.lineWidth===6','A8');
for(let k=0;k<8;++k)
  for(const style of ['egg','heart','happy'])
    add(`badge_${k}_${style}`, [133, 5, 54, 54], [160, 32],
      `st.phase='${style==='egg'?'EGG':'PET'}'; egg.creature=${k}; st.hint=0; badge.happy=${style==='happy'?1:0}; badge.sv=0; time=0; drawBadge()`,
      '(m,c,i)=>i>1');
add('home', [4, 4, 48, 48], [4, 4], 'drawChrome()', '(m,c,i) => i < 3');
add('mic', [268, 4, 48, 48], [268, 4], 'drawChrome()', '(m,c,i) => i >= 3');
add('start', [119, 79, 82, 82], [160, 120],
  'time=0; drawStart()', '(m,c,i) => i !== 0');
add('hint_hand', [174, 132, 38, 48], [190, 162],
  "st.phase='EGG'; st.hint=2; time=0; drawHelperHand()");
for(const shape of ['star','circle','rect','heart'])
  add(`particle_${shape}`,[148,108,24,24],[160,120],
    `parts=[{x:160,y:120,vx:0,vy:0,g:0,life:1,max:1,rot:0,vr:0,add:false,shape:'${shape}',size:6,color:'#FFFFFF'}];drawFx()`,
    null,'A8');

const browser = await chromium.launch({ channel: 'msedge', headless: true });
const rows = [];
try {
  const page = await browser.newPage({ viewport: { width: 320, height: 240 } });
  await page.addInitScript(() => {
    window.requestAnimationFrame = () => 0;
    window.__filter = null;
    const proto = CanvasRenderingContext2D.prototype;
    for (const method of ['fill', 'stroke', 'fillRect', 'drawImage']) {
      const original = proto[method];
      proto[method] = function (...args) {
        if (window.__filter && this.canvas.id === 'c' && !window.__filter(method, this, window.__op++)) return;
        return original.apply(this, args);
      };
    }
  });
  for (const spec of specs) {
    await page.goto(pathToFileURL(reference).href + '?seed=1234');
    const row = await page.evaluate(s => {
      const canvas = document.getElementById('c'), ctx = canvas.getContext('2d');
      canvas.width = 320; canvas.height = 240; ctx.setTransform(1,0,0,1,0,0);
      ctx.clearRect(0,0,320,240); window.__op = 0;
      window.__filter = s.filter ? (0,eval)(`(${s.filter})`) : null;
      (0,eval)(s.expr);
      window.__filter = null;
      let [x,y,w,h] = s.crop;
      if (s.format !== 'RGB565') {
        const rgba = ctx.getImageData(x,y,w,h).data;
        let left=w, top=h, right=-1, bottom=-1;
        for (let py=0; py<h; ++py) for (let px=0; px<w; ++px) {
          if (!rgba[(py*w+px)*4+3]) continue;
          left=Math.min(left,px); top=Math.min(top,py); right=Math.max(right,px); bottom=Math.max(bottom,py);
        }
        if (right < 0) throw Error(`empty sprite ${s.id}`);
        if (left===0 || top===0 || right===w-1 || bottom===h-1)
          throw Error(`sprite ${s.id} clipped by crop ${s.crop}`);
        const x1=Math.max(0,x+left-2),y1=Math.max(0,y+top-2);
        w=Math.min(320,x+right+3)-x1; h=Math.min(240,y+bottom+3)-y1; x=x1;y=y1;
      }
      const pixels=ctx.getImageData(x,y,w,h).data;
      let raw='';
      for (let i=0;i<pixels.length;i+=8192) raw+=String.fromCharCode(...pixels.subarray(i,i+8192));
      const png=document.createElement('canvas'); png.width=w;png.height=h;
      png.getContext('2d').drawImage(canvas,x,y,w,h,0,0,w,h);
      return { crop:[x,y,w,h], rgba:btoa(raw), png:png.toDataURL('image/png').split(',')[1] };
    }, spec);
    fs.writeFileSync(path.join(output, spec.id+'.png'), Buffer.from(row.png,'base64'));
    rows.push({spec,...row});
  }
} finally { await browser.close(); }

const entries=[], chunks=[], manifest=[];
const HEADER=32, ENTRY=52;
let offset=HEADER+ENTRY*rows.length;
for (const {spec,crop,rgba} of rows) {
  const [x,y,w,h]=crop, raw=Buffer.from(rgba,'base64');
  const stride=spec.format==='A8' ? w : w*2;
  const data=Buffer.alloc(stride*h+(spec.format==='RGB565A8'?w*h:0));
  for(let i=0;i<w*h;++i) {
    if(spec.format==='A8') { data[i]=raw[i*4+3]; continue; }
    const r=raw[i*4]>>3, g=raw[i*4+1]>>2, b=raw[i*4+2]>>3;
    data.writeUInt16LE(r<<11|g<<5|b,i*2);
    if(spec.format==='RGB565A8') data[w*h*2+i]=raw[i*4+3];
  }
  const padding=(4-offset%4)%4;
  if(padding) {chunks.push(Buffer.alloc(padding));offset+=padding;}
  entries.push({name:spec.id,fmt:{RGB565:1,RGB565A8:2,A8:3}[spec.format],
    w,h,stride,ox:spec.origin[0]-x,oy:spec.origin[1]-y,offset,size:data.length});
  chunks.push(data); offset+=data.length;
  manifest.push({id:spec.id,source:'v2/reference/hatch_a_pet.html',call:spec.expr,
    crop,origin:[spec.origin[0]-x,spec.origin[1]-y],width:w,height:h,
    format:spec.format,frame_timing:spec.timing,bytes:data.length,filter:spec.filter});
}
const table=Buffer.alloc(entries.length*ENTRY);
for(let i=0;i<entries.length;++i) {
  const e=entries[i],o=i*ENTRY;
  if(e.name.length>23) throw Error(`name too long ${e.name}`);
  table.write(e.name,o,'ascii');table.writeUInt32LE(e.fmt,o+24);
  table.writeUInt16LE(e.w,o+28);table.writeUInt16LE(e.h,o+30);
  table.writeUInt32LE(e.stride,o+32);table.writeFloatLE(e.ox,o+36);
  table.writeFloatLE(e.oy,o+40);table.writeUInt32LE(e.offset,o+44);
  table.writeUInt32LE(e.size,o+48);
}
const payload=Buffer.concat([table,...chunks]);
const signature=crypto.createHash('sha256').update(payload).digest().subarray(0,16);
const header=Buffer.alloc(HEADER);
header.write('CPOP');header.writeUInt32LE(1,4);header.writeUInt32LE(entries.length,8);
header.writeUInt32LE(offset,12);signature.copy(header,16);
fs.writeFileSync(path.join(output,'hatch_a_pet.pack'),Buffer.concat([header,payload]));
fs.writeFileSync(path.join(game,'device/main/pack_signature.hpp'),
  '#pragma once\n#include <cstdint>\nnamespace hatch_a_pet::pack {\n'
  +`inline constexpr uint32_t bytes=${offset};\n`
  +`inline constexpr uint8_t signature[16]={${[...signature].join(',')}};\n}\n`);
fs.writeFileSync(path.join(game,'assets/manifest.json'),JSON.stringify({
  reference:'v2/reference/hatch_a_pet.html',
  source_sha256:crypto.createHash('sha256').update(fs.readFileSync(reference)).digest('hex'),
  exporter:'tools/export_assets.mjs',browser:browser.version(),
  pack_format:'CPOP v1 (flash mapped LVGL RGB565 / RGB565A8 / A8)',
  packed_bytes:offset,psram_bytes:0,assets:manifest},null,2)+'\n');
console.log(`Hatch artwork ${entries.length} sprites, ${offset} bytes (${(offset/1048576).toFixed(3)} MiB); PSRAM 0`);
if(offset>1.2*1048576) throw Error('Hatch assets exceed 1.2 MiB flash budget');
