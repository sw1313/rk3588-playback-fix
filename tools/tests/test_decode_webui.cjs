// Run with Node.js and Playwright installed (or exposed through NODE_PATH).
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const { pathToFileURL } = require('node:url');
const path = require('node:path');

(async () => {
  const browser = await chromium.launch({ headless: true, channel: process.env.PLAYWRIGHT_CHANNEL });
  let checks = 0;
  const check = (value) => { assert.ok(value); checks++; };
  const url = pathToFileURL(path.resolve(__dirname, '../../magisk-decode/webroot/index.html')).href;
  const page = await browser.newPage({ viewport: { width: 1000, height: 900 } });
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  await page.addInitScript(() => {
    const keys = 'mpp rga codec2 codec2_sdk codec2_sdk_next codec2_buffer libyuv software hdr_static hdr10plus p010_fast av1_native av1_grain av1_afbc'.split(' ');
    const deps = {codec2:['mpp','rga'],codec2_sdk:['codec2'],codec2_sdk_next:['codec2_sdk'],codec2_buffer:['codec2_sdk'],software:['codec2'],hdr_static:['codec2'],hdr10plus:['codec2'],p010_fast:['codec2'],av1_native:['codec2_sdk_next'],av1_afbc:['av1_native'],av1_grain:['mpp']};
    const requested = Object.fromEntries(keys.map(k => [k, 1]));
    let enabled = true;
    window.commands = [];
    window.mockInvalid = false;
    window.mockFailure = false;
    window.ksu = { exec(command, options, callback) {
      window.commands.push(command);
      const match = /^sh \/data\/adb\/modules\/rk3588_decode_stack\/webui\.sh (status|set ([a-z0-9_]+) (on|off)|backend (legacy|simd|latest))$/.exec(command);
      if (!match) throw new Error('Unexpected command');
      if (match[2] === 'all') enabled = match[3] === 'on';
      function enable(key) { requested[key] = 1; (deps[key] || []).forEach(enable); }
      if (match[4]) { requested.codec2_sdk = Number(match[4] !== 'legacy'); requested.codec2_sdk_next = Number(match[4] === 'latest'); enable('codec2'); }
      else if (match[2] && match[2] !== 'all') { if (match[3] === 'on') enable(match[2]); else requested[match[2]] = 0; }
      const effective = {};
      function resolve(k) { return effective[k] ??= requested[k] && Number(enabled) && Number((deps[k] || []).every(d => resolve(d) === 1)); }
      keys.forEach(resolve);
      const state = {schema:1,version:'0.6.5',activeVersion:'0.6.5',enabled,activeKnown:true,activeEnabled:true,valid:!window.mockInvalid,staged:false,removing:false,health:'passed',pendingReboot:!enabled || keys.some(k => effective[k] !== 1),components:Object.fromEntries(keys.map(k => [k,{requested:requested[k],effective:effective[k],active:1}]))};
      setTimeout(() => window[callback](window.mockFailure ? 1 : 0, JSON.stringify(state), ''), 10);
    }};
  });
  try {
    await page.goto(url);
    await page.locator('#notice.ready').waitFor();
    check(await page.getByRole('switch').count() === 13);
    check(await page.getByRole('combobox').count() === 1);
    check(await page.locator('#toggle-codec2_sdk').count() === 0 && await page.locator('#toggle-codec2_sdk_next').count() === 0);
    check(await page.getByRole('combobox').inputValue() === 'latest');
    await page.getByRole('combobox').selectOption('simd');
    await page.locator('#notice.pending').waitFor();
    check(await page.locator('[data-component="av1_native"] > .component.inactive').count() === 1);
    check(await page.locator('#toggle-av1_native').getAttribute('aria-checked') === 'true');
    await page.getByRole('combobox').selectOption('legacy');
    await page.waitForFunction(() => document.querySelector('#changes').textContent.includes('已保存'));
    check(await page.locator('[data-component="codec2_buffer"] > .component.inactive').count() === 1);
    await page.getByRole('combobox').selectOption('latest');
    await page.locator('#notice.ready').waitFor();
    check(await page.locator('[data-component="codec2_backend"] > .children [data-component="av1_native"]').count() === 1);
    check(await page.locator('[data-component="av1_native"] > .children [data-component="av1_afbc"]').count() === 1);
    check(await page.locator('#toggle-av1_afbc').getAttribute('aria-checked') === 'true');
    await page.locator('#toggle-av1_afbc').click();
    await page.locator('#notice.pending').waitFor();
    const afbc = page.locator('[data-component="av1_afbc"] > .component');
    check((await afbc.innerText()).includes('当前：开启') && (await afbc.innerText()).includes('重启后：关闭'));
    await page.locator('#toggle-av1_afbc').click();
    await page.locator('#notice.ready').waitFor();
    await page.locator('#toggle-mpp').click();
    await page.locator('#notice.pending').waitFor();
    const c2 = page.locator('[data-component="codec2"] > .component');
    check((await c2.innerText()).includes('需要先启用：MPP / VPU'));
    check(await page.locator('#toggle-codec2').getAttribute('aria-checked') === 'true');
    check(await page.locator('#toggle-codec2').isEnabled());
    check(await c2.evaluate(e => e.classList.contains('inactive')));
    check(await page.locator('[data-component="mpp"] > .children [data-component="codec2"]').count() === 1);
    await page.locator('#whole').click();
    await page.waitForFunction(() => document.querySelector('#whole').getAttribute('aria-checked') === 'false' && !document.querySelector('#whole').disabled);
    check((await page.locator('#whole-state').innerText()).includes('当前：启用 · 重启后：停用'));
    await page.locator('#whole').click();
    await page.waitForFunction(() => document.querySelector('#whole').getAttribute('aria-checked') === 'true' && !document.querySelector('#whole').disabled);
    check(await page.locator('#toggle-mpp').getAttribute('aria-checked') === 'false');
    await page.locator('#toggle-mpp').click();
    await page.locator('#notice.ready').waitFor();
    await page.getByRole('combobox').selectOption('simd');
    await page.locator('#notice.pending').waitFor();
    await page.locator('#toggle-av1_afbc').click();
    await page.waitForFunction(() => document.querySelector('#toggle-av1_afbc').getAttribute('aria-checked') === 'false' && !document.querySelector('#toggle-av1_afbc').disabled);
    await page.locator('#toggle-mpp').click();
    await page.waitForFunction(() => !document.querySelector('#toggle-mpp').disabled);
    await page.locator('#toggle-av1_afbc').click();
    await page.locator('#notice.ready').waitFor();
    check(await page.getByRole('combobox').inputValue() === 'latest');
    check((await page.locator('#changes').innerText()).includes('联动开启'));
    await page.keyboard.press('Tab');
    check(await page.evaluate(() => ['BUTTON','SELECT'].includes(document.activeElement.tagName)));
    await page.setViewportSize({width:390,height:844});
    check(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
    if (process.env.WEBUI_SCREENSHOT) await page.screenshot({path:process.env.WEBUI_SCREENSHOT,fullPage:true});
    await page.evaluate(() => {window.mockInvalid=true;});
    await page.locator('#refresh').click();
    await page.locator('#notice.error').waitFor();
    check(await page.locator('#whole').isEnabled() && await page.locator('#toggle-mpp').isDisabled());
    await page.locator('#whole').click();
    await page.waitForFunction(() => document.querySelector('#whole').getAttribute('aria-checked') === 'false');
    check(true);
    await page.evaluate(() => {window.mockFailure=true;});
    await page.locator('#refresh').click();
    await page.waitForFunction(() => document.querySelector('#notice').textContent.includes('操作失败'));
    check(await page.locator('#whole').isDisabled() && await page.locator('#refresh').isEnabled());
    const unprivileged = await browser.newPage();
    await unprivileged.goto(url);
    await unprivileged.locator('#notice.error').waitFor();
    check((await unprivileged.locator('#notice').innerText()).includes('KSU WebUI'));
    check(await unprivileged.locator('#whole').isDisabled());
    check(errors.length === 0);
    console.log(JSON.stringify({browser_assertions:checks,passed:true,bridge:'mock callback contract; real Android bridge tested separately'}));
  } finally { await browser.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
