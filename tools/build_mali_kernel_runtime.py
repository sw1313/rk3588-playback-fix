"""Build local health/controller/scanner; no PC connection or network is needed."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--work',type=Path,required=True);a=p.parse_args()
 source=ROOT/'magisk-mali-global/src/kernel';work=a.work.resolve();work.mkdir(parents=True,exist_ok=True)
 llvm=a.ndk.resolve()/'toolchains/llvm/prebuilt'/('windows-x86_64' if os.name=='nt' else 'linux-x86_64')/'bin';cc=llvm/('clang.exe' if os.name=='nt' else 'clang')
 args=['--target=aarch64-linux-android32','-O2','-std=c11','-Wall','-Wextra','-Werror','-I'+str(source)]
 for local,public in [(ROOT,'/src'),(a.ndk.resolve(),'/ndk'),(work,'/obj')]:
  for spelling in {str(local),local.as_posix()}:args+=['-ffile-prefix-map='+spelling+'='+public]
 hashes={}
 for name in ['kernel_controller','kernel_health','kernel_scan']:
  out=work/name;r=subprocess.run([str(cc),*args,str(source/(name+'.c')),'-o',str(out)],capture_output=True)
  if r.returncode:raise SystemExit(r.stderr.decode('utf-8','replace'))
  raw=out.read_bytes();assert not any(x in raw for x in [str(ROOT).encode(),ROOT.as_posix().encode(),str(a.ndk.resolve()).encode(),a.ndk.resolve().as_posix().encode()])
  hashes[name]=hashlib.sha256(raw).hexdigest()
 (work/'BUILD.json').write_text(json.dumps(hashes,indent=2));print(json.dumps(hashes,indent=2))
if __name__=='__main__':main()
