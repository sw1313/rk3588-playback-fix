"""Build the hash-gated API32 global Mali installer without redistributing Mali blobs.

The installer fetches both native drivers from pinned HTTPS upstream URLs. All
included interface libraries are built from the bundled frozen AOSP source.
Original firmware libraries are link inputs only and are never copied to output.
"""
import argparse,hashlib,json,os,shutil,subprocess,zipfile
from pathlib import Path
from package_licenses import stage as stage_licenses
from stage_mali_kernel import stage as stage_kernel
ROOT=Path(__file__).resolve().parents[1];MODULE=ROOT/'magisk-mali-global'
PACKAGES={'common':('android.hardware.common-V2-ndk.so',2),'graphics-common':('android.hardware.graphics.common-V3-ndk.so',3),'allocator':('android.hardware.graphics.allocator-V1-ndk.so',1)}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args):
 r=subprocess.run([str(x) for x in args],capture_output=True,text=True,encoding='utf-8',errors='replace')
 if r.returncode:raise SystemExit((r.stdout+r.stderr)[-5000:])
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ['ndk','sdk','headers','binder32','binder64','libcpp32','libcpp64','work']:parser.add_argument('--'+name,type=Path,required=True)
 parser.add_argument('--kernel-artifacts',type=Path,required=True)
 parser.add_argument('--kernel-runtime',type=Path,required=True)
 parser.add_argument('--source-ref',help='Source tag/ref; working-tree marks an unpublished local candidate')
 parser.add_argument('--build-tools',default='35.0.0');parser.add_argument('--aidl-build-tools',default='34.0.0')
 a=parser.parse_args();w=a.work.resolve();out=w/'package'
 if out.exists():raise SystemExit('Use a fresh output directory')
 w.mkdir(parents=True,exist_ok=True);h=a.headers.resolve();src=MODULE/'src';sdk=a.sdk.resolve();ndk=a.ndk.resolve()
 host='windows-x86_64' if os.name=='nt' else 'linux-x86_64';ext='.exe' if os.name=='nt' else '';llvm=ndk/'toolchains/llvm/prebuilt'/host/'bin';bt=sdk/'build-tools'/a.build_tools;aidl=sdk/'build-tools'/a.aidl_build_tools/('aidl'+ext)
 for key,(soname,version) in PACKAGES.items():
  gen=w/'generated'/key;gen.mkdir(parents=True)
  args=[aidl,'--lang=ndk','--structured','--stability=vintf','--min_sdk_version=32','--version='+str(version),'--hash='+(src/'aidl'/key/'.hash').read_text(encoding='utf-8').strip(),'-o',gen,'-h',gen]
  args+=['-I'+str(src/'aidl'/name) for name in PACKAGES];args+=list((src/'aidl'/key).rglob('*.aidl'));run(args)
 shutil.copytree(MODULE,out,ignore=shutil.ignore_patterns('payload','system','bin','active*','status.txt','compositor-status.txt','disable','checksums.sha256','payload.sha256','skip_mount'))
 (out/'bin').mkdir()
 run([llvm/('clang'+ext),'--target=aarch64-linux-android32','-std=c11','-O2','-static','-Wall','-Wextra','-Werror','-ffile-prefix-map='+ROOT.as_posix()+'=/src',src/'cold_boot.c','-Wl,-z,relro,-z,now','-o',out/'bin/cold_boot64'])
 common_args=['-std=c++17','-O2','-fPIC','-shared','-fno-exceptions','-fno-rtti','-nostdinc++','-nostdlib++','-DANDROID','-DBINDER_STABILITY_SUPPORT','-isystem'+str(h/'external/libcxx/include'),'-isystem'+str(h/'generated-headers/bionic/libc/libc/android_vendor.32_arm_armv8-a_shared/gen/include'),'-I'+str(src/'binder-cpp')]
 for original,replacement in [(MODULE.resolve(),'/src'),(h,'/platform'),(ndk,'/ndk'),(w,'/obj')]:
  for spelling in {str(original).replace('\\','/'),str(original).replace('/','\\')}:common_args+=['-ffile-prefix-map='+spelling+'='+replacement]
 common_args+=['-I'+str(h/'frameworks/native/libs/binder/ndk'/n) for n in ['include_ndk','include_platform']]
 common_args+=['-I'+str(w/'generated'/key) for key in PACKAGES]
 for abi,target,directory in [('32','armv7a-linux-androideabi32','lib'),('64','aarch64-linux-android32','lib64')]:
  dest=out/'payload'/directory;dest.mkdir(parents=True)
  binder=getattr(a,'binder'+abi).resolve();cpp=getattr(a,'libcpp'+abi).resolve()
  for name,(soname,version) in PACKAGES.items():
   args=[llvm/('clang++'+ext),'--target='+target,*common_args,*list((w/'generated'/name).rglob('*.cpp')),'-o',dest/soname,'-Wl,-soname,'+soname,'-Wl,--no-undefined','-Wl,-z,relro,-z,now',binder,cpp,'-llog']
   if name!='common':args+=[dest/PACKAGES['common'][0]]
   if name=='allocator':args+=[dest/PACKAGES['graphics-common'][0]]
   run(args)
  args=[llvm/('clang'+ext),'--target='+target,'-O2','-Wall','-Wextra','-Werror','-I'+str(src/'include'),'-I'+str(h/'hardware/libhardware/include'),'-I'+str(h/'system/core/libcutils/include'),'-I'+str(h/'system/core/libsystem/include'),src/'graphics_probe.c','-ldl','-o',out/'bin'/('graphics_probe'+abi)];run(args)
 classes=w/'classes';classes.mkdir();dex=w/'dex';dex.mkdir()
 run(['javac','-source','8','-target','8','-d',classes,src/'FetchDriver.java'])
 run([bt/('d8.bat' if os.name=='nt' else 'd8'),'--min-api','32','--output',dex,classes/'FetchDriver.class'])
 with zipfile.ZipFile(out/'bin/fetch-driver.jar','w',zipfile.ZIP_DEFLATED) as z:z.write(dex/'classes.dex','classes.dex')
 lines=[sha(f)+'  '+f.relative_to(out).as_posix() for f in sorted((out/'payload').rglob('*.so'))]
 for line in (MODULE/'contracts/driver-sources.txt').read_text(encoding='utf-8').splitlines():
  abi,size,digest,url=line.split();directory='lib' if abi=='arm' else 'lib64';lines.append(digest+'  payload/'+directory+'/egl/libGLES_mali.so')
 (out/'payload.sha256').write_bytes(('\n'.join(sorted(lines))+'\n').encode())
 stage_licenses(out,'mali-global',source_ref=a.source_ref)
 stage_kernel(out,a.kernel_artifacts,a.kernel_runtime)
 report=ROOT/'docs/mali-dvfs-031.zh-CN.md'
 if report.exists():shutil.copyfile(report,out/'GPU_RESPONSE_VALIDATION.zh-CN.md')
 cold_report=ROOT/'docs/mali-cold-binding-040.zh-CN.md'
 if cold_report.exists():shutil.copyfile(cold_report,out/'COLD_BINDING_CANDIDATE.zh-CN.md')
 repair_report=ROOT/'docs/mali-zygisk-repair-041.zh-CN.md'
 if repair_report.exists():shutil.copyfile(repair_report,out/'ZYGISK_REPAIR_VALIDATION.zh-CN.md')
 fence_report=ROOT/'docs/mali-csf-fence-042.zh-CN.md'
 if fence_report.exists():shutil.copyfile(fence_report,out/'CSF_FENCE_CANDIDATE.zh-CN.md')
 shutil.copyfile(ROOT/'mali-kernel/UPSTREAM.json',out/'KERNEL_UPSTREAM.json')
 files=sorted(f for f in out.rglob('*') if f.is_file())
 for f in files:
  if f.suffix in {'.sh','.md','.txt','.json','.sha256','.prop','.java','.c','.cpp','.h','.aidl'}:f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
 (out/'checksums.sha256').write_bytes(''.join(sha(f)+'  '+f.relative_to(out).as_posix()+'\n' for f in files).encode())
 version=dict(line.split('=',1) for line in (MODULE/'module.prop').read_text(encoding='utf-8').splitlines() if '=' in line)['version']
 dest=w/('rk3588-mali-global-'+version+'.zip')
 with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED) as z:
  for f in sorted(out.rglob('*')):
   if f.is_file():z.write(f,f.relative_to(out).as_posix())
 with zipfile.ZipFile(dest) as z:assert z.testzip() is None
 print(dest,sha(dest))
if __name__=='__main__':main()
