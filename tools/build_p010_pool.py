"""Build the internal guarded ARM64 P010 pool component; firmware is link input only."""
import argparse,hashlib,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ['ndk','headers','platform-libs','codec-buffer-header','video-api-header','work']:p.add_argument('--'+key,type=Path,required=True)
 a=p.parse_args();w=a.work.resolve();w.mkdir(parents=True,exist_ok=True);out=w/'package'
 if out.exists():raise SystemExit('Use a fresh output directory')
 h=a.headers.resolve();v=a.platform_libs.resolve();llvm=a.ndk.resolve()/'toolchains/llvm/prebuilt/windows-x86_64/bin'
 names={'include','include_vndk','include_types','include_private','include_outside_system'}
 inc=sorted(x for x in h.rglob('*') if x.is_dir() and x.name in names and 'external/libcxx' not in x.as_posix() and 'bionic' not in x.as_posix())
 inc+=sorted(x for x in (h/'generated-headers').rglob('gen') if (x/'android').exists())
 inc+=[h/'system/libfmq/base',h/'frameworks/native/libs/binder/ndk/include_cpp',h/'frameworks/native/libs/binder/ndk/include_ndk']
 shutil.copytree(ROOT/'magisk-codec2-pool',out)
 (out/'include/media/hardware').mkdir(parents=True)
 shutil.copyfile(a.codec_buffer_header,out/'include/Codec2Buffer.h');shutil.copyfile(a.video_api_header,out/'include/media/hardware/VideoAPI.h')
 (out/'src').mkdir();shutil.copyfile(ROOT/'magisk-codec2/src/p010_pool.cpp',out/'src/p010_pool.cpp')
 # Freeze the exact public byte patcher used for the generated private copy.
 shutil.copyfile(ROOT/'magisk-hdr/src/byte_patch.c',out/'src/byte_patch.c')
 shutil.copyfile(ROOT/'tools/hdr-probes/p010_pool_test.cpp',out/'src/p010_pool_test.cpp')
 # Preserve a test source include that resolves within the component snapshot.
 test=(out/'src/p010_pool_test.cpp').read_text(encoding='utf-8').replace('../../magisk-codec2/src/p010_pool.cpp','p010_pool.cpp')
 (out/'src/p010_pool_test.cpp').write_text(test,encoding='utf-8',newline='\n')
 (out/'system/lib64').mkdir(parents=True);(out/'bin').mkdir()
 libs=['libutils.so','libcutils.so','libc++.so','libstagefright_foundation.so']
 args=[str(llvm/'clang++.exe'),'--target=aarch64-linux-android32','-std=c++17','-O2','-fvisibility=hidden','-fno-exceptions','-fno-rtti','-nostdinc++','-nostdlib++','-DANDROID','-Wno-deprecated-declarations','-Wno-c99-designator','-isystem'+str(h/'external/libcxx/include'),'-isystem'+str(h/'generated-headers/bionic/libc/libc/android_vendor.32_arm_armv8-a_shared/gen/include')]
 args+=['-ffile-prefix-map='+str(ROOT)+'=/src/rk3588-playback-fix','-ffile-prefix-map='+str(h)+'=/src/platform']+['-I'+str(x) for x in inc]+['-I'+str(out/'include')]
 link=['-Wl,--no-undefined','-Wl,-z,relro,-z,now','-ldl','-llog']+[str(v/n) for n in libs]
 for source,name,flags in [('p010_pool.cpp',out/'system/lib64/libcp10.so',['-fPIC','-shared']),('p010_pool_test.cpp',out/'bin/pool-test64',[])]:
  subprocess.run(args+flags+[str(out/'src'/source),'-o',str(name)]+link,check=True)
 subprocess.run([str(llvm/'clang.exe'),'--target=aarch64-linux-android32','-O2',str(out/'src/byte_patch.c'),'-o',str(out/'bin/byte_patch64')],check=True)
 for x in [out/'system/lib64/libcp10.so',*list((out/'bin').iterdir())]:subprocess.run([str(llvm/'llvm-strip.exe'),'--strip-unneeded',str(x)],check=True)
 (out/'contracts/platform.sha256').write_text(''.join(sha(v/n)+'  /system/lib64/'+n+'\n' for n in libs),encoding='utf-8',newline='\n')
 files=sorted(x for x in out.rglob('*') if x.is_file())
 for x in files:
  if x.suffix in {'.sh','.md','.txt','.json','.tsv','.cpp','.h','.c','.sha256'}:x.write_bytes(x.read_bytes().replace(b'\r\n',b'\n'))
 (out/'checksums.sha256').write_text(''.join(sha(x)+'  '+x.relative_to(out).as_posix()+'\n' for x in files),encoding='utf-8',newline='\n')
 print('BUILT_INTERNAL_P010_POOL',sha(out/'system/lib64/libcp10.so'))
if __name__=='__main__':main()
