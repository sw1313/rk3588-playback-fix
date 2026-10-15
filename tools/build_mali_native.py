"""Build private API32 app-driver APKs from user-supplied, hash-pinned Mali inputs.

Firmware libraries and signing keys are never fetched or added to the source tree.
The result is a local compatibility-test module, not a generic Mali update.
"""
import argparse,hashlib,json,shutil,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];MODULE=ROOT/'magisk-mali-native'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args):
 r=subprocess.run([str(a) for a in args],capture_output=True,text=True,encoding='utf-8',errors='replace')
 if r.returncode:raise SystemExit((r.stdout+r.stderr)[-5000:])
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ['ndk','sdk','headers','binder','libcpp','g15','g18','allocator','graphics-common','keystore','work']:p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--build-tools',default='35.0.0');p.add_argument('--key-alias',default='diagnostic');p.add_argument('--storepass-env',default='RK_MALI_KEYSTORE_PASSWORD')
 a=p.parse_args();w=a.work.resolve();out=w/'package'
 if out.exists():raise SystemExit('Use a fresh output directory')
 w.mkdir(parents=True,exist_ok=True);llvm=a.ndk.resolve()/'toolchains/llvm/prebuilt/windows-x86_64/bin'
 h=a.headers.resolve();bt=a.sdk.resolve()/'build-tools'/a.build_tools;jar=a.sdk.resolve()/'platforms/android-35/android.jar'
 supplied={name:getattr(a,name).resolve() for name in ['g15','g18','allocator','graphics_common']}
 pins=json.loads((MODULE/'contracts/candidates.json').read_text(encoding='utf-8'))
 for name,path in supplied.items():
  if digest(path)!=pins[name]:raise SystemExit('Candidate input differs: '+name)
 # Platform C++ headers and its original runtime preserve std::__1 ABI.
 args=[llvm/'clang++.exe','--target=aarch64-linux-android32','-std=c++17','-O2','-fPIC','-shared','-fno-exceptions','-fno-rtti','-nostdinc++','-nostdlib++','-DANDROID','-DBINDER_STABILITY_SUPPORT','-isystem'+str(h/'external/libcxx/include'),'-isystem'+str(h/'generated-headers/bionic/libc/libc/android_vendor.32_arm_armv8-a_shared/gen/include'),'-I'+str(MODULE/'src/generated')]
 args+=['-I'+str(h/'frameworks/native/libs/binder/ndk'/n) for n in ['include_cpp','include_ndk','include_platform']]
 common=w/'android.hardware.common-V2-ndk.so'
 args+=[MODULE/'src/NativeHandle.cpp','-o',common,'-Wl,-soname,android.hardware.common-V2-ndk.so','-Wl,--no-undefined','-Wl,-z,relro,-z,now',a.binder.resolve(),a.libcpp.resolve(),'-llog'];run(args)
 alias=w/'vulkan.rk3588.so';run([llvm/'clang.exe','--target=aarch64-linux-android32','-O2','-fPIC','-shared',MODULE/'src/vulkan_alias.c','-Wl,--no-as-needed',a.g15.resolve(),'-o',alias])
 load=w/'load_probe64';run([llvm/'clang.exe','--target=aarch64-linux-android32','-O2','-Wall','-Wextra','-Werror',MODULE/'src/load_probe.c','-ldl','-o',load])
 shutil.copytree(MODULE,out,ignore=shutil.ignore_patterns('system','bin','checksums.sha256','status.txt','disable'))
 (out/'bin').mkdir();shutil.copy2(load,out/'bin/load_probe64')
 sphal='libutilscallstack.so\nandroid.hardware.graphics.mapper@4.0.so\nlibgralloctypes.so\nlibhidlbase.so\nlibcutils.so\nlibhardware.so\nlibbase.so\nlibdmabufheap.so\nlibutils.so\n'
 for candidate in ['g15','g18']:
  app=w/candidate;app.mkdir(exist_ok=True)
  manifest='<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.sw1313.malidriver.'+candidate+'" android:versionCode="2" android:versionName="0.1.1"><uses-sdk android:minSdkVersion="32" android:targetSdkVersion="32"/><application android:label="RK Mali '+candidate+' local driver" android:hasCode="false" android:extractNativeLibs="false" android:forceQueryable="true"><meta-data android:name="com.android.graphics.driver.build_time" android:value="L1792022400"/></application></manifest>'
  (app/'AndroidManifest.xml').write_text(manifest,encoding='utf-8')
  run([bt/'aapt2.exe','link','--manifest',app/'AndroidManifest.xml','-I',jar,'-o',app/'base.apk'])
  with zipfile.ZipFile(app/'base.apk','a',zipfile.ZIP_STORED) as z:
   z.writestr('assets/sphal_libraries.txt',sphal)
   for name,path in [('libGLES_mali.so',supplied[candidate]),('vulkan.rk3588.so',alias),('android.hardware.common-V2-ndk.so',common),('android.hardware.graphics.allocator-V1-ndk.so',a.allocator),('android.hardware.graphics.common-V3-ndk.so',a.graphics_common)]:z.write(path,'lib/arm64-v8a/'+name)
  run([bt/'zipalign.exe','-f','-p','4',app/'base.apk',app/'aligned.apk'])
  dest=out/'system/app'/('RKMali'+candidate)/('RkMali-'+candidate+'.apk');dest.parent.mkdir(parents=True)
  run([bt/'apksigner.bat','sign','--ks',a.keystore.resolve(),'--ks-key-alias',a.key_alias,'--ks-pass','env:'+a.storepass_env,'--out',dest,app/'aligned.apk'])
  run([bt/'apksigner.bat','verify',dest])
 files=sorted(f for f in out.rglob('*') if f.is_file())
 for f in files:
  if f.suffix in {'.sh','.md','.json','.prop','.sha256','.c','.cpp','.h','.aidl','.txt'}:f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
 (out/'checksums.sha256').write_bytes(''.join(digest(f)+'  '+f.relative_to(out).as_posix()+'\n' for f in files).encode())
 dest=w/'rk3588-native-mali-0.1.1-local.zip'
 with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED) as z:
  for f in sorted(out.rglob('*')):
   if f.is_file():z.write(f,f.relative_to(out).as_posix())
 print(dest,digest(dest))
if __name__=='__main__':main()
