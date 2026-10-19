"""Build the read-only NV15/P010 acceptance probe using private API32 link inputs."""
from pathlib import Path
import argparse,os,subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['ndk','headers','private-libs','work']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();h=a.headers.resolve();v=a.private_libs.resolve();w=a.work.resolve();w.mkdir(parents=True,exist_ok=False)
    host='windows-x86_64' if os.name=='nt' else 'darwin-x86_64' if os.uname().sysname=='Darwin' else 'linux-x86_64'
    cc=a.ndk.resolve()/'toolchains/llvm/prebuilt'/host/'bin'/('clang++.exe' if os.name=='nt' else 'clang++')
    includes=sorted(x for x in h.rglob('*') if x.is_dir() and x.name in {'include','include_vndk','include_types','include_private','include_outside_system'} and 'external/libcxx' not in x.as_posix() and 'bionic' not in x.as_posix())
    includes+=sorted(x for x in (h/'generated-headers').rglob('gen') if (x/'android').exists())
    includes+=[h/'system/libfmq/base',h/'frameworks/native/libs/binder/ndk/include_cpp',h/'frameworks/native/libs/binder/ndk/include_ndk']
    args=[cc,'--target=aarch64-linux-android32','-O2','-std=c++17','-fno-exceptions','-fno-rtti','-nostdinc++','-nostdlib++','-DANDROID','-Wno-deprecated-declarations','-isystem'+str(h/'external/libcxx/include'),'-isystem'+str(h/'generated-headers/bionic/libc/libc/android_vendor.32_arm_armv8-a_shared/gen/include')]
    args+=['-I'+str(x) for x in includes]
    args+=[Path(__file__).with_name('native_hevc_precision.cpp'),'-o',w/'native_hevc_precision64','-Wl,--no-undefined','-lmediandk','-landroid','-ldl']
    args+=[v/x for x in ['libui.so','libutils.so','libcutils.so','libc++.so']]
    r=subprocess.run([str(x) for x in args],capture_output=True,text=True)
    (w/'build.log').write_text(r.stdout+r.stderr,encoding='utf-8')
    if r.returncode:raise SystemExit('Build failed: '+str(w/'build.log'))
    print(w/'native_hevc_precision64')

if __name__=='__main__':main()
