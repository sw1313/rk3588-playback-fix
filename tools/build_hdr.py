"""Build the original-HWC static-metadata adapter. Private libraries are link inputs only."""
import argparse,os,subprocess,hashlib,shutil
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('ndk','headers','private-libs','drm-headers','work'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();root=Path(__file__).resolve().parents[1];module=root/'magisk-hdr';w=a.work.resolve();w.mkdir(parents=True,exist_ok=True)
 h=a.headers.resolve();v=a.private_libs.resolve()
 host='windows-x86_64' if os.name=='nt' else 'darwin-x86_64' if os.uname().sysname=='Darwin' else 'linux-x86_64';ext='.exe' if os.name=='nt' else ''
 llvm=a.ndk.resolve()/'toolchains/llvm/prebuilt'/host/'bin'
 names={'include','include_vndk','include_types','include_private','include_outside_system'}
 includes=sorted(p for p in h.rglob('*') if p.is_dir() and p.name in names and 'external/libcxx' not in p.as_posix() and 'bionic' not in p.as_posix())
 includes+=sorted(p for p in (h/'generated-headers').rglob('gen') if (p/'android').exists())
 includes+=[h/'system/libfmq/base',h/'frameworks/native/libs/binder/ndk/include_cpp',h/'frameworks/native/libs/binder/ndk/include_ndk',a.drm_headers.resolve()]
 args=[llvm/('clang++'+ext),'--target=aarch64-linux-android32','-std=c++17','-O2','-fPIC','-shared','-fvisibility=hidden','-fno-exceptions','-fno-rtti','-nostdinc++','-nostdlib++','-DANDROID','-Wno-deprecated-declarations','-Wno-c99-designator','-isystem'+str(h/'external/libcxx/include'),'-isystem'+str(h/'generated-headers/bionic/libc/libc/android_vendor.32_arm_armv8-a_shared/gen/include')]
 args+=['-I'+str(p) for p in includes]
 args+=[module/'src/hwc_bridge.cpp','-o',w/'hwcomposer.rk30board.so','-Wl,--no-undefined','-Wl,-z,relro,-z,now','-ldl','-lm','-llog']
 args+=[v/name for name in ['libui.so','libutils.so','libcutils.so','libc++.so']]
 result=subprocess.run([str(x) for x in args],capture_output=True,text=True);(w/'compile.log').write_text(result.stdout+result.stderr,encoding='utf-8')
 if result.returncode:raise SystemExit('HWC adapter build failed: '+str(w/'compile.log'))
 subprocess.run([str(llvm/('clang'+ext)),'--target=aarch64-linux-android32','-O2',str(module/'src/check.c'),'-ldl','-o',str(w/'check64')],check=True)
 subprocess.run([str(llvm/('clang'+ext)),'--target=aarch64-linux-android32','-O2','-Wall','-Wextra','-Werror',str(module/'src/packet_probe.c'),'-o',str(w/'packet_probe64')],check=True)
 payload=w/'package'
 if payload.exists():raise SystemExit('Use a fresh package directory; existing payload is never overwritten')
 shutil.copytree(module,payload,ignore=shutil.ignore_patterns('system','bin','checksums.sha256'))
 lib=payload/'system/vendor/lib64/hw/hwcomposer.rk30board.so';lib.parent.mkdir(parents=True);shutil.copyfile(w/'hwcomposer.rk30board.so',lib)
 (payload/'bin').mkdir()
 for name in ['check64','packet_probe64']:shutil.copyfile(w/name,payload/'bin'/name)
 for binary in [lib,*list((payload/'bin').iterdir())]:subprocess.run([str(llvm/('llvm-strip'+ext)),'--strip-unneeded',str(binary)],check=True)
 files=sorted(p for p in payload.rglob('*') if p.is_file())
 for f in files:
  if f.suffix in {'.sh','.md','.txt','.prop','.sha256','.json','.py','.c','.cpp','.h'}:f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
 (payload/'checksums.sha256').write_bytes(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.relative_to(payload).as_posix()+'\n' for f in files).encode())
 print('Built HWC metadata adapter:',w)
if __name__=='__main__':main()
