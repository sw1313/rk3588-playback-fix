"""Build the pinned SDK14 Codec2 core for the verified API32 stack.
Private firmware files are link inputs only; they are never packaged.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
R=Path(__file__).resolve().parents[1];M=R/'magisk-codec2-sdk'
p=argparse.ArgumentParser(description=__doc__)
for name in ['upstream','prepared-root','vndk-headers','ndk','rga-source','libhardware','work']:p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();W=a.work.resolve();SDK=a.prepared_root.resolve();H=a.vndk_headers.resolve();N=a.ndk.resolve();G=a.rga_source.resolve();S=W/'source'
def run(*args):subprocess.run([str(x) for x in args],check=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
contracts={line.split()[1]:line.split()[0] for line in (M/'contracts/platform.sha256').read_text().splitlines()}
for path,source in [('/system/lib/libhardware.so',a.libhardware)]:
 if not source.is_file() or sha(source)!=contracts[path]:raise SystemExit('Unknown additional API32 link input: '+path)
rev=json.loads((M/'UPSTREAM.json').read_text())['commit']
if subprocess.check_output(['git','-C',str(a.upstream),'rev-parse','HEAD'],text=True).strip()!=rev:raise SystemExit('Unknown SDK14 source revision')
if subprocess.check_output(['git','-C',str(a.upstream),'status','--porcelain'],text=True).strip():raise SystemExit('Modified upstream source')
if W.exists():raise SystemExit('Use a fresh build directory')
W.mkdir(parents=True);shutil.copytree(a.upstream,S,ignore=shutil.ignore_patterns('.git'))
run('git','-C',S,'--git-dir='+str(a.upstream.resolve()/'.git'),'--work-tree='+str(S),'apply',M/'patches/api32-full-precision.patch')
def q(p):return '"'+Path(p).as_posix()+'"'
incs=[S/'store',S/'component/base',S/'component/base/include',S/'component/video',S/'component/osal/include',S/'component/osal/include/mpp',SDK/'aosp-codec2/core/include',SDK/'aosp-codec2/vndk/include',SDK/'aosp-codec2/vndk/internal',SDK/'aosp-codec2/components/base/include',SDK/'aosp-codec2/sfplugin',SDK/'aosp-codec2/sfplugin/utils',SDK/'aosp-hardware',SDK/'platform-private',G/'include',G/'im2d_api',G/'core/3rdparty/android_hal',H/'system/memory/libion/kernel-headers']
names={'include','include_vndk','include_types','include_private','include_outside_system'}
incs+=sorted(p for p in H.rglob('*') if p.is_dir() and p.name in names and 'external/libcxx' not in p.as_posix() and 'bionic' not in p.as_posix())
incs+=sorted(p for p in (H/'generated-headers').rglob('gen') if (p/'android').exists())
incs+=[H/'system/libfmq/base',H/'frameworks/native/libs/binder/ndk/include_cpp',H/'frameworks/native/libs/binder/ndk/include_ndk']
srcs=[S/'component/C2RKComponentFactory.cpp',*sorted((S/'component/base').glob('*.cpp')),*sorted((S/'component/video').glob('*.cpp')),*sorted((S/'component/osal').glob('*.cpp')),*sorted((S/'store').glob('*.cpp'))]
V=SDK/'vendor32';libs=['libmpp.so','librga.so','libbase.so','libcodec2.so','libcodec2_vndk.so','libsfplugin_ccodec_utils.so','libstagefright_foundation.so','libgralloc_priv.so','libutils.so','libcutils.so','libui.so','libc++.so','liblog.so','libhidlbase.so','libgralloctypes.so','android.hardware.graphics.mapper@4.0.so','libdmabufheap.so']
paths=[V/lib for lib in libs]
paths.append(a.libhardware.resolve())
options=['-O2','-std=c++17','-fno-exceptions','-fno-rtti','-nostdinc++','-Wno-c++11-narrowing','-Wno-deprecated-declarations','-isystem'+(H/'external/libcxx/include').as_posix(),'-isystem'+(H/'generated-headers/bionic/libc/libc/android_vendor.32_arm_armv8-a_shared/gen/include').as_posix()]
options+=['-ffile-prefix-map='+p.as_posix()+'='+label for p,label in [(W,'build'),(SDK,'sdk32'),(H,'vndk32'),(G,'rga'),(R,'module'),(N,'ndk')]]
text='cmake_minimum_required(VERSION 3.12)\nproject(codec2_sdk32 LANGUAGES C CXX)\nadd_library(codec2_rx_component SHARED '+' '.join(q(p) for p in srcs)+')\n'
text+='target_include_directories(codec2_rx_component PRIVATE '+' '.join(q(p) for p in incs)+')\n'
text+='target_compile_options(codec2_rx_component PRIVATE '+' '.join('"'+x+'"' for x in options)+')\n'
text+='target_compile_definitions(codec2_rx_component PRIVATE ANDROID ANDROID_8 ANDROID_7_DRM ANDROID_12 USE_HARDWARE_ROCKCHIP RK_DRM_GRALLOC=1)\n'
text+='target_link_options(codec2_rx_component PRIVATE -nostdlib++ -Wl,--no-undefined -Wl,-Bsymbolic)\n'
text+='target_link_libraries(codec2_rx_component PRIVATE dl m '+' '.join(q(p) for p in paths)+')\n'
P=W/'project';P.mkdir();(P/'CMakeLists.txt').write_text(text,encoding='utf-8')
run('cmake','-S',P,'-B',W/'arm','-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(N/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=armeabi-v7a','-DANDROID_PLATFORM=android-32','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_SKIP_RPATH=ON')
run('cmake','--build',W/'arm','--parallel','6')
out=W/'package';shutil.copytree(M,out)
dest=out/'system/vendor/lib/hw/libcodec2_rx_component.so';dest.parent.mkdir(parents=True);shutil.copyfile(W/'arm/libcodec2_rx_component.so',dest)
host='windows-x86_64' if os.name=='nt' else 'darwin-x86_64' if os.uname().sysname=='Darwin' else 'linux-x86_64'
run(N/'toolchains/llvm/prebuilt'/host/'bin'/('llvm-strip.exe' if os.name=='nt' else 'llvm-strip'),'--strip-unneeded',dest)
files=sorted(p for p in out.rglob('*') if p.is_file())
for f in files:
 if f.suffix in {'.md','.json','.patch','.h','.cpp'}:f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
(out/'checksums.sha256').write_text(''.join(sha(f)+'  '+f.relative_to(out).as_posix()+'\n' for f in files),encoding='utf-8',newline='\n')
print('PASS source-built API32 SDK core; package',out,'SHA256',sha(dest))
