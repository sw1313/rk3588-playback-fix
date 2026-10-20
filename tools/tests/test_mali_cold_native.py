"""Run the compiled native cold loader against fake files on Android.

RK_COLD_TEST cannot flash a block device, load a real module, resume a process,
set an Android property, or reboot. All effects are contained in a fixed fixture.
These tests are failure-path verification, not real cold boot acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,shlex,subprocess,tarfile,tempfile
ROOT=Path(__file__).resolve().parents[2]
BASE='/data/local/tmp/rk-cold-fixture'
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--adb',required=True);p.add_argument('--serial',required=True)
 p.add_argument('--adb-port',default='5037');p.add_argument('--su',default='su')
 p.add_argument('--ndk',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
 a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 compiler=a.ndk/'toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
 exe=a.out/'cold_fixture64'
 subprocess.run([str(compiler),'--target=aarch64-linux-android32','-DRK_COLD_TEST','-O2','-static','-Wall','-Wextra','-Werror',str(ROOT/'magisk-mali-global/src/cold_boot.c'),'-o',str(exe)],check=True)
 adb=[a.adb,'-P',a.adb_port,'-s',a.serial]
 def shell(cmd,check=True):return subprocess.run(adb+['shell',shlex.quote(a.su)+' -c '+shlex.quote(cmd)],capture_output=True,text=True,check=check)
 def push(src,dest):subprocess.run(adb+['push',str(src),dest],capture_output=True,check=True)
 require=shell('test ! -e '+BASE,False);assert require.returncode==0,'Fixture exists; inspect and remove explicitly first'
 push(exe,'/data/local/tmp/rk-cold-fixture64')
 shell('chmod 0700 /data/local/tmp/rk-cold-fixture64')
 shell('/data/local/tmp/rk-cold-fixture64 pidfd-test')
 original=b'O'*4096;candidate=b'C'*4096;kmd=b'test source module';csf=b'test firmware';g32=b'g32';g64=b'g64'
 profile='\n'.join(k+' '+v for k,v in {'profile_id':'a'*64,'original_sha256':sha(original),'original_bytes':'4096','kmd_sha256':sha(kmd),'csf_sha256':sha(csf),'gles32_sha256':sha(g32),'gles64_sha256':sha(g64)}.items())+'\n'
 defaults={'state/boot/original.img':original,'state/boot/candidate.img':candidate,'state/boot/image.sha256':sha(candidate)+'\n','state/boot/profile':profile,'boot-device':candidate,'boot-id':'fixture-boot-uuid\n','state/mode':'on\n','state/kernel-mode':'next\n','state/cold-mode':'on\n','state/cold-plan':'fixture-boot-uuid\n','module/active-mode':'on\n','module/kernel/rk_mali_next.ko':kmd,'firmware':csf,'gles32':g32,'gles64':g64,'properties/init.svc_debug_pid.surfaceflinger':'101\n','properties/init.svc_debug_pid.vendor.hwcomposer-2-1':'102\n','process-101':'State:\tT (stopped)\nPPid:\t1\n','process-102':'State:\tT (stopped)\nPPid:\t1\n'}
 results=[]
 defaults.update({'module/payload/lib/egl/libGLES_mali.so':g32,'module/payload/lib64/egl/libGLES_mali.so':g64,'state/cache/'+sha(csf)+'.bin':csf})
 for index,service in [(103,'vendor.gralloc-4-0'),(104,'zygote'),(105,'zygote_secondary')]:
  defaults['properties/init.svc_debug_pid.'+service]=str(index)+'\n'
  defaults['process-'+str(index)]='State:\tT (stopped)\nPPid:\t1\n'
 for index in range(101,106):defaults['process-exe-'+str(index)]='init\n'
 def case(name,changes,command,expected,restored=False,event=None,symlink=False):
  with tempfile.TemporaryDirectory() as td:
   folder=Path(td)/'fixture';folder.mkdir()
   files=defaults|changes
   for path,data in files.items():
    if data is None:continue
    f=folder/path;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(data.encode() if isinstance(data,str) else data)
   archive=Path(td)/'fixture.tar'
   with tarfile.open(archive,'w') as tar:
    for f in folder.rglob('*'):
     if f.is_file():
      ti=tar.gettarinfo(str(f),f.relative_to(folder).as_posix());ti.uid=ti.gid=0;ti.mode=0o600
      with f.open('rb') as stream:tar.addfile(ti,stream)
   push(archive,'/data/local/tmp/rk-cold-fixture.tar')
  shell('mkdir -p '+BASE+' && tar -xf /data/local/tmp/rk-cold-fixture.tar -C '+BASE)
  if symlink:shell('ln -s '+shlex.quote(symlink if isinstance(symlink,str) else 'unexpected-driver')+' '+BASE+'/gpu-driver')
  r=shell('/data/local/tmp/rk-cold-fixture64 '+command,False)
  assert r.returncode==expected,(name,r.returncode,r.stderr)
  disk=shell('sha256sum '+BASE+'/boot-device').stdout.split()[0]
  if restored:assert disk==sha(files['state/boot/original.img']),(name,'did not restore original')
  elif command=='apply' and expected==0:assert disk==sha(candidate),(name,'did not apply candidate')
  if event:assert event in shell('cat '+BASE+'/state/cold-journal').stdout,(name,'missing event')
  if command=='stage' and expected==0:
   held=shell('cat '+BASE+'/state/cold-pids').stdout
   assert len(held.splitlines())==3 and 'zygote' not in held,(name,'Zygote must never be held')
  results.append({'name':name,'exit':r.returncode,'passed':True})
  shell('rm -rf '+BASE)
 case('apply verifies write',{'boot-device':original},'apply',0)
 case('failed partial candidate write restores original',{'boot-device':original,'state/fail-write':'yes'},'apply',70,True,event='BOOT_WRITE_FAILED_ORIGINAL_RECOVERED')
 case('restore verifies original',{},'restore',0,True)
 alternate=b'U'*4096
 alternate_profile=profile.replace('original_sha256 '+sha(original),'original_sha256 '+sha(alternate))
 case('per-user original hash restores that user backup',{'state/boot/original.img':alternate,'state/boot/profile':alternate_profile},'restore',0,True)
 case('one user backup cannot restore another profile',{'state/boot/original.img':alternate},'restore',70)
 case('missing candidate still restores',{'state/boot/candidate.img':None},'restore',0,True)
 case('corrupt candidate still restores',{'state/boot/candidate.img':b'bad'},'restore',0,True)
 case('foreign boot never overwritten',{'boot-device':b'X'*4096},'restore',70,event='REFUSED_FOREIGN_BOOT')
 case('corrupt backup refuses write',{'state/boot/original.img':b'bad'},'restore',70)
 case('incomplete profile rejects',{'state/boot/profile':profile.replace('original_bytes 4096\n','')},'verify',65)
 case('extra profile tokens reject',{'state/boot/profile':profile+'original_bytes 4096 extra\n'},'verify',65)
 case('verified first bind releases without restarts',{},'stage',0,event='COLD_FIRST_BIND_GRAPHICS_RELEASED_NO_CLIENT_RESTART')
 case('normal Zygote process is never paused or required',{'process-104':'State:\tR\nPPid:\t1\n','process-exe-104':'app_process64'},'stage',0)
 case('init barrier passes current first release',{'state/cold-receipt':'fixture-boot-uuid '+'a'*64+' released'},'barrier',0,symlink='../../../bus/platform/drivers/rk_next_mali')
 case('init barrier stale release restores original',{'state/cold-receipt':'previous-uuid '+'a'*64+' released'},'barrier',30,True,event='COLD_INIT_BARRIER_TIMEOUT',symlink='../../../bus/platform/drivers/rk_next_mali')
 case('preflight permits matching enabled payload before injector starts',{},'preflight',0)
 case('preflight disabled restores before injector starts',{'module/disable':''},'preflight',30,True,event='COLD_DISABLED_OR_MODULE_REMOVED')
 case('preflight old failed trial restores before injector starts',{'state/cold-pending':'previous-uuid'},'preflight',30,True,event='PREVIOUS_COLD_BOOT_UNQUALIFIED')
 case('preflight bad payload restores before injector starts',{'module/kernel/rk_mali_next.ko':'wrong'},'preflight',30,True,event='COLD_PREFLIGHT_PAYLOAD_MISMATCH')
 case('manager disabled restores boot',{'module/disable':''},'stage',30,True)
 case('manager removed restores boot',{'module/remove':''},'stage',30,True)
 case('parent off restores boot',{'state/mode':'off'},'stage',30,True)
 case('previous incomplete boot restores',{'state/cold-pending':'previous-uuid'},'stage',30,True,event='PREVIOUS_COLD_BOOT_UNQUALIFIED')
 case('stale overlay plan rejected',{'state/cold-plan':'previous-uuid'},'stage',30,True)
 case('wrong firmware restores',{'firmware':'wrong'},'stage',30,True)
 case('wrong ARM32 restores',{'gles32':'wrong'},'stage',30,True)
 case('wrong ARM64 restores',{'gles64':'wrong'},'stage',30,True)
 case('wrong KMD restores',{'module/kernel/rk_mali_next.ko':'wrong'},'stage',30,True)
 case('graphics already executing refuses load',{'process-101':'State:\tR\nPPid:\t1\n'},'stage',30,True)
 case('non-init parent refuses release',{'process-102':'State:\tT\nPPid:\t99\n'},'stage',30,True)
 case('stopped executed graphics process refuses release',{'process-exe-101':'surfaceflinger'},'stage',30,True)
 case('already bound GPU refuses load',{},'stage',30,True,event='REFUSED_ALREADY_BOUND_GPU',symlink=True)
 case('failed module first bind restores',{'state/fail-load':'yes'},'stage',30,True,event='COLD_FIRST_BIND_FAILED')
 case('fence fix default remains off',{},'stage',0,event='csf_fence_safe=0')
 case('fence fix on latched before release',{'state/fence-mode':'on\n'},'stage',0,event='csf_fence_safe=1')
 case('fence fix invalid state remains off',{'state/fence-mode':'invalid\n'},'stage',0,event='csf_fence_safe=0')
 case('qualification without release refuses',{},'qualify',67)
 case('current release and health qualifies',{'state/cold-receipt':'fixture-boot-uuid '+'a'*64+' released','state/kernel-run/result':'COLD_AUTOMATED_PASS'},'qualify',0,symlink='../../../bus/platform/drivers/rk_next_mali')
 case('stale release cannot qualify',{'state/cold-receipt':'old-uuid '+'a'*64+' released','state/kernel-run/result':'COLD_AUTOMATED_PASS'},'qualify',67,symlink='../../../bus/platform/drivers/rk_next_mali')
 (a.out/'native-results.json').write_text(json.dumps({'status':'fixture_only','pidfd_syscalls_checked_on_disposable_child':True,'cases':results},indent=2))
 print('PASS',len(results),'isolated native cases; no actual boot/device GPU mutation')
if __name__=='__main__':main()
