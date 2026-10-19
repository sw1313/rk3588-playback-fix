"""Build internal exact-ABI HDMI event component for Decode Stack only."""
import argparse,hashlib,shutil,subprocess,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['ndk','runtime','hardware-legacy','work']:p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();w=a.work.resolve()
 if w.exists():raise SystemExit('Use fresh work directory')
 original='897c94e050aec76d20374fc175b13a1ed1a9c147d7f53fa14bc6a4f8b57f34c5';legacy='b1271355d0ed08a8d4b41f321184df0df12d02b6972a75babe59ff11e5d5cc48'
 if sha(a.runtime)!=original or sha(a.hardware_legacy)!=legacy:raise SystemExit('Unknown firmware link inputs')
 out=w/'package';shutil.copytree(ROOT/'magisk-hdmi-event',out);(out/'target').mkdir();(out/'bin').mkdir();(out/'contracts').mkdir()
 llvm=a.ndk/'toolchains/llvm/prebuilt/windows-x86_64/bin';c=llvm/'clang++.exe'
 common=[str(c),'--target=aarch64-linux-android32','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-ffile-prefix-map='+str(ROOT)+'=/src/rk3588-playback-fix','-ffile-prefix-map='+str(w)+'=/build']
 subprocess.run(common+['-nostdlib++','-fPIC','-shared','-fvisibility=hidden',str(out/'src/hdmi_event.cpp'),'-Wl,--no-undefined,-z,relro,-z,now','-Wl,--no-as-needed',str(a.hardware_legacy),'-ldl','-llog','-o',str(out/'target/libhevt.so')],check=True)
 subprocess.run(common+['-static-libstdc++',str(ROOT/'tools/hdr-probes/hdmi_event_test.cpp'),'-ldl','-o',str(out/'bin/event-test64')],check=True)
 shutil.copyfile(ROOT/'tools/hdr-probes/hdmi_event_test.cpp',out/'src/event_test.cpp')
 t=(out/'src/event_test.cpp').read_text().replace('../../magisk-hdmi-event/src/event_guard.h','event_guard.h');(out/'src/event_test.cpp').write_text(t,encoding='utf-8')
 subprocess.run([str(llvm/'clang.exe'),'--target=aarch64-linux-android32','-O2','-Wall','-Wextra','-Werror',str(ROOT/'magisk-hdr/src/byte_patch.c'),'-o',str(out/'bin/byte_patch64')],check=True)
 shutil.copyfile(ROOT/'magisk-hdr/src/byte_patch.c',out/'src/byte_patch.c')
 old=b'libhardware_legacy.so\0';new=b'libhevt.so\0'.ljust(len(old),b'\0');data=a.runtime.read_bytes();assert data.count(old)==1;pos=data.index(old);variant=data[:pos]+new+data[pos+len(old):];patched=hashlib.sha256(variant).hexdigest()
 (out/'contracts/runtime-arm64.tsv').write_text(f'{pos} {old.hex()} {new.hex()}\n',encoding='utf-8')
 (out/'contracts/runtime.json').write_text(json.dumps({'original_sha256':original,'patched_sha256':patched,'import_RVA':'0x207570','register_method_RVA':'0x15b7c0','scope':'Source-built bounded HDMI extcon debounce; original firmware generated on device only','private_firmware_bundled':False},indent=2)+'\n',encoding='utf-8')
 (out/'contracts/platform.sha256').write_text(legacy+'  /system/lib64/libhardware_legacy.so\n',encoding='utf-8')
 # Fixed-hash shell recipe; no private firmware or runtime input is copied.
 f=out/'prepare.sh';s=f.read_text();s=s.replace('@ORIGINAL@',original).replace('@PATCHED@',patched);f.write_text(s,encoding='utf-8')
 for b in [out/'target/libhevt.so',*list((out/'bin').iterdir())]:subprocess.run([str(llvm/'llvm-strip.exe'),'--strip-unneeded',str(b)],check=True)
 files=sorted(x for x in out.rglob('*') if x.is_file())
 for f in files:
  if f.suffix in {'.sh','.md','.txt','.json','.tsv','.cpp','.h','.c','.sha256'}:f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
 (out/'checksums.sha256').write_text(''.join(sha(f)+'  '+f.relative_to(out).as_posix()+'\n' for f in files),encoding='utf-8',newline='\n')
 print('BUILT internal HDMI event component',patched,sha(out/'target/libhevt.so'))
if __name__=='__main__':main()
