"""Build the hash-pinned API32 Codec2 Magisk module from a prepared SDK.
Firmware libraries are private link inputs; no firmware binary enters the ZIP.
"""
import argparse,hashlib,json,os,re,shutil,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];MODULE=ROOT/'magisk-codec2'
REV='1e9d80a10b09c9a6e0bf5cc1dbe4b6d3e66dbc2b'
def run(*args):subprocess.run([str(x) for x in args],check=True)
def q(p):return '"'+Path(p).as_posix()+'"'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ndk',type=Path,required=True);parser.add_argument('--prepared-root',type=Path,required=True);parser.add_argument('--vndk-headers',type=Path,required=True);parser.add_argument('--rga-source',type=Path,required=True);parser.add_argument('--work',type=Path,default=ROOT/'build/codec2-module');a=parser.parse_args()
 sdk=a.prepared_root.resolve();headers=a.vndk_headers.resolve();work=a.work.resolve();work.mkdir(parents=True,exist_ok=True)
 stock=sdk/'vendor32/libcodec2_rk_component.so'
 if not stock.is_file() or sha(stock)!='22ce9ddbfc80d72b06ac1d330eed7a11f90575d8b56dc2e0358ed7b75c62a4bb':raise SystemExit('Missing or unknown stock Codec2 registry input')
 names={s.decode() for s in stock.read_bytes().split(b'\0') if re.fullmatch(rb'(?:OMX\.)?c2\.rk\.[a-z0-9]+\.(?:decoder|encoder)(?:\.secure)?',s)}
 declared=set((MODULE/'contracts/stock-components.txt').read_text().splitlines())
 published=set(re.findall(r'"((?:OMX\.)?c2\.rk\.[^"]+)"',(MODULE/'service-src/PublishedComponents.h').read_text()))
 if names!=declared or names!=published:raise SystemExit('The complete original component registry must be retained')
 upstream=sdk/'codec2-a12';actual=subprocess.check_output(['git','-C',str(upstream),'rev-parse','HEAD'],text=True).strip()
 if actual!=REV or subprocess.check_output(['git','-C',str(upstream),'diff','--name-only'],text=True).strip():raise SystemExit('Wrong or modified public Codec2 baseline')
 source=work/'source'
 if source.exists():raise SystemExit('Use a fresh build directory; existing source is never overwritten')
 shutil.copytree(upstream,source,ignore=shutil.ignore_patterns('.git'))
 run('git','-C',source,'--git-dir='+str(upstream/'.git'),'--work-tree='+str(source),'apply',MODULE/'patches/codec2-p010.patch')
 run('git','-C',source,'--git-dir='+str(upstream/'.git'),'--work-tree='+str(source),'apply',MODULE/'patches/codec2-hdr-bridge.patch')
 for header in (MODULE/'bridge-src').glob('*.h'):shutil.copyfile(header,source/'component/include'/header.name)
 for name in ['aosp-codec2','aosp-hardware','platform-private','vendor32']:
  if not (sdk/name).is_dir():raise SystemExit('Missing prepared SDK directory: '+name)
 host='windows-x86_64' if os.name=='nt' else 'darwin-x86_64' if os.uname().sysname=='Darwin' else 'linux-x86_64';ext='.exe' if os.name=='nt' else '';llvm=a.ndk.resolve()/'toolchains/llvm/prebuilt'/host/'bin'
 includes=[source/'component/include',source/'osal/include',sdk/'aosp-codec2/core/include',sdk/'aosp-codec2/vndk/include',sdk/'aosp-codec2/vndk/internal',sdk/'aosp-codec2/components/base/include',sdk/'aosp-codec2/sfplugin',sdk/'aosp-codec2/sfplugin/utils',sdk/'aosp-hardware',sdk/'platform-private',MODULE/'service-src',work/'generated',a.rga_source.resolve()/'include',a.rga_source.resolve()/'im2d_api',a.rga_source.resolve()/'core/3rdparty/android_hal',headers/'system/memory/libion/kernel-headers']
 names={'include','include_vndk','include_types','include_private','include_outside_system'}
 includes+=sorted(p for p in headers.rglob('*') if p.is_dir() and p.name in names and 'external/libcxx' not in p.as_posix() and 'bionic' not in p.as_posix())
 includes+=sorted(p for p in (headers/'generated-headers').rglob('gen') if (p/'android').exists())
 includes+=[headers/'system/libfmq/base',headers/'frameworks/native/libs/binder/ndk/include_cpp',headers/'frameworks/native/libs/binder/ndk/include_ndk']
 generated=work/'generated';generated.mkdir();(generated/'C2RKVersion.h').write_text('#define C2_GIT_BUILD_VERSION "RK3588 Codec2 compatibility 0.1.2 API32 HDR"\n')
 vendor=sdk/'vendor32';links=['libmpp.so','librga.so','libbase.so','libcodec2.so','libcodec2_vndk.so','libsfplugin_ccodec_utils.so','libstagefright_foundation.so','libgralloc_priv.so','libutils.so','libcutils.so','libui.so','libc++.so','liblog.so','libhidlbase.so','libgralloctypes.so','android.hardware.graphics.mapper@4.0.so']
 svc_links=links+['libhidlbase.so','libbinder.so','libavservices_minijail.so','libdmabufheap.so']
 for name in svc_links:
  if not (vendor/name).is_file():raise SystemExit('Missing private link input '+name)
 osal=sorted((source/'osal').glob('*.cpp'));component=[source/'component/C2RKComponentFactory.cpp',*sorted((source/'component/base').glob('*.cpp')),*sorted((source/'component/mpi').glob('*.cpp')),*osal]
 # Exclude optional source files not used by the pinned Android.bp decoder build.
 approved={'C2RKComponentFactory.cpp','C2RKComponent.cpp','C2RKInterface.cpp','C2RKCodecMapper.cpp','C2RKExtendParam.cpp','C2RKMlvecLegacy.cpp','C2RKMpiDec.cpp','C2RKMpiEnc.cpp'}
 component=[p for p in component if p.parent.name=='osal' or p.name in approved]
 cmake='cmake_minimum_required(VERSION 3.12)\nproject(codec2_compat LANGUAGES C CXX)\n'
 cmake+='add_library(codec2_rx_component SHARED '+ ' '.join(q(p) for p in component)+')\n'
 cmake+='add_executable(codec2_compat_service '+' '.join(q(p) for p in [MODULE/'service-src/main.cpp',MODULE/'service-src/CompatRouter.cpp',MODULE/'service-src/C2RKComponentStore.cpp',*osal])+')\n'
 options=['-O2','-std=c++17','-fno-exceptions','-fno-rtti','-nostdinc++','-Wno-c++11-narrowing','-Wno-deprecated-declarations','-isystem'+(headers/'external/libcxx/include').as_posix(),'-isystem'+(headers/'generated-headers/bionic/libc/libc/android_vendor.32_arm_armv8-a_shared/gen/include').as_posix()]
 options+=['-ffile-prefix-map='+p.as_posix()+'='+name for p,name in [(sdk,'sdk'),(work,'build'),(headers,'vndk32'),(ROOT,'module'),(a.rga_source.resolve(),'rga')]]
 for target,libs in [('codec2_rx_component',links),('codec2_compat_service',svc_links)]:
  cmake+='target_include_directories('+target+' PRIVATE '+' '.join(q(p) for p in includes)+')\n'
  cmake+='target_compile_options('+target+' PRIVATE '+' '.join('"'+x+'"' for x in options)+')\n'
  cmake+='target_compile_definitions('+target+' PRIVATE ANDROID ANDROID_8 ANDROID_7_DRM ANDROID_12 USE_HARDWARE_ROCKCHIP RK_DRM_GRALLOC=1)\n'
  cmake+='target_link_options('+target+' PRIVATE -nostdlib++ -Wl,--no-undefined '+('-Wl,--export-dynamic' if target.endswith('service') else '-Wl,-Bsymbolic')+')\n'
  cmake+='target_link_libraries('+target+' PRIVATE dl m '+' '.join(q(vendor/x) for x in libs)+')\n'
 project=work/'project';project.mkdir();(project/'CMakeLists.txt').write_text(cmake)
 build=work/'arm';run('cmake','-S',project,'-B',build,'-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(a.ndk.resolve()/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=armeabi-v7a','-DANDROID_PLATFORM=android-32','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_SKIP_RPATH=ON');run('cmake','--build',build,'--parallel','8')
 payload=work/'package';shutil.copytree(MODULE,payload,ignore=shutil.ignore_patterns('system','bin'))
 binary=payload/'system/vendor/bin/hw/android.hardware.media.c2@1.1-service';binary.parent.mkdir(parents=True);shutil.copyfile(build/'codec2_compat_service',binary)
 library=payload/'system/vendor/lib/hw/libcodec2_rx_component.so';library.parent.mkdir(parents=True);shutil.copyfile(build/'libcodec2_rx_component.so',library)
 for file in [binary,library]:run(llvm/('llvm-strip'+ext),'--strip-unneeded',file)
 (payload/'bin').mkdir();run(llvm/('clang'+ext),'--target=aarch64-linux-android32','-O2','-s','-fPIE','-pie',MODULE/'src/patcher.c','-o',payload/'bin/patcher')
 for script in [*payload.glob('*.sh'),payload/'contracts/stock-components.txt']:script.write_bytes(script.read_bytes().replace(b'\r\n',b'\n'))
 files=sorted(p for p in payload.rglob('*') if p.is_file());(payload/'checksums.sha256').write_text(''.join(sha(p)+'  '+p.relative_to(payload).as_posix()+'\n' for p in files))
 version=dict(line.split('=',1) for line in (MODULE/'module.prop').read_text(encoding='utf-8').splitlines() if '=' in line)['version']
 archive=work/('rk3588-codec2-compat-v'+version+'.zip')
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
  for f in sorted(payload.rglob('*')):
   if f.is_file():z.write(f,f.relative_to(payload).as_posix())
 print('ZIP',archive,'SHA256',sha(archive))
if __name__=='__main__':main()
