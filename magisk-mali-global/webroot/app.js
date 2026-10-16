"use strict";
(() => {
 const $=id=>document.getElementById(id),bridge='sh /data/adb/modules/rk3588_mali_global/webui.sh';
 let lang='zh',state=null,busy=false,id=0;
 try{lang=localStorage.getItem('mali-language')||(/^(zh)/i.test(navigator.language)?'zh':'en');}catch{}
 if(!MaliI18n[lang])lang='zh';
 const t=k=>MaliI18n[lang][k]||k;
 const label=v=>t(v==='next'?'next':v==='stock'?'stock':v==='on'?'on':v==='off'?'off':v==='active'?'active':v==='checking'?'checking':v==='failed'?'failed':'unknown');
 function notice(key,kind=''){ $('notice').textContent=t(key);$('notice').className=kind; }
 function controls(){ $('whole').disabled=busy||!state||state.removing;$('kernel').disabled=busy||!state||!state.supported||state.removing;$('refresh').disabled=busy; }
 function paint(){
  document.documentElement.lang=lang==='zh'?'zh-CN':'en';document.title=t('title');$('language').textContent=lang==='zh'?'English':'中文';
  const texts={'title':'title','whole-title':'whole','whole-desc':'wholeDesc','kernel-title':'kernel','kernel-desc':'kernelDesc','recovery-title':'recoveryTitle','recovery-desc':'recoveryDesc','refresh':'refresh','footnote':'footnote'};
  for(const [id,key] of Object.entries(texts))$(id).textContent=t(key);
  for(const o of $('kernel').options)o.textContent=t(o.value);
  if(!state){notice(window.ksu?'loading':'host');controls();return;}
  $('version').textContent=t('version')+' '+state.version+' · '+t('applied')+' '+state.activeVersion;
  $('whole').setAttribute('aria-checked',String(state.requested==='on'));
  $('whole-current').textContent=t('current')+label(state.active);$('whole-next').textContent=t('after')+label(state.effective);
  $('kernel').value=state.kernelRequested;$('kernel-current').textContent=t('current')+label(state.kernelActive);$('kernel-next').textContent=t('after')+label(state.kernelEffective);
  $('kernel-card').classList.toggle('inactive',state.effective!=='on'||!state.supported);
  $('dependency').textContent=!state.supported?t('unsupported'):state.effective!=='on'?t('retained'):'';
  $('health').textContent=t('health')+label(state.kernelHealth);
  $('recovery').textContent=t(state.recovery==='none'?'recoveryNone':'recoveryDone');
  if(state.removing)notice('removing','pending');else if(!state.moduleEnabled)notice('disabled','pending');else if(state.kernelHealth==='failed')notice('failed','error');else if(state.kernelHealth==='checking')notice('checking');else if(state.staged)notice('staged','pending');else if(state.pendingReboot)notice('pending','pending');else notice('ready');controls();
 }
 function exec(command){return new Promise((resolve,reject)=>{
  if(!window.ksu||typeof window.ksu.exec!=='function'){reject(Error('host'));return;}
  const key='rkMali_'+Date.now()+'_'+id++,timer=setTimeout(()=>{delete window[key];reject(Error('error'));},15000);
  window[key]=(errno,out)=>{clearTimeout(timer);delete window[key];try{if(Number(errno))throw Error('error');const s=JSON.parse(out);if(s.schema!==1||!['on','off'].includes(s.requested)||!['stock','next'].includes(s.kernelRequested))throw Error('error');resolve(s);}catch{reject(Error('error'));}};
  try{window.ksu.exec(command,'{}',key);}catch{clearTimeout(timer);delete window[key];reject(Error('error'));}
 });}
 async function update(action){if(busy)return;busy=true;controls();try{state=await exec(bridge+' '+action);paint();$('changes').textContent=action==='status'?'':t('saved');}catch(e){notice(e.message==='host'?'host':'error','error');}finally{busy=false;controls();}}
 $('language').onclick=()=>{lang=lang==='zh'?'en':'zh';try{localStorage.setItem('mali-language',lang);}catch{}$('changes').textContent='';paint();};
 $('whole').onclick=()=>update('set '+(state.requested==='on'?'off':'on'));
 $('kernel').onchange=()=>{if(['stock','next'].includes($('kernel').value))update('kernel '+$('kernel').value);};
 $('refresh').onclick=()=>update('status');paint();update('status');
})();
