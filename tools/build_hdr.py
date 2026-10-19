"""Build the original-HWC static-metadata adapter. Private libraries are link inputs only."""
import argparse,os,subprocess,hashlib,shutil,json
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--atomic-test',action='store_true',help='Build the isolated actual-hook regression executable outside the module package')
 p.add_argument('--client-hdr-test',action='store_true',help='Build isolated actual client HDR callback tests and scratch RGB10 allocation probe')
 p.add_argument('--qms-candidate',action='store_true',help='Compile nonrelease QMS frontend; requires a validated current-boot kernel endpoint to activate')
 p.add_argument('--qms-frontend-test',action='store_true',help='Build isolated actual QMS callback regression executable, never include it in the module')
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
 args+=['-ffile-prefix-map='+str(root)+'=/src/rk3588-playback-fix','-fmacro-prefix-map='+str(root)+'=/src/rk3588-playback-fix']
 args+=['-I'+str(p) for p in includes]
 if a.qms_candidate:args+=['-DRKQMS_HWC_CANDIDATE=1']
 args+=[module/'src/hwc_bridge.cpp','-o',w/'hwcomposer.rk30board.so','-Wl,--no-undefined','-Wl,-z,relro,-z,now','-ldl','-lm','-llog']
 args+=[v/name for name in ['libui.so','libutils.so','libcutils.so','libc++.so']]
 result=subprocess.run([str(x) for x in args],capture_output=True,text=True);(w/'compile.log').write_text(result.stdout+result.stderr,encoding='utf-8')
 if result.returncode:raise SystemExit('HWC adapter build failed: '+str(w/'compile.log'))
 # SF's original dependency remains loaded through this source-built shim.
 # The private DSO is a link input only and is never copied into the package.
 sfargs=args[:args.index(module/'src/hwc_bridge.cpp')]
 for source,name in [(module/'src/sf_target10.cpp','libsfhdr.so'),(root/'tools/hdr-probes/sf_target10_test.cpp','sf-target-test64')]:
  cmd=[x for x in sfargs if name=='libsfhdr.so' or x not in ('-shared','-fPIC')]
  cmd += [source,'-o',w/name,'-Wl,--no-undefined','-Wl,-z,relro,-z,now','-ldl','-llog','-lEGL','-lGLESv2']
  if name=='libsfhdr.so':cmd+=['-Wl,-soname,libsfhdr.so','-Wl,--no-as-needed',v/'librkgfx_dc.so','-Wl,--as-needed',v/'libui.so',v/'libutils.so',v/'libc++.so']
  result=subprocess.run([str(x) for x in cmd],capture_output=True,text=True)
  (w/(name+'-build.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
  if result.returncode:raise SystemExit('SF target build failed: '+str(w/(name+'-build.log')))
 for source,name in [(module/'src/alloc_target10.cpp','libga10.so'),(root/'tools/hdr-probes/alloc_target10_test.cpp','alloc-target-test64')]:
  cmd=[x for x in sfargs if name=='libga10.so' or x not in ('-shared','-fPIC')]
  cmd += [source,'-o',w/name,'-Wl,--no-undefined','-Wl,-z,relro,-z,now','-ldl','-llog']
  if name=='libga10.so':cmd+=['-Wl,-soname,libga10.so','-Wl,--no-as-needed',v/'libcutils.so','-Wl,--as-needed']
  else:cmd+=[v/'libc++.so']
  result=subprocess.run([str(x) for x in cmd],capture_output=True,text=True)
  (w/(name+'-build.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
  if result.returncode:raise SystemExit('Allocator target build failed: '+str(w/(name+'-build.log')))
 if a.atomic_test:
  testargs=[x for x in args if x not in ('-shared','-fPIC')]
  testargs=[root/'tools/hdr-probes/atomic_reuse_test.cpp' if x==module/'src/hwc_bridge.cpp' else w/'atomic_reuse_test64' if x==w/'hwcomposer.rk30board.so' else x for x in testargs]
  result=subprocess.run([str(x) for x in testargs],capture_output=True,text=True)
  (w/'atomic-test-build.log').write_text(result.stdout+result.stderr,encoding='utf-8')
  if result.returncode:raise SystemExit('Atomic test build failed: '+str(w/'atomic-test-build.log'))
 if a.qms_frontend_test:
  testargs=[x for x in args if x not in ('-shared','-fPIC')]
  testargs=[root/'tools/hdr-probes/qms_frontend_test.cpp' if x==module/'src/hwc_bridge.cpp' else w/'qms_frontend_test64' if x==w/'hwcomposer.rk30board.so' else x for x in testargs]
  result=subprocess.run([str(x) for x in testargs],capture_output=True,text=True)
  (w/'qms-frontend-test-build.log').write_text(result.stdout+result.stderr,encoding='utf-8')
  if result.returncode:raise SystemExit('QMS frontend test build failed: '+str(w/'qms-frontend-test-build.log'))
 if a.client_hdr_test:
  for source,name in [('tools/hdr-probes/client_hdr_test.cpp','client_hdr_test64'),('tools/hdr-probes/client_target_plan_test.cpp','client_target_plan_test64'),('tools/hdr-probes/target_readback_test.cpp','target_readback_test64'),('tools/hdr-probes/rgb10_alloc_probe.cpp','rgb10_alloc_probe64'),('tools/hdr-probes/rgb10_queue_probe.cpp','rgb10_queue_probe64')]:
   testargs=[x for x in args if x not in ('-shared','-fPIC')]
   testargs=[root/source if x==module/'src/hwc_bridge.cpp' else w/name if x==w/'hwcomposer.rk30board.so' else x for x in testargs]
   if name=='rgb10_queue_probe64':testargs+=[v/'libgui.so','-lEGL','-lGLESv2']
   if name=='target_readback_test64':testargs+=['-lEGL','-lGLESv2']
   result=subprocess.run([str(x) for x in testargs],capture_output=True,text=True)
   (w/(name+'-build.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
   if result.returncode:raise SystemExit('Client HDR test build failed: '+str(w/(name+'-build.log')))
 subprocess.run([str(llvm/('clang'+ext)),'--target=aarch64-linux-android32','-O2',str(module/'src/check.c'),'-ldl','-o',str(w/'check64')],check=True)
 subprocess.run([str(llvm/('clang'+ext)),'--target=aarch64-linux-android32','-O2','-Wall','-Wextra','-Werror',str(module/'src/packet_probe.c'),'-o',str(w/'packet_probe64')],check=True)
 subprocess.run([str(llvm/('clang'+ext)),'--target=aarch64-linux-android32','-std=c11','-O2','-Wall','-Wextra','-Werror',str(module/'src/plane_probe.c'),'-ldl','-o',str(w/'plane_probe64')],check=True)
 subprocess.run([str(llvm/('clang'+ext)),'--target=aarch64-linux-android32','-O2','-Wall','-Wextra','-Werror',str(module/'src/byte_patch.c'),'-o',str(w/'byte_patch64')],check=True)
 for target,name in [('aarch64-linux-android32','hwui_probe64'),('armv7a-linux-androideabi32','hwui_probe32')]:
  subprocess.run([str(llvm/('clang'+ext)),'--target='+target,'-O2','-Wall','-Wextra','-Werror',str(root/'tools/hdr-probes/hwui_format_probe.c'),'-ldl','-o',str(w/name)],check=True)
 payload=w/'package'
 if payload.exists():raise SystemExit('Use a fresh package directory; existing payload is never overwritten')
 shutil.copytree(module,payload,ignore=shutil.ignore_patterns('system','bin','checksums.sha256'))
 if a.qms_candidate:
  (payload/'QMS_CANDIDATE.json').write_text(json.dumps({
   'status':'FRONTEND_ONLY_NATIVE_BINDING_AND_BOOT_PAYLOAD_MISSING',
   'compiled_feature':'RKQMS_HWC_CANDIDATE',
   'kernel_endpoint_registered':False,'validated_native_binding':False,
   'boot_loader_supplied':False,'installable_Decode_upgrade':False,
   'hardware_or_physical_HDMI_acceptance':False,'release_accepted':False,
  },indent=2)+'\n',encoding='utf8')
 lib=payload/'system/vendor/lib64/hw/hwcomposer.rk30board.so';lib.parent.mkdir(parents=True);shutil.copyfile(w/'hwcomposer.rk30board.so',lib)
 sf=payload/'target/libsfhdr.so';sf.parent.mkdir(parents=True);shutil.copyfile(w/'libsfhdr.so',sf)
 ga=payload/'target/libga10.so';shutil.copyfile(w/'libga10.so',ga)
 (payload/'bin').mkdir()
 for name in ['check64','packet_probe64','plane_probe64','byte_patch64','hwui_probe64','hwui_probe32','sf-target-test64','alloc-target-test64']:shutil.copyfile(w/name,payload/'bin'/name)
 for binary in [lib,sf,ga,*list((payload/'bin').iterdir())]:subprocess.run([str(llvm/('llvm-strip'+ext)),'--strip-unneeded',str(binary)],check=True)
 files=sorted(p for p in payload.rglob('*') if p.is_file())
 for f in files:
  if f.suffix in {'.sh','.md','.txt','.prop','.sha256','.json','.py','.c','.cpp','.h'}:f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
 (payload/'checksums.sha256').write_bytes(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.relative_to(payload).as_posix()+'\n' for f in files).encode())
 print('Built HWC metadata adapter:',w)
if __name__=='__main__':main()
