// Seeded state snapshots: reference Canvas versus offscreen LVGL RGB565.
import { createRequire } from 'node:module';
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here=path.dirname(fileURLToPath(import.meta.url));
const game=path.resolve(here,'..');
const root=path.resolve(game,'..','..','..');
const {chromium}=createRequire(path.join(root,'tools/quality_bar_golden/package.json'))('playwright-core');
const exe=process.argv[2]||path.join(root,'build-v2-native/hatch_a_pet/Release/hatch_a_pet_native.exe');
const reference=pathToFileURL(path.join(root,'v2/reference/hatch_a_pet.html')).href;
const output=path.join(game,'assets/generated');
const scenes=['start','egg','knock','hatch','pet','choice','yes','no','hint'];
// Measured on this game's five seeded captures (not inherited from Aquarium).
const limits={start:[5,0.005],egg:[5,0.02],knock:[7,0.04],
  hatch:[12,0.08],pet:[6,0.03],choice:[7,0.04],
  yes:[8,0.06],no:[8,0.05],hint:[8,0.05]};
const browser=await chromium.launch({channel:'msedge',headless:true});
try {
  const page=await browser.newPage({viewport:{width:320,height:240}});
  await page.addInitScript(() => {window.requestAnimationFrame=()=>0;});
  for(const scene of scenes) {
    const bmp=path.join(output,`lvgl_${scene}.bmp`);
    const result=spawnSync(exe,['--check','--scene',scene,'--seed','1234','--screenshot',bmp],{encoding:'utf8'});
    if(result.status!==0) throw Error(`${scene}: native failed: ${result.stderr}`);
    await page.goto(reference+'?seed=1234');
    const rgba=await page.evaluate(async ({scene,bmp}) => {
      const cv=document.getElementById('c'),ctx=cv.getContext('2d');
      cv.width=320;cv.height=240;ctx.setTransform(1,0,0,1,0,0);
      const step=n=>{for(let i=0;i<n;++i)update(0.016);};
      if(scene!=='start') {
        startGame();step(80);
        if(scene==='hint')step(760);
        if(scene!=='egg'&&scene!=='hint') {
          const knocks=scene==='knock'?1:egg.need;
          for(let n=0;n<knocks;++n) {knock();step(6);}
          if(scene!=='knock') {
            step(scene==='hatch'?30:130);
            if(['choice','yes','no'].includes(scene))step(320);
            if(scene==='yes'||scene==='no') {
              scene==='yes'?chooseYes():chooseNo();
              step(75);
            }
          }
        }
      } else update(0.016);
      ctx.clearRect(0,0,320,240);drawScene();
      if(st.phase==='START')drawStart();
      const ref=ctx.getImageData(0,0,320,240).data;
      const image=new Image();
      image.src='data:image/bmp;base64,'+bmp;
      await image.decode();
      const other=document.createElement('canvas');
      other.width=320;other.height=240;
      const oc=other.getContext('2d');
      oc.drawImage(image,0,0);
      const native=oc.getImageData(0,0,320,240).data;
      let absolute=0,large=0,edge=0;
      for(let i=0;i<320*240;++i) {
        let diff=0;
        for(let c=0;c<3;++c) diff+=Math.abs(ref[i*4+c]-native[i*4+c]);
        absolute+=diff/3;
        if(diff/3>40)large++;
        const x=i%320,y=Math.floor(i/320);
        if((x<3||x>316||y<3||y>236)&&diff/3>40)edge++;
      }
      return {mean: absolute/(320*240),large_fraction:large/(320*240),
        edge,ref_png:cv.toDataURL('image/png').split(',')[1],
        native_png:other.toDataURL('image/png').split(',')[1]};
    },{scene,bmp:fs.readFileSync(bmp).toString('base64')});
    fs.writeFileSync(path.join(output,`browser_${scene}.png`),Buffer.from(rgba.ref_png,'base64'));
    fs.writeFileSync(path.join(output,`lvgl_${scene}.png`),Buffer.from(rgba.native_png,'base64'));
    console.log(`${scene}: mean RGB abs ${rgba.mean.toFixed(2)}, >40px ${(rgba.large_fraction*100).toFixed(2)}%, edge ${rgba.edge}`);
    if(rgba.mean>limits[scene][0]||rgba.large_fraction>limits[scene][1]||rgba.edge>50)
      throw Error(`${scene}: seeded reference difference exceeds measured Hatch thresholds`);
  }
} finally {await browser.close();}
