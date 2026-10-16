const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const { pathToFileURL } = require('node:url');
const path = require('node:path');
(async()=>{
 const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL});
 const page=await browser.newPage({viewport:{width:1000,height:1000},locale:'zh-CN'});let checks=0;
 const check=v=>{assert.ok(v);checks++;};const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{
  let mode='on',kernel='stock';window.commands=[];window.mockUnsupported=false;window.mockStaged=false;window.mockRecovery=false;
  window.ksu={exec(command,opts,callback){
   window.commands.push(command);
   const m=/^sh \/data\/adb\/modules\/rk3588_mali_global\/webui\.sh (status|set (on|off)|kernel (stock|next))$/.exec(command);
   if(!m)throw Error('Unexpected command');
   if(m[2])mode=m[2];if(m[3]){kernel=m[3];if(kernel==='next')mode='on';}
   const next=mode==='on'&&!window.mockUnsupported?kernel:'stock';
   const s={schema:1,version:'0.3.0',activeVersion:'0.3.0',requested:mode,effective:mode,active:'on',activeKnown:true,moduleEnabled:true,staged:window.mockStaged,removing:false,supported:!window.mockUnsupported,kernelRequested:kernel,kernelEffective:next,kernelActive:'stock',kernelHealth:'stock',recovery:window.mockRecovery?'interrupted_previous_boot':'none',pendingReboot:mode!=='on'||next!=='stock'};
   setTimeout(()=>window[callback](0,JSON.stringify(s),''),10);
  }};
 });
 try{
  await page.goto(pathToFileURL(path.resolve(__dirname,'../../magisk-mali-global/webroot/index.html')).href);
  await page.waitForFunction(()=>document.querySelector('#whole').getAttribute('aria-checked')==='true');
  check(await page.getByRole('switch').count()===1);check(await page.getByRole('combobox').count()===1);
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
  check(errors.length===0);console.log(JSON.stringify({checks,passed:true}));
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
