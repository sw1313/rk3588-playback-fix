"""Build the exact API32 Decode refresh-session component; no firmware distributed."""
import argparse, hashlib, json, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SF='fddde345b957a0898c44cf1c62d23e9736d98d246e46bb3f39244c0e7d6b0d1a'
SHIM='bc552b3cb34b95bbc355a15033b228cab922950986bcdb84501222b2beea80f3'
GFX='632cf852f27daf89a301c0519dca85a856057f99fd7bc9aa40549759b42dfb9c'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ['ndk','surfaceflinger','sfhdr','gfx','utils','work']:p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();w=a.work.resolve();w.mkdir(parents=True,exist_ok=False)
 for path,digest in [(a.surfaceflinger,SF),(a.sfhdr,SHIM),(a.gfx,GFX)]:
  if sha(path)!=digest:raise SystemExit('Unknown input: '+path.name)
 out=w/'component';out.mkdir();(out/'system/lib64').mkdir(parents=True)
 llvm=a.ndk.resolve()/'toolchains/llvm/prebuilt/windows-x86_64/bin'
 common=[str(llvm/'clang++.exe'),'--target=aarch64-linux-android32','-std=c++17','-O2','-fPIC','-fvisibility=hidden','-fno-exceptions','-fno-rtti','-nostdlib++','-Wall','-Wextra','-Werror','-Wl,-z,relro,-z,now','-ffile-prefix-map='+str(ROOT)+'=/src','-fmacro-prefix-map='+str(ROOT)+'=/src']
 lib=out/'system/lib64/librkses.so'
 subprocess.run(common+['-shared',str(ROOT/'magisk-hdr/src/refresh_session.cpp'),'-o',str(lib),'-Wl,-soname,librkses.so','-Wl,--no-as-needed',str(a.gfx.resolve()),'-Wl,--as-needed','-ldl','-llog'],check=True)
 subprocess.run(common+[str(ROOT/'tools/hdr-probes/refresh_session_test.cpp'),'-o',str(w/'refresh-session-test64'),'-ldl','-llog'],check=True)
 subprocess.run([str(llvm/'llvm-strip.exe'),'--strip-unneeded',str(lib)],check=True)
 # Source-built HDR loader keeps its code and all dependencies. Insert the
 # session DSO in its existing dependency chain, preserving original gfx.
 b=a.sfhdr.read_bytes();old=b'librkgfx_dc.so\0';new=b'librkses.so\0'.ljust(len(old),b'\0')
 if b.count(old)!=1 or b.find(old)!=3635:raise SystemExit('HDR loader dependency ABI changed')
 variant=b[:3635]+new+b[3635+len(old):]
 (out/'system/lib64/libsfhdr.so').write_bytes(variant)
 (out/'loader.sha256').write_text(hashlib.sha256(variant).hexdigest()+'\n',encoding='ascii')
 (out/'loader-recipe.tsv').write_text(f'3635 {old.hex()} {new.hex()}\n',encoding='ascii')
 # Build native bounded patcher so install generates the loader locally.
 subprocess.run([str(llvm/'clang.exe'),'--target=aarch64-linux-android32','-O2',str(ROOT/'magisk-hdr/src/byte_patch.c'),'-o',str(out/'byte_patch64')],check=True)
 shutil.copyfile(ROOT/'magisk-hdr/src/refresh_session.cpp',out/'refresh_session.cpp')
 meta={'schema':2,'status':'UNVALIDATED_SYSTEM_REFRESH_SESSION_CANDIDATE','firmware_sf_sha256':SF,'original_source_built_loader_sha256':SHIM,'loader_sha256':hashlib.sha256(variant).hexdigest(),'original_gfx_sha256':GFX,'utils_sha256':sha(a.utils),'library_sha256':sha(lib),'scope':'retain existing fixed-source vote in same Layer in Auto; common Binder/BLAST transaction destination; native LayerHistory handles hidden/removed layers','abi':'API32 ARM64 exact Layer::setFrameRate indirect 24-byte FrameRate; pinned startup prologue relocation','app_hooks':False,'boot_flash':False,'physical_acceptance':False,'rollback':'display_session=0 and reboot; whole module disable restores firmware'}
 (out/'CANDIDATE.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8')
 (out/'platform.sha256').write_text(sha(a.utils)+'  /system/lib64/libutils.so\n'+GFX+'  /system/lib64/librkgfx_dc.so\n',encoding='ascii')
 for f in out.rglob('*'):
  if f.is_file() and f.suffix in {'.cpp','.json','.sha256','.tsv'}:f.write_bytes(f.read_bytes().replace(b'\r\n',b'\n'))
 (out/'checksums.sha256').write_text(''.join(sha(f)+'  '+f.relative_to(out).as_posix()+'\n' for f in sorted(out.rglob('*')) if f.is_file()),encoding='ascii',newline='\n')
 print(json.dumps({'component':str(out),'library_sha256':sha(lib),'loader_sha256':meta['loader_sha256'],'physical_acceptance':False}))
if __name__=='__main__':main()
