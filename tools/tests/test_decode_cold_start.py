"""Exercise the actual Decode service's cold coordination with inert Android sh stubs."""
from pathlib import Path
import argparse,json,shlex,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
BASE='/data/local/tmp/rk-decode-cold-policy040'
STUBS=r'''
getprop(){ cat @BASE@/properties/"$1" 2>/dev/null; }
record(){ printf '%s\n' "$*" >> @BASE@/calls; }
start(){ record start "$@"; }
stop(){ record stop "$@"; }
setprop(){ record setprop "$@"; }
resetprop(){ record resetprop "$@"; }
reboot(){ record reboot; }
sync(){ :; }
timeout(){ record timeout;return 0; }
sha256sum(){ return 0; }
logcat(){ :; }
sleep(){
 ticks=$(cat @BASE@/ticks);ticks=$((ticks+1));echo "$ticks" > @BASE@/ticks
 if [ "$(cat @BASE@/delayed)" = yes ];then
  echo 'fixture-uuid profile released' > @BASE@/mali/cold-receipt
  echo active > @BASE@/properties/vendor.rkhdr.adapter
 fi
}
'''.replace('@BASE@',BASE)
def main():
 p=argparse.ArgumentParser();p.add_argument('--adb',required=True);p.add_argument('--serial',required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 adb=[a.adb,'-s',a.serial]
 def shell(cmd,check=True):return subprocess.run(adb+['shell',cmd],capture_output=True,text=True,encoding='utf-8',errors='replace',check=check)
 def put(path,data):
  with tempfile.NamedTemporaryFile(delete=False) as f:f.write(data.encode());name=Path(f.name)
  try:subprocess.run(adb+['push',str(name),path],capture_output=True,check=True)
  finally:name.unlink()
 assert shell('test ! -e '+BASE,False).returncode==0,'Fixture exists; inspect first'
 shell('mkdir -p '+BASE+'/properties '+BASE+'/module '+BASE+'/mali')
 results=[]
 try:
  service=(ROOT/'magisk-decode/service.sh').read_text(encoding='utf-8').replace('/proc/cmdline',BASE+'/cmdline').replace('/proc/sys/kernel/random/boot_id',BASE+'/boot-id').replace('/data/adb/rk3588-mali-global',BASE+'/mali')
  put(BASE+'/module/service.sh',service);put(BASE+'/module/common.sh',(ROOT/'magisk-decode/common.sh').read_text(encoding='utf-8')+STUBS)
  put(BASE+'/boot-id','fixture-uuid\n');put(BASE+'/properties/sys.boot_completed','1\n');put(BASE+'/properties/init.svc.vendor.hwcomposer-2-1','stopped\n')
  def case(name,cold,receipt,adapter,delayed,hdr,expected):
   shell('rm -f '+BASE+'/module/disable');put(BASE+'/calls','');put(BASE+'/ticks','0\n');put(BASE+'/delayed',delayed+'\n')
   put(BASE+'/cmdline',('initcall_blacklist=kbase_platform_driver_init' if cold else 'original')+'\n');put(BASE+'/mali/cold-receipt',receipt+'\n');put(BASE+'/properties/vendor.rkhdr.adapter',adapter+'\n')
   config=''.join(k+'=0\n' for k in ['mpp','rga','codec2','libyuv','software','codec2_sdk','codec2_sdk_next','hdr10plus'])+'hdr_static='+str(hdr)+'\n'
   put(BASE+'/module/active-config',config)
   r=shell('sh '+BASE+'/module/service.sh',False);assert r.returncode==expected,(name,r.returncode,r.stderr)
   calls=shell('cat '+BASE+'/calls').stdout
   if cold:assert not any(s.startswith(('stop ','start ','setprop ctl.restart')) for s in calls.splitlines()),(name,calls)
   else:assert 'setprop ctl.restart zygote' in calls
   status=shell('cat '+BASE+'/module/status.txt').stdout
   assert ('PASS unified decode stack' in status)==(expected==0),(name,status)
   results.append({'name':name,'passed':True,'exit':r.returncode})
  case('initial HWC active: no restart',True,'fixture-uuid profile released','active','no',1,0)
  case('delayed first release and adapter: wait',True,'old profile pending','pending','yes',1,0)
  case('stale receipt rejected',True,'old profile released','active','no',1,1)
  case('unreleased current receipt rejected',True,'fixture-uuid profile pending','active','no',1,1)
  case('cold HDR adapter missing: fail without restart',True,'fixture-uuid profile released','pending','no',1,1)
  case('HDR off still waits for first GPU release',True,'fixture-uuid profile released','pending','no',0,0)
  case('legacy startup still uses existing reload',False,'old profile released','pending','yes',1,0)
  a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps({'status':'isolated_shell_only','cases':results},indent=2));print('PASS',len(results),'Decode cold-start policy cases; no real service/property mutation')
 finally:shell('rm -r '+BASE)
if __name__=='__main__':main()
