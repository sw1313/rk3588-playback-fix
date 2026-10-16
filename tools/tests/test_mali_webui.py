"""Run real Android sh state/boot-latch tests in a disposable isolated fixture."""
from pathlib import Path
import argparse,json,shlex,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
def main():
 p=argparse.ArgumentParser();p.add_argument('--adb',default='adb');p.add_argument('--serial');a=p.parse_args()
 adb=[a.adb]+(['-s',a.serial] if a.serial else [])
 fixture='/data/local/tmp/rk-mali-ui-test068';current=fixture+'/modules/rk3588_mali_global';staged=fixture+'/modules_update/rk3588_mali_global';state=fixture+'/state'
 def shell(s):return subprocess.run(adb+['shell',s],capture_output=True,text=True,encoding='utf-8',errors='replace',check=True).stdout
 def put(path,text):
  with tempfile.NamedTemporaryFile(delete=False) as f:f.write(text.encode());local=Path(f.name)
  try:
   subprocess.run(adb+['push',str(local),path],check=True,capture_output=True)
  finally:local.unlink()
 checks=0
 def expect(v,message):
  nonlocal checks
  assert v,message;checks+=1
 def invoke(*args):return json.loads(shell('sh '+current+'/webui.sh '+' '.join(shlex.quote(x) for x in args)))
 shell('test ! -e '+fixture+' && mkdir -p '+current+' '+staged+' '+state)
 try:
  for name in ['kernel-state.sh','mode-control.sh','webui.sh','module.prop']:
   text=(ROOT/'magisk-mali-global'/name).read_text(encoding='utf-8')
   text=text.replace('/data/adb/modules_update',fixture+'/modules_update').replace('/data/adb/modules',fixture+'/modules').replace('/data/adb/rk3588-mali-global',state)
   if name=='kernel-state.sh':text+='\nkernel_supported(){ [ "$(cat '+state+'/supported)" = yes ]; }\nkernel_active(){ cat '+state+'/actual-kernel; }\n'
   put(current+'/'+name,text)
  put(state+'/supported','yes\n');put(state+'/actual-kernel','stock\n');put(state+'/mode','on\n');put(state+'/kernel-mode','stock\n')
  s=invoke('status');expect(not s['activeKnown'] and s['pendingReboot'],'unknown boot receipt is pending')
  shell('cat /proc/sys/kernel/random/boot_id > '+current+'/active-boot-id');put(current+'/active-mode','on\n');put(current+'/active-version','0.3.0\n')
  s=invoke('status');expect(s['activeKnown'] and not s['pendingReboot'],'original pair acknowledged')
  s=invoke('kernel','next');expect(s['kernelRequested']=='next' and s['kernelEffective']=='next' and s['kernelActive']=='stock' and s['pendingReboot'],'save is never live activation')
  s=invoke('set','off');expect(s['kernelRequested']=='next' and s['kernelEffective']=='stock','parent off retains child pair')
  s=invoke('set','on');expect(s['kernelEffective']=='next','parent on restores child selection')
  invoke('set','off');s=invoke('kernel','next');expect(s['requested']=='on','select new pair links userspace on')
  s=invoke('kernel','stock');expect(not s['pendingReboot'],'cancel pending pair change')
  put(state+'/supported','no\n');s=invoke('status');expect(not s['supported'] and s['kernelEffective']=='stock','unsupported firmware stays original')
  bad=subprocess.run(adb+['shell','sh '+current+'/webui.sh kernel next'],capture_output=True);expect(bad.returncode!=0,'unsupported pair cannot be enabled')
  put(state+'/supported','yes\n');invoke('kernel','next')
  # A completed prior startup must not prevent the next requested boot.
  shell('sh -c '+shlex.quote('. '+current+'/kernel-state.sh; kernel_boot_plan'))
  expect(shell('cat '+state+'/kernel-boot-plan').strip()=='next','boot plans requested pair')
  expect(shell('cat '+state+'/kernel-pending').strip()==shell('cat /proc/sys/kernel/random/boot_id').strip(),'latch written before transition')
  put(state+'/kernel-pending','different-boot\n')
  shell('sh -c '+shlex.quote('. '+current+'/kernel-state.sh; kernel_boot_plan'))
  s=invoke('status');expect(s['kernelRequested']=='stock' and s['recovery']=='interrupted_previous_boot','interrupted startup persists kernel-only rollback')
  expect(s['requested']=='on' and shell('cat '+state+'/kernel-boot-plan').strip()=='stock','rollback retains global userspace')
  put(state+'/kernel-pending','old-failed-attempt\n');invoke('kernel','next')
  expect(shell('test ! -e '+state+'/kernel-pending && echo cleared').strip()=='cleared','explicit retry clears an inactive failed latch')
  shell('touch '+current+'/disable');s=invoke('status');expect(not s['moduleEnabled'] and s['effective']=='off','manager disable suppresses both')
  shell('rm '+current+'/disable')
  put(staged+'/module.prop','id=rk3588_mali_global\nversion=0.3.1\n')
  for name in ['kernel-state.sh','mode-control.sh']:put(staged+'/'+name,shell('cat '+current+'/'+name))
  s=invoke('status');expect(s['staged'] and s['version']=='0.3.1' and s['activeVersion']=='0.3.0' and s['pendingReboot'],'staged update is distinguished from current boot')
  for args in [('kernel','next;id'),('set','on','extra'),('status','extra'),('unknown',)]:
   result=subprocess.run(adb+['shell','sh '+current+'/webui.sh '+' '.join(shlex.quote(x) for x in args)],capture_output=True)
   expect(result.returncode!=0,'fixed command boundary')
  print(json.dumps({'checks':checks,'android_shell_passed':True,'live_module_modified':False}))
 finally:shell('rm -r '+fixture)
if __name__=='__main__':main()
