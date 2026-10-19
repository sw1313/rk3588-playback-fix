"""Exercise the actual policy startup fallback with confined Android fake files."""
import argparse,hashlib,json,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE='/data/local/tmp/rk-display-policy-guard-test'
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['adb','serial']:p.add_argument('--'+n,required=True)
 p.add_argument('--port',default='5038');p.add_argument('--work',type=Path,required=True);a=p.parse_args()
 a.work.mkdir(parents=True,exist_ok=False);adb=[a.adb,'-P',a.port,'-s',a.serial];cases=[]
 def shell(cmd):return subprocess.run(adb+['shell',cmd],capture_output=True,text=True,encoding='utf-8',timeout=15)
 def check(ok,label):
  if not ok:raise AssertionError(label)
  cases.append(label)
 check(shell('test ! -e '+BASE).returncode==0,'fresh fixture')
 module=BASE+'/module';state=BASE+'/state'
 def put(path,data):
  with tempfile.NamedTemporaryFile('w',encoding='utf-8',newline='\n',delete=False) as f:f.write(data);name=f.name
  try:subprocess.run(adb+['push',name,path],capture_output=True,check=True,timeout=15)
  finally:Path(name).unlink()
 def call(mode):return shell('sh '+module+'/display-policy-state.sh '+mode)
 def read(path):return shell('cat '+path).stdout.strip()
 try:
  shell('mkdir -p '+module+'/components/display_policy/variants/oat/arm64 '+state+' '+BASE+'/system-oat')
  source=(ROOT/'magisk-decode/display-policy-state.sh').read_text(encoding='utf-8')
  source=source.replace('/data/adb/rk3588-display-policy',state).replace('/proc/sys/kernel/random/boot_id',BASE+'/boot').replace('/system/framework/services.jar',BASE+'/services.jar')
  source=source.replace('/system/framework/oat/arm64',BASE+'/system-oat').replace('/proc/$pid/maps',BASE+'/maps')
  source=source.replace('/data/dalvik-cache/arm64',BASE+'/cache')
  mocks='set -eu\ngetprop() { case "$1" in sys.boot_completed) cat '+BASE+'/completed;; *) cat '+BASE+'/saved-hdr;; esac; }\n'
  mocks+="resetprop() { printf '%s\\n' \"$*\" > "+BASE+'/applied-hdr; }\npidof() { cat '+BASE+'/pid; }'
  source=source.replace('set -eu',mocks)
  put(module+'/display-policy-state.sh',source);put(BASE+'/boot','boot1');put(BASE+'/completed','0');put(BASE+'/pid','42')
  config='codec2=1\ndisplay_qms=1\ndisplay_policy=1\n'
  put(module+'/config',config)
  check(call('begin').returncode==0 and 'display_policy=0' in read(module+'/config'),'missing preflight disables only policy')
  payload='fixture not a real framework';digest=hashlib.sha256(payload.encode()).hexdigest()
  put(BASE+'/services.jar',payload);put(module+'/components/display_policy/preflight',digest)
  put(module+'/config',config);put(module+'/active-config',config)
  check(call('begin').returncode==0 and 'display_policy=0' in read(module+'/config'),'jar-only candidate cannot start without trusted OAT inputs')
  oat_manifest=''
  for artifact in ['services.odex','services.vdex','services.art','ethernet-service.odex','ethernet-service.vdex']:
   put(module+'/components/display_policy/variants/oat/arm64/'+artifact,payload)
   put(BASE+'/system-oat/'+artifact,payload)
   oat_manifest+=digest+'  variants/oat/arm64/'+artifact+'\n'
  put(module+'/components/display_policy/oat.sha256',oat_manifest)
  put(module+'/components/display_policy/oat-inputs.sha256',digest+'  '+BASE+'/services.jar\n')
  maps=BASE+'/system-oat/services.odex\n'+BASE+'/system-oat/ethernet-service.odex\n'
  put(BASE+'/maps',maps)
  put(module+'/config',config)
  shell('mkdir -p '+BASE+'/cache')
  cache_name='system@framework@services.jar@classes.dex'
  put(BASE+'/cache/'+cache_name,'old extracted dex cache')
  put(BASE+'/cache/unrelated-app.dex','keep unrelated app cache')
  check(call('begin').returncode==0 and read(state+'/pending')=='boot1 '+digest,'record current boot and variant')
  archived=read(state+'/cache-last')
  check(read(archived+'/'+cache_name)=='old extracted dex cache' and shell('test ! -e '+BASE+'/cache/'+cache_name).returncode==0,'conflicting system-server cache archived before a new trial')
  check(read(BASE+'/cache/unrelated-app.dex')=='keep unrelated app cache','unrelated app cache untouched')
  check(call('qualified').returncode!=0,'boot incomplete cannot qualify')
  put(BASE+'/completed','1');put(BASE+'/services.jar','wrong')
  check(call('qualified').returncode!=0,'wrong active framework cannot qualify')
  put(BASE+'/services.jar',payload)
  put(BASE+'/system-oat/services.odex','wrong')
  check(call('qualified').returncode!=0,'wrong mounted OAT cannot qualify')
  put(BASE+'/system-oat/services.odex',payload);put(BASE+'/maps','unrelated')
  check(call('qualified').returncode!=0,'correct files without actual system-server OAT mapping cannot qualify')
  put(BASE+'/maps',maps)
  check(call('qualified').returncode==0 and read(state+'/pending')==read(state+'/qualified'),'completed exact startup qualifies')
  put(BASE+'/boot','boot2')
  check(call('begin').returncode==0 and read(state+'/pending')=='boot2 '+digest and 'display_policy=1' in read(module+'/config'),'qualified prior startup permits next boot')
  put(BASE+'/boot','boot3')
  check(call('begin').returncode==0 and read(module+'/config')==config.replace('display_policy=1','display_policy=0').strip(),'unqualified prior boot restores only policy selection')
  check('previous-start-not-qualified' in read(state+'/failure'),'failed startup recorded')
  check(read(state+'/failed-pending')=='boot2 '+digest and shell('test ! -e '+state+'/pending').returncode==0,'failed receipt archived and consumed')
  check(shell("stat -c '%a' "+module+'/config').stdout.strip()=='600','fallback keeps cold-loader config permissions')
  put(module+'/config',config);put(BASE+'/boot','boot4')
  check(call('begin').returncode==0 and read(module+'/config')==config.strip() and read(state+'/pending')=='boot4 '+digest,'explicit enable after fallback starts a fresh trial')
  check(call('qualified').returncode==0,'fresh retry must independently qualify')
  put(BASE+'/boot','boot5')
  check(call('begin').returncode==0 and read(module+'/config')==config.strip(),'qualified retry remains enabled after another reboot')
  put(module+'/active-config',config);put(BASE+'/saved-hdr','')
  check(call('restore-hdr').returncode==0 and shell('test ! -e '+BASE+'/applied-hdr').returncode==0,'unset HDR choice never changes firmware default')
  put(BASE+'/saved-hdr','invalid')
  check(call('restore-hdr').returncode==0 and shell('test ! -e '+BASE+'/applied-hdr').returncode==0,'invalid HDR choice never changes output')
  for value in ['0','1']:
   put(BASE+'/saved-hdr',value)
   check(call('restore-hdr').returncode==0 and read(BASE+'/applied-hdr')=='-n vendor.hwc.hdr_force_disable '+value,'restore real saved HDR '+value)
  put(module+'/active-config',config.replace('display_policy=1','display_policy=0'));shell('rm -f '+BASE+'/applied-hdr')
  check(call('restore-hdr').returncode==0 and shell('test ! -e '+BASE+'/applied-hdr').returncode==0,'disabled component ignores saved HDR choice')
  put(module+'/config',config)
  check(call('failed').returncode==0 and read(module+'/config')==config.replace('display_policy=1','display_policy=0').strip(),'runtime failure preserves QMS and codec selections')
  check(read(state+'/failed-pending')=='boot5 '+digest and shell('test ! -e '+state+'/pending').returncode==0,'runtime failure also consumes only its failed receipt')
  put(module+'/config',config);put(BASE+'/boot','boot6')
  check(call('begin').returncode==0 and read(state+'/pending')=='boot6 '+digest and read(module+'/config')==config.strip(),'explicit retry after runtime failure is not poisoned by the old trial')
  check(read(archived+'/'+cache_name)=='old extracted dex cache','cache backup survives later boots and fallback')
 finally:
  shell('rm -rf /data/local/tmp/rk-display-policy-guard-test')
 result={'passed':True,'cases':cases,'real_boot_or_module_flags_changed':False}
 (a.work/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 print('PASS policy startup fallback cases='+str(len(cases))+'; isolated fake boot/framework only')
if __name__=='__main__':main()
