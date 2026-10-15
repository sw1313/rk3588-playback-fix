"""Build API32 software decoders from pinned public backends and private APEX link inputs.

Requires CMake/Ninja, NDK, the prepared Codec2 SDK/VNDK headers, and a
decoder-only PIC/high-bit-depth libvpx static build from v1.17.0. Firmware
libraries are link inputs only and are never copied into the package.
"""
import argparse,base64,hashlib,json,os,shutil,subprocess,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];MODULE=ROOT/'magisk-software'
VPX='6df3ec34557879fff673706f4a1d9fbd0f3a6f0e'
GAV1='085032331f60c10055d32ab52932bd7bd77e6f53'
BASELINES={
 'gav1/C2SoftGav1Dec.cpp':'79dc7299df982b19d558d95eab8c611fb80a1a21824c3ea240a2a75fd53f3c09',
 'gav1/C2SoftGav1Dec.h':'724cb1a3f368f7adc02ab651d1cd309407ad9cd7c829ce790c8e0604c4b683cb',
 'vpx/C2SoftVpxDec.cpp':'493496c938837a66900c622e4e0719498c51eb206bcd46e6c4cb8c34e29254d3',
 'vpx/C2SoftVpxDec.h':'1d38f6c914fbd591fd2cbc1768a386d945d5e57654a79420b987845ec27d8fb4'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(*args):subprocess.run([str(x) for x in args],check=True)
def q(p):return '"'+Path(p).as_posix()+'"'
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['ndk','prepared-root','vndk-headers','link-libs','vpx-source','vpx-build','gav1-source','work']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--resume',action='store_true',help='Regenerate only this builder\'s wrapper/project/package files and reuse its compilation cache')
 a=p.parse_args();w=a.work.resolve();w.mkdir(parents=True,exist_ok=True)
 sdk=a.prepared_root.resolve();h=a.vndk_headers.resolve();links=a.link_libs.resolve()
 for s,revision in [(a.vpx_source,VPX),(a.gav1_source,GAV1)]:
  if subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip()!=revision:raise SystemExit('Backend revision mismatch')
  if subprocess.check_output(['git','-C',str(s),'diff','--name-only'],text=True).strip():raise SystemExit('Modified backend source')
 config=(a.vpx_build/'vpx_config.h').read_text(encoding='utf-8')
 for flag in ['CONFIG_VP9_HIGHBITDEPTH','CONFIG_VP8_DECODER','CONFIG_VP9_DECODER','CONFIG_PIC']:
  if '#define '+flag+' 1' not in config:raise SystemExit('Missing required VPX configuration: '+flag)
 for flag in ['CONFIG_VP8_ENCODER','CONFIG_VP9_ENCODER']:
  if '#define '+flag+' 0' not in config:raise SystemExit('VPX must be decoder-only')
 src=w/'wrappers'
 if src.exists() and not a.resume:raise SystemExit('Use a fresh work directory or --resume for this generated build')
 src.mkdir(exist_ok=True)
 for name,digest in BASELINES.items():
  target=src/Path(name).name
  local=sdk/'aosp-codec2/components'/name
  if local.exists():data=local.read_bytes()
  else:
   url='https://android.googlesource.com/platform/frameworks/av/+/refs/tags/android-12.1.0_r27/media/codec2/components/'+name+'?format=TEXT'
   data=base64.b64decode(urllib.request.urlopen(url,timeout=30).read())
  if hashlib.sha256(data).hexdigest()!=digest:raise SystemExit('Unknown API32 wrapper baseline: '+name)
  target.write_bytes(data)
 # An explicit work tree prevents a surrounding repository's ignored build
 # directory from making git apply silently skip every patch.
 run('git','-C',src,'--git-dir='+str(ROOT/'.git'),'--work-tree='+str(src),'apply',MODULE/'patches/full-precision-p010.patch')
 shutil.copyfile(MODULE/'src/full_precision.h',src/'full_precision.h')
 # The wrapper's libgav1 include uses libgav1/src/... under its parent.
 gavparent=a.gav1_source.resolve().parent
 if a.gav1_source.name!='libgav1':raise SystemExit('libgav1 checkout must use the directory name libgav1')
 includes=[sdk/'aosp-codec2/core/include',sdk/'aosp-codec2/vndk/include',sdk/'aosp-codec2/vndk/internal',sdk/'aosp-codec2/components/base/include',sdk/'aosp-codec2/sfplugin',sdk/'aosp-codec2/sfplugin/utils',sdk/'aosp-hardware',sdk/'platform-private',gavparent,a.vpx_source.resolve()]
 names={'include','include_vndk','include_types','include_private','include_outside_system','include_outside_system_only'}
 includes+=sorted(x for x in h.rglob('*') if x.is_dir() and x.name in names and 'external/libcxx' not in x.as_posix() and 'bionic' not in x.as_posix())
 includes+=sorted(x for x in (h/'generated-headers').rglob('gen') if (x/'android').exists())
 cxx='-nostdinc++ -isystem'+(h/'external/libcxx/include').as_posix()+' -D_LIBCPP_SUPPORT_XLOCALE_POSIX_L_FALLBACK_H -fvisibility=hidden -fno-rtti -fno-exceptions -flto -fsanitize=cfi -fsanitize-cfi-cross-dso'
 for path,label in [(w,'build'),(sdk,'sdk'),(h,'vndk32'),(ROOT,'module'),(gavparent,'backends')]:cxx+=' -ffile-prefix-map='+path.as_posix()+'='+label
 needed=['libbase','libcodec2','libcodec2_vndk','libutils','libcodec2_soft_common','libcutils','liblog','libsfplugin_ccodec_utils','libstagefright_foundation','libc++']
 for name in needed:
  if not (links/(name+'.so')).is_file():raise SystemExit('Missing private APEX link library: '+name)
 cm=['cmake_minimum_required(VERSION 3.16)','project(software_codecs C CXX)','set(CMAKE_POSITION_INDEPENDENT_CODE ON)',f'set(CMAKE_CXX_FLAGS "{cxx}")','set(LIBGAV1_ENABLE_TESTS OFF CACHE BOOL "" FORCE)','set(LIBGAV1_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)','set(LIBGAV1_THREADPOOL_USE_STD_MUTEX 1 CACHE BOOL "" FORCE)',f'add_subdirectory({q(a.gav1_source.resolve())} gav1)','add_library(vpxnew STATIC IMPORTED)',f'set_target_properties(vpxnew PROPERTIES IMPORTED_LOCATION {q(a.vpx_build.resolve()/"libvpx.a")})']
 for target,file,defs,backend in [('codec2_soft_av1dec_gav1',src/'C2SoftGav1Dec.cpp','CODECNAME="c2.android.av1.decoder"','libgav1_static'),('codec2_soft_vp8dec',src/'C2SoftVpxDec.cpp','','vpxnew'),('codec2_soft_vp9dec',src/'C2SoftVpxDec.cpp','VP9','vpxnew'),('factory_probe64',MODULE/'src/factory_probe.cpp','','')]:
  cm += [f'add_{"executable" if target=="factory_probe64" else "library"}({target} '+('' if target=='factory_probe64' else 'SHARED ')+q(file)+')',f'target_include_directories({target} PRIVATE '+' '.join(q(x) for x in includes)+')',f'target_compile_options({target} PRIVATE -O2 -std=c++17 -Wno-c++11-narrowing -Wno-deprecated-declarations -fvisibility=default)',f'target_compile_definitions({target} PRIVATE ANDROID {defs})',f'target_link_options({target} PRIVATE -nostdlib++ -flto -fsanitize=cfi -fsanitize-cfi-cross-dso -Wl,--no-undefined -Wl,-Bsymbolic -Wl,--exclude-libs,ALL)',f'target_link_libraries({target} PRIVATE {backend} '+' '.join(q(links/(x+'.so')) for x in needed)+' log dl m)']
 project=w/'project';project.mkdir(exist_ok=True);(project/'CMakeLists.txt').write_text('\n'.join(cm)+'\n',encoding='utf-8')
 build=w/'arm64';run('cmake','-S',project,'-B',build,'-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(a.ndk.resolve()/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-32','-DANDROID_STL=none','-DCMAKE_BUILD_TYPE=Release');run('cmake','--build',build,'--parallel','6')
 out=w/'package';shutil.copytree(MODULE,out,dirs_exist_ok=a.resume,ignore=shutil.ignore_patterns('system','apex','bin','checksums.sha256'));(out/'apex/lib64').mkdir(parents=True,exist_ok=True);(out/'bin').mkdir(exist_ok=True);(out/'licenses').mkdir(exist_ok=True)
 llvm=a.ndk.resolve()/'toolchains/llvm/prebuilt'/('windows-x86_64' if os.name=='nt' else 'linux-x86_64')/'bin';ext='.exe' if os.name=='nt' else ''
 for f in build.glob('*.so'):
  dest=out/'apex/lib64'/f.name;shutil.copyfile(f,dest);run(llvm/('llvm-strip'+ext),'--strip-unneeded',dest)
 shutil.copyfile(build/'factory_probe64',out/'bin/factory_probe64');run(llvm/('llvm-strip'+ext),'--strip-unneeded',out/'bin/factory_probe64')
 for name,path in [('libvpx-LICENSE',a.vpx_source/'LICENSE'),('libvpx-PATENTS',a.vpx_source/'PATENTS'),('libgav1-LICENSE',a.gav1_source/'LICENSE'),('libgav1-AUTHORS',a.gav1_source/'AUTHORS')]:shutil.copyfile(path,out/'licenses'/name)
 (out/'SOURCE.json').write_text(json.dumps({'aosp':'android-12.1.0_r27','libvpx':VPX,'libgav1':GAV1,'vpx_config_sha256':sha(a.vpx_build/'vpx_config.h'),'vpx_archive_sha256':sha(a.vpx_build/'libvpx.a'),'cfi':True,'ten_bit_output':'P010 for Surface and explicit P010 clients; original conversion for explicit legacy eight-bit copy requests'},indent=2),encoding='utf-8')
 for f in out.rglob('*'):
  if f.is_file() and f.suffix in {'.sh','.txt','.prop','.sha256','.md','.json','.patch','.cpp','.h'}:f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
 files=sorted(f for f in out.rglob('*') if f.is_file() and f.name!='checksums.sha256');(out/'checksums.sha256').write_bytes(''.join(sha(f)+'  '+f.relative_to(out).as_posix()+'\n' for f in files).encode())
 print('PACKAGE',out)
if __name__=='__main__':main()
