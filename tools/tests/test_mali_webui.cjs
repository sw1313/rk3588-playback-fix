const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const { pathToFileURL } = require('node:url');
const path = require('node:path');
(async()=>{
 const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL});
 const page=await browser.newPage({viewport:{width:1000,height:1000},locale:'zh-CN'});let checks=0;
 const check=v=>{assert.ok(v);checks++;};const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{
  let mode='on',kernel='stock',dvfs='off',cold='off',fence='off';window.commands=[];window.mockUnsupported=false;window.mockStaged=false;window.mockRecovery=false;window.mockColdPrepared=false;window.mockColdActive='off';window.mockFenceActive='off';
  window.ksu={exec(command,opts,callback){
   window.commands.push(command);
   const m=/^sh \/data\/adb\/modules\/rk3588_mali_global\/webui\.sh (status|set (on|off)|kernel (stock|next)|dvfs (on|off)|cold (on|off)|fence (on|off))$/.exec(command);
   if(!m)throw Error('Unexpected command');
   if(m[2])mode=m[2];if(m[3]){kernel=m[3];if(kernel==='next')mode='on';}
   if(m[4]){dvfs=m[4];if(dvfs==='on'){mode='on';kernel='next';}}
   if(m[5]){cold=m[5];if(cold==='on'){mode='on';kernel='next';}else kernel='stock';}
   if(m[6]){fence=m[6];if(fence==='on'){mode='on';kernel='next';cold='on';}}
   const next=mode==='on'&&!window.mockUnsupported?kernel:'stock';
   const s={rootStartup:window.mockRootStartup||'supported',fenceRequested:fence,fenceSupported:window.mockColdPrepared&&!window.mockUnsupported,fenceActive:window.mockFenceActive,fenceEffective:mode==='on'&&next==='next'&&cold==='on'?fence:'off',coldRequested:cold,coldPrepared:window.mockColdPrepared,coldActive:window.mockColdActive,coldEffective:mode==='on'&&next==='next'?cold:'off',schema:1,version:'0.3.1-test1',activeVersion:'0.3.1-test1',requested:mode,effective:mode,active:'on',activeKnown:true,moduleEnabled:true,staged:window.mockStaged,removing:false,supported:!window.mockUnsupported,kernelRequested:kernel,kernelEffective:next,kernelActive:'stock',kernelHealth:'stock',recovery:window.mockRecovery?'interrupted_previous_boot':'none',dvfsRequested:dvfs,dvfsEffective:next==='next'&&mode==='on'?dvfs:'off',dvfsActive:'off',dvfsSupported:!window.mockUnsupported,dvfsHealth:'off',pollingIntervalMs:100,pendingReboot:mode!=='on'||next!=='stock'};
   setTimeout(()=>window[callback](0,JSON.stringify(s),''),10);
  }};
 });
 try{
  await page.goto(pathToFileURL(path.resolve(__dirname,'../../magisk-mali-global/webroot/index.html')).href);
  await page.waitForFunction(()=>document.querySelector('#whole').getAttribute('aria-checked')==='true');
  check(await page.getByRole('switch').count()===4);check(await page.getByRole('combobox').count()===1);
  await page.evaluate(()=>window.mockRootStartup='other_root_backend');
  await page.locator('#refresh').click();
  await page.waitForFunction(()=>document.querySelector('#cold-detail').textContent.includes('当前 root 缺少'));
  check(!(await page.locator('#whole').isDisabled()));check(await page.locator('#cold').isDisabled());
  await page.evaluate(()=>window.mockRootStartup='supported');
  await page.locator('#refresh').click();
  await page.waitForFunction(()=>document.querySelector('#cold-detail').textContent.includes('尚未准备'));

  // Regression: the cold button had aria state but no track/thumb CSS at all.
  check(await page.getByRole('switch').evaluateAll(buttons=>buttons.every(b=>{
   const track=b.getBoundingClientRect(),thumb=b.querySelector('span').getBoundingClientRect();
   return track.width===58&&track.height===32&&thumb.width===24&&thumb.height===24;
  })));
  check(await page.locator('#dvfs').getAttribute('aria-checked')==='false');
  await page.locator('#dvfs').click();await page.waitForFunction(()=>!document.querySelector('#dvfs').disabled&&document.querySelector('#dvfs').getAttribute('aria-checked')==='true');
  check(await page.locator('#kernel').inputValue()==='next');check((await page.locator('#dvfs-current').innerText()).includes('关闭'));
  await page.locator('#kernel').selectOption('stock');await page.waitForFunction(()=>!document.querySelector('#kernel').disabled);
  check(await page.locator('#dvfs').getAttribute('aria-checked')==='true');check(await page.locator('#dvfs-card.inactive').count()===1);
  await page.locator('#dvfs').click();await page.waitForFunction(()=>!document.querySelector('#dvfs').disabled&&document.querySelector('#dvfs').getAttribute('aria-checked')==='false');
  await page.locator('#kernel').selectOption('next');await page.locator('#notice.pending').waitFor();
  check((await page.locator('#kernel-current').innerText()).includes('原内核'));check((await page.locator('#kernel-next').innerText()).includes('g18p0'));
  await page.locator('#whole').click();await page.waitForFunction(()=>!document.querySelector('#whole').disabled&&document.querySelector('#whole').getAttribute('aria-checked')==='false');
  check(await page.locator('#kernel').inputValue()==='next');check(await page.locator('#kernel-card.inactive').count()===1);check((await page.locator('#kernel-next').innerText()).includes('原内核'));
  await page.locator('#kernel').selectOption('stock');await page.waitForFunction(()=>!document.querySelector('#kernel').disabled);
  await page.locator('#kernel').selectOption('next');await page.waitForFunction(()=>document.querySelector('#whole').getAttribute('aria-checked')==='true'&&!document.querySelector('#kernel').disabled);
  check(await page.locator('#kernel-card.inactive').count()===0);
  const count=await page.evaluate(()=>window.commands.length);await page.locator('#language').click();
  check(await page.locator('html').getAttribute('lang')==='en');check(await page.evaluate(()=>window.commands.length)===count);
  check((await page.locator('#kernel-desc').innerText()).includes('Reboot to apply'));
  await page.reload();await page.waitForFunction(()=>!document.querySelector('#refresh').disabled);check(await page.locator('html').getAttribute('lang')==='en');
  check(await page.evaluate(()=>Object.keys(MaliI18n.zh).sort().join()===Object.keys(MaliI18n.en).sort().join()));
  await page.setViewportSize({width:390,height:844});check(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  if(process.env.WEBUI_SCREENSHOT)await page.screenshot({path:process.env.WEBUI_SCREENSHOT,fullPage:true});
  await page.evaluate(()=>window.mockUnsupported=true);await page.locator('#refresh').click();await page.waitForFunction(()=>document.querySelector('#kernel').disabled&&!document.querySelector('#refresh').disabled);check((await page.locator('#dependency').innerText()).includes('compatibility'));
  await page.evaluate(()=>{window.mockUnsupported=false;window.mockRecovery=true;});await page.locator('#refresh').click();await page.waitForFunction(()=>!document.querySelector('#refresh').disabled);check((await page.locator('#recovery').innerText()).includes('automatically'));
  check(await page.locator('#cold').isDisabled());
  await page.evaluate(()=>window.mockColdPrepared=true);await page.locator('#refresh').click();await page.waitForFunction(()=>!document.querySelector('#cold').disabled);
  await page.locator('#cold').click();await page.waitForFunction(()=>document.querySelector('#cold').getAttribute('aria-checked')==='true'&&!document.querySelector('#refresh').disabled);
  check(await page.locator('#kernel').inputValue()==='next');check((await page.locator('#cold-current').innerText()).includes('Off'));check((await page.locator('#cold-next').innerText()).includes('On'));
  await page.waitForFunction(()=>new DOMMatrix(getComputedStyle(document.querySelector('#cold span')).transform).m41===24);
  check(await page.locator('#cold').evaluate(b=>getComputedStyle(b).backgroundColor===getComputedStyle(document.querySelector('#whole')).backgroundColor));
  await page.evaluate(()=>window.mockColdActive='on');await page.locator('#refresh').click();await page.waitForFunction(()=>!document.querySelector('#refresh').disabled);
  check((await page.locator('#cold-current').innerText()).includes('On'));
  if(process.env.WEBUI_COLD_SCREENSHOT)await page.screenshot({path:process.env.WEBUI_COLD_SCREENSHOT,fullPage:true});
  await page.locator('#cold').click();await page.waitForFunction(()=>document.querySelector('#cold').getAttribute('aria-checked')==='false'&&!document.querySelector('#refresh').disabled);check(await page.locator('#kernel').inputValue()==='stock');
  await page.waitForFunction(()=>new DOMMatrix(getComputedStyle(document.querySelector('#cold span')).transform).m41===0);
  check((await page.locator('#cold-current').innerText()).includes('On')&&(await page.locator('#cold-next').innerText()).includes('Off'));
  await page.locator('#fence').click();await page.waitForFunction(()=>!document.querySelector('#refresh').disabled&&document.querySelector('#fence').getAttribute('aria-checked')==='true');
  check(await page.locator('#cold').getAttribute('aria-checked')==='true');check((await page.locator('#fence-current').innerText()).includes('Off'));check((await page.locator('#fence-next').innerText()).includes('On'));
  await page.evaluate(()=>window.mockFenceActive='on');await page.locator('#refresh').click();await page.waitForFunction(()=>!document.querySelector('#refresh').disabled);
  check((await page.locator('#fence-current').innerText()).includes('On'));
  await page.locator('#fence').click();await page.waitForFunction(()=>!document.querySelector('#refresh').disabled&&document.querySelector('#fence').getAttribute('aria-checked')==='false');
  check((await page.locator('#fence-current').innerText()).includes('On')&&(await page.locator('#fence-next').innerText()).includes('Off'));
  await page.locator('#fence').click();await page.waitForFunction(()=>!document.querySelector('#refresh').disabled&&document.querySelector('#fence').getAttribute('aria-checked')==='true');
  await page.locator('#whole').click();await page.waitForFunction(()=>!document.querySelector('#refresh').disabled&&document.querySelector('#whole').getAttribute('aria-checked')==='false');
  check(await page.locator('#fence').getAttribute('aria-checked')==='true');check(await page.locator('#fence-card.inactive').count()===1);check((await page.locator('#fence-next').innerText()).includes('Off'));
  check(errors.length===0);console.log(JSON.stringify({checks,passed:true}));
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
