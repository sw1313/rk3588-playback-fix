"""Run the real Zygisk health policy with inert Android shell fixtures."""
from pathlib import Path
import argparse,subprocess,tempfile,json
ROOT=Path(__file__).resolve().parents[2];BASE='/data/local/tmp/rk-zygisk-policy041'
def main():
 p=argparse.ArgumentParser();p.add_argument('--adb',required=True);p.add_argument('--serial',required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();adb=[a.adb,'-s',a.serial]
 def shell(cmd,check=True):return subprocess.run(adb+['shell',cmd],capture_output=True,text=True,encoding='utf-8',check=check)
 def put(path,data):
  with tempfile.NamedTemporaryFile(delete=False) as f:f.write(data.encode());name=Path(f.name)
  try:subprocess.run(adb+['push',str(name),path],capture_output=True,check=True)
  finally:name.unlink()
 assert shell('test ! -e '+BASE,False).returncode==0
 shell('mkdir -p '+BASE+'/module '+BASE+'/state/kernel-run '+BASE+'/zn/bin '+BASE+'/lsp')
 try:
  source=(ROOT/'magisk-mali-global/zygisk-health.sh').read_text(encoding='utf-8').replace('/data/adb/rk3588-mali-global',BASE+'/state').replace('/data/adb/modules/zygisksu',BASE+'/zn').replace('/data/adb/modules/zygisk_lsposed',BASE+'/lsp')
  source=source.replace('set -eu','getprop(){ case "$1" in init.svc_debug_pid.zygote) echo 101;;*) echo 102;;esac; }\npidof(){ cat '+BASE+'/lspd; }\nset -eu')
  put(BASE+'/module/zygisk-health.sh',source);put(BASE+'/zn/bin/zygiskd','#!/system/bin/sh\ncat '+BASE+'/status\n');shell('chmod 0755 '+BASE+'/zn/bin/zygiskd')
  valid='inject_state:1\nmodules_with_issue:0\nzygote_state_0:1,0,101,zygote,3,0\nzygote_state_1:1,1,102,zygote_secondary,3,0\nmodules64:1,zygisk_lsposed\nmodules32:1,zygisk_lsposed\n'
  cases=[('both ABIs and LSPosed active',valid,'500',0),('failed injection refuses qualification',valid.replace('inject_state:1','inject_state:0'),'500',1),('stale primary PID refuses',valid.replace(',101,',',999,'),'500',1),('secondary injection pending refuses',valid.replace('zygote_secondary,3,0','zygote_secondary,1,0'),'500',1),('missing 32-bit LSPosed refuses',valid.replace('modules32:1,zygisk_lsposed','modules32:0'),'500',1),('missing lspd refuses',valid,'',1)]
  result=[]
  for name,status,pid,expected in cases:
   put(BASE+'/status',status);put(BASE+'/lspd',pid);r=shell('sh '+BASE+'/module/zygisk-health.sh',False);assert r.returncode==expected,(name,r.returncode,r.stderr);result.append({'name':name,'passed':True})
  shell('touch '+BASE+'/zn/disable');r=shell('sh '+BASE+'/module/zygisk-health.sh',False);assert r.returncode==0;result.append({'name':'explicit external Zygisk disable respected','passed':True})
  put(BASE+'/state/zygisk-required-boot',shell('cat /proc/sys/kernel/random/boot_id').stdout);put(BASE+'/status',valid.replace('inject_state:1','inject_state:0'))
  r=shell('sh '+BASE+'/module/zygisk-health.sh',False);assert r.returncode!=0;result.append({'name':'boot-required injector cannot bypass health by auto-disabling','passed':True})
  a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2));print('PASS',len(result),'Zygisk qualification policies; no real injection/settings mutation')
 finally:shell('rm -r '+BASE)
if __name__=='__main__':main()
